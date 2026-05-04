// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace stackchan::gob_fork::view {

// Screensaver timeout selection page. Persists to NVS via gob_fork_nvs.
// Tapping an option saves immediately and invokes onBack (auto-return).
class ScreensaverSettingsPage {
public:
    using OnBack = std::function<void()>;

    explicit ScreensaverSettingsPage(OnBack on_back);
    ~ScreensaverSettingsPage();

    ScreensaverSettingsPage(const ScreensaverSettingsPage&)            = delete;
    ScreensaverSettingsPage& operator=(const ScreensaverSettingsPage&) = delete;

    void update();

private:
    void build_ui();
    void apply_choice(uint32_t seconds);

    OnBack   _on_back;
    bool     _back_pending = false;
    uint32_t _current_s    = 0;

    std::unique_ptr<uitk::lvgl_cpp::Container>            _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>                _header_label;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Button>>  _option_buttons;
    std::unique_ptr<uitk::lvgl_cpp::Button>               _btn_back;
};

}  // namespace stackchan::gob_fork::view
