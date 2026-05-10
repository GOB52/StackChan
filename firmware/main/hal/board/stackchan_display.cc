/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// Modified by GOB (X:@GOB_52_GOB / GitHub:GOB52) - StackChan firmware fork
#include "stackchan_display.h"
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include <esp_psram.h>
#include <algorithm>
#include <string_view>
#include <vector>
#include <cstring>
#include <src/misc/cache/lv_cache.h>
#include <settings.h>
#include <lvgl.h>
#include <lvgl_theme.h>
#include <stackchan/stackchan.h>
#include <stackchan/avatar/skins/image/skin_loader.h>
#include <stackchan/gob_fork_nvs.h>
#include <assets/lang_config.h>
#include <hal/hal.h>

using namespace stackchan;
using namespace stackchan::avatar;

#define TAG "StackChanAvatarDisplay"

namespace {

// GOB fork: UTF-8 1 char のバイト長を返す。lead byte の上位ビットで判定。
size_t utf8_char_len(const unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;       // ASCII
    if ((c & 0xE0) == 0xC0) return 2;       // 2-byte
    if ((c & 0xF0) == 0xE0) return 3;       // 3-byte (CJK の大半)
    if ((c & 0xF8) == 0xF0) return 4;       // 4-byte
    return 1;                                // 不正バイト: 1 として進める
}

// GOB fork: UTF-8 文字列の char (= code point) 数。pacing 用。
size_t utf8_char_count(std::string_view s)
{
    size_t count = 0;
    for (size_t i = 0; i < s.size(); ) {
        i += utf8_char_len(static_cast<unsigned char>(s[i]));
        count++;
    }
    return count;
}

// GOB fork: 句読点判定 (CJK 全角 + ASCII)。これらの文字直後で segment を切る。
bool is_segment_delimiter(std::string_view ch)
{
    static constexpr const char* DELIMS[] = {
        "、", "。", "！", "？", "，",  // CJK 句読点 (3 byte UTF-8)
        ",", ".", "!", "?",            // ASCII
    };
    for (auto* d : DELIMS) {
        if (ch == d) return true;
    }
    return false;
}

// GOB fork: 1 chunk を句読点で fragment に分割 (内部用)。区切り文字は前 fragment に含める。
std::vector<std::string> split_to_fragments(std::string_view text)
{
    std::vector<std::string> fragments;
    std::string current;
    for (size_t i = 0; i < text.size(); ) {
        const size_t len = utf8_char_len(static_cast<unsigned char>(text[i]));
        const std::string_view ch = text.substr(i, len);
        current.append(ch);
        i += len;
        if (is_segment_delimiter(ch)) {
            fragments.emplace_back(std::move(current));
            current.clear();
        }
    }
    if (!current.empty()) fragments.emplace_back(std::move(current));
    return fragments;
}

// GOB fork: bubble の inner_w (280px) に収まる字数の概算上限。
// CJK 14px/char 平均で 280/14 = 20。少し余裕を持たせて 18 文字。
constexpr size_t SEGMENT_MAX_CHARS = 18;

// GOB fork: 句読点で fragment 化 → bubble に収まる範囲で隣接 fragment をマージ。
//
// - 入力が短ければ 1 segment (split せず)
// - 長文でも 18 字以内に収まる範囲で fragment を結合
// - 1 fragment 単体が 18 字超なら分割不能なのでそのまま 1 segment として返す
// - '%' で始まる LLM tool call indicator は分割せず 1 segment で一括 dispatch
//   (内部に '.' (関数 separator) を含むが意味的に 1 つの命令なので)
std::vector<std::string> smart_split(std::string_view text)
{
    if (!text.empty() && text.front() == '%') {
        return {std::string(text)};
    }
    auto fragments = split_to_fragments(text);
    std::vector<std::string> segments;
    std::string current;
    size_t current_chars = 0;

    for (auto& frag : fragments) {
        const size_t frag_chars = utf8_char_count(frag);
        if (current.empty()) {
            current        = std::move(frag);
            current_chars  = frag_chars;
        } else if (current_chars + frag_chars <= SEGMENT_MAX_CHARS) {
            current.append(frag);
            current_chars += frag_chars;
        } else {
            segments.push_back(std::move(current));
            current        = std::move(frag);
            current_chars  = frag_chars;
        }
    }
    if (!current.empty()) {
        segments.push_back(std::move(current));
    }
    return segments;
}

}  // namespace

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_awesome_30_4);

// Have to register themes, so the asset apply can update the text font
void StackChanAvatarDisplay::InitializeLcdThemes()
{
    auto text_font       = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font       = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_awesome_30_4);

    // light theme
    auto light_theme = new LvglTheme("light");
    light_theme->set_background_color(lv_color_hex(0xFFFFFF));        // rgb(255, 255, 255)
    light_theme->set_text_color(lv_color_hex(0x000000));              // rgb(0, 0, 0)
    light_theme->set_chat_background_color(lv_color_hex(0xE0E0E0));   // rgb(224, 224, 224)
    light_theme->set_user_bubble_color(lv_color_hex(0x00FF00));       // rgb(0, 128, 0)
    light_theme->set_assistant_bubble_color(lv_color_hex(0xDDDDDD));  // rgb(221, 221, 221)
    light_theme->set_system_bubble_color(lv_color_hex(0xFFFFFF));     // rgb(255, 255, 255)
    light_theme->set_system_text_color(lv_color_hex(0x000000));       // rgb(0, 0, 0)
    light_theme->set_border_color(lv_color_hex(0x000000));            // rgb(0, 0, 0)
    light_theme->set_low_battery_color(lv_color_hex(0x000000));       // rgb(0, 0, 0)
    light_theme->set_text_font(text_font);
    light_theme->set_icon_font(icon_font);
    light_theme->set_large_icon_font(large_icon_font);

    // dark theme
    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_background_color(lv_color_hex(0x000000));        // rgb(0, 0, 0)
    dark_theme->set_text_color(lv_color_hex(0xFFFFFF));              // rgb(255, 255, 255)
    dark_theme->set_chat_background_color(lv_color_hex(0x1F1F1F));   // rgb(31, 31, 31)
    dark_theme->set_user_bubble_color(lv_color_hex(0x00FF00));       // rgb(0, 128, 0)
    dark_theme->set_assistant_bubble_color(lv_color_hex(0x222222));  // rgb(34, 34, 34)
    dark_theme->set_system_bubble_color(lv_color_hex(0x000000));     // rgb(0, 0, 0)
    dark_theme->set_system_text_color(lv_color_hex(0xFFFFFF));       // rgb(255, 255, 255)
    dark_theme->set_border_color(lv_color_hex(0xFFFFFF));            // rgb(255, 255, 255)
    dark_theme->set_low_battery_color(lv_color_hex(0xFF0000));       // rgb(255, 0, 0)
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("light", light_theme);
    theme_manager.RegisterTheme("dark", dark_theme);
}

StackChanAvatarDisplay::StackChanAvatarDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                                               int width, int height, int offset_x, int offset_y, bool mirror_x,
                                               bool mirror_y, bool swap_xy)
    : LvglDisplay(), panel_io_(panel_io), panel_(panel)
{
    width_  = width;
    height_ = height;

    // Initialize LCD themes
    InitializeLcdThemes();

    // Load theme from settings
    Settings settings("display", false);
    std::string theme_name = settings.GetString("theme", "light");
    current_theme_         = LvglThemeManager::GetInstance().GetTheme(theme_name);

    // Draw white screen
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    {
        esp_err_t __err = esp_lcd_panel_disp_on_off(panel_, true);
        if (__err == ESP_ERR_NOT_SUPPORTED) {
            ESP_LOGW(TAG, "Panel does not support disp_on_off; assuming ON");
        } else {
            ESP_ERROR_CHECK(__err);
        }
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

#if CONFIG_SPIRAM
    // lv image cache, currently only PNG is supported
    size_t psram_size_mb = esp_psram_get_size() / 1024 / 1024;
    if (psram_size_mb >= 8) {
        lv_image_cache_resize(2 * 1024 * 1024, true);
        ESP_LOGI(TAG, "Use 2MB of PSRAM for image cache");
    } else if (psram_size_mb >= 2) {
        lv_image_cache_resize(512 * 1024, true);
        ESP_LOGI(TAG, "Use 512KB of PSRAM for image cache");
    }
#endif

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    // port_cfg.task_priority   = 20;
    port_cfg.task_priority = 3;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle      = panel_io_,
        .panel_handle   = panel_,
        .control_handle = nullptr,
        .buffer_size    = static_cast<uint32_t>(width_ * 20),
        .double_buffer  = false,
        .trans_size     = 0,
        .hres           = static_cast<uint32_t>(width_),
        .vres           = static_cast<uint32_t>(height_),
        .monochrome     = false,
        .rotation =
            {
                .swap_xy  = swap_xy,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags =
            {
                .buff_dma     = 1,
                .buff_spiram  = 0,
                .sw_rotate    = 0,
                .swap_bytes   = 1,
                .full_refresh = 0,
                .direct_mode  = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Create a timer to hide the preview image
    esp_timer_create_args_t preview_timer_args = {
        .callback =
            [](void* arg) {
                StackChanAvatarDisplay* display = static_cast<StackChanAvatarDisplay*>(arg);
                display->SetPreviewImage(nullptr);
            },
        .arg                   = this,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "preview_timer",
        .skip_unhandled_events = false,
    };
    esp_timer_create(&preview_timer_args, &preview_timer_);

    // Create boot logo label if not warm boot
    if (GetHAL().getWarmRebootTarget() < 0) {
        ESP_LOGI(TAG, "Create boot logo label");
        Lock();
        {
            uitk::lvgl_cpp::ScreenActive screen;
            screen.setBgColor(lv_color_hex(0x000000));
        }
        GetHAL().bootLogo = std::make_unique<BootLogo>();
        Unlock();
    }

    // Robot will be created later in SetupXiaoZhiUI()
}

StackChanAvatarDisplay::~StackChanAvatarDisplay()
{
    ESP_LOGI(TAG, "Destroying StackChanAvatarDisplay");

    // GOB fork: segment timer / queue 清掃
    if (segment_timer_ != nullptr) {
        lv_timer_delete(segment_timer_);
        segment_timer_ = nullptr;
    }
    segment_queue_.clear();

    if (preview_timer_ != nullptr) {
        esp_timer_stop(preview_timer_);
        esp_timer_delete(preview_timer_);
    }

    if (preview_image_ != nullptr) {
        lv_obj_del(preview_image_);
    }

    auto& stackchan = GetStackChan();
    if (stackchan.hasAvatar()) {
        stackchan.resetAvatar();
    }
}

bool StackChanAvatarDisplay::Lock(int timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void StackChanAvatarDisplay::Unlock()
{
    lvgl_port_unlock();
}

lv_disp_t* StackChanAvatarDisplay::GetLvglDisplay()
{
    return display_;
}

#include <hal/board/hal_bridge.h>

void StackChanAvatarDisplay::SetupUI()
{
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    Display::SetupUI();  // Mark SetupUI as called

    auto& stackchan = GetStackChan();

    if (stackchan.hasAvatar()) {
        ESP_LOGW(TAG, "Avatar already created");
        return;
    }

    DisplayLockGuard lock(this);

    ESP_LOGI(TAG, "Creating Stack-chan Avatar...");

    // GOB fork: share the same NVS-backed skin selection as AVATAR app so AI
    // Agent mode also reflects the user's chosen skin (or DefaultAvatar via the
    // "__default__" sentinel). load_avatar_or_fallback handles SD mount, LVGL
    // lock recursion (already inside DisplayLockGuard), and falls back to
    // DefaultAvatar on any SD/JSON/PNG failure.
    auto skin_result = stackchan::avatar::image::load_avatar_or_fallback(lv_screen_active());
    if (!skin_result.error_message.empty()) {
        ESP_LOGW(TAG, "Skin fallback to DefaultAvatar: %s",
                 skin_result.error_message.c_str());
    }
    ESP_LOGI(TAG, "Loaded skin: %s", skin_result.loaded_skin_id.c_str());

    auto avatar = std::move(skin_result.avatar);
    avatar->getPanel()->onClick().connect([]() {
        if (hal_bridge::is_xiaozhi_ready()) {
            hal_bridge::toggle_xiaozhi_chat_state();
        }
    });

    stackchan.attachAvatar(std::move(avatar));
    stackchan.addModifier(std::make_unique<BreathModifier>());
    blink_modifier_id_ = stackchan.addModifier(std::make_unique<BlinkModifier>());
    stackchan.addModifier(std::make_unique<HeadPetModifier>());
    stackchan.addModifier(std::make_unique<ImuEventModifier>());

    preview_image_ = lv_image_create(lv_screen_active());
    lv_obj_set_size(preview_image_, 320, 240);
    lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);

    // GetHAL().startStackChanAutoUpdate(24);

    ESP_LOGI(TAG, "Avatar created and started");
}

void StackChanAvatarDisplay::LvglLock()
{
    if (!Lock(30000)) {
        ESP_LOGE("Display", "Failed to lock display");
    }
}

void StackChanAvatarDisplay::LvglUnlock()
{
    Unlock();
}

void StackChanAvatarDisplay::SetEmotion(const char* emotion)
{
    auto& stackchan = GetStackChan();

    if (!stackchan.hasAvatar() || !emotion) {
        return;
    }

    DisplayLockGuard lock(this);

    // ESP_LOGE(TAG, "SetEmotion: %s", emotion);

    auto& avatar = stackchan.avatar();

    // Map emotion string to stackchan::Emotion
    if (strcmp(emotion, "neutral") == 0) {
        avatar.setEmotion(Emotion::Neutral);
    } else if (strcmp(emotion, "happy") == 0) {
        avatar.setEmotion(Emotion::Happy);
    } else if (strcmp(emotion, "laughing") == 0) {
        avatar.setEmotion(Emotion::Happy);
    } else if (strcmp(emotion, "angry") == 0) {
        avatar.setEmotion(Emotion::Angry);
    } else if (strcmp(emotion, "sad") == 0) {
        avatar.setEmotion(Emotion::Sad);
    } else if (strcmp(emotion, "crying") == 0) {
        avatar.setEmotion(Emotion::Sad);
    } else if (strcmp(emotion, "sleepy") == 0) {
        avatar.setEmotion(Emotion::Sleepy);
        avatar.setSpeech("Zzz…");
        is_sleeping_ = true;
        // avatar.mouth().setWeight(10);

        // Stop idle motion
        ESP_LOGW(TAG, "Stop idle motion");
        if (idle_motion_modifier_id_ >= 0) {
            stackchan.removeModifier(idle_motion_modifier_id_);
            idle_motion_modifier_id_ = -1;
            stackchan.removeModifier(idle_expression_modifier_id_);
            idle_expression_modifier_id_ = -1;
        }

        // Return to default pose
        auto& motion = GetStackChan().motion();
        motion.pitchServo().moveWithSpeed(0, 80);

    } else if (strcmp(emotion, "doubtful") == 0) {
        avatar.setEmotion(Emotion::Doubt);
    } else {
        ESP_LOGW(TAG, "Unknown emotion: %s, using NEUTRAL", emotion);
        avatar.setEmotion(Emotion::Neutral);
    }

    // Resync blink modifier base eye weights
    auto blink_modifier = static_cast<BlinkModifier*>(stackchan.getModifier(blink_modifier_id_));
    if (blink_modifier) {
        blink_modifier->resyncEyeWeights();
    }
}

void StackChanAvatarDisplay::SetChatMessage(const char* role, const char* content)
{
    if (!setup_ui_called_) {
        ESP_LOGW(TAG, "SetChatMessage('%s', '%s') called before SetupUI() - message will be lost!", role, content);
    }

    auto& stackchan = GetStackChan();
    if (!stackchan.hasAvatar()) {
        return;
    }

    // ESP_LOGE(TAG, "SetChatMessage: role=%s, content=%s", role ? role : "null", content ? content : "null");

    DisplayLockGuard lock(this);

    if (strcmp(role, "system") == 0) {
        // GOB fork: 状態 status は assistant utterance を上書きする。queue に
        // 残った segment があるなら破棄して新メッセージで置換。
        clearSegmentQueue();
        stackchan.avatar().setSpeech(content);
    } else if (strcmp(role, "assistant") == 0) {
        // GOB fork: NVS の bubble_fx flag で挙動切替。
        // ON  : 句読点 split + segment_queue + tail-follow scroll + dismiss
        // OFF : upstream そのまま (chunk ごと setSpeech で置換)
        if (stackchan::gob_fork::get_bubble_fx_enabled()) {
            enqueueAssistantChunks(std::string(content));
        } else {
            clearSegmentQueue();  // 念のため (FX OFF 中は queue 不使用)
            stackchan.avatar().setSpeech(content);
        }
    }
}

void StackChanAvatarDisplay::ClearChatMessages()
{
    auto& stackchan = GetStackChan();
    if (!stackchan.hasAvatar()) {
        return;
    }

    DisplayLockGuard lock(this);

    // GOB fork: queue + timer も解除 (segment 残骸を消す)
    clearSegmentQueue();
    stackchan.avatar().clearSpeech();

    ESP_LOGI(TAG, "Chat messages cleared");
}

// ---- GOB fork: long-sentence segmentation queue ----------------------------

void StackChanAvatarDisplay::enqueueAssistantChunks(const std::string& content)
{
    if (content.empty()) return;

    auto segments = smart_split(content);
    for (auto& s : segments) {
        if (!s.empty()) segment_queue_.push_back(std::move(s));
    }
    if (!segment_timer_ && !segment_queue_.empty()) {
        // タイマ未起動かつ queue に内容あり → 先頭を即 dispatch (以降は timer chain)
        dispatchNextSegmentLocked();
    }
}

void StackChanAvatarDisplay::dispatchNextSegmentLocked()
{
    if (segment_queue_.empty()) return;
    std::string seg = std::move(segment_queue_.front());
    segment_queue_.pop_front();

    auto& stackchan = GetStackChan();
    if (stackchan.hasAvatar()) {
        stackchan.avatar().appendSpeech(seg);
    }

    if (!segment_queue_.empty()) {
        // 次 segment までの delay = 表示中 segment の字数 × 200ms (TTS rate ~5 chars/sec)
        // 最低 200ms (1 文字以下でも間を取る)
        const uint32_t chars = static_cast<uint32_t>(utf8_char_count(seg));
        const uint32_t delay_ms = std::max<uint32_t>(200u, chars * 200u);
        segment_timer_ = lv_timer_create(&StackChanAvatarDisplay::onSegmentTimer, delay_ms, this);
        if (segment_timer_) {
            lv_timer_set_repeat_count(segment_timer_, 1);
        }
    }
}

void StackChanAvatarDisplay::clearSegmentQueue()
{
    segment_queue_.clear();
    if (segment_timer_) {
        lv_timer_delete(segment_timer_);
        segment_timer_ = nullptr;
    }
}

void StackChanAvatarDisplay::onSegmentTimer(lv_timer_t* t)
{
    auto* self = static_cast<StackChanAvatarDisplay*>(lv_timer_get_user_data(t));
    if (!self) return;
    // repeat_count=1 で auto-delete されるので pointer をクリア
    self->segment_timer_ = nullptr;
    // LVGL task context = lock 状態。直接 dispatch 可能
    self->dispatchNextSegmentLocked();
}

void StackChanAvatarDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image)
{
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        return;
    }

    if (image == nullptr) {
        esp_timer_stop(preview_timer_);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        return;
    }

    preview_image_cached_ = std::move(image);
    auto img_dsc          = preview_image_cached_->image_dsc();
    // Set image source and show preview image
    lv_image_set_src(preview_image_, img_dsc);
    if (img_dsc->header.w > 0 && img_dsc->header.h > 0) {
        // Scale to fit width
        lv_image_set_scale(preview_image_, 256 * width_ / img_dsc->header.w);
    }

    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(preview_image_);
    esp_timer_stop(preview_timer_);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, 6000 * 1000));
}

void StackChanAvatarDisplay::UpdateStatusBar(bool update_all)
{
}

void StackChanAvatarDisplay::SetTheme(Theme* theme)
{
    ESP_LOGI(TAG, "SetTheme: %s", theme->name().c_str());

    auto& stackchan = GetStackChan();
    if (!stackchan.hasAvatar()) {
        ESP_LOGE(TAG, "Avatar is invalid");
        return;
    }

    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font  = lvgl_theme->text_font()->font();

    stackchan.avatar().setSpeechTextFont((void*)text_font);
}

#include <hal/board/hal_bridge.h>
static bool _is_xiaozhi_ready = false;
static bool _is_xiaozhi_idle  = false;
bool hal_bridge::is_xiaozhi_ready()
{
    return _is_xiaozhi_ready;
}
bool hal_bridge::is_xiaozhi_idle()
{
    return _is_xiaozhi_idle;
}

void StackChanAvatarDisplay::SetStatus(const char* status)
{
    // ESP_LOGE(TAG, "SetStatus: %s", status);

    auto& stackchan = GetStackChan();
    if (!stackchan.hasAvatar()) {
        ESP_LOGE(TAG, "Avatar is invalid");
        return;
    }

    auto& avatar = stackchan.avatar();
    auto& motion = stackchan.motion();

    DisplayLockGuard lock(this);

    bool is_idle      = false;
    bool is_listening = false;

    if (strcmp(status, Lang::Strings::LISTENING) == 0) {
        if (speaking_modifier_id_ >= 0) {
            // Stop speaking
            stackchan.removeModifier(speaking_modifier_id_);
            avatar.mouth().setWeight(0);
            speaking_modifier_id_ = -1;
            // GOB fork: bubble FX が ON のときのみ dismiss 予約 (slide 完了 +
            // 3 秒)。OFF (upstream 挙動) では bubble は明示クリアまで残す。
            if (stackchan::gob_fork::get_bubble_fx_enabled()) {
                avatar.requestSpeechDismissAfterRender(3000);
            }
        }

        GetHAL().setRgbColor(0, 0, 50, 0);
        GetHAL().refreshRgb();

    } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
        _is_xiaozhi_ready = true;

        if (speaking_modifier_id_ >= 0) {
            // Stop speaking
            stackchan.removeModifier(speaking_modifier_id_);
            avatar.mouth().setWeight(0);
            speaking_modifier_id_ = -1;
        }

        is_idle = true;

        GetHAL().setRgbColor(0, 0, 0, 0);
        GetHAL().refreshRgb();

    } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
        if (speaking_modifier_id_ < 0) {
            speaking_modifier_id_ = stackchan.addModifier(std::make_unique<SpeakingModifier>(0, 180, false));
        }

        GetHAL().setRgbColor(0, 0, 0, 50);
        GetHAL().refreshRgb();
    } else {
        avatar.setSpeech(status);
    }

    if (is_idle) {
        // Start idle motion
        ESP_LOGW(TAG, "Start idle motion");
        if (idle_motion_modifier_id_ < 0) {
            idle_motion_modifier_id_     = stackchan.addModifier(std::make_unique<IdleMotionModifier>());
            idle_expression_modifier_id_ = stackchan.addModifier(std::make_unique<IdleExpressionModifier>());
        }

        _is_xiaozhi_idle = true;
    } else {
        // Stop idle motion
        ESP_LOGW(TAG, "Stop idle motion");
        if (idle_motion_modifier_id_ >= 0) {
            stackchan.removeModifier(idle_motion_modifier_id_);
            idle_motion_modifier_id_ = -1;
            stackchan.removeModifier(idle_expression_modifier_id_);
            idle_expression_modifier_id_ = -1;
        }

        // if (!is_listening) {
        //     // Return to default pose
        //     motion.pitchServo().moveWithSpeed(200, 350);
        //     motion.yawServo().moveWithSpeed(0, 350);
        // }

        _is_xiaozhi_idle = false;
    }

    // Clear sleep state
    if (is_sleeping_) {
        avatar.setSpeech("");
    }
}

void StackChanAvatarDisplay::ShowNotification(const char* notification, int duration_ms)
{
}
