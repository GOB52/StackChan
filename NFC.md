# NFC (NFC-A NDEF) Guide

StackChan firmware GOB fork における NFC タグの使い方と、独自 NDEF プロトコルの仕様をまとめる。

---

## 1. 概要

| 項目 | 内容 |
|---|---|
| ハードウェア | **M5Unit-NFC** (ST25R3916、I2C 接続。BMI270 / RTC / FT6336 と I2C bus 共有) |
| ライブラリ | `m5stack/M5Unit-NFC` (`feature/esp-idf-native-support` ブランチ、ESP-IDF Component Manager 経由) |
| 対応スコープ | **NFC-A** (ISO 14443-A、Type-B/F は非対応) + **NDEF** (NFC Data Exchange Format) フォーマット + **MIME** type レコード |
| ペイロード形式 | UTF-8 JSON (`application/vnd.stackchan.cmd+json`、RFC 6838 vendor tree) |
| 用途 | タグをかざすと stackchan:cmd (`move` / `rgb` / `home` / `dance` / `stop_dance`) を実行 |
| MCP との関係 | xiaozhi の `self.robot.*` MCP tool と同一の `actions/` 層を共有 → AI 音声と NFC で同セマンティクス |

### 1.1 関連ソース

| ファイル | 役割 |
|---|---|
| `firmware/main/hal/hal_nfc.cpp` | UnitNFC bring-up + poll task + NDEF read + retry |
| `firmware/main/stackchan/nfc/nfc_cmd_dispatcher.{h,cpp}` | JSON parse + cmd 振り分け |
| `firmware/main/stackchan/actions/robot_actions.{h,cpp}` | MCP/NFC 共通の動作実装 |
| `firmware/main/stackchan/gob_fork_nvs.{h,cpp}` | `nfc_enabled` (opt-in) |

---

## 2. ユーザー向け: NFC を有効化してタグを使う

### 2.1 NFC を有効化 (NVS opt-in)

**デフォルトは OFF**。理由: UnitNFC poll は I2C bus を共有しているため、有効化前に xiaozhi の audio codec / DSP と相性問題がないか確認するため明示的に opt-in としている。

有効化方法:

- **GOB FORK アプリ → NFC 設定** (提供されている場合)
- または NVS API: `stackchan::gob_fork::set_nfc_enabled(true)` を起動コードから 1 度呼ぶ

切替後 **再起動** で反映 (poll task は起動時に作る)。

### 2.2 推奨タグ

| タグ種別 | NDEF 容量 | コメント |
|---|---|---|
| **NTAG213** | 137 B | 1 cmd は十分書ける (move 等で 50-80 B) |
| **NTAG215** | 480 B | 複数 cmd / 余裕あるサイズ |
| **NTAG216** | 872 B | 大きめのペイロード可 |
| MIFARE Ultralight | 46 B | 単純な cmd のみ (例: `{"v":1,"cmd":"home"}`) |

NFC-A 互換であれば上記以外でも動作する。

### 2.3 スマホからタグへ書き込む (NFC Tools)

**NFC Tools** (Wakdev) を推奨。Android / iOS いずれにも提供されており、MIME type レコードを直接書き込めるため本 fork のプロトコルとの相性がよい。

手順:

1. アプリで「書き込む」 → 「レコードを追加」 → **カスタム** (Custom)
2. **MIME type** を選択
3. MIME type 欄: `application/vnd.stackchan.cmd+json`
4. Content 欄: 後述の JSON (例: `{"v":1,"cmd":"home","s":500}`)
5. 「書き込む」 → タグをスマホに近づけて完了

### 2.4 動作確認

**前提**: AI AGENT が起動済みであること (起動時の初期化が一段落してから NFC poll task が立ち上がる)。Boot 直後 (起動アニメーション中・WiFi 接続中など) はまだ NFC が有効化されていないので反応しない。

タグを CoreS3 にかざすと:

1. **chime 再生** (xiaozhi 標準の通知音、TTS 発話中はサイレント)
2. **Toast 表示**: `% NFC.<cmd>(...)`  (画面上端 2 秒)
3. **シリアルログ**:
   ```
   I HAL-NFC: PICC detected uid=04A1B2... type=NTAG213
   I HAL-NFC: NDEF read start uid=...
   I HAL-NFC: stackchan:cmd received uid=... bytes=...
   I NFC-CMD: <cmd> ...
   ```

### 2.5 トラブルシュート

| 症状 | 原因候補 | 対処 |
|---|---|---|
| 何も反応しない | NFC opt-in OFF / UnitNFC 未接続 | `nfc_enabled` 確認 / 配線確認 |
| `PICC detected` は出るが cmd 実行されない | MIME type が違う | NFC Tools の MIME type 欄を再確認 (`application/vnd.stackchan.cmd+json`) |
| `record skipped: TNF=...` (warn) | TNF が MIME (2) でない | NDEF レコードを「カスタム MIME」で書き直す |
| `JSON parse failed` (warn) | JSON 構文エラー | `jq . your.json` で検証 |
| `unsupported version N` (warn) | `v` フィールド ≠ 1 | `"v": 1` を指定 |
| `unknown cmd '...'` (warn) | cmd 名タイプミス | §4 の一覧と照合 |
| `% NFC.move(reject: type)` Toast | 引数の **型** が違う (例: 数値ではなく文字列) | JSON の型を確認。範囲外は clamp されるので reject にはならない |

---

## 3. タグ制作者向け: NDEF レコードの作り方

### 3.1 NDEF レコード仕様

NFC Forum NDEF 1.0 準拠。本 fork が読むのは:

| フィールド | 値 |
|---|---|
| **TNF** | `2` (MIME Media) |
| **Type** | `application/vnd.stackchan.cmd+json` (固定文字列) |
| **Payload** | UTF-8 で encode された JSON 文字列 |
| **ID** | 任意 (使用しない) |

その他の TNF (Well-known, External 等) や、対象外 MIME type のレコードは **warn ログを出して無視**。

### 3.2 MIME type は固定

```
application/vnd.stackchan.cmd+json
```

RFC 6838 vendor tree の `vnd.stackchan` 名前空間を予約使用。将来も変更しない。

### 3.3 最小ペイロード例

すべて UTF-8 JSON。改行/空白の有無は任意 (NDEF 上のバイト数を抑えたければ空白なしが望ましい)。

```json
{"v":1,"cmd":"home"}
```

```json
{"v":1,"cmd":"home","s":800}
```

```json
{"v":1,"cmd":"move","x":30,"y":45,"s":300}
```

```json
{"v":1,"cmd":"rgb","r":160,"g":40,"b":40}
```

```json
{"v":1,"cmd":"dance","style":"happy"}
```

```json
{"v":1,"cmd":"stop_dance"}
```

### 3.4 複数レコードの取り扱い

1 つの NDEF メッセージ (Message TLV) 内に **複数 MIME レコード** を含めることが可能。すべての該当レコードが順次 dispatch される。

非対象 record (URI / Text 等) が混ざっていても問題なし (skip + warn)。

### 3.5 サイズ目安

NDEF メッセージのオーバーヘッド (header + TLV + record) で約 30-40 B 上乗せされる。

| ペイロード | おおよその NDEF サイズ |
|---|---|
| `{"v":1,"cmd":"home"}` | ~50 B |
| `{"v":1,"cmd":"home","s":800}` | ~60 B |
| `{"v":1,"cmd":"move","x":30,"y":45,"s":300}` | ~75 B |
| `{"v":1,"cmd":"rgb","r":160,"g":40,"b":40}` | ~70 B |
| `{"v":1,"cmd":"dance","style":"happy"}` | ~70 B |

NTAG213 (137 B) なら単一 cmd は確実。複数 cmd を 1 タグにまとめるなら NTAG215 以上。

---

## 4. JSON プロトコルリファレンス

### 4.1 共通エンベロープ

```json
{ "v": 1, "cmd": "<command name>", ...args }
```

| key | 必須 | 値 |
|---|---|---|
| `v` | **必須** | プロトコルバージョン。現状 `1` のみ。`v != 1` のレコードは drop |
| `cmd` | **必須** | コマンド名。下表参照 |
| その他 | 任意 | コマンド固有の引数 |

### 4.2 `move` — サーボを動かす

```json
{"v":1,"cmd":"move","x":30,"y":45,"s":300}
```

| arg | 型 | 範囲 | デフォルト | 単位 |
|---|---|---|---|---|
| `x` | int | -128 〜 128 | `-9999` (=変更しない) | yaw 度 (内部で ×10) |
| `y` | int | 0 〜 90 | `-9999` (=変更しない) | pitch 度 |
| `s` | int | 100 〜 1000 | 150 | servo 速度 |

意味は MCP の `self.robot.set_head_angles` と同じ。`-9999` を指定すると、その軸は現在位置を保持。

### 4.3 `rgb` — NeonLight 色変更

```json
{"v":1,"cmd":"rgb","r":160,"g":40,"b":40}
```

| arg | 型 | 範囲 | デフォルト |
|---|---|---|---|
| `r` / `g` / `b` | int | 0 〜 168 | 0 |

MCP の `self.robot.set_led_color` の "safe range" (168 上限) と一致。両側 NeonLight 同色。

### 4.4 `home` — ホームポジションに戻る

```json
{"v":1,"cmd":"home","s":500}
```

| arg | 型 | 範囲 | デフォルト |
|---|---|---|---|
| `s` | int | 100 〜 1000 | 500 |

MCP の `self.robot.head_home` と同じ実装。

### 4.5 `dance` — ダンス開始

```json
{"v":1,"cmd":"dance","style":"happy"}
```

| arg | 型 | デフォルト |
|---|---|---|
| `style` | string | `"happy"` |

style が未知の場合 Toast `% NFC.dance(unknown style=...)` を出して何もしない (warn ログ)。MCP の `self.robot.dance` と同等。

### 4.6 `stop_dance` — ダンス停止

```json
{"v":1,"cmd":"stop_dance"}
```

引数なし。MCP の `self.robot.stop_dance` と同等。

### 4.7 cmd 一覧表

| cmd | 引数 | 範囲 | デフォルト | 対応 MCP tool |
|---|---|---|---|---|
| `move` | `x`, `y`, `s` | x: -128..128 / y: 0..90 / s: 100..1000 | -9999 / -9999 / 150 | `self.robot.set_head_angles` |
| `rgb` | `r`, `g`, `b` | 0..168 | 0 | `self.robot.set_led_color` |
| `home` | `s` | 100..1000 | 500 | `self.robot.head_home` |
| `dance` | `style` | (string) | `"happy"` | `self.robot.dance` |
| `stop_dance` | (なし) | — | — | `self.robot.stop_dance` |

範囲外の数値は **clamp + warn ログ** (reject にはならない)。型不一致 (例: int 期待箇所が string) は **reject**。

---

## 5. 開発者向け: 内部構造

### 5.1 関連ソース

| ファイル | 役割 |
|---|---|
| `firmware/main/hal/hal_nfc.cpp` | UnitNFC bring-up、poll task、reactivate / NDEF read retry、NFC Cmd 解析準備 |
| `firmware/main/stackchan/nfc/nfc_cmd_dispatcher.{h,cpp}` | JSON parse、cmd dispatch、Toast / chime feedback |
| `firmware/main/stackchan/actions/robot_actions.{h,cpp}` | MCP / NFC 共通アクション (`head_home` / `dance` / `stop_dance` 等) |
| `firmware/main/hal/hal.h` | `NfcTagEvent_t` / `NfcCmdEvent_t` 型 + `Hal::onNfcTagDetected` / `onNfcCmdReceived` シグナル |

### 5.2 Poll フロー

```
_nfc_task (core1, prio1, vTaskDelay 10ms)
 └─ poll_once
     ├─ _units->update()
     ├─ _nfc_a->detect(piccs, timeout)         timeout: speaking?10:50 ms
     ├─ for each PICC:
     │    └─ process_picc
     │        ├─ identify(u) [失敗→skip]
     │        ├─ tag_events.push (uid + type)
     │        ├─ reactivate_with_retry(u)      retry: speaking?0:3
     │        ├─ supportsNDEF()? [no→skip]
     │        ├─ read_ndef_with_retry(u, tlvs) retry: speaking?1:3
     │        └─ parse_tlvs_and_collect → cmd_events
     ├─ deactivate()
     └─ emit シグナル (busy 解除後にまとめて)
```

PNG 等の重い処理 (Toast / OGG) は busy 解除後に走るよう、`_nfc_busy.store(false)` の後で signal emit する。

### 5.3 Signal

```cpp
struct NfcTagEvent_t { std::string uid; std::string type; };
struct NfcCmdEvent_t { std::string uid; std::vector<uint8_t> payload; };

uitk::Signal<NfcTagEvent_t> Hal::onNfcTagDetected;
uitk::Signal<NfcCmdEvent_t> Hal::onNfcCmdReceived;
```

`CmdDispatcher::initOnce()` が `onNfcCmdReceived` に接続して JSON dispatch を担う。アプリ側で UID 集計などを行いたい場合は `onNfcTagDetected` を別途 connect。

### 5.4 I2C 排他

NFC poll は core1 で I2C 操作中、FT6336 の touch poll (core 0 esp_timer) と bus を取り合うとフリーズが起きうる。回避策:

- `std::atomic<bool> _nfc_busy` を poll 中 true に立てる
- FT6336 esp_timer は `Hal::isNfcBusy()` を確認して poll skip
- NDEF read 等の重い区間も busy true で守られる (部分解放禁止 — `tmp/case_w_failure_analysis.md` 参照: 部分解放は bus stuck → 全 I2C 死)

### 5.5 Speaking-aware tuning (要約)

xiaozhi の TTS 発話中 (`Application::GetDeviceState() == kDeviceStateSpeaking`) は audio_input task が CPU 0 を占有し、NFC poll task が回らない瞬間がある (5/5s に低下するケースを実機ログで確認)。この CPU starve の上で同じ retry 数を維持すると cycle 数がさらに低下する悪循環になるため、speaking 中はコストを切り詰める方針:

| 項目 | non-speaking | speaking | 根拠 |
|---|---|---|---|
| `detect` timeout | 50 ms | 10 ms | speaking 中は cycle 機会優先 (REQA polling 数より cycle 回転を稼ぐ) |
| `reactivate` retry | 最大 3 回 | 0 回 | speaking 中は recover 実績ゼロ (実機ログで確認済み)、retry 浪費が cycle starve を悪化 |
| `ndefRead` retry | 最大 3 回 | 1 回 | speaking 中も 1 retry での recover 実例あり、2 回以降は不発のため打ち切り |

実用上 speaking 中も「セリフ間の隙間」では検知が成功する。完全な解決には audio task の優先度調整 / Wake Up Mode 採用などの根本対策が必要 (詳細は `tmp/nfc_detect_lightening.md` X1-X5、`tmp/nfc_speaking_rf_failure_analysis.md` を参照)。

### 5.6 新しい cmd を追加する手順

1. `firmware/main/stackchan/actions/robot_actions.{h,cpp}` に共通実装を追加 (MCP / NFC 共有のため)
2. xiaozhi 側 MCP tool (`self.robot.<name>`) にも同じ呼び出しをラップ (任意だが推奨: AI 音声と NFC で挙動を揃える)
3. `nfc_cmd_dispatcher.cpp` の `initOnce()` 内 `registerHandler("<name>", ...)` を追加
4. 引数の range は `constexpr` 定数で MCP と揃える (例: `_RGB_MIN` / `_RGB_MAX`)
5. Toast 文言は既存形式に合わせる: 受理 `% NFC.<cmd>(...)` / reject `!! NFC.<cmd>(reject: ...)`

下位互換: 既存タグは新 cmd を含まない限り影響なし。逆に古いファームに新 cmd を含むタグをかざすと `unknown cmd` ログが出るのみで安全に無視される。

---

## 6. Audio / Toast feedback

### 6.1 Audio (chime)

- 受理時 (`dispatch` が cmd handler を呼んだ後): `OGG_NEW_NOTIFICATION` を再生
- **xiaozhi が speaking 中はサイレント**: AI 発話と被らせない (Toast は表示する)
- 拒否時 / 不明 cmd / parse error: chime なし

### 6.2 Toast (画面上端、2 秒)

| 状況 | 文言例 |
|---|---|
| 受理 | `% NFC.move(x=30, y=45, s=300)` |
| 受理 (引数なし) | `% NFC.stop_dance(stopped)` |
| 引数の型違反 | `!! NFC.move(reject: type)` |
| 範囲外 | (clamp + warn ログ。Toast は受理側で表示) |
| 不明 cmd / parse error / version 不一致 | (Toast/chime なし、warn ログのみ) |

複数 record は Toast が重なるため `ToastDecorator::stack` で順次表示される (ToastManager と同じ挙動)。

---

## 7. 既知の制約 / Future Work

- **NFC-A 限定** — Type-B / Type-F は未対応 (M5Unit-NFC のレイヤ A クラスのみ使用)
- **MIME record のみ** — URI / Text / External など他の TNF はすべて skip
- **`v` 固定 1** — 将来の互換性確保のため将来 v=2 を追加する余地はある
- **speaking 中の poll cycle 数低下** — CPU starve が根因。Wake Up Mode (低電力 inductive sensing) 採用や audio task の優先度調整は未実装
- **常時 polling** — Wake Up Mode (IRQ 駆動) は ST25R3916 のハード機能としてあるが、ライブラリ未対応
- **core 1 pin** — xiaozhi audio AEC が core 0 占有するため core 1 固定。FT6336 abort 保護は xiaozhi-esp32.patch (i2c_device.cc retry-on-timeout) で代替

---

## 8. リファレンス

- ライブラリ: [m5stack/M5Unit-NFC](https://github.com/m5stack/M5Unit-NFC) (`feature/esp-idf-native-support` ブランチ、ESP-IDF Component Manager)
- NDEF 仕様: [NFC Forum — NDEF 1.0](https://nfc-forum.org/)
- 関連 fork メモ:
  - `tmp/nfc_detect_lightening.md` — 軽量化案 (X1-X5)
  - `tmp/nfc_speaking_rf_failure_analysis.md` — speaking 中 RF 劣化分析
  - `tmp/case_w_failure_analysis.md` — NDEF read 中 I2C 部分解放 NG の根拠
  - `tmp/external_control_paths.md` — 外部制御経路総覧 (NFC は B 軸)

---
---

# NFC (NFC-A NDEF) Guide  *(English)*

This document describes how to use NFC tags with the StackChan firmware GOB fork, and the proprietary NDEF protocol it implements.

---

## 1. Overview

| Item | Value |
|---|---|
| Hardware | **M5Unit-NFC** (ST25R3916, I2C — shared with BMI270 / RTC / FT6336) |
| Library | `m5stack/M5Unit-NFC` (`feature/esp-idf-native-support` branch, ESP-IDF Component Manager) |
| Scope | **NFC-A only** (ISO 14443-A; Type-B/F not supported) + **NDEF** + **MIME** records |
| Payload format | UTF-8 JSON (`application/vnd.stackchan.cmd+json`, RFC 6838 vendor tree) |
| Use case | Tap a tag → execute a stackchan:cmd (`move` / `rgb` / `home` / `dance` / `stop_dance`) |
| MCP relationship | Shares the `actions/` layer with xiaozhi's `self.robot.*` MCP tools — AI voice and NFC produce identical behavior |

### 1.1 Source files

| File | Role |
|---|---|
| `firmware/main/hal/hal_nfc.cpp` | UnitNFC bring-up + poll task + NDEF read + retry |
| `firmware/main/stackchan/nfc/nfc_cmd_dispatcher.{h,cpp}` | JSON parse + cmd dispatch |
| `firmware/main/stackchan/actions/robot_actions.{h,cpp}` | Shared MCP / NFC actions |
| `firmware/main/stackchan/gob_fork_nvs.{h,cpp}` | `nfc_enabled` (opt-in) |

---

## 2. For users: enable NFC and use tags

### 2.1 Enable NFC (NVS opt-in)

**Default is OFF**. Reason: UnitNFC poll shares the I2C bus with audio codec / DSP, so NFC is opt-in to make sure users explicitly accept any contention before turning it on.

How to enable:

- **GOB FORK app → NFC settings** (if exposed)
- Or NVS API: call `stackchan::gob_fork::set_nfc_enabled(true)` once

A **reboot** is required (poll task is created at boot).

### 2.2 Recommended tags

| Tag | NDEF capacity | Note |
|---|---|---|
| **NTAG213** | 137 B | One cmd fits comfortably (50–80 B per cmd) |
| **NTAG215** | 480 B | Multiple cmds / generous size |
| **NTAG216** | 872 B | Larger payloads |
| MIFARE Ultralight | 46 B | Simple cmds only (e.g. `{"v":1,"cmd":"home"}`) |

Any NFC-A compatible tag works.

### 2.3 Writing tags from a phone (NFC Tools)

Use **NFC Tools** (Wakdev). It is available on both Android and iOS, and lets you write a MIME-type record directly, which matches this fork's protocol.

Steps:

1. "Write" → "Add a record" → **Custom**
2. Select **MIME type**
3. MIME type: `application/vnd.stackchan.cmd+json`
4. Content: the JSON described below (e.g. `{"v":1,"cmd":"home","s":500}`)
5. "Write" → tap the tag

### 2.4 Verifying behavior

**Prerequisite**: the AI AGENT must be up and running. The NFC poll task starts only after the boot-time initialization settles, so taps during early boot (splash animation, WiFi connection) produce no reaction.

When you tap a tag near the CoreS3:

1. **Chime** (xiaozhi notification sound — silent during TTS)
2. **Toast** at the top: `% NFC.<cmd>(...)` (2 s)
3. **Serial log**:
   ```
   I HAL-NFC: PICC detected uid=04A1B2... type=NTAG213
   I HAL-NFC: NDEF read start uid=...
   I HAL-NFC: stackchan:cmd received uid=... bytes=...
   I NFC-CMD: <cmd> ...
   ```

### 2.5 Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| No reaction at all | NFC opt-in OFF / UnitNFC not connected | Check `nfc_enabled` / wiring |
| `PICC detected` but no cmd | MIME type mismatch | Re-check the MIME field in NFC Tools |
| `record skipped: TNF=...` (warn) | TNF is not MIME (2) | Re-author as a custom MIME record |
| `JSON parse failed` (warn) | JSON syntax error | Validate with `jq .` |
| `unsupported version N` (warn) | `v` ≠ 1 | Set `"v": 1` |
| `unknown cmd '...'` (warn) | Typo in cmd name | Compare with §4 |
| Toast `% NFC.move(reject: type)` | Argument **type** wrong (e.g. string instead of int) | Check JSON types. Out-of-range numbers are clamped, not rejected |

---

## 3. For tag authors: NDEF record details

### 3.1 NDEF record specification

Per NFC Forum NDEF 1.0. The fork only consumes:

| Field | Value |
|---|---|
| **TNF** | `2` (MIME Media) |
| **Type** | `application/vnd.stackchan.cmd+json` (fixed string) |
| **Payload** | UTF-8 JSON string |
| **ID** | unused |

Other TNFs (Well-known, External, ...) and other MIME types are **logged at warn level and skipped**.

### 3.2 MIME type is fixed

```
application/vnd.stackchan.cmd+json
```

Reserved under the RFC 6838 vendor tree (`vnd.stackchan`). Will not change.

### 3.3 Minimal payloads

All UTF-8 JSON. Whitespace is optional (drop it to save bytes on small tags).

```json
{"v":1,"cmd":"home"}
```

```json
{"v":1,"cmd":"home","s":800}
```

```json
{"v":1,"cmd":"move","x":30,"y":45,"s":300}
```

```json
{"v":1,"cmd":"rgb","r":160,"g":40,"b":40}
```

```json
{"v":1,"cmd":"dance","style":"happy"}
```

```json
{"v":1,"cmd":"stop_dance"}
```

### 3.4 Multiple records

A single NDEF message (Message TLV) may contain **multiple MIME records**. Every matching record is dispatched in order.

Non-matching records (URI / Text / etc.) coexist fine — they are simply skipped with a warn log.

### 3.5 Size estimates

NDEF overhead (header + TLV + record framing) adds about 30–40 B.

| Payload | Approx. NDEF size |
|---|---|
| `{"v":1,"cmd":"home"}` | ~50 B |
| `{"v":1,"cmd":"home","s":800}` | ~60 B |
| `{"v":1,"cmd":"move","x":30,"y":45,"s":300}` | ~75 B |
| `{"v":1,"cmd":"rgb","r":160,"g":40,"b":40}` | ~70 B |
| `{"v":1,"cmd":"dance","style":"happy"}` | ~70 B |

NTAG213 (137 B) handles a single cmd reliably. For multiple cmds on one tag, use NTAG215 or larger.

---

## 4. JSON protocol reference

### 4.1 Common envelope

```json
{ "v": 1, "cmd": "<command name>", ...args }
```

| Key | Required | Value |
|---|---|---|
| `v` | **yes** | Protocol version. Currently only `1`. Records with `v ≠ 1` are dropped |
| `cmd` | **yes** | Command name (see table) |
| others | optional | Per-command arguments |

### 4.2 `move` — actuate servos

```json
{"v":1,"cmd":"move","x":30,"y":45,"s":300}
```

| Arg | Type | Range | Default | Unit |
|---|---|---|---|---|
| `x` | int | -128 to 128 | `-9999` (no change) | yaw degrees (×10 internally) |
| `y` | int | 0 to 90 | `-9999` (no change) | pitch degrees |
| `s` | int | 100 to 1000 | 150 | servo speed |

Mirrors MCP `self.robot.set_head_angles`. `-9999` keeps the current angle on that axis.

### 4.3 `rgb` — NeonLight color

```json
{"v":1,"cmd":"rgb","r":160,"g":40,"b":40}
```

| Arg | Type | Range | Default |
|---|---|---|---|
| `r` / `g` / `b` | int | 0 to 168 | 0 |

Matches the "safe range" of MCP `self.robot.set_led_color` (168 max). Both NeonLights take the same color.

### 4.4 `home` — return to home pose

```json
{"v":1,"cmd":"home","s":500}
```

| Arg | Type | Range | Default |
|---|---|---|---|
| `s` | int | 100 to 1000 | 500 |

Same implementation as MCP `self.robot.head_home`.

### 4.5 `dance` — start dance

```json
{"v":1,"cmd":"dance","style":"happy"}
```

| Arg | Type | Default |
|---|---|---|
| `style` | string | `"happy"` |

Unknown styles produce a Toast `% NFC.dance(unknown style=...)` and do nothing (warn log). Matches MCP `self.robot.dance`.

### 4.6 `stop_dance` — stop dance

```json
{"v":1,"cmd":"stop_dance"}
```

No arguments. Matches MCP `self.robot.stop_dance`.

### 4.7 Command summary

| cmd | Args | Range | Default | MCP equivalent |
|---|---|---|---|---|
| `move` | `x`, `y`, `s` | x: -128..128 / y: 0..90 / s: 100..1000 | -9999 / -9999 / 150 | `self.robot.set_head_angles` |
| `rgb` | `r`, `g`, `b` | 0..168 | 0 | `self.robot.set_led_color` |
| `home` | `s` | 100..1000 | 500 | `self.robot.head_home` |
| `dance` | `style` | (string) | `"happy"` | `self.robot.dance` |
| `stop_dance` | (none) | — | — | `self.robot.stop_dance` |

Out-of-range numerics are **clamped + warn-logged** (not rejected). Wrong-type values (e.g. string where int is required) are **rejected**.

---

## 5. For developers: internals

### 5.1 Source files

| File | Role |
|---|---|
| `firmware/main/hal/hal_nfc.cpp` | UnitNFC bring-up, poll task, reactivate / NDEF read retries, NFC cmd parse |
| `firmware/main/stackchan/nfc/nfc_cmd_dispatcher.{h,cpp}` | JSON parse, cmd dispatch, Toast / chime feedback |
| `firmware/main/stackchan/actions/robot_actions.{h,cpp}` | Shared MCP / NFC actions (`head_home` / `dance` / `stop_dance` etc.) |
| `firmware/main/hal/hal.h` | `NfcTagEvent_t` / `NfcCmdEvent_t` + `Hal::onNfcTagDetected` / `onNfcCmdReceived` signals |

### 5.2 Poll flow

```
_nfc_task (core1, prio1, vTaskDelay 10 ms)
 └─ poll_once
     ├─ _units->update()
     ├─ _nfc_a->detect(piccs, timeout)         timeout: speaking?10:50 ms
     ├─ for each PICC:
     │    └─ process_picc
     │        ├─ identify(u) [fail → skip]
     │        ├─ tag_events.push (uid + type)
     │        ├─ reactivate_with_retry(u)      retry: speaking?0:3
     │        ├─ supportsNDEF()? [no → skip]
     │        ├─ read_ndef_with_retry(u, tlvs) retry: speaking?1:3
     │        └─ parse_tlvs_and_collect → cmd_events
     ├─ deactivate()
     └─ emit signals (after busy is released)
```

Heavy work (Toast, OGG playback) runs after `_nfc_busy.store(false)` so the busy window stays short.

### 5.3 Signals

```cpp
struct NfcTagEvent_t { std::string uid; std::string type; };
struct NfcCmdEvent_t { std::string uid; std::vector<uint8_t> payload; };

uitk::Signal<NfcTagEvent_t> Hal::onNfcTagDetected;
uitk::Signal<NfcCmdEvent_t> Hal::onNfcCmdReceived;
```

`CmdDispatcher::initOnce()` connects to `onNfcCmdReceived` for JSON dispatch. App-side UID aggregation can connect to `onNfcTagDetected` separately.

### 5.4 I2C arbitration

NFC poll runs on core 1 and contends with FT6336 touch poll (core 0 esp_timer) for the I2C bus. Mitigation:

- `std::atomic<bool> _nfc_busy` is set during a poll
- The FT6336 esp_timer checks `Hal::isNfcBusy()` and skips its poll
- The whole NDEF read window is covered by busy (partial release is forbidden — see `tmp/case_w_failure_analysis.md`: a partial release stuck the bus and killed all I2C)

### 5.5 Speaking-aware tuning (summary)

While xiaozhi is speaking (`Application::GetDeviceState() == kDeviceStateSpeaking`), the audio_input task occupies CPU 0 and the NFC poll task can drop to ~5 cycles in 5 s (observed in real-device logs). Holding the same retry counts on top of CPU starve only worsens the cycle starvation, so speaking-mode trims cost:

| Item | Non-speaking | Speaking | Rationale |
|---|---|---|---|
| `detect` timeout | 50 ms | 10 ms | Prefer cycle throughput over REQA polling depth |
| `reactivate` retry | up to 3 | 0 | Zero recover examples during speaking in real-device logs — retries only worsen starve |
| `ndefRead` retry | up to 3 | 1 | One retry has documented recovers; further retries unproductive |

In practice, gaps between sentences let detection succeed during speech. Full fix requires audio task priority tuning or Wake Up Mode (see `tmp/nfc_detect_lightening.md` X1-X5, `tmp/nfc_speaking_rf_failure_analysis.md`).

### 5.6 Adding a new cmd

1. Add the shared implementation under `firmware/main/stackchan/actions/robot_actions.{h,cpp}` (so MCP and NFC share the code path)
2. (Recommended) Wire the same call from the xiaozhi MCP tool `self.robot.<name>` so AI voice and NFC behave identically
3. In `nfc_cmd_dispatcher.cpp` `initOnce()`, add `registerHandler("<name>", ...)`
4. Define ranges as `constexpr` to match MCP (e.g. `_RGB_MIN` / `_RGB_MAX`)
5. Use the existing Toast format: accept `% NFC.<cmd>(...)`, reject `!! NFC.<cmd>(reject: ...)`

Backward compatibility: existing tags are unaffected unless they include the new cmd. An old firmware seeing a new-cmd tag just logs `unknown cmd` and ignores it safely.

---

## 6. Audio / Toast feedback

### 6.1 Audio (chime)

- On accept (after a cmd handler runs): plays `OGG_NEW_NOTIFICATION`
- **Suppressed during xiaozhi speaking** (Toast still shows)
- No chime on reject / unknown cmd / parse error

### 6.2 Toast (top of screen, 2 s)

| Situation | Example text |
|---|---|
| Accept | `% NFC.move(x=30, y=45, s=300)` |
| Accept (no args) | `% NFC.stop_dance(stopped)` |
| Type violation | `!! NFC.move(reject: type)` |
| Out of range | (clamped + warn log; Toast shows the accepted value) |
| Unknown cmd / parse error / version mismatch | (no Toast / chime; warn log only) |

Multiple records stack via `ToastDecorator::stack` (same as ToastManager).

---

## 7. Known limits / future work

- **NFC-A only** — Type-B / Type-F unsupported (Layer A class only)
- **MIME records only** — URI / Text / External etc. are skipped
- **`v` fixed at 1** — room for v=2 in the future
- **Cycle-rate dip during speaking** — root cause is CPU starve. Wake Up Mode (low-power inductive sensing) and audio task priority tuning are not implemented
- **Always polling** — Wake Up Mode (IRQ-driven) exists in ST25R3916 hardware but is unsupported by the library
- **Pinned to core 1** — xiaozhi audio AEC owns core 0. FT6336 abort protection that previously required core 0 colocation is now provided by the xiaozhi-esp32.patch i2c_device.cc retry-on-timeout patch

---

## 8. References

- Library: [m5stack/M5Unit-NFC](https://github.com/m5stack/M5Unit-NFC) (`feature/esp-idf-native-support`, ESP-IDF Component Manager)
- NDEF spec: [NFC Forum — NDEF 1.0](https://nfc-forum.org/)
- Fork notes:
  - `tmp/nfc_detect_lightening.md` — lightening options (X1-X5)
  - `tmp/nfc_speaking_rf_failure_analysis.md` — RF degradation during speaking
  - `tmp/case_w_failure_analysis.md` — why partial I2C release during NDEF read is forbidden
  - `tmp/external_control_paths.md` — external control map (NFC = axis B)
