# Avatar (Skin) Guide

StackChan firmware GOB fork における Avatar / Skin システムの作成・配置・切替方法をまとめる。

---

## 1. 概要

本 fork の Avatar には 2 系統がある。

| 種別 | 実装 | データ所在 | 備考 |
|---|---|---|---|
| **DefaultAvatar** | `firmware/main/stackchan/avatar/skins/default/` | コード内 (vector 描画) | 上流由来。SD なしでも動作 (フォールバック先) |
| **ImageAvatar** | `firmware/main/stackchan/avatar/skins/image/` | SD カード上の PNG 群 | GOB fork 追加。複数 skin 切替可 |

### 1.1 Skin 選択の優先順位

起動時の skin は次の順で決まる:

1. NVS の `gob_fork:skin_current` に値があり、それが特殊値 `__default__` なら **DefaultAvatar 強制** (SD すら見ない)
2. SD カード未挿入 / mount 失敗 → DefaultAvatar (Toast: "SD card not inserted" / "SD mount failed")
3. `/sdcard/avatar.json` を読み、NVS の `skin_current` が空なら `avatar.json` の `current` を採用
4. 採用 ID が `avatar.json/skins[]` に存在しなければ `skins[0]` にフォールバック
5. `/sdcard/<id>/manifest.json` + 参照 PNG を PSRAM に load → ImageAvatar
6. 上記いずれかが失敗 → DefaultAvatar (Toast: 失敗理由)

### 1.2 関連ソース

| ファイル | 役割 |
|---|---|
| `firmware/main/stackchan/avatar/skins/image/skin_loader.{h,cpp}` | `avatar.json` / `manifest.json` パース、PNG ロード、フォールバック |
| `firmware/main/stackchan/avatar/skins/image/image_avatar.{h,cpp}` | ImageAvatar 本体 (Avatar 派生) |
| `firmware/main/stackchan/avatar/skins/image/image_assets.h` | `ImageAvatarConfig` / `PngBuffer` 等のデータ構造 |
| `firmware/main/stackchan/avatar/avatar/avatar.h` | Avatar 基底 + `HeadPetDecoratorConfig` / `DizzyConfig` |
| `firmware/main/apps/app_gob_fork/view/skin_browser_page.{h,cpp}` | GOB FORK アプリ内の Skin Browser UI |
| `firmware/main/stackchan/gob_fork_nvs.{h,cpp}` | NVS 永続化 (`skin_current` 等) |

---

## 2. ユーザー向け: SD カードに自分の Skin を入れる

### 2.1 ファイル配置

```
/sdcard/
├── avatar.json              ← 必須。skin インデックス
├── <skin_id>/               ← skin ごとのフォルダ (例: ponko)
│   ├── manifest.json        ← 必須。skin 定義
│   └── *.png                ← manifest が参照する PNG 群
└── ...
```

`<skin_id>` はフォルダ名 = `avatar.json/skins[].id` = `manifest.json/id` の三者一致。

### 2.2 avatar.json (最小例)

```json
{
    "version": 1,
    "current": "ponko",
    "skins": [
        { "id": "ponko", "name": "Ponko" }
    ]
}
```

| フィールド | 型 | 必須 | 説明 |
|---|---|---|---|
| `version` | int | 任意 | 将来用。現在は使用しない (0 デフォルト) |
| `current` | string | **必須** | デフォルトで読み込む skin の id |
| `skins[]` | array | **必須** | 1 件以上必要 |
| `skins[].id` | string | **必須** | フォルダ名と一致させる |
| `skins[].name` | string | 任意 | Skin Browser に表示。未指定なら id |

### 2.3 切替方法

3 通り:

1. **GOB FORK アプリ → Skin Browser** で Apply (NVS に保存、再起動後も有効)
2. SD の `avatar.json/current` を書き換え (NVS に値が無いときのみ反映)
3. **GOB FORK → Skin Browser → Default Avatar 強制**: `skin_current` に `__default__` をセット (SD すら見ない)

NVS をクリアして avatar.json の current に戻したいときは:
- GOB FORK アプリ内の "Reset Skin Selection" 系メニュー
- または `nvs_erase_namespace("gob_fork")` 相当 (factory reset)

### 2.4 トラブルシュート

起動時 Toast (画面上端) と Skin Browser エラー表示の意味:

| Toast / 表示 | 原因 | 対処 |
|---|---|---|
| `SD card not inserted` | カード未挿入 | SD カード挿入 |
| `SD mount failed` | フォーマット異常 / 接触不良 | FAT32 で再フォーマット |
| `Skin index load error` | `/sdcard/avatar.json` が無い / JSON 構文エラー | JSON 検証 |
| `Skin manifest/PNG load error` | `manifest.json` が無い / PNG 欠如 / サイズ超過 | manifest と PNG 整合確認 |
| `LOAD ERROR` (Skin Browser) | 個別 skin の load 失敗 | 上に同じ。エラー詳細は version 行に表示 |

---

## 3. Skin 制作者向け: PNG を準備する

### 3.1 画面サイズと座標系

- 物理画面: **320 × 240** (CoreS3、横向き)
- `manifest.json` の `x`, `y`, `w`, `h`: **左上原点** ピクセル座標 (LVGL の絶対座標)
- decorator の `x`, `y`: **画面中心 (160, 120) からのオフセット** (LV_ALIGN_CENTER 基準)

混在しやすいので注意:
- `base` / `eye_left` / `eye_right` / `mouth` の矩形 → **左上原点**
- `emotion_decorators[].x/y` / `head_pet.heart.x/y` 等 → **画面中心オフセット**

### 3.2 必須 PNG

最小構成 (7 枚):

| カテゴリ | ファイル | 用途 |
|---|---|---|
| ベース | `base.image` | 顔の背景 (固定表示) |
| 左目 | `eye_left.open` / `eye_left.closed` | 開眼 / 閉眼 (まばたき) |
| 右目 | `eye_right.open` / `eye_right.closed` | 同上 |
| 口 | `mouth.normal` / `mouth.open` | 閉口 / 開口 (発話アニメ) |

左右で同じ画像を使ってもよい (Robot サンプル参照: 1 枚を共有)。

### 3.3 サイズ上限と PSRAM 消費

- **1 PNG あたり ≤ 1 MB** (loader が拒否)
- `manifest.json` 全体 ≤ 64 KB
- 全 PNG は load 時 **PSRAM** に常駐 (`MALLOC_CAP_SPIRAM`)
- LVGL の image cache が初回描画後にデコード済みデータを保持

実用目安: 1 skin あたり 30〜80 KB (Takao 系サンプル参照)。

### 3.4 推奨フォーマット

- **PNG**: 8bit / α 透過対応
- ベース画像は 320 × 240 全面 (透過部分は黒で透過処理)
- 目 / 口は対象 BBox (manifest の `w`, `h`) と一致させる
- LVGL は読み込み時に内部形式へ展開するため、PNG 自体の圧縮率より展開後サイズの方が PSRAM 圧迫要因になる

---

## 4. manifest.json リファレンス

すべてのトップレベルキー:

```
{
    "id":       string  (任意。informational)
    "name":     string  (任意。Skin Browser metadata)
    "version":  string  (任意。Skin Browser metadata)
    "author":   string  (任意。Skin Browser metadata)
    "base":               { ... }          必須
    "eye_left":           { ... }          必須
    "eye_right":          { ... }          必須
    "mouth":              { ... }          必須
    "emotion_decorators": [ ... ]          任意
    "head_pet":           { ... }          任意
    "dizzy":              { ... }          任意
}
```

### 4.1 base

```json
"base": {
    "image": "base.png",
    "x": 0,
    "y": 0,
    "w": 320,
    "h": 240
}
```

| key | デフォルト | 説明 |
|---|---|---|
| `image` | (必須) | PNG ファイル名 (skin フォルダ相対) |
| `x`, `y` | 0, 0 | 左上原点 |
| `w`, `h` | 320, 240 | 表示サイズ |

### 4.2 eye_left / eye_right

```json
"eye_left": {
    "open":   "eye_left_open.png",
    "closed": "eye_left_closed.png",
    "x": 210, "y": 50, "w": 60, "h": 60
}
```

| key | 説明 |
|---|---|
| `open` / `closed` | 開眼 / 閉眼 PNG (ファイル名空文字 = 省略可だが両方欠如するとまばたきが消える) |
| `x`, `y` | 矩形左上 (画面左上原点) |
| `w`, `h` | 矩形サイズ。両 eye の中心は **getEyeCenterOffset()** で自動算出され、Dizzy decorator のデフォルト位置などに使用される |

### 4.3 mouth

```json
"mouth": {
    "normal": "mouth_close.png",
    "open":   "mouth_open.png",
    "normal_x": 76, "normal_y": 170, "normal_w": 168, "normal_h": 60,
    "open_x":   76, "open_y":   170, "open_w":   168, "open_h":   60
}
```

normal (閉口) と open (開口) で別矩形を指定可能。発話強度に応じて自動切替される。

### 4.4 emotion_decorators[]

Emotion ごとに、表示する装飾 (decorator) を列挙:

```json
"emotion_decorators": [
    { "emotion": "Happy",  "kind": "Heart",  "anim_ms": 500, "x": 108, "y": 20 },
    { "emotion": "Angry",  "kind": "Angry",  "anim_ms": 500, "x": 108, "y": 20 },
    { "emotion": "Sad",    "kind": "Sweat",  "anim_ms": 700, "x": -116, "y": 20 },
    { "emotion": "Sleepy", "kind": "Sleepy", "anim_ms": 200, "x": 108, "y": -70 },
    { "emotion": "Doubt",  "kind": "Doubt",  "anim_ms": 500, "x": 108, "y": 20 }
]
```

| key | 説明 |
|---|---|
| `emotion` | `Neutral` / `Happy` / `Angry` / `Sad` / `Doubt` / `Sleepy` のいずれか |
| `kind` | `None` / `Heart` / `Angry` / `Sweat` / `Shy` / `Dizzy` / `Sleepy` / `Doubt` のいずれか |
| `anim_ms` | アニメーション間隔 (ms)。デフォルト 500 |
| `x`, `y` | (任意) 位置上書き。**画面中心オフセット**。意味は `kind` で異なる: <br/>・Heart / Angry / Sweat / Sleepy / Doubt: 絶対オフセット (デフォルト位置を置換) <br/>・Shy / Dizzy: 各々の左右デフォルト位置への追加オフセット |

position を省略すると各 decorator の組み込みデフォルト (DefaultAvatar 互換) を使用。

### 4.5 head_pet

「頭をなでる」操作 (HeadPet) 中に表示する Heart / Shy decorator の位置を skin ごとに上書き:

```json
"head_pet": {
    "heart": { "x": 110, "y": -90, "anim_ms": 500 },
    "shy": {
        "left":  { "x": -80, "y": 30 },
        "right": { "x":  80, "y": 30 }
    }
}
```

- `heart`: 単一 decorator。指定があれば HeadPetModifier がこの位置を使用
- `shy`: **左右両方** が必須 (片方だけだと無視されデフォルト位置)
- 全部省略すると `HeadPetModifier` が DefaultAvatar 配置を使用

### 4.6 dizzy

シェイク (IMU) で発火する Dizzy decorator のカスタマイズ:

```json
"dizzy": {
    "color": "#000000",
    "anim_ms": 300,
    "scale": 1.7,
    "position": {
        "left":  { "x": -70, "y": -16 },
        "right": { "x":  70, "y": -16 }
    }
}
```

| key | 説明 |
|---|---|
| `color` | `"#RRGGBB"` または `"0xRRGGBB"`。省略時はデフォルト色 |
| `anim_ms` | アニメ間隔 (ms)。デフォルト 300 |
| `scale` | 描画倍率 (1.0 = 等倍)。デフォルト 1.0 |
| `position.left/right` | 左右目に対する **画面中心オフセット**。両方必須 (片方だけだと無視され、`getEyeCenterOffset()` 自動算出にフォールバック) |

### 4.7 任意メタ (Skin Browser 用)

```json
{
    "id": "jacko",
    "name": "Jacko",
    "version": "1.0.0",
    "author": "Takao (M5AvatarLite) / converted by GOB"
}
```

これら 4 フィールドは **skin の動作には影響しない**。Skin Browser ページの表示にのみ使用される。

### 4.8 完全例

フル仕様の参考サンプル一式 (Takao 由来 6 skin: girl / jacko / phi / puipui / robot / slime、出典: [mongonta0716/M5Core2ImageAvatarLite](https://github.com/mongonta0716/M5Core2ImageAvatarLite)) を **`takao_pack.zip`** として配布:

- ダウンロード: **[GOB52/StackChan Wiki — Resources](https://github.com/GOB52/StackChan/wiki/Resources)**

zip 内には `avatar.json` と 6 つの skin フォルダ (各フォルダ内に `manifest.json` + PNG 群) が含まれる。`jacko/manifest.json` がフル仕様 (head_pet / dizzy / emotion_decorators 全部入り) の参考例。

```bash
# 例: SD カードを /Volumes/SDCARD にマウント、zip を ~/Downloads に保存した場合
unzip ~/Downloads/takao_pack.zip -d /tmp
cp -r /tmp/takao_pack/* /Volumes/SDCARD/
```

---

## 5. 開発者向け: 内部構造

### 5.1 Load フロー

```
Boot
 └─ load_avatar_or_fallback(parent)              skin_loader.cpp
     ├─ NVS skin_current == "__default__" ? → DefaultAvatar (no SD)
     ├─ SdGuard::isInserted() ?               → DefaultAvatar fallback
     ├─ SdGuard + MountGuard 取得
     ├─ /sdcard/avatar.json 読み → AvatarIndex
     ├─ NVS skin_current が非空ならそれを採用、空なら index.current
     ├─ skin_id が index.skins[] に無ければ skins[0] にフォールバック
     ├─ /sdcard/<id>/manifest.json + PNG を PSRAM に load
     └─ ImageAvatar(std::move(cfg)).init(parent)
```

PNG は `heap_caps_malloc(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` で確保。`PngBuffer` (ImageAvatarConfig 内) が所有、ImageAvatar の生存期間中保持される。

### 5.2 SD / I2C 排他

- `hal::SdGuard` ctor: LVGL ロック取得 + GPIO35 を SD MOSI に切替
- `hal::SdGuard` dtor: GPIO35 を LCD_DC に戻し、LVGL ロック解放
- `MountGuard`: `/sdcard` mount/unmount スコープガード
- `TouchSkipGuard` (Skin Browser): SD load + thumb 生成中の touchpad poll を抑止 (I2C bus 競合回避)

### 5.3 Decorator 上書き機構

`Avatar` 基底に下記 virtual を追加 (GOB fork):

```cpp
virtual const HeadPetDecoratorConfig* getHeadPetConfig() const { return nullptr; }
virtual const DizzyConfig*            getDizzyConfig()   const { return nullptr; }
virtual uitk::Vector2i getEyeCenterOffset(bool isLeft) const;  // DefaultAvatar 既定
```

- `nullptr` → 各 modifier (HeadPet / IMU) が decorator 組み込みデフォルトを使用
- `ImageAvatar` がこれらを override し、manifest 由来の `head_pet` / `dizzy` を返す
- `getEyeCenterOffset()` は manifest の `eye_left` / `eye_right` 矩形中心から自動計算

### 5.4 NVS

| Namespace | Key | 型 | 用途 |
|---|---|---|---|
| `gob_fork` | `skin_current` | str | 現在 skin id (空 = avatar.json 優先, `__default__` = DefaultAvatar 強制) |
| `gob_fork` | `scr_timeout_s` | u32 | (参考) スクリーンセーバ秒数 |
| `gob_fork` | `nfc_enabled` | u8 | (参考) NFC 有効化 |
| `gob_fork` | `time_24h` | u8 | (参考) 12H/24H |

API は `firmware/main/stackchan/gob_fork_nvs.h` を参照。

### 5.5 新しい Decorator/Element 追加の指針

1. `firmware/main/stackchan/avatar/decorators/<name>.cpp` に decorator 実装追加
2. `EmotionDecoratorKind` (`image_assets.h`) に enum 値追加
3. `skin_loader.cpp` の `_kind_map` に文字列マッピング追加
4. ImageAvatar / DefaultAvatar の対応 modifier から呼び出し
5. (任意) skin ごとの設定が必要なら manifest スキーマ拡張 (`HeadPetDecoratorConfig` のように `Avatar` virtual で公開)

下位互換: 既存 manifest が新 enum 文字列を含まない限り読み込みは壊れない。新 enum を未指定の skin は decorator が表示されないだけ。

---

## 6. Emotion / DecoratorKind 一覧

### 6.1 Emotion (`Emotion` enum, `elements/emotion.h`)

`Neutral` / `Happy` / `Angry` / `Sad` / `Doubt` / `Sleepy`

### 6.2 EmotionDecoratorKind (`image_assets.h`)

| 値 | 既定位置 | 用途 |
|---|---|---|
| `None` | (描画なし) | 明示的に decorator を出さない |
| `Heart` | 画面上部中央 | Happy / なでなで |
| `Angry` | 画面上部 | Angry |
| `Sweat` | 顔横 | Sad / 焦り |
| `Shy` | 両頬 (左右) | はにかみ。各目に対する追加オフセット |
| `Dizzy` | 両目重畳 (左右) | シェイク。`DizzyConfig` で色 / scale / position 上書き可 |
| `Sleepy` | 頭上 | Sleepy |
| `Doubt` | 頭上 | Doubt |

---

## 7. 既知の制約 / Future Work

- PNG 1 枚 1 MB 上限 (PSRAM 圧迫回避)
- `manifest.json` 64 KB 上限
- emotion_decorators の `kind` で **`Shy`** / **`Dizzy`** の position は左右対の追加オフセット (絶対オフセットではない) 仕様。1 つの emotion に左右 2 個指定する用途は現状未対応
- Animated PNG / アニメーションフレーム列は未対応 (静止画のみ)
- 言語切替・複数フォントは Avatar 側非依存 (xiaozhi 側で持つ)
- 個別 element (e.g., 鼻 / 髪) の追加は 5.5 のとおり拡張可だが現状実装なし

---

## 8. リファレンス / 出典

- **[mongonta0716/M5Core2ImageAvatarLite](https://github.com/mongonta0716/M5Core2ImageAvatarLite)** — `tmp/takao_pack/` の girl / jacko / phi / puipui / robot / slime 6 skin (顔パーツ画像) の出典。Takao Akaki (mongonta555) 氏が公開している M5Stack 用 ImageAvatar ライブラリで、本 fork はそのアセットを ESP-IDF + LVGL 上で描画する仕組みに変換して使用している
- LVGL: [https://lvgl.io/](https://lvgl.io/) (PNG decode + image cache)
- PNG 仕様: [PNG (Portable Network Graphics) Specification](https://www.w3.org/TR/png/)

---
---

# Avatar (Skin) Guide  *(English)*

This document describes the Avatar / Skin system in the StackChan firmware GOB fork: how to author, deploy, and switch skins.

---

## 1. Overview

The fork supports two avatar implementations:

| Type | Source location | Data location | Notes |
|---|---|---|---|
| **DefaultAvatar** | `firmware/main/stackchan/avatar/skins/default/` | In code (vector) | Inherited from upstream. Works with no SD card (used as fallback) |
| **ImageAvatar** | `firmware/main/stackchan/avatar/skins/image/` | PNGs on SD card | Added in GOB fork. Multiple skins selectable |

### 1.1 Skin selection priority

At boot the active skin is determined by:

1. If NVS `gob_fork:skin_current` equals `__default__` → **force DefaultAvatar** (SD never read)
2. SD card not inserted / mount failed → DefaultAvatar (Toast: `SD card not inserted` / `SD mount failed`)
3. Read `/sdcard/avatar.json`. If NVS `skin_current` is non-empty, use it; else use `avatar.json/current`
4. If selected id is missing from `avatar.json/skins[]`, fall back to `skins[0]`
5. Load `/sdcard/<id>/manifest.json` and PNG assets into PSRAM → ImageAvatar
6. Any failure above → DefaultAvatar (Toast describes cause)

### 1.2 Source files

| File | Role |
|---|---|
| `firmware/main/stackchan/avatar/skins/image/skin_loader.{h,cpp}` | Parses `avatar.json` / `manifest.json`, loads PNGs, fallback handling |
| `firmware/main/stackchan/avatar/skins/image/image_avatar.{h,cpp}` | ImageAvatar class (derives from Avatar) |
| `firmware/main/stackchan/avatar/skins/image/image_assets.h` | `ImageAvatarConfig` / `PngBuffer` data structures |
| `firmware/main/stackchan/avatar/avatar/avatar.h` | Avatar base + `HeadPetDecoratorConfig` / `DizzyConfig` |
| `firmware/main/apps/app_gob_fork/view/skin_browser_page.{h,cpp}` | Skin Browser UI inside the GOB FORK app |
| `firmware/main/stackchan/gob_fork_nvs.{h,cpp}` | NVS persistence (`skin_current`, etc.) |

---

## 2. For users: putting your skin on the SD card

### 2.1 File layout

```
/sdcard/
├── avatar.json              required: skin index
├── <skin_id>/               one folder per skin (e.g. ponko)
│   ├── manifest.json        required: skin definition
│   └── *.png                PNGs referenced by manifest
└── ...
```

`<skin_id>` must match `avatar.json/skins[].id` and `manifest.json/id` (folder name authoritative).

### 2.2 Minimal avatar.json

```json
{
    "version": 1,
    "current": "ponko",
    "skins": [
        { "id": "ponko", "name": "Ponko" }
    ]
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `version` | int | optional | Reserved (default 0) |
| `current` | string | **yes** | id of the skin loaded by default |
| `skins[]` | array | **yes** | Must contain at least one entry |
| `skins[].id` | string | **yes** | Must match folder name |
| `skins[].name` | string | optional | Shown in Skin Browser. Falls back to id |

### 2.3 How to switch

Three ways:

1. **GOB FORK app → Skin Browser → Apply** (persists to NVS, survives reboot)
2. Edit `avatar.json/current` on the SD card (used only when NVS is empty)
3. **GOB FORK → Skin Browser → "force Default"**: writes `__default__` to `skin_current` (SD never read)

To clear the NVS override and fall back to `avatar.json/current`:
- Use the "Reset Skin Selection" entry in the GOB FORK app
- Or factory-reset by erasing the `gob_fork` NVS namespace

### 2.4 Troubleshooting

Top-of-screen Toasts and Skin Browser error messages:

| Toast / Display | Cause | Action |
|---|---|---|
| `SD card not inserted` | No card | Insert SD |
| `SD mount failed` | Bad format / contact | Reformat as FAT32 |
| `Skin index load error` | `/sdcard/avatar.json` missing or malformed | Validate JSON |
| `Skin manifest/PNG load error` | `manifest.json` missing, PNG missing or oversized | Verify manifest <-> PNG match |
| `LOAD ERROR` (Skin Browser) | Per-skin load failed; details shown in version field | Same as above |

---

## 3. For skin authors: preparing PNGs

### 3.1 Screen size and coordinates

- Physical resolution: **320 × 240** (CoreS3, landscape)
- `manifest.json` `x`, `y`, `w`, `h`: **top-left origin** in pixels (LVGL absolute)
- Decorator `x`, `y`: **offset from screen center (160, 120)** (LV_ALIGN_CENTER basis)

Easy to confuse:
- `base` / `eye_left` / `eye_right` / `mouth` rects → **top-left origin**
- `emotion_decorators[].x/y` / `head_pet.heart.x/y` → **center offset**

### 3.2 Required PNGs

Minimum (7 files):

| Category | Field | Purpose |
|---|---|---|
| Base | `base.image` | Face background (always shown) |
| Left eye | `eye_left.open` / `eye_left.closed` | Open / closed (blink) |
| Right eye | `eye_right.open` / `eye_right.closed` | Same |
| Mouth | `mouth.normal` / `mouth.open` | Closed / open (speech) |

Sharing one PNG between left and right is allowed (see Robot sample).

### 3.3 Size limits and PSRAM cost

- **≤ 1 MB per PNG** (loader rejects larger)
- `manifest.json` ≤ 64 KB
- All PNGs are PSRAM-resident at load (`MALLOC_CAP_SPIRAM`)
- LVGL keeps decoded image cache after first render

Practical ballpark: 30–80 KB total per skin (see Takao samples).

### 3.4 Recommended format

- **PNG**, 8-bit with alpha
- Base image should fill 320 × 240 (use alpha for transparency)
- Eye / mouth bitmaps must match the `w`/`h` in manifest
- Decoded RAM footprint matters more than PNG file size

---

## 4. manifest.json reference

All top-level keys:

```
{
    "id":       string  (optional, informational)
    "name":     string  (optional, Skin Browser metadata)
    "version":  string  (optional, Skin Browser metadata)
    "author":   string  (optional, Skin Browser metadata)
    "base":               { ... }          required
    "eye_left":           { ... }          required
    "eye_right":          { ... }          required
    "mouth":              { ... }          required
    "emotion_decorators": [ ... ]          optional
    "head_pet":           { ... }          optional
    "dizzy":              { ... }          optional
}
```

### 4.1 base

```json
"base": {
    "image": "base.png",
    "x": 0, "y": 0, "w": 320, "h": 240
}
```

| Key | Default | Description |
|---|---|---|
| `image` | (required) | PNG filename (relative to skin folder) |
| `x`, `y` | 0, 0 | Top-left origin |
| `w`, `h` | 320, 240 | Display size |

### 4.2 eye_left / eye_right

```json
"eye_left": {
    "open":   "eye_left_open.png",
    "closed": "eye_left_closed.png",
    "x": 210, "y": 50, "w": 60, "h": 60
}
```

| Key | Description |
|---|---|
| `open` / `closed` | Open / closed PNG (empty string skips, but blinking disappears if both missing) |
| `x`, `y` | Rect top-left (screen origin) |
| `w`, `h` | Rect size. Eye centers feed `getEyeCenterOffset()`, used as the default Dizzy decorator anchor |

### 4.3 mouth

```json
"mouth": {
    "normal": "mouth_close.png",
    "open":   "mouth_open.png",
    "normal_x": 76, "normal_y": 170, "normal_w": 168, "normal_h": 60,
    "open_x":   76, "open_y":   170, "open_w":   168, "open_h":   60
}
```

normal (closed) and open can have independent rects. Switched automatically by speech intensity.

### 4.4 emotion_decorators[]

Per-Emotion decorator list:

```json
"emotion_decorators": [
    { "emotion": "Happy",  "kind": "Heart",  "anim_ms": 500, "x": 108, "y": 20 },
    { "emotion": "Angry",  "kind": "Angry",  "anim_ms": 500, "x": 108, "y": 20 },
    { "emotion": "Sad",    "kind": "Sweat",  "anim_ms": 700, "x": -116, "y": 20 },
    { "emotion": "Sleepy", "kind": "Sleepy", "anim_ms": 200, "x": 108, "y": -70 },
    { "emotion": "Doubt",  "kind": "Doubt",  "anim_ms": 500, "x": 108, "y": 20 }
]
```

| Key | Description |
|---|---|
| `emotion` | One of `Neutral` / `Happy` / `Angry` / `Sad` / `Doubt` / `Sleepy` |
| `kind` | One of `None` / `Heart` / `Angry` / `Sweat` / `Shy` / `Dizzy` / `Sleepy` / `Doubt` |
| `anim_ms` | Animation interval (ms). Default 500 |
| `x`, `y` | (optional) Position override, **center offset**. Semantics depend on `kind`: <br/>- Heart / Angry / Sweat / Sleepy / Doubt: absolute offset (replaces default) <br/>- Shy / Dizzy: extra offset added to each side's default |

If position is omitted, the decorator's built-in default (DefaultAvatar layout) is used.

### 4.5 head_pet

Override Heart / Shy decorator positions during HeadPet (head petting) per skin:

```json
"head_pet": {
    "heart": { "x": 110, "y": -90, "anim_ms": 500 },
    "shy": {
        "left":  { "x": -80, "y": 30 },
        "right": { "x":  80, "y": 30 }
    }
}
```

- `heart`: single decorator. If specified, HeadPetModifier uses this position
- `shy`: **both left and right required** (asymmetric input is ignored, falling back to default)
- All omitted → HeadPetModifier uses DefaultAvatar layout

### 4.6 dizzy

Customize the Dizzy decorator triggered by IMU shake:

```json
"dizzy": {
    "color": "#000000",
    "anim_ms": 300,
    "scale": 1.7,
    "position": {
        "left":  { "x": -70, "y": -16 },
        "right": { "x":  70, "y": -16 }
    }
}
```

| Key | Description |
|---|---|
| `color` | `"#RRGGBB"` or `"0xRRGGBB"`. Omitted → default color |
| `anim_ms` | Animation interval (ms). Default 300 |
| `scale` | Render scale (1.0 = native). Default 1.0 |
| `position.left/right` | **Center offset** per eye. Both required (otherwise falls back to `getEyeCenterOffset()` auto layout) |

### 4.7 Optional metadata (Skin Browser)

```json
{
    "id": "jacko",
    "name": "Jacko",
    "version": "1.0.0",
    "author": "Takao (M5AvatarLite) / converted by GOB"
}
```

These four fields **do not affect skin behavior**. They are displayed by the Skin Browser only.

### 4.8 Full example

A complete reference sample pack (6 Takao-derived skins: girl / jacko / phi / puipui / robot / slime; source: [mongonta0716/M5Core2ImageAvatarLite](https://github.com/mongonta0716/M5Core2ImageAvatarLite)) is distributed as **`takao_pack.zip`**:

- Download: **[GOB52/StackChan Wiki — Resources](https://github.com/GOB52/StackChan/wiki/Resources)**

The zip contains `avatar.json` plus six skin folders (each with `manifest.json` + PNGs). `jacko/manifest.json` is the most complete reference (head_pet / dizzy / emotion_decorators all populated).

```bash
# Example: SD mounted at /Volumes/SDCARD, zip saved to ~/Downloads
unzip ~/Downloads/takao_pack.zip -d /tmp
cp -r /tmp/takao_pack/* /Volumes/SDCARD/
```

---

## 5. For developers: internals

### 5.1 Load flow

```
Boot
 └─ load_avatar_or_fallback(parent)              skin_loader.cpp
     ├─ NVS skin_current == "__default__" ? → DefaultAvatar (no SD)
     ├─ SdGuard::isInserted() ?               → DefaultAvatar fallback
     ├─ Acquire SdGuard + MountGuard
     ├─ Read /sdcard/avatar.json → AvatarIndex
     ├─ Use NVS skin_current if non-empty, else index.current
     ├─ If skin_id missing from index.skins[], fall back to skins[0]
     ├─ Load /sdcard/<id>/manifest.json + PNGs into PSRAM
     └─ ImageAvatar(std::move(cfg)).init(parent)
```

PNGs are allocated via `heap_caps_malloc(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)`. `PngBuffer` (in ImageAvatarConfig) owns them and lives as long as the ImageAvatar.

### 5.2 SD / I2C mutual exclusion

- `hal::SdGuard` ctor: takes LVGL lock + switches GPIO35 to SD MOSI
- `hal::SdGuard` dtor: restores GPIO35 to LCD_DC, releases LVGL lock
- `MountGuard`: scoped `/sdcard` mount/unmount
- `TouchSkipGuard` (Skin Browser): suppresses touchpad poll during SD load + thumbnail generation (avoids I2C bus contention)

### 5.3 Decorator override mechanism

Avatar base adds (GOB fork):

```cpp
virtual const HeadPetDecoratorConfig* getHeadPetConfig() const { return nullptr; }
virtual const DizzyConfig*            getDizzyConfig()   const { return nullptr; }
virtual uitk::Vector2i getEyeCenterOffset(bool isLeft) const;  // DefaultAvatar default
```

- `nullptr` → modifiers (HeadPet / IMU) use decorator built-in defaults
- `ImageAvatar` overrides these and returns the manifest-driven `head_pet` / `dizzy`
- `getEyeCenterOffset()` auto-derives from `eye_left` / `eye_right` rect centers in the manifest

### 5.4 NVS

| Namespace | Key | Type | Use |
|---|---|---|---|
| `gob_fork` | `skin_current` | str | Current skin id (empty = follow avatar.json, `__default__` = force DefaultAvatar) |
| `gob_fork` | `scr_timeout_s` | u32 | (ref) Screensaver seconds |
| `gob_fork` | `nfc_enabled` | u8 | (ref) Enable NFC |
| `gob_fork` | `time_24h` | u8 | (ref) 12H/24H clock |

API: `firmware/main/stackchan/gob_fork_nvs.h`.

### 5.5 Adding a new decorator / element

1. Add the decorator implementation under `firmware/main/stackchan/avatar/decorators/<name>.cpp`
2. Add a value to `EmotionDecoratorKind` (`image_assets.h`)
3. Add a string mapping to `_kind_map` in `skin_loader.cpp`
4. Wire it into the corresponding modifier in ImageAvatar / DefaultAvatar
5. (Optional) If per-skin config is needed, extend the manifest schema (expose via an `Avatar` virtual, like `HeadPetDecoratorConfig`)

Backward compatibility: existing manifests keep working as long as they do not contain new enum strings. Skins lacking the new enum simply do not display that decorator.

---

## 6. Emotion / DecoratorKind reference

### 6.1 Emotion (`Emotion` enum, `elements/emotion.h`)

`Neutral` / `Happy` / `Angry` / `Sad` / `Doubt` / `Sleepy`

### 6.2 EmotionDecoratorKind (`image_assets.h`)

| Value | Default position | Use |
|---|---|---|
| `None` | (no draw) | Explicitly suppress decorator |
| `Heart` | Top center | Happy / petting |
| `Angry` | Top | Angry |
| `Sweat` | Side of face | Sad / panic |
| `Shy` | Both cheeks (L/R) | Bashful. Per-eye additional offset |
| `Dizzy` | Both eyes overlay (L/R) | Shake. `DizzyConfig` overrides color / scale / position |
| `Sleepy` | Above head | Sleepy |
| `Doubt` | Above head | Doubt |

---

## 7. Known limits / future work

- 1 MB per PNG (PSRAM safety)
- 64 KB for `manifest.json`
- For `kind` = `Shy` / `Dizzy`, the position is "additive offset to L/R defaults" rather than absolute — a single emotion cannot currently target two independent absolute positions
- Animated PNG / multi-frame sequences are not supported (still images only)
- Localization / multi-font handling lives outside the avatar layer (xiaozhi side)
- Adding entirely new elements (e.g. nose / hair) is possible per §5.5 but unimplemented today

---

## 8. References / Credits

- **[mongonta0716/M5Core2ImageAvatarLite](https://github.com/mongonta0716/M5Core2ImageAvatarLite)** — Source of the face-part PNGs for the 6 skins (girl / jacko / phi / puipui / robot / slime) in `tmp/takao_pack/`. Original ImageAvatar library for M5Stack by Takao Akaki (mongonta555); this fork converts those assets into a manifest-driven format renderable under ESP-IDF + LVGL.
- LVGL: [https://lvgl.io/](https://lvgl.io/) (PNG decode + image cache)
- PNG spec: [PNG (Portable Network Graphics) Specification](https://www.w3.org/TR/png/)
