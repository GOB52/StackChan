// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
//
// Parses stackchan:cmd MIME payloads (UTF-8 JSON) emitted by hal_nfc and
// dispatches to per-command handlers. Handlers are thin wrappers around the
// shared actions/ layer (same code path as `self.robot.*` MCP tools).
#pragma once
#include <sdkconfig.h>
#include <ArduinoJson.hpp>
#include <functional>
#include <map>
#include <string>

struct NfcCmdEvent_t;  // declared in hal/hal.h

namespace stackchan {
namespace nfc {

class CmdDispatcher {
public:
    using Handler = std::function<void(const ArduinoJson::JsonObjectConst&)>;

    CmdDispatcher() = default;

    // Register a handler for a NFC short cmd name (e.g. "rgb"). Replaces any
    // existing handler for the same name.
    void registerHandler(const std::string& cmd_name, Handler h);

    // Parse + validate + route. Errors at any stage are logged at warn level
    // and the call returns silently (no abort, no exception).
    void dispatch(const NfcCmdEvent_t& ev) const;

    // Bind dispatch() to GetHAL().onNfcCmdReceived. Idempotent.
    void connectToHal();

    // Process-wide singleton (owns signal binding state).
    static CmdDispatcher& instance();

    // Register the built-in command handlers and connect to HAL. Safe to call
    // multiple times.
    static void initOnce();

private:
    std::map<std::string, Handler> _handlers;
    bool                           _connected{false};
};

}  // namespace nfc
}  // namespace stackchan
