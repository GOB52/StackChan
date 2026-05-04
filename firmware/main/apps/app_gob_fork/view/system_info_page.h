// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <functional>
#include <memory>

namespace stackchan::gob_fork::view {

// Static system info page (Heap / PSRAM / Uptime / CPU / Reset reason).
// Snapshot taken at construction; no realtime refresh.
class SystemInfoPage {
public:
    using OnBack = std::function<void()>;

    explicit SystemInfoPage(OnBack on_back);
    ~SystemInfoPage();

    SystemInfoPage(const SystemInfoPage&)            = delete;
    SystemInfoPage& operator=(const SystemInfoPage&) = delete;

    // Called from host App's onRunning() (already inside LvglLockGuard).
    void update();

private:
    void build_ui();

    OnBack _on_back;
    bool   _back_pending = false;

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _header_label;
    // 2-column layout: right-aligned labels + left-aligned values for ':' alignment.
    std::unique_ptr<uitk::lvgl_cpp::Label>     _label_col;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _value_col;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_back;
};

}  // namespace stackchan::gob_fork::view
