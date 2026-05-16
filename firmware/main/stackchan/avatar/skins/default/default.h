/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../../avatar/avatar.h"
#include "../../avatar/elements/feature.h"
#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <memory>

namespace stackchan::avatar {

/**
 * @brief
 *
 */
class DefaultAvatar : public Avatar {
public:
    lv_color_t primaryColor   = lv_color_white();
    lv_color_t secondaryColor = lv_color_black();

    void init(lv_obj_t* parent, const lv_font_t* font = &lv_font_montserrat_16);
    uitk::lvgl_cpp::Container* getPanel() const;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _pannel;
};

/**
 * @brief
 *
 */
class DefaultEyes : public Feature {
public:
    DefaultEyes(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor, bool isLeftEye);
    ~DefaultEyes();

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setEmotion(const Emotion& emotion) override;
    void setVisible(bool visible) override;
    void setSize(int size) override;

private:
    bool _is_left_eye    = false;
    int _eyelid_offset_y = 0;

    std::unique_ptr<uitk::lvgl_cpp::Container> _container;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eye;
    std::unique_ptr<uitk::lvgl_cpp::Container> _eyelid;
};

/**
 * @brief
 *
 */
class DefaultMouth : public Feature {
public:
    DefaultMouth(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor);
    ~DefaultMouth();

    void setPosition(const uitk::Vector2i& position) override;
    void setWeight(int weight) override;
    void setRotation(int rotation) override;
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _mouth;
};

/**
 * @brief
 *
 */
class DefaultSpeechBubble : public SpeechBubble {
public:
    DefaultSpeechBubble(lv_obj_t* parent, lv_color_t primaryColor, lv_color_t secondaryColor, const lv_font_t* font);
    ~DefaultSpeechBubble();

    void setSpeech(std::string_view text) override;
    void appendSpeech(std::string_view text) override;
    void requestDismissAfterRender(uint32_t delay_ms) override;
    void clearSpeech() override;
    void setVisible(bool visible) override;
    void setTextFont(void* font) override;

private:
    // GOB fork: text を内部で蓄積し、bubble 幅と label 位置を再計算して
    // optionally slide animation を起動する共通ヘルパ。
    // animated_chars > 0 で animation 有効 (duration は slide distance に比例)。
    // CLIP mode + 手動 label.x 制御。appendSpeech 経路で使用。
    void renderAndPosition(size_t animated_chars);

    // GOB fork: upstream 互換 (SCROLL_CIRCULAR + center) のレンダリング。
    // setSpeech (= status msg / FX OFF assistant) で使用。LVGL ネイティブ
    // 循環スクロールに任せる。
    void renderAsVanilla();

    // GOB fork: dismiss timer 管理 (LVGL one-shot)
    void armDismissTimer(uint32_t delay_ms);
    void cancelDismissTimer();
    static void onSlideCompleted(lv_anim_t* a);
    static void onDismissTimer(lv_timer_t* t);

    std::unique_ptr<uitk::lvgl_cpp::Container> _container;
    std::unique_ptr<uitk::lvgl_cpp::Image> _arrow;
    std::unique_ptr<uitk::lvgl_cpp::Container> _bubble;
    // GOB fork: bubble の内側 inner_w (= bubble_w - 2*margin) でテキストを clip
    // するためのコンテナ。label はこの子として配置し、label.x はこの clip 内座標。
    // これにより bubble の左右 margin (角丸領域) を text が侵食しない。
    std::unique_ptr<uitk::lvgl_cpp::Container> _text_clip;
    std::unique_ptr<uitk::lvgl_cpp::Label> _text;

    // GOB fork: utterance accumulator + tail-follow scroll state
    std::string _accumulated;
    bool        _streaming     = false;
    int32_t     _label_x_logical = 0;  // 直近 anim の target (実位置は lv_obj_get_x で取得)

    // GOB fork: dismiss timer state
    bool        _slide_active = false;             // animation 走行中
    uint32_t    _dismiss_pending_delay_ms = 0;     // anim 完了後に arm したい delay (0 = 保留なし)
    lv_timer_t* _dismiss_timer = nullptr;           // 走行中 LVGL timer (nullptr = 無)

    // GOB fork: '%' prefix chunk (LLM tool call indicator) との境界判定。
    // regular ↔ '%' の遷移で accumulator をクリアする。
    bool        _last_was_tool_indicator = false;
};

}  // namespace stackchan::avatar
