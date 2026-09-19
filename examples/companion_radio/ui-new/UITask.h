#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/ContactInfo.h>
#include "../solo/PathDetails.h"
#include <Arduino.h>
#include <helpers/sensors/LPPDataHelpers.h>

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif
#ifdef PIN_VIBRATION
  #include <helpers/ui/GenericVibration.h>
#endif

#include "../AbstractUITask.h"
#include "../NodePrefs.h"
#include "../solo/SoloRuntime.h"
#include "../solo/BootTimeSync.h"
#include "../solo/GpsMode.h"
#include "../solo/GpsCourse.h"
#include "../solo/TimezonePolicy.h"
#include "../solo/NodeLoginCoordinator.h"
#include "../solo/DiagnosticLog.h"
#include "KeyboardWidget.h"
#include "ScreenHistory.h"
#if defined(CARDKB_ADDRESS) && SOLO_FEAT_CARDKB
  #include <helpers/ui/CardKBController.h>
#endif

#include "../solo/BatteryPolicy.h"
#include "../solo/BatteryRuntime.h"

class UITask : public AbstractUITask {
  DisplayDriver* _display;
  SensorManager* _sensors;
#ifdef PIN_BUZZER
  genericBuzzer buzzer;
#endif
#ifdef PIN_VIBRATION
  GenericVibration vibration;
#endif
  unsigned long _next_refresh, _auto_off;
  bool _notification_wake_active;
  solo::LowBatteryReminder _low_battery_reminder;
  solo::LowPowerLatch _low_power_latch;
  solo::EmergencyWindow _emergency_window;
  bool _low_power_mode = false;
  bool _emergency_gps_on = false;
  void wakeForNotification(bool allow_wake = true);
  void notifyLowBattery();
  void setLowPowerMode(bool active);
  void setEmergencyMode(bool active);
  NodePrefs* _node_prefs;
  solo::Runtime _solo;
  solo::NodeLoginCoordinator _node_login;
  solo::DiagnosticLog _diagnostic_log;
#if ENV_INCLUDE_GPS == 1
  solo::GpsCourse _gps_course;
  uint32_t _next_gps_course_sample_ms = 0;
  uint32_t _last_gps_fix_ms = 0;
  bool _has_gps_fix_age = false;
#endif
  bool _deferred_prefs_save = false;
  uint32_t _deferred_prefs_save_ms = 0;
  bool _dnd_active = false; // manual notification silence; RAM only
  char _alert[80];
  char _notif_mel_buf[220];  // persistent RTTTL buffer for custom notification melodies
  KeyboardWidget _kb;        // shared across all screens — only one active at a time
  unsigned long _alert_expiry;
  int _msgcount;
  int _last_notif_ch_idx;
  uint8_t _last_notif_dm_prefix[4];
  bool _last_notif_dm_valid;
  struct DMUnreadEntry { uint8_t prefix[4]; uint8_t count; uint8_t seen; };
  static const int DM_UNREAD_TABLE_SIZE = 16;
  DMUnreadEntry _dm_unread_table[DM_UNREAD_TABLE_SIZE];
  // The shared history can hold at most 32 distinct room conversations, so a
  // same-sized table cannot lose an unread room merely because many are active.
  static const int ROOM_UNREAD_TABLE_SIZE = 32;
  DMUnreadEntry _room_unread_table[ROOM_UNREAD_TABLE_SIZE];
  unsigned long ui_started_at, next_batt_chck;
  uint16_t _batt_mv;  // EMA-filtered battery voltage
  uint16_t _reported_radio_errors = 0;
  solo::BatteryRuntimeEstimator _battery_runtime;
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
  UIScreen* splash = nullptr;
  UIScreen* home = nullptr;
  UIScreen* settings = nullptr;
  UIScreen* messages_screen = nullptr;
  UIScreen* child_unlock = nullptr;
  UIScreen* tools_screen = nullptr;
  UIScreen* ringtone_edit = nullptr;
  UIScreen* admin_screen = nullptr;
  UIScreen* nearby_screen = nullptr;
  UIScreen* auto_advert_screen = nullptr;
  UIScreen* diag_screen = nullptr;
  UIScreen* repeater_screen = nullptr;
  UIScreen* curr = nullptr;
  solo::BootTimeSync _boot_time_sync;
  ScreenHistory<4> _screen_history;
  void pushScreenReturn(UIScreen* screen);
  UIScreen* popScreenReturn();
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
#if defined(CARDKB_ADDRESS) && SOLO_FEAT_CARDKB
  CardKBController _cardkb;
  bool _cardkb_was_present = false;
#endif
  void pollCardKB();
  void turnDisplayOn();
  void turnDisplayOff();

  void setCurrScreen(UIScreen* c);

  // Centred alert overlay (the showAlert() box). Wraps long text to up to
  // three lines inside the box instead of letting it overflow the border.
  // Shared by every normal screen that can show a transient alert.
  void renderAlertOverlay();

public:

  UITask(mesh::MainBoard* board, MultiSerialInterface* interface_manager) : AbstractUITask(board, interface_manager), _display(NULL), _sensors(NULL), _node_prefs(NULL) {
    next_batt_chck = _next_refresh = 0;
    ui_started_at = 0;
    _batt_mv = 0;
    _msgcount = 0;
    _notification_wake_active = false;
    _last_notif_ch_idx = -1;
    _last_notif_dm_valid = false;
    memset(_last_notif_dm_prefix, 0, sizeof(_last_notif_dm_prefix));
    memset(_dm_unread_table, 0, sizeof(_dm_unread_table));
    memset(_room_unread_table, 0, sizeof(_room_unread_table));
    curr = NULL;
  }
  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);
  void onBLEDisconnected() override { _next_refresh = 0; }
  void onSensorTelemetry() override { _next_refresh = 0; } // redraw, never wake

  NodePrefs* getNodePrefs() const { return _node_prefs; }
  int16_t localOffsetMinutes(uint32_t utc_time) const {
    return _node_prefs ? solo::TimezonePolicy::offsetMinutes(
        _node_prefs->timezone_mode, _node_prefs->timezone_manual_min,
        _node_prefs->timezone_city, utc_time) : 0;
  }
  uint32_t currentUtcTime() const;
  bool isChildModeLocked() const { return _solo.childLocked(_node_prefs); }
  bool isTimeSyncPending() const { return _boot_time_sync.pending(); }
  bool isChildModeRestricted() const override { return isChildModeLocked(); }
  void setChildAdminUnlocked(bool unlocked);
  void applyChildMode();
#if defined(CARDKB_ADDRESS) && SOLO_FEAT_CARDKB
  bool isCardKBConnected() const { return _cardkb.isPresent(); }
#else
  bool isCardKBConnected() const { return false; }
#endif
  // Global metric/imperial preference for distance/speed display.
  bool useImperial() const { return _node_prefs && _node_prefs->units_imperial; }
  uint16_t getBattMilliVolts() const { return _batt_mv > 0 ? _batt_mv : AbstractUITask::getBattMilliVolts(); }
  solo::BatteryRuntimeEstimator::State batteryRuntimeState() const { return _battery_runtime.state(); }
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
  solo::PathAttemptSnapshot latestPathAttempt(const uint8_t* pub_key) const;
  void gotoMessagesCategory(uint8_t category);
  void gotoChildUnlockScreen();
  void openContactDM(const ContactInfo& ci);
  bool allowOnDeviceContactMessage(const ContactInfo& ci) const override {
    return solo::Policy::contactAllowed(_node_prefs, isChildModeLocked(), &ci);
  }
  bool allowOnDeviceChannelMessage(uint8_t index) const override;
  void openPreferredTranscript();
  void shareToMessage(const char* text);
  void pickLocShareTarget();
  void pickBotChannelTarget();
  void pickBotRoomTarget();
  int  getRecentDMContacts(uint8_t out[][NodePrefs::FAVOURITE_PREFIX_LEN], int max) const;
  void gotoToolsScreen();
  int getToolsItemCount() const;
  const char* getToolsItemLabel(int index) const;
  void openToolsItem(int index);
  void gotoRingtoneEditor(int slot = 0);
  void gotoBotScreen();
  void openAdminFor(const ContactInfo& ci); // selected Node List repeater/room
  void returnFromAdmin();
  void gotoNearbyScreen();
  void gotoDiscoverScreen();
  void gotoAutoAdvertScreen();
  void gotoLiveShareScreen();
  // A contact was removed (companion app / CLI): drop any UI reference to its
  // pubkey that would otherwise dangle — a pinned favourite slot, the Locator
  // target if it was this contact, the Live Share target if it was this
  // contact (auto-share turns off rather than guessing a new recipient), and
  // any per-contact mute/melody override (those tables have only 16 shared
  // slots, so an orphan isn't just stale — it can starve other contacts).
  void onContactRemoved(const uint8_t* pub_key) override;
  // A channel slot was cleared: turn off anything armed against it by index
  // (the bot's channel, Live Share's channel target) and drop its per-channel
  // melody bit, so a future channel re-added at the same slot starts clean.
  void onChannelRemoved(uint8_t channel_idx) override;
  void gotoDiagnosticsScreen();
  void gotoRepeaterScreen();
  // Clear any active alert overlay early (alarm dismiss).
  void clearAlert() { _alert_expiry = 0; }
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
  void logFailure(const char* operation, const char* reason);
  void logWarning(const char* operation, const char* reason);
  void reportEvent(solo::DiagnosticLog::Severity severity, const char* operation,
                   const char* reason, bool background = false, bool screen_wake = true);
  void onOperationFailure(const char* operation, const char* reason) override {
    reportEvent(solo::DiagnosticLog::ERROR, operation, reason, true);
  }
  void onOperationWarning(const char* operation, const char* reason) override {
    reportEvent(solo::DiagnosticLog::WARNING, operation, reason, true);
  }
  const solo::DiagnosticLog& diagnosticLog() const { return _diagnostic_log; }
  void clearDiagnosticLog() { _diagnostic_log.clear(); }
  void resetReportedRadioErrors() { _reported_radio_errors = 0; }
  bool notificationAllowed(UIEventType event, uint8_t contact_type = 0,
                           const uint8_t* pub_key = nullptr, int channel_idx = -1) const;
  void presentNotification(UIEventType event, bool play_sound, bool vibrate);
  void handleNewMsg(uint8_t path_len, const char* from_name, const char* text,
                    int msgcount, uint8_t contact_type, const uint8_t* pub_key,
                    bool present, bool wake_screen);
  bool notificationQuietAffected(UIEventType event) const;
  bool isQuietTimeActive() const;
  bool isNotificationQuietActive() const;
  bool isNotificationAudioMuted() const;
  void notifyHomeAction(); // UI feedback, unlike delivery ACKs, follows silence policy
  bool isLowPowerMode() const { return _low_power_mode; }
  bool isEmergencyMode() const { return _emergency_window.active(); }
  uint32_t emergencyRemainingSeconds() const {
    return _emergency_window.remainingSeconds(millis());
  }
  void beginEmergencyMode() { setEmergencyMode(true); }
  void endEmergencyMode() { setEmergencyMode(false); }
  void addChannelMsg(uint8_t channel_idx, const char* text, uint32_t timestamp = 0) override;
  bool addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text, uint32_t sender_timestamp = 0) override;
  void addAppDMMsg(const uint8_t* pub_key, const char* text, uint32_t timestamp,
                   uint8_t attempt, uint32_t ack_tag, uint32_t ack_deadline_ms,
                   uint8_t path_len) override;
  int addOwnChannelMsg(uint8_t channel_idx, const char* text, int text_len,
                       uint32_t timestamp) override;
  void armChannelRelay(int history_pos, uint32_t seq) override;
  void onMsgAck(uint32_t ack_crc) override;
  bool matchMsgAck(uint32_t ack_crc, uint8_t* prefix) override;
  void onNodeLoginCancelled(const uint8_t* prefix) override;
  void onChannelRelayed(uint32_t seq) override;
  void onChannelRelayExpired(uint32_t seq) override;
  void onNodeLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) override;
  bool startNodeLogin(solo::NodeLoginCoordinator::Owner owner, const ContactInfo& contact,
                      const char* password, bool used_saved_password = false);
  bool retryNodeLogin(solo::NodeLoginCoordinator::Attempt& attempt);
  bool nodeLoginBusy() const { return _node_login.active(); }
  void cancelNodeLogin(solo::NodeLoginCoordinator::Owner owner, const uint8_t* pub_key);
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
  // shape as MessageHistory::chUnread() for channels.
  uint8_t getDMUnread(const uint8_t* pub_key) const;
  bool hasDirectDMContact(const uint8_t* pub_key) const {
    for (int i = 0; i < DM_UNREAD_TABLE_SIZE; i++)
      if (_dm_unread_table[i].seen && memcmp(_dm_unread_table[i].prefix, pub_key, 4) == 0)
        return true;
    return false;
  }
  void clearDMUnread(const uint8_t* pub_key) {
    for (int i = 0; i < DM_UNREAD_TABLE_SIZE; i++)
      if (_dm_unread_table[i].seen && memcmp(_dm_unread_table[i].prefix, pub_key, 4) == 0)
        { _dm_unread_table[i].count = 0; return; }
  }
  void clearAllDMUnread() {
    for (int i = 0; i < DM_UNREAD_TABLE_SIZE; i++) _dm_unread_table[i].count = 0;
  }
  // A screen remains selected while the panel is asleep. Treat it as visible
  // only while it is actually current and the physical display is powered.
  bool isMessagesScreenVisible() const {
    return curr == messages_screen && _display != NULL && _display->isOn();
  }
  void forgetDMContact(const uint8_t* pub_key) {
    for (int i = 0; i < DM_UNREAD_TABLE_SIZE; i++)
      if (_dm_unread_table[i].seen && memcmp(_dm_unread_table[i].prefix, pub_key, 4) == 0)
        { memset(&_dm_unread_table[i], 0, sizeof(_dm_unread_table[i])); return; }
  }
  // Frees an unread/sender slot when its conversation has fallen out of the DM
  // ring. A zero unread count alone retains the proven direct-DM identity.
  void reconcileDMUnread();
  bool hasDisplay() const { return _display != NULL; }
  DisplayDriver* getDisplay() const { return _display; }

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
    for (int i = 0; i < NodePrefs::FAVOURITES_DIAL_COUNT; i++) {
      if (memcmp(_node_prefs->favourite_contacts[i], pub_key, NodePrefs::FAVOURITE_PREFIX_LEN) == 0) {
        // All-zero prefix is "empty" — never matches a real key.
        bool any = false;
        for (uint8_t b = 0; b < NodePrefs::FAVOURITE_PREFIX_LEN; b++)
          if (_node_prefs->favourite_contacts[i][b]) { any = true; break; }
        if (any) return i;
      }
    }
    return -1;
  }
  bool isFavouriteSlotEmpty(int slot) const {
    if (!_node_prefs || slot < 0 || slot >= NodePrefs::FAVOURITES_DIAL_COUNT) return true;
    for (uint8_t b = 0; b < NodePrefs::FAVOURITE_PREFIX_LEN; b++)
      if (_node_prefs->favourite_contacts[slot][b]) return false;
    return true;
  }
  bool setFavouriteSlot(int slot, const uint8_t* pub_key);
  void clearFavouriteSlot(int slot) {
    if (!_node_prefs || slot < 0 || slot >= NodePrefs::FAVOURITES_COUNT) return;
    memset(_node_prefs->favourite_contacts[slot], 0, NodePrefs::FAVOURITE_PREFIX_LEN);
  }
  bool isButtonPressed() const;

  bool isBuzzerQuiet() { 
#ifdef PIN_BUZZER
    return buzzer.isQuiet();
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
  solo::GpsCourse::Source getGpsCourse(long& course_millideg) const {
#if ENV_INCLUDE_GPS == 1
    return _gps_course.read(millis(), course_millideg);
#else
    (void)course_millideg; return solo::GpsCourse::NONE;
#endif
  }
  bool getGpsFixAgeMs(uint32_t& age) const {
    if (!_has_gps_fix_age) return false;
    age = millis() - _last_gps_fix_ms;
    return true;
  }
  void toggleGPS();
  void applyBrightness();
  void setBrightnessLevel(uint8_t level);
  uint8_t getBrightnessLevel() const { return _node_prefs ? _node_prefs->display_brightness : 2; }
  void setBuzzerVolumeLevel(uint8_t level);
  uint8_t getBuzzerVolume() const { return _node_prefs ? _node_prefs->buzzer_volume : 4; }
  void applyTxPower();
  void applyRadioParams(); // freq/bw/sf/cr from prefs (radio preset change)
  // Save-on-exit helper for the screen `_dirty` pattern: persists NodePrefs once
  // only if `dirty`, then clears the flag. Standardises the screens' exit paths
  // (some used to leave the flag set, relying on onShow() to reset it) and keeps
  // the "did we touch flash?" answer in one place. Returns whether it saved.
  bool savePrefsIfDirty(bool& dirty);
  void requestPrefsSave();
  void applyRotation();
  void applyFullRefreshInterval();
  uint32_t autoOffMillis() const {
    if (_low_power_mode) return 5000;
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
