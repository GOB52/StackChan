// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once

namespace stackchan::hal {

// SD アクセス用 RAII ガード。
//   - ctor: LVGL lock を取得し、GPIO35 を LCD_DC 出力 → SPI3 MISO 入力へ切替
//   - dtor: GPIO35 を LCD_DC 出力に戻し、LVGL lock を解放
//
// 呼出側責任: スコープ内では LCD/LVGL を操作しないこと (LVGL lock 経由で他タスクは
// 既に排他されているが、自タスクで lvgl API を叩くのも避ける)。
//
// SD 物理アクセスは ensureMounted() 後に標準 VFS (fopen 等) で /sdcard 配下を読む。
class SdGuard {
public:
    SdGuard();
    ~SdGuard();

    SdGuard(const SdGuard&)            = delete;
    SdGuard& operator=(const SdGuard&) = delete;
    SdGuard(SdGuard&&)                 = delete;
    SdGuard& operator=(SdGuard&&)      = delete;

    // 初回のみ esp_vfs_fat_sdspi_mount を呼ぶ (冪等)。成功で true。
    // mount 失敗 / カード未挿入なら false (詳細はログ)。
    bool ensureMounted();

    // 直近 ensureMounted の結果。
    static bool isMounted();

    // AW9523 P0_3 (TF_SW) を読み、カード挿入有無を返す。
    // SdGuard 構築不要 (I2C のみ使用)。
    static bool isInserted();

    // SD アクセス中 (=このガード生存中) かどうか。touchpad polling など
    // 他タスクが I2C/共有リソースをスキップしたい場合に参照する。
    // 拡張 skip フラグ (setExtraTouchSkip) との OR で判定する。
    // ※ esp_timer_task など別タスクから安全に読める (atomic)。
    static bool isActive();

    // SD アクセス本体に加えて、関連処理 (thumb 生成等) も touch poll を抑止
    // したい場合に呼ぶ拡張フラグ。SdGuard 本体のスコープ外でも有効。
    static void setExtraTouchSkip(bool skip);

    // 拡張 skip スコープ用 RAII ヘルパ。
    class TouchSkipGuard {
    public:
        TouchSkipGuard()  { SdGuard::setExtraTouchSkip(true);  }
        ~TouchSkipGuard() { SdGuard::setExtraTouchSkip(false); }
        TouchSkipGuard(const TouchSkipGuard&)            = delete;
        TouchSkipGuard& operator=(const TouchSkipGuard&) = delete;
    };
};

}  // namespace stackchan::hal
