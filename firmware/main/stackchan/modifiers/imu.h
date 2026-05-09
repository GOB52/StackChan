/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "../modifiable.h"
#include "../avatar/decorators/decorators.h"
#include <hal/hal.h>
#include <cstdint>
#include <memory>

namespace stackchan {

/**
 * @brief
 *
 */
class ImuEventModifier : public Modifier {
public:
    ImuEventModifier(uint32_t reactionDurationMs = 4000) : _reaction_duration_ms(reactionDurationMs)
    {
        _signal_connection = GetHAL().onImuMotionEvent.connect([this](ImuMotionEvent event) {
            if (event == ImuMotionEvent::Shake) {
                _event_shake = true;
            }
        });
    }

    ~ImuEventModifier()
    {
        GetHAL().onImuMotionEvent.disconnect(_signal_connection);
    }

    void _update(Modifiable& stackchan) override
    {
        uint32_t now = GetHAL().millis();

        // 收到晃动事件
        if (_event_shake) {
            _event_shake = false;
            handle_shake_start(stackchan, now);
        }

        // 如果处于晃动反应状态
        if (_is_reacting) {
            if (now >= _next_toggle_tick) {
                _next_toggle_tick = now + 600;

                _toggle_phase      = !_toggle_phase;
                int mouth_rotation = _toggle_phase ? -25 : 25;

                auto& avatar = stackchan.avatar();
                avatar.mouth().setRotation(mouth_rotation);
                avatar.mouth().setWeight(65);
            }

            //  Lock motion modify and move home
            auto& motion = stackchan.motion();
            if (!motion.isModifyLocked()) {
                motion.setModifyLock(true);
                motion.goHome(300);
            }

            // 检查是否结束反应
            if (now >= _restore_at) {
                restore_state(stackchan);
            }
        }
    }

private:
    void handle_shake_start(Modifiable& stackchan, uint32_t now)
    {
        if (!_is_reacting) {
            // 首次触发时，记录状态以便恢复
            _is_reacting = true;

            auto& avatar = stackchan.avatar();

            avatar.setModifyLock(true);
            avatar.leftEye().setVisible(false);
            avatar.rightEye().setVisible(false);

            stackchan.avatar().removeDecorator(_dizzy_decorator_id);
            stackchan.avatar().removeDecorator(_shy_decorator_id);

            // GOB fork: per-skin Dizzy positioning + manifest-driven color /
            // anim / scale. Position follows actual eye centers via avatar
            // virtual; DefaultAvatar returns built-in defaults so behavior
            // there is unchanged.
            const auto* dz_cfg = avatar.getDizzyConfig();
            uint32_t dz_anim   = dz_cfg ? dz_cfg->anim_ms : 300;
            auto dizzy = std::make_unique<avatar::DizzyDecorator>(lv_screen_active(), 0, dz_anim);
            if (dz_cfg && dz_cfg->has_color) {
                dizzy->setColor(lv_color_hex(dz_cfg->color_hex));
            }
            if (dz_cfg && dz_cfg->has_scale) {
                dizzy->setScale(dz_cfg->scale);
            }
            // Position priority: manifest dizzy.position > eye-center auto-derive.
            int lx, ly, rx, ry;
            if (dz_cfg && dz_cfg->has_position) {
                lx = dz_cfg->left_x;  ly = dz_cfg->left_y;
                rx = dz_cfg->right_x; ry = dz_cfg->right_y;
            } else {
                auto eye_l = avatar.getEyeCenterOffset(true);
                auto eye_r = avatar.getEyeCenterOffset(false);
                lx = eye_l.x; ly = eye_l.y;
                rx = eye_r.x; ry = eye_r.y;
            }
            dizzy->setLeftRightPosition(lx, ly, rx, ry);
            _dizzy_decorator_id = avatar.addDecorator(std::move(dizzy));

            // Shy reuses head_pet config (cheek positions are the same
            // semantic placement). Falls back to ShyDecorator defaults when
            // the skin doesn't override.
            auto shy = std::make_unique<avatar::ShyDecorator>(lv_screen_active(), 0);
            const auto* hp_cfg = avatar.getHeadPetConfig();
            if (hp_cfg && hp_cfg->has_shy) {
                shy->setLeftRightPosition(hp_cfg->shy_left_x, hp_cfg->shy_left_y,
                                          hp_cfg->shy_right_x, hp_cfg->shy_right_y);
            }
            _shy_decorator_id = avatar.addDecorator(std::move(shy));
        }

        // 刷新恢复时间和切换时间
        _restore_at = now + _reaction_duration_ms;
        if (_next_toggle_tick <= now) {
            _next_toggle_tick = now;  // 立即触发第一次嘴巴动作
        }
    }

    void restore_state(Modifiable& stackchan)
    {
        if (!_is_reacting) {
            return;
        }

        auto& avatar = stackchan.avatar();
        avatar.setModifyLock(false);
        avatar.leftEye().setVisible(true);
        avatar.rightEye().setVisible(true);
        avatar.mouth().setWeight(0);
        avatar.mouth().setRotation(0);

        stackchan.avatar().removeDecorator(_dizzy_decorator_id);
        stackchan.avatar().removeDecorator(_shy_decorator_id);
        _dizzy_decorator_id = -1;
        _shy_decorator_id   = -1;

        auto& motion = stackchan.motion();
        motion.setModifyLock(false);

        _is_reacting = false;
    }

    // 信号相关
    int _signal_connection;
    volatile bool _event_shake = false;

    // 状态控制
    bool _is_reacting          = false;
    bool _toggle_phase         = false;
    uint32_t _restore_at       = 0;
    uint32_t _next_toggle_tick = 0;
    uint32_t _reaction_duration_ms;

    int _dizzy_decorator_id = -1;
    int _shy_decorator_id   = -1;
};

}  // namespace stackchan
