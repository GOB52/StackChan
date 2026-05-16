// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "gob_fork_nvs.h"
#include <nvs.h>
#include <nvs_flash.h>

namespace stackchan::gob_fork {

namespace {

constexpr const char* _key_skin_current   = "skin_current";
constexpr const char* _key_screensaver_to = "scr_timeout_s";
#if CONFIG_GOB_FORK_ENABLE_NFC
constexpr const char* _key_nfc_enabled    = "nfc_enabled";
#endif
constexpr const char* _key_time_24h       = "time_24h";
constexpr const char* _key_bubble_fx      = "bubble_fx";
#if CONFIG_GOB_FORK_ENABLE_ERROR_TOAST
constexpr const char* _key_error_toast    = "error_toast";
#endif

}  // namespace

std::string get_skin_current()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return "";
    }
    size_t len = 0;
    if (nvs_get_str(h, _key_skin_current, nullptr, &len) != ESP_OK || len == 0 || len > 64) {
        nvs_close(h);
        return "";
    }
    std::string out(len, '\0');
    if (nvs_get_str(h, _key_skin_current, out.data(), &len) != ESP_OK) {
        nvs_close(h);
        return "";
    }
    nvs_close(h);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

bool set_skin_current(const std::string& id)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (nvs_set_str(h, _key_skin_current, id.c_str()) == ESP_OK)
                    && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

bool clear_skin_current()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const esp_err_t e = nvs_erase_key(h, _key_skin_current);
    const bool ok = (e == ESP_OK || e == ESP_ERR_NVS_NOT_FOUND) && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

uint32_t get_screensaver_timeout_s()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return SCREENSAVER_TIMEOUT_DEFAULT_S;
    }
    uint32_t v = SCREENSAVER_TIMEOUT_DEFAULT_S;
    nvs_get_u32(h, _key_screensaver_to, &v);  // ignore error: keep default
    nvs_close(h);
    return v;
}

bool set_screensaver_timeout_s(const uint32_t s)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (nvs_set_u32(h, _key_screensaver_to, s) == ESP_OK)
                    && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

#if CONFIG_GOB_FORK_ENABLE_NFC
bool get_nfc_enabled()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t v = 0;  // default OFF
    nvs_get_u8(h, _key_nfc_enabled, &v);  // ignore error: keep default
    nvs_close(h);
    return v != 0;
}

bool set_nfc_enabled(const bool enabled)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (nvs_set_u8(h, _key_nfc_enabled, enabled ? 1 : 0) == ESP_OK)
                    && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}
#endif  // CONFIG_GOB_FORK_ENABLE_NFC

bool get_time_format_24h()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t v = 0;  // default 12H (false)
    nvs_get_u8(h, _key_time_24h, &v);  // ignore error: keep default
    nvs_close(h);
    return v != 0;
}

bool set_time_format_24h(const bool use24h)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (nvs_set_u8(h, _key_time_24h, use24h ? 1 : 0) == ESP_OK)
                    && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

bool get_bubble_fx_enabled()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return true;  // default ON
    uint8_t v = 1;  // default ON
    nvs_get_u8(h, _key_bubble_fx, &v);  // ignore error: keep default
    nvs_close(h);
    return v != 0;
}

bool set_bubble_fx_enabled(const bool enabled)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (nvs_set_u8(h, _key_bubble_fx, enabled ? 1 : 0) == ESP_OK)
                    && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}

#if CONFIG_GOB_FORK_ENABLE_ERROR_TOAST
bool get_error_toast_enabled()
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return true;  // default ON
    uint8_t v = 1;  // default ON
    nvs_get_u8(h, _key_error_toast, &v);  // ignore error: keep default
    nvs_close(h);
    return v != 0;
}

bool set_error_toast_enabled(const bool enabled)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return false;
    const bool ok = (nvs_set_u8(h, _key_error_toast, enabled ? 1 : 0) == ESP_OK)
                    && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
}
#endif  // CONFIG_GOB_FORK_ENABLE_ERROR_TOAST

}  // namespace stackchan::gob_fork
