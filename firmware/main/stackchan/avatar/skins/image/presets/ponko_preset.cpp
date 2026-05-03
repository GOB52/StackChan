// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "ponko_preset.h"

namespace stackchan::avatar::image {

std::unique_ptr<ImageAvatar> make_ponko_avatar()
{
    ImageAvatarConfig cfg;

    cfg.base_image_name = "ponko_base.png";
    cfg.base_x          = 0;
    cfg.base_y          = 0;
    cfg.base_w          = 320;
    cfg.base_h          = 240;

    cfg.eye_left.open_image_name   = "ponko_eye_left_open.png";
    cfg.eye_left.closed_image_name = "ponko_eye_left_closed.png";
    cfg.eye_left.x                 = 0;
    cfg.eye_left.y                 = 0;
    cfg.eye_left.width             = 140;
    cfg.eye_left.height            = 96;

    cfg.eye_right.open_image_name   = "ponko_eye_right_open.png";
    cfg.eye_right.closed_image_name = "ponko_eye_right_closed.png";
    cfg.eye_right.x                 = 180;
    cfg.eye_right.y                 = 0;
    cfg.eye_right.width             = 140;
    cfg.eye_right.height            = 96;

    cfg.mouth.normal_image_name = "ponko_mouth_smile.png";
    cfg.mouth.open_image_name   = "ponko_mouth_open_wide.png";
    cfg.mouth.normal_x          = 112;
    cfg.mouth.normal_y          = 148;
    cfg.mouth.normal_w          = 96;
    cfg.mouth.normal_h          = 40;
    cfg.mouth.open_x            = 104;
    cfg.mouth.open_y            = 144;
    cfg.mouth.open_w            = 112;
    cfg.mouth.open_h            = 52;

    // Default Y for Heart/Angry is -70 (forehead), Sweat is -72.
    // Override Y to ~+20 (cheek area, between eyes and mouth) keeping default X side.
    constexpr int CHEEK_Y = 20;
    cfg.emotion_decorators = {
        // {emotion, kind, anim_ms, has_custom_position, x, y}
        {Emotion::Happy,  EmotionDecoratorKind::Heart,  500, true, 108,  CHEEK_Y},  // 右頬
        {Emotion::Angry,  EmotionDecoratorKind::Angry,  500, true, 108,  CHEEK_Y},  // 右頬
        {Emotion::Sad,    EmotionDecoratorKind::Sweat,  700, true, -116, CHEEK_Y},  // 左頬
        // Phase 2-1: 目は ImageAvatar 側で強制閉じ。Doubt は Heart と同じ頬位置。
        {Emotion::Sleepy, EmotionDecoratorKind::Sleepy, 200, true, 108,  -70},      // 頭右上
        {Emotion::Doubt,  EmotionDecoratorKind::Doubt,  500, true, 108,  CHEEK_Y},  // 右頬 (Heart と同位置)
    };

    return std::make_unique<ImageAvatar>(std::move(cfg));
}

}  // namespace stackchan::avatar::image
