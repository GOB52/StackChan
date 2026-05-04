// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
//
// Avatar Decorator wrapping the existing view::Toast widget so xiaozhi
// runtime (mooncake destroyed) can pop toast notifications via the Avatar
// update tick. The mooncake-side ToastManager / view::pop_a_toast path is
// unaffected; both can coexist (use view::pop_a_toast in launcher / app
// modes, stackchan::avatar::pop_avatar_toast in xiaozhi runtime).
#pragma once
#include "../avatar/decorator.h"
#include <apps/common/toast/toast.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace stackchan::avatar {

class ToastDecorator : public Decorator {
public:
    ToastDecorator(lv_obj_t* parent, const ::view::Toast::Config_t& cfg);
    ~ToastDecorator() override;

    void _update() override;

    // Apply the visual stack-back effect (mirrors ToastManager::onRunning's
    // call to Toast::stack on existing toasts when a new one arrives).
    void stack();

    // Live ToastDecorator instances. Single-thread access expected
    // (LvglLockGuard required at all use sites).
    static const std::vector<ToastDecorator*>& liveInstances();

private:
    ::view::Toast _toast;
};

// Pop a toast on the Avatar. No-op (warn log only) if Avatar is not
// attached - use view::pop_a_toast for launcher / app_setup modes where
// no Avatar is displayed. Intended for xiaozhi runtime callers (NFC
// dispatcher today; future MCP wrappers, HAL Signals, etc.).
void pop_avatar_toast(std::string_view  msg,
                      ::view::ToastType type       = ::view::ToastType::Info,
                      uint32_t          durationMs = 2000);

}  // namespace stackchan::avatar
