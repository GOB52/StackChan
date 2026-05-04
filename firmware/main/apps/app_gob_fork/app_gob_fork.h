// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include "../app_setup/view/view.h"
#include "view/skin_browser_page.h"
#include "view/system_info_page.h"
#include "view/sd_card_info_page.h"
#include "view/about_page.h"
#include "view/screensaver_settings_page.h"
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <mooncake.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class AppGobFork : public mooncake::AppAbility {
public:
    AppGobFork();

    void onCreate() override;
    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class Page {
        Main,
        SkinBrowser,
        SystemInfo,
        SdCardInfo,
        About,
        ScreensaverSettings,
    };

    enum class PendingMenuAction {
        None,
        ResetNvs,         // clear NVS → AVATAR uses avatar.json's current
        UseDefaultAvatar, // NVS = "__default__" → AVATAR uses DefaultAvatar
        RestartDevice,    // GetHAL().reboot() (cold restart, returns to Launcher)
    };

    Page _current_page    = Page::Main;
    bool _switch_pending  = false;
    Page _pending_page    = Page::Main;

    std::vector<view::SelectMenuPage::MenuSection> _menu_sections;
    std::unique_ptr<view::SelectMenuPage>          _menu_page;

    std::unique_ptr<stackchan::gob_fork::view::SkinBrowserPage>           _skin_browser;
    std::unique_ptr<stackchan::gob_fork::view::SystemInfoPage>            _system_info_page;
    std::unique_ptr<stackchan::gob_fork::view::SdCardInfoPage>            _sd_card_info_page;
    std::unique_ptr<stackchan::gob_fork::view::AboutPage>                 _about_page;
    std::unique_ptr<stackchan::gob_fork::view::ScreensaverSettingsPage>   _screensaver_settings_page;

    void build_main_menu();
    void enter_skin_browser();
    void enter_system_info();
    void enter_sd_card_info();
    void enter_about();
    void enter_screensaver_settings();

    // Confirm dialog (custom modal; LV_USE_MSGBOX disabled).
    std::unique_ptr<uitk::lvgl_cpp::Container> _dialog_overlay;
    std::unique_ptr<uitk::lvgl_cpp::Container> _dialog_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _dialog_title;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _dialog_body;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _dialog_ok;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _dialog_cancel;

    PendingMenuAction _menu_action_pending  = PendingMenuAction::None;
    bool              _dialog_close_pending = false;

    void show_confirm_dialog(const char* title, const std::string& body, PendingMenuAction on_yes);
    void close_dialog();
    void perform_menu_action(PendingMenuAction action);
};
