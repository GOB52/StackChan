// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "skin_loader.h"
#include "../default/default.h"  // DefaultAvatar fallback
#include "../../avatar/elements/emotion.h"
#include <hal/board/sd_guard.h>
#include <ArduinoJson.hpp>
#include <mooncake_log.h>
#include <fmt/format.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace stackchan::avatar::image {

static const char* _tag = "SkinLoader";

// ---- File I/O (SD only) -------------------------------------------------------
static bool read_text_file(const std::string& path, std::string& out, std::string& err)
{
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        err = fmt::format("fopen failed: {}", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024) {
        err = fmt::format("text file size invalid ({}): {}", sz, path);
        fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t got = fread(out.data(), 1, static_cast<size_t>(sz), f);
    fclose(f);
    if (got != static_cast<size_t>(sz)) {
        err = fmt::format("fread short: got {}/{} from {}", got, sz, path);
        return false;
    }
    return true;
}

// Load entire PNG file into a PSRAM-backed buffer.
static bool read_png_file(const std::string& path, PngBuffer& out, std::string& err)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        err = fmt::format("fopen failed: {}", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
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
    size_t got = fread(raw, 1, static_cast<size_t>(sz), f);
    fclose(f);
    if (got != static_cast<size_t>(sz)) {
        heap_caps_free(raw);
        err = fmt::format("fread short: got {}/{} from {}", got, sz, path);
        return false;
    }
    out.bytes.reset(raw);
    out.size = static_cast<size_t>(sz);
    mclog::tagInfo(_tag, "  PNG: {} ({} bytes)", path, sz);
    return true;
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
    auto it = _emotion_map.find(s);
    if (it == _emotion_map.end()) return false;
    out = it->second;
    return true;
}

static bool parse_decorator_kind(const std::string& s, EmotionDecoratorKind& out)
{
    auto it = _kind_map.find(s);
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
    std::string manifest_path = skin_dir + "/manifest.json";
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
            std::string emo_str  = v["emotion"].as<std::string>();
            std::string kind_str = v["kind"].as<std::string>();
            if (!parse_emotion(emo_str, mapping.emotion)) {
                err = fmt::format("{}: unknown emotion '{}'", manifest_path, emo_str);
                return false;
            }
            if (!parse_decorator_kind(kind_str, mapping.kind)) {
                err = fmt::format("{}: unknown decorator kind '{}'", manifest_path, kind_str);
                return false;
            }
            mapping.animation_interval_ms = v["anim_ms"] | 500u;
            bool has_x = !v["x"].isNull();
            bool has_y = !v["y"].isNull();
            if (has_x && has_y) {
                mapping.has_custom_position = true;
                mapping.x = v["x"] | 0;
                mapping.y = v["y"] | 0;
            }
            out.emotion_decorators.push_back(std::move(mapping));
        }
    }

    if (!out.base.valid()) {
        err = fmt::format("{}: base PNG not loaded", manifest_path);
        return false;
    }
    return true;
}

// ---- NVS-backed current skin selection ---------------------------------------
static const char* _nvs_namespace = "skin";
static const char* _nvs_key       = "current";

std::string get_current_skin_id_nvs()
{
    nvs_handle_t h = 0;
    if (nvs_open(_nvs_namespace, NVS_READONLY, &h) != ESP_OK) {
        return "";
    }
    size_t len = 0;
    if (nvs_get_str(h, _nvs_key, nullptr, &len) != ESP_OK || len == 0 || len > 64) {
        nvs_close(h);
        return "";
    }
    std::string out(len, '\0');
    if (nvs_get_str(h, _nvs_key, out.data(), &len) != ESP_OK) {
        nvs_close(h);
        return "";
    }
    nvs_close(h);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

bool set_current_skin_id_nvs(const std::string& id)
{
    nvs_handle_t h = 0;
    if (nvs_open(_nvs_namespace, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = (nvs_set_str(h, _nvs_key, id.c_str()) == ESP_OK) && (nvs_commit(h) == ESP_OK);
    nvs_close(h);
    return ok;
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
    int64_t t_start = esp_timer_get_time();

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
        if (!guard.ensureMounted()) {
            mclog::tagError(_tag, "SD mount failed");
            return make_default_fallback(parent, "SD mount failed");
        }

        // 3. avatar.json
        if (!load_avatar_index(index, err)) {
            mclog::tagError(_tag, "avatar.json: {}", err);
            return make_default_fallback(parent, "Skin index load error");
        }

        // 4. NVS override or avatar.json's "current"
        skin_id = get_current_skin_id_nvs();
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
        std::string skin_dir = "/sdcard/" + skin_id;
        mclog::tagInfo(_tag, "Loading skin '{}' from {}", skin_id, skin_dir);
        if (!load_image_avatar_config(skin_dir, cfg, err)) {
            mclog::tagError(_tag, "skin '{}' load failed: {}", skin_id, err);
            return make_default_fallback(parent, "Skin manifest/PNG load error");
        }
    }
    // SdGuard dtor: GPIO35 → LCD_DC, LVGL lock released

    // Compute total PSRAM-resident PNG bytes for diagnostics.
    size_t total_bytes = cfg.base.size +
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
    int64_t elapsed_ms = (esp_timer_get_time() - t_start) / 1000;
    mclog::tagInfo(_tag, "Loaded skin '{}' ({} bytes total, {} ms)",
                   skin_id, total_bytes, static_cast<int>(elapsed_ms));
    return result;
}

}  // namespace stackchan::avatar::image
