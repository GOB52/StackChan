// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
//
// Phase 1a — M5Unit-NFC integration via M5UnitUnified ESP-IDF native I2C.
// Polls NFC-A PICCs on the existing I2C master bus shared with BMI270/RTC,
// emits NfcTagEvent_t with UID + classified type.
//
// Phase 1b — Additionally read NDEF on identified tags and emit
// NfcCmdEvent_t for each NDEF MIME record whose type is
// "application/vnd.stackchan.cmd+json" (RFC 6838 vendor tree).
// JSON parsing and dispatch are deferred to Phase 1c.
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
#include "nfc/layer/ndef_layer.hpp"
#include "nfc/a/nfca.hpp"

static const std::string_view _tag = "HAL-NFC";

namespace {
std::unique_ptr<m5::unit::UnitUnified>     _units;
std::unique_ptr<m5::unit::UnitNFC>         _unit_nfc;
std::unique_ptr<m5::nfc::NFCLayerA>        _nfc_a;
std::unique_ptr<m5::nfc::ndef::NDEFLayer>  _ndef;

constexpr const char* _STACKCHAN_CMD_MIME = "application/vnd.stackchan.cmd+json";
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

                        // NFCLayerA::identify() halts the PICC on success; any
                        // subsequent operation (NDEF read / dump) requires an
                        // explicit reactivate first. Skip this PICC if wakeup
                        // fails (tag removed mid-cycle, etc.).
                        if (!_nfc_a->reactivate(u)) {
                            mclog::tagWarn(_tag,
                                           "reactivate failed after identify uid={}",
                                           ev.uid);
                            continue;
                        }

                        // Phase 1b: read NDEF and emit any record matching the
                        // stackchan:cmd MIME type. Each branch is logged so the
                        // serial monitor shows why a tag did not trigger emit.
                        auto ftag = m5::nfc::a::get_nfc_forum_tag_type(u.type);
                        if (!_ndef) {
                            mclog::tagError(_tag, "NDEF layer not initialized");
                        } else if (ftag == m5::nfc::NFCForumTag::None) {
                            mclog::tagWarn(_tag,
                                           "NDEF skipped: PICC type '{}' is not an NFC Forum Tag",
                                           ev.type);
                        } else {
                            std::vector<m5::nfc::ndef::TLV> tlvs;
                            mclog::tagInfo(_tag,
                                           "NDEF read start uid={} ftag={}",
                                           ev.uid,
                                           static_cast<int>(ftag));
                            if (!_ndef->read(ftag, tlvs)) {
                                mclog::tagWarn(_tag,
                                               "NDEF read failed uid={} (tag not formatted "
                                               "or transient I2C error)",
                                               ev.uid);
                            } else if (tlvs.empty()) {
                                mclog::tagWarn(_tag, "NDEF read OK but no TLVs uid={}", ev.uid);
                            } else {
                                size_t msg_tlvs = 0, total_records = 0, match_count = 0;
                                for (auto& tlv : tlvs) {
                                    if (!tlv.isMessageTLV()) {
                                        mclog::tagInfo(_tag,
                                                       "skip non-Message TLV (tag={})",
                                                       static_cast<int>(tlv.tag()));
                                        continue;
                                    }
                                    msg_tlvs++;
                                    for (auto& rec : tlv.records()) {
                                        total_records++;
                                        const std::string rec_type(rec.type());
                                        const int         tnf_val = static_cast<int>(rec.tnf());
                                        if (rec.tnf() != m5::nfc::ndef::TNF::MIMEMedia) {
                                            mclog::tagWarn(_tag,
                                                           "record skipped: TNF={} (need MIME=2) "
                                                           "type='{}' bytes={}",
                                                           tnf_val,
                                                           rec_type,
                                                           rec.payloadSize());
                                            continue;
                                        }
                                        if (rec_type != _STACKCHAN_CMD_MIME) {
                                            mclog::tagWarn(_tag,
                                                           "record skipped: MIME type '{}' "
                                                           "!= '{}' bytes={}",
                                                           rec_type,
                                                           _STACKCHAN_CMD_MIME,
                                                           rec.payloadSize());
                                            continue;
                                        }
                                        NfcCmdEvent_t cev{
                                            ev.uid,
                                            std::vector<uint8_t>(rec.payload(),
                                                                 rec.payload() + rec.payloadSize())};
                                        mclog::tagInfo(_tag,
                                                       "stackchan:cmd received uid={} bytes={}",
                                                       cev.uid,
                                                       cev.payload.size());
                                        GetHAL().onNfcCmdReceived.emit(cev);
                                        match_count++;
                                    }
                                }
                                if (match_count == 0) {
                                    mclog::tagWarn(_tag,
                                                   "no stackchan:cmd record on tag uid={} "
                                                   "(message_tlvs={} records={})",
                                                   ev.uid,
                                                   msg_tlvs,
                                                   total_records);
                                }
                            }
                        }
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
    _ndef  = std::make_unique<m5::nfc::ndef::NDEFLayer>(*_nfc_a);
    mclog::tagInfo(_tag, "UnitNFC ready, starting poll task (core0, prio1, 1000ms)");

    // Pin to core 0 so std::this_thread::yield() inside ST25R3916 polling loops
    // can let the FT6336 esp_timer callback (also on core 0) preempt and access
    // the shared I2C bus. Priority 1 keeps xiaozhi audio (high prio) ahead.
    xTaskCreatePinnedToCore(_nfc_task, "nfc", 6144, NULL, 1, NULL, 0);
}
