/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// Modified by GOB (X:@GOB_52_GOB / GitHub:GOB52) - StackChan firmware fork
#include "app_avatar.h"
#include "view/ws_call.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>
#include <smooth_lvgl.hpp>
#include <stackchan/stackchan.h>
#include <apps/common/common.h>

// 0 = DefaultAvatar (upstream), 1 = ponko ImageAvatar (StackChan fork by GOB)
#define USE_PONKO_AVATAR 1

// 1 = cycle Neutral/Happy/Angry/Sad every 5s for emotion + decorator pipeline check
#define ENABLE_EMOTION_TEST 1

// 1 = always-on SpeakingModifier (mouth flapping) for mouth animation check
#define ENABLE_SPEAKING_TEST 1

#if USE_PONKO_AVATAR
#include <stackchan/avatar/skins/image/presets/ponko_preset.h>
#endif
#include <string_view>
#include <cstdint>
#include <memory>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;
using namespace stackchan;

#include <string>
#include <sstream>
#include <unordered_set>

static bool contains_word(const std::string& text, const std::unordered_set<std::string>& words)
{
    auto to_lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    };

    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        token = to_lower(token);
        if (words.find(token) != words.end()) {
            return true;
        }
    }
    return false;
}

AppAvatar::AppAvatar()
{
    // 配置 App 名
    setAppInfo().name = "AVATAR";
    // 配置 App 图标
    static auto icon  = assets::get_image("icon_sentinel.bin");
    setAppInfo().icon = (void*)&icon;
    // 配置 App 主题颜色
    static uint32_t theme_color = 0xFF6699;
    setAppInfo().userData       = (void*)&theme_color;
}

// App 被安装时会被调用
void AppAvatar::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppAvatar::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    // Create loading page
    std::unique_ptr<view::LoadingPage> loading_page;
    {
        LvglLockGuard lock;
        loading_page = std::make_unique<view::LoadingPage>(0xFF6699, 0x431525);
    }

    // Start avatar service
    GetHAL().startWebSocketAvatarService([&](std::string_view msg) {
        LvglLockGuard lock;
        loading_page->setMessage(msg);
    });
    // GetHAL().startBleServer();

    LvglLockGuard lock;

    // Destroy loading page
    loading_page.reset();

    // Create avatar (Default or ponko, switched via USE_PONKO_AVATAR)
#if USE_PONKO_AVATAR
    auto avatar = avatar::image::make_ponko_avatar();
#else
    auto avatar = std::make_unique<avatar::DefaultAvatar>();
#endif
    avatar->init(lv_screen_active());
    GetStackChan().attachAvatar(std::move(avatar));

    // Register autonomous animation modifiers (StackChan fork by GOB)
    GetStackChan().addModifier(std::make_unique<BlinkModifier>());
#if ENABLE_SPEAKING_TEST
    // Continuous mouth flapping (motion disabled: motion may be unattached in this app)
    GetStackChan().addModifier(std::make_unique<SpeakingModifier>(0, 180, false));
#endif

    /* ------------------------------- BLE events ------------------------------- */
    GetHAL().onBleAvatarData.connect([&](const char* data) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_ble_avatar_data.update_flag) {
            return;
        }
        _ble_avatar_data.update_flag = true;
        _ble_avatar_data.data_ptr    = (char*)data;
    });

    GetHAL().onBleMotionData.connect([&](const char* data) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_ble_motion_data.update_flag) {
            return;
        }
        _ble_motion_data.update_flag = true;
        _ble_motion_data.data_ptr    = (char*)data;
    });

    /* ---------------------------- Websocket events ---------------------------- */
    // Avatar control
    GetHAL().onWsAvatarData.connect([&](std::string_view data) {
        LvglLockGuard lvgl_lock;
        GetStackChan().updateAvatarFromJson(data.data());
    });

    // Motion control
    GetHAL().onWsMotionData.connect([&](std::string_view data) {
        LvglLockGuard lvgl_lock;
        check_auto_angle_sync_mode();
        GetStackChan().updateMotionFromJson(data.data());
    });

    // Phone call handling
    GetHAL().onWsCallRequest.connect([&](std::string caller) {
        if (_ws_call_view_id >= 0) {
            mclog::tagWarn(getAppInfo().name, "ws call view already exists");
            return;
        }

        LvglLockGuard lvgl_lock;

        auto& avatar = GetStackChan().avatar();
        avatar.setSpeech("");
        avatar.leftEye().setVisible(false);
        avatar.rightEye().setVisible(false);
        avatar.mouth().setVisible(false);

        auto view      = std::make_unique<view::WsCallView>(lv_screen_active(), caller);
        view->onAccept = []() {
            auto& avatar = GetStackChan().avatar();
            avatar.setSpeech("");
            avatar.leftEye().setVisible(true);
            avatar.rightEye().setVisible(true);
            avatar.mouth().setVisible(true);

            GetHAL().onWsCallResponse.emit(true);
        };
        view->onDecline = []() {
            auto& avatar = GetStackChan().avatar();
            avatar.setSpeech("");
            avatar.leftEye().setVisible(true);
            avatar.rightEye().setVisible(true);
            avatar.mouth().setVisible(true);

            GetHAL().onWsCallResponse.emit(false);
        };
        view->onEnd     = []() { GetHAL().onWsCallEnd.emit(WsSignalSource::Local); };
        view->onDestory = [&]() { _ws_call_view_id = -1; };

        _ws_call_view_id = avatar.addDecorator(std::move(view));
    });

    GetHAL().onWsCallEnd.connect([&](WsSignalSource source) {
        if (source != WsSignalSource::Remote) {
            return;
        }

        LvglLockGuard lvgl_lock;

        if (_ws_call_view_id < 0) {
            mclog::tagWarn(getAppInfo().name, "ws call view does not exist");
            return;
        }

        auto& avatar = GetStackChan().avatar();
        avatar.setSpeech("");
        avatar.leftEye().setVisible(true);
        avatar.rightEye().setVisible(true);
        avatar.mouth().setVisible(true);

        avatar.removeDecorator(_ws_call_view_id);
        _ws_call_view_id = -1;
    });

    // Text message handling
    GetHAL().onWsTextMessage.connect([&](const WsTextMessage_t& message) {
        LvglLockGuard lvgl_lock;

        auto& stackchan = GetStackChan();

        stackchan.addModifier(
            std::make_unique<TimedSpeechModifier>(fmt::format("{} says: {}", message.name, message.content), 6000));
        stackchan.addModifier(std::make_unique<SpeakingModifier>(2000));

        // Special handling
        if (contains_word(message.content, {"hello", "hi"})) {
            stackchan.addModifier(std::make_unique<TimedEmotionModifier>(avatar::Emotion::Happy, 2000));
        } else if (contains_word(message.content, {"love"})) {
            stackchan.addModifier(std::make_unique<TimedEmotionModifier>(avatar::Emotion::Happy, 2000));
        }
    });

    GetHAL().onWsDanceData.connect([&](std::string_view data) {
        LvglLockGuard lvgl_lock;
        auto sequence = stackchan::animation::parse_sequence_from_json(data.data());
        if (!sequence.empty()) {
            GetStackChan().addModifier(std::make_unique<DanceModifier>(sequence));
        }
    });

    GetHAL().onWsLog.connect([&](CommonLogLevel level, std::string_view msg) {
        auto type         = static_cast<view::ToastType>(level);
        uint32_t duration = type == view::ToastType::Error ? 12000 : 1600;
        view::pop_a_toast(msg, type, duration);
    });

    /* ------------------------------ Video window ------------------------------ */
    _video_window = std::make_unique<view::VideoWindow>(lv_screen_active());

    /* ----------------------------- Common widgets ----------------------------- */
    view::create_home_indicator([&]() { close(); }, 0xFF9ABC, 0x431525);
    view::create_status_bar(0xFF9ABC, 0x431525);
}

void AppAvatar::onRunning()
{
    std::lock_guard<std::mutex> lock(_mutex);

    LvglLockGuard lvgl_lock;

    if (_ble_avatar_data.update_flag) {
        GetStackChan().updateAvatarFromJson(_ble_avatar_data.data_ptr);
        _ble_avatar_data.update_flag = false;
        _ble_avatar_data.data_ptr    = nullptr;
    }

    if (_ble_motion_data.update_flag) {
        check_auto_angle_sync_mode();
        GetStackChan().updateMotionFromJson(_ble_motion_data.data_ptr);
        _ble_motion_data.update_flag = false;
        _ble_motion_data.data_ptr    = nullptr;
    }

#if ENABLE_EMOTION_TEST
    {
        static uint32_t last_emo_tick = 0;
        static int emo_idx            = 0;
        if (GetHAL().millis() - last_emo_tick > 5000) {
            last_emo_tick = GetHAL().millis();
            const avatar::Emotion emos[] = {
                avatar::Emotion::Neutral,
                avatar::Emotion::Happy,
                avatar::Emotion::Angry,
                avatar::Emotion::Sad,
                avatar::Emotion::Doubt,
                avatar::Emotion::Sleepy,
            };
            const char* names[] = {"Neutral", "Happy", "Angry", "Sad", "Doubt", "Sleepy"};
            constexpr int EMO_COUNT = sizeof(emos) / sizeof(emos[0]);
            mclog::tagInfo(getAppInfo().name, "emotion test: {}", names[emo_idx]);
            GetStackChan().avatar().setEmotion(emos[emo_idx]);
            GetStackChan().avatar().setSpeech(names[emo_idx]);  // 吹き出しに emotion 名表示
            emo_idx = (emo_idx + 1) % EMO_COUNT;
        }
    }
#endif

    GetStackChan().update();

    view::update_home_indicator();
    view::update_status_bar();
}

void AppAvatar::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    {
        LvglLockGuard lock;

        GetStackChan().resetAvatar();
        _video_window.reset();

        GetHAL().onBleAvatarData.clear();
        GetHAL().onBleMotionData.clear();

        GetHAL().onWsAvatarData.clear();
        GetHAL().onWsMotionData.clear();
        GetHAL().onWsCallRequest.clear();
        GetHAL().onWsCallEnd.clear();
        GetHAL().onWsTextMessage.clear();
        GetHAL().onWsDanceData.clear();

        view::destroy_home_indicator();
        view::destroy_status_bar();
    }

    GetHAL().requestWarmReboot(1);
}

void AppAvatar::check_auto_angle_sync_mode()
{
    auto& motion = GetStackChan().motion();

    // If far from last command, enable auto angle sync
    if (GetHAL().millis() - _last_motion_cmd_tick > 2000) {
        motion.setAutoAngleSyncEnabled(true);
    } else {
        motion.setAutoAngleSyncEnabled(false);
    }

    _last_motion_cmd_tick = GetHAL().millis();
}
