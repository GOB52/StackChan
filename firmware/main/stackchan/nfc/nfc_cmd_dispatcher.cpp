// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "nfc_cmd_dispatcher.h"
#include "../../hal/hal.h"
#include <mooncake_log.h>
#include <string_view>

namespace stackchan {
namespace nfc {

namespace {
constexpr std::string_view _tag = "NFC-CMD";
constexpr int              _SUPPORTED_VERSION = 1;
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

    // Stub handlers — Phase 2 replaces these with MCP tool calls
    // (self.robot.set_head_angles / set_led_color / new self.robot.head_home /
    // self.robot.dance). For now they only log so we can verify parsing and
    // routing end-to-end.
    d.registerHandler("move", [](const ArduinoJson::JsonObjectConst& args) {
        int x = args["x"] | 0;
        int y = args["y"] | 0;
        int s = args["s"] | 500;
        mclog::tagInfo(_tag, "[stub] move x={} y={} s={}", x, y, s);
    });
    d.registerHandler("home", [](const ArduinoJson::JsonObjectConst& args) {
        int s = args["s"] | 500;
        mclog::tagInfo(_tag, "[stub] home s={}", s);
    });
    d.registerHandler("rgb", [](const ArduinoJson::JsonObjectConst& args) {
        int r = args["r"] | 0;
        int g = args["g"] | 0;
        int b = args["b"] | 0;
        mclog::tagInfo(_tag, "[stub] rgb r={} g={} b={}", r, g, b);
    });
    d.registerHandler("dance", [](const ArduinoJson::JsonObjectConst& args) {
        int id = args["id"] | 0;
        mclog::tagInfo(_tag, "[stub] dance id={}", id);
    });

    d.connectToHal();
    mclog::tagInfo(_tag, "dispatcher initialized ({} handlers)", (unsigned)d._handlers.size());
}

}  // namespace nfc
}  // namespace stackchan
