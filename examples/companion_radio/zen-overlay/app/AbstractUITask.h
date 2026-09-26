#pragma once
#include <helpers/ContactInfo.h>

#include <MeshCore.h>
#include <helpers/ui/ZenDisplayDriver.h>
#include <helpers/ui/ZenUIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/ZenBuzzer.h>
#endif

#include "ZenPrefs.h"
#include "zen/OperationResult.h"
#include "zen/TimeSyncSource.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    advertReceivedFlood,
    advertReceivedZeroHop,
    newContact,
    ack
};

class AbstractUITask {
protected:
  mesh::MainBoard* _board;
  MultiSerialInterface* _interfaceManager;
  BaseSerialInterface* _bluetoothInterface;
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager,
                 BaseSerialInterface* bluetoothInterface)
      : _board(board), _interfaceManager(interfaceManager),
        _bluetoothInterface(bluetoothInterface) {
    _connected = false;
  }

public:
  enum PrefsRuntimeEffect : uint16_t {
    PREFS_APPLY_NONE       = 0,
    PREFS_APPLY_GPS        = 1 << 0,
    PREFS_APPLY_BLUETOOTH  = 1 << 1,
    PREFS_APPLY_BRIGHTNESS = 1 << 2,
    PREFS_APPLY_ROTATION   = 1 << 3,
    PREFS_APPLY_EINK       = 1 << 4,
    PREFS_APPLY_BUZZER     = 1 << 5,
    PREFS_APPLY_ALL        = 0xFFFF
  };
  // Restricted-mode state exposed as policy context to mesh-side Zen services.
  // A UI that does not explicitly expose a parent-unlocked session fails
  // closed when mesh-side code sees child_mode_enabled in persisted prefs.
  virtual bool isChildModeRestricted() const { return true; }
  // On-device history is a separate trust boundary from the companion offline
  // queue. Restricted UIs override these predicates; baseline transports and
  // protocol acknowledgements continue unchanged.
  virtual bool allowOnDeviceContactMessage(const ContactInfo&) const { return true; }
  virtual bool allowOnDeviceChannelMessage(uint8_t) const { return true; }
  void setHasConnection(bool connected) {
    bool prev = _connected;
    _connected = connected;
    if (prev && !connected) onBLEDisconnected();
  }
  bool hasConnection() const { return _connected; }
  virtual void onBLEDisconnected() {}
  // An end-to-end ACK (CRC) arrived for one of our sent messages — drives the
  // DM delivery-status marker. Default no-op for UIs that don't track it.
  virtual void onMsgAck(uint32_t ack_crc) { (void)ack_crc; }
  virtual void onNodeLoginCancelled(const uint8_t* prefix) { (void)prefix; }
  // A repeater rebroadcast of one of our channel sends was heard (seq from
  // lastChannelRelaySeq()) — drives the channel "relayed into mesh" marker.
  virtual void onChannelRelayed(uint32_t seq) { (void)seq; }
  virtual void onChannelRelayExpired(uint32_t seq, uint8_t heard, bool transmitted) {
    (void)seq; (void)heard; (void)transmitted;
  }
  // Result of an on-device-UI-triggered MyMesh::sendNodeLogin() arrived.
  // pub_key is the contact's key prefix (>=4 bytes valid); permissions is the
  // remote node ACL byte (only meaningful when success is true).
  virtual void onNodeLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) { (void)pub_key; (void)success; (void)permissions; }
  virtual void onOperationResult(const zen::OperationResult& result) {
    (void)result;
  }
  virtual void onTimeSynchronized(zen::TimeSyncSource source) { (void)source; }
  // A preference transaction failed and MyMesh restored the last committed
  // values. Rich UIs re-apply any hardware state they previewed while editing.
  virtual void onPrefsRestored() { }
  // The shared MeshCore preference record was committed successfully. Runtime
  // owners reconcile peripherals here so BLE, CLI and on-device edits all use
  // the same persistence-then-apply ordering.
  virtual void onPrefsCommitted(uint16_t effects) { (void)effects; }
  // Text reply to an on-device-UI-triggered MyMesh::sendAdminCommand() arrived
  // (see AdminScreen). pub_key is the contact's key prefix (>=4 bytes valid).
  virtual void onAdminReply(const uint8_t* pub_key, const char* text) { (void)pub_key; (void)text; }
  virtual void onSensorTelemetry() { }
  virtual void onNewContact(const ContactInfo& contact) { (void)contact; }
  // True only when a BLE central is actually bonded/connected.
  bool isBLEConnected() const {
    return _bluetoothInterface && _bluetoothInterface->isEnabled() &&
           _bluetoothInterface->isConnected();
  }
  // True when the Bluetooth companion app is connected. For app-connected
  // behaviour like Auto buzzer mute.
  bool isClientConnected() const { return _interfaceManager->isConnected(); }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  bool isCompanionInterfaceEnabled() const { return _interfaceManager->isEnabled(); }
  virtual void msgRead(int msgcount) = 0;
  virtual void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, uint8_t contact_type = 0, const uint8_t* pub_key = nullptr) = 0;
  virtual void notify(UIEventType t = UIEventType::none) = 0;
  // Context-rich receive hook. The default preserves the legacy UI behaviour;
  // richer UIs can make one policy decision for alerts, sound, vibration and
  // wake without pushing UI concerns into the mesh receive/ACK path.
  virtual void incomingMessage(UIEventType event, uint8_t path_len,
                               const char* from_name, const char* text, int msgcount,
                               uint8_t contact_type = 0, const uint8_t* pub_key = nullptr,
                               int channel_idx = -1) {
    (void)channel_idx;
    newMsg(path_len, from_name, text, msgcount, contact_type, pub_key);
    notify(event);
  }
  virtual void addChannelMsg(uint8_t channel_idx, const char* text, uint32_t timestamp = 0) {}
  virtual bool addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text, uint32_t sender_timestamp = 0) { return false; }
  virtual int addOwnChannelMsg(uint8_t channel_idx, const char* text,
                               int text_len, uint32_t timestamp) { return -1; }
  virtual void armChannelRelay(int history_pos, uint32_t seq) {}
  // A contact is gone — removed explicitly (companion app / CLI command) or
  // silently auto-evicted to make room when the contact table is full. Lets
  // UI state that references contacts by pubkey (favourite slots, the
  // Locator/Live Share target) drop a reference that would otherwise dangle.
  // Default no-op.
  virtual void onContactRemoved(const uint8_t* pub_key) {}
  // A channel slot was cleared (companion app set it to an empty secret).
  // Drop any setting that referenced it by index — otherwise a new channel
  // added later at the same slot would silently inherit the old one's bot/
  // share target or notification melody. Default no-op.
  virtual void onChannelRemoved(uint8_t channel_idx) {}
  // Controlled power transitions must pass through the UI so staged settings,
  // preferences and debounced contacts are committed before power is removed.
  virtual void shutdown(bool restart = false) = 0;
  virtual void loop() = 0;
};
