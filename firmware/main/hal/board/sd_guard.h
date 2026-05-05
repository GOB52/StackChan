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
// SD 物理アクセスは mount() 後に標準 VFS (fopen 等) で /sdcard 配下を読む。
//
// mount/unmount は SdGuard ctor/dtor とは独立した明示 static API として提供する。
// 短期 access は scope 内で mount → 操作 → unmount、長期 (skin browser 等) は
// page lifecycle に紐付けて mount 維持 → 終了時 unmount する設計。小粒度の
// mount → unmount → mount は避けること (各 mount は esp_vfs_fat_sdspi_mount
// 呼出で数百 ms かかる)。
class SdGuard {
public:
    SdGuard();
    ~SdGuard();

    SdGuard(const SdGuard&)            = delete;
    SdGuard& operator=(const SdGuard&) = delete;
    SdGuard(SdGuard&&)                 = delete;
    SdGuard& operator=(SdGuard&&)      = delete;

    // esp_vfs_fat_sdspi_mount を呼ぶ。冪等 (既 mount なら true 返す)。
    // mount 失敗 / カード未挿入なら false (詳細はログ)。
    static bool mount();

    // esp_vfs_fat_sdmmc_unmount を呼ぶ。冪等 (既 unmount なら true 返す)。
    static bool unmount();

    // 現在 mount 中かどうか。
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

    // 短期 scope での mount/unmount 管理用 RAII ヘルパ。
    // 複数 early return がある関数で unmount を毎回明示する冗長を避ける用途。
    // 長期 mount (skin browser 等の page lifecycle) では使わず、明示 mount/unmount を使うこと。
    class MountGuard {
    public:
        MountGuard() : _ok(SdGuard::mount()) {}
        ~MountGuard() { if (_ok) SdGuard::unmount(); }
        MountGuard(const MountGuard&)            = delete;
        MountGuard& operator=(const MountGuard&) = delete;
        bool ok() const { return _ok; }
    private:
        bool _ok;
    };

    // 起動時に SD card と一度ハンドシェイクを行い、SPI state machine を確定状態に
    // 持っていく。GPIO35 が LCD_DC と SD MISO で物理共有されているため、未初期化
    // SD card は CS=HIGH でも MISO を random drive することがあり、launcher 起動
    // 直後の高速 LCD writes で画面崩れを引き起こす。
    //
    // 1 度 mount を試みれば SD と SPI 通信 (CMD0 等) が走り、card は spec 通り
    // CS=HIGH で tristate するようになる。SD 未挿入 / mount 失敗のいずれも
    // 害なし (skin loader が後で再試行する)。Hal::init() から xiaozhi_board_init
    // 直後に 1 回だけ呼ぶ。
    static void performEarlyProbe();
};

}  // namespace stackchan::hal
