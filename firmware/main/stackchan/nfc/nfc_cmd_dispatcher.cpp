// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "nfc_cmd_dispatcher.h"
#include "../../hal/hal.h"
#include "../../hal/board/hal_bridge.h"
#include "../../assets/assets.h"
#include "../stackchan.h"
#include "../actions/robot_actions.h"
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

// Phase 3 validation — ranges/units mirror the corresponding self.robot.*
// MCP tools so AI voice and NFC tags share semantics.
//   move  : x=yaw deg (-128..128), y=pitch deg (0..90), internally *10
//           before moveWithSpeed (matches set_head_angles).
//   rgb   : r/g/b 0..168 (matches set_led_color "safe range").
//   home  : speed 100..1000 (matches head_home).
constexpr int _NO_CHANGE      = -9999;
constexpr int _YAW_MIN_DEG    = -128;
constexpr int _YAW_MAX_DEG    =  128;
constexpr int _PITCH_MIN_DEG  =  0;
constexpr int _PITCH_MAX_DEG  =  90;
constexpr int _SPEED_MIN      =  100;
constexpr int _SPEED_MAX      =  1000;
constexpr int _MOVE_SPEED_DEF =  150;
constexpr int _HOME_SPEED_DEF =  500;
constexpr int _RGB_MIN        =  0;
constexpr int _RGB_MAX        =  168;

// Type-checked extraction. Missing key uses default. Returns false when
// the key exists but has a wrong type — caller then drops the entire cmd
// (Q4: reject on type mismatch).
bool extract_int(const ArduinoJson::JsonObjectConst& args, const char* cmd,
                 const char* key, int default_val, int& out)
{
    auto v = args[key];
    if (v.isNull()) { out = default_val; return true; }
    if (!v.is<int>()) {
        mclog::tagWarn(_tag, "{} rejected: '{}' not an integer", cmd, key);
        return false;
    }
    out = v.as<int>();
    return true;
}

bool extract_str(const ArduinoJson::JsonObjectConst& args, const char* cmd,
                 const char* key, const char* default_val, std::string& out)
{
    auto v = args[key];
    if (v.isNull()) { out = default_val; return true; }
    if (!v.is<const char*>()) {
        mclog::tagWarn(_tag, "{} rejected: '{}' not a string", cmd, key);
        return false;
    }
    out = v.as<const char*>();
    return true;
}

// Clamp with warn log. Numeric values are clamped (Q1=c hybrid: numbers
// clamp, strings/enums reject).
int clamp_int(const char* cmd, const char* key, int v, int min, int max)
{
    if (v < min) {
        mclog::tagWarn(_tag, "{} '{}' clamped: {} -> {} (min)", cmd, key, v, min);
        return min;
    }
    if (v > max) {
        mclog::tagWarn(_tag, "{} '{}' clamped: {} -> {} (max)", cmd, key, v, max);
        return max;
    }
    return v;
}

void toast_reject(const char* cmd, const char* reason)
{
    char buf[96];
    std::snprintf(buf, sizeof(buf), "!! NFC.%s(reject: %s)", cmd, reason);
    stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
}
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
        // x=yaw deg, y=pitch deg (mirrors MCP set_head_angles unit). Internally
        // multiplied by 10 to get the servo's native 0.1° unit.
        int x_deg, y_deg, s;
        if (!extract_int(args, "move", "x", _NO_CHANGE,      x_deg) ||
            !extract_int(args, "move", "y", _NO_CHANGE,      y_deg) ||
            !extract_int(args, "move", "s", _MOVE_SPEED_DEF, s)) {
            toast_reject("move", "type");
            return;
        }
        if (x_deg != _NO_CHANGE) x_deg = clamp_int("move", "x", x_deg, _YAW_MIN_DEG,   _YAW_MAX_DEG);
        if (y_deg != _NO_CHANGE) y_deg = clamp_int("move", "y", y_deg, _PITCH_MIN_DEG, _PITCH_MAX_DEG);
        s = clamp_int("move", "s", s, _SPEED_MIN, _SPEED_MAX);

        mclog::tagInfo(_tag, "move x={} y={} s={}", x_deg, y_deg, s);
        {
            LvglLockGuard lock;
            auto& motion = GetStackChan().motion();
            if (y_deg != _NO_CHANGE) motion.pitchServo().moveWithSpeed(y_deg * 10, s);
            if (x_deg != _NO_CHANGE) motion.yawServo().moveWithSpeed(x_deg * 10, s);
        }
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%% NFC.move(x=%d, y=%d, s=%d)", x_deg, y_deg, s);
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });
    d.registerHandler("rgb", [](const ArduinoJson::JsonObjectConst& args) {
        int r, g, b;
        if (!extract_int(args, "rgb", "r", 0, r) ||
            !extract_int(args, "rgb", "g", 0, g) ||
            !extract_int(args, "rgb", "b", 0, b)) {
            toast_reject("rgb", "type");
            return;
        }
        r = clamp_int("rgb", "r", r, _RGB_MIN, _RGB_MAX);
        g = clamp_int("rgb", "g", g, _RGB_MIN, _RGB_MAX);
        b = clamp_int("rgb", "b", b, _RGB_MIN, _RGB_MAX);

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

    // Phase 2b — wired to actions/ shared layer. The same logic powers
    // the corresponding self.robot.* MCP tools so AI voice and NFC tags
    // produce identical behavior. See stackchan/actions/robot_actions.{h,cpp}.
    d.registerHandler("home", [](const ArduinoJson::JsonObjectConst& args) {
        int s;
        if (!extract_int(args, "home", "s", _HOME_SPEED_DEF, s)) {
            toast_reject("home", "type");
            return;
        }
        s = clamp_int("home", "s", s, _SPEED_MIN, _SPEED_MAX);
        mclog::tagInfo(_tag, "home s={}", s);
        stackchan::actions::head_home(s);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%% NFC.home(s=%d)", s);
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });
    d.registerHandler("dance", [](const ArduinoJson::JsonObjectConst& args) {
        // MCP tool uses string "style"; NFC payload uses the same key for
        // consistency. Default is "happy" (matches MCP default).
        std::string style;
        if (!extract_str(args, "dance", "style", "happy", style)) {
            toast_reject("dance", "type");
            return;
        }
        mclog::tagInfo(_tag, "dance style={}", style);
        bool ok = stackchan::actions::dance(style);
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      ok ? "%% NFC.dance(style=%s)" : "%% NFC.dance(unknown style=%s)",
                      style.c_str());
        stackchan::avatar::pop_avatar_toast(buf, view::ToastType::Info, _TOAST_MS);
    });
    d.registerHandler("stop_dance", [](const ArduinoJson::JsonObjectConst& args) {
        bool stopped = stackchan::actions::stop_dance();
        mclog::tagInfo(_tag, "stop_dance stopped={}", stopped);
        stackchan::avatar::pop_avatar_toast(
            stopped ? "% NFC.stop_dance(stopped)" : "% NFC.stop_dance(no dance)",
            view::ToastType::Info, _TOAST_MS);
    });

    d.connectToHal();
    mclog::tagInfo(_tag, "dispatcher initialized ({} handlers)", (unsigned)d._handlers.size());
}

}  // namespace nfc
}  // namespace stackchan
