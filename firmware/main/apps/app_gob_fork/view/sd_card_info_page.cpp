// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "sd_card_info_page.h"
#include <hal/board/sd_guard.h>
#include <stackchan/avatar/skins/image/skin_loader.h>
#include <fmt/format.h>
#include <mooncake_log.h>
#include <esp_vfs_fat.h>
#include <sys/stat.h>
#include <dirent.h>

namespace stackchan::gob_fork::view {

using namespace uitk::lvgl_cpp;
using namespace stackchan::avatar::image;

namespace {

const char* _tag = "SdCardInfoPage";

// Sum sizes of regular files inside `dir`. Returns total bytes; sets file_count.
size_t sum_dir_bytes(const std::string& dir, int& file_count)
{
    DIR* d = opendir(dir.c_str());
    if (!d) return 0;
    size_t total = 0;
    file_count   = 0;
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_name[0] == '.') continue;  // skip ./.. and dotfiles
        std::string path = dir + "/" + ent->d_name;
        struct stat st {};
        if (stat(path.c_str(), &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        total += static_cast<size_t>(st.st_size);
        ++file_count;
    }
    closedir(d);
    return total;
}

}  // namespace

SdCardInfoPage::SdCardInfoPage(OnBack on_back) : _on_back(std::move(on_back))
{
    build_ui();
}

SdCardInfoPage::~SdCardInfoPage() = default;

SdCardInfoPage::InfoSnapshot SdCardInfoPage::collect_info() const
{
    InfoSnapshot snap;

    if (!stackchan::hal::SdGuard::isInserted()) {
        snap.error_message = "No SD card inserted.";
        return snap;
    }

    // TouchSkipGuard covers the entire heavy I/O window to avoid I2C contention.
    stackchan::hal::SdGuard::TouchSkipGuard touch_skip;

    AvatarIndex index;
    std::string err;
    bool        index_ok = false;

    struct SkinSize {
        std::string id;
        size_t      bytes;
        int         files;
    };
    std::vector<SkinSize> skin_sizes;

    {
        stackchan::hal::SdGuard guard;
        stackchan::hal::SdGuard::MountGuard mg;
        if (!mg.ok()) {
            snap.error_message = "SD mount failed.";
            return snap;
        }

        snap.present = true;
        if (esp_vfs_fat_info("/sdcard", &snap.total_b, &snap.free_b) == ESP_OK) {
            snap.stat_ok = true;
        } else {
            mclog::tagWarn(_tag, "esp_vfs_fat_info failed");
        }

        index_ok = load_avatar_index(index, err);
        if (index_ok) {
            for (auto& e : index.skins) {
                int files = 0;
                size_t bytes = sum_dir_bytes("/sdcard/" + e.id, files);
                skin_sizes.push_back({e.id, bytes, files});
            }
        }
    }

    // Build skin list section.
    std::string& list = snap.skin_list_text;
    list += "Avatar skins:\n";
    if (!index_ok) {
        list += fmt::format("  (avatar.json error: {})", err);
    } else if (skin_sizes.empty()) {
        list += "  (none)";
    } else {
        size_t total_skin_b = 0;
        for (auto& s : skin_sizes) total_skin_b += s.bytes;
        list += fmt::format("  {} skin(s), ~{} KB total\n",
                            skin_sizes.size(), (total_skin_b + 1023) / 1024);
        for (auto& s : skin_sizes) {
            list += fmt::format("  - {:<12} {} files, {} KB\n",
                                s.id, s.files, (s.bytes + 1023) / 1024);
        }
    }

    return snap;
}

void SdCardInfoPage::build_ui()
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
    _header_label->setText("SD Card Info");
    _header_label->setTextFont(&lv_font_montserrat_16);
    _header_label->setTextColor(lv_color_hex(0x1B5E20));
    _header_label->align(LV_ALIGN_TOP_LEFT, 12, 30);

    InfoSnapshot snap = collect_info();

    constexpr int label_w = 64;  // "Mount:" / "Total:" / "Free:"
    lv_obj_t* anchor = nullptr;  // for back placement

    if (!snap.present) {
        // No card / mount failure: show single-line message, no columns.
        _skin_list_label = std::make_unique<Label>(*_panel);
        _skin_list_label->setText(snap.error_message.c_str());
        _skin_list_label->setTextFont(&lv_font_montserrat_14);
        _skin_list_label->setTextColor(lv_color_hex(0x1B5E20));
        _skin_list_label->setWidth(296);
        lv_label_set_long_mode(_skin_list_label->raw_ptr(), LV_LABEL_LONG_MODE_WRAP);
        _skin_list_label->align(LV_ALIGN_TOP_LEFT, 12, 56);
        anchor = _skin_list_label->raw_ptr();
    } else {
        // 2-column for Mount/Total/Free.
        const std::string labels = "Mount:\nTotal:\nFree:";
        std::string values;
        if (snap.stat_ok) {
            double total_mb = snap.total_b / (1024.0 * 1024.0);
            double free_mb  = snap.free_b  / (1024.0 * 1024.0);
            values = fmt::format(
                "/sdcard\n"
                "{:.1f} MB\n"
                "{:.1f} MB ({:.0f}%)",
                total_mb,
                free_mb,
                snap.total_b ? (100.0 * snap.free_b / snap.total_b) : 0.0
            );
        } else {
            values = "/sdcard\n?\n?";
        }

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

        // Skin list block below the 2-column block.
        _skin_list_label = std::make_unique<Label>(*_panel);
        _skin_list_label->setText(snap.skin_list_text.c_str());
        _skin_list_label->setTextFont(&lv_font_montserrat_14);
        _skin_list_label->setTextColor(lv_color_hex(0x1B5E20));
        _skin_list_label->setWidth(296);
        lv_label_set_long_mode(_skin_list_label->raw_ptr(), LV_LABEL_LONG_MODE_WRAP);
        lv_obj_update_layout(_label_col->raw_ptr());
        lv_obj_align_to(_skin_list_label->raw_ptr(), _label_col->raw_ptr(),
                        LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
        anchor = _skin_list_label->raw_ptr();
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

    // Place back below the bottom-most content (variable: skin list length).
    lv_obj_update_layout(anchor);
    lv_obj_align_to(_btn_back->raw_ptr(), anchor,
                    LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);
}

void SdCardInfoPage::update()
{
    if (_back_pending) {
        _back_pending = false;
        if (_on_back) _on_back();
    }
}

}  // namespace stackchan::gob_fork::view
