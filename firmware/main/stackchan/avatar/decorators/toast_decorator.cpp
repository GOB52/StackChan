// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "toast_decorator.h"
#include <hal/hal.h>
#include <stackchan/stackchan.h>
#include <mooncake_log.h>
#include <algorithm>

namespace stackchan::avatar {

namespace {
constexpr std::string_view _tag = "avatar-toast";

std::vector<ToastDecorator*>& _live()
{
    static std::vector<ToastDecorator*> v;
    return v;
}
}  // namespace

ToastDecorator::ToastDecorator(lv_obj_t* parent, const ::view::Toast::Config_t& cfg)
{
    _toast.config = cfg;
    _toast.init(parent);
    _toast.open();
    _live().push_back(this);
}

ToastDecorator::~ToastDecorator()
{
    auto& v = _live();
    v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

void ToastDecorator::_update()
{
    _toast.update();
    if (_toast.getState() == ::view::Toast::State::Closed) {
        requestDestroy();
    }
}

void ToastDecorator::stack()
{
    _toast.stack();
}

const std::vector<ToastDecorator*>& ToastDecorator::liveInstances()
{
    return _live();
}

void pop_avatar_toast(std::string_view msg, ::view::ToastType type, uint32_t durationMs)
{
    auto& sc = GetStackChan();
    if (!sc.hasAvatar()) {
        mclog::tagWarn(_tag, "no avatar; toast dropped: {}", msg);
        return;
    }

    LvglLockGuard lock;

    // Push existing toasts back before adding the new one. Matches
    // view::ToastManager::onRunning behavior (toast.cpp:184-188) so the
    // visual stack-back effect carries over to the Decorator path.
    for (auto* td : _live()) {
        td->stack();
    }

    ::view::Toast::Config_t cfg{type, durationMs, std::string(msg)};
    sc.avatar().addDecorator(
        std::make_unique<ToastDecorator>(lv_screen_active(), cfg));
}

}  // namespace stackchan::avatar
