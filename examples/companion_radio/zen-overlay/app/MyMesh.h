#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"
#include "zen/ZenPolicy.h"
#include "zen/RemoteLoginAdapter.h"
#include "zen/RemoteTelemetryAdapter.h"
#include "zen/AdminSession.h"
#include "zen/SensorTelemetry.h"
#include "zen/RepeaterSignalMonitor.h"
#include "zen/TimezonePolicy.h"
#include "zen/ConfigMaintenance.h"
#include "zen/FloodScopeView.h"
#include "zen/RelayEchoTiming.h"
#include "zen/ChannelSlotPolicy.h"
#include "zen/GpsService.h"
#include "zen/TransportTrace.h"
#include "zen/BaselinePrefsFingerprint.h"
#include <helpers/ui/ZenDisplayDriver.h>

// Forward declaration for UITask
class UITask;

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 13

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "14 Aug 2026"
#endif

// Zen release version. The underlying MeshCore protocol/base version is
// reported separately through the MESHCORE_VERSION build flag.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v2.2.2"
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "ZenPrefs.h"
#include "zen/ZenStore.h"

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

// LORA_FREQ/BW/SF/CR fallbacks now live in ZenPrefs.h (DataStore.cpp needs them too).
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

struct DiscoverResult {
  char    name[32];   // contact name if known, "" if unknown (use type label)
  uint8_t type;       // ADV_TYPE_REPEATER / ADV_TYPE_SENSOR / ADV_TYPE_ROOM
  bool    is_known;   // true = in contacts[], false = new unknown node
  int8_t  rssi;       // RSSI of the response as received by us (dBm)
  int8_t  snr_x4;     // SNR of the response as received by us (dB × 4)
  int8_t  remote_snr_x4; // SNR at which responder heard our request (dB × 4)
  uint8_t pub_key[PUB_KEY_SIZE];
  uint32_t timestamp;
};

#define EXPECTED_ACK_TABLE_SIZE 8

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  using FloodScopeState = zen::FloodScopeView::State;

  struct StorageStatus {
    bool available;
    bool low_space;
    uint32_t used_kb;
    uint32_t total_kb;
    uint32_t free_bytes;
  };
  StorageStatus getStorageStatus(bool contacts_channels) const {
    // MeshCore remains the sole owner of both filesystems. Diagnostics use its
    // public accounting API and never traverse or mutate baseline files.
    (void)contacts_channels;
    uint32_t used = _store->getStorageUsedKb();
    uint32_t total = _store->getStorageTotalKb();
    return {total != 0, total > used && (total - used) < 8,
            used, total, total > used ? (total - used) * 1024UL : 0};
  }
  FloodScopeState getFloodScopeState() const {
    TransportKey key;
    memcpy(key.key, _prefs.default_scope_key, sizeof(key.key));
    return zen::FloodScopeView::state(send_unscoped, !send_scope.isNull(),
                                      _prefs.default_scope_name[0] != '\0' && !key.isNull());
  }
  const char* getDefaultFloodScopeName() const { return _prefs.default_scope_name; }
  bool hasTemporaryFloodScopeOverride() const {
    return send_unscoped || !send_scope.isNull();
  }
  bool useDefaultFloodScopeNow();
  bool setDefaultFloodScope(const char* name, const uint8_t* key);
  bool setDefaultFloodScopeName(const char* name);
  bool hasDefaultFloodScope() const {
    TransportKey key;
    memcpy(key.key, _prefs.default_scope_key, sizeof(key.key));
    return _prefs.default_scope_name[0] != '\0' && !key.isNull();
  }

  // Zen power policy remains an application overlay. It must not add states to
  // MeshCore's Dispatcher or alter the baseline receive/transmit lifecycle.
  void applyPowerState(bool low_power, bool emergency);
  bool isLowPowerMode() const { return _low_power_mode; }
  bool isEmergencyMode() const { return _emergency_mode; }
  bool radioAvailable() const { return !_low_power_mode || _emergency_mode; }
  int sendMessage(const ContactInfo& recipient, uint32_t timestamp, uint8_t attempt,
                  const char* text, uint32_t& expected_ack, uint32_t& est_timeout) {
    return !radioAvailable() ? MSG_SEND_FAILED
                           : BaseChatMesh::sendMessage(recipient, timestamp, attempt, text,
                                                       expected_ack, est_timeout);
  }
  int sendUIMessage(const ContactInfo& recipient, uint32_t timestamp, uint8_t attempt,
                    const char* text, uint32_t& expected_ack, uint32_t& est_timeout);
  bool sendGroupMessage(uint32_t timestamp, mesh::GroupChannel& channel,
                        const char* sender_name, const char* text, int text_len) {
    return radioAvailable() && BaseChatMesh::sendGroupMessage(timestamp, channel,
                                                               sender_name, text, text_len);
  }
  int sendRequest(const ContactInfo& recipient, uint8_t req_type,
                  uint32_t& tag, uint32_t& est_timeout) {
    return !radioAvailable() ? MSG_SEND_FAILED
                           : BaseChatMesh::sendRequest(recipient, req_type, tag, est_timeout);
  }
  int sendRequest(const ContactInfo& recipient, const uint8_t* data, uint8_t len,
                  uint32_t& tag, uint32_t& est_timeout) {
    return !radioAvailable() ? MSG_SEND_FAILED
                           : BaseChatMesh::sendRequest(recipient, data, len, tag, est_timeout);
  }
  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  ZenPrefs *getNodePrefs();
  uint32_t getBLEPin();
  const zen::TransportTrace& transportTrace() const { return _transport_trace; }
  void clearTransportTrace() { _transport_trace.clear(); }
  bool identityLoadedAtBoot() const { return _identity_loaded_at_boot; }
  void getPublicKeyPrefix(char* out, size_t size) const {
    if (!out || size < 9) return;
    mesh::Utils::toHex(out, self_id.pub_key, 4);
  }

  // MeshCore's iterator is the authoritative view of persisted contacts. The
  // baseline indexed accessor addresses internal slots, including anonymous
  // request scratch entries, so Zen exposes a zero-based UI view without
  // changing BaseChatMesh.
  bool getContactByIdx(uint32_t idx, ContactInfo& contact) {
    ContactsIterator iterator = startContactsIterator();
    for (uint32_t current = 0; iterator.hasNext(this, contact); current++)
      if (current == idx) return true;
    return false;
  }

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  bool advertIndicatorActive() const;
  bool sendNodeDiscoverReq(bool silent = false);
  void onUserDisplayWake();
  uint8_t repeaterSignalBars() const {
    return _repeater_signal.bars(_prefs.sf, millis(), radioAvailable());
  }
  void enterCLIRescue();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);
  int  getDiscoverResults(DiscoverResult dest[], int max_count);

  // On-device contact management — lets Nearby Nodes add a discovered node or
  // delete a contact without the phone app. Mirrors the CMD_ADD/REMOVE paths.
  bool addDiscoveredContact(const uint8_t* pub_key, const char* name, uint8_t type);
  bool deleteContactByKey(const uint8_t* pub_key);
  // Change the MeshCore favourite flag used by app sync, list filters and
  // Child Mode. Carousel pins are intentionally stored separately in prefs.
  bool setContactFavourite(const uint8_t* pub_key, bool favourite);

  // Ping/Trace functionality
  #define PING_RESULT_MAX 4
  typedef void (*PingCallback)(uint32_t tag, int16_t snr_out_x4, int16_t snr_back_x4, uint32_t rtt_ms);
  
  struct PingResult {
    uint32_t tag;
    uint32_t auth_code;
    int16_t snr_out_x4;    // SNR out (to first hop) × 4
    int16_t snr_back_x4;   // SNR back (from last hop to us) × 4
    uint32_t rtt_ms;       // Round-trip time in milliseconds
    bool received;
    unsigned long sent_ms;
  };
  
  uint32_t sendPing(const uint8_t* dest_pubkey, uint8_t hash_width = 1);
  void setPingCallback(PingCallback cb, void* arg);
  void clearPingResult(uint32_t tag);
  PingResult* getPingResult(uint32_t tag);
  PingCallback getPingCallback() const { return _ping_callback; }
  AbstractUITask* getUITask() const { return _ui; }

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;
  bool isRepeatLooped(const mesh::Packet* packet) const;
  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;
  void logTx(mesh::Packet* packet, int len) override;
  void logTxFail(mesh::Packet* packet, int len) override;
  void trackRelaySend(const mesh::Packet* pkt);   // arm the UI relayed-into-mesh tracker
public:
  // Seq of the most recently tracked channel send — the UI records it on the
  // outgoing history entry so a heard echo (onChannelRelayed) can match it back.
  uint32_t lastChannelRelaySeq() const { return _last_relay_seq; }
private:
  bool childRestrictionsActive() const;
  bool childAllowsContact(const ContactInfo& contact, uint8_t expected_type) const;
  bool childAllowsChannel(uint8_t channel_idx);

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override { return setChannel(channel_idx, ch); }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  // Local sensor view uses an independent request; never clear app requests.
  zen::SensorTelemetry sensorTelemetry;
  enum SensorReplyState { SENSOR_IDLE, SENSOR_WAITING, SENSOR_READY, SENSOR_FAILED };
  SensorReplyState sensorReplyState() const { return _sensor_state; }
  void cancelSensorTelemetry() { _sensor_state = SENSOR_IDLE; sensorTelemetry.clear(); }
  void failSensorTelemetry() { _sensor_state = SENSOR_FAILED; }
  bool requestSensorTelemetry(const uint8_t* key, uint32_t* est_timeout = nullptr,
                              bool force_flood = false) {
    cancelSensorTelemetry();
    ContactInfo* contact = lookupContactByPubKey(key, PUB_KEY_SIZE);
    uint32_t timeout;
    ContactInfo route;
    if (!contact || contact->type != ADV_TYPE_SENSOR) {
      _sensor_state = SENSOR_FAILED;
      return false;
    }
    route = *contact;
    if (force_flood) route.out_path_len = OUT_PATH_UNKNOWN;
    if (sendRequest(route, REQ_TYPE_GET_TELEMETRY_DATA, _sensor_tag, timeout) == MSG_SEND_FAILED) {
      _transport_trace.add(zen::TransportTrace::SEND_REJECTED, key,
                           zen::TransportTrace::TELEMETRY_QUEUED);
      _sensor_state = SENSOR_FAILED;
      return false;
    }
    _transport_trace.add(zen::TransportTrace::TELEMETRY_QUEUED, key,
                         force_flood ? 1 : 0);
    memcpy(_sensor_key, key, PUB_KEY_SIZE);
    if (est_timeout) *est_timeout = timeout;
    _sensor_state = SENSOR_WAITING;
    return true;
  }

  // On-device UI login to a room or remotely managed node — no phone app required.
  // The remote node's ACL grants permission per-identity (self_id), not per
  // command source, so this reuses the same sendLogin() the BLE CMD_SEND_LOGIN
  // path uses; the async result lands in onContactResponse() and is pushed to
  // the UI via AbstractUITask::onNodeLoginResult().
  bool sendNodeLogin(const ContactInfo& contact, const char* password,
                     uint32_t& est_timeout, bool force_flood = false) {
    // UI requests must never cancel a companion-app request (or another UI login).
    // The app path deliberately retains its upstream single-request behaviour.
    // Only another login is ambiguous with this response. Status, telemetry
    // and binary requests have their own response matching and must not make a
    // standalone node login appear to fail merely because an app request is
    // still pending.
    if (_ui_login_pending || zen::RemoteLoginAdapter::appBusy(
          pending_login, _app_login_deadline, millis())) return false;
    // An unanswered app login must not indefinitely reserve the UI login path.
    pending_login = 0;
    ContactInfo route = contact;
    if (force_flood) route.out_path_len = OUT_PATH_UNKNOWN;
    if (sendLogin(route, password, est_timeout) == MSG_SEND_FAILED) {
      _transport_trace.add(zen::TransportTrace::SEND_REJECTED,
                           contact.id.pub_key, zen::TransportTrace::LOGIN_QUEUED);
      return false;
    }
    _transport_trace.add(zen::TransportTrace::LOGIN_QUEUED,
                         contact.id.pub_key, force_flood ? 1 : 0);
    memcpy(_ui_pending_login_key, contact.id.pub_key, PUB_KEY_SIZE);
    _ui_login_pending = true;
    return true;
  }

  // Pubkey-guarded cancellation for the UI coordinator. Late replies are then
  // ignored instead of being attributed to a newer attempt.
  void cancelUiPendingLogin(const uint8_t* pub_key) {
    if (_ui_login_pending &&
        memcmp(_ui_pending_login_key, pub_key, PUB_KEY_SIZE) == 0)
      _ui_login_pending = false;
  }

  // On-device UI logout: drops the local keep-alive tracking (mirrors the app's
  // CMD_LOGOUT) and forgets the saved password, so the next room open prompts
  // for credentials again instead of silently re-using them. No packet is sent
  // to the server -- the room ACL has no session state to tear down, this is
  // purely local "forget this login" bookkeeping.
  void logoutRoom(const uint8_t* pub_key) {
    stopConnection(pub_key);
    forgetRoomPassword(pub_key);
  }

  // On-device-saved room/repeater login passwords, persisted to flash (own
  // small file, independent of /contacts3) so a room that's already been
  // logged into doesn't need its password retyped after reboot.
  bool saveRoomPassword(const uint8_t* pub_key, const char* password);
  bool getRoomPassword(const uint8_t* pub_key, char* out_password, uint8_t max_len);
  bool forgetRoomPassword(const uint8_t* pub_key);

  // On-device channel add/edit/delete (Messages > Channels). Shares the exact
  // setChannel + saveChannels + onChannelRemoved-cleanup sequence the
  // CMD_SET_CHANNEL BLE handler already performs, so both paths stay in sync.
  enum ChannelSaveResult { CHANNEL_SAVED, CHANNEL_DUPLICATE, CHANNEL_INVALID_SLOT, CHANNEL_SAVE_FAILED };
  ChannelSaveResult setChannelLocal(uint8_t idx, const ChannelDetails& ch);

  // Local administration uses the same wire commands as the app, but owns
  // its pending reply independently. Always resolve the current routing data.
  void authorizeAdmin(const uint8_t* key) { _admin_session.authorize(key); }
  void cancelAdminCommand() { _admin_session.cancel(millis()); }
  void closeAdminSession() { _admin_session.close(millis()); }
  bool sendAdminCommand(const ContactInfo& contact, const char* cmd_text,
                        uint32_t& est_timeout, bool force_flood = false) {
    ContactInfo* current = lookupContactByPubKey(contact.id.pub_key, PUB_KEY_SIZE);
    if (!current || (_ui && _ui->isChildModeRestricted()) ||
        !_admin_session.ready(contact.id.pub_key, millis())) return false;
    char tagged[161];
    if (!_admin_session.formatRequest(tagged, sizeof(tagged), cmd_text)) return false;
    ContactInfo route = *current;
    if (force_flood) route.out_path_len = OUT_PATH_UNKNOWN;
    if (sendCommandData(route, rtc_clock.getCurrentTimeUnique(), 0, tagged, est_timeout) == MSG_SEND_FAILED) {
      _transport_trace.add(zen::TransportTrace::SEND_REJECTED,
                           contact.id.pub_key, zen::TransportTrace::ADMIN_QUEUED);
      return false;
    }
    _transport_trace.add(zen::TransportTrace::ADMIN_QUEUED,
                         contact.id.pub_key, force_flood ? 1 : 0);
    _admin_session.begin(millis(), est_timeout + 4000);
    return true;
  }

  bool saveBaselinePrefs() {
    _prefs.node_lat = sensors.node_lat;
    _prefs.node_lon = sensors.node_lon;
    uint64_t baseline = zen::BaselinePrefsFingerprint::calculate(_prefs);
    bool base_changed = !_baseline_fingerprint_valid ||
                        baseline != _baseline_fingerprint;
    bool base_ok = !base_changed || _store->savePrefs(_prefs);
    if (base_ok && base_changed) {
      _baseline_fingerprint = baseline;
      _baseline_fingerprint_valid = true;
    }
    if (!base_ok && _ui) _ui->onOperationResult(zen::OperationResult::make(
        zen::Operation::STORAGE, zen::OperationOutcome::TRANSPORT_FAILURE,
        zen::OperationReason::SAVE_FAILED, zen::RESULT_BACKGROUND));
    return base_ok;
  }
  bool saveBaselinePrefsFromCompanion() {
    // Companion commands own MeshCore preferences. Use the baseline DataStore
    // path directly and update only Zen's write-suppression fingerprint after
    // the baseline transaction has completed.
    _prefs.node_lat = sensors.node_lat;
    _prefs.node_lon = sensors.node_lon;
    bool ok = _store->savePrefs(_prefs);
    if (ok) {
      _baseline_fingerprint = zen::BaselinePrefsFingerprint::calculate(_prefs);
      _baseline_fingerprint_valid = true;
    }
    return ok;
  }
  bool saveBaselinePrefsFromCompanionAndReply() {
    bool ok = saveBaselinePrefsFromCompanion();
    if (ok) writeOKFrame();
    else writeErrFrame(5); // ERR_CODE_FILE_IO_ERROR in MyMesh.cpp
    return ok;
  }
  zen::OperationReason zenSaveFailureReason() const {
    switch (_zen_store.failure()) {
      case zen::ZenStore::Failure::WRITE_LOCKED:
        return zen::OperationReason::PREFS_STORE_LOCKED;
      case zen::ZenStore::Failure::TEMP_WRITE_FAILED:
        return zen::OperationReason::PREFS_TEMP_WRITE_FAILED;
      case zen::ZenStore::Failure::TEMP_VERIFY_FAILED:
        return zen::OperationReason::PREFS_TEMP_VERIFY_FAILED;
      case zen::ZenStore::Failure::BACKUP_FAILED:
        return zen::OperationReason::PREFS_BACKUP_FAILED;
      case zen::ZenStore::Failure::PROMOTION_VERIFY_FAILED:
        return zen::OperationReason::PREFS_VERIFY_FAILED;
      case zen::ZenStore::Failure::PROMOTION_FAILED:
        return zen::OperationReason::PREFS_PROMOTION_FAILED;
      case zen::ZenStore::Failure::FILESYSTEM_UNAVAILABLE:
      case zen::ZenStore::Failure::ENCODE_FAILED:
      case zen::ZenStore::Failure::RENAME_FAILED:
      case zen::ZenStore::Failure::NONE:
      default:
        return zen::OperationReason::SAVE_FAILED;
    }
  }
  zen::OperationOutcome zenSaveFailureOutcome() const {
    return _zen_store.failure() == zen::ZenStore::Failure::WRITE_LOCKED
             ? zen::OperationOutcome::REJECTED
             : (_zen_store.failure() == zen::ZenStore::Failure::TEMP_VERIFY_FAILED ||
                _zen_store.failure() == zen::ZenStore::Failure::PROMOTION_VERIFY_FAILED
                  ? zen::OperationOutcome::VERIFICATION_FAILED
                  : zen::OperationOutcome::TRANSPORT_FAILURE);
  }
  bool savePrefs() {
    // MeshCore and Zen persist independently. Baseline values are serialized
    // only by the original DataStore/NodePrefs implementation; ZenStore can
    // neither inspect nor modify those files.
    zen::ConfigMaintenance::apply(_prefs);
    bool base_ok = saveBaselinePrefs();
    bool zen_ok = _zen_store.save(_prefs);
    if (!zen_ok && _ui) {
      zen::OperationResult result = zen::OperationResult::make(
        zen::Operation::STORAGE, zenSaveFailureOutcome(), zenSaveFailureReason(),
        zen::RESULT_BACKGROUND);
      result.context.value = (int16_t)_zen_store.generation();
      _ui->onOperationResult(result);
    }
    return base_ok && zen_ok;
  }
  bool saveZenPrefs() {
    // Zen-only UI state must not cause a write to MeshCore's baseline
    // preferences. Keeping this path separate also prevents an unrelated
    // baseline save failure from being reported for a successful Zen change.
    zen::ConfigMaintenance::apply(_prefs);
    bool ok = _zen_store.save(_prefs);
    if (!ok && _ui) {
      zen::OperationResult result = zen::OperationResult::make(
        zen::Operation::STORAGE, zenSaveFailureOutcome(), zenSaveFailureReason(),
        zen::RESULT_BACKGROUND);
      result.context.value = (int16_t)_zen_store.generation();
      _ui->onOperationResult(result);
    }
    return ok;
  }
  bool zenPrefsIncompatible() const { return _zen_store.writeLocked(); }
  void resetZenFields(ZenPrefs& prefs) {
    uint8_t* first = &prefs.buzzer_volume;
    memset(first, 0, sizeof(ZenPrefs) - (size_t)(first - (uint8_t*)&prefs));
  }
  void flushDirtyContacts() {
    if (dirty_contacts_expiry) {
      saveContacts();
      dirty_contacts_expiry = 0;
    }
  }
  DataStore* getDataStore() const { return _store; }
  // Apply the companion radio parameters. Repeater mode shares this network.
  void applyRadioParams();

  bool isAckPending(uint32_t expected_ack) const {
    if (expected_ack == 0) return false;   // 0 marks an empty/cleared slot, not a real ACK
    for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++)
      if (expected_ack_table[i].ack != 0 && expected_ack_table[i].ack == expected_ack) return true;
    return false;
  }


#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    zen::GpsService::Configuration config;
    config.enabled = _prefs.gps_enabled != 0;
    config.adaptive = _prefs.gps_adaptive != 0;
    config.interval_seconds = _prefs.gps_interval;
    sensors.applyConfiguration(config);
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;

private:

  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  bool isValidClientRepeatFreq(uint32_t f) const;
  // helpers, short-cuts
  bool saveChannels() {
    _store->saveChannels(this);
    return true;
  }
  void saveContacts();

  DataStore* _store;
  zen::ZenStore _zen_store;
  ZenPrefs _prefs;
  uint64_t _baseline_fingerprint = 0;
  bool _baseline_fingerprint_valid = false;
  zen::TransportTrace _transport_trace;
  bool _identity_loaded_at_boot = false;
  uint32_t pending_login;
  bool _low_power_mode = false;
  bool _emergency_mode = false;
  uint32_t _app_login_deadline = 0;
  bool _ui_login_pending = false; // independent of app status/telemetry requests
  uint8_t _ui_pending_login_key[PUB_KEY_SIZE]{};
  char pending_login_pw[16];  // password of the in-flight app login, persisted on success for ADV_TYPE_ROOM (see saveRoomPassword)
  zen::AdminSession _admin_session;
  SensorReplyState _sensor_state = SENSOR_IDLE;
  uint8_t _sensor_key[PUB_KEY_SIZE] = {};
  uint32_t _sensor_tag = 0;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  // UI "relayed into mesh" tracker for channel sends. A small
  // ring so a quick burst of channel sends are each tracked (not just the latest).
  // Hashing on receive only runs while at least one slot is pending, so the hot
  // flood-recv path is untouched otherwise.
  static const int RELAY_RING = 4;
  struct RelaySlot {
    uint8_t  hash[MAX_HASH_SIZE];
    uint16_t len;
    uint32_t deadline;
    uint32_t seq;
    uint8_t  heard;
    bool     transmitted;
    bool     pending;
  };
  RelaySlot _relay[RELAY_RING];
  int      _relay_head;        // next ring slot to overwrite
  int      _relay_active;      // number of slots currently pending (cheap gate)
  uint32_t _relay_seq;         // monotonic id counter for tracked sends
  uint32_t _last_relay_seq;    // seq of the most recent tracked send (for the UI to record)
  bool send_unscoped;   // force un-scoped flood (instead of using send_scope)
  char cli_command[80];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;
  unsigned long _next_auto_advert_ms;
  unsigned long _advert_indicator_until_ms;
  mesh::Packet* createConfiguredSelfAdvert();
  bool sendConfiguredSelfAdvert(bool flood);
  void noteAdvertQueued();

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table

  #define DISCOVER_RESULTS_MAX 16
  DiscoverResult  _discover_results[DISCOVER_RESULTS_MAX];
  int             _discover_count;
  uint32_t        _pending_node_discover_tag;
  unsigned long   _pending_node_discover_until;
  bool            _pending_node_discover_silent;
  zen::RepeaterSignalMonitor _repeater_signal;

  // Dedup for NODE_DISCOVER_RESP copies heard more than once: the responder's
  // zero-hop direct copy and a re-flooded copy relayed by another repeater
  // carry different packet hashes, so the mesh duplicate filter passes both.
  // Keyed by (tag, responder pubkey prefix) with a short expiry — covers both
  // the standalone on-device scan and responses forwarded to the app (which
  // would otherwise list the same repeater twice).
  #define DISCOVER_SEEN_MAX 16
  struct DiscoverSeen { uint32_t tag; uint8_t pk[6]; unsigned long until; };
  DiscoverSeen _disc_seen[DISCOVER_SEEN_MAX];
  uint8_t      _disc_seen_head;
  bool isDupDiscoverResp(uint32_t tag, const uint8_t* pub_key);   // records when new

  // ── Ping/Trace state ──────────────────────────────────────────────────────
  PingResult _ping_results[PING_RESULT_MAX];
  PingCallback _ping_callback;
  void* _ping_callback_arg;
};

extern MyMesh the_mesh;
