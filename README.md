# StackChan GOB fork

## このフォークで追加された主な機能 / Highlights

### 1. Image-based Avatar (PNG スキン)
SD カードに置いた PNG セットでアバターを差替できます。複数スキンを切替可能で、選択は NVS に永続化されます。Emotion ごとの装飾 (Heart / Shy / Dizzy / Sleepy / Doubt) や HeadPet 演出にも対応。詳細は [Avatar.md](Avatar.md) を参照。

### 2. NFC タグ連携
M5Unit-NFC を opt-in で接続でき、`stackchan:cmd` MIME を持つ NDEF タグから JSON コマンドを発火できます (head 動作 / dance / emotion 切替 等)。**使用前に GOB FORK menu → NFC を ON にして再起動** する必要があります (デフォルト OFF、AI Agent 中の audio 安定性への影響を避けるため)。詳細は [NFC.md](NFC.md) を参照。

### 3. GOB FORK ランチャーアプリ
launcher 画面に専用 app を追加し、Skin Browser / SD Card Info / System Info / Screensaver 設定 / About を一括管理できます。

### 4. ステータスバー強化
12H / 24H 切替、TZ 設定 (NVS 永続化)、SNTP 同期に対応。

---

## Highlights of this fork  *(English)*

### 1. Image-based avatar (PNG skin)
Swap the avatar with a PNG set placed on the SD card. Multi-skin selection is persisted in NVS. Per-emotion decorators (Heart / Shy / Dizzy / Sleepy / Doubt) and the HeadPet animation are supported. See [Avatar.md](Avatar.md) for details.

### 2. NFC tag integration
Optional M5Unit-NFC support: NDEF tags with the `stackchan:cmd` MIME type trigger JSON commands (head motion / dance / emotion switch, etc.). **Before use, enable NFC via the GOB FORK menu and reboot** (off by default to keep AI Agent audio stable). See [NFC.md](NFC.md) for details.

### 3. GOB FORK launcher app
Adds a dedicated app to the launcher with Skin Browser / SD Card Info / System Info / Screensaver settings / About in one place.

### 4. Status bar enhancements
12H / 24H toggle, timezone setting (NVS-persisted), and SNTP synchronization.

---

> ## $\color{blue}{\textsf{推奨 / Recommended}}$
>
> 本 fork (GOB fork) と公式ファームウェアを行き来する際、**公式ファームウェアの書き戻し (M5Burner 等)** や **NVS partition の事故的破損** により、Wi-Fi / xiaozhi 接続情報など各種設定がリセットされる可能性があります。あらかじめ NVS partition (16 KB, offset 0x9000) のバックアップを取っておくことを推奨します。
>
> ### バックアップ / リストア手順
>
> ```bash
> # バックアップ (任意のタイミング、書き込み前推奨)
> esptool.py --port /dev/tty.usbmodemXXXX --baud 460800 read_flash 0x9000 0x4000 nvs_backup.bin
>
> # リストア (設定が消えた / 壊れたとき)
> esptool.py --port /dev/tty.usbmodemXXXX --baud 460800 write_flash 0x9000 nvs_backup.bin
> ```
>
> なお、**公式 → fork 方向 (`idf.py flash` 等) では NVS は自動的に引き継がれます** (app partition のみ書き換わるため)。バックアップが特に効くのは fork → 公式 (M5Burner 経由) や NVS が破損したケースです。
>
> ---
>
> When switching between this fork (GOB fork) and the official firmware, settings such as Wi-Fi and xiaozhi connection info may be lost — either via M5Burner re-flashing the official firmware (which overwrites NVS) or via accidental NVS partition corruption. We recommend backing up the NVS partition (16 KB at offset 0x9000) in advance.
>
> ### Backup / Restore
>
> ```bash
> # Backup (any time; recommended before flashing)
> esptool.py --port /dev/tty.usbmodemXXXX --baud 460800 read_flash 0x9000 0x4000 nvs_backup.bin
>
> # Restore (when settings are lost or NVS is corrupted)
> esptool.py --port /dev/tty.usbmodemXXXX --baud 460800 write_flash 0x9000 nvs_backup.bin
> ```
>
> Note that the **official → fork direction (e.g. `idf.py flash`) preserves NVS automatically** because only the app partition is rewritten. The backup matters mainly for the fork → official direction (via M5Burner) or NVS corruption recovery.

## ビルド前の注意 / Pre-build Notes

GOB fork をビルドする前に以下を実施してください。

### 1. `firmware/fetch_repos.py` の実行

外部依存 (xiaozhi-esp32 等) を取得し、本 fork が必要とする patch を適用します。

```bash
cd firmware
python3 ./fetch_repos.py
```

必要なタイミング:
- 初回 clone 後
- `firmware/repos.json` または `firmware/patches/` 配下の patch を更新したとき
- `firmware/xiaozhi-esp32/` を消してやり直したいとき

### 2. FATFS Long File Name (LFN) の有効化

本 fork は `sdkconfig.defaults` で **FATFS の Long File Name (LFN)** を有効化しています (公式は OFF)。`sdkconfig.defaults` は `firmware/sdkconfig` が**存在しないとき**だけ参照されるため、既存ビルド環境ではそのままだと反映されません。

```bash
rm firmware/sdkconfig
```

必要なタイミング:
- 公式 → 本 fork へ切替えた直後 (LFN を含む defaults 変更を取り込むため)
- `firmware/sdkconfig.defaults` を更新したとき
- `idf.py fullclean` は `build/` のみ消すので `sdkconfig` は別途削除が必要

#### 既に `sdkconfig` をカスタマイズしている場合

削除すると自分の設定が消えてしまうため、`idf.py menuconfig` で以下を**手で有効化**してください:

| 設定 | menuconfig パス |
| ---- | --------------- |
| `CONFIG_FATFS_LFN_STACK=y` | Component config → FAT Filesystem support → Long filename support → **Long filename buffer in stack** |
| `CONFIG_FATFS_MAX_LFN=255` | Component config → FAT Filesystem support → **Max long filename length** = `255` |

(両方未設定だと `/sdcard/eye_right_closed.png` 等の長いファイル名が開けず、Skin が DefaultAvatar に fallback します。)

---

Before building this fork, please run the following.

### 1. Run `firmware/fetch_repos.py`

Fetches external deps (xiaozhi-esp32 etc.) and applies fork-specific patches.

```bash
cd firmware
python3 ./fetch_repos.py
```

When required:
- After the initial clone
- After updating `firmware/repos.json` or any patch under `firmware/patches/`
- When you want to wipe `firmware/xiaozhi-esp32/` and start over

### 2. Enable FATFS Long File Name (LFN)

This fork enables **FATFS Long File Name (LFN)** in `sdkconfig.defaults` (the official fork has it OFF). `sdkconfig.defaults` is only consulted when `firmware/sdkconfig` is **absent**, so an existing build env will not pick up the change unless you delete it.

```bash
rm firmware/sdkconfig
```

When required:
- Right after switching from the official firmware to this fork
- After modifying `firmware/sdkconfig.defaults`
- Note: `idf.py fullclean` only wipes `build/`, so `sdkconfig` must be removed separately.

#### If you already have a customized `sdkconfig`

Deleting it would discard your settings, so enable the following manually with `idf.py menuconfig`:

| Setting | menuconfig path |
| ------- | --------------- |
| `CONFIG_FATFS_LFN_STACK=y` | Component config → FAT Filesystem support → Long filename support → **Long filename buffer in stack** |
| `CONFIG_FATFS_MAX_LFN=255` | Component config → FAT Filesystem support → **Max long filename length** = `255` |

(Without both, long filenames such as `/sdcard/eye_right_closed.png` cannot be opened and the skin falls back to DefaultAvatar.)

---
# StackChan Open-Source


<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1205/K151_stack_chan_main_pictures_01.webp" width="60%">

Here are StackChan related open-source resources, including source code of the StackChan firmware, remote controller firmware, mobile app (iOS and Android), and server. 

Update of this repo could be a little late than the released firmware and mobile app. 

----

<img src="https://cdn.shopify.com/s/files/1/0056/7689/2250/files/5a589623895f65487717894d9240f6b8.png" width="60%">

**StackChan is a super kawaii AI desktop robot co-created by M5Stack and the user community.** It uses the M5Stack **flagship IoT development kit [CoreS3](https://docs.m5stack.com/en/core/CoreS3)** as its main controller, powered by an ESP32-S3 SoC featuring a 240 MHz dual-core processor, with 16MB Flash and 8MB PSRAM onboard, and supporting Wi-Fi and BLE. The main unit also integrates a 2.0-inch capacitive touch display with a high-strength glass cover, a 0.3 MP camera, a proximity & ambient light sensor, a 9-axis IMU (accelerometer + gyroscope + magnetometer), a microSD card slot, a 1W speaker, dual microphones, and power/reset buttons. 

The **robot body**, connected to the main unit, includes a USB-C interface for power and data, a 550 mAh battery, two feedback servos (360-degree continuous rotation on the horizontal axis and 90-degree movement on the vertical axis), two rows totaling 12 RGB LEDs, infrared transmitter and receiver, a three-zone touch panel, and a full-featured NFC module. 

The **factory firmware** is feature-rich, including an AI Agent, lively and expressive animations, ESP-NOW wireless remote control, and online app downloads. It can connect to a mobile app for video viewing, remote avatar control, and more, and also supports online updates (OTA). The product also supports programming via Arduino, UiFlow2, and other methods, and can connect to various expansion units in the M5Stack ecosystem, making it easy to implement a wide range of custom functions. 

> ⚠️ Do not forcibly rotate any movable parts connected to the motors by hand when you are unsure whether the motors are powered and under control, as this may cause hardware damage. 

- Purchase link: [M5Stack Official Store](https://shop.m5stack.com/products/stackchan-kawaii-co-created-open-source-ai-desktop-robot) | [淘宝 Taobao](https://item.taobao.com/item.htm?id=1042238294510)

- Product document page: [English](https://docs.m5stack.com/en/StackChan) | [日本語](https://docs.m5stack.com/ja/StackChan) | [中文](https://docs.m5stack.com/zh_CN/StackChan)

- Board support package: https://github.com/m5stack/StackChan-BSP

Thank you to the contributors of the StackChan community, especially: 

| ![](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1205/avatar_stack_chan.jpg) | ![](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1205/avatar_takao.jpg) |
| -------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| [@stack_chan](https://x.com/stack_chan)                                          | [@mongonta555](https://x.com/mongonta555)                                   |
| Shinya Ishikawa                                                                  | Takao Akaki                                                                 |
