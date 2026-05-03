// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "image_avatar.h"
#include <assets/assets.h>
#include <utility>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar::image;

static const int OPEN_THRESHOLD = 30;  // weight >= threshold -> open_wide, < -> smile

ImageMouth::ImageMouth(lv_obj_t* parent, MouthAsset asset) : _asset(std::move(asset))
{
    if (!_asset.normal_image_name.empty()) {
        _dsc_normal = assets::get_image(_asset.normal_image_name);
    }
    if (!_asset.open_image_name.empty()) {
        _dsc_open = assets::get_image(_asset.open_image_name);
    }

    _img = std::make_unique<Image>(parent);
    _img->setAlign(LV_ALIGN_TOP_LEFT);
    _img->setPos(_asset.normal_x, _asset.normal_y);
    if (_asset.normal_w > 0 && _asset.normal_h > 0) {
        _img->setSize(_asset.normal_w, _asset.normal_h);
    }
    _img->setSrc(&_dsc_normal);
    _is_open = false;

    _weight = 0;
}

ImageMouth::~ImageMouth() = default;

void ImageMouth::setWeight(int weight)
{
    Feature::setWeight(weight);

    bool want_open = (_weight >= OPEN_THRESHOLD);
    if (want_open == _is_open) {
        return;
    }
    _is_open = want_open;

    if (want_open) {
        _img->setSrc(&_dsc_open);
        _img->setPos(_asset.open_x, _asset.open_y);
        if (_asset.open_w > 0 && _asset.open_h > 0) {
            _img->setSize(_asset.open_w, _asset.open_h);
        }
    } else {
        _img->setSrc(&_dsc_normal);
        _img->setPos(_asset.normal_x, _asset.normal_y);
        if (_asset.normal_w > 0 && _asset.normal_h > 0) {
            _img->setSize(_asset.normal_w, _asset.normal_h);
        }
    }
}

void ImageMouth::setVisible(bool visible)
{
    Element::setVisible(visible);
    _img->setHidden(!visible);
}
