// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "nfc_cmd_dispatcher.h"
#include "../../hal/hal.h"
#include "../../hal/board/hal_bridge.h"
#include "../../assets/assets.h"
#include "../stackchan.h"
#include "../avatar/decorators/toast_decorator.h"
#include <mooncake_log.h>
#include <cstdio>
#include <string_view>

namespace stackchan {
namespace nfc {

namespace {
constexpr std::string_view _tag = "NFC-CMD";
constexpr int              _SUPPORTED_VERSION = 1;

// Toast duration for cmd ack. Long enough to read the parameters, short enough
// not to compete with AI TTS bubble. Multi-record tags stack via the Avatar
// Decorator pool (ToastDecorator::stack, mirrors ToastManager behavior).
constexpr uint32_t _TOAST_MS = 2000;
}  // namespace

void CmdDispatcher::registerHandler(const std::string& cmd_name, Handler h)
{
    _handlers[cmd_name] = std::move(h);
}

void CmdDispatcher::dispatch(const NfcCmdEvent_t& ev)
{
    ArduinoJson::JsonDocument doc;
    auto err = ArduinoJson::deserializeJson(doc, ev.payload.data(), ev.payload.size());
    if (err) {
        mclog::tagWarn(_tag, "JSON parse failed uid={} bytes={} err={}",
                       ev.uid, ev.payload.size(), err.c_str());
        return;
    }

    if (!doc["v"].is<int>()) {
        mclog::tagWarn(_tag, "missing/invalid 'v' field uid={}", ev.uid);
        return;
    }
    int v = doc["v"].as<int>();
    if (v != _SUPPORTED_VERSION) {
        mclog::tagWarn(_tag, "unsupported version {} (expected {}) uid={}",
                       v, _SUPPORTED_VERSION, ev.uid);
        return;
    }

    if (!doc["cmd"].is<const char*>()) {
        mclog::tagWarn(_tag, "missing/invalid 'cmd' field uid={}", ev.uid);
        return;
    }
    std::string cmd = doc["cmd"].as<const char*>();

    auto it = _handlers.find(cmd);
    if (it == _handlers.end()) {
        mclog::tagWarn(_tag, "unknown cmd '{}' uid={}", cmd, ev.uid);
        return;
    }

    it->second(doc.as<ArduinoJson::JsonObjectConst>());

    // Audio feedback on successful dispatch (xiaozhi notification chime).
    // Errors / unknown cmd above return before this so users only hear ding
    // on accepted commands. Suppressed during xiaozhi TTS so the chime
    // doesn't step on AI speech audio (toast still shows).
    if (!hal_bridge::is_xiaozhi_speaking()) {
        hal_bridge::app_play_sound(OGG_NEW_NOTIFICATION);
    }
}

void CmdDispatcher::connectToHal()
{
    if (_connected) return;
    GetHAL().onNfcCmdReceived.connect(
        [this](const NfcCmdEvent_t& ev) { dispatch(ev); });
    _connected = true;
}

CmdDispatcher& CmdDispatcher::instance()
{
    static CmdDispatcher inst;
    return inst;
}

void CmdDispatcher::initOnce()
{
    auto& d = instance();
    if (d._connected) return;

    // Phase 2a — Wrap existing functionality the same way MCP self.robot.*
    // tools do (LvglLockGuard + GetStackChan() / motion / neon light). Wrapping
    // here (rather than via the MCP server's JSON-RPC path) keeps this fork
    // free of xiaozhi-side patches; the underlying API calls are identical.
    //
    // Codex spec units (tmp/stackchan_ndef_command_type.md):
    //   move: x=yaw, y=pitch, s=speed. Angles in 0.1° units (10 = 1°).
    //   rgb : r/g/b 0..255.
    // Phase 3 will add range clamping / validation per cmd.

    d.registerHandler("move", [](const ArduinoJson::JsonObjectConst& args) {
        constexpr int NO_CHANGE = -9999;
        int x = args["x"] | NO_CHANGE;  // yaw (0.1°)
        int y = args["y"] | NO_CHANGE;  // pitch (0.1°)
        int s = args["s"] | 150;         // speed (matches MCP default)
        mclog::tagInfo(_tag, "move x={} y={} s={}", x, y, s);
        {
            LvglLockGuard lock;
            auto& motion = GetStackChan().motion();
            if (y != NO_CHANGE) motion.pitchServo().moveWithSpeed(y, s);
            if (x != NO_CHANGE) motion.yawServo().moveWithSpeed(x, s);
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%% NFC.move(x=%d, y=%d, s=%d)", x, y, s);
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });
    d.registerHandler("rgb", [](const ArduinoJson::JsonObjectConst& args) {
        int r = args["r"] | 0;
        int g = args["g"] | 0;
        int b = args["b"] | 0;
        mclog::tagInfo(_tag, "rgb r={} g={} b={}", r, g, b);
        {
            LvglLockGuard lock;
            GetStackChan().leftNeonLight().setColor(
                static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
            GetStackChan().rightNeonLight().setColor(
                static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%% NFC.rgb(%d, %d, %d)", r, g, b);
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });

    // Phase 2b will add MCP tools self.robot.head_home / self.robot.dance and
    // wire these handlers to call the same underlying APIs.
    d.registerHandler("home", [](const ArduinoJson::JsonObjectConst& args) {
        int s = args["s"] | 500;
        mclog::tagInfo(_tag, "[stub] home s={}", s);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%% NFC.home(s=%d)", s);
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });
    d.registerHandler("dance", [](const ArduinoJson::JsonObjectConst& args) {
        int id = args["id"] | 0;
        mclog::tagInfo(_tag, "[stub] dance id={}", id);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%% NFC.dance(id=%d)", id);
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });

    d.connectToHal();
    mclog::tagInfo(_tag, "dispatcher initialized ({} handlers)", (unsigned)d._handlers.size());
}

}  // namespace nfc
}  // namespace stackchan
