/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// Modified by GOB (X:@GOB_52_GOB / GitHub:GOB52) - StackChan firmware fork
#pragma once
#include "../avatar/decorator.h"
#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace stackchan::avatar {

/**
 * @brief
 *
 */
class HeartDecorator : public Decorator {
public:
    /**
     * @brief
     *
     * @param parent
     * @param destroyAfterMs Destroy after milliseconds, 0 for infinite
     * @param animationIntervalMs Animation update interval in milliseconds, 0 for none
     */
    HeartDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0, uint32_t animationIntervalMs = 500);
    ~HeartDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    void setRotation(int rotation) override;
    void setColor(lv_color_t color);

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _heart;

    uint32_t _destroy_at            = 0;
    uint32_t _next_animation_tick   = 0;
    uint32_t _animation_interval_ms = 0;
    bool _has_lifetime              = false;

    int _animation_index = 0;
};

/**
 * @brief
 *
 */
class AngryDecorator : public Decorator {
public:
    /**
     * @brief
     *
     * @param parent
     * @param destroyAfterMs Destroy after milliseconds, 0 for infinite
     * @param animationIntervalMs Animation update interval in milliseconds, 0 for none
     */
    AngryDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0, uint32_t animationIntervalMs = 500);
    ~AngryDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    void setRotation(int rotation) override;
    void setColor(lv_color_t color);

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _angry;

    uint32_t _destroy_at            = 0;
    uint32_t _next_animation_tick   = 0;
    uint32_t _animation_interval_ms = 0;
    bool _has_lifetime              = false;

    int _animation_index = 0;
};

/**
 * @brief
 *
 */
class SweatDecorator : public Decorator {
public:
    /**
     * @brief
     *
     * @param parent
     * @param destroyAfterMs Destroy after milliseconds, 0 for infinite
     * @param animationIntervalMs Animation update interval in milliseconds, 0 for none
     */
    SweatDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0, uint32_t animationIntervalMs = 700);
    ~SweatDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    void setRotation(int rotation) override;
    void setColor(lv_color_t color);
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _sweat;

    uint32_t _destroy_at            = 0;
    uint32_t _next_animation_tick   = 0;
    uint32_t _animation_interval_ms = 0;
    bool _has_lifetime              = false;

    int _animation_index = 0;
    int _base_x          = 0;
    int _base_y          = 0;
};

/**
 * @brief
 *
 */
class ShyDecorator : public Decorator {
public:
    /**
     * @brief
     *
     * @param parent
     * @param destroyAfterMs Destroy after milliseconds, 0 for infinite
     */
    ShyDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0);
    ~ShyDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    // GOB fork: independent left / right cheek positions (LV_ALIGN_CENTER offset).
    // Use this when the avatar's cheeks are not L/R-symmetric around screen center.
    void setLeftRightPosition(int lx, int ly, int rx, int ry);
    void setRotation(int rotation) override;
    void setColor(lv_color_t color);
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _left;
    std::unique_ptr<uitk::lvgl_cpp::Image> _right;

    uint32_t _destroy_at = 0;
    bool _has_lifetime   = false;
};

/**
 * @brief
 *
 */
class DizzyDecorator : public Decorator {
public:
    /**
     * @brief
     *
     * @param parent
     * @param destroyAfterMs Destroy after milliseconds, 0 for infinite
     * @param animationIntervalMs Animation update interval in milliseconds, 0 for none
     */
    DizzyDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0, uint32_t animationIntervalMs = 500);
    ~DizzyDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    // GOB fork: independent left / right eye-center positions (LV_ALIGN_CENTER
    // offset). Used by IMUModifier to follow per-skin eye layout.
    void setLeftRightPosition(int lx, int ly, int rx, int ry);
    void setRotation(int rotation) override;
    void setColor(lv_color_t color);
    // GOB fork: scale multiplier (1.0f = 100%, internally LVGL 256-based).
    void setScale(float scale);
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _left;
    std::unique_ptr<uitk::lvgl_cpp::Image> _right;

    uint32_t _destroy_at            = 0;
    uint32_t _next_animation_tick   = 0;
    uint32_t _animation_interval_ms = 0;
    bool _has_lifetime              = false;

    int _animation_index = 0;
};

// ---------------------------------------------------------------------------
// Below are added by GOB (StackChan firmware fork) for ImageAvatar.
// SleepyDecorator: PNG image "ZZZ" floating upward and fading out, then loops.
// DoubtDecorator:  PNG image "?" wiggling (rotation) like Heart.
// ---------------------------------------------------------------------------

class SleepyDecorator : public Decorator {
public:
    SleepyDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0, uint32_t animationIntervalMs = 200);
    ~SleepyDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    void setColor(lv_color_t color);
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _zzz;

    uint32_t _destroy_at            = 0;
    uint32_t _next_animation_tick   = 0;
    uint32_t _animation_interval_ms = 0;
    bool _has_lifetime              = false;

    int _base_x   = 0;
    int _base_y   = 0;
    int _y_offset = 0;  // base_y からの上方向オフセット (負方向に浮上)
    int _opa      = 0;  // 0..255
};

class DoubtDecorator : public Decorator {
public:
    DoubtDecorator(lv_obj_t* parent, uint32_t destroyAfterMs = 0, uint32_t animationIntervalMs = 500);
    ~DoubtDecorator();

    void _update() override;

    using Element::setPosition;

    void setPosition(int x, int y);
    void setRotation(int rotation) override;
    void setColor(lv_color_t color);
    void setVisible(bool visible) override;

private:
    std::unique_ptr<uitk::lvgl_cpp::Image> _question;

    uint32_t _destroy_at            = 0;
    uint32_t _next_animation_tick   = 0;
    uint32_t _animation_interval_ms = 0;
    bool _has_lifetime              = false;

    int _animation_index = 0;
};

}  // namespace stackchan::avatar
