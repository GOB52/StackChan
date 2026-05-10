/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// Modified by GOB (X:@GOB_52_GOB / GitHub:GOB52) - StackChan firmware fork
#include "default.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;

static const Vector2i _container_pos  = Vector2i(0, 89);
static const Vector2i _container_size = Vector2i(320, 74);
static const Vector2i _arrow_offset   = Vector2i(40, -15);
static const int _text_mx             = 20;
static const int _bubble_min_width    = 90;
// GOB fork: FX ON 用 (画面幅と一致、右端 clip 解消)。
static const int _bubble_max_width    = 320;
// GOB fork: FX OFF (renderAsVanilla) は upstream 1.3.0 と同値の 340 を使用。
static const int _vanilla_bubble_max_width = 340;
static const int _bubble_height       = 52;
static const int _bubble_min_offset_x = 66;
static const int _bubble_max_offset_x = 0;

// GOB fork: tail-follow slide animation parameters
static constexpr int      SLIDE_MIN_DURATION_MS = 300;
// 上限は WDT 対策。長文 chunk が連続したとき 1 つの anim を 1500ms 以内に収め、
// LVGL refresh の連続稼働時間を抑えて opus_codec の CPU 1 を奪わない。
// 結果、長文 chunk では実効速度が SLIDE_PX_PER_SEC を超えて加速する。
static constexpr uint32_t SLIDE_MAX_DURATION_MS = 1500;
static constexpr float    SLIDE_PX_PER_SEC      = 70.0f;  // ~5 CJK chars/sec @ 14px/char

LV_IMAGE_DECLARE(default_bubble_arrow);

DefaultSpeechBubble::DefaultSpeechBubble(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor,
                                         const lv_font_t* font)
{
    _container = std::make_unique<Container>(parent);
    _container->setRadius(0);
    _container->setAlign(LV_ALIGN_CENTER);
    _container->setBorderWidth(0);
    _container->setBgOpa(0);
    _container->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _container->setSize(_container_size.x, _container_size.y);
    _container->setPos(_container_pos.x, _container_pos.y);
    _container->setPadding(0, 0, 0, 0);

    _arrow = std::make_unique<Image>(_container->get());
    _arrow->setSrc(&default_bubble_arrow);
    _arrow->setAlign(LV_ALIGN_CENTER);
    _arrow->setPos(_arrow_offset.x, _arrow_offset.y);
    _arrow->setImageRecolorOpa(LV_OPA_COVER);
    _arrow->setImageRecolor(primaryColor);

    _bubble = std::make_unique<Container>(_container->get());
    _bubble->setRadius(LV_RADIUS_CIRCLE);
    _bubble->setAlign(LV_ALIGN_CENTER);
    _bubble->setBorderWidth(0);
    _bubble->setBgColor(primaryColor);
    _bubble->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _bubble->setSize(_bubble_max_width, _bubble_height);
    _bubble->setPos(0, 11);
    _bubble->setPadding(0, 0, 0, 0);

    // GOB fork: text clip container — bubble 内の inner area (= 左右 margin を
    // 引いた領域) で text を clip する。これにより tail-follow scroll 時にも
    // text が bubble の角丸領域に侵食しない。
    _text_clip = std::make_unique<Container>(_bubble->get());
    _text_clip->setBgOpa(0);
    _text_clip->setBorderWidth(0);
    _text_clip->setRadius(0);
    _text_clip->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _text_clip->setSize(_bubble_max_width - _text_mx * 2, _bubble_height);
    _text_clip->setAlign(LV_ALIGN_LEFT_MID);
    _text_clip->setPos(_text_mx, 0);
    _text_clip->setPadding(0, 0, 0, 0);

    _text = std::make_unique<Label>(_text_clip->get());
    _text->setTextColor(secondaryColor);
    _text->setTextFont(font);
    _text->setTextAlign(LV_TEXT_ALIGN_LEFT);
    _text->setAlign(LV_ALIGN_LEFT_MID);
    _text->setPos(0, 0);
    // GOB fork: SCROLL_CIRCULAR から CLIP に変更し、位置を手動制御で tail-follow scroll。
    _text->setLongMode(LV_LABEL_LONG_MODE_CLIP);

    clearSpeech();
}

DefaultSpeechBubble::~DefaultSpeechBubble()
{
    cancelDismissTimer();
    if (_text) {
        lv_anim_delete(_text->get(), nullptr);
    }
    _text.reset();
    _text_clip.reset();
    _bubble.reset();
    _arrow.reset();
    _container.reset();
}

namespace {

// LVGL anim exec callback: 単に lv_obj_set_x を呼ぶだけ
void _anim_set_x_cb(void* var, int32_t v)
{
    lv_obj_set_x(static_cast<lv_obj_t*>(var), v);
}

// GOB fork: chunk に紛れ込む改行を除去 (`\n`/`\r`)。LVGL label は CLIP モードでも
// 改行を尊重して複数行レイアウトするため、bubble 高さ (52px、単一行想定) を
// 超過し、LV_ALIGN_LEFT_MID の縦中央基準で text が上へずれる。CJK は単語間
// space を使わないので、改行は単純削除でよい (sentence 区切りは句点で判別可)。
std::string strip_newlines(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c != '\n' && c != '\r') out.push_back(c);
    }
    return out;
}

}  // namespace

void DefaultSpeechBubble::renderAndPosition(size_t animated_chars)
{
    if (!_text || !_bubble || !_text_clip) return;

    // GOB fork: appendSpeech 経路では CLIP + 左寄せ + 手動 x で tail-follow。
    // 直前に renderAsVanilla() で label を `_bubble` 直下 (vanilla 構造) に
    // 移動していた可能性があるため、`_text_clip` 配下へ戻す。
    const bool parent_changed = (lv_obj_get_parent(_text->get()) != _text_clip->get());
    if (parent_changed) {
        lv_obj_set_parent(_text->get(), _text_clip->get());
    }
    // 直前に renderAsVanilla() で SCROLL_CIRCULAR + CENTER に切替えていた
    // 可能性があるため、毎回 mode を明示的に揃える。
    _text->setLongMode(LV_LABEL_LONG_MODE_CLIP);
    _text->setTextAlign(LV_TEXT_ALIGN_LEFT);
    _text->setAlign(LV_ALIGN_LEFT_MID);

    _text->setText(_accumulated);

    lv_point_t text_size = {0, 0};
    if (!_accumulated.empty()) {
        lv_text_get_size(&text_size, _accumulated.c_str(), _text->getTextFont(),
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    const int text_w = text_size.x;

    int bubble_width = std::min(text_w + _text_mx * 2, _bubble_max_width);
    bubble_width     = std::max(bubble_width, _bubble_min_width);

    const int bubble_offset_x = static_cast<int>(map_range(
        bubble_width, _bubble_min_width, _bubble_max_width,
        _bubble_min_offset_x, _bubble_max_offset_x));

    _bubble->setWidth(bubble_width);
    _bubble->setX(bubble_offset_x);

    // Clip container も bubble の inner_w に合わせて伸縮 (左右 margin _text_mx は固定)
    const int inner_w = bubble_width - _text_mx * 2;
    _text_clip->setWidth(inner_w);

    // CLIP container が children を inner_w にクリップするので label は実 text 幅でよい
    const int label_w = std::max(text_w, inner_w);
    _text->setWidth(label_w);

    // label.x は _text_clip 内座標 (origin = clip 左上)
    int32_t target_x;
    if (text_w <= inner_w) {
        // 収まる: clip 中央
        target_x = (inner_w - text_w) / 2;
    } else {
        // 超過: 末尾を clip 右端に揃える (tail-follow)
        target_x = inner_w - text_w;
    }

    lv_obj_t* const label_obj = _text->get();
    // 既存 anim をキャンセル (cancellation で completed_cb は発火しない)。
    lv_anim_delete(label_obj, _anim_set_x_cb);

    // GOB fork: 親変更直後 (renderAsVanilla → renderAndPosition 遷移) は
    // lv_obj_get_x が前 anchor (CENTER) の左端値を返したままだと anim が
    // cur=−40 → target=0 のような誤った遷移を見せてしまう。
    // layout を即時更新して新 anchor (LEFT_MID) 基準の cur_x (= 0) を反映してから
    // anim を起動することで、滑らかな tail-follow を維持しつつジャンプも回避する。
    if (parent_changed) {
        lv_obj_update_layout(label_obj);
    }

    if (animated_chars == 0) {
        lv_obj_set_x(label_obj, target_x);
        _label_x_logical = target_x;
        // GOB fork: 非アニメーションでは即「描画完了」扱い。dismiss が pending なら arm。
        _slide_active = false;
        if (_dismiss_pending_delay_ms > 0) {
            armDismissTimer(_dismiss_pending_delay_ms);
            _dismiss_pending_delay_ms = 0;
        }
        return;
    }

    const int32_t cur_x  = lv_obj_get_x(label_obj);
    const int32_t delta  = std::abs(target_x - cur_x);
    uint32_t dur = std::max<uint32_t>(
        SLIDE_MIN_DURATION_MS,
        static_cast<uint32_t>(static_cast<float>(delta) / SLIDE_PX_PER_SEC * 1000.0f));
    // GOB fork: 上限クランプで長文 chunk 連続時の総 LVGL 稼働時間を制御。
    if (dur > SLIDE_MAX_DURATION_MS) dur = SLIDE_MAX_DURATION_MS;

    _slide_active = true;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label_obj);
    lv_anim_set_values(&a, cur_x, target_x);
    lv_anim_set_time(&a, dur);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, _anim_set_x_cb);
    lv_anim_set_completed_cb(&a, &DefaultSpeechBubble::onSlideCompleted);
    lv_anim_set_user_data(&a, this);
    lv_anim_start(&a);
    _label_x_logical = target_x;
}

void DefaultSpeechBubble::renderAsVanilla()
{
    if (!_text || !_bubble) return;

    // GOB fork: FX OFF / status msg は upstream 1.3.0 のロジックと値を完全に
    // 再現する。具体的には:
    //   - label の親を `_bubble` 直 (中間 clip container を使わない)
    //   - SCROLL_CIRCULAR + CENTER align + (0,0) + width 280 (固定)
    //   - bubble_max_width 340
    //   - lv_anim_delete を呼ばない (LVGL native scroll anim を保護)
    //   - 改行 strip もしない (upstream 準拠、status msg は改行を含まない想定)
    //
    // 直前に renderAndPosition (FX ON) で `_text_clip` 配下に移動していた
    // 場合に備え、親が `_bubble` でなければ戻す。
    if (_text_clip && lv_obj_get_parent(_text->get()) != _bubble->get()) {
        lv_obj_set_parent(_text->get(), _bubble->get());
    }

    _text->setLongMode(LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    _text->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _text->setAlign(LV_ALIGN_CENTER);
    _text->setPos(0, 0);
    _text->setWidth(320 - _text_mx * 2);  // 280 固定 (upstream 1.3.0 と同値)

    _text->setText(_accumulated);

    lv_point_t text_size = {0, 0};
    if (!_accumulated.empty()) {
        lv_text_get_size(&text_size, _accumulated.c_str(), _text->getTextFont(),
                         0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    }
    const int text_w = static_cast<int>(text_size.x);

    int bubble_width = std::min(text_w + _text_mx * 2, _vanilla_bubble_max_width);
    bubble_width     = std::max(bubble_width, _bubble_min_width);

    const int bubble_offset_x = static_cast<int>(map_range(
        bubble_width, _bubble_min_width, _vanilla_bubble_max_width,
        _bubble_min_offset_x, _bubble_max_offset_x));

    _bubble->setWidth(bubble_width);
    _bubble->setX(bubble_offset_x);

    // FX ON 用の手動 slide / dismiss state はリセットだけしておく。
    _slide_active             = false;
    _dismiss_pending_delay_ms = 0;
    _label_x_logical          = 0;
}

void DefaultSpeechBubble::setSpeech(std::string_view text)
{
    if (text.empty()) {
        clearSpeech();
        return;
    }

    // 新コンテンツ到着 → pending dismiss を取消
    cancelDismissTimer();

    _streaming                 = false;
    _last_was_tool_indicator   = false;
    // GOB fork: setSpeech は status msg + FX OFF 時の assistant 共通経路。
    // upstream 1.3.0 互換のため改行 strip もしない (status msg は改行を含まない)。
    _accumulated               = std::string(text);

    renderAsVanilla();

    setVisible(true);
}

void DefaultSpeechBubble::appendSpeech(std::string_view text)
{
    if (text.empty()) return;

    // 新コンテンツ到着 → pending dismiss を取消
    cancelDismissTimer();

    bool just_cleared = false;

    if (!_streaming) {
        // utterance 開始: 過去 status (e.g. "Speaking") を捨てる
        _accumulated.clear();
        _streaming               = true;
        _last_was_tool_indicator = false;
        just_cleared             = true;
    }

    // GOB fork: '%' で始まる chunk は LLM の tool call indicator。
    // regular セリフ群と '%' indicator 群の境界で accumulator をクリアし、
    // 群が混じらないようにする。
    const bool is_tool_indicator = (text.front() == '%');
    if (is_tool_indicator != _last_was_tool_indicator) {
        _accumulated.clear();
        just_cleared = true;
    }
    _last_was_tool_indicator = is_tool_indicator;

    const std::string sanitized = strip_newlines(text);
    if (sanitized.empty()) return;
    _accumulated.append(sanitized);

    // GOB fork: accumulator clear 直後は label.x を 0 にスナップしてから
    // 新 anim を開始する。これにより、旧 anim の途中位置 (e.g. +80 や -500)
    // から新 target へ「逆方向スライド」する事態を回避し、その遷移中に
    // 発生する文字描画グリッチを防ぐ。clip 全体を invalidate して旧位置の
    // 残骸も確実にクリア。
    if (just_cleared && _text) {
        lv_anim_delete(_text->get(), nullptr);
        lv_obj_set_x(_text->get(), 0);
        if (_text_clip) {
            lv_obj_invalidate(_text_clip->get());
        }
    }

    renderAndPosition(sanitized.size());

    setVisible(true);
}

void DefaultSpeechBubble::requestDismissAfterRender(uint32_t delay_ms)
{
    if (_slide_active) {
        // anim 完了を待つ。完了コールバックが arm する。
        _dismiss_pending_delay_ms = delay_ms > 0 ? delay_ms : 1;
    } else {
        // 即タイマ起動
        armDismissTimer(delay_ms);
    }
}

void DefaultSpeechBubble::clearSpeech()
{
    cancelDismissTimer();
    _streaming                 = false;
    _slide_active              = false;
    _last_was_tool_indicator   = false;
    _accumulated.clear();
    if (_text) {
        lv_anim_delete(_text->get(), nullptr);
        _text->setText("");
        lv_obj_set_x(_text->get(), 0);  // clip-relative origin
    }
    _label_x_logical = 0;
    setVisible(false);
}

void DefaultSpeechBubble::armDismissTimer(uint32_t delay_ms)
{
    cancelDismissTimer();
    if (delay_ms == 0) {
        clearSpeech();
        return;
    }
    _dismiss_timer = lv_timer_create(&DefaultSpeechBubble::onDismissTimer, delay_ms, this);
    if (_dismiss_timer) {
        lv_timer_set_repeat_count(_dismiss_timer, 1);
    }
}

void DefaultSpeechBubble::cancelDismissTimer()
{
    if (_dismiss_timer) {
        lv_timer_delete(_dismiss_timer);
        _dismiss_timer = nullptr;
    }
    _dismiss_pending_delay_ms = 0;
}

void DefaultSpeechBubble::onSlideCompleted(lv_anim_t* a)
{
    auto* self = static_cast<DefaultSpeechBubble*>(lv_anim_get_user_data(a));
    if (!self) return;
    self->_slide_active = false;
    if (self->_dismiss_pending_delay_ms > 0) {
        self->armDismissTimer(self->_dismiss_pending_delay_ms);
        self->_dismiss_pending_delay_ms = 0;
    }
}

void DefaultSpeechBubble::onDismissTimer(lv_timer_t* t)
{
    auto* self = static_cast<DefaultSpeechBubble*>(lv_timer_get_user_data(t));
    if (!self) return;
    // タイマは repeat_count=1 で自動削除されるので、ここでは pointer をクリアするだけ
    self->_dismiss_timer = nullptr;
    self->clearSpeech();
}

void DefaultSpeechBubble::setVisible(bool visible)
{
    SpeechBubble::setVisible(visible);

    _container->setHidden(!visible);
}

void DefaultSpeechBubble::setTextFont(void* font)
{
    if (_text && font) {
        _text->setTextFont((lv_font_t*)font);
    }
}
