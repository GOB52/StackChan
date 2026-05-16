/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "element.h"
#include <string_view>
#include <string>

namespace stackchan::avatar {

/**
 * @brief Speech bubble base class
 *
 */
class SpeechBubble : public Element {
public:
    virtual ~SpeechBubble() = default;

    virtual void setSpeech(std::string_view text)
    {
    }

    // GOB fork: utterance-level accumulator for tail-follow scroll. xiaozhi の
    // sentence chunk が連続到来したとき、前文を保持しつつ末尾を表示する。実装
    // しない skin は default impl が setSpeech にフォールバック。
    virtual void appendSpeech(std::string_view text)
    {
        setSpeech(text);
    }

    // GOB fork: TTS 終了後 (Speaking → Listening) に bubble を消すリクエスト。
    // slide animation 走行中なら完了を待ち、その後 delay_ms 経過してから
    // clearSpeech を呼ぶ実装を期待する。default impl は単純な即時 clearSpeech。
    virtual void requestDismissAfterRender(uint32_t delay_ms)
    {
        (void)delay_ms;
        clearSpeech();
    }

    virtual void clearSpeech()
    {
    }

    virtual void setTextFont(void* font)
    {
    }
};

}  // namespace stackchan::avatar
