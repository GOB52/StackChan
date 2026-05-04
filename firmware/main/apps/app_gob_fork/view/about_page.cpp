// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "about_page.h"
#include <fmt/format.h>
#include <cstring>
#include <cstdlib>

// GOB_FORK_GIT_HASH is defined unconditionally in firmware/CMakeLists.txt
// (falls back to "unknown" when git rev-parse fails). No source-side fallback
// needed.

namespace stackchan::gob_fork::view {

using namespace uitk::lvgl_cpp;

namespace {

// Convert __DATE__ "MMM DD YYYY" + __TIME__ "HH:MM:SS" to "YYYY/MM/DD HH:MM:SS".
std::string format_build_datetime()
{
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    int month = 0;
    for (int i = 0; i < 12; ++i) {
        if (std::strncmp(__DATE__, months[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }
    int day  = std::atoi(&__DATE__[4]);  // "MMM DD YYYY" → day at offset 4-5
    int year = std::atoi(&__DATE__[7]);  // year at offset 7-10
    return fmt::format("{:04}/{:02}/{:02} {}", year, month, day, __TIME__);
}

}  // namespace

AboutPage::AboutPage(OnBack on_back) : _on_back(std::move(on_back))
{
    build_ui();
}

AboutPage::~AboutPage() = default;

void AboutPage::build_ui()
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
    _header_label->setText("About");
    _header_label->setTextFont(&lv_font_montserrat_16);
    _header_label->setTextColor(lv_color_hex(0x1B5E20));
    _header_label->align(LV_ALIGN_TOP_LEFT, 12, 30);

    // QR code (top-right): GitHub fork repo URL. Sized large for easy scanning.
    // Info text flows below QR (panel scrolls when content overflows).
    constexpr int qr_size = 144;
    static constexpr const char* _qr_url = "https://github.com/GOB52/StackChan";
    _qr_obj = lv_qrcode_create(_panel->raw_ptr());
    lv_qrcode_set_size(_qr_obj, qr_size);
    lv_qrcode_set_dark_color(_qr_obj, lv_color_hex(0x000000));
    lv_qrcode_set_light_color(_qr_obj, lv_color_hex(0xFFFFFF));
    lv_qrcode_update(_qr_obj, _qr_url, std::strlen(_qr_url));
    lv_obj_align(_qr_obj, LV_ALIGN_TOP_RIGHT, -12, 30);  // ends at y=174

    // Info text starts just below the QR's bottom edge.
    constexpr int info_y_start = 30 + qr_size + 8;  // 182

    // 2-column layout for both top block (Firmware/GOB Fork/Build) and bottom
    // social block (X/GitHub). Same label column width keeps colons aligned
    // across the page.
    constexpr int label_x_pos = 12;
    constexpr int label_w     = 80;
    constexpr int value_x_pos = label_x_pos + label_w + 4;  // 96

    auto make_label = [&](std::unique_ptr<Label>& dst, const char* text, int y, bool right_align) {
        dst = std::make_unique<Label>(*_panel);
        dst->setText(text);
        dst->setTextFont(&lv_font_montserrat_14);
        dst->setTextColor(lv_color_hex(0x1B5E20));
        if (right_align) {
            dst->setWidth(label_w);
            lv_obj_set_style_text_align(dst->raw_ptr(), LV_TEXT_ALIGN_RIGHT, 0);
            dst->align(LV_ALIGN_TOP_LEFT, label_x_pos, y);
        } else {
            dst->align(LV_ALIGN_TOP_LEFT, value_x_pos, y);
        }
    };

    // Top block: Firmware / GOB Fork / Build (multi-line per column).
    std::string top_labels = "Firmware:\nGOB Fork:\nBuild:";
    std::string top_values = fmt::format(
        "V{}\n{}\n{}",
        FIRMWARE_VERSION,
        GOB_FORK_GIT_HASH,
        format_build_datetime()
    );
    make_label(_label_top, top_labels.c_str(), info_y_start, true);
    _value_top = std::make_unique<Label>(*_panel);
    _value_top->setText(top_values.c_str());
    _value_top->setTextFont(&lv_font_montserrat_14);
    _value_top->setTextColor(lv_color_hex(0x1B5E20));
    _value_top->align(LV_ALIGN_TOP_LEFT, value_x_pos, info_y_start);

    // Bottom social block: X / GitHub. Aligned below top block via align_to so
    // it adapts if top block height changes.
    make_label(_label_x,      "X:",          0, true);
    make_label(_value_x,      "@GOB_52_GOB", 0, false);
    make_label(_label_github, "GitHub:",     0, true);
    make_label(_value_github, "GOB52",       0, false);

    lv_obj_update_layout(_label_top->raw_ptr());
    lv_obj_align_to(_label_x->raw_ptr(),      _label_top->raw_ptr(), LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
    lv_obj_align_to(_value_x->raw_ptr(),      _value_top->raw_ptr(), LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
    lv_obj_align_to(_label_github->raw_ptr(), _label_x->raw_ptr(),   LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_align_to(_value_github->raw_ptr(), _value_x->raw_ptr(),   LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

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

    // Back below GitHub row (label column starts at x=12 — anchor on it).
    lv_obj_update_layout(_label_github->raw_ptr());
    lv_obj_align_to(_btn_back->raw_ptr(), _label_github->raw_ptr(),
                    LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
}

void AboutPage::update()
{
    if (_back_pending) {
        _back_pending = false;
        if (_on_back) _on_back();
    }
}

}  // namespace stackchan::gob_fork::view
