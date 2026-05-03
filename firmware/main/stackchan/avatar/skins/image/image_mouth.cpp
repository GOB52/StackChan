// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "image_avatar.h"

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar::image;

static const int OPEN_THRESHOLD = 30;  // weight >= threshold -> open_wide, < -> smile

namespace {
void build_dsc_from_png(lv_image_dsc_t& dsc, const PngBuffer& src)
{
    if (!src.valid()) return;
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf    = LV_COLOR_FORMAT_RAW_ALPHA;
    dsc.data_size    = src.size;
    dsc.data         = src.bytes.get();
}
}  // namespace

ImageMouth::ImageMouth(lv_obj_t* parent, const MouthAsset& asset)
    : _normal_x(asset.normal_x), _normal_y(asset.normal_y),
      _normal_w(asset.normal_w), _normal_h(asset.normal_h),
      _open_x(asset.open_x), _open_y(asset.open_y),
      _open_w(asset.open_w), _open_h(asset.open_h)
{
    build_dsc_from_png(_dsc_normal, asset.normal);
    build_dsc_from_png(_dsc_open, asset.open);

    _img = std::make_unique<Image>(parent);
    _img->setAlign(LV_ALIGN_TOP_LEFT);
    _img->setPos(_normal_x, _normal_y);
    if (_normal_w > 0 && _normal_h > 0) {
        _img->setSize(_normal_w, _normal_h);
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
        _img->setPos(_open_x, _open_y);
        if (_open_w > 0 && _open_h > 0) {
            _img->setSize(_open_w, _open_h);
        }
    } else {
        _img->setSrc(&_dsc_normal);
        _img->setPos(_normal_x, _normal_y);
        if (_normal_w > 0 && _normal_h > 0) {
            _img->setSize(_normal_w, _normal_h);
        }
    }
}

void ImageMouth::setVisible(bool visible)
{
    Element::setVisible(visible);
    _img->setHidden(!visible);
}
