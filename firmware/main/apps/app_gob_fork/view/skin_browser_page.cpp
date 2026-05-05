// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "skin_browser_page.h"
#include "../skin_thumbnail.h"
#include <hal/board/sd_guard.h>
#include <ArduinoJson.hpp>
#include <esp_timer.h>
#include <mooncake_log.h>
#include <fmt/format.h>
#include <cstdio>

namespace stackchan::gob_fork::view {

using namespace stackchan::avatar::image;
using namespace uitk::lvgl_cpp;

namespace {
const char* _tag = "SkinBrowser";
constexpr int THUMB_SCALE_DIV = 4;  // 320x240 -> 80x60
constexpr int THUMB_W = 320 / THUMB_SCALE_DIV;
constexpr int THUMB_H = 240 / THUMB_SCALE_DIV;
}  // namespace

SkinBrowserPage::SkinBrowserPage(std::vector<SkinIndexEntry> skins,
                                 std::string nvs_current,
                                 OnApply on_apply,
                                 OnBack  on_back)
    : _skins(std::move(skins)),
      _nvs_current(std::move(nvs_current)),
      _on_apply(std::move(on_apply)),
      _on_back(std::move(on_back))
{
    // Start at the currently-selected skin if it exists in the list, else 0.
    for (size_t i = 0; i < _skins.size(); ++i) {
        if (_skins[i].id == _nvs_current) {
            _index = static_cast<int>(i);
            break;
        }
    }

    build_ui();
    hookup_buttons();
    show_loading(true);

    _pending = PendingAction::LoadCurrent;
}

SkinBrowserPage::~SkinBrowserPage()
{
    if (_thumb_image) {
        _thumb_image->setSrc(nullptr);
    }
    release_thumb();
    release_cfg();
}

void SkinBrowserPage::build_ui()
{
    _panel = std::make_unique<Container>(lv_screen_active());
    _panel->setSize(320, 240);
    _panel->setBgColor(lv_color_hex(0xE8F5E9));  // very light green
    _panel->setPadding(0, 0, 0, 0);
    _panel->setBorderWidth(0);
    _panel->setRadius(0);
    // Layout fits in one screen normally; scroll engages only when content
    // (e.g., very long wrapped author) exceeds the panel.
    _panel->setScrollDir(LV_DIR_VER);
    _panel->setScrollbarMode(LV_SCROLLBAR_MODE_AUTO);

    // Header (top): "Skin Browser" + index "x/N"
    _header_label = std::make_unique<Label>(*_panel);
    _header_label->setText("Skin Browser");
    _header_label->setTextFont(&lv_font_montserrat_14);
    _header_label->setTextColor(lv_color_hex(0x2E7D32));  // medium green
    _header_label->align(LV_ALIGN_TOP_LEFT, 12, 30);

    _index_label = std::make_unique<Label>(*_panel);
    _index_label->setText("0/0");
    _index_label->setTextFont(&lv_font_montserrat_14);
    _index_label->setTextColor(lv_color_hex(0x2E7D32));
    _index_label->align(LV_ALIGN_TOP_RIGHT, -12, 30);

    // Thumb box (left)
    _thumb_box = std::make_unique<Container>(*_panel);
    _thumb_box->setSize(THUMB_W + 4, THUMB_H + 4);
    _thumb_box->setBgColor(lv_color_hex(0x000000));
    _thumb_box->setBorderWidth(1);
    _thumb_box->setBorderColor(lv_color_hex(0x1B5E20));  // dark green
    _thumb_box->setRadius(4);
    _thumb_box->setPadding(2, 2, 2, 2);
    _thumb_box->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _thumb_box->align(LV_ALIGN_TOP_LEFT, 12, 48);

    _thumb_image = std::make_unique<Image>(*_thumb_box);
    _thumb_image->align(LV_ALIGN_CENTER, 0, 0);

    // Info column (right of thumb): individual labels (Q3=個別).
    const int info_x = 12 + THUMB_W + 4 + 12;   // x = 108
    const int info_w = 320 - info_x - 12;       // w = 200

    _name_label = std::make_unique<Label>(*_panel);
    _name_label->setText("");
    _name_label->setTextFont(&lv_font_montserrat_16);
    _name_label->setTextColor(lv_color_hex(0x1B5E20));  // dark green
    _name_label->align(LV_ALIGN_TOP_LEFT, info_x, 48);

    _version_label = std::make_unique<Label>(*_panel);
    _version_label->setText("");
    _version_label->setTextFont(&lv_font_montserrat_14);
    _version_label->setTextColor(lv_color_hex(0x2E7D32));
    _version_label->align(LV_ALIGN_TOP_LEFT, info_x, 70);

    _author_label = std::make_unique<Label>(*_panel);
    _author_label->setText("");
    _author_label->setTextFont(&lv_font_montserrat_14);
    _author_label->setTextColor(lv_color_hex(0x2E7D32));
    _author_label->setWidth(info_w);
    lv_label_set_long_mode(_author_label->raw_ptr(), LV_LABEL_LONG_MODE_WRAP);
    _author_label->align(LV_ALIGN_TOP_LEFT, info_x, 88);

    // Files + Badge (full-width row below thumb area).
    _files_label = std::make_unique<Label>(*_panel);
    _files_label->setText("");
    _files_label->setTextFont(&lv_font_montserrat_14);
    _files_label->setTextColor(lv_color_hex(0x2E7D32));
    _files_label->align(LV_ALIGN_TOP_LEFT, 12, 122);

    _badge_label = std::make_unique<Label>(*_panel);
    _badge_label->setText("[current]");
    _badge_label->setTextFont(&lv_font_montserrat_14);
    _badge_label->setTextColor(lv_color_hex(0x4CAF50));
    _badge_label->align(LV_ALIGN_TOP_RIGHT, -12, 122);
    _badge_label->addFlag(LV_OBJ_FLAG_HIDDEN);

    // Nav row (h=36): < / Apply / >
    auto make_nav_btn = [&](std::unique_ptr<Button>& btn, const char* text, int x, int w) {
        btn = std::make_unique<Button>(*_panel);
        btn->setSize(w, 36);
        btn->align(LV_ALIGN_TOP_LEFT, x, 144);
        btn->setBgColor(lv_color_hex(0xC8E6C9));  // light green
        btn->setBorderWidth(0);
        btn->setShadowWidth(0);
        btn->setRadius(12);
        btn->label().setText(text);
        btn->label().setTextFont(&lv_font_montserrat_16);
        btn->label().setTextColor(lv_color_hex(0x1B5E20));  // dark green
    };
    make_nav_btn(_btn_prev,  LV_SYMBOL_LEFT,  12,  60);
    make_nav_btn(_btn_apply, "Apply",         80, 160);
    make_nav_btn(_btn_next,  LV_SYMBOL_RIGHT, 248, 60);

    // Back row (h=36, full-width). Returns to GOB FORK main menu.
    _btn_back = std::make_unique<Button>(*_panel);
    _btn_back->setSize(296, 36);
    _btn_back->align(LV_ALIGN_TOP_LEFT, 12, 188);
    _btn_back->setBgColor(lv_color_hex(0x1B5E20));  // dark green
    _btn_back->setBorderWidth(0);
    _btn_back->setShadowWidth(0);
    _btn_back->setRadius(12);
    _btn_back->label().setText(LV_SYMBOL_LEFT " Back");
    _btn_back->label().setTextFont(&lv_font_montserrat_16);
    _btn_back->label().setTextColor(lv_color_hex(0xFFFFFF));

    // Loading overlay (initially hidden, toggled via show_loading).
    _loading_overlay = std::make_unique<Container>(*_panel);
    _loading_overlay->setSize(320, 240);
    _loading_overlay->align(LV_ALIGN_CENTER, 0, 0);
    _loading_overlay->setBgColor(lv_color_hex(0x000000));
    _loading_overlay->setBgOpa(LV_OPA_60);
    _loading_overlay->setBorderWidth(0);
    _loading_overlay->setRadius(0);
    _loading_overlay->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _loading_overlay->addFlag(LV_OBJ_FLAG_HIDDEN);

    _loading_label = std::make_unique<Label>(*_loading_overlay);
    _loading_label->setText("Loading...");
    _loading_label->setTextFont(&lv_font_montserrat_24);
    _loading_label->setTextColor(lv_color_hex(0xFFFFFF));
    _loading_label->align(LV_ALIGN_CENTER, 0, 0);
}

void SkinBrowserPage::hookup_buttons()
{
    _btn_prev->onClick().connect([this]()  { prev_skin(); });
    _btn_next->onClick().connect([this]()  { next_skin(); });
    _btn_apply->onClick().connect([this]() { confirm_apply(); });
    _btn_back->onClick().connect([this]()  { _pending = PendingAction::Back; });
}

void SkinBrowserPage::next_skin()
{
    if (_skins.empty() || _pending != PendingAction::None) return;
    _index = (_index + 1) % static_cast<int>(_skins.size());
    show_loading(true);
    _pending = PendingAction::LoadCurrent;
}

void SkinBrowserPage::prev_skin()
{
    if (_skins.empty() || _pending != PendingAction::None) return;
    _index = (_index - 1 + static_cast<int>(_skins.size())) % static_cast<int>(_skins.size());
    show_loading(true);
    _pending = PendingAction::LoadCurrent;
}

void SkinBrowserPage::update()
{
    // Defer dialog close (set by Cancel/OK callbacks) so we don't destroy
    // an lvgl button from inside its own click handler.
    if (_dialog_close_pending) {
        _dialog_close_pending = false;
        close_dialog();
    }

    switch (_pending) {
        case PendingAction::None:
            break;
        case PendingAction::LoadCurrent:
            perform_load_current();
            _pending = PendingAction::None;
            show_loading(false);
            break;
        case PendingAction::Apply:
            _pending = PendingAction::None;
            if (_on_apply && !_skins.empty()) {
                _on_apply(_skins[_index].id);
            }
            break;
        case PendingAction::Back:
            _pending = PendingAction::None;
            if (_on_back) _on_back();
            break;
    }
}

void SkinBrowserPage::perform_load_current()
{
    int64_t t0 = esp_timer_get_time();
    _load_failed = false;
    _load_err.clear();

    if (_skins.empty()) {
        _load_failed = true;
        _load_err    = "no skins";
        update_info_labels();
        return;
    }

    // SD load + thumb 生成の全期間で touchpad poll を抑止する。
    // SdGuard 単独では SD scope のみ skip だが、その後の thumb gen 中も
    // I2C bus が不安定で touch poll → I2C timeout → abort を起こすため。
    stackchan::hal::SdGuard::TouchSkipGuard touch_skip;

    if (_thumb_image) _thumb_image->setSrc(nullptr);
    release_thumb();
    release_cfg();

    const std::string& id = _skins[_index].id;
    std::string skin_dir  = "/sdcard/" + id;
    {
        stackchan::hal::SdGuard guard;
        // GOB fork: caller (AppGobFork::enter_skin_browser) で page lifecycle
        // 全期間 mount 済み前提。ここでは mount 状態のチェックのみ。
        if (!stackchan::hal::SdGuard::isMounted()) {
            _load_failed = true;
            _load_err    = "SD not mounted";
            mclog::tagError(_tag, "SD not mounted (caller should mount first)");
            update_info_labels();
            return;
        }
        if (!load_image_avatar_config(skin_dir, _cfg, _load_err)) {
            _load_failed = true;
            mclog::tagError(_tag, "load '{}' failed: {}", id, _load_err);
            update_info_labels();
            return;
        }
        FILE* f = fopen((skin_dir + "/manifest.json").c_str(), "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 64 * 1024) {
                std::string text(static_cast<size_t>(sz), '\0');
                fread(text.data(), 1, static_cast<size_t>(sz), f);
                ArduinoJson::JsonDocument doc;
                if (!ArduinoJson::deserializeJson(doc, text)) {
                    _name    = doc["name"].as<std::string>();
                    _version = doc["version"].as<std::string>();
                    _author  = doc["author"].as<std::string>();
                }
            }
            fclose(f);
        }
    }
    if (_name.empty()) _name = _skins[_index].name;

    _file_count  = (_cfg.base.valid()                ? 1 : 0) +
                   (_cfg.eye_left.open.valid()       ? 1 : 0) +
                   (_cfg.eye_left.closed.valid()     ? 1 : 0) +
                   (_cfg.eye_right.open.valid()      ? 1 : 0) +
                   (_cfg.eye_right.closed.valid()    ? 1 : 0) +
                   (_cfg.mouth.normal.valid()        ? 1 : 0) +
                   (_cfg.mouth.open.valid()          ? 1 : 0);
    _total_bytes = _cfg.base.size +
                   _cfg.eye_left.open.size + _cfg.eye_left.closed.size +
                   _cfg.eye_right.open.size + _cfg.eye_right.closed.size +
                   _cfg.mouth.normal.size + _cfg.mouth.open.size;
    _is_current  = (id == _nvs_current);

    std::string thumb_err;
    _thumb_buf = make_skin_thumbnail(_cfg, THUMB_SCALE_DIV, _panel->raw_ptr(), thumb_err);
    if (!_thumb_buf) {
        _load_failed = true;
        _load_err    = "thumb: " + thumb_err;
        mclog::tagError(_tag, "thumb gen failed: {}", thumb_err);
    } else {
        _thumb_image->setSrc(_thumb_buf);
    }

    update_info_labels();

    int64_t elapsed_ms = (esp_timer_get_time() - t0) / 1000;
    mclog::tagInfo(_tag, "loaded '{}' ({} ms)", id, static_cast<int>(elapsed_ms));
}

void SkinBrowserPage::update_info_labels()
{
    if (_index_label) {
        if (_skins.empty()) {
            _index_label->setText("0/0");
        } else {
            _index_label->setText(fmt::format("{}/{}", _index + 1, _skins.size()).c_str());
        }
    }

    if (_name_label) {
        _name_label->setText(_load_failed ? "LOAD ERROR" : _name.c_str());
    }

    if (_load_failed) {
        if (_version_label) _version_label->setText(_load_err.c_str());
        if (_author_label)  _author_label->setText("");
        if (_files_label)   _files_label->setText("");
    } else {
        if (_version_label) {
            _version_label->setText(
                _version.empty() ? "v?" : ("v" + _version).c_str());
        }
        if (_author_label) {
            _author_label->setText(
                _author.empty() ? "" : ("by " + _author).c_str());
        }
        if (_files_label) {
            _files_label->setText(
                fmt::format("{} files (~{} KB)",
                            _file_count, (_total_bytes + 1023) / 1024).c_str());
        }
    }

    if (_badge_label) {
        if (_is_current && !_load_failed) {
            _badge_label->removeFlag(LV_OBJ_FLAG_HIDDEN);
        } else {
            _badge_label->addFlag(LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void SkinBrowserPage::show_loading(bool loading)
{
    if (_loading_overlay) {
        if (loading) {
            _loading_overlay->removeFlag(LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(_loading_overlay->raw_ptr());
        } else {
            _loading_overlay->addFlag(LV_OBJ_FLAG_HIDDEN);
        }
    }
    auto set_disabled = [&](Button* b, bool dis) {
        if (!b) return;
        if (dis) lv_obj_add_state(b->raw_ptr(), LV_STATE_DISABLED);
        else     lv_obj_remove_state(b->raw_ptr(), LV_STATE_DISABLED);
    };
    set_disabled(_btn_prev.get(),  loading);
    set_disabled(_btn_apply.get(), loading);
    set_disabled(_btn_next.get(),  loading);
    set_disabled(_btn_back.get(),  loading);
}

// -- Confirm dialog (custom modal; LV_USE_MSGBOX disabled in this build) ----

void SkinBrowserPage::confirm_apply()
{
    if (_skins.empty() || _load_failed) return;
    show_confirm_dialog("Apply Skin?",
                        fmt::format("Switch to: {}", _name),
                        PendingAction::Apply);
}

void SkinBrowserPage::show_confirm_dialog(const char* title,
                                          const std::string& body,
                                          PendingAction on_yes)
{
    close_dialog();

    _dialog_overlay = std::make_unique<Container>(*_panel);
    _dialog_overlay->setSize(320, 240);
    _dialog_overlay->align(LV_ALIGN_CENTER, 0, 0);
    _dialog_overlay->setBgColor(lv_color_hex(0x000000));
    _dialog_overlay->setBgOpa(LV_OPA_60);
    _dialog_overlay->setBorderWidth(0);
    _dialog_overlay->setRadius(0);
    _dialog_overlay->setPadding(0, 0, 0, 0);
    _dialog_overlay->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(_dialog_overlay->raw_ptr());

    _dialog_panel = std::make_unique<Container>(*_dialog_overlay);
    _dialog_panel->setSize(280, 140);
    _dialog_panel->align(LV_ALIGN_CENTER, 0, 0);
    _dialog_panel->setBgColor(lv_color_hex(0xFFFFFF));
    _dialog_panel->setBorderWidth(1);
    _dialog_panel->setBorderColor(lv_color_hex(0x6A6882));
    _dialog_panel->setRadius(8);
    _dialog_panel->setPadding(12, 12, 12, 12);
    _dialog_panel->removeFlag(LV_OBJ_FLAG_SCROLLABLE);

    _dialog_title = std::make_unique<Label>(*_dialog_panel);
    _dialog_title->setText(title);
    _dialog_title->setTextFont(&lv_font_montserrat_16);
    _dialog_title->setTextColor(lv_color_hex(0x26206A));
    _dialog_title->align(LV_ALIGN_TOP_LEFT, 0, 0);

    _dialog_body = std::make_unique<Label>(*_dialog_panel);
    _dialog_body->setText(body.c_str());
    _dialog_body->setTextFont(&lv_font_montserrat_14);
    _dialog_body->setTextColor(lv_color_hex(0x6A6882));
    _dialog_body->setWidth(256);
    _dialog_body->align(LV_ALIGN_TOP_LEFT, 0, 28);
    lv_label_set_long_mode(_dialog_body->raw_ptr(), LV_LABEL_LONG_MODE_WRAP);

    _dialog_cancel = std::make_unique<Button>(*_dialog_panel);
    _dialog_cancel->setSize(110, 32);
    _dialog_cancel->align(LV_ALIGN_BOTTOM_LEFT, 0, 0);
    _dialog_cancel->setBgColor(lv_color_hex(0xCCCCCC));
    _dialog_cancel->setBorderWidth(0);
    _dialog_cancel->setShadowWidth(0);
    _dialog_cancel->setRadius(8);
    _dialog_cancel->label().setText("Cancel");
    _dialog_cancel->label().setTextFont(&lv_font_montserrat_14);
    _dialog_cancel->label().setTextColor(lv_color_hex(0x000000));
    _dialog_cancel->onClick().connect([this]() { _dialog_close_pending = true; });

    _dialog_ok = std::make_unique<Button>(*_dialog_panel);
    _dialog_ok->setSize(110, 32);
    _dialog_ok->align(LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    _dialog_ok->setBgColor(lv_color_hex(0x4CAF50));
    _dialog_ok->setBorderWidth(0);
    _dialog_ok->setShadowWidth(0);
    _dialog_ok->setRadius(8);
    _dialog_ok->label().setText("OK");
    _dialog_ok->label().setTextFont(&lv_font_montserrat_14);
    _dialog_ok->label().setTextColor(lv_color_hex(0xFFFFFF));
    _dialog_ok->onClick().connect([this, on_yes]() {
        _pending              = on_yes;
        _dialog_close_pending = true;
    });
}

void SkinBrowserPage::close_dialog()
{
    _dialog_ok.reset();
    _dialog_cancel.reset();
    _dialog_body.reset();
    _dialog_title.reset();
    _dialog_panel.reset();
    _dialog_overlay.reset();
}

void SkinBrowserPage::release_thumb()
{
    if (_thumb_buf) {
        lv_draw_buf_destroy(_thumb_buf);
        _thumb_buf = nullptr;
    }
}

void SkinBrowserPage::release_cfg()
{
    _cfg = ImageAvatarConfig{};
    _name.clear();
    _version.clear();
    _author.clear();
    _file_count  = 0;
    _total_bytes = 0;
    _is_current  = false;
}

}  // namespace stackchan::gob_fork::view
