// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "app_gob_fork.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>
#include <stackchan/stackchan.h>
#include <stackchan/avatar/skins/image/skin_loader.h>
#include <stackchan/gob_fork_nvs.h>
#include <hal/board/sd_guard.h>
#include <apps/common/common.h>

using namespace mooncake;
using namespace view;
using namespace stackchan::avatar::image;
using namespace uitk::lvgl_cpp;

AppGobFork::AppGobFork()
{
    setAppInfo().name = "GOB FORK";
    static auto icon  = assets::get_image("icon_gob_fork.bin");
    setAppInfo().icon = (void*)&icon;
    static uint32_t theme_color = 0x4CAF50;
    setAppInfo().userData       = (void*)&theme_color;
}

void AppGobFork::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppGobFork::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    _current_page             = Page::Main;
    _switch_pending           = false;
    _menu_action_pending      = PendingMenuAction::None;
    _dialog_close_pending     = false;

    LvglLockGuard lock;
    build_main_menu();
    // Green theme: secondary=light green, primary=dark green
    view::create_home_indicator([&]() { close(); }, 0xC8E6C9, 0x1B5E20);
    view::create_status_bar(0xC8E6C9, 0x1B5E20);
}

void AppGobFork::onRunning()
{
    LvglLockGuard lock;

    // Defer dialog close to next tick (avoid destroying button from inside its own cb).
    if (_dialog_close_pending) {
        _dialog_close_pending = false;
        close_dialog();
    }

    // Process queued menu action (Reset / UseDefault / Restart).
    if (_menu_action_pending != PendingMenuAction::None) {
        auto action = _menu_action_pending;
        _menu_action_pending = PendingMenuAction::None;
        perform_menu_action(action);
    }

    if (_menu_page)                 _menu_page->update();
    if (_skin_browser)              _skin_browser->update();
    if (_system_info_page)          _system_info_page->update();
    if (_sd_card_info_page)         _sd_card_info_page->update();
    if (_about_page)                _about_page->update();
    if (_screensaver_settings_page) _screensaver_settings_page->update();

    if (_switch_pending) {
        _switch_pending = false;
        _menu_page.reset();
        _skin_browser.reset();
        _system_info_page.reset();
        _sd_card_info_page.reset();
        _about_page.reset();
        _screensaver_settings_page.reset();
        _menu_sections.clear();
        _current_page = _pending_page;
        switch (_current_page) {
            case Page::Main:                build_main_menu();             break;
            case Page::SkinBrowser:         enter_skin_browser();          break;
            case Page::SystemInfo:          enter_system_info();           break;
            case Page::SdCardInfo:          enter_sd_card_info();          break;
            case Page::About:               enter_about();                 break;
            case Page::ScreensaverSettings: enter_screensaver_settings();  break;
        }
    }

    GetStackChan().update();

    view::update_home_indicator();
    view::update_status_bar();
}

void AppGobFork::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    // Note: AVATAR is NOT auto re-opened here. After close, the Launcher
    // resumes (re-builds LauncherView) and the user taps the AVATAR icon to
    // load the new skin. Auto-opening AVATAR from onClose would bypass the
    // Launcher's single-app state machine and leave LauncherView visible
    // overlapping the avatar.
    {
        LvglLockGuard lock;
        close_dialog();
        _menu_sections.clear();
        _menu_page.reset();
        _skin_browser.reset();
        _system_info_page.reset();
        _sd_card_info_page.reset();
        _about_page.reset();
        _screensaver_settings_page.reset();
        view::destroy_home_indicator();
        view::destroy_status_bar();
    }
}

void AppGobFork::build_main_menu()
{
    _menu_sections = {
        {
            "Avatar",
            {
                {"Skin Browser",
                 [&]() {
                     _switch_pending = true;
                     _pending_page   = Page::SkinBrowser;
                 }},
                {"Reset to Default Skin",
                 [&]() {
                     show_confirm_dialog("Reset to Default Skin?",
                                         "Clear NVS and use avatar.json's default skin.",
                                         PendingMenuAction::ResetNvs);
                 }},
                {"Use Default Avatar",
                 [&]() {
                     show_confirm_dialog("Use Default Avatar?",
                                         "Switch to built-in geometric avatar (no SD).",
                                         PendingMenuAction::UseDefaultAvatar);
                 }},
            },
        },
        {
            "Display",
            {
                {"Screensaver",
                 [&]() {
                     _switch_pending = true;
                     _pending_page   = Page::ScreensaverSettings;
                 }},
            },
        },
        {
            "System",
            {
                {"System Info",
                 [&]() {
                     _switch_pending = true;
                     _pending_page   = Page::SystemInfo;
                 }},
                {"SD Card Info",
                 [&]() {
                     _switch_pending = true;
                     _pending_page   = Page::SdCardInfo;
                 }},
                {"Restart Device",
                 [&]() {
                     show_confirm_dialog("Restart Device?",
                                         "The device will reboot to the launcher.",
                                         PendingMenuAction::RestartDevice);
                 }},
            },
        },
        {
            "About",
            {
                {"About",
                 [&]() {
                     _switch_pending = true;
                     _pending_page   = Page::About;
                 }},
            },
        },
    };
    constexpr view::SelectMenuPage::Colors green_colors{
        0xE8F5E9,  // panel_bg     : very light green
        0x2E7D32,  // section_text : medium green
        0xC8E6C9,  // btn_bg       : light green
        0x1B5E20,  // btn_text     : dark green
    };
    _menu_page = std::make_unique<view::SelectMenuPage>(_menu_sections, green_colors);
}

void AppGobFork::enter_skin_browser()
{
    AvatarIndex idx;
    std::string err;
    bool ok = false;
    {
        stackchan::hal::SdGuard guard;
        if (guard.ensureMounted()) {
            ok = load_avatar_index(idx, err);
        } else {
            err = "SD mount failed";
        }
    }

    if (!ok || idx.skins.empty()) {
        mclog::tagWarn(getAppInfo().name, "skin index load failed: {}", err);
        view::pop_a_toast(err.empty() ? "No SD / avatar.json" : err,
                          view::ToastType::Error, 2400);
        _switch_pending = true;
        _pending_page   = Page::Main;
        return;
    }

    std::string nvs_current = stackchan::gob_fork::get_skin_current();

    auto on_apply = [this](const std::string& id) {
        if (!stackchan::gob_fork::set_skin_current(id)) {
            view::pop_a_toast("NVS save failed", view::ToastType::Error, 2000);
            return;
        }
        mclog::tagInfo(getAppInfo().name, "skin saved: {}", id);
        close();
    };
    auto on_back = [this]() {
        _switch_pending = true;
        _pending_page   = Page::Main;
    };

    _skin_browser = std::make_unique<stackchan::gob_fork::view::SkinBrowserPage>(
        std::move(idx.skins), nvs_current,
        std::move(on_apply), std::move(on_back));
}

void AppGobFork::enter_system_info()
{
    auto on_back = [this]() {
        _switch_pending = true;
        _pending_page   = Page::Main;
    };
    _system_info_page = std::make_unique<stackchan::gob_fork::view::SystemInfoPage>(std::move(on_back));
}

void AppGobFork::enter_sd_card_info()
{
    auto on_back = [this]() {
        _switch_pending = true;
        _pending_page   = Page::Main;
    };
    _sd_card_info_page = std::make_unique<stackchan::gob_fork::view::SdCardInfoPage>(std::move(on_back));
}

void AppGobFork::enter_about()
{
    auto on_back = [this]() {
        _switch_pending = true;
        _pending_page   = Page::Main;
    };
    _about_page = std::make_unique<stackchan::gob_fork::view::AboutPage>(std::move(on_back));
}

void AppGobFork::enter_screensaver_settings()
{
    auto on_back = [this]() {
        _switch_pending = true;
        _pending_page   = Page::Main;
    };
    _screensaver_settings_page =
        std::make_unique<stackchan::gob_fork::view::ScreensaverSettingsPage>(std::move(on_back));
}

void AppGobFork::perform_menu_action(PendingMenuAction action)
{
    switch (action) {
        case PendingMenuAction::ResetNvs:
            if (!stackchan::gob_fork::clear_skin_current()) {
                view::pop_a_toast("NVS clear failed", view::ToastType::Error, 2000);
                return;
            }
            mclog::tagInfo(getAppInfo().name, "skin NVS cleared");
            close();
            break;
        case PendingMenuAction::UseDefaultAvatar:
            if (!stackchan::gob_fork::set_skin_current(stackchan::gob_fork::FORCE_DEFAULT_AVATAR_ID)) {
                view::pop_a_toast("NVS save failed", view::ToastType::Error, 2000);
                return;
            }
            mclog::tagInfo(getAppInfo().name, "use DefaultAvatar");
            close();
            break;
        case PendingMenuAction::RestartDevice:
            mclog::tagInfo(getAppInfo().name, "restart device");
            GetHAL().reboot();
            break;
        case PendingMenuAction::None:
            break;
    }
}

// -- Confirm dialog (shared between menu actions) ---------------------------

void AppGobFork::show_confirm_dialog(const char* title,
                                     const std::string& body,
                                     PendingMenuAction on_yes)
{
    close_dialog();

    _dialog_overlay = std::make_unique<Container>(lv_screen_active());
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
        _menu_action_pending  = on_yes;
        _dialog_close_pending = true;
    });
}

void AppGobFork::close_dialog()
{
    _dialog_ok.reset();
    _dialog_cancel.reset();
    _dialog_body.reset();
    _dialog_title.reset();
    _dialog_panel.reset();
    _dialog_overlay.reset();
}
