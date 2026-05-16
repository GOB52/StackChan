// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <sdkconfig.h>

namespace stackchan::gob_fork::error_toast {

#if CONFIG_GOB_FORK_ENABLE_ERROR_TOAST

// ESP_LOGE hook を設置し、whitelist TAG (MCP / MQTT / Application / OTA /
// WifiStation) に該当する error log を red Toast として画面に表示する。
// 同 TAG は 1 sec quiet window で throttle、console 出力は維持。
// enabled は NVS (error_toast key、default ON) で永続。
//
// 起動シーケンスで 1 回だけ呼ぶ。再呼出しは no-op。
void init();

// 現在の enabled cache 値。
bool is_enabled();

// NVS から enabled を再読込 (menu toggle 後に呼ぶ)。
void refresh_enabled_from_nvs();

#else  // !CONFIG_GOB_FORK_ENABLE_ERROR_TOAST

// 機能無効ビルド: 呼出側のコード変更を不要にするための inline no-op stubs。
// task / queue / log hook いずれも生成されず、DRAM コストはゼロ。
inline void init() {}
inline bool is_enabled() { return false; }
inline void refresh_enabled_from_nvs() {}

#endif  // CONFIG_GOB_FORK_ENABLE_ERROR_TOAST

}  // namespace stackchan::gob_fork::error_toast
