// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "skin_loader.h"
#include "../default/default.h"  // DefaultAvatar fallback
#include "../../avatar/elements/emotion.h"
#include <assets.h>  // xiaozhi-esp32 main/assets.h (Assets singleton, GetAssetData)
#include <ArduinoJson.hpp>
#include <mooncake_log.h>
#include <fmt/format.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <unordered_map>

namespace stackchan::avatar::image {

static const char* _tag = "SkinLoader";

// ---- Asset I/O ----------------------------------------------------------------
static bool read_text_asset(const std::string& name, std::string& out, std::string& err)
{
    void* data_ptr   = nullptr;
    size_t data_size = 0;
    if (!Assets::GetInstance().GetAssetData(name, data_ptr, data_size)) {
        err = fmt::format("asset not found: {}", name);
        return false;
    }
    if (!data_ptr || data_size == 0) {
        err = fmt::format("asset empty: {}", name);
        return false;
    }
    out.assign(static_cast<const char*>(data_ptr), data_size);
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
    std::string text;
    if (!read_text_asset("avatar.json", text, err)) return false;

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
        e.id        = v["id"].as<std::string>();
        e.name      = v["name"].as<std::string>();
        e.manifest  = v["manifest"].as<std::string>();
        e.thumbnail = v["thumbnail"].as<std::string>();
        if (e.id.empty() || e.manifest.empty()) {
            err = "avatar.json skin entry missing 'id' or 'manifest'";
            return false;
        }
        out.skins.push_back(std::move(e));
    }
    return true;
}

// ---- skin manifest (e.g. ponko.json) parsing ----------------------------------
bool load_image_avatar_config(const std::string& manifest_filename, ImageAvatarConfig& out, std::string& err)
{
    std::string text;
    if (!read_text_asset(manifest_filename, text, err)) return false;

    ArduinoJson::JsonDocument doc;
    auto perr = ArduinoJson::deserializeJson(doc, text);
    if (perr) {
        err = fmt::format("{} parse error: {}", manifest_filename, perr.c_str());
        return false;
    }

    // base
    auto base = doc["base"];
    out.base_image_name = base["image"].as<std::string>();
    out.base_x = base["x"] | 0;
    out.base_y = base["y"] | 0;
    out.base_w = base["w"] | 320;
    out.base_h = base["h"] | 240;

    // eye_left
    auto el = doc["eye_left"];
    out.eye_left.open_image_name   = el["open"].as<std::string>();
    out.eye_left.closed_image_name = el["closed"].as<std::string>();
    out.eye_left.x      = el["x"] | 0;
    out.eye_left.y      = el["y"] | 0;
    out.eye_left.width  = el["w"] | 0;
    out.eye_left.height = el["h"] | 0;

    // eye_right
    auto er = doc["eye_right"];
    out.eye_right.open_image_name   = er["open"].as<std::string>();
    out.eye_right.closed_image_name = er["closed"].as<std::string>();
    out.eye_right.x      = er["x"] | 0;
    out.eye_right.y      = er["y"] | 0;
    out.eye_right.width  = er["w"] | 0;
    out.eye_right.height = er["h"] | 0;

    // mouth
    auto m = doc["mouth"];
    out.mouth.normal_image_name = m["normal"].as<std::string>();
    out.mouth.open_image_name   = m["open"].as<std::string>();
    out.mouth.normal_x = m["normal_x"] | 0;
    out.mouth.normal_y = m["normal_y"] | 0;
    out.mouth.normal_w = m["normal_w"] | 0;
    out.mouth.normal_h = m["normal_h"] | 0;
    out.mouth.open_x   = m["open_x"]   | 0;
    out.mouth.open_y   = m["open_y"]   | 0;
    out.mouth.open_w   = m["open_w"]   | 0;
    out.mouth.open_h   = m["open_h"]   | 0;

    // emotion_decorators
    out.emotion_decorators.clear();
    auto deco_array = doc["emotion_decorators"].as<ArduinoJson::JsonArrayConst>();
    if (!deco_array.isNull()) {
        for (auto v : deco_array) {
            EmotionDecoratorMapping mapping;
            std::string emo_str  = v["emotion"].as<std::string>();
            std::string kind_str = v["kind"].as<std::string>();
            if (!parse_emotion(emo_str, mapping.emotion)) {
                err = fmt::format("{}: unknown emotion '{}'", manifest_filename, emo_str);
                return false;
            }
            if (!parse_decorator_kind(kind_str, mapping.kind)) {
                err = fmt::format("{}: unknown decorator kind '{}'", manifest_filename, kind_str);
                return false;
            }
            mapping.animation_interval_ms = v["anim_ms"] | 500u;

            // Position is optional; if present, both x/y must be specified together.
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

    // Minimal sanity check.
    if (out.base_image_name.empty()) {
        err = fmt::format("{}: base.image is empty", manifest_filename);
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
    if (!out.empty() && out.back() == '\0') out.pop_back();  // drop trailing NUL
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
static SkinLoadResult make_default_fallback(lv_obj_t* parent)
{
    SkinLoadResult r;
    auto def = std::make_unique<DefaultAvatar>();
    def->init(parent);
    r.avatar          = std::move(def);
    r.used_fallback   = true;
    r.loaded_skin_id  = "default (fallback)";
    return r;
}

SkinLoadResult load_avatar_or_fallback(lv_obj_t* parent)
{
    AvatarIndex index;
    std::string err;
    if (!load_avatar_index(index, err)) {
        mclog::tagError(_tag, "Skin index load failed: {} (using default)", err);
        auto r = make_default_fallback(parent);
        r.error_message = "Skin load error";  // short for top toast; details in log
        return r;
    }

    // Determine current skin id: NVS override > avatar.json's "current".
    std::string desired = get_current_skin_id_nvs();
    if (desired.empty()) {
        desired = index.current;
    } else {
        mclog::tagInfo(_tag, "NVS current_skin: '{}' (overrides avatar.json's '{}')",
                       desired, index.current);
    }

    // Locate selected skin entry.
    const SkinIndexEntry* selected = nullptr;
    for (auto& e : index.skins) {
        if (e.id == desired) {
            selected = &e;
            break;
        }
    }
    if (!selected) {
        mclog::tagError(_tag, "skin '{}' not in index, fallback to skins[0]", desired);
        selected = &index.skins[0];
    }

    ImageAvatarConfig cfg;
    if (!load_image_avatar_config(selected->manifest, cfg, err)) {
        mclog::tagError(_tag, "Skin manifest load failed: {} (using default)", err);
        auto r = make_default_fallback(parent);
        r.error_message = "Skin load error";  // short for top toast; details in log
        return r;
    }

    auto img = std::make_unique<ImageAvatar>(std::move(cfg));
    img->init(parent);
    SkinLoadResult result;
    result.avatar         = std::move(img);
    result.loaded_skin_id = selected->id;
    mclog::tagInfo(_tag, "Loaded skin '{}' from {}", selected->id, selected->manifest);
    return result;
}

}  // namespace stackchan::avatar::image
