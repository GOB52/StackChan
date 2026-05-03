// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "decorators.h"
#include <hal/hal.h>
#include <vector>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;

static const Vector2i _doubt_default_position        = Vector2i(108, -70);
static const std::vector<int> _doubt_rotation_frames = {150, 200};  // Heart 同様の揺れ (15.0° / 20.0°)

LV_IMAGE_DECLARE(decorator_question);

DoubtDecorator::DoubtDecorator(lv_obj_t* parent, uint32_t destroyAfterMs, uint32_t animationIntervalMs)
    : _animation_interval_ms(animationIntervalMs)
{
    _question = std::make_unique<Image>(parent);
    _question->setSrc(&decorator_question);
    _question->setAlign(LV_ALIGN_CENTER);
    _question->setPos(_doubt_default_position.x, _doubt_default_position.y);

    _question->setTransformPivot(_question->getWidth() / 2, _question->getHeight() / 2);
    _question->setRotation(_doubt_rotation_frames[0]);

    uint32_t now = GetHAL().millis();
    if (destroyAfterMs > 0) {
        _destroy_at   = now + destroyAfterMs;
        _has_lifetime = true;
    }
    if (_animation_interval_ms > 0) {
        _next_animation_tick = now + _animation_interval_ms;
    }
}

DoubtDecorator::~DoubtDecorator() = default;

void DoubtDecorator::_update()
{
    uint32_t now = GetHAL().millis();

    if (_has_lifetime && now >= _destroy_at) {
        requestDestroy();
        return;
    }

    if (_animation_interval_ms > 0 && now >= _next_animation_tick) {
        _next_animation_tick = now + _animation_interval_ms;
        _animation_index     = (_animation_index + 1) % _doubt_rotation_frames.size();
        _question->setRotation(_doubt_rotation_frames[_animation_index]);
    }
}

void DoubtDecorator::setPosition(int x, int y)
{
    if (_question) {
        _question->setPos(x, y);
    }
}

void DoubtDecorator::setRotation(int rotation)
{
    if (_question) {
        _question->setRotation(rotation);
    }
}

void DoubtDecorator::setColor(lv_color_t color)
{
    if (_question) {
        _question->setImageRecolorOpa(LV_OPA_COVER);
        _question->setImageRecolor(color);
    }
}

void DoubtDecorator::setVisible(bool visible)
{
    Element::setVisible(visible);
    if (_question) {
        _question->setHidden(!visible);
    }
}
