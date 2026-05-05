// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "system_info_page.h"
#include <hal/hal.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_idf_version.h>
#include <esp_clk_tree.h>
#include <fmt/format.h>

namespace stackchan::gob_fork::view {

using namespace uitk::lvgl_cpp;

namespace {

const char* reset_reason_str(esp_reset_reason_t r)
{
    switch (r) {
        case ESP_RST_POWERON:    return "Power on";
        case ESP_RST_EXT:        return "External pin";
        case ESP_RST_SW:         return "Software";
        case ESP_RST_PANIC:      return "Panic";
        case ESP_RST_INT_WDT:    return "Int WDT";
        case ESP_RST_TASK_WDT:   return "Task WDT";
        case ESP_RST_WDT:        return "Other WDT";
        case ESP_RST_DEEPSLEEP:  return "Deep sleep wake";
        case ESP_RST_BROWNOUT:   return "Brownout";
        case ESP_RST_SDIO:       return "SDIO";
        case ESP_RST_USB:        return "USB peripheral";
        case ESP_RST_JTAG:       return "JTAG";
        case ESP_RST_EFUSE:      return "Efuse error";
        case ESP_RST_PWR_GLITCH: return "Power glitch";
        case ESP_RST_CPU_LOCKUP: return "CPU lockup";
        case ESP_RST_UNKNOWN:
        default:                 return "Unknown";
    }
}

std::string format_uptime(uint32_t millis)
{
    uint32_t s = millis / 1000;
    uint32_t h = s / 3600;
    uint32_t m = (s % 3600) / 60;
    uint32_t sec = s % 60;
    return fmt::format("{:02}:{:02}:{:02}", h, m, sec);
}

std::string format_kb_pair(size_t free_b, size_t total_b)
{
    return fmt::format("{} / {} KB", free_b / 1024, total_b / 1024);
}

}  // namespace

SystemInfoPage::SystemInfoPage(OnBack on_back) : _on_back(std::move(on_back))
{
    build_ui();
}

SystemInfoPage::~SystemInfoPage() = default;

void SystemInfoPage::build_ui()
{
    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setBgColor(lv_color_hex(0xE8F5E9));  // very light green
    _panel->setPadding(0, 0, 0, 0);
    _panel->setBorderWidth(0);
    _panel->setRadius(0);
    _panel->setScrollDir(LV_DIR_VER);
    _panel->setScrollbarMode(LV_SCROLLBAR_MODE_AUTO);

    _header_label = std::make_unique<Label>(*_panel);
    _header_label->setText("System Info");
    _header_label->setTextFont(&lv_font_montserrat_16);
    _header_label->setTextColor(lv_color_hex(0x1B5E20));  // dark green
    _header_label->align(LV_ALIGN_TOP_LEFT, 12, 30);

    // Snapshot system metrics.
    size_t heap_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t heap_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    uint32_t cpu_hz = 0;
    esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_APPROX, &cpu_hz);

    esp_chip_info_t chip = {};
    esp_chip_info(&chip);

    std::string labels =
        "Heap free:\n"
        "PSRAM free:\n"
        "Battery:\n"
        "Uptime:\n"
        "CPU freq:\n"
        "Chip:\n"
        "Reset:\n"
        "IDF:";
    std::string values = fmt::format(
        "{}\n"
        "{:.1f} / {:.1f} MB\n"
        "{}% ({})\n"
        "{}\n"
        "{} MHz\n"
        "ESP32-S{} rev{} ({}c)\n"
        "{}\n"
        "{}",
        format_kb_pair(heap_free, heap_total),
        psram_free / (1024.0 * 1024.0),
        psram_total / (1024.0 * 1024.0),
        GetHAL().getBatteryLevel(),
        GetHAL().isBatteryCharging() ? "CHG" : "BAT",
        format_uptime(GetHAL().millis()),
        cpu_hz / 1000000,
        static_cast<int>(chip.model),
        chip.revision,
        chip.cores,
        reset_reason_str(esp_reset_reason()),
        esp_get_idf_version()
    );

    constexpr int label_w = 96;  // "PSRAM free:" fits within
    _label_col = std::make_unique<Label>(*_panel);
    _label_col->setText(labels.c_str());
    _label_col->setTextFont(&lv_font_montserrat_14);
    _label_col->setTextColor(lv_color_hex(0x1B5E20));
    _label_col->setWidth(label_w);
    lv_obj_set_style_text_align(_label_col->raw_ptr(), LV_TEXT_ALIGN_RIGHT, 0);
    _label_col->align(LV_ALIGN_TOP_LEFT, 12, 56);

    _value_col = std::make_unique<Label>(*_panel);
    _value_col->setText(values.c_str());
    _value_col->setTextFont(&lv_font_montserrat_14);
    _value_col->setTextColor(lv_color_hex(0x1B5E20));
    _value_col->align(LV_ALIGN_TOP_LEFT, 12 + label_w + 4, 56);

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

    // Place back below the label column (both columns have same line count).
    lv_obj_update_layout(_label_col->raw_ptr());
    lv_obj_align_to(_btn_back->raw_ptr(), _label_col->raw_ptr(),
                    LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
}

void SystemInfoPage::update()
{
    if (_back_pending) {
        _back_pending = false;
        if (_on_back) _on_back();
    }
}

}  // namespace stackchan::gob_fork::view
