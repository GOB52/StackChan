// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "image_avatar.h"
#include "../../decorators/decorators.h"
#include <assets/assets.h>
#include <mooncake_log.h>
#include <algorithm>
#include <utility>

using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar;
using namespace stackchan::avatar::image;

static const std::string_view _tag = "ImageAvatar";

ImageAvatar::ImageAvatar(ImageAvatarConfig config) : _config(std::move(config))
{
}

void ImageAvatar::init(lv_obj_t* parent)
{
    _panel = std::make_unique<Container>(parent);
    _panel->align(LV_ALIGN_CENTER, 0, 0);
    _panel->setSize(320, 240);
    _panel->setRadius(0);
    _panel->setBorderWidth(0);
    _panel->setBgOpa(0);
    _panel->setPadding(0, 0, 0, 0);
    _panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    if (!_config.base_image_name.empty()) {
        _base_dsc = assets::get_image(_config.base_image_name);
        _base     = std::make_unique<Image>(_panel->get());
        _base->setAlign(LV_ALIGN_TOP_LEFT);
        _base->setPos(_config.base_x, _config.base_y);
        _base->setSize(_config.base_w, _config.base_h);
        _base->setSrc(&_base_dsc);
    }

    _key_elements.leftEye  = std::make_unique<ImageEyes>(_panel->get(), _config.eye_left);
    _key_elements.rightEye = std::make_unique<ImageEyes>(_panel->get(), _config.eye_right);
    _key_elements.mouth    = std::make_unique<ImageMouth>(_panel->get(), _config.mouth);
}

Container* ImageAvatar::getPanel() const
{
    return _panel ? _panel.get() : nullptr;
}

void ImageAvatar::setEmotion(const Emotion& emotion)
{
    Avatar::setEmotion(emotion);

    if (_emotion_decorator_id >= 0) {
        removeDecorator(_emotion_decorator_id);
        _emotion_decorator_id = -1;
    }

    auto it = std::find_if(_config.emotion_decorators.begin(), _config.emotion_decorators.end(),
                           [&](const EmotionDecoratorMapping& m) { return m.emotion == emotion; });
    if (it == _config.emotion_decorators.end() || it->kind == EmotionDecoratorKind::None) {
        return;
    }

    auto* deco_parent = lv_screen_active();
    std::unique_ptr<Decorator> deco;
    switch (it->kind) {
        case EmotionDecoratorKind::Heart: {
            auto h = std::make_unique<HeartDecorator>(deco_parent, 0, it->animation_interval_ms);
            if (it->has_custom_position) h->setPosition(it->x, it->y);
            deco = std::move(h);
            break;
        }
        case EmotionDecoratorKind::Angry: {
            auto a = std::make_unique<AngryDecorator>(deco_parent, 0, it->animation_interval_ms);
            if (it->has_custom_position) a->setPosition(it->x, it->y);
            deco = std::move(a);
            break;
        }
        case EmotionDecoratorKind::Sweat: {
            auto s = std::make_unique<SweatDecorator>(deco_parent, 0, it->animation_interval_ms);
            if (it->has_custom_position) s->setPosition(it->x, it->y);
            deco = std::move(s);
            break;
        }
        case EmotionDecoratorKind::Shy: {
            auto s = std::make_unique<ShyDecorator>(deco_parent, 0);
            if (it->has_custom_position) s->setPosition(it->x, it->y);
            deco = std::move(s);
            break;
        }
        case EmotionDecoratorKind::Dizzy: {
            auto d = std::make_unique<DizzyDecorator>(deco_parent, 0, it->animation_interval_ms);
            if (it->has_custom_position) d->setPosition(it->x, it->y);
            deco = std::move(d);
            break;
        }
        default:
            return;
    }

    if (deco) {
        _emotion_decorator_id = addDecorator(std::move(deco));
    }
}
