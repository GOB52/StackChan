// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <functional>
#include <memory>

namespace stackchan::gob_fork::view {

// Static About page (Firmware version + GOB fork git hash + build date).
class AboutPage {
public:
    using OnBack = std::function<void()>;

    explicit AboutPage(OnBack on_back);
    ~AboutPage();

    AboutPage(const AboutPage&)            = delete;
    AboutPage& operator=(const AboutPage&) = delete;

    void update();

private:
    void build_ui();

    OnBack _on_back;
    bool   _back_pending = false;

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _header_label;
    // 2-column for Firmware / GOB Fork / Build (top info block).
    std::unique_ptr<uitk::lvgl_cpp::Label>     _label_top;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _value_top;
    // 2-column for X / GitHub social rows.
    std::unique_ptr<uitk::lvgl_cpp::Label>     _label_x;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _value_x;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _label_github;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _value_github;
    // QR code is owned by panel (raw lv_obj_t*); destroyed when panel is destroyed.
    lv_obj_t*                                  _qr_obj = nullptr;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_back;
};

}  // namespace stackchan::gob_fork::view
