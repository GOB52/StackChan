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
// Skin folder = `<id>`, manifest path = `/sdcard/<id>/manifest.json`
// (manifest/thumbnail filenames implicit; no separate fields needed).
struct SkinIndexEntry {
    std::string id;
    std::string name;
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

// Top-level loader (SD-only):
//   1. Read /sdcard/avatar.json and parse as AvatarIndex.
//   2. Determine current skin id: NVS override > avatar.json's "current".
//   3. Load /sdcard/<id>/manifest.json + each PNG into PSRAM-backed PngBuffer.
//   4. On any failure (no card / mount / JSON / PNG missing), fall back to
//      upstream DefaultAvatar (geometric) and populate error_message for toast.
// Returned avatar is already initialized with `parent` (caller just calls attachAvatar()).
SkinLoadResult load_avatar_or_fallback(lv_obj_t* parent);

// Lower-level helpers.
bool load_avatar_index(AvatarIndex& out, std::string& err);
// `skin_dir` example: "/sdcard/ponko". Reads manifest.json + all referenced PNGs
// into PSRAM (out PngBuffers). Caller owns out (move into ImageAvatar).
bool load_image_avatar_config(const std::string& skin_dir, ImageAvatarConfig& out, std::string& err);

// NVS access for current skin selection lives in stackchan/gob_fork_nvs.h
// (namespace stackchan::gob_fork). This loader consults it; callers wishing
// to read/write the selection should include that header directly.

}  // namespace stackchan::avatar::image
