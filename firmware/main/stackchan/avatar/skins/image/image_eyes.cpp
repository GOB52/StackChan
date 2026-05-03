// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "image_avatar.h"
#include <assets/assets.h>
#include <utility>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar::image;

static const int OPEN_THRESHOLD = 50;  // weight >= threshold -> open, < -> closed

ImageEyes::ImageEyes(lv_obj_t* parent, EyeAsset asset) : _asset(std::move(asset))
{
    if (!_asset.open_image_name.empty()) {
        _dsc_open = assets::get_image(_asset.open_image_name);
    }
    if (!_asset.closed_image_name.empty()) {
        _dsc_closed = assets::get_image(_asset.closed_image_name);
    }

    _img = std::make_unique<Image>(parent);
    _img->setAlign(LV_ALIGN_TOP_LEFT);
    _img->setPos(_asset.x, _asset.y);
    if (_asset.width > 0 && _asset.height > 0) {
        _img->setSize(_asset.width, _asset.height);
    }
    _img->setSrc(&_dsc_open);
    _is_open = true;

    _weight = 100;
}

ImageEyes::~ImageEyes() = default;

void ImageEyes::setWeight(int weight)
{
    Feature::setWeight(weight);

    bool want_open = (_weight >= OPEN_THRESHOLD);
    if (want_open == _is_open) {
        return;
    }
    _is_open = want_open;
    _img->setSrc(want_open ? &_dsc_open : &_dsc_closed);
}

void ImageEyes::setVisible(bool visible)
{
    Element::setVisible(visible);
    _img->setHidden(!visible);
}

void ImageEyes::setPosition(const Vector2i& position)
{
    Element::setPosition(position);

    // Microscopic gaze offset (-100..100 -> -4..4 px) so eyebrow/forehead don't shift
    int dx = (_position.x * 4) / 100;
    int dy = (_position.y * 4) / 100;
    _img->setPos(_asset.x + dx, _asset.y + dy);
}
