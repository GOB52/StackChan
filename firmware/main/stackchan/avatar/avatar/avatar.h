/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// Modified by GOB (X:@GOB_52_GOB / GitHub:GOB52) - StackChan firmware fork
#pragma once
#include "elements/key_elements.h"
#include "decorator.h"
#include <cstdint>
#include <memory>

// Forward decl so base Avatar can expose a getPanel() polymorphic API without
// pulling smooth_lvgl into this header.
namespace smooth_ui_toolkit::lvgl_cpp { class Container; }

namespace stackchan::avatar {

// GOB fork: per-skin head-pet decorator positions. Coordinates are
// LV_ALIGN_CENTER offsets (screen center = 0,0) to match the existing
// Heart/Shy default-position semantics. has_* flags let the host code fall
// back to each decorator's built-in default when a skin doesn't provide
// custom positions (preserves DefaultAvatar behavior).
struct HeadPetDecoratorConfig {
    bool     has_heart     = false;
    int      heart_x       = 0;
    int      heart_y       = 0;
    uint32_t heart_anim_ms = 500;

    bool has_shy     = false;
    int  shy_left_x  = 0;
    int  shy_left_y  = 0;
    int  shy_right_x = 0;
    int  shy_right_y = 0;
};

// GOB fork: per-skin Dizzy decorator customization (used when shaken). When
// has_position = false the position auto-derives from the avatar's eye
// centers via getEyeCenterOffset(); when true, the explicit left/right
// LV_ALIGN_CENTER offsets are used (e.g. to align with the pupil instead of
// the eye PNG bounding box). scale = 1.0f means no scaling (LVGL native 256).
struct DizzyConfig {
    bool     has_color = false;
    uint32_t color_hex = 0xFFFFFFu;
    uint32_t anim_ms   = 300;
    bool     has_scale = false;
    float    scale     = 1.0f;
    bool     has_position = false;
    int      left_x  = 0;
    int      left_y  = 0;
    int      right_x = 0;
    int      right_y = 0;
};

/**
 * @brief Avatar base class
 *
 */
class Avatar {
public:
    virtual ~Avatar() = default;

    /**
     * @brief Top-level container of the avatar (used by host code to attach
     * click handlers etc.). Concrete subclasses (DefaultAvatar, ImageAvatar)
     * return their root panel.
     */
    virtual smooth_ui_toolkit::lvgl_cpp::Container* getPanel() const = 0;

    /**
     * @brief GOB fork: optional per-skin head-pet decorator positions.
     * Returns nullptr by default → host code uses decorator built-in defaults
     * (DefaultAvatar behavior). ImageAvatar overrides to expose its config.
     */
    virtual const HeadPetDecoratorConfig* getHeadPetConfig() const { return nullptr; }

    /**
     * @brief GOB fork: per-skin eye center (LV_ALIGN_CENTER offset). Used by
     * IMUModifier to align the Dizzy decorator over the actual eyes regardless
     * of skin. Default returns DefaultEyes/DefaultDizzy positions (-70/+70, -16)
     * to preserve DefaultAvatar behavior; ImageAvatar overrides with values
     * derived from the manifest's eye_left / eye_right rect.
     */
    virtual uitk::Vector2i getEyeCenterOffset(bool isLeft) const
    {
        return isLeft ? uitk::Vector2i(-70, -16) : uitk::Vector2i(70, -16);
    }

    /**
     * @brief GOB fork: optional per-skin Dizzy decorator color / anim / scale.
     * Returns nullptr by default → IMUModifier uses DizzyDecorator built-in
     * defaults (DefaultAvatar behavior). ImageAvatar overrides to expose its
     * manifest-driven config.
     */
    virtual const DizzyConfig* getDizzyConfig() const { return nullptr; }

    /**
     * @brief Update avatar, trigger all elements, decorators and modifiers to update
     *
     */
    virtual void update()
    {
        _key_elements.forEach([](Element* element) {
            // Update all elements
            element->_update();
        });

        _decorator_pool.forEach([this](Decorator* decorator, int id) {
            // Update all decorators
            decorator->_update();
        });

        // Cleanup pools
        _decorator_pool.cleanup();
    }

    const KeyElements_t& getKeyElements()
    {
        return _key_elements;
    }

    /**
     * @brief Set emotion. GOB fork: suppressDecorator=true makes ImageAvatar
     * skip adding the manifest's emotion-decorator (Heart/Sweat/...) for this
     * call. DefaultAvatar ignores the flag (no decorator system). Used by
     * HeadPetModifier to avoid double-Heart when Happy is set as a side
     * effect of head petting (HeadPet adds its own Heart/Shy decorators).
     */
    virtual void setEmotion(const Emotion& emotion, bool suppressDecorator = false)
    {
        (void)suppressDecorator;  // base impl has no decorator-from-emotion logic
        _emotion = emotion;

        _key_elements.forEach([&emotion](Element* element) {
            // Set for all elements
            element->setEmotion(emotion);
        });

        _decorator_pool.forEach([&emotion](Decorator* decorator, int id) {
            // Set for all decorators
            decorator->setEmotion(emotion);
        });
    }

    Emotion getEmotion() const
    {
        return _emotion;
    }

    Feature& leftEye()
    {
        return *getKeyElements().leftEye;
    }

    Feature& rightEye()
    {
        return *getKeyElements().rightEye;
    }

    Feature& mouth()
    {
        return *getKeyElements().mouth;
    }

    void setSpeech(std::string_view text)
    {
        if (getKeyElements().speechBubble) {
            getKeyElements().speechBubble->setSpeech(text);
        }
    }

    // GOB fork: 連続 chunk を accumulate して tail-follow scroll させる。
    // xiaozhi の SetChatMessage("assistant", ...) で利用。
    void appendSpeech(std::string_view text)
    {
        if (getKeyElements().speechBubble) {
            getKeyElements().speechBubble->appendSpeech(text);
        }
    }

    // GOB fork: TTS 終了後の bubble dismiss リクエスト (slide 完了 + delay_ms 後)。
    void requestSpeechDismissAfterRender(uint32_t delay_ms)
    {
        if (getKeyElements().speechBubble) {
            getKeyElements().speechBubble->requestDismissAfterRender(delay_ms);
        }
    }

    void clearSpeech()
    {
        if (getKeyElements().speechBubble) {
            getKeyElements().speechBubble->clearSpeech();
        }
    }

    void setSpeechTextFont(void* font)
    {
        if (getKeyElements().speechBubble) {
            getKeyElements().speechBubble->setTextFont(font);
        }
    }

    void setModifyLock(bool locked)
    {
        _is_modify_locked = locked;
    }

    bool isModifyLocked()
    {
        return _is_modify_locked;
    }

    /* ---------------------------- Decorator helpers --------------------------- */

    int addDecorator(std::unique_ptr<Decorator> decorator)
    {
        return _decorator_pool.create(std::move(decorator));
    }

    bool removeDecorator(int id)
    {
        return _decorator_pool.destroy(id);
    }

    void clearDecorators()
    {
        _decorator_pool.clear();
    }

protected:
    Avatar() = default;

    Emotion _emotion = Emotion::Neutral;
    KeyElements_t _key_elements;
    ObjectPool<Decorator> _decorator_pool;

    bool _is_modify_locked = false;
};

}  // namespace stackchan::avatar
