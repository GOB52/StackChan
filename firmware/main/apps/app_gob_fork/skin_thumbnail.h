// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <stackchan/avatar/skins/image/image_assets.h>
#include <lvgl.h>
#include <string>

namespace stackchan::avatar::image {

// Composite base + eye_l_open + eye_r_open + mouth_normal into a scaled-down
// LVGL draw buffer (RGB565) using lv_snapshot_take.
//
// scale_div    : 4 = 1/4 (320x240 -> 80x60). Must be >= 1.
// parent_layer : any lv_obj on the active screen used as the temporary render host
//                (the host obj is hidden during render and deleted before return).
// err          : filled on failure.
//
// On success returns a draw_buf that the caller MUST release via lv_draw_buf_destroy()
// when no longer used. Detach from any lv_image first (lv_image_set_src(img, NULL)).
lv_draw_buf_t* make_skin_thumbnail(const ImageAvatarConfig& cfg,
                                   int scale_div,
                                   lv_obj_t* parent_layer,
                                   std::string& err);

}  // namespace stackchan::avatar::image
