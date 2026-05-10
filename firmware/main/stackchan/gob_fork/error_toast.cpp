// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "error_toast.h"
#include "utf8_helper.h"

#include <stackchan/gob_fork_nvs.h>
#include <apps/common/toast/toast.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace stackchan::gob_fork::error_toast {

namespace {

constexpr const char* SELF_TAG = "GobErrorToast";

// 監視対象 TAG (whitelist)。順序は throttle_state_ の index と対応。
constexpr const char* TOAST_TAGS[] = {
    "MCP",
    "MQTT",
    "Application",
    "OTA",
    "WifiStation",
};
constexpr size_t TOAST_TAGS_COUNT = sizeof(TOAST_TAGS) / sizeof(TOAST_TAGS[0]);

// 整形パラメータ (頭 17 + "..." + 尻 17 = 37 chars 超で truncate)
constexpr size_t HEAD_CHARS  = 17;
constexpr size_t TAIL_CHARS  = 17;
constexpr size_t THRESHOLD   = HEAD_CHARS + 3 + TAIL_CHARS;
constexpr uint32_t THROTTLE_MS       = 1000;  // 同一 TAG の最小間隔
constexpr uint32_t TOAST_DURATION_MS = 1600;

// dispatch queue 経由で main task に渡す固定長メッセージ。
struct ToastMsg {
    char text[80];
};

QueueHandle_t   toast_queue_  = nullptr;
TaskHandle_t    toast_task_   = nullptr;
vprintf_like_t  prev_vprintf_ = nullptr;
std::atomic<bool> enabled_cache_{true};

// per-TAG の最終 toast 時刻 (ms since boot)
uint32_t last_toast_ms_[TOAST_TAGS_COUNT] = {0};

bool is_whitelisted(std::string_view tag, size_t* idx_out)
{
    for (size_t i = 0; i < TOAST_TAGS_COUNT; ++i) {
        if (tag == TOAST_TAGS[i]) {
            if (idx_out) *idx_out = i;
            return true;
        }
    }
    return false;
}

// "TAG: <body>" に整形。body が THRESHOLD 超なら head/tail truncate。
std::string format_toast(std::string_view tag, std::string_view body)
{
    std::string out;
    out.reserve(tag.size() + 2 + THRESHOLD);
    out.append(tag);
    out.append(": ");

    if (utf8::char_count(body) <= THRESHOLD) {
        out.append(body);
    } else {
        const size_t head_end   = utf8::byte_offset_head(body, HEAD_CHARS);
        const size_t tail_start = utf8::byte_offset_tail(body, TAIL_CHARS);
        out.append(body.substr(0, head_end));
        out.append("...");
        out.append(body.substr(tail_start));
    }

    if (out.size() >= sizeof(ToastMsg::text)) {
        out.resize(sizeof(ToastMsg::text) - 1);
    }
    return out;
}

// "[\033[…m]E (12345) TAG: body[\033[0m]\n" → tag, body 抽出。
// 失敗時は false。
bool parse_loge_line(const char* line, std::string& tag, std::string& body)
{
    if (!line) return false;

    // 先頭 ANSI escape (\033[..m) を skip
    while (*line == '\033') {
        const char* m = std::strchr(line, 'm');
        if (!m) return false;
        line = m + 1;
    }

    if (line[0] != 'E' || line[1] != ' ' || line[2] != '(') return false;

    const char* close_paren = std::strchr(line + 3, ')');
    if (!close_paren) return false;

    const char* tag_start = close_paren + 1;
    while (*tag_start == ' ') ++tag_start;

    const char* colon = std::strchr(tag_start, ':');
    if (!colon || colon == tag_start) return false;

    tag.assign(tag_start, static_cast<size_t>(colon - tag_start));

    const char* body_start = colon + 1;
    while (*body_start == ' ') ++body_start;

    body.assign(body_start);

    // 末尾の改行・ANSI reset (\033[0m) を除去
    while (!body.empty()) {
        const char back = body.back();
        if (back == '\n' || back == '\r') {
            body.pop_back();
            continue;
        }
        // ANSI reset の検出: 末尾が 'm' で、それ以前に '\033' があれば全部削る
        const auto esc = body.find_last_of('\033');
        if (esc != std::string::npos && back == 'm') {
            body.resize(esc);
            continue;
        }
        break;
    }
    return true;
}

void toast_dispatch_task(void* /*arg*/)
{
    ToastMsg msg{};
    while (true) {
        if (xQueueReceive(toast_queue_, &msg, portMAX_DELAY) == pdTRUE) {
            view::pop_a_toast(msg.text, view::ToastType::Error, TOAST_DURATION_MS);
        }
    }
}

int custom_vprintf(const char* fmt, va_list args)
{
    // 1. 旧 vprintf へ転送 (console 出力維持)
    va_list args_copy;
    va_copy(args_copy, args);
    int ret = prev_vprintf_ ? prev_vprintf_(fmt, args_copy) : std::vprintf(fmt, args_copy);
    va_end(args_copy);

    // 2. enabled 判定
    if (!enabled_cache_.load(std::memory_order_relaxed)) return ret;
    if (!toast_queue_) return ret;

    // 3. format 解決
    char buf[256];
    int written = std::vsnprintf(buf, sizeof(buf), fmt, args);
    if (written <= 0) return ret;
    if (written >= static_cast<int>(sizeof(buf))) buf[sizeof(buf) - 1] = '\0';

    // 4. parse (E レベルのみ)
    std::string tag;
    std::string body;
    if (!parse_loge_line(buf, tag, body)) return ret;

    // 5. whitelist 判定
    size_t idx = 0;
    if (!is_whitelisted(tag, &idx)) return ret;

    // 6. throttle (同 TAG は THROTTLE_MS に 1 回)
    const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (now_ms - last_toast_ms_[idx] < THROTTLE_MS) return ret;
    last_toast_ms_[idx] = now_ms;

    // 7. 整形 → queue へ push (溢れは drop)
    const std::string formatted = format_toast(tag, body);
    ToastMsg msg{};
    std::strncpy(msg.text, formatted.c_str(), sizeof(msg.text) - 1);
    xQueueSend(toast_queue_, &msg, 0);

    return ret;
}

}  // namespace

void init()
{
    if (toast_queue_) return;  // 二重初期化を防止

    refresh_enabled_from_nvs();

    toast_queue_ = xQueueCreate(8, sizeof(ToastMsg));
    if (!toast_queue_) {
        ESP_LOGE(SELF_TAG, "queue alloc failed");
        return;
    }

    BaseType_t ok = xTaskCreate(&toast_dispatch_task, "gob_err_toast",
                                4096, nullptr, 3, &toast_task_);
    if (ok != pdPASS) {
        ESP_LOGE(SELF_TAG, "dispatch task create failed");
        vQueueDelete(toast_queue_);
        toast_queue_ = nullptr;
        return;
    }

    prev_vprintf_ = esp_log_set_vprintf(&custom_vprintf);
    ESP_LOGI(SELF_TAG, "error toast hook installed");
}

bool is_enabled()
{
    return enabled_cache_.load(std::memory_order_relaxed);
}

void refresh_enabled_from_nvs()
{
    enabled_cache_.store(get_error_toast_enabled(), std::memory_order_relaxed);
}

}  // namespace stackchan::gob_fork::error_toast
