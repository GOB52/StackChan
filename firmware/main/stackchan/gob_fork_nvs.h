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
bool        set_screensaver_timeout_s(uint32_t s);

// --- NFC enable (NVS key: "nfc_enabled") ------------------------------------
// Default OFF: UnitNFC poll task interferes with audio codec / DSP timing
// on the shared I2C bus and causes AFE FEED ringbuffer overruns when running
// alongside xiaozhi. Users opt in explicitly when they want NFC.
bool        get_nfc_enabled();
bool        set_nfc_enabled(bool enabled);

}  // namespace stackchan::gob_fork
