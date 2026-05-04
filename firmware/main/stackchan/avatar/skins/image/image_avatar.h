// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include "../../avatar/avatar.h"
#include "../../avatar/elements/feature.h"
#include "image_assets.h"
#include <lvgl.h>
#include <smooth_lvgl.hpp>
#include <memory>

namespace stackchan::avatar::image {

class ImageAvatar : public Avatar {
public:
    // Takes ownership of config (PngBuffer ownership transferred). PSRAM PNG
    // bytes inside config outlive this avatar (config is moved into _config).
    explicit ImageAvatar(ImageAvatarConfig config);
    ~ImageAvatar() override = default;

    void init(lv_obj_t* parent);
    uitk::lvgl_cpp::Container* getPanel() const override;

    void setEmotion(const Emotion& emotion, bool suppressDecorator = false) override;

    const HeadPetDecoratorConfig* getHeadPetConfig() const override { return &_config.head_pet; }

    const DizzyConfig* getDizzyConfig() const override { return &_config.dizzy; }

    // Per-skin eye centers derived from manifest's eye_left / eye_right rect.
    // Returned values are LV_ALIGN_CENTER offsets (screen center 160, 120 → 0).
    uitk::Vector2i getEyeCenterOffset(bool isLeft) const override
    {
        const auto& e = isLeft ? _config.eye_left : _config.eye_right;
        return uitk::Vector2i(e.x + e.width / 2 - 160, e.y + e.height / 2 - 120);
    }

private:
    ImageAvatarConfig _config;  // owns PngBuffer bytes (PSRAM)
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    lv_image_dsc_t _base_dsc{};  // points into _config.base.bytes
    std::unique_ptr<uitk::lvgl_cpp::Image> _base;

    int _emotion_decorator_id = -1;
};

class ImageEyes : public Feature {
public:
    // Borrows asset's PngBuffer (caller keeps ownership). dsc.data points to
    // asset.open.bytes / asset.closed.bytes — must outlive this object.
    ImageEyes(lv_obj_t* parent, const EyeAsset& asset);
    ~ImageEyes() override;

    void setWeight(int weight) override;
    void setVisible(bool visible) override;
    void setPosition(const uitk::Vector2i& position) override;

private:
    int _x = 0, _y = 0;
    int _width = 0, _height = 0;
    lv_image_dsc_t _dsc_open{};
    lv_image_dsc_t _dsc_closed{};
    std::unique_ptr<uitk::lvgl_cpp::Image> _img;
    bool _is_open = true;
};

class ImageMouth : public Feature {
public:
    // Borrows asset's PngBuffer (caller keeps ownership).
    ImageMouth(lv_obj_t* parent, const MouthAsset& asset);
    ~ImageMouth() override;

    void setWeight(int weight) override;
    void setVisible(bool visible) override;

private:
    int _normal_x = 0, _normal_y = 0, _normal_w = 0, _normal_h = 0;
    int _open_x   = 0, _open_y   = 0, _open_w   = 0, _open_h   = 0;
    lv_image_dsc_t _dsc_normal{};
    lv_image_dsc_t _dsc_open{};
    std::unique_ptr<uitk::lvgl_cpp::Image> _img;
    bool _is_open = false;
};

}  // namespace stackchan::avatar::image
