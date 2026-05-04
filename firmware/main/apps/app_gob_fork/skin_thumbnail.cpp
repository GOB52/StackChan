// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "skin_thumbnail.h"
#include <misc/cache/lv_cache.h>  // lv_image_cache_drop (not exposed by lvgl.h)
#include <mooncake_log.h>

namespace stackchan::avatar::image {

namespace {
const char* _tag = "SkinThumb";

void build_dsc_from_png(lv_image_dsc_t& dsc, const PngBuffer& src)
{
    if (!src.valid()) return;
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf    = LV_COLOR_FORMAT_RAW_ALPHA;
    dsc.data_size    = src.size;
    dsc.data         = src.bytes.get();
}
}  // namespace

lv_draw_buf_t* make_skin_thumbnail(const ImageAvatarConfig& cfg,
                                   int scale_div,
                                   lv_obj_t* parent_layer,
                                   std::string& err)
{
    if (scale_div < 1) { err = "invalid scale_div"; return nullptr; }
    if (!cfg.base.valid()) { err = "base PNG not loaded"; return nullptr; }
    if (!parent_layer) { err = "parent_layer null"; return nullptr; }

    const int     tw    = 320 / scale_div;
    const int     th    = 240 / scale_div;
    const int32_t scale = LV_SCALE_NONE / scale_div;

    lv_obj_t* host = lv_obj_create(parent_layer);
    lv_obj_set_size(host, tw, th);
    lv_obj_set_style_radius(host, 0, 0);
    lv_obj_set_style_border_width(host, 0, 0);
    lv_obj_set_style_pad_all(host, 0, 0);
    lv_obj_set_style_bg_color(host, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(host, LV_OPA_COVER, 0);
    lv_obj_remove_flag(host, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(host, LV_OBJ_FLAG_HIDDEN);

    lv_image_dsc_t base_dsc{}, eye_l_dsc{}, eye_r_dsc{}, mouth_dsc{};

    auto add_layer = [&](lv_image_dsc_t& dsc, const PngBuffer& png, int x, int y) {
        if (!png.valid()) return;
        build_dsc_from_png(dsc, png);
        lv_obj_t* img = lv_image_create(host);
        lv_image_set_src(img, &dsc);
        lv_image_set_pivot(img, 0, 0);
        lv_image_set_scale(img, scale);
        lv_obj_set_pos(img, x / scale_div, y / scale_div);
    };

    add_layer(base_dsc,  cfg.base,             cfg.base_x,         cfg.base_y);
    add_layer(eye_l_dsc, cfg.eye_left.open,    cfg.eye_left.x,     cfg.eye_left.y);
    add_layer(eye_r_dsc, cfg.eye_right.open,   cfg.eye_right.x,    cfg.eye_right.y);
    add_layer(mouth_dsc, cfg.mouth.normal,     cfg.mouth.normal_x, cfg.mouth.normal_y);

    lv_obj_update_layout(host);

    lv_draw_buf_t* draw_buf = lv_snapshot_take(host, LV_COLOR_FORMAT_RGB565);

    lv_obj_delete(host);
    lv_image_cache_drop(&base_dsc);
    lv_image_cache_drop(&eye_l_dsc);
    lv_image_cache_drop(&eye_r_dsc);
    lv_image_cache_drop(&mouth_dsc);

    if (!draw_buf) { err = "lv_snapshot_take failed"; return nullptr; }

    mclog::tagInfo(_tag, "thumb {}x{} ({} bytes)", tw, th,
                   static_cast<unsigned>(draw_buf->data_size));
    return draw_buf;
}

}  // namespace stackchan::avatar::image
