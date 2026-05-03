// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#include "sd_guard.h"
#include "hal_bridge.h"
#include <hal/hal.h>
#include <driver/gpio.h>
#include <driver/sdspi_host.h>
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <esp_rom_gpio.h>
#include <sdmmc_cmd.h>
#include <soc/spi_periph.h>
#include <mooncake_log.h>
#include <fmt/format.h>
#include <cassert>

namespace stackchan::hal {

namespace {

constexpr const char* _tag          = "SdGuard";
constexpr gpio_num_t  _gpio_dc_miso = GPIO_NUM_35;  // shared LCD_DC / SD MISO
constexpr gpio_num_t  _gpio_sd_cs   = GPIO_NUM_4;
// ESP32-S3 GPIO matrix: virtual pad id 0x3C routes constant LOW into an input
// signal — i.e., effectively "disconnect" the signal from any physical pad.
constexpr uint32_t    _matrix_in_low = 0x3C;

bool          _mounted      = false;
sdmmc_card_t* _card         = nullptr;
bool          _guard_active = false;

void enter_sd_mode()
{
    // GPIO35 を input にし、SPI3 MISO input signal をマトリクス経由で接続する。
    // LCD 通常時は spi_bus_initialize で MISO=NC のためマトリクスは未配線。
    gpio_set_direction(_gpio_dc_miso, GPIO_MODE_INPUT);
    esp_rom_gpio_connect_in_signal(_gpio_dc_miso,
                                   spi_periph_signal[SPI3_HOST].spiq_in, false);
}

void exit_sd_mode()
{
    // SPI3 MISO input signal をマトリクスから切断 (constant LOW を割当て)、
    // GPIO35 を LCD_DC 出力モードへ戻す。
    esp_rom_gpio_connect_in_signal(_matrix_in_low,
                                   spi_periph_signal[SPI3_HOST].spiq_in, false);
    gpio_set_direction(_gpio_dc_miso, GPIO_MODE_OUTPUT);
}

bool mount_sd()
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot         = SPI3_HOST;

    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs               = _gpio_sd_cs;
    slot.host_id               = SPI3_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mcfg = {};
    mcfg.format_if_mount_failed           = false;
    mcfg.max_files                        = 4;
    mcfg.allocation_unit_size             = 16 * 1024;

    esp_err_t e = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mcfg, &_card);
    if (e != ESP_OK) {
        mclog::tagError(_tag, "esp_vfs_fat_sdspi_mount failed: 0x{:x}", static_cast<int>(e));
        return false;
    }
    uint64_t bytes = static_cast<uint64_t>(_card->csd.capacity) * _card->csd.sector_size;
    mclog::tagInfo(_tag, "mounted /sdcard ({} MB)", static_cast<uint32_t>(bytes >> 20));
    return true;
}

}  // namespace

SdGuard::SdGuard()
{
    assert(!_guard_active && "nested SdGuard not allowed");
    _guard_active = true;

    GetHAL().lvglLock();
    enter_sd_mode();
}

SdGuard::~SdGuard()
{
    exit_sd_mode();
    GetHAL().lvglUnlock();
    _guard_active = false;
}

bool SdGuard::ensureMounted()
{
    if (_mounted) {
        return true;
    }
    _mounted = mount_sd();
    return _mounted;
}

bool SdGuard::isMounted()
{
    return _mounted;
}

bool SdGuard::isInserted()
{
    return hal_bridge::board_is_sd_inserted();
}

}  // namespace stackchan::hal
