#pragma once

#include <MeshCore.h>
#include <helpers/ui/ZenDisplayDriver.h>
#include <helpers/ui/ZenUIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/ContactInfo.h>
#include "../zen/PathDetails.h"
#include <Arduino.h>
#include <helpers/sensors/LPPDataHelpers.h>

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

#ifdef PIN_BUZZER
  #include <helpers/ui/ZenBuzzer.h>
#endif
#ifdef PIN_VIBRATION
  #include <helpers/ui/GenericVibration.h>
#endif

#include "../AbstractUITask.h"
#include "../ZenPrefs.h"
#include "../zen/ZenRuntime.h"
#include "../zen/GpsMode.h"
#include "../zen/GpsService.h"
#include "../zen/TimeLocationCoordinator.h"
#include "../zen/LocalTimeService.h"
#include "../zen/NodeLoginCoordinator.h"
#include "../zen/RemoteNodeCoordinator.h"
#include "../zen/DiagnosticLog.h"
#include "../zen/OperationResultCoordinator.h"
#include "../zen/RemoteOperationResultAdapter.h"
#include "../zen/MessageUnreadCoordinator.h"
#include "../zen/NotificationCoordinator.h"
#include "../zen/NotificationProfiles.h"
#include "../zen/NotificationEligibility.h"
#include "../zen/NotificationPopupState.h"
#include "../zen/PeripheralPowerCoordinator.h"
#include "../zen/BluetoothPolicyAdapter.h"
#include "KeyboardWidget.h"
#include "ScreenHistory.h"
#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  #include <helpers/ui/CardKBController.h>
#endif

#include "../zen/BatteryPolicy.h"
#include "../zen/BatteryRuntime.h"

class UITask : public AbstractUITask {
  ZenDisplayDriver* _display;
  SensorManager* _sensors;
  zen::GpsService* _gps;
#ifdef PIN_BUZZER
  ZenBuzzer buzzer;
#endif
#ifdef PIN_VIBRATION
  GenericVibration vibration;
#endif
  unsigned long _next_refresh, _auto_off;
  zen::NotificationWakeController _notification_wake;
  zen::LowBatteryReminder _low_battery_reminder;
  zen::LowPowerLatch _low_power_latch;
  zen::EmergencyWindow _emergency_window;
  zen::PeripheralPowerCoordinator _power;
  zen::BluetoothPolicyAdapter _bluetooth_policy;
  bool _applying_power = false;
  void wakeForNotification(bool allow_wake = true);
  void notifyLowBattery();
  void setLowPowerMode(bool active);
  void setEmergencyMode(bool active);
  void syncPowerInputs();
  void reconcilePower(bool force = false);
  bool applyBluetoothTarget(bool enabled);
  void observeBluetoothState();
  void setDisplayRequested(bool on);
  ZenPrefs* _node_prefs;
  zen::Runtime _zen_runtime;
  zen::NodeLoginCoordinator _node_login;
  zen::RemoteNodeCoordinator _remote_node;
  zen::DiagnosticLog _diagnostic_log;
  zen::OperationResultCoordinator _operation_results;
  zen::TimeLocationCoordinator _time_location;
  zen::LocalTimeService _local_time;
  bool _deferred_prefs_save = false;
  uint32_t _deferred_prefs_save_ms = 0;
  uint8_t _deferred_prefs_attempts = 0;
  bool _dnd_active = false; // manual notification silence; RAM only
  char _alert[80];
  const char* _boot_stage = "Starting";
  zen::NotificationPopupState _notification_popup;
  char _notif_mel_buf[220];  // persistent RTTTL buffer for custom notification melodies
  KeyboardWidget _kb;        // shared across all screens — only one active at a time
  unsigned long _alert_expiry;
  int _msgcount;
  zen::MessageUnreadCoordinator _message_unread;
  unsigned long ui_started_at, next_batt_chck;
  uint16_t _batt_mv;  // EMA-filtered battery voltage
  zen::BatteryRuntimeEstimator _battery_runtime;
  unsigned long next_backlight_btn_check = 0;
#ifdef PIN_STATUS_LED
  int led_state = 0;
  unsigned long next_led_change = 0;
  unsigned long last_led_increment = 0;
#endif

#ifdef PIN_USER_BTN_ANA
  unsigned long _analogue_pin_read_millis = millis();
#endif

  // Registering a new screen touches 4 sites: (1) the member below, (2) the
  // `new XScreen()` in begin(), (3) the gotoXScreen() declaration further down,
  // (4) its one-line definition in UITask.cpp. Sites 1/3/4 are compile-checked;
  // only a forgotten (2) can slip through — the nullptr initialisers here turn
  // that into an inert no-op (see UITask::setCurrScreen) rather than a crash.
  ZenUIScreen* splash = nullptr;
  ZenUIScreen* home = nullptr;
  ZenUIScreen* settings = nullptr;
  ZenUIScreen* messages_screen = nullptr;
  ZenUIScreen* child_unlock = nullptr;
  ZenUIScreen* tools_screen = nullptr;
  ZenUIScreen* ringtone_edit = nullptr;
  ZenUIScreen* admin_screen = nullptr;
  ZenUIScreen* nearby_screen = nullptr;
  ZenUIScreen* auto_advert_screen = nullptr;
  ZenUIScreen* diag_screen = nullptr;
  ZenUIScreen* repeater_screen = nullptr;
  ZenUIScreen* curr = nullptr;
  ScreenHistory<4> _screen_history;
  void openScreen(ZenUIScreen* destination, ZenUIScreen* return_to = nullptr);
  void returnToScreen(ZenUIScreen* fallback);
  void beginBootTimeSync();
  void tickBootTimeSync();


  // Ping state
  bool _ping_active = false;
  uint32_t _ping_tag = 0;
  unsigned long _ping_sent_ms = 0;
  int16_t _ping_snr_out_x4 = 0;
  int16_t _ping_snr_back_x4 = 0;
  uint32_t _ping_rtt_ms = 0;

  void userLedHandler();

  // Button action handlers
  // allow_wake is false for Wio joystick/Enter input: only its dedicated Back
  // control is allowed to turn an intentionally blank display back on.
  char checkDisplayOn(char c, bool allow_wake = true);
  char handleLongPress(char c, bool allow_wake = true);
  char handleDoubleClick(char c);
  char handleTripleClick(char c);

  // Key FIFO: a burst of taps captured during a blocking refresh (e-ink) is
  // drained from the buttons into this queue, then all keys are applied before
  // a single redraw — so rapid navigation steps neither get lost nor cost one
  // slow refresh each. Also fixes losing a key when two buttons fire in the
  // same loop iteration.
  static const uint8_t KEY_QUEUE_SIZE = 16;
  struct QueuedKey { char key; bool cardkb; };
  QueuedKey _key_queue[KEY_QUEUE_SIZE];
  uint8_t _kq_head = 0, _kq_tail = 0;
  void enqueueKey(char c, bool cardkb = false);
  bool dequeueKey(char& c);
  void discardCardKBKeys();

  // Optional M5Stack CardKB on the Grove/Wire1 bus. CARDKB_ADDRESS is an
  // explicit board/build opt-in so an unrelated sensor at 0x5F cannot silently
  // enable keyboard polling on other targets.
#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  CardKBController _cardkb;
  bool _cardkb_was_present = false;
#endif
  void pollCardKB();
  void turnDisplayOn();
  void turnDisplayOff();

  void setCurrScreen(ZenUIScreen* c);

  // Centred alert overlay (the showAlert() box). Wraps long text to up to
  // three lines inside the box instead of letting it overflow the border.
  // Shared by every normal screen that can show a transient alert.
  void renderAlertOverlay();

public:

  UITask(mesh::MainBoard* board, MultiSerialInterface* interface_manager,
         BaseSerialInterface* bluetooth_interface)
      : AbstractUITask(board, interface_manager, bluetooth_interface),
        _display(NULL), _sensors(NULL), _gps(NULL), _node_prefs(NULL) {
    next_batt_chck = _next_refresh = 0;
    ui_started_at = 0;
    _batt_mv = 0;
    _msgcount = 0;
    _message_unread.reset();
    curr = NULL;
  }
  void begin(ZenDisplayDriver* display, SensorManager* sensors,
             zen::GpsService* gps, ZenPrefs* node_prefs);
  const char* bootStage() const { return _boot_stage; }
  bool bootSplashActive() const { return curr && curr == splash; }
  void showBootStage(const char* stage);
  // nRF52 event sleep has no timed wake of its own. While the display is on,
  // keep servicing splash, refresh and auto-off deadlines even when Bluetooth
  // is disabled and there are no radio events to wake the processor.
  bool requiresAwakeLoop() const { return _display && _display->isOn(); }
  void onBLEDisconnected() override { _next_refresh = 0; }
  void onSensorTelemetry() override;

  ZenPrefs* getNodePrefs() const { return _node_prefs; }
  int16_t localOffsetMinutes(uint32_t utc_time) const {
    return _local_time.offsetMinutes(utc_time);
  }
  uint32_t currentUtcTime() const;
  bool isChildModeLocked() const { return _zen_runtime.childLocked(_node_prefs); }
  bool isTimeSyncPending() const { return _time_location.syncPending(); }
  bool isChildModeRestricted() const override { return isChildModeLocked(); }
  void setChildAdminUnlocked(bool unlocked);
  void applyChildMode();
#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  bool isCardKBConnected() const { return _cardkb.isPresent(); }
#else
  bool isCardKBConnected() const { return false; }
#endif
  // Runtime Bluetooth requests are RAM-only. Settings change the saved intent
  // and call applyBluetoothPrefs() on exit.
  void enableBluetooth();
  void disableBluetooth();
  // Global metric/imperial preference for distance/speed display.
  bool useImperial() const { return _node_prefs && _node_prefs->units_imperial; }
  uint16_t getBattMilliVolts() const { return _batt_mv > 0 ? _batt_mv : AbstractUITask::getBattMilliVolts(); }
  zen::BatteryRuntimeEstimator::State batteryRuntimeState() const { return _battery_runtime.state(); }
  uint32_t batteryRuntimeSeconds() const { return _battery_runtime.seconds(); }
  void sleepDisplay() { turnDisplayOff(); }
  void gotoHomeScreen() { _screen_history.clear(); setCurrScreen(home); }
  void gotoSettingsScreen();
  int getSettingsSectionCount() const;
  const char* getSettingsSectionLabel(int index) const;
  void openSettingsSection(int index);
  void gotoRadioSettings();
  void gotoBluetoothSettings();
#if ENV_INCLUDE_GPS == 1
  void gotoGpsPollingSettings();
#endif
  void gotoMessagesScreen();
  zen::PathAttemptSnapshot latestPathAttempt(const uint8_t* pub_key) const;
  void gotoMessagesCategory(uint8_t category);
  void gotoChildUnlockScreen();
  void openContactDM(const ContactInfo& ci);
  bool allowOnDeviceContactMessage(const ContactInfo& ci) const override {
    return zen::Policy::contactAllowed(_node_prefs, isChildModeLocked(), &ci);
  }
  bool allowOnDeviceChannelMessage(uint8_t index) const override;
  void openPreferredTranscript();
  int  getRecentDMContacts(uint8_t out[][ZenPrefs::FAVOURITE_PREFIX_LEN], int max) const;
  void gotoToolsScreen();
  int getToolsItemCount() const;
  const char* getToolsItemLabel(int index) const;
  void openToolsItem(int index);
  void gotoRingtoneEditor(int slot = 0);
  void openAdminFor(const ContactInfo& ci); // selected Node List repeater/room
  void returnFromAdmin();
  void gotoNearbyScreen();
  void gotoDiscoverScreen();
  void gotoAutoAdvertScreen();
  // A contact was removed (companion app / CLI): drop pinned favourites and
  // per-contact notification overrides that would otherwise dangle.
  void onContactRemoved(const uint8_t* pub_key) override;
  // A cleared channel slot must not retain a notification melody override.
  void onChannelRemoved(uint8_t channel_idx) override;
  void gotoDiagnosticsScreen();
  void gotoRepeaterScreen();
  // Clear any active alert overlay early (alarm dismiss).
  void clearAlert() { _alert_expiry = 0; _notification_popup.clear(); }
  // Shared on-screen keyboard — only one screen drives it at a time.
  KeyboardWidget& keyboard() { return _kb; }
  // Current GPS position (1e6-scaled degrees), false when there's no usable
  // fix. Single source of truth for "where am I", shared by the nav / compass
  // / map screens so the LocationProvider lookup isn't duplicated per screen.
  bool currentLocation(int32_t& lat, int32_t& lon) const;
  void playMelody(const char* melody);
  void previewMelody(uint8_t selection, uint8_t empty_fallback);
  void stopMelody();
  bool isMelodyPlaying();
  void showAlert(const char* text, int duration_millis);
  void showAlertPriority(const char* text, int duration_millis, uint8_t priority);
  void publishOperation(const zen::OperationResult& result,
                        zen::OperationStatus* screen_status = nullptr);
  void operationError(zen::Operation operation, zen::OperationOutcome outcome,
                      zen::OperationReason reason,
                      zen::OperationStatus* status = nullptr,
                      uint8_t flags = zen::RESULT_SCREEN_WAKE) {
    publishOperation(zen::OperationResult::make(operation, outcome, reason, flags), status);
  }
  void operationWarning(zen::Operation operation, zen::OperationReason reason,
                        zen::OperationStatus* status = nullptr,
                        uint8_t flags = zen::RESULT_SCREEN_WAKE) {
    publishOperation(zen::OperationResult::make(
        operation, zen::OperationOutcome::WARNING, reason, flags), status);
  }
  void publishRemoteOperation(zen::Operation operation,
                      zen::RemoteNodeOperation::Result result,
                      zen::OperationStatus* screen_status = nullptr);
  void onOperationResult(const zen::OperationResult& result) override {
    publishOperation(result);
  }
  void onPrefsRestored() override;
  void onPrefsCommitted(uint16_t effects) override;
  void onTimeSynchronized(zen::TimeSyncSource source) override;
  const zen::DiagnosticLog& diagnosticLog() const { return _diagnostic_log; }
  void clearDiagnosticLog() { _diagnostic_log.clear(); }
  bool notificationAllowed(UIEventType event, uint8_t contact_type = 0,
                           const uint8_t* pub_key = nullptr, int channel_idx = -1) const;
  zen::NotificationContext notificationContext() const;
  zen::NotificationDecision dispatchNotification(
      const zen::NotificationEvent& event, bool present_outputs = true);
  void executeNotification(const zen::NotificationEvent& event,
                           const zen::NotificationDecision& decision);
  void processMessageNotification(const zen::NotificationEvent& event,
                                  uint8_t path_len, const char* from_name,
                                  const char* text, int msgcount,
                                  uint8_t contact_type, const uint8_t* pub_key,
                                  bool include_sound);
  void presentNotification(const zen::NotificationEvent& event,
                           bool play_sound, bool vibrate);
  void handleNewMsg(uint8_t path_len, const char* from_name, const char* text,
                    int msgcount, uint8_t contact_type, const uint8_t* pub_key);
  bool isQuietTimeActive() const;
  bool isNotificationQuietActive() const;
  bool isNotificationAudioMuted() const;
  void notifyHomeAction(); // UI feedback, unlike delivery ACKs, follows silence policy
  bool isLowPowerMode() const { return _power.restrictions().low_power; }
  zen::PeripheralPowerCoordinator::EffectiveState effectivePowerState() const {
    return _power.effective();
  }
  zen::PeripheralPowerCoordinator::EffectiveState appliedPowerState() const {
    return _power.hasAppliedState()
        ? _power.applied() : zen::PeripheralPowerCoordinator::EffectiveState();
  }
  uint16_t pendingPowerChanges() const { return _power.pendingChanges(); }
  bool isEmergencyMode() const { return _emergency_window.active(); }
  uint32_t emergencyRemainingSeconds() const {
    return _emergency_window.remainingSeconds(millis());
  }
  void beginEmergencyMode() { setEmergencyMode(true); }
  void endEmergencyMode() { setEmergencyMode(false); }
  void addChannelMsg(uint8_t channel_idx, const char* text, uint32_t timestamp = 0) override;
  bool addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text, uint32_t sender_timestamp = 0) override;
  int addOwnChannelMsg(uint8_t channel_idx, const char* text, int text_len,
                       uint32_t timestamp) override;
  void armChannelRelay(int history_pos, uint32_t seq) override;
  void onMsgAck(uint32_t ack_crc) override;
  void onNodeLoginCancelled(const uint8_t* prefix) override;
  void onChannelRelayed(uint32_t seq) override;
  void onChannelRelayExpired(uint32_t seq, uint8_t heard, bool transmitted) override;
  void onNodeLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) override;
  bool startNodeLogin(zen::NodeLoginCoordinator::Owner owner, const ContactInfo& contact,
                      const char* password, bool used_saved_password = false);
  bool retryNodeLogin(zen::NodeLoginCoordinator::Attempt& attempt);
  bool nodeLoginBusy() const { return _node_login.active() || _remote_node.active(); }
  zen::RemoteNodeCoordinator& remoteNode() { return _remote_node; }
  const zen::RemoteNodeCoordinator& remoteNode() const { return _remote_node; }
  bool remoteStatus(zen::RemoteNodeCoordinator::Owner owner,
                    char* out, size_t size) const;
  void cancelNodeLogin(zen::NodeLoginCoordinator::Owner owner, const uint8_t* pub_key);
  bool isRoomLoggedIn(const uint8_t* pub_key) const { return _node_login.isLoggedIn(pub_key); }
  void logoutRoom(const uint8_t* pub_key);
  void onAdminReply(const uint8_t* pub_key, const char* text) override;
  int  getDMUnreadTotal() const;
  int  getMsgCount() const { return _msgcount; }
  int  getChannelUnreadCount() const;
  int  getRoomUnreadCount() const;
  int  markMessageCategoryRead(uint8_t category);
  uint8_t getRoomUnread(const uint8_t* pub_key) const;
  void clearRoomUnread(const uint8_t* pub_key);
  void clearRoomUnread();
  // Clamped to the DM ring's actual occupancy for this contact -- defined in
  // UITask.cpp (needs MessagesScreen to be a complete type). Same self-healing
  // shape as ConversationStore::chUnread() for channels.
  uint8_t getDMUnread(const uint8_t* pub_key) const;
  bool hasDirectDMContact(const uint8_t* pub_key) const {
    return _message_unread.has(zen::MessageUnreadCoordinator::DIRECT, pub_key);
  }
  void clearDMUnread(const uint8_t* pub_key) {
    _message_unread.clear(zen::MessageUnreadCoordinator::DIRECT, pub_key);
  }
  void clearAllDMUnread() {
    _message_unread.clearAll(zen::MessageUnreadCoordinator::DIRECT);
  }
  // A screen remains selected while the panel is asleep. Treat it as visible
  // only while it is actually current and the physical display is powered.
  bool isMessagesScreenVisible() const {
    return curr == messages_screen && _display != NULL && _display->isOn();
  }
  void forgetDMContact(const uint8_t* pub_key) {
    _message_unread.forget(zen::MessageUnreadCoordinator::DIRECT, pub_key);
  }
  // Frees an unread/sender slot when its conversation has fallen out of the DM
  // ring. A zero unread count alone retains the proven direct-DM identity.
  void reconcileDMUnread();
  bool hasDisplay() const { return _display != NULL; }
  ZenDisplayDriver* getDisplay() const { return _display; }

  // Ping helpers
  bool startPing(const uint8_t* pub_key);
  bool isPingActive() const { return _ping_active; }
  void getPingResult(int16_t& snr_out_x4, int16_t& snr_back_x4, uint32_t& rtt_ms) const {
    snr_out_x4 = _ping_snr_out_x4;
    snr_back_x4 = _ping_snr_back_x4;
    rtt_ms = _ping_rtt_ms;
  }
  void clearPing();
  void handlePingResult(uint32_t tag, int16_t snr_out_x4, int16_t snr_back_x4, uint32_t rtt_ms);

  // Favourites dial helpers. Serialized storage retains six legacy slots; the
  // current UI exposes slots 0..FAVOURITES_DIAL_COUNT-1.
  int findFavouriteSlot(const uint8_t* pub_key) const {
    if (!_node_prefs || !pub_key) return -1;
    for (int i = 0; i < ZenPrefs::FAVOURITES_DIAL_COUNT; i++) {
      if (memcmp(_node_prefs->favourite_contacts[i], pub_key, ZenPrefs::FAVOURITE_PREFIX_LEN) == 0) {
        // All-zero prefix is "empty" — never matches a real key.
        bool any = false;
        for (uint8_t b = 0; b < ZenPrefs::FAVOURITE_PREFIX_LEN; b++)
          if (_node_prefs->favourite_contacts[i][b]) { any = true; break; }
        if (any) return i;
      }
    }
    return -1;
  }
  bool isFavouriteSlotEmpty(int slot) const {
    if (!_node_prefs || slot < 0 || slot >= ZenPrefs::FAVOURITES_DIAL_COUNT) return true;
    for (uint8_t b = 0; b < ZenPrefs::FAVOURITE_PREFIX_LEN; b++)
      if (_node_prefs->favourite_contacts[slot][b]) return false;
    return true;
  }
  bool setFavouriteSlot(int slot, const uint8_t* pub_key);
  void clearFavouriteSlot(int slot) {
    if (!_node_prefs || slot < 0 || slot >= ZenPrefs::FAVOURITES_COUNT) return;
    memset(_node_prefs->favourite_contacts[slot], 0, ZenPrefs::FAVOURITE_PREFIX_LEN);
  }
  bool isButtonPressed() const;

  bool isBuzzerQuiet() { 
#ifdef PIN_BUZZER
    return isNotificationAudioMuted();
#else
    return true;
#endif
  }

  void cycleNotificationMode(int direction = 1); // Left/Right step On/Off/Auto
  int  getNotificationMode() const; // 0=On, 1=Off, 2=Auto
  bool getGPSState();
  uint8_t getGPSMode() const;
  void setGPSMode(uint8_t mode);
  void applyGpsPrefs();
  void applyBluetoothPrefs();
  bool hasGPS();   // true if this board exposes a toggleable GPS (distinct from GPS being off)
  zen::GpsCourse::Source getGpsCourse(long& course_millideg) const {
#if ENV_INCLUDE_GPS == 1
    return _time_location.course(course_millideg);
#else
    (void)course_millideg; return zen::GpsCourse::NONE;
#endif
  }
  bool getGpsFixAgeMs(uint32_t& age) const {
    const auto& status = _time_location.status();
    if (!status.fix_age_available) return false;
    age = status.fix_age_ms;
    return true;
  }
  bool getGpsAdaptiveRetry(uint32_t& remaining_ms) const {
    return _gps && _gps->getAdaptiveRetry(remaining_ms);
  }
  const zen::TimeLocationCoordinator::Status& timeLocationStatus() const {
    return _time_location.status();
  }
  void toggleGPS();
  void applyBrightness();
  void setBrightnessLevel(uint8_t level);
  uint8_t getBrightnessLevel() const { return _node_prefs ? _node_prefs->display_brightness : 2; }
  void setBuzzerVolumeLevel(uint8_t level);
  uint8_t getBuzzerVolume() const { return _node_prefs ? _node_prefs->buzzer_volume : 4; }
  void applyTxPower();
  void applyRadioParams(); // freq/bw/sf/cr from prefs (radio preset change)
  // Save-on-exit helper for the screen `_dirty` pattern: persists ZenPrefs once
  // only if `dirty`, then clears the flag. Standardises the screens' exit paths
  // (some used to leave the flag set, relying on onShow() to reset it) and keeps
  // the "did we touch flash?" answer in one place. Returns whether it saved.
  bool savePrefsIfDirty(bool& dirty);
  void requestPrefsSave();
  void applyRotation();
  void applyFullRefreshInterval();
  uint32_t autoOffMillis() const {
    if (isLowPowerMode()) return 5000;
    if (!_node_prefs || _node_prefs->auto_off_secs == 0) return 0;
    return (uint32_t)_node_prefs->auto_off_secs * 1000UL;
  }


  // from AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, uint8_t contact_type = 0, const uint8_t* pub_key = nullptr) override;
  void incomingMessage(UIEventType event, uint8_t path_len,
                       const char* from_name, const char* text, int msgcount,
                       uint8_t contact_type = 0, const uint8_t* pub_key = nullptr,
                       int channel_idx = -1) override;
  void notify(UIEventType t = UIEventType::none) override;
  void onNewContact(const ContactInfo& contact) override;
  void loop() override;

  void shutdown(bool restart = false);
};
