// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <cstdint>
#include <string>

namespace stackchan::gob_fork {

// Single NVS namespace for all GOB fork settings. Erasing this namespace
// (nvs_erase_namespace) effectively factory-resets the fork-specific config.
inline constexpr const char* NVS_NAMESPACE = "gob_fork";

// Sentinel skin id: forces DefaultAvatar (no SD load).
inline constexpr const char* FORCE_DEFAULT_AVATAR_ID = "__default__";

// Default screensaver timeout if NVS unset (seconds; matches upstream default).
inline constexpr uint32_t SCREENSAVER_TIMEOUT_DEFAULT_S = 30;

// --- Skin selection (NVS key: "skin_current") -------------------------------
// Returns persisted skin id, or empty string if unset / NVS error.
std::string get_skin_current();
// Persist skin selection. Returns true on success.
bool        set_skin_current(const std::string& id);
// Erase skin selection. After clear, the loader falls back to avatar.json's
// "current". Treats "key not found" as success.
bool        clear_skin_current();

// --- Screensaver timeout (NVS key: "scr_timeout_s") -------------------------
// Returns timeout in seconds (0 = Off). Returns SCREENSAVER_TIMEOUT_DEFAULT_S
// if unset or on NVS error.
uint32_t    get_screensaver_timeout_s();
// Persist screensaver timeout (seconds; 0 = Off). Returns true on success.
bool        set_screensaver_timeout_s(const uint32_t s);

// --- NFC enable (NVS key: "nfc_enabled") ------------------------------------
// Default OFF: UnitNFC poll task interferes with audio codec / DSP timing
// on the shared I2C bus and causes AFE FEED ringbuffer overruns when running
// alongside xiaozhi. Users opt in explicitly when they want NFC.
bool        get_nfc_enabled();
bool        set_nfc_enabled(const bool enabled);

// --- Status bar time format (NVS key: "time_24h") ---------------------------
// Default false (12H "3:45 PM" — matches upstream behavior).
// true selects 24H "15:45".
bool        get_time_format_24h();
bool        set_time_format_24h(const bool use24h);

// --- Speech bubble FX (NVS key: "bubble_fx") --------------------------------
// Default ON: utterance accumulator + tail-follow scroll + 1-sec dismiss +
// long-sentence segmentation. OFF falls back to upstream behavior (each
// chunk replaces previous text).
bool        get_bubble_fx_enabled();
bool        set_bubble_fx_enabled(const bool enabled);

// --- Error toast (NVS key: "error_toast") -----------------------------------
// Default ON: ESP_LOGE from a TAG whitelist (MCP / MQTT / Application / OTA /
// WifiStation) is surfaced as a red Toast on screen. Console output is
// preserved. Throttled to 1 toast per TAG per second.
bool        get_error_toast_enabled();
bool        set_error_toast_enabled(const bool enabled);

}  // namespace stackchan::gob_fork
