// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once

namespace stackchan::gob_fork::error_toast {

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

}  // namespace stackchan::gob_fork::error_toast
