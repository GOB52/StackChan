// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
//
// Phase 1a — M5Unit-NFC integration via M5UnitUnified ESP-IDF native I2C.
// Polls NFC-A PICCs on the existing I2C master bus shared with BMI270/RTC,
// emits NfcTagEvent_t with UID + classified type. NDEF / stackchan:cmd
// dispatch is deferred to Phase 1b / Phase 2.
#include "hal.h"
#include "board/hal_bridge.h"
#include "../stackchan/gob_fork_nvs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>
#include <memory>
#include <vector>

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>

static const std::string_view _tag = "HAL-NFC";

namespace {
std::unique_ptr<m5::unit::UnitUnified> _units;
std::unique_ptr<m5::unit::UnitNFC> _unit_nfc;
std::unique_ptr<m5::nfc::NFCLayerA> _nfc_a;
}  // namespace

static void _nfc_task(void* /*param*/)
{
    while (1) {
        if (_units && _unit_nfc && _nfc_a) {
            _units->update();

            std::vector<m5::nfc::a::PICC> piccs;
            if (_nfc_a->detect(piccs)) {
                for (auto& u : piccs) {
                    if (_nfc_a->identify(u)) {
                        NfcTagEvent_t ev{u.uidAsString(), u.typeAsString()};
                        mclog::tagInfo(_tag, "PICC detected uid={} type={}", ev.uid, ev.type);
                        GetHAL().onNfcTagDetected.emit(ev);
                    } else {
                        mclog::tagWarn(_tag, "PICC identify failed uid={}", u.uidAsString());
                    }
                }
                _nfc_a->deactivate();
            }
        }
        // runs at all when get_nfc_enabled() == true.
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Public entry: called from _stackchan_update_task once xiaozhi reports
// ready. Honors NVS opt-in (default OFF) so users explicitly enable NFC
// when they want it. Forwards to nfc_init() which holds the actual UnitNFC
// bring-up (Units.add + begin) and poll task creation.
void Hal::startNfc()
{
    if (!stackchan::gob_fork::get_nfc_enabled()) {
        mclog::tagInfo(_tag, "disabled (NVS opt-in)");
        return;
    }
    nfc_init();
}

void Hal::nfc_init()
{
    mclog::tagInfo(_tag, "init");

    auto i2c_bus = hal_bridge::board_get_i2c_bus();
    if (!i2c_bus) {
        mclog::tagError(_tag, "i2c bus not available");
        return;
    }

    _units    = std::make_unique<m5::unit::UnitUnified>();
    _unit_nfc = std::make_unique<m5::unit::UnitNFC>();

    if (!_units->add(*_unit_nfc, i2c_bus)) {
        mclog::tagError(_tag, "Units.add failed");
        _units.reset();
        _unit_nfc.reset();
        return;
    }
    if (!_units->begin()) {
        mclog::tagError(_tag, "Units.begin failed (no UnitNFC on I2C bus?)");
        _units.reset();
        _unit_nfc.reset();
        return;
    }

    _nfc_a = std::make_unique<m5::nfc::NFCLayerA>(*_unit_nfc);
    mclog::tagInfo(_tag, "UnitNFC ready, starting poll task (core0, prio1, 1000ms)");

    // Pin to core 0 so std::this_thread::yield() inside ST25R3916 polling loops
    // can let the FT6336 esp_timer callback (also on core 0) preempt and access
    // the shared I2C bus. Priority 1 keeps xiaozhi audio (high prio) ahead.
    xTaskCreatePinnedToCore(_nfc_task, "nfc", 6144, NULL, 1, NULL, 0);
}
