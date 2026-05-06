// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
//
// Shared robot action helpers used by both MCP tools
// (firmware/main/hal/hal_mcp.cpp) and NFC command dispatcher
// (firmware/main/stackchan/nfc/nfc_cmd_dispatcher.cpp). Single source of
// truth so AI voice and physical NFC tags trigger identical behavior.
#pragma once
#include <string>

namespace stackchan {
namespace actions {

// Move head to neutral (yaw=0, pitch=0) with the given speed (servo units).
void head_home(int speed = 500);

// Start a dance sequence by style name. Recognized values:
//   "happy", "robot", "panic", "look_around"
// Any in-flight dance is replaced. Returns false on unknown style.
bool dance(const std::string& style);

// Stop the in-flight dance (if any). Safe to call when no dance is running.
// Returns true only if a dance was actually stopped.
bool stop_dance();

}  // namespace actions
}  // namespace stackchan
