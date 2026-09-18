#pragma once
#include <helpers/ContactInfo.h>

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/MultiSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"

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
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, MultiSerialInterface* interfaceManager) : _board(board), _interfaceManager(interfaceManager) {
    _connected = false;
  }

public:
  // Restricted-mode state exposed as policy context to mesh-side Solo services.
  // A UI that does not explicitly expose a parent-unlocked session fails
  // closed when mesh-side code sees child_mode_enabled in persisted prefs.
  virtual bool isChildModeRestricted() const { return true; }
  // On-device history is a separate trust boundary from the app/USB offline
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
  // Return the four-byte contact prefix when an on-device send owns this ACK.
  virtual bool matchMsgAck(uint32_t ack_crc, uint8_t* prefix) {
    onMsgAck(ack_crc);
    (void)prefix;
    return false;
  }
  // A repeater rebroadcast of one of our channel sends was heard (seq from
  // lastChannelRelaySeq()) — drives the channel "relayed into mesh" marker.
  virtual void onChannelRelayed(uint32_t seq) { (void)seq; }
  virtual void onChannelRelayExpired(uint32_t seq) { (void)seq; }
  // Result of an on-device-UI-triggered MyMesh::sendNodeLogin() arrived.
  // pub_key is the contact's key prefix (>=4 bytes valid); permissions is the
  // remote node ACL byte (only meaningful when success is true).
  virtual void onNodeLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) { (void)pub_key; (void)success; (void)permissions; }
  virtual void onOperationFailure(const char* operation, const char* reason) {
    (void)operation; (void)reason;
  }
  // Text reply to an on-device-UI-triggered MyMesh::sendAdminCommand() arrived
  // (see AdminScreen). pub_key is the contact's key prefix (>=4 bytes valid).
  virtual void onAdminReply(const uint8_t* pub_key, const char* text) { (void)pub_key; (void)text; }
  virtual void onSensorTelemetry() { }
  virtual void onNewContact(const ContactInfo& contact) { (void)contact; }
  // True only when a BLE central is actually bonded/connected. On a dual
  // (BLE+USB) interface hasConnection() is always true (USB counts), so use
  // this for BLE-specific UI like the pairing-PIN prompt.
  bool isBLEConnected() const { return _interfaceManager->isBluetoothConnected(); }
  // True when a companion app is connected over any transport (BLE bonded or an
  // open USB-CDC port). For app-connected behaviour like Auto buzzer mute.
  bool isClientConnected() const { return _interfaceManager->isClientConnected(); }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isBluetoothEnabled() const { return _interfaceManager->isBluetoothEnabled(); }
  void enableBluetooth() { _interfaceManager->enableBluetooth(); }
  void disableBluetooth() { _interfaceManager->disableBluetooth(); }
  // Compatibility names retained for Solo screens and Child Mode. These
  // control every companion transport, matching the old dual-interface wrapper.
  bool isSerialEnabled() const { return _interfaceManager->isEnabled(); }
  void enableSerial() { _interfaceManager->enable(); }
  void disableSerial() { _interfaceManager->disable(); }
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
  // App/USB sends are mirrored after the mesh accepts them. The app owns retry
  // scheduling, so UI implementations must track status without transmitting.
  virtual void addAppDMMsg(const uint8_t* pub_key, const char* text,
                           uint32_t timestamp, uint8_t attempt,
                           uint32_t ack_tag, uint32_t ack_deadline_ms,
                           uint8_t path_len) {}
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
