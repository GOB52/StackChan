// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include "../../avatar/avatar.h"
#include "image_avatar.h"
#include <lvgl.h>
#include <memory>
#include <string>
#include <vector>

namespace stackchan::avatar::image {

// Index file (avatar.json) entry — Launcher-menu friendly inline metadata.
struct SkinIndexEntry {
    std::string id;
    std::string name;
    std::string manifest;   // Filename of the per-skin manifest JSON.
    std::string thumbnail;  // Optional thumbnail asset name.
};

struct AvatarIndex {
    int version = 0;
    std::string current;  // ID of the currently selected skin.
    std::vector<SkinIndexEntry> skins;
};

struct SkinLoadResult {
    std::unique_ptr<Avatar> avatar;  // Already init()-ed; ready to attachAvatar().
    std::string error_message;       // Empty on success.
    bool used_fallback = false;      // True when DefaultAvatar fallback was used.
    std::string loaded_skin_id;      // ID of the skin actually loaded (e.g., "ponko" or "default").
};

// Top-level loader:
//   1. Read avatar.json from assets and parse as AvatarIndex.
//   2. Find current skin entry, load its manifest JSON, parse as ImageAvatarConfig.
//   3. On any failure, fall back to upstream DefaultAvatar (geometric).
// Returned avatar is already initialized with `parent` (caller just calls attachAvatar()).
SkinLoadResult load_avatar_or_fallback(lv_obj_t* parent);

// Lower-level helpers (exposed for testing / future SD-card path).
bool load_avatar_index(AvatarIndex& out, std::string& err);
bool load_image_avatar_config(const std::string& manifest_filename, ImageAvatarConfig& out, std::string& err);

// NVS-backed current skin selection (Phase 1.5b).
// get_current_skin_id_nvs(): returns persisted skin id, or empty string if not set / NVS error.
// set_current_skin_id_nvs(): persist selection. Returns true on success.
std::string get_current_skin_id_nvs();
bool        set_current_skin_id_nvs(const std::string& id);

}  // namespace stackchan::avatar::image
