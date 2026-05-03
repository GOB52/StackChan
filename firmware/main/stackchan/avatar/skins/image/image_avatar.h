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
    explicit ImageAvatar(ImageAvatarConfig config);
    ~ImageAvatar() override = default;

    void init(lv_obj_t* parent);
    uitk::lvgl_cpp::Container* getPanel() const;

    void setEmotion(const Emotion& emotion) override;

private:
    ImageAvatarConfig _config;
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    lv_image_dsc_t _base_dsc{};
    std::unique_ptr<uitk::lvgl_cpp::Image> _base;

    int _emotion_decorator_id = -1;
};

class ImageEyes : public Feature {
public:
    ImageEyes(lv_obj_t* parent, EyeAsset asset);
    ~ImageEyes() override;

    void setWeight(int weight) override;
    void setVisible(bool visible) override;
    void setPosition(const uitk::Vector2i& position) override;

private:
    EyeAsset _asset;
    lv_image_dsc_t _dsc_open{};
    lv_image_dsc_t _dsc_closed{};
    std::unique_ptr<uitk::lvgl_cpp::Image> _img;
    bool _is_open = true;
};

class ImageMouth : public Feature {
public:
    ImageMouth(lv_obj_t* parent, MouthAsset asset);
    ~ImageMouth() override;

    void setWeight(int weight) override;
    void setVisible(bool visible) override;

private:
    MouthAsset _asset;
    lv_image_dsc_t _dsc_normal{};
    lv_image_dsc_t _dsc_open{};
    std::unique_ptr<uitk::lvgl_cpp::Image> _img;
    bool _is_open = false;
};

}  // namespace stackchan::avatar::image
