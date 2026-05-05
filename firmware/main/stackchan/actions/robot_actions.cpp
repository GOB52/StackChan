// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
//
// Phase 2b — implementation. Acquires LvglLockGuard before touching
// motion/modifier state (matches the pattern used by self.robot.* MCP
// tools). Dance modifier ID is tracked atomically so a new dance request
// or stop_dance() can replace/cancel the in-flight one safely.
#include "robot_actions.h"
#include "../stackchan.h"
#include "../modifiers/dance.h"
#include "../../hal/hal.h"
#include <mooncake_log.h>
#include <atomic>
#include <memory>

namespace stackchan {
namespace actions {

namespace {
constexpr std::string_view _tag = "Actions";

// Tracks the in-flight DanceModifier slot id. -1 = no dance active.
// Atomic because dance()/stop_dance() may be called from NFC dispatch
// context as well as MCP tool callback context. Pool ID can be reused
// after destroy(), so we type-check via dynamic_cast before remove.
std::atomic<int> _dance_modifier_id{-1};

const animation::KeyframeSequence* lookup_dance_sequence(const std::string& style)
{
    if (style == "happy")       return &DanceModifier::Happy;
    if (style == "robot")       return &DanceModifier::Robot;
    if (style == "panic")       return &DanceModifier::Panic;
    if (style == "look_around") return &DanceModifier::LookAround;
    return nullptr;
}

// Cancel the in-flight dance if its slot still holds a DanceModifier.
// dynamic_cast guards against ID reuse (pool re-allocates the same id
// after destroy(), so a stale id may now point to an unrelated modifier).
void cancel_in_flight_dance_locked()
{
    int id = _dance_modifier_id.exchange(-1);
    if (id < 0) return;
    auto& sc = GetStackChan();
    Modifier* m = sc.getModifier(id);
    if (!m) return;
    if (!dynamic_cast<DanceModifier*>(m)) return;
    sc.removeModifier(id);
}
}  // namespace

void head_home(int speed)
{
    mclog::tagInfo(_tag, "head_home speed={}", speed);
    LvglLockGuard lock;
    auto& motion = GetStackChan().motion();
    motion.yawServo().moveWithSpeed(0, speed);
    motion.pitchServo().moveWithSpeed(0, speed);
}

bool dance(const std::string& style)
{
    auto* seq = lookup_dance_sequence(style);
    if (!seq) {
        mclog::tagWarn(_tag, "dance: unknown style '{}'", style);
        return false;
    }
    mclog::tagInfo(_tag, "dance style={}", style);
    LvglLockGuard lock;
    cancel_in_flight_dance_locked();
    int id = GetStackChan().addModifier(std::make_unique<DanceModifier>(*seq));
    _dance_modifier_id.store(id);
    return true;
}

bool stop_dance()
{
    LvglLockGuard lock;
    int id = _dance_modifier_id.exchange(-1);
    if (id < 0) return false;
    auto& sc = GetStackChan();
    Modifier* m = sc.getModifier(id);
    if (!m) return false;
    if (!dynamic_cast<DanceModifier*>(m)) return false;
    bool ok = sc.removeModifier(id);
    if (ok) mclog::tagInfo(_tag, "stop_dance: stopped id={}", id);
    return ok;
}

}  // namespace actions
}  // namespace stackchan
