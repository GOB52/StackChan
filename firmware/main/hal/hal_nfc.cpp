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
//
// Refactor: poll task は phase 別の helper 関数に分割
//   poll_once -> process_picc -> read_ndef_with_retry / parse_tlvs_and_collect
// 案 R (read miss retry): reactivate→ndefRead を最大 (1 + MAX_RETRY) 回試行。
#include "hal.h"
#include "board/hal_bridge.h"
#include "../stackchan/gob_fork_nvs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mooncake_log.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include <M5UnitUnified.h>
#include <M5UnitUnifiedNFC.h>
#include "nfc/a/nfca.hpp"

static const std::string_view _tag = "HAL-NFC";

namespace {
std::unique_ptr<m5::unit::UnitUnified>     _units;
std::unique_ptr<m5::unit::UnitNFC>         _unit_nfc;
std::unique_ptr<m5::nfc::NFCLayerA>        _nfc_a;

constexpr const char* _STACKCHAN_CMD_MIME = "application/vnd.stackchan.cmd+json";

// True while the poll task is mid-iteration (units->update / detect /
// identify / NDEF read). FT6336 esp_timer (core 0) checks this and skips
// its 20ms touch poll to avoid cross-core I2C contention with this task
// (core 1). Atomic so cross-core load/store is well defined.
std::atomic<bool> _nfc_busy{false};

// 案 R: NDEF read transient miss を救う。失敗時に deactivate → reactivate →
// ndefRead を最大 MAX_RETRY 回追加試行。RF flux 弱 / I2C 競合 / tag 動き等で
// 1 度の read miss が発生しても、次の cycle を待たずに recover できる。
// 失敗時の追加コスト: 1 retry あたり ~70ms (deactivate + reactivate + ndefRead)。
//
// 案 E: 発声中は retry を行わない (1 回のみ試行)。
// 理由: TTS 発声中はスピーカー電源負荷 + EMI で NFC RF が劣化し、20ms 程度の
// retry 間隔では tag が応答を回復しない。retry が AFE feed task を ~250ms
// starve させて Ringbuffer full を引き起こすが、発声中は救えないので副作用のみ。
// 非発声時は従来通り MAX_RETRY=2 で recover を狙う。
bool read_ndef_with_retry(m5::nfc::a::PICC& u, std::vector<m5::nfc::ndef::TLV>& tlvs)
{
    const int max_retry = hal_bridge::is_xiaozhi_speaking() ? 1 : 3;
    for (int i = 0; i <= max_retry; i++) {
        if (_nfc_a->ndefRead(tlvs)) {
            if (i > 0) {
                mclog::tagInfo(_tag, "NDEF read recovered on retry {}/{}", i, max_retry);
            }
            return true;
        }
        if (i < max_retry) {
            mclog::tagWarn(_tag, "NDEF read miss, retry {}/{}", i + 1, max_retry);
            _nfc_a->deactivate();
            //vTaskDelay(pdMS_TO_TICKS(20));  // tag RF 安定化待ち
            if (!_nfc_a->reactivate(u)) {
                mclog::tagWarn(_tag, "reactivate failed during NDEF read retry");
                return false;
            }
        }
    }
    return false;
}

// TLV/record loop を集約。stackchan:cmd MIME record を cmd_events に追加。
// 各 skip 経路には警告ログを残し、シリアルで原因が追えるようにする。
void parse_tlvs_and_collect(const std::vector<m5::nfc::ndef::TLV>& tlvs,
                            const std::string& uid,
                            std::vector<NfcCmdEvent_t>& cmd_events)
{
    size_t msg_tlvs = 0, total_records = 0, match_count = 0;
    for (auto& tlv : tlvs) {
        if (!tlv.isMessageTLV()) {
            mclog::tagInfo(_tag, "skip non-Message TLV (tag={})",
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
                               tnf_val, rec_type, rec.payloadSize());
                continue;
            }
            if (rec_type != _STACKCHAN_CMD_MIME) {
                mclog::tagWarn(_tag,
                               "record skipped: MIME type '{}' "
                               "!= '{}' bytes={}",
                               rec_type, _STACKCHAN_CMD_MIME, rec.payloadSize());
                continue;
            }
            NfcCmdEvent_t cev{
                uid,
                std::vector<uint8_t>(rec.payload(),
                                     rec.payload() + rec.payloadSize())};
            mclog::tagInfo(_tag, "stackchan:cmd received uid={} bytes={}",
                           cev.uid, cev.payload.size());
            cmd_events.push_back(std::move(cev));
            match_count++;
        }
    }
    if (match_count == 0) {
        mclog::tagWarn(_tag,
                       "no stackchan:cmd record on tag uid={} "
                       "(message_tlvs={} records={})",
                       uid, msg_tlvs, total_records);
    }
}

// 1 PICC 単位の処理: identify → emit tag (pending) → reactivate → NDEF read →
// parse して cmd_events (pending) に追加。途中失敗は warn ログ + early return。
void process_picc(m5::nfc::a::PICC& u,
                  std::vector<NfcTagEvent_t>& tag_events,
                  std::vector<NfcCmdEvent_t>& cmd_events)
{
    if (!_nfc_a->identify(u)) {
        mclog::tagWarn(_tag, "PICC identify failed uid={}", u.uidAsString());
        return;
    }
    NfcTagEvent_t ev{u.uidAsString(), u.typeAsString()};
    mclog::tagInfo(_tag, "PICC detected uid={} type={}", ev.uid, ev.type);
    tag_events.push_back(ev);

    // identify は成功時 PICC を halt するので reactivate が必須。
    // 失敗 = tag 既に外された等、このサイクルでの NDEF read は諦める。
    if (!_nfc_a->reactivate(u)) {
        mclog::tagWarn(_tag, "reactivate failed after identify uid={}", ev.uid);
        return;
    }

    if (!u.supportsNDEF()) {
        mclog::tagWarn(_tag,
                       "NDEF skipped: PICC type '{}' does not support NDEF",
                       ev.type);
        return;
    }

    std::vector<m5::nfc::ndef::TLV> tlvs;
    mclog::tagInfo(_tag, "NDEF read start uid={}", ev.uid);
    if (!read_ndef_with_retry(u, tlvs)) {
        mclog::tagWarn(_tag, "NDEF read failed uid={} (after retries)", ev.uid);
        return;
    }
    if (tlvs.empty()) {
        mclog::tagWarn(_tag, "NDEF read OK but no TLVs uid={}", ev.uid);
        return;
    }
    parse_tlvs_and_collect(tlvs, ev.uid, cmd_events);
}

// 1 cycle 分の poll: units->update → detect → 全 PICC 処理 → deactivate。
// 案 X': detect timeout 50ms (default 1000ms)。1 秒待つと busy ratio が
// 80%+ になり FT6336 touch poll が starve するため。
void poll_once(std::vector<NfcTagEvent_t>& tag_events,
               std::vector<NfcCmdEvent_t>& cmd_events)
{
    _units->update();

    std::vector<m5::nfc::a::PICC> piccs;
    if (!_nfc_a->detect(piccs, 50)) {
        return;
    }
    for (auto& u : piccs) {
        process_picc(u, tag_events, cmd_events);
    }
    _nfc_a->deactivate();
}

}  // namespace

bool Hal::isNfcBusy()
{
    return _nfc_busy.load(std::memory_order_acquire);
}

static void _nfc_task(void* /*param*/)
{
    while (1) {
        if (_units && _unit_nfc && _nfc_a) {
            // 案 N2: emit を NFC busy 終了後に集約する。handler 内の重い処理
            // (LVGL toast / OGG demux 等) が busy 期間を伸ばさないように。
            // 安全性: handler は move/setColor/Toast/PlaySound いずれも I2C 非使用。
            std::vector<NfcTagEvent_t> tag_events;
            std::vector<NfcCmdEvent_t> cmd_events;

            _nfc_busy.store(true, std::memory_order_release);
            poll_once(tag_events, cmd_events);
            _nfc_busy.store(false, std::memory_order_release);

            for (const auto& ev : tag_events) {
                GetHAL().onNfcTagDetected.emit(ev);
            }
            for (const auto& cev : cmd_events) {
                GetHAL().onNfcCmdReceived.emit(cev);
            }
        }
        // detect timeout 50ms (案 X') により busy 期間短縮、poll 周期 100ms。
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
    mclog::tagInfo(_tag, "UnitNFC ready, starting poll task (core1, prio1, 100ms, detect=50ms)");

    // Pin to core 1 to avoid CPU starvation during AI speech: xiaozhi's audio
    // AEC tasks on core 0 run at high priority and preempt this low-prio NFC
    // poll task continuously while TTS is active, making PICCs undetectable.
    // FT6336 abort protection that previously required core 0 colocation is
    // now provided by the xiaozhi-esp32.patch i2c_device.cc retry-on-timeout
    // patch (SW1), so core 1 is safe.
    xTaskCreatePinnedToCore(_nfc_task, "nfc", 6144, NULL, 1, NULL, 1);
}
