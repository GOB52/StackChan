// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "screensaver_settings_page.h"
#include <stackchan/gob_fork_nvs.h>
#include <apps/common/common.h>
#include <mooncake_log.h>
#include <fmt/format.h>

namespace stackchan::gob_fork::view {

using namespace uitk::lvgl_cpp;

namespace {

const char* _tag = "ScreensaverSettings";

struct Option {
    uint32_t    seconds;
    const char* label;
};

constexpr Option _options[] = {
    {0,   "Off"},
    {30,  "30 sec"},
    {60,  "1 min"},
    {300, "5 min"},
    {600, "10 min"},
};

}  // namespace

ScreensaverSettingsPage::ScreensaverSettingsPage(OnBack on_back) : _on_back(std::move(on_back))
{
    _current_s = stackchan::gob_fork::get_screensaver_timeout_s();
    build_ui();
}

ScreensaverSettingsPage::~ScreensaverSettingsPage() = default;

void ScreensaverSettingsPage::build_ui()
{
    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setBgColor(lv_color_hex(0xE8F5E9));
    _panel->setPadding(0, 0, 0, 0);
    _panel->setBorderWidth(0);
    _panel->setRadius(0);
    _panel->setScrollDir(LV_DIR_VER);
    _panel->setScrollbarMode(LV_SCROLLBAR_MODE_AUTO);

    _header_label = std::make_unique<Label>(*_panel);
    _header_label->setText("Screensaver Timeout");
    _header_label->setTextFont(&lv_font_montserrat_16);
    _header_label->setTextColor(lv_color_hex(0x1B5E20));
    _header_label->align(LV_ALIGN_TOP_LEFT, 12, 30);

    int y = 54;
    constexpr int btn_h = 28;
    constexpr int btn_gap = 4;

    for (const auto& opt : _options) {
        auto btn = std::make_unique<Button>(*_panel);
        btn->setSize(296, btn_h);
        btn->align(LV_ALIGN_TOP_LEFT, 12, y);
        bool is_current = (opt.seconds == _current_s);
        btn->setBgColor(lv_color_hex(is_current ? 0x4CAF50 : 0xC8E6C9));
        btn->setBorderWidth(0);
        btn->setShadowWidth(0);
        btn->setRadius(8);
        std::string text = is_current ? fmt::format("{}  [current]", opt.label)
                                      : std::string(opt.label);
        btn->label().setText(text.c_str());
        btn->label().setTextFont(&lv_font_montserrat_14);
        btn->label().setTextColor(lv_color_hex(is_current ? 0xFFFFFF : 0x1B5E20));
        uint32_t target = opt.seconds;
        btn->onClick().connect([this, target]() { apply_choice(target); });
        _option_buttons.push_back(std::move(btn));
        y += btn_h + btn_gap;
    }

    _btn_back = std::make_unique<Button>(*_panel);
    _btn_back->setSize(296, 32);
    _btn_back->setBgColor(lv_color_hex(0x1B5E20));
    _btn_back->setBorderWidth(0);
    _btn_back->setShadowWidth(0);
    _btn_back->setRadius(12);
    _btn_back->label().setText(LV_SYMBOL_LEFT " Back");
    _btn_back->label().setTextFont(&lv_font_montserrat_16);
    _btn_back->label().setTextColor(lv_color_hex(0xFFFFFF));
    _btn_back->onClick().connect([this]() { _back_pending = true; });

    // Place back below the last option so it scrolls together (panel scroll AUTO).
    if (!_option_buttons.empty()) {
        lv_obj_align_to(_btn_back->raw_ptr(),
                        _option_buttons.back()->raw_ptr(),
                        LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
    }
}

void ScreensaverSettingsPage::apply_choice(uint32_t seconds)
{
    if (seconds == _current_s) {
        _back_pending = true;  // no change → just go back
        return;
    }
    if (!stackchan::gob_fork::set_screensaver_timeout_s(seconds)) {
        ::view::pop_a_toast("NVS save failed", ::view::ToastType::Error, 2000);
        return;
    }
    mclog::tagInfo(_tag, "screensaver timeout saved: {}s", seconds);
    ::view::pop_a_toast("Saved (effective on launcher resume)",
                        ::view::ToastType::Info, 1600);
    _current_s    = seconds;
    _back_pending = true;
}

void ScreensaverSettingsPage::update()
{
    if (_back_pending) {
        _back_pending = false;
        if (_on_back) _on_back();
    }
}

}  // namespace stackchan::gob_fork::view
