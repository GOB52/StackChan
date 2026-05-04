/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// Modified by GOB (X:@GOB_52_GOB / GitHub:GOB52) - StackChan firmware fork
#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <lvgl.h>
#include <functional>
#include <smooth_ui_toolkit.hpp>
#include <uitk/short_namespace.hpp>
#include <smooth_lvgl.hpp>
#include <array>
#include <vector>
#include <lvgl_image.h>
#include <string_view>
#include <atomic>

/**
 * @brief
 *
 */
enum class HeadPetGesture { None, Press, Release, SwipeForward, SwipeBackward };

/**
 * @brief
 *
 */
enum class WsSignalSource {
    Local = 0,
    Remote,
};

/**
 * @brief
 *
 */
struct WsTextMessage_t {
    std::string name;
    std::string content;
};

/**
 * @brief
 *
 */
enum class ImuMotionEvent {
    None = 0,
    Shake,
    PickUp,
};

/**
 * @brief NFC tag detection event payload (GOB fork: M5Unit-NFC integration).
 * Phase 1a: UID + classified type only. NDEF / cmd payload decoding planned
 * for Phase 1b.
 */
struct NfcTagEvent_t {
    std::string uid;   // hex string (e.g. "04A1B2C3D4E5F6")
    std::string type;  // classified PICC type (e.g. "MIFARE Classic 1K")
};

/**
 * @brief stackchan:cmd NDEF record received from a PICC (GOB fork, Phase 1b).
 * Payload is the raw bytes of an NDEF MIME record whose type string equals
 * "application/vnd.stackchan.cmd+json" (RFC 6838 vendor tree). JSON parsing
 * and dispatch happen in Phase 1c.
 */
struct NfcCmdEvent_t {
    std::string          uid;      // tag UID for logging / future idempotency
    std::vector<uint8_t> payload;  // raw NDEF record payload bytes (UTF-8 JSON)
};

/**
 * @brief
 *
 */
enum class AppConfigEvent {
    None = 0,
    AppConnected,
    AppDisconnected,
    TryWifiConnect,
    WifiConnectFailed,
    WifiConnected,
};

/**
 * @brief
 *
 */
enum class CommonLogLevel {
    Info = 0,
    Warning,
    Error,
};

/**
 * @brief
 *
 */
namespace app_center {

struct AppInfo_t {
    std::string name;
    std::string iconUrl;
    std::string description;
    std::string firmwareUrl;
};

using AppInfoList_t = std::vector<AppInfo_t>;

};  // namespace app_center

/**
 * @brief
 *
 */
enum class WifiStatus {
    None = 0,
    Low,
    Medium,
    High,
};

/**
 * @brief
 *
 */
struct UserAccountInfo_t {
    std::string username;
    std::string deviceName;
};

/**
 * @brief
 *
 */
class BootLogo {
public:
    BootLogo()
    {
        _panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
        _panel->setSize(320, 240);
        _panel->setAlign(LV_ALIGN_CENTER);
        _panel->setBorderWidth(0);
        _panel->setBgOpa(0);
        _panel->setPaddingAll(0);

        _label_logo = std::make_unique<uitk::lvgl_cpp::Label>(_panel->get());
        _label_logo->setTextFont(&lv_font_montserrat_24);
        _label_logo->setTextColor(lv_color_hex(0xFFFFFF));
        _label_logo->align(LV_ALIGN_CENTER, 0, -28);
        _label_logo->setText("STACKCHAN");

        // StackChan firmware fork - added by GOB (X:@GOB_52_GOB / GitHub:GOB52)
        _label_subtitle = std::make_unique<uitk::lvgl_cpp::Label>(_panel->get());
        _label_subtitle->setTextFont(&lv_font_montserrat_16);
        _label_subtitle->setTextColor(lv_color_hex(0x4CAF50));
        _label_subtitle->align(LV_ALIGN_CENTER, 0, -2);
        _label_subtitle->setText("GOB fork");

        _label_msg = std::make_unique<uitk::lvgl_cpp::Label>(_panel->get());
        _label_msg->setTextFont(&lv_font_montserrat_16);
        _label_msg->setTextColor(lv_color_hex(0xBFBFBF));
        _label_msg->align(LV_ALIGN_CENTER, 0, 22);
        _label_msg->setText("Starting up ...");

        _label_version = std::make_unique<uitk::lvgl_cpp::Label>(_panel->get());
        _label_version->setTextFont(&lv_font_montserrat_14);
        _label_version->setTextColor(lv_color_hex(0x8B8B8B));
        _label_version->align(LV_ALIGN_BOTTOM_RIGHT, -7, -6);
        _label_version->setText("V" FIRMWARE_VERSION);
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label_logo;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label_subtitle;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label_msg;
    std::unique_ptr<uitk::lvgl_cpp::Label> _label_version;
};

/**
 * @brief
 *
 */
class Hal {
public:
    void init();

    /* --------------------------------- System --------------------------------- */
    void delay(std::uint32_t ms);
    std::uint32_t millis();
    void feedTheDog();
    std::array<uint8_t, 6> getFactoryMac();
    std::string getFactoryMacString(std::string divider = "");
    void reboot();
    void updateHeapStatusLog();
    uint8_t getBatteryLevel();
    bool isBatteryCharging();
    void factoryReset();

    /* --------------------------------- Display -------------------------------- */
    lv_indev_t* lvTouchpad = nullptr;
    std::unique_ptr<BootLogo> bootLogo;
    void lvglLock();
    void lvglUnlock();
    void setBackLightBrightness(uint8_t brightness, bool permanent = false);
    uint8_t getBackLightBrightness();

    /* --------------------------------- Xiaozhi -------------------------------- */
    void requestXiaozhiStart()
    {
        _xiaozhi_start_requested = true;
    }
    bool isXiaozhiStartRequested()
    {
        return _xiaozhi_start_requested;
    }
    void startXiaozhi();

    /* ----------------------------------- BLE ---------------------------------- */
    uitk::Signal<const char*> onBleMotionData;
    uitk::Signal<const char*> onBleAvatarData;
    uitk::Signal<const char*> onBleConfigData;
    uitk::Signal<const char*> onBleRgbData;
    uitk::Signal<AppConfigEvent> onAppConfigEvent;

    void startBleServer();
    bool isBleConnected();
    void startAppConfigServer();
    bool isAppConfiged();
    void resetAppConfiged();

    /* --------------------------------- HeadPet -------------------------------- */
    uitk::Signal<HeadPetGesture> onHeadPetGesture;

    /* ----------------------------------- RGB ---------------------------------- */
    void setRgbColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
    void showRgbColor(uint8_t r, uint8_t g, uint8_t b);
    void refreshRgb();

    /* ---------------------------------- Power --------------------------------- */
    void setServoPowerEnabled(bool enabled);

    /* -------------------------------- Websocket ------------------------------- */
    uitk::Signal<std::string_view> onWsMotionData;
    uitk::Signal<std::string_view> onWsAvatarData;
    uitk::Signal<std::string> onWsCallRequest;
    uitk::Signal<bool> onWsCallResponse;
    uitk::Signal<WsSignalSource> onWsCallEnd;
    uitk::Signal<const WsTextMessage_t&> onWsTextMessage;
    uitk::Signal<bool> onWsVideoModeChange;
    uitk::Signal<std::shared_ptr<LvglImage>> onWsVideoFrame;
    uitk::Signal<std::string_view> onWsDanceData;
    uitk::Signal<CommonLogLevel, std::string_view> onWsLog;

    void startWebSocketAvatarService(std::function<void(std::string_view)> onStartLog);

    /* ----------------------------------- IMU ---------------------------------- */
    uitk::Signal<ImuMotionEvent> onImuMotionEvent;

    /* ----------------------------------- NFC ---------------------------------- */
    // GOB fork: M5Unit-NFC tag detection. Phase 1a — UID/type only.
    uitk::Signal<const NfcTagEvent_t&> onNfcTagDetected;
    // GOB fork: stackchan:cmd NDEF record received (Phase 1b). Emitted once per
    // matching External Type record on the tag; payload is raw bytes.
    uitk::Signal<const NfcCmdEvent_t&> onNfcCmdReceived;
    // Lazily initialize UnitNFC + start poll task. Called from the
    // stackchan update task once xiaozhi is ready, so that the audio
    // codec init (ES7210 / AW88298) gets the I2C bus first without
    // contention from the NFC poll loop.
    void startNfc();
    // True while the NFC poll task is mid-iteration (detect / identify /
    // NDEF read in progress). Other I2C consumers on a different core
    // (notably the FT6336 touchpad esp_timer on core 0) check this and
    // skip their poll cycle to avoid cross-core I2C contention.
    static bool isNfcBusy();

    /* ---------------------------------- Time ---------------------------------- */
    void syncRtcTimeToSystem();
    void syncSystemTimeToRtc();
    void setTimezone(std::string_view tz);
    std::string getTimezone();
    // Called from SNTP cb (tcpip_thread); sets pending flag, actual I2C/RTC write
    // is deferred to main task via tickSntpRtcSyncIfPending() to avoid stack overflow.
    void requestRtcSyncFromSntp();
    void tickSntpRtcSyncIfPending();

    /* --------------------------------- EspNow --------------------------------- */
    uitk::Signal<const std::vector<uint8_t>&> onEspNowData;
    void startEspNow(int channel);
    bool espNowSend(const std::vector<uint8_t>& data, const uint8_t* destAddr = nullptr);
    void setLaserEnabled(bool enabled);

    /* ------------------------------- Warm Reboot ------------------------------ */
    void requestWarmReboot(int appIndex);
    int getWarmRebootTarget();
    void clearWarmRebootRequest();

    /* --------------------------------- Network -------------------------------- */
    void startNetwork(std::function<void(std::string_view)> onLog);
    WifiStatus getWifiStatus();
    void startSntp();

    /* -------------------------------- App center ------------------------------- */
    app_center::AppInfoList_t fetchAppList();
    void launchApp(std::string_view url, std::function<void(int)> onProgress);

    /* --------------------------------- EzData --------------------------------- */
    void startEzDataService(std::function<void(std::string_view)> onStartLog);
    uitk::Signal<std::string_view> onEzdataPairCode;

    /* ------------------------------- User Acount ------------------------------ */
    UserAccountInfo_t getUserAccountInfo();
    bool updateAccountInfo(std::function<void(std::string_view)> onLog);
    bool unbindAccount(std::function<void(std::string_view)> onLog);

    /* ----------------------------------- OTA ---------------------------------- */
    bool updateFirmware(std::function<void(std::string_view)> onLog);

    /* ---------------------------------- Audio --------------------------------- */
    void setSpeakerVolume(uint8_t volume, bool permanent = false);
    uint8_t getSpeakerVolume();

private:
    bool _xiaozhi_start_requested = false;
    std::atomic<bool> _rtc_sync_pending{false};

    void xiaozhi_board_init();
    void lvgl_init();
    void xiaozhi_mcp_init();
    void ble_init(bool useAltUuid);
    void servo_init();
    void head_touch_init();
    void io_expander_init();
    void imu_init();
    void nfc_init();  // GOB fork: M5Unit-NFC
    void rtc_init();
};

Hal& GetHAL();

/**
 * @brief
 *
 */
class LvglLockGuard {
public:
    LvglLockGuard()
    {
        GetHAL().lvglLock();
    }
    ~LvglLockGuard()
    {
        GetHAL().lvglUnlock();
    }
};
