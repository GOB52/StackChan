// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "image_avatar.h"

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar::image;

static const int OPEN_THRESHOLD = 50;  // weight >= threshold -> open, < -> closed

namespace {
// Build an LVGL image descriptor from PSRAM-resident PNG bytes.
// Caller (this class) owns dsc; data pointer borrows from PngBuffer (asset).
void build_dsc_from_png(lv_image_dsc_t& dsc, const PngBuffer& src)
{
    if (!src.valid()) return;
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf    = LV_COLOR_FORMAT_RAW_ALPHA;
    dsc.data_size    = src.size;
    dsc.data         = src.bytes.get();
}
}  // namespace

ImageEyes::ImageEyes(lv_obj_t* parent, const EyeAsset& asset)
    : _x(asset.x), _y(asset.y), _width(asset.width), _height(asset.height)
{
    build_dsc_from_png(_dsc_open, asset.open);
    build_dsc_from_png(_dsc_closed, asset.closed);

    _img = std::make_unique<Image>(parent);
    _img->setAlign(LV_ALIGN_TOP_LEFT);
    _img->setPos(_x, _y);
    if (_width > 0 && _height > 0) {
        _img->setSize(_width, _height);
    }
    _img->setSrc(&_dsc_open);
    _is_open = true;

    _weight = 100;
}

ImageEyes::~ImageEyes() = default;

void ImageEyes::setWeight(const int weight)
{
    Feature::setWeight(weight);

    const bool want_open = (_weight >= OPEN_THRESHOLD);
    if (want_open == _is_open) {
        return;
    }
    _is_open = want_open;
    _img->setSrc(want_open ? &_dsc_open : &_dsc_closed);
}

void ImageEyes::setVisible(const bool visible)
{
    Element::setVisible(visible);
    _img->setHidden(!visible);
}

void ImageEyes::setPosition(const Vector2i& position)
{
    Element::setPosition(position);

    // Microscopic gaze offset (-100..100 -> -4..4 px) so eyebrow/forehead don't shift
    const int dx = (_position.x * 4) / 100;
    const int dy = (_position.y * 4) / 100;
    _img->setPos(_x + dx, _y + dy);
}
