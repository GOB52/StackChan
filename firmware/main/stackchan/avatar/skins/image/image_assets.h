// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include "../../avatar/elements/emotion.h"
#include <cstdint>
#include <string>
#include <vector>

namespace stackchan::avatar::image {

struct EyeAsset {
    std::string open_image_name;
    std::string closed_image_name;
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;
};

struct MouthAsset {
    std::string normal_image_name;
    std::string open_image_name;
    int normal_x = 0, normal_y = 0;
    int open_x   = 0, open_y   = 0;
    int normal_w = 0, normal_h = 0;
    int open_w   = 0, open_h   = 0;
};

enum class EmotionDecoratorKind {
    None,
    Heart,
    Angry,
    Sweat,
    Shy,
    Dizzy,
};

struct EmotionDecoratorMapping {
    Emotion emotion                = Emotion::Neutral;
    EmotionDecoratorKind kind      = EmotionDecoratorKind::None;
    uint32_t animation_interval_ms = 500;
    // Optional position override (LV_ALIGN_CENTER offset).
    // Heart/Angry/Sweat: absolute offset replacing default.
    // Shy/Dizzy: extra offset added to left/right default positions.
    bool has_custom_position = false;
    int x                    = 0;
    int y                    = 0;
};

struct ImageAvatarConfig {
    std::string base_image_name;
    int base_x = 0, base_y = 0;
    int base_w = 320, base_h = 240;

    EyeAsset eye_left;
    EyeAsset eye_right;
    MouthAsset mouth;

    std::vector<EmotionDecoratorMapping> emotion_decorators;
};

}  // namespace stackchan::avatar::image
