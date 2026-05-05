// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <functional>
#include <memory>

namespace stackchan::gob_fork::view {

// SD card info: mount status, capacity / free, skin folder list with sizes.
// Snapshot taken at construction (no realtime refresh).
class SdCardInfoPage {
public:
    using OnBack = std::function<void()>;

    explicit SdCardInfoPage(OnBack on_back);
    ~SdCardInfoPage();

    SdCardInfoPage(const SdCardInfoPage&)            = delete;
    SdCardInfoPage& operator=(const SdCardInfoPage&) = delete;

    void update();

private:
    struct InfoSnapshot {
        bool        present     = false;  // SD inserted + mounted
        bool        stat_ok     = false;
        uint64_t    total_b     = 0;
        uint64_t    free_b      = 0;
        std::string skin_list_text;       // pre-formatted ("(none)" / multi-line list / error)
        std::string error_message;        // set when present=false (e.g. "No SD card")
    };

    void build_ui();
    InfoSnapshot collect_info() const;

    OnBack _on_back;
    bool   _back_pending = false;

    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _header_label;
    // 2-column for Mount/Total/Free (right-aligned label / left-aligned value).
    std::unique_ptr<uitk::lvgl_cpp::Label>     _label_col;
    std::unique_ptr<uitk::lvgl_cpp::Label>     _value_col;
    // Bottom block: skin list ("No SD" case reuses this for the single message).
    std::unique_ptr<uitk::lvgl_cpp::Label>     _skin_list_label;
    std::unique_ptr<uitk::lvgl_cpp::Button>    _btn_back;
};

}  // namespace stackchan::gob_fork::view
