// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "skin_loader.h"
#include "../default/default.h"  // DefaultAvatar fallback
#include "../../avatar/elements/emotion.h"
#include <hal/board/sd_guard.h>
#include <stackchan/gob_fork_nvs.h>
#include <ArduinoJson.hpp>
#include <mooncake_log.h>
#include <fmt/format.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace stackchan::avatar::image {

static const char* _tag = "SkinLoader";

// ---- File I/O (SD only) -------------------------------------------------------
// GOB fork: AI Agent 起動時に稀発する SD 一過性 fopen 失敗 (xiaozhi 並走中の SPI3
// バス瞬断 / カード内部 GC 等が疑われる) への対策。fopen/fread を最大 3 回まで
// retry し、各試行で経過時間と errno を診断ログに残す。次回再発時の根本原因特定
// を容易にしつつ、transient な ~1s 級失敗は救う。
namespace {
constexpr int     MAX_FILE_RETRIES   = 3;
// GOB fork: SD card 内部 GC / wear-leveling は数百 ms〜数 s かかるため、50ms
// では復帰機会を見逃す。実機で 0x107 (ESP_ERR_TIMEOUT) を捉えた診断ログ
// (`sdmmc_send_cmd` 1 秒タイムアウトが 3 回連続) から、500ms backoff に拡大。
// 合計待ち時間: 3 × 1s (cmd timeout) + 2 × 0.5s (backoff) = 約 4 秒。
constexpr int     RETRY_DELAY_MS     = 500;
constexpr int64_t SLOW_OPEN_WARN_US  = 100 * 1000;
constexpr int64_t SLOW_READ_WARN_US  = 200 * 1000;
}  // namespace

static bool read_text_file_once(const std::string& path, std::string& out, std::string& err)
{
    const int64_t t_open0 = esp_timer_get_time();
    FILE* f = fopen(path.c_str(), "r");
    const int64_t t_open_us = esp_timer_get_time() - t_open0;
    if (!f) {
        const int e = errno;
        err = fmt::format("fopen failed (errno={} {}): {}", e, std::strerror(e), path);
        return false;
    }
    if (t_open_us > SLOW_OPEN_WARN_US) {
        mclog::tagWarn(_tag, "  slow fopen ({} ms): {}", t_open_us / 1000, path);
    }
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024) {
        err = fmt::format("text file size invalid ({}): {}", sz, path);
        fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    const int64_t t_read0 = esp_timer_get_time();
    size_t got = fread(out.data(), 1, static_cast<size_t>(sz), f);
    const int64_t t_read_us = esp_timer_get_time() - t_read0;
    const int read_errno = (got != static_cast<size_t>(sz)) ? errno : 0;
    fclose(f);
    if (got != static_cast<size_t>(sz)) {
        err = fmt::format("fread short: got {}/{} (errno={} {}) from {}",
                          got, sz, read_errno, std::strerror(read_errno), path);
        return false;
    }
    if (t_read_us > SLOW_READ_WARN_US) {
        mclog::tagWarn(_tag, "  slow fread ({} ms, {} bytes): {}", t_read_us / 1000, sz, path);
    }
    return true;
}

static bool read_text_file(const std::string& path, std::string& out, std::string& err)
{
    for (int attempt = 1; attempt <= MAX_FILE_RETRIES; ++attempt) {
        std::string this_err;
        if (read_text_file_once(path, out, this_err)) {
            if (attempt > 1) {
                mclog::tagWarn(_tag, "read_text_file recovered on attempt {}: {}", attempt, path);
            }
            return true;
        }
        err = this_err;
        if (attempt < MAX_FILE_RETRIES) {
            mclog::tagWarn(_tag, "read_text_file attempt {} failed ({}); retry after {}ms",
                           attempt, this_err, RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }
    return false;
}

// Load entire PNG file into a PSRAM-backed buffer.
static bool read_png_file_once(const std::string& path, PngBuffer& out, std::string& err)
{
    const int64_t t_open0 = esp_timer_get_time();
    FILE* f = fopen(path.c_str(), "rb");
    const int64_t t_open_us = esp_timer_get_time() - t_open0;
    if (!f) {
        const int e = errno;
        err = fmt::format("fopen failed (errno={} {}): {}", e, std::strerror(e), path);
        return false;
    }
    if (t_open_us > SLOW_OPEN_WARN_US) {
        mclog::tagWarn(_tag, "  slow fopen ({} ms): {}", t_open_us / 1000, path);
    }
    fseek(f, 0, SEEK_END);
    const long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1024 * 1024) {
        err = fmt::format("png size invalid ({}): {}", sz, path);
        fclose(f);
        return false;
    }
    auto* raw = static_cast<uint8_t*>(heap_caps_malloc(static_cast<size_t>(sz),
                                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!raw) {
        err = fmt::format("PSRAM alloc failed ({} bytes): {}", sz, path);
        fclose(f);
        return false;
    }
    const int64_t t_read0 = esp_timer_get_time();
    size_t got = fread(raw, 1, static_cast<size_t>(sz), f);
    const int64_t t_read_us = esp_timer_get_time() - t_read0;
    const int read_errno = (got != static_cast<size_t>(sz)) ? errno : 0;
    fclose(f);
    if (got != static_cast<size_t>(sz)) {
        heap_caps_free(raw);
        err = fmt::format("fread short: got {}/{} (errno={} {}) from {}",
                          got, sz, read_errno, std::strerror(read_errno), path);
        return false;
    }
    if (t_read_us > SLOW_READ_WARN_US) {
        mclog::tagWarn(_tag, "  slow fread ({} ms, {} bytes): {}", t_read_us / 1000, sz, path);
    }
    out.bytes.reset(raw);
    out.size = static_cast<size_t>(sz);
    mclog::tagInfo(_tag, "  PNG: {} ({} bytes)", path, sz);
    return true;
}

static bool read_png_file(const std::string& path, PngBuffer& out, std::string& err)
{
    for (int attempt = 1; attempt <= MAX_FILE_RETRIES; ++attempt) {
        std::string this_err;
        if (read_png_file_once(path, out, this_err)) {
            if (attempt > 1) {
                mclog::tagWarn(_tag, "read_png_file recovered on attempt {}: {}", attempt, path);
            }
            return true;
        }
        err = this_err;
        // PSRAM 解放は read_png_file_once 内で完了済み (out.bytes は未代入)
        if (attempt < MAX_FILE_RETRIES) {
            mclog::tagWarn(_tag, "read_png_file attempt {} failed ({}); retry after {}ms",
                           attempt, this_err, RETRY_DELAY_MS);
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }
    }
    return false;
}

// ---- Enum parsers -------------------------------------------------------------
static const std::unordered_map<std::string, Emotion> _emotion_map = {
    {"Neutral", Emotion::Neutral},
    {"Happy",   Emotion::Happy},
    {"Angry",   Emotion::Angry},
    {"Sad",     Emotion::Sad},
    {"Doubt",   Emotion::Doubt},
    {"Sleepy",  Emotion::Sleepy},
};

static const std::unordered_map<std::string, EmotionDecoratorKind> _kind_map = {
    {"None",   EmotionDecoratorKind::None},
    {"Heart",  EmotionDecoratorKind::Heart},
    {"Angry",  EmotionDecoratorKind::Angry},
    {"Sweat",  EmotionDecoratorKind::Sweat},
    {"Shy",    EmotionDecoratorKind::Shy},
    {"Dizzy",  EmotionDecoratorKind::Dizzy},
    {"Sleepy", EmotionDecoratorKind::Sleepy},
    {"Doubt",  EmotionDecoratorKind::Doubt},
};

static bool parse_emotion(const std::string& s, Emotion& out)
{
    const auto it = _emotion_map.find(s);
    if (it == _emotion_map.end()) return false;
    out = it->second;
    return true;
}

static bool parse_decorator_kind(const std::string& s, EmotionDecoratorKind& out)
{
    const auto it = _kind_map.find(s);
    if (it == _kind_map.end()) return false;
    out = it->second;
    return true;
}

// ---- avatar.json parsing ------------------------------------------------------
bool load_avatar_index(AvatarIndex& out, std::string& err)
{
    mclog::tagInfo(_tag, "Reading /sdcard/avatar.json");
    std::string text;
    if (!read_text_file("/sdcard/avatar.json", text, err)) return false;

    ArduinoJson::JsonDocument doc;
    auto perr = ArduinoJson::deserializeJson(doc, text);
    if (perr) {
        err = fmt::format("avatar.json parse error: {}", perr.c_str());
        return false;
    }

    out.version = doc["version"] | 0;
    out.current = doc["current"].as<std::string>();
    if (out.current.empty()) {
        err = "avatar.json missing 'current'";
        return false;
    }

    auto skins_array = doc["skins"].as<ArduinoJson::JsonArrayConst>();
    if (skins_array.isNull() || skins_array.size() == 0) {
        err = "avatar.json missing or empty 'skins'";
        return false;
    }

    out.skins.clear();
    for (auto v : skins_array) {
        SkinIndexEntry e;
        e.id   = v["id"].as<std::string>();
        e.name = v["name"].as<std::string>();
        if (e.id.empty()) {
            err = "avatar.json skin entry missing 'id'";
            return false;
        }
        out.skins.push_back(std::move(e));
    }
    mclog::tagInfo(_tag, "avatar.json: {} skins, current='{}'", out.skins.size(), out.current);
    return true;
}

// ---- skin manifest parsing + PNG loading -------------------------------------
// `skin_dir` example: "/sdcard/ponko". Loads manifest.json + all referenced PNGs
// into PSRAM (filling out.{base,eye_left,eye_right,mouth} PngBuffers).
bool load_image_avatar_config(const std::string& skin_dir, ImageAvatarConfig& out, std::string& err)
{
    const std::string manifest_path = skin_dir + "/manifest.json";
    mclog::tagInfo(_tag, "Reading {}", manifest_path);
    std::string text;
    if (!read_text_file(manifest_path, text, err)) return false;

    ArduinoJson::JsonDocument doc;
    auto perr = ArduinoJson::deserializeJson(doc, text);
    if (perr) {
        err = fmt::format("{} parse error: {}", manifest_path, perr.c_str());
        return false;
    }

    auto load_png = [&](const std::string& filename, PngBuffer& dst) -> bool {
        if (filename.empty()) return true;  // optional asset, skip
        return read_png_file(skin_dir + "/" + filename, dst, err);
    };

    // base
    {
        auto base = doc["base"];
        std::string fn = base["image"].as<std::string>();
        if (fn.empty()) {
            err = fmt::format("{}: base.image is empty", manifest_path);
            return false;
        }
        if (!load_png(fn, out.base)) return false;
        out.base_x = base["x"] | 0;
        out.base_y = base["y"] | 0;
        out.base_w = base["w"] | 320;
        out.base_h = base["h"] | 240;
    }

    // eye_left
    {
        auto el = doc["eye_left"];
        if (!load_png(el["open"].as<std::string>(), out.eye_left.open)) return false;
        if (!load_png(el["closed"].as<std::string>(), out.eye_left.closed)) return false;
        out.eye_left.x      = el["x"] | 0;
        out.eye_left.y      = el["y"] | 0;
        out.eye_left.width  = el["w"] | 0;
        out.eye_left.height = el["h"] | 0;
    }

    // eye_right
    {
        auto er = doc["eye_right"];
        if (!load_png(er["open"].as<std::string>(), out.eye_right.open)) return false;
        if (!load_png(er["closed"].as<std::string>(), out.eye_right.closed)) return false;
        out.eye_right.x      = er["x"] | 0;
        out.eye_right.y      = er["y"] | 0;
        out.eye_right.width  = er["w"] | 0;
        out.eye_right.height = er["h"] | 0;
    }

    // mouth
    {
        auto m = doc["mouth"];
        if (!load_png(m["normal"].as<std::string>(), out.mouth.normal)) return false;
        if (!load_png(m["open"].as<std::string>(), out.mouth.open)) return false;
        out.mouth.normal_x = m["normal_x"] | 0;
        out.mouth.normal_y = m["normal_y"] | 0;
        out.mouth.normal_w = m["normal_w"] | 0;
        out.mouth.normal_h = m["normal_h"] | 0;
        out.mouth.open_x   = m["open_x"]   | 0;
        out.mouth.open_y   = m["open_y"]   | 0;
        out.mouth.open_w   = m["open_w"]   | 0;
        out.mouth.open_h   = m["open_h"]   | 0;
    }

    // emotion_decorators
    out.emotion_decorators.clear();
    auto deco_array = doc["emotion_decorators"].as<ArduinoJson::JsonArrayConst>();
    if (!deco_array.isNull()) {
        for (auto v : deco_array) {
            EmotionDecoratorMapping mapping;
            const std::string emo_str  = v["emotion"].as<std::string>();
            const std::string kind_str = v["kind"].as<std::string>();
            if (!parse_emotion(emo_str, mapping.emotion)) {
                err = fmt::format("{}: unknown emotion '{}'", manifest_path, emo_str);
                return false;
            }
            if (!parse_decorator_kind(kind_str, mapping.kind)) {
                err = fmt::format("{}: unknown decorator kind '{}'", manifest_path, kind_str);
                return false;
            }
            mapping.animation_interval_ms = v["anim_ms"] | 500u;
            const bool has_x = !v["x"].isNull();
            const bool has_y = !v["y"].isNull();
            if (has_x && has_y) {
                mapping.has_custom_position = true;
                mapping.x = v["x"] | 0;
                mapping.y = v["y"] | 0;
            }
            out.emotion_decorators.push_back(std::move(mapping));
        }
    }

    // head_pet (GOB fork): per-skin Heart / Shy positions for HeadPetModifier.
    // Optional. When absent, HeadPetModifier falls back to decorator built-in
    // defaults (DefaultAvatar layout).
    auto hp = doc["head_pet"];
    if (!hp.isNull()) {
        auto h = hp["heart"];
        if (!h.isNull()) {
            out.head_pet.has_heart     = true;
            out.head_pet.heart_x       = h["x"] | 0;
            out.head_pet.heart_y       = h["y"] | 0;
            out.head_pet.heart_anim_ms = h["anim_ms"] | 500u;
        }
        auto s = hp["shy"];
        if (!s.isNull()) {
            auto sl = s["left"];
            auto sr = s["right"];
            // Both left and right required to enable shy override (otherwise
            // we'd produce an asymmetric defaults+override hybrid).
            if (!sl.isNull() && !sr.isNull()) {
                out.head_pet.has_shy     = true;
                out.head_pet.shy_left_x  = sl["x"] | 0;
                out.head_pet.shy_left_y  = sl["y"] | 0;
                out.head_pet.shy_right_x = sr["x"] | 0;
                out.head_pet.shy_right_y = sr["y"] | 0;
            }
        }
    }

    // dizzy (GOB fork): per-skin Dizzy color / animation / scale used by
    // IMUModifier. All fields optional; position is auto-derived from
    // eye_left/eye_right rect via Avatar::getEyeCenterOffset(). Color accepts
    // "#RRGGBB" or "0xRRGGBB"; missing → keep DizzyDecorator default.
    auto dz = doc["dizzy"];
    if (!dz.isNull()) {
        auto color_node = dz["color"];
        if (!color_node.isNull()) {
            const char* cstr = color_node | static_cast<const char*>(nullptr);
            if (cstr) {
                std::string s(cstr);
                if (!s.empty() && s[0] == '#') s.erase(0, 1);
                if (s.size() >= 2 && (s[0] == '0') && (s[1] == 'x' || s[1] == 'X')) s.erase(0, 2);
                if (!s.empty()) {
                    try {
                        out.dizzy.color_hex = static_cast<uint32_t>(std::stoul(s, nullptr, 16));
                        out.dizzy.has_color = true;
                    } catch (...) {
                        // ignore malformed color, keep default
                    }
                }
            }
        }
        out.dizzy.anim_ms = dz["anim_ms"] | 300u;
        auto scale_node = dz["scale"];
        if (!scale_node.isNull()) {
            float sc = scale_node | 1.0f;
            if (sc > 0.0f) {
                out.dizzy.scale     = sc;
                out.dizzy.has_scale = true;
            }
        }
        // Optional per-eye position override (LV_ALIGN_CENTER offset). Both
        // left and right required to enable; otherwise auto-derive from eye
        // centers via Avatar::getEyeCenterOffset.
        auto pos = dz["position"];
        if (!pos.isNull()) {
            auto pl = pos["left"];
            auto pr = pos["right"];
            if (!pl.isNull() && !pr.isNull()) {
                out.dizzy.has_position = true;
                out.dizzy.left_x       = pl["x"] | 0;
                out.dizzy.left_y       = pl["y"] | 0;
                out.dizzy.right_x      = pr["x"] | 0;
                out.dizzy.right_y      = pr["y"] | 0;
            }
        }
    }

    if (!out.base.valid()) {
        err = fmt::format("{}: base PNG not loaded", manifest_path);
        return false;
    }
    return true;
}

// ---- Top-level loader ---------------------------------------------------------
static SkinLoadResult make_default_fallback(lv_obj_t* parent, const std::string& reason)
{
    SkinLoadResult r;
    auto def = std::make_unique<DefaultAvatar>();
    def->init(parent);
    r.avatar         = std::move(def);
    r.used_fallback  = true;
    r.loaded_skin_id = "default (fallback)";
    r.error_message  = reason;  // shown as top toast
    return r;
}

SkinLoadResult load_avatar_or_fallback(lv_obj_t* parent)
{
    const int64_t t_start = esp_timer_get_time();

    // Forced DefaultAvatar (set via NVS = "__default__"). Skip SD entirely.
    {
        std::string nvs_id = stackchan::gob_fork::get_skin_current();
        if (nvs_id == stackchan::gob_fork::FORCE_DEFAULT_AVATAR_ID) {
            mclog::tagInfo(_tag, "NVS forces DefaultAvatar (id='{}')", nvs_id);
            SkinLoadResult r = make_default_fallback(parent, "");
            r.error_message  = "";  // not an error path
            r.loaded_skin_id = stackchan::gob_fork::FORCE_DEFAULT_AVATAR_ID;
            return r;
        }
    }

    // 1. Card present?
    if (!hal::SdGuard::isInserted()) {
        mclog::tagError(_tag, "SD card not inserted");
        return make_default_fallback(parent, "SD card not inserted");
    }

    // 2. Acquire SD bus (LvglLockGuard + GPIO35 swap inside ctor)
    AvatarIndex index;
    ImageAvatarConfig cfg;
    std::string err;
    std::string skin_id;
    {
        hal::SdGuard guard;
        hal::SdGuard::MountGuard mg;
        if (!mg.ok()) {
            mclog::tagError(_tag, "SD mount failed");
            return make_default_fallback(parent, "SD mount failed");
        }

        // 3. avatar.json
        if (!load_avatar_index(index, err)) {
            mclog::tagError(_tag, "avatar.json: {}", err);
            return make_default_fallback(parent, "Skin index load error");
        }

        // 4. NVS override or avatar.json's "current"
        skin_id = stackchan::gob_fork::get_skin_current();
        if (skin_id.empty()) {
            skin_id = index.current;
        } else {
            mclog::tagInfo(_tag, "NVS current_skin: '{}' (overrides avatar.json's '{}')",
                           skin_id, index.current);
        }

        // 5. validate skin_id is in index
        bool found = false;
        for (auto& e : index.skins) {
            if (e.id == skin_id) { found = true; break; }
        }
        if (!found) {
            mclog::tagError(_tag, "skin '{}' not in index, fallback to skins[0]", skin_id);
            skin_id = index.skins[0].id;
        }

        // 6. /sdcard/<skin_id>/manifest.json + PNGs
        const std::string skin_dir = "/sdcard/" + skin_id;
        mclog::tagInfo(_tag, "Loading skin '{}' from {}", skin_id, skin_dir);
        if (!load_image_avatar_config(skin_dir, cfg, err)) {
            mclog::tagError(_tag, "skin '{}' load failed: {}", skin_id, err);
            return make_default_fallback(parent, "Skin manifest/PNG load error");
        }
    }
    // SdGuard dtor: GPIO35 → LCD_DC, LVGL lock released

    // Compute total PSRAM-resident PNG bytes for diagnostics.
    const size_t total_bytes = cfg.base.size +
                         cfg.eye_left.open.size + cfg.eye_left.closed.size +
                         cfg.eye_right.open.size + cfg.eye_right.closed.size +
                         cfg.mouth.normal.size + cfg.mouth.open.size;

    // 7. Build ImageAvatar (LVGL operations outside SdGuard scope, but PngBuffers
    //    in cfg are now PSRAM-resident and stable). LVGL will decode PNG bytes
    //    on first render; subsequent frames hit LVGL image cache.
    auto img = std::make_unique<ImageAvatar>(std::move(cfg));
    img->init(parent);
    SkinLoadResult result;
    result.avatar         = std::move(img);
    result.loaded_skin_id = skin_id;
    const int64_t elapsed_ms = (esp_timer_get_time() - t_start) / 1000;
    mclog::tagInfo(_tag, "Loaded skin '{}' ({} bytes total, {} ms)",
                   skin_id, total_bytes, static_cast<int>(elapsed_ms));
    return result;
}

}  // namespace stackchan::avatar::image
