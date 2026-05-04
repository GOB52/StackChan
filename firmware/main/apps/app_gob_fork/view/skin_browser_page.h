// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <stackchan/avatar/skins/image/skin_loader.h>
#include <stackchan/avatar/skins/image/image_assets.h>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace stackchan::gob_fork::view {

// Browse skins (prev/next), display dynamic thumbnail + manifest info, and
// offer Apply (with confirm) / Back actions. Reset/UseDefault have been moved
// to the parent app's main menu.
class SkinBrowserPage {
public:
    using OnApply = std::function<void(const std::string& skin_id)>;
    using OnBack  = std::function<void()>;

    SkinBrowserPage(std::vector<stackchan::avatar::image::SkinIndexEntry> skins,
                    std::string nvs_current,
                    OnApply on_apply,
                    OnBack  on_back);
    ~SkinBrowserPage();

    SkinBrowserPage(const SkinBrowserPage&)            = delete;
    SkinBrowserPage& operator=(const SkinBrowserPage&) = delete;

    // Called from the host App's onRunning() (already inside LvglLockGuard).
    void update();

private:
    enum class PendingAction {
        None,
        LoadCurrent,   // SD load + thumb generation for _index
        Apply,
        Back,
    };

    void build_ui();
    void hookup_buttons();

    void next_skin();
    void prev_skin();

    // Heavy work: SD mount → load skin cfg → make thumbnail → update UI.
    void perform_load_current();

    void update_info_labels();
    void show_loading(bool loading);

    void confirm_apply();

    void release_thumb();
    void release_cfg();

    std::vector<stackchan::avatar::image::SkinIndexEntry> _skins;
    int         _index = 0;
    std::string _nvs_current;

    OnApply _on_apply;
    OnBack  _on_back;

    PendingAction _pending = PendingAction::None;

    stackchan::avatar::image::ImageAvatarConfig _cfg;
    lv_draw_buf_t* _thumb_buf = nullptr;

    std::string _name;
    std::string _version;
    std::string _author;
    int         _file_count  = 0;
    size_t      _total_bytes = 0;
    bool        _is_current  = false;
    bool        _load_failed = false;
    std::string _load_err;

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _header_label;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _index_label;

    std::unique_ptr<uitk::lvgl_cpp::Container> _thumb_box;   // border around thumb
    std::unique_ptr<uitk::lvgl_cpp::Image>     _thumb_image;

    // Per-field labels (Q3=個別 ─ split, with WRAP on author for long manifest text).
    std::unique_ptr<uitk::lvgl_cpp::Label>     _name_label;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _version_label;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _author_label;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _files_label;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _badge_label;

    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_prev;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_apply;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_next;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_back;

    std::unique_ptr<uitk::lvgl_cpp::Container> _loading_overlay;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _loading_label;

    // Custom confirm dialog (LV_USE_MSGBOX is disabled in this build).
    std::unique_ptr<uitk::lvgl_cpp::Container> _dialog_overlay;
    std::unique_ptr<uitk::lvgl_cpp::Container> _dialog_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _dialog_title;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _dialog_body;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _dialog_ok;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _dialog_cancel;

    void show_confirm_dialog(const char* title, const std::string& body, PendingAction on_yes);
    void close_dialog();

    // Deferred close to avoid destroying lvgl objects from inside their own
    // event callback (causes use-after-free / reboot).
    bool _dialog_close_pending = false;
};

}  // namespace stackchan::gob_fork::view
