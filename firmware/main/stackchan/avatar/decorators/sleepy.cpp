// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "decorators.h"
#include <hal/hal.h>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;

static const Vector2i _sleepy_default_position = Vector2i(108, -70);
static const int _sleepy_float_max             = 16;  // 上方向への最大移動量 (px)
static const int _sleepy_step_y                = 2;   // tick 毎の上方向移動量
static const int _sleepy_step_opa              = 32;  // tick 毎のフェード量

LV_IMAGE_DECLARE(decorator_zzz);

SleepyDecorator::SleepyDecorator(lv_obj_t* parent, uint32_t destroyAfterMs, uint32_t animationIntervalMs)
    : _animation_interval_ms(animationIntervalMs),
      _base_x(_sleepy_default_position.x),
      _base_y(_sleepy_default_position.y)
{
    _zzz = std::make_unique<Image>(parent);
    _zzz->setSrc(&decorator_zzz);
    _zzz->setAlign(LV_ALIGN_CENTER);
    _zzz->setPos(_base_x, _base_y);
    _zzz->setOpa(LV_OPA_COVER);

    _y_offset = 0;
    _opa      = LV_OPA_COVER;

    const uint32_t now = GetHAL().millis();
    if (destroyAfterMs > 0) {
        _destroy_at   = now + destroyAfterMs;
        _has_lifetime = true;
    }
    if (_animation_interval_ms > 0) {
        _next_animation_tick = now + _animation_interval_ms;
    }
}

SleepyDecorator::~SleepyDecorator() = default;

void SleepyDecorator::_update()
{
    const uint32_t now = GetHAL().millis();

    if (_has_lifetime && now >= _destroy_at) {
        requestDestroy();
        return;
    }

    if (_animation_interval_ms == 0 || now < _next_animation_tick) {
        return;
    }
    _next_animation_tick = now + _animation_interval_ms;

    _y_offset -= _sleepy_step_y;
    _opa -= _sleepy_step_opa;
    if (_opa <= 0 || _y_offset <= -_sleepy_float_max) {
        // 1 周期完了 → 元位置に戻して再スタート
        _y_offset = 0;
        _opa      = LV_OPA_COVER;
    }
    _zzz->setPos(_base_x, _base_y + _y_offset);
    _zzz->setOpa(static_cast<lv_opa_t>(_opa));
}

void SleepyDecorator::setPosition(const int x, const int y)
{
    _base_x = x;
    _base_y = y;
    if (_zzz) {
        _zzz->setPos(_base_x, _base_y + _y_offset);
    }
}

void SleepyDecorator::setColor(const lv_color_t color)
{
    if (_zzz) {
        _zzz->setImageRecolorOpa(LV_OPA_COVER);
        _zzz->setImageRecolor(color);
    }
}

void SleepyDecorator::setVisible(const bool visible)
{
    Element::setVisible(visible);
    if (_zzz) {
        _zzz->setHidden(!visible);
    }
}
