#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp after SensorPlaceholders.h is defined.

#include "../Features.h"
#include "../RadioPresets.h"
#include "RadioParamsEditor.h"
#include "RadioPresetPicker.h"
#include "ChildMode.h"
#include "DigitEditor.h"
#include "QuietTime.h"
#include "TimeOfDayEditor.h"
#include "TimezoneEditor.h"
#include "MessageEditorSupport.h"
#include "HomePageRegistry.h"
#include "../solo/GpsMode.h"
#include "../solo/QuickReplies.h"
#include "../solo/BuiltinMelodies.h"
#include "../solo/TimezonePolicy.h"

class SettingsScreen : public UIScreen {
  UITask* _task;

  enum SettingItem {
    // Display section
    SECTION_DISPLAY,
#if FEAT_BRIGHTNESS_SETTING
    BRIGHTNESS,
#endif
#if AUTO_OFF_MILLIS > 0
    AUTO_OFF,
#endif
    BATT_DISPLAY,
#if FEAT_CLOCK_SECONDS_SETTING
    CLOCK_SECONDS,
#endif
    CLOCK_FORMAT,
#if FEAT_DISPLAY_ROTATION_SETTING
    ROTATION,
#endif
#if FEAT_JOYSTICK_ROTATION_SETTING
    JOY_ROTATION,
#endif
#if FEAT_FULL_REFRESH_SETTING
    EINK_FULL_REFRESH,
#endif
    // Notification presentation is separate from sound selection.
    SECTION_NOTIFICATIONS,
    NOTIFICATION_MODE,
    NOTIFICATION_SCREEN_WAKE,
#if SOLO_FEAT_QUIET_TIME
    QUIET_TIME,
    QUIET_FROM,
    QUIET_UNTIL,
#endif
    // Sound section
    SECTION_SOUND,
    BUZZER_VOLUME,
    DM_MELODY,
    CH_MELODY,
    NEW_CONTACT_MELODY,
    AD_SOUND,
    AD_SOUND_SCOPE,
    // Home pages section
    SECTION_HOME_PAGES,
    HOME_QUICK_MSG, HOME_FAVOURITES, HOME_SENSORS,
#if ENV_INCLUDE_GPS == 1
    HOME_GPS,
#endif
    HOME_ADVERT, HOME_BT, HOME_RADIO, HOME_TOOLS, HOME_SETTINGS,
    // Radio section
    SECTION_RADIO,
    TX_POWER,
    RADIO_PRESET,
    CUSTOM_FREQ, CUSTOM_SF, CUSTOM_BW, CUSTOM_CR,
    // System section
    SECTION_SYSTEM,
    DEVICE_NAME,
    TIMEZONE,
#if ENV_INCLUDE_GPS == 1
    GPS_ENABLED,
    GPS_POLLING,
#endif
    UNITS,
    IMPORT_MESHCORE,
    REBOOT,
    // Bluetooth section
    SECTION_BLUETOOTH,
    BLUETOOTH_ENABLED,
    BLUETOOTH_PIN_MODE,
    BLUETOOTH_PIN,
    // Keyboard section
    SECTION_KEYBOARD,
    KEYBOARD_TYPE,
#if defined(CARDKB_ADDRESS) && SOLO_FEAT_CARDKB
    KEYBOARD_CARDKB_STATUS,
#endif
    // Contacts section
    SECTION_CONTACTS, DM_FILTER, CH_FILTER, ROOM_FILTER,
    // Child mode section
#if SOLO_FEAT_CHILD_MODE
    SECTION_CHILD, CHILD_ENABLED, CHILD_PIN, CHILD_CHANNELS, CHILD_ROOMS, CHILD_FAVOURITES,
#endif
    // Quick Replies section
    SECTION_QUICK_REPLIES,
    MSG_SLOT_0, MSG_SLOT_1, MSG_SLOT_2, MSG_SLOT_3, MSG_SLOT_4,
    Count
  };

  // Sections are walked once from the enum, honouring the #if guards. The home
  // card selects a section, then this screen renders only that section's items.
  int  _selected = 0;   // SettingItem under the cursor, resolved per input/render
  int  _reserve = 0;    // right-edge px reserved for the scrollbar (0 when list fits)
  bool _dirty = false;
  uint32_t _initial_prefs_fingerprint = 0;
  bool _have_initial_prefs = false;
#if ENV_INCLUDE_GPS == 1
  bool _gps_dirty = false; // staged until this settings screen is closed
  bool _gps_initial_enabled = false;
  bool _gps_pending_enabled = false;
  uint8_t _gps_initial_polling = 0;
  uint8_t _gps_pending_polling = 0;
#endif
  bool _bluetooth_dirty = false; // staged with GPS; applied and saved on exit
  uint8_t _bluetooth_initial = 1;

  static const int NUM_SECTIONS = 10 + SOLO_FEAT_CHILD_MODE;
  static const int MAX_PER_SEC  = 16;
  uint8_t _sec_items[NUM_SECTIONS][MAX_PER_SEC]; // SettingItem per (section, row)
  uint8_t _sec_count[NUM_SECTIONS];
  uint8_t _sec_header[NUM_SECTIONS];             // the SECTION_* enum for each section
  int     _num_sections = 0;
  int     _open_section = -1;
  int     _open_item = -1;
  int     _active_section = -1;
  int     _section_sel = 0;
  int     _section_scroll = 0;

#if AUTO_OFF_MILLIS > 0
  static const uint16_t AUTO_OFF_OPTS[5];
  static const char* AUTO_OFF_LABELS[5];
  static const int AUTO_OFF_COUNT = 5;
#endif
  static const char* BATT_DISPLAY_LABELS[3];
  static const int BATT_DISPLAY_COUNT = 3;
  static const char* AD_SCOPE_LABELS[2];
  static const int AD_SCOPE_COUNT = 2;
#if FEAT_FULL_REFRESH_SETTING
  static const char* EINK_FULL_REFRESH_LABELS[5];
  static const int   EINK_FULL_REFRESH_COUNT = 5;
#endif

  // The companion's own radio fields, as the shared preset picker's target
  // (Tools › Repeater points the same picker at the dedicated repeater profile).
  RadioPresetPicker::Target radioTarget(NodePrefs* p) const {
    return { &p->freq, &p->bw, &p->sf, &p->cr };
  }

  // Value column start, pulled left by the scrollbar gutter so right-side
  // values never render under the indicator when the list scrolls.
  int valCol(DisplayDriver& display) const { return display.valCol() - _reserve; }

  static uint32_t prefsFingerprint(const NodePrefs& prefs) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&prefs);
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < sizeof(NodePrefs); i++) {
      hash ^= bytes[i];
      hash *= 16777619UL;
    }
    return hash;
  }

  // Shared 0/90/180/270 labels for display + joystick rotation.
  static const char* rotLabel(uint8_t r) {
    static const char* const L[] = { "0 deg", "90 deg", "180 deg", "270 deg" };
    return L[r & 3];
  }

  void renderBar(DisplayDriver& display, int x, int y, int value, int max_val) {
    const int gap     = 2;
    const int avail   = display.width() - x - _reserve;
    const int raw     = (avail - (max_val - 1) * gap) / max_val;
    const int cap     = display.getLineHeight() - 2;
    const int box_h   = raw < cap ? (raw < 2 ? 2 : raw) : cap;
    const int box_w   = box_h;
    for (int i = 0; i < max_val; i++) {
      int bx = x + i * (box_w + gap);
      display.drawRect(bx, y, box_w, box_h);
      if (i < value)
        display.fillRect(bx + 1, y + 1, box_w - 2, box_h - 2);
    }
  }

#if AUTO_OFF_MILLIS > 0
  int autoOffIndex() {
    NodePrefs* p = _task->getNodePrefs();
    if (!p) return 1;
    for (int i = 0; i < AUTO_OFF_COUNT; i++)
      if (AUTO_OFF_OPTS[i] == p->auto_off_secs) return i;
    return 1;
  }
#endif


  bool isSection(int item) const {
    return item == SECTION_DISPLAY || item == SECTION_NOTIFICATIONS ||
           item == SECTION_SOUND ||
           item == SECTION_HOME_PAGES ||
           item == SECTION_RADIO   || item == SECTION_SYSTEM ||
           item == SECTION_BLUETOOTH ||
           item == SECTION_KEYBOARD ||
           item == SECTION_CONTACTS ||
#if SOLO_FEAT_CHILD_MODE
           item == SECTION_CHILD ||
#endif
           item == SECTION_QUICK_REPLIES;
  }

  const char* sectionName(int item) const {
    if (item == SECTION_DISPLAY)    return "Display";
    if (item == SECTION_NOTIFICATIONS) return "Notifications";
    if (item == SECTION_SOUND)      return "Sound";
    if (item == SECTION_HOME_PAGES) return "Home Pages";
    if (item == SECTION_RADIO)      return "Radio";
    if (item == SECTION_SYSTEM)     return "System";
    if (item == SECTION_BLUETOOTH)  return "Bluetooth";
    if (item == SECTION_KEYBOARD)   return "Keyboard";
    if (item == SECTION_CONTACTS)   return "Contacts";
#if SOLO_FEAT_CHILD_MODE
    if (item == SECTION_CHILD)      return "Child Mode";
#endif
    if (item == SECTION_QUICK_REPLIES) return "Quick Replies";
    return "";
  }

  // Walk the SettingItem enum once, bucketing items under their section header.
  // #if-guarded items need no special handling — they simply aren't in the enum.
  void buildSections() {
    int cur = -1;
    for (int i = 0; i < (int)Count; i++) {
      if (isSection(i)) {
        if (++cur >= NUM_SECTIONS) break;
        _sec_header[cur] = (uint8_t)i;
        _sec_count[cur]  = 0;
      } else if (cur >= 0 && _sec_count[cur] < MAX_PER_SEC) {
        _sec_items[cur][_sec_count[cur]++] = (uint8_t)i;
      }
    }
    _num_sections = cur + 1;
  }

  bool isHomePage(int item) const {
    return item == HOME_RADIO      || item == HOME_BT      ||
           item == HOME_ADVERT   || item == HOME_TOOLS      ||
           item == HOME_SETTINGS   || item == HOME_QUICK_MSG ||
           item == HOME_FAVOURITES || item == HOME_SENSORS
#if ENV_INCLUDE_GPS == 1
           || item == HOME_GPS
#endif
    ;
  }

  uint16_t homePageBit(int item) const {
    int bit = homePageBitIndex(item);
    // SETTINGS and QUICK_MSG are always visible (no mask bit). All other pages
    // — including FAVOURITES — toggle via home_pages_mask.
    if (bit < 0 || bit == NodePrefs::HPB_SETTINGS || bit == NodePrefs::HPB_QUICK_MSG) return 0;
    return (uint16_t)(1 << bit);
  }

  const char* homePageLabel(int item) const {
    int bit = homePageBitIndex(item);
    return NodePrefs::homePageLabel((uint8_t)(bit >= 0 ? bit : NodePrefs::HPB_COUNT));
  }

  bool homePageVisible(int item, const NodePrefs* p) const {
    if (item == HOME_SETTINGS || item == HOME_QUICK_MSG) return true;
    uint16_t bit = homePageBit(item);
    if (!bit) return false;
    int index = homePageBitIndex(item);
    return index >= 0 && homepage::visible(p, (uint8_t)index, false);
  }

  bool homePageToggleable(int item) const {
    return item != HOME_SETTINGS && item != HOME_QUICK_MSG;
  }

  // Returns the bit-index used in page_order for this SettingItem, or -1.
  // Bit-index values are defined once in NodePrefs::HomePageBit.
  int homePageBitIndex(int item) const {
    if (item == HOME_SENSORS)   return NodePrefs::HPB_SENSORS;
    if (item == HOME_FAVOURITES) return NodePrefs::HPB_FAVOURITES;
    if (item == HOME_RADIO)     return NodePrefs::HPB_RADIO;
    if (item == HOME_BT)        return NodePrefs::HPB_BLUETOOTH;
    if (item == HOME_ADVERT)    return NodePrefs::HPB_ADVERT;
#if ENV_INCLUDE_GPS == 1
    if (item == HOME_GPS)       return NodePrefs::HPB_GPS;
#endif
    if (item == HOME_TOOLS)     return NodePrefs::HPB_TOOLS;
    if (item == HOME_SETTINGS)  return NodePrefs::HPB_SETTINGS;
    if (item == HOME_QUICK_MSG) return NodePrefs::HPB_QUICK_MSG;
    return -1;
  }

  // Returns 1-based position of item in page_order, or 0 if no custom order / not found.
  int homePagePosition(int item, const NodePrefs* p) const {
    if (!p || p->page_order_set != NodePrefs::PAGE_ORDER_MAGIC) return 0;
    int bit = homePageBitIndex(item);
    if (bit < 0) return 0;
    int position = homepage::position(p, (uint8_t)bit);
    // Clock permanently owns absolute slot 1 and is not configurable. Present
    // the remaining pages as their own 1-based sequence.
    return position > 1 ? position - 1 : 0;
  }

  // Initialises page_order to the default display sequence if not already set.
  // Also repairs a partially-initialised order where CLOCK is absent; migrates
  // older orders by inserting FAVOURITES after CLOCK; and appends any pages that
  // are absent from a stale saved order (e.g. TOOLS/MESSAGES added by later firmware).
  void ensurePageOrderInit(NodePrefs* p) const { homepage::ensureOrder(p); }

  // Swaps item's page_order slot with its neighbour in the given direction (-1=earlier, +1=later).
  void movePageInOrder(int item, int delta, NodePrefs* p) {
    int bit = homePageBitIndex(item);
    if (bit < 0) return;
    homepage::move(p, (uint8_t)bit, delta);
  }

  bool isMsgSlot(int item) const {
    return item >= MSG_SLOT_0 &&
           item < MSG_SLOT_0 + solo::QuickReplies::CUSTOM_COUNT;
  }

  int msgSlotIndex(int item) const {
    return item - MSG_SLOT_0;
  }

  void renderItem(DisplayDriver& display, int item, int y, bool sel) {
    NodePrefs* p = _task->getNodePrefs();

    drawRowSelection(display, y, sel, _reserve);

    display.setCursor(2, y);

#if FEAT_BRIGHTNESS_SETTING
    if (item == BRIGHTNESS) {
      display.print("Brightness");
      renderBar(display, valCol(display), y, (p ? p->display_brightness : 2) + 1, 5);
    } else
#endif
    if (item == NOTIFICATION_MODE) {
      display.print("Mode");
      display.setCursor(valCol(display), y);
      { static const char* labels[] = { "On", "Off", "Auto" };
        int m = _task->getNotificationMode();
        display.print(labels[m < 3 ? m : 0]); }
    } else if (item == NOTIFICATION_SCREEN_WAKE) {
      display.print("Screen Wake");
      display.setCursor(valCol(display), y);
      { static const char* labels[] = { "Off", "On", "Always" };
        uint8_t wake = p ? p->notification_screen_wake : 1;
        display.print(labels[wake < 3 ? wake : 1]); }
    } else if (item == BUZZER_VOLUME) {
      display.print("Volume");
#ifdef PIN_BUZZER
      renderBar(display, valCol(display), y, _task->getBuzzerVolume() + 1, 5);
#else
      display.setCursor(valCol(display), y);
      display.print("N/A");
#endif
    } else if (item == DM_MELODY) {
      display.print("DM Sound");
      display.setCursor(valCol(display), y);
      { uint8_t v = p ? p->notif_melody_dm : 0;
        display.print(solo::BuiltinMelodies::label(v)); }
    } else if (item == CH_MELODY) {
      display.print("Ch Sound");
      display.setCursor(valCol(display), y);
      { uint8_t v = p ? p->notif_melody_ch : 0;
        display.print(solo::BuiltinMelodies::label(v)); }
    } else if (item == NEW_CONTACT_MELODY) {
      display.print("New Sound");
      display.setCursor(valCol(display), y);
      { uint8_t v = p ? p->notif_melody_new_contact : solo::BuiltinMelodies::NONE;
        display.print(solo::BuiltinMelodies::label(v)); }
    } else if (item == AD_SOUND) {
      display.print("AD Sound");
      display.setCursor(valCol(display), y);
      { uint8_t v = p ? p->notif_melody_ad : 0;
        display.print(solo::BuiltinMelodies::label(v)); }
    } else if (item == AD_SOUND_SCOPE) {
      display.print("AD Scope");
      display.setCursor(valCol(display), y);
      { uint8_t v = p ? p->advert_sound_scope : ADVERT_SOUND_SCOPE_ALL;
        display.print(AD_SCOPE_LABELS[v < AD_SCOPE_COUNT ? v : 0]); }
#if SOLO_FEAT_QUIET_TIME
    } else if (item == QUIET_TIME) {
      display.print("Quiet Time");
      display.setCursor(valCol(display), y);
      if (!p || !p->quiet_time_enabled) display.print("Off");
      else display.print(_task->isQuietTimeActive() ? "Active" : "On");
    } else if (item == QUIET_FROM || item == QUIET_UNTIL) {
      display.print(item == QUIET_FROM ? "Quiet From" : "Quiet Until");
      int x = valCol(display);
      if (sel && _quiet_editor.active() && _quiet_edit_item == item) {
        _quiet_editor.render(display, x, y);
      } else {
        char buf[6];
        quiettime::formatTime(buf, sizeof(buf),
                              p ? (item == QUIET_FROM ? p->quiet_time_start_min
                                                     : p->quiet_time_end_min) : 0);
        display.setCursor(x, y);
        display.print(buf);
      }
#endif
    } else if (isHomePage(item)) {
      if (p) ensurePageOrderInit(p);
      int pos = homePagePosition(item, p);
      if (pos > 0) {
        char pb[5]; snprintf(pb, sizeof(pb), "%2d ", pos);
        display.print(pb);
      }
      display.print(homePageLabel(item));
      display.setCursor(display.width() - 6 * display.getCharWidth() - _reserve, y);
      if (!homePageToggleable(item))
        display.print("Always");
      else
        display.print(homePageVisible(item, p) ? "On" : "Off");
    } else if (item == TX_POWER) {
      display.print("TX Pwr");
      char buf[8];
      snprintf(buf, sizeof(buf),"%ddBm", p ? p->tx_power_dbm : 0);
      display.setCursor(valCol(display), y);
      display.print(buf);
    } else if (item == RADIO_PRESET) {
      display.print("Preset");
      const char* name = p ? _picker.currentName(p, radioTarget(p)) : "Custom";
      int xc = valCol(display);
      display.drawTextEllipsized(xc, y, display.width() - xc - _reserve, name, sel);
    } else if (item == CUSTOM_FREQ) {
      display.print("Freq");
      int xc = valCol(display);
      if (sel && _editor.active()) {
        _editor.render(display, xc, y);
      } else {
        char buf[10];
        snprintf(buf, sizeof(buf), "%.3f", p ? p->freq : 0.0f);
        display.setCursor(xc, y);
        display.print(buf);
      }
    } else if (item == CUSTOM_SF) {
      display.print("SF");
      char buf[6];
      snprintf(buf, sizeof(buf), "%d", p ? (int)p->sf : 0);
      display.setCursor(valCol(display), y);
      display.print(buf);
    } else if (item == CUSTOM_BW) {
      display.print("BW");
      char buf[10];
      snprintf(buf, sizeof(buf), "%.1f", p ? p->bw : 0.0f);
      display.setCursor(valCol(display), y);
      display.print(buf);
    } else if (item == CUSTOM_CR) {
      display.print("CR");
      char buf[6];
      snprintf(buf, sizeof(buf), "%d", p ? (int)p->cr : 0);
      display.setCursor(valCol(display), y);
      display.print(buf);
#if AUTO_OFF_MILLIS > 0
    } else if (item == AUTO_OFF) {
      display.print("Auto-off");
      display.setCursor(valCol(display), y);
      display.print(AUTO_OFF_LABELS[autoOffIndex()]);
#endif
    } else if (item == TIMEZONE) {
      display.print("Time Zone");
      display.setCursor(valCol(display), y);
      if (p && p->timezone_mode == solo::TimezonePolicy::CITY) {
        display.print(solo::TimezonePolicy::cityName(p->timezone_city));
      } else {
        char buf[8];
        int mins = p ? p->timezone_manual_min : 0;
        int magnitude = mins < 0 ? -mins : mins;
        snprintf(buf, sizeof(buf), "%c%02d:%02d", mins < 0 ? '-' : '+',
                 magnitude / 60, magnitude % 60);
        display.print(buf);
      }
#if ENV_INCLUDE_GPS == 1
    } else if (item == GPS_ENABLED) {
      display.print("GPS");
      display.setCursor(valCol(display), y);
      display.print(_gps_pending_enabled ? "On" : "Off");
    } else if (item == GPS_POLLING) {
      display.print("GPS Polling");
      display.setCursor(valCol(display), y);
      display.print(solo::GpsMode::pollingLabel(_gps_pending_polling));
#endif
    } else if (item == BLUETOOTH_ENABLED) {
      display.print("Bluetooth");
      display.setCursor(valCol(display), y);
      display.print((p && p->bluetooth_enabled) ? "On" : "Off");
    } else if (item == BLUETOOTH_PIN_MODE) {
      display.print("PIN Mode");
      display.setCursor(valCol(display), y);
      display.print((p && p->ble_pin) ? "Fixed" : "Random");
    } else if (item == BLUETOOTH_PIN) {
      display.print("PIN");
      display.setCursor(valCol(display), y);
      if (p && p->ble_pin) {
        char pin[8];
        snprintf(pin, sizeof(pin), "%06lu", (unsigned long)p->ble_pin);
        display.print(pin);
      } else {
        display.print("Random");
      }
    } else if (item == UNITS) {
      display.print("Units");
      display.setCursor(valCol(display), y);
      display.print((p && p->units_imperial) ? "Imperial" : "Metric");
    } else if (item == DEVICE_NAME) {
      display.print("Name");
      int vx = valCol(display);
      display.drawTextEllipsized(vx, y, display.width() - vx - _reserve,
                                 the_mesh.getNodeName(), sel);
    } else if (item == IMPORT_MESHCORE) {
      display.print("Import MeshCore");
    } else if (item == REBOOT) {
      display.print("Reboot");   // action row: Enter reboots this device
    } else if (item == KEYBOARD_TYPE) {
      display.print("Type");
      display.setCursor(valCol(display), y);
      display.print((p && p->keyboard_type) ? "T9" : "ABC");
#if defined(CARDKB_ADDRESS) && SOLO_FEAT_CARDKB
    } else if (item == KEYBOARD_CARDKB_STATUS) {
      display.print("CardKB");
      display.setCursor(valCol(display), y);
      display.print(_task->isCardKBConnected() ? "Found" : "Missing");
#endif
    } else if (item == BATT_DISPLAY) {
      display.print("Battery");
      display.setCursor(valCol(display), y);
      uint8_t mode = p ? p->batt_display_mode : 0;
      display.print(BATT_DISPLAY_LABELS[mode < BATT_DISPLAY_COUNT ? mode : 0]);
#if FEAT_CLOCK_SECONDS_SETTING
    } else if (item == CLOCK_SECONDS) {
      display.print("Seconds");
      display.setCursor(valCol(display), y);
      display.print((p && p->clock_hide_seconds) ? "Off" : "On");
#endif
    } else if (item == CLOCK_FORMAT) {
      display.print("Format");
      display.setCursor(valCol(display), y);
      display.print((p && p->clock_12h) ? "12h" : "24h");
#if FEAT_DISPLAY_ROTATION_SETTING
    } else if (item == ROTATION) {
      display.print("Rotation");
      display.setCursor(valCol(display), y);
      display.print(rotLabel(p ? p->display_rotation : 0));
#endif
#if FEAT_JOYSTICK_ROTATION_SETTING
    } else if (item == JOY_ROTATION) {
      display.print("Joystick");
      display.setCursor(valCol(display), y);
      display.print(rotLabel(p ? p->joystick_rotation : 0));
#endif
#if FEAT_FULL_REFRESH_SETTING
    } else if (item == EINK_FULL_REFRESH) {
      display.print("Full Refresh");
      display.setCursor(valCol(display), y);
      { uint8_t idx = p ? p->eink_full_refresh_every : 0;
        if (idx >= EINK_FULL_REFRESH_COUNT) idx = 0;
        display.print(EINK_FULL_REFRESH_LABELS[idx]); }
#endif
    } else if (item == DM_FILTER) {
      display.print("DMs");
      display.setCursor(valCol(display), y);
      display.print((p && p->dm_show_all) ? "All" : "Fav");
    } else if (item == CH_FILTER) {
      display.print("Channels");
      display.setCursor(valCol(display), y);
      display.print((p && p->ch_fav_only) ? "Fav" : "All");
    } else if (item == ROOM_FILTER) {
      display.print("Rooms");
      display.setCursor(valCol(display), y);
      display.print((p && p->room_fav_only) ? "Fav" : "All");
#if SOLO_FEAT_CHILD_MODE
    } else if (item == CHILD_ENABLED) {
      display.print("Enabled"); display.setCursor(valCol(display), y);
      display.print((p && p->child_mode_enabled) ? "On" : "Off");
    } else if (item == CHILD_PIN) {
      display.print("Set PIN"); display.setCursor(valCol(display), y); display.print("******");
    } else if (item == CHILD_CHANNELS) {
      display.print("Channels"); display.setCursor(valCol(display), y);
      display.print((p && p->child_channels_enabled) ? "On" : "Off");
    } else if (item == CHILD_ROOMS) {
      display.print("Rooms"); display.setCursor(valCol(display), y);
      display.print((p && p->child_rooms_enabled) ? "On" : "Off");
    } else if (item == CHILD_FAVOURITES) {
      uint16_t bit = NodePrefs::HP_FAVOURITES;
      display.print("Favourites");
      display.setCursor(valCol(display), y);
      display.print((p && (p->child_visible_pages & bit)) ? "On" : "Off");
#endif
    } else if (isMsgSlot(item)) {
      int slot = msgSlotIndex(item);
      char label[10];
      snprintf(label, sizeof(label), "Custom %d", slot + 1);
      display.print(label);
      const char* tmpl = (p && p->custom_msgs[slot][0]) ? p->custom_msgs[slot] : "(empty)";
      int xm = valCol(display);
      display.drawTextEllipsized(xm, y, display.width() - xm - _reserve, tmpl, sel);
    }
  }

  // Keyboard state for editing message slots
  int            _edit_slot = -1;  // -1 = not editing, 0..9 = slot being edited
  bool           _edit_name = false;  // editing DEVICE_NAME via the keyboard
  KeyboardWidget* _kb;

  // Radio preset picker — names are too long for the value column, so Enter on
  // RADIO_PRESET opens it as a full-width scrollable list instead of cycling.
  // Shared with Tools › Repeater (see RadioPresetPicker.h). _picker.saving means
  // _kb is open to name a new preset, not a message slot.
  RadioPresetPicker _picker;

  // Manual radio-parameter editing (digit-by-digit Freq editor + SF/BW/CR
  // stepping), shared with Tools › Repeater — see RadioParamsEditor.h.
  RadioParamsEditor _editor;
  DigitEditor _child_pin;
  DigitEditor _bluetooth_pin;
  uint32_t _child_pin_first_hash = 0;
  bool _child_pin_confirming = false;
  bool _child_warning_active = false;
  bool _child_warning_enable = false;
  bool _import_warning_active = false;
  bool _import_warning_yes = false;
  TimeOfDayEditor _quiet_editor;
  TimezoneEditor _timezone_editor;
  int _quiet_edit_item = -1;

  void commitStagedChanges() {
    bool gps_changed = false;
#if ENV_INCLUDE_GPS == 1
    gps_changed = _gps_dirty;
    if (gps_changed) {
      NodePrefs* p = _task->getNodePrefs();
      p->gps_enabled = _gps_pending_enabled;
      p->gps_interval = solo::GpsMode::pollingInterval(_gps_pending_polling);
      p->gps_adaptive = solo::GpsMode::pollingIsAdaptive(_gps_pending_polling);
      _task->applyGpsPrefs();
    }
#endif
    bool bluetooth_changed = _bluetooth_dirty;
    if (bluetooth_changed) _task->applyBluetoothPrefs();

    NodePrefs* p = _task->getNodePrefs();
    bool save_dirty = p && (!_have_initial_prefs ||
                            prefsFingerprint(*p) != _initial_prefs_fingerprint);
    _task->savePrefsIfDirty(save_dirty);
    _dirty = false;
#if ENV_INCLUDE_GPS == 1
    _gps_dirty = false;
#endif
    _bluetooth_dirty = false;
  }

public:
  SettingsScreen(UITask* task, KeyboardWidget* kb)
    : _task(task), _kb(kb) {
    buildSections();
  }

  // Capture staged fields without applying hardware changes immediately; the
  // caller is about to power off or reboot and performs the single prefs write.
  void prepareForShutdown() {
#if ENV_INCLUDE_GPS == 1
    if (_gps_dirty && _task->getNodePrefs()) {
      NodePrefs* p = _task->getNodePrefs();
      p->gps_enabled = _gps_pending_enabled;
      p->gps_interval = solo::GpsMode::pollingInterval(_gps_pending_polling);
      p->gps_adaptive = solo::GpsMode::pollingIsAdaptive(_gps_pending_polling);
    }
    _gps_dirty = false;
#endif
    _bluetooth_dirty = false;
    _dirty = false;
  }


  void onShow() override {
    _dirty = false;
    NodePrefs* p = _task->getNodePrefs();
    _have_initial_prefs = p != nullptr;
    if (p) _initial_prefs_fingerprint = prefsFingerprint(*p);
#if ENV_INCLUDE_GPS == 1
    _gps_dirty = false;
    _gps_initial_enabled = p && p->gps_enabled;
    _gps_pending_enabled = _gps_initial_enabled;
    _gps_initial_polling = solo::GpsMode::pollingFromPrefs(
        p ? p->gps_interval : 0, p && p->gps_adaptive);
    _gps_pending_polling = _gps_initial_polling;
#endif
    _bluetooth_dirty = false;
    _bluetooth_initial = p ? p->bluetooth_enabled : 1;
    _edit_name = false;
    _edit_slot = -1;
    _picker.menu.active = false;
    _picker.saving = false;
    _picker.deleting = false;
    _picker.confirm_slot = -1;
    _child_pin.active = false;
    _bluetooth_pin.active = false;
    _child_pin_confirming = false;
    _child_pin_first_hash = 0;
    _child_warning_active = false;
    _import_warning_active = false;
    _quiet_edit_item = -1;
    _quiet_editor.editing = false;
    _timezone_editor.close();
    _active_section = (_open_section >= 0 && _open_section < _num_sections)
                        ? _open_section : 0;
    _open_section = -1;
    _section_sel = 0;
    if (_open_item >= 0) {
      for (int i = 0; i < _sec_count[_active_section]; i++) {
        if (_sec_items[_active_section][i] == _open_item) {
          _section_sel = i;
          break;
        }
      }
    }
    _open_item = -1;
    _section_scroll = 0;
    _editor.freq.active = false;
  }

  void onHide() override {
    _task->stopMelody();
    commitStagedChanges();
  }

  int sectionCount() const { return _num_sections; }
  const char* sectionLabel(int index) const {
    return (index >= 0 && index < _num_sections)
             ? sectionName(_sec_header[index]) : "";
  }
  void openSection(int index) { _open_section = index; }
  void openRadioSettings() {
    for (int section = 0; section < _num_sections; section++) {
      if (_sec_header[section] == SECTION_RADIO) {
        _open_section = section;
        return;
      }
    }
  }
  void openBluetoothSettings() {
    for (int section = 0; section < _num_sections; section++) {
      if (_sec_header[section] == SECTION_BLUETOOTH) {
        _open_section = section;
        return;
      }
    }
  }
#if ENV_INCLUDE_GPS == 1
  void openGpsPolling() {
    for (int section = 0; section < _num_sections; section++) {
      for (int row = 0; row < _sec_count[section]; row++) {
        if (_sec_items[section][row] == GPS_POLLING) {
          _open_section = section;
          _open_item = GPS_POLLING;
          return;
        }
      }
    }
  }
#endif

  int render(DisplayDriver& display) override {
    display.setTextSize(1);

    if (_timezone_editor.active()) {
      NodePrefs* prefs = _task->getNodePrefs();
      if (prefs) _timezone_editor.render(display, *prefs, _task->currentUtcTime());
      return 1000;
    }

    if (_child_warning_active) {
      display.drawCenteredHeader("CAUTION");
      int y = display.listStart();
      display.drawTextCentered(display.width() / 2, y, "If you forget the");
      display.drawTextCentered(display.width() / 2, y + display.lineStep(), "PIN, the device must");
      display.drawTextCentered(display.width() / 2, y + display.lineStep() * 2, "be ERASED & REFLASHED");
      int oy = display.height() - display.lineStep();
      int half = display.width() / 2;
      display.drawSelectionRow(0, oy - 1, half - 1, display.getLineHeight() + 1, _child_warning_enable);
      display.drawTextCentered(half / 2, oy, "Enable");
      display.drawSelectionRow(half, oy - 1, half - 1, display.getLineHeight() + 1, !_child_warning_enable);
      display.drawTextCentered(half + half / 2, oy, "Cancel");
      display.setColor(DisplayDriver::LIGHT);
      return 0;
    }
    if (_import_warning_active) {
      display.drawCenteredHeader("Import MeshCore");
      int y = display.listStart();
      display.drawTextCentered(display.width() / 2, y, "Restore MeshCore");
      display.drawTextCentered(display.width() / 2, y + display.lineStep(), "radio and device");
      display.drawTextCentered(display.width() / 2, y + display.lineStep() * 2, "Zen options kept.");
      int oy = display.height() - display.lineStep();
      int half = display.width() / 2;
      display.drawSelectionRow(0, oy - 1, half - 1, display.getLineHeight() + 1, _import_warning_yes);
      display.drawTextCentered(half / 2, oy, "Import");
      display.drawSelectionRow(half, oy - 1, half - 1, display.getLineHeight() + 1, !_import_warning_yes);
      display.drawTextCentered(half + half / 2, oy, "Cancel");
      display.setColor(DisplayDriver::LIGHT);
      return 0;
    }
    if (_child_pin.active) {
      display.drawCenteredHeader(_child_pin_confirming ? "Confirm PIN" : "Set Child PIN");
      childmode::renderPinEditor(display, _child_pin, display.valCol(), display.height() / 2);
      return 0;
    }
    if (_bluetooth_pin.active) {
      display.drawCenteredHeader("Bluetooth PIN");
      _bluetooth_pin.render(display, display.valCol(), display.height() / 2);
      return 0;
    }
    if (_edit_slot >= 0 || _edit_name || _picker.saving) {
      return _kb->render(display);
    }

    display.drawCenteredHeader(sectionName(_sec_header[_active_section]));
    drawList(display, _sec_count[_active_section], _section_sel, _section_scroll,
      [&](int item, int y, bool sel, int reserve) {
        _reserve = reserve;
        renderItem(display, _sec_items[_active_section][item], y, sel);
      });

    if (_picker.menu.active) _picker.menu.render(display);

    return 2000;
  }

  bool handleInput(char c) override {
    if (_task->isChildModeLocked()) {
      _task->gotoHomeScreen();
      return true;
    }
    if (_import_warning_active) {
      if (keyIsPrev(c) || keyIsNext(c) || c == KEY_UP || c == KEY_DOWN) {
        _import_warning_yes = !_import_warning_yes;
      } else if (c == KEY_ENTER) {
        _import_warning_active = false;
        if (_import_warning_yes) {
          commitStagedChanges();
          if (the_mesh.restoreMeshCorePrefs()) {
            _task->showAlert("Settings imported", 900);
            _task->shutdown(true); // apply imported radio and device settings
          } else {
            _task->logWarning("MeshCore import", "Invalid or unavailable prefs");
          }
        }
      } else if (c == KEY_CANCEL) {
        _import_warning_active = false;
      }
      return true;
    }
    if (_child_warning_active) {
      if (keyIsPrev(c) || keyIsNext(c) || c == KEY_UP || c == KEY_DOWN) {
        _child_warning_enable = !_child_warning_enable;
      } else if (c == KEY_ENTER) {
        if (_child_warning_enable) {
          NodePrefs* p = _task->getNodePrefs();
          if (p) {
            _task->setChildAdminUnlocked(true);  // finish this authenticated settings visit
            p->child_mode_enabled = 1;
            _dirty = true;
          }
        }
        _child_warning_active = false;
      } else if (c == KEY_CANCEL) {
        _child_warning_active = false;
      }
      return true;
    }
    if (_child_pin.active) {
      DigitEditor::Result r = _child_pin.handleInput(c);
      if (r == DigitEditor::DONE) {
        uint32_t hash = childmode::pinHash((uint32_t)_child_pin.value);
        if (!_child_pin_confirming) {
          _child_pin_first_hash = hash;
          _child_pin_confirming = true;
          _child_pin.begin(0, 0, 999999, 6, 0);
        } else if (hash == _child_pin_first_hash) {
          NodePrefs* p = _task->getNodePrefs();
          if (p) { p->child_mode_pin_hash = hash; _dirty = true; }
          _child_pin_confirming = false;
          _child_pin_first_hash = 0;
          _task->showAlert("PIN saved", 800);
        } else {
          _child_pin_confirming = false;
          _child_pin_first_hash = 0;
          _task->logWarning("Child PIN", "PINs did not match");
        }
      } else if (r == DigitEditor::CANCELLED) {
        _child_pin_confirming = false;
        _child_pin_first_hash = 0;
      }
      return true;
    }
    if (_bluetooth_pin.active) {
      DigitEditor::Result r = _bluetooth_pin.handleInput(c);
      if (r == DigitEditor::DONE) {
        NodePrefs* prefs = _task->getNodePrefs();
        if (prefs) {
          prefs->ble_pin = (uint32_t)_bluetooth_pin.value;
          _dirty = true;
          _task->showAlert("PIN after reboot", 1200);
        }
      }
      return true;
    }
    NodePrefs* p = _task->getNodePrefs();

    if (_timezone_editor.active()) {
      if (!p) { _timezone_editor.close(); return true; }
      uint8_t old_mode = p->timezone_mode;
      uint8_t old_city = p->timezone_city;
      int16_t old_offset = p->timezone_manual_min;
      bool handled = _timezone_editor.handleInput(c, *p);
      _dirty |= old_mode != p->timezone_mode || old_city != p->timezone_city ||
                old_offset != p->timezone_manual_min;
      // Back applies the dedicated editor. Persist once here when its value
      // changed, then refresh the screen fingerprint so leaving Settings does
      // not write the same configuration a second time.
      if (!_timezone_editor.active() && c == KEY_CANCEL) {
        bool changed = !_have_initial_prefs ||
                       prefsFingerprint(*p) != _initial_prefs_fingerprint;
        _task->savePrefsIfDirty(changed);
        _initial_prefs_fingerprint = prefsFingerprint(*p);
        _have_initial_prefs = true;
        _dirty = false;
      }
      return handled;
    }

#if SOLO_FEAT_QUIET_TIME
    if (_quiet_editor.active()) {
      TimeOfDayEditor::Result r = _quiet_editor.handleInput(c);
      if (r == TimeOfDayEditor::DONE && p) {
        if (_quiet_edit_item == QUIET_FROM) p->quiet_time_start_min = _quiet_editor.value;
        else if (_quiet_edit_item == QUIET_UNTIL) p->quiet_time_end_min = _quiet_editor.value;
        _dirty = true;
      }
      if (r != TimeOfDayEditor::NONE) _quiet_edit_item = -1;
      return true;
    }
#endif

    // Keyboard editing mode for message slots
    if (_edit_slot >= 0) {
      auto res = _kb->handleInput(c);
      if (res == KeyboardWidget::DONE) {
        if (p) {
          strncpy(p->custom_msgs[_edit_slot], _kb->buf, sizeof(p->custom_msgs[0]) - 1);
          p->custom_msgs[_edit_slot][sizeof(p->custom_msgs[0]) - 1] = '\0';
          _dirty = true;
        }
        _edit_slot = -1;
      } else if (res == KeyboardWidget::CANCELLED) {
        _edit_slot = -1;
      }
      return true;
    }

    // Keyboard editing mode for the device name
    if (_edit_name) {
      auto res = _kb->handleInput(c);
      if (res == KeyboardWidget::DONE) {
        if (p) {
          strncpy(p->node_name, _kb->buf, sizeof(p->node_name) - 1);
          p->node_name[sizeof(p->node_name) - 1] = '\0';
          _dirty = true;   // savePrefsIfDirty on exit; getNodeName()/self-advert read node_name live
        }
        _edit_name = false;
      } else if (res == KeyboardWidget::CANCELLED) {
        _edit_name = false;
      }
      return true;
    }

    // Digit-by-digit Freq editor
    if (_editor.active()) {
      if (_editor.handleFreqInput(c) && p) { _task->applyRadioParams(); _dirty = true; }
      return true;
    }

    // Keyboard editing mode for naming a new saved preset
    if (_picker.saving) {
      auto res = _kb->handleInput(c);
      if (res == KeyboardWidget::DONE) {
        if (p && _picker.save(p, _kb->buf, radioTarget(p))) {
          _dirty = true;
          _task->showAlert("Preset saved", 800);
        }
        _picker.saving = false;
      } else if (res == KeyboardWidget::CANCELLED) {
        _picker.saving = false;
      }
      return true;
    }

    // Radio preset popup (and its "delete a saved preset" sub-list)
    if (_picker.menu.active) {
      auto res = _picker.menu.handleInput(c);
      if (res == PopupMenu::SELECTED && p) {
        switch (_picker.onSelected(_picker.menu.selectedIndex(), p, radioTarget(p))) {
          case RadioPresetPicker::START_SAVE:
            _kb->begin("", (int)sizeof(p->user_radio_presets[0].name) - 1);
            _kb->clearPlaceholders();   // preset names are literal
            break;
          case RadioPresetPicker::APPLIED:
            _task->applyRadioParams();
            _dirty = true;
            break;
          case RadioPresetPicker::DELETED:
            _dirty = true;
            _task->showAlert("Preset deleted", 800);
            break;
          case RadioPresetPicker::NONE:
            break;
        }
      } else if (res == PopupMenu::CANCELLED) {
        _picker.deleting = false;
        _picker.confirm_slot = -1;
      }
      return true;
    }

    if (c == KEY_CANCEL) {
      if (p && p->child_mode_enabled) _task->setChildAdminUnlocked(false);
      _task->gotoHomeScreen();
      return true;
    }

    int item_count = _sec_count[_active_section];
    if (c == KEY_UP && item_count > 0) {
      _section_sel = _section_sel > 0 ? _section_sel - 1 : item_count - 1;
      return true;
    }
    if (c == KEY_DOWN && item_count > 0) {
      _section_sel = _section_sel + 1 < item_count ? _section_sel + 1 : 0;
      return true;
    }
    if (item_count <= 0) return false;
    _selected = _sec_items[_active_section][_section_sel];

    bool right = keyIsNext(c);
    bool left  = keyIsPrev(c);
    bool enter = (c == KEY_ENTER);

#if FEAT_BRIGHTNESS_SETTING
    if (_selected == BRIGHTNESS) {
      uint8_t lvl = _task->getBrightnessLevel();
      if (right && lvl < 4) { _task->setBrightnessLevel(lvl + 1); _dirty = true; return true; }
      if (left  && lvl > 0) { _task->setBrightnessLevel(lvl - 1); _dirty = true; return true; }
      return right || left;
    }
#endif
    if (_selected == NOTIFICATION_MODE && (left || right)) {
      // Mode changes behave like the other settings rows: redraw the value,
      // apply it immediately, and save the final difference on exit.
      _task->cycleNotificationMode(left ? -1 : 1);
      _dirty = true;
      return true;
    }
    if (_selected == NOTIFICATION_SCREEN_WAKE && p && (left || right || enter)) {
      uint8_t wake = p->notification_screen_wake < 3 ? p->notification_screen_wake : 1;
      p->notification_screen_wake = left ? (wake + 2) % 3 : (wake + 1) % 3;
      _dirty = true;
      return true;
    }
    if (_selected == BUZZER_VOLUME) {
#ifdef PIN_BUZZER
      uint8_t lvl = _task->getBuzzerVolume();
      if (right && lvl < 4) { _task->setBuzzerVolumeLevel(lvl + 1); _dirty = true; return true; }
      if (left  && lvl > 0) { _task->setBuzzerVolumeLevel(lvl - 1); _dirty = true; return true; }
#endif
      return right || left;
    }
    if (_selected == DM_MELODY && p && (left || right || enter)) {
      if (enter) _task->previewMelody(p->notif_melody_dm, solo::BuiltinMelodies::MESSAGE);
      else {
        p->notif_melody_dm = (p->notif_melody_dm + (left ? solo::BuiltinMelodies::COUNT - 1 : 1)) % solo::BuiltinMelodies::COUNT;
        _dirty = true;
      }
      return true;
    }
    if (_selected == CH_MELODY && p && (left || right || enter)) {
      if (enter) _task->previewMelody(p->notif_melody_ch, solo::BuiltinMelodies::KERPLOP);
      else {
        p->notif_melody_ch = (p->notif_melody_ch + (left ? solo::BuiltinMelodies::COUNT - 1 : 1)) % solo::BuiltinMelodies::COUNT;
        _dirty = true;
      }
      return true;
    }
    if (_selected == NEW_CONTACT_MELODY && p && (left || right || enter)) {
      if (enter) _task->previewMelody(p->notif_melody_new_contact, solo::BuiltinMelodies::NONE);
      else {
        p->notif_melody_new_contact = (p->notif_melody_new_contact +
            (left ? solo::BuiltinMelodies::COUNT - 1 : 1)) % solo::BuiltinMelodies::COUNT;
        _dirty = true;
      }
      return true;
    }
    if (_selected == AD_SOUND && p && (left || right || enter)) {
      if (enter) _task->previewMelody(p->notif_melody_ad, solo::BuiltinMelodies::MESSAGE);
      else {
        p->notif_melody_ad = (p->notif_melody_ad + (left ? solo::BuiltinMelodies::COUNT - 1 : 1)) % solo::BuiltinMelodies::COUNT;
        _dirty = true;
      }
      return true;
    }
    if (_selected == AD_SOUND_SCOPE && p && (left || right || enter)) {
      p->advert_sound_scope ^= 1;
      _dirty = true; return true;
    }
#if SOLO_FEAT_QUIET_TIME
    if (_selected == QUIET_TIME && p && (left || right || enter)) {
      p->quiet_time_enabled ^= 1;
      _dirty = true; return true;
    }
    if ((_selected == QUIET_FROM || _selected == QUIET_UNTIL) && p && enter) {
      _quiet_edit_item = _selected;
      _quiet_editor.begin(_selected == QUIET_FROM ? p->quiet_time_start_min
                                                  : p->quiet_time_end_min);
      return true;
    }
#endif
    if (isHomePage(_selected) && p) {
      if (left || right) {
        movePageInOrder(_selected, left ? -1 : 1, p);
        _dirty = true;
        return true;
      }
      if (enter && homePageToggleable(_selected)) {
        if (!p->home_pages_mask) p->home_pages_mask = NodePrefs::HP_ALL;
        p->home_pages_mask ^= homePageBit(_selected);
        _dirty = true;
        return true;
      }
      return enter;
    }
    if (_selected == TX_POWER && p) {
      if (right && p->tx_power_dbm < 22) { p->tx_power_dbm++; _task->applyTxPower(); _dirty = true; return true; }
      if (left  && p->tx_power_dbm > 2)  { p->tx_power_dbm--; _task->applyTxPower(); _dirty = true; return true; }
    }
    if (_selected == RADIO_PRESET && p && enter) {
      _picker.open(p, radioTarget(p), "Radio Preset");
      return true;
    }
    // Enter Freq's digit-by-digit editor. Bounds come from the radio driver
    // itself (RadioLib's own validated range for this chip) so a digit can never
    // be nudged to a value setFrequency() would reject.
    if (_selected == CUSTOM_FREQ && p && enter) {
      float min_mhz, max_mhz;
      radio_driver.getFreqBounds(min_mhz, max_mhz);
      _editor.beginFreq(p->freq, min_mhz, max_mhz);
      return true;
    }
    int dir = right ? 1 : (left ? -1 : 0);
    if (_selected == CUSTOM_SF && p && dir && RadioParamsEditor::stepSF(p->sf, dir)) { _task->applyRadioParams(); _dirty = true; return true; }
    if (_selected == CUSTOM_BW && p && dir && RadioParamsEditor::stepBW(p->bw, dir)) { _task->applyRadioParams(); _dirty = true; return true; }
    if (_selected == CUSTOM_CR && p && dir && RadioParamsEditor::stepCR(p->cr, dir)) { _task->applyRadioParams(); _dirty = true; return true; }
#if AUTO_OFF_MILLIS > 0
    if (_selected == AUTO_OFF && p) {
      int idx = autoOffIndex();
      if (right) idx = (idx + 1) % AUTO_OFF_COUNT;
      if (left)  idx = (idx + AUTO_OFF_COUNT - 1) % AUTO_OFF_COUNT;
      if (left || right) { p->auto_off_secs = AUTO_OFF_OPTS[idx]; _dirty = true; return true; }
    }
#endif
    if (_selected == TIMEZONE && p && enter) {
      _timezone_editor.begin();
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (_selected == GPS_ENABLED && p && (left || right || enter)) {
      _gps_pending_enabled = !_gps_pending_enabled;
      _gps_dirty = _gps_pending_enabled != _gps_initial_enabled ||
                   _gps_pending_polling != _gps_initial_polling;
      return true;
    }
    if (_selected == GPS_POLLING && p && (left || right)) {
      int choice = _gps_pending_polling;
      if (left) choice = (choice + solo::GpsMode::POLLING_COUNT - 1) % solo::GpsMode::POLLING_COUNT;
      else choice = (choice + 1) % solo::GpsMode::POLLING_COUNT;
      _gps_pending_polling = (uint8_t)choice;
      _gps_dirty = _gps_pending_enabled != _gps_initial_enabled ||
                   _gps_pending_polling != _gps_initial_polling;
      return true;
    }
#endif
    if (_selected == BLUETOOTH_ENABLED && p && (left || right || enter)) {
      p->bluetooth_enabled ^= 1;
      _bluetooth_dirty = p->bluetooth_enabled != _bluetooth_initial;
      return true;
    }
    if (_selected == BLUETOOTH_PIN_MODE && p && (left || right || enter)) {
      if (p->ble_pin) {
        p->ble_pin = 0;
      } else {
        uint32_t active = the_mesh.getBLEPin();
        p->ble_pin = active >= 100000 && active <= 999999 ? active : 123456;
      }
      _dirty = true;
      return true;
    }
    if (_selected == BLUETOOTH_PIN && p && enter) {
      uint32_t pin = p->ble_pin;
      if (pin < 100000 || pin > 999999) {
        uint32_t active = the_mesh.getBLEPin();
        pin = active >= 100000 && active <= 999999 ? active : 123456;
      }
      _bluetooth_pin.begin(pin, 100000, 999999, 6, 0);
      return true;
    }
    if (_selected == UNITS && p && (left || right || enter)) {
      p->units_imperial ^= 1;
      _dirty = true;
      return true;
    }
    if (_selected == DEVICE_NAME && p && enter) {
      _edit_name = true;
      _kb->begin(the_mesh.getNodeName(), (int)sizeof(p->node_name) - 1);
      _kb->clearPlaceholders();   // a device name is literal, not a message
      return true;
    }
    if (_selected == REBOOT && enter) {
      _task->showAlert("Rebooting...", 800);
      _task->shutdown(true);
      return true;
    }
    if (_selected == IMPORT_MESHCORE && enter) {
      if (!the_mesh.hasMeshCorePrefs()) {
        _task->logWarning("MeshCore import", "No /prefs.json found");
      } else {
        _import_warning_active = true;
        _import_warning_yes = false;
      }
      return true;
    }
    if (_selected == KEYBOARD_TYPE && p && (left || right || enter)) {
      p->keyboard_type ^= 1;
      _dirty = true;
      return true;
    }
    if (_selected == BATT_DISPLAY && p) {
      int idx = p->batt_display_mode < BATT_DISPLAY_COUNT ? p->batt_display_mode : 0;
      if (right) idx = (idx + 1) % BATT_DISPLAY_COUNT;
      if (left)  idx = (idx + BATT_DISPLAY_COUNT - 1) % BATT_DISPLAY_COUNT;
      if (left || right) { p->batt_display_mode = idx; _dirty = true; return true; }
    }
#if FEAT_CLOCK_SECONDS_SETTING
    if (_selected == CLOCK_SECONDS && p && (left || right || enter)) {
      p->clock_hide_seconds ^= 1;
      _dirty = true;
      return true;
    }
#endif
    if (_selected == CLOCK_FORMAT && p && (left || right || enter)) {
      p->clock_12h ^= 1;
      _dirty = true;
      return true;
    }
#if FEAT_DISPLAY_ROTATION_SETTING
    if (_selected == ROTATION && p && (left || right)) {
      p->display_rotation = (p->display_rotation + (left ? 3 : 1)) & 3;
      _task->applyRotation();
      _dirty = true;
      return true;
    }
#endif
#if FEAT_JOYSTICK_ROTATION_SETTING
    if (_selected == JOY_ROTATION && p && (left || right)) {
      p->joystick_rotation = (p->joystick_rotation + (left ? 3 : 1)) & 3;
      _dirty = true;
      return true;
    }
#endif
#if FEAT_FULL_REFRESH_SETTING
    if (_selected == EINK_FULL_REFRESH && p && (left || right)) {
      int idx = p->eink_full_refresh_every;
      if (idx >= EINK_FULL_REFRESH_COUNT) idx = 0;
      idx = (idx + (left ? EINK_FULL_REFRESH_COUNT - 1 : 1)) % EINK_FULL_REFRESH_COUNT;
      p->eink_full_refresh_every = idx;
      _task->applyFullRefreshInterval();
      _dirty = true;
      return true;
    }
#endif
    if (_selected == DM_FILTER && p && (left || right || enter)) {
      p->dm_show_all = p->dm_show_all ? 0 : 1;
      _dirty = true;
      return true;
    }
    if (_selected == CH_FILTER && p && (left || right || enter)) {
      p->ch_fav_only = p->ch_fav_only ? 0 : 1;
      _dirty = true;
      return true;
    }
    if (_selected == ROOM_FILTER && p && (left || right || enter)) {
      p->room_fav_only = p->room_fav_only ? 0 : 1;
      _dirty = true;
      return true;
    }
#if SOLO_FEAT_CHILD_MODE
    if (_selected == CHILD_PIN && p && enter) {
      _child_pin_confirming = false;
      _child_pin_first_hash = 0;
      _child_pin.begin(0, 0, 999999, 6, 0);
      return true;
    }
    if (_selected == CHILD_ENABLED && p && (left || right || enter)) {
      if (!p->child_mode_enabled && p->child_mode_pin_hash == 0) {
        _task->logWarning("Child Mode", "Set PIN first");
        return true;
      }
      if (p->child_mode_enabled) {
        p->child_mode_enabled = 0;
        _dirty = true;
      } else {
        _child_warning_active = true;
        _child_warning_enable = false;  // confirmation defaults to Cancel
      }
      // The parent remains in this settings session. Leaving Settings locks it.
      return true;
    }
    if (_selected == CHILD_FAVOURITES && p && (left || right || enter)) {
      uint16_t bit = NodePrefs::HP_FAVOURITES;
      p->child_visible_pages ^= bit;
      _dirty = true;
      return true;
    }
    if (_selected == CHILD_CHANNELS && p && (left || right || enter)) {
      p->child_channels_enabled ^= 1;
      _dirty = true;
      return true;
    }
    if (_selected == CHILD_ROOMS && p && (left || right || enter)) {
      p->child_rooms_enabled ^= 1;
      _dirty = true;
      if (p->child_rooms_enabled)
        _task->logWarning("Child Rooms", "All room users allowed");
      return true;
    }
#endif
    if (isMsgSlot(_selected) && enter) {
      int slot = msgSlotIndex(_selected);
      _edit_slot = slot;
      // Bound to the custom_msgs store (140 B) so the wider keyboard buffer
      // can't overflow it on save.
      messageeditor::begin(*_kb, p ? p->custom_msgs[slot] : "",
                           p ? (int)sizeof(p->custom_msgs[slot]) - 1 : KB_MAX_LEN,
                           &sensors);
      return true;
    }
    return false;
  }
};

#if AUTO_OFF_MILLIS > 0
const uint16_t SettingsScreen::AUTO_OFF_OPTS[5]   = { 5, 15, 30, 60, 0 };
const char*    SettingsScreen::AUTO_OFF_LABELS[5]  = { "5s", "15s", "30s", "60s", "Never" };
#endif
const char*    SettingsScreen::BATT_DISPLAY_LABELS[3] = { "Icon", "%", "V" };
const char*    SettingsScreen::AD_SCOPE_LABELS[2] = { "All", "Zero-hop" };
#if FEAT_FULL_REFRESH_SETTING
const char* SettingsScreen::EINK_FULL_REFRESH_LABELS[5] = { "Off", "5", "10", "20", "30" };
#endif
