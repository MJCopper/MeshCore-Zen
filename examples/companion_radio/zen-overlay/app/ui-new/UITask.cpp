#include "UITask.h"
#include "SoundNotifier.h"
#include "HomePageRegistry.h"
#include "../zen/NotificationPreferences.h"
#include "../zen/NotificationPolicy.h"
#include "../zen/TimeDeadline.h"
#include "../zen/GpsCourse.h"
#include "../zen/PinAttemptLimiter.h"
#include <helpers/TxtDataHelpers.h>
#include <helpers/UTF8Helpers.h>
#include "../MyMesh.h"
#include "../MsgExpand.h"
#include "../Features.h"
#include "../GeoUtils.h"
#include "target.h"
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#if UI_HAS_JOYSTICK
  #define TOGGLE_HINT "Enter: Toggle"
  #define ADVERT_HINT "Enter: Advert"
#else
  #define TOGGLE_HINT "Hold: Toggle"
  #define ADVERT_HINT "Hold: Advert"
#endif

#include "icons.h"
#include "ChildMode.h"
#include "DigitEditor.h"
#include "QuietTime.h"
#include "CardKBEditorInput.h"

uint32_t UITask::currentUtcTime() const {
  return rtc_clock.getCurrentTime();
}

// Blinking status indicators: on for the first half of a 4 s cycle, but e-ink
// can't repaint fast enough to blink, so it shows them steadily.
static inline bool blinkOn() {
  return Features::BLINK_INDICATORS ? ((millis() % 4000) < 2000) : true;
}

#if ENV_INCLUDE_GPS == 1
// Scroll a 32-point tape around the live course. Named points occupy every
// fourth slot and each intervening dot represents another 11.25° increment.
static void drawGpsCourseTape(ZenDisplayDriver& display, int y, bool available,
                              long course_millideg, zen::GpsCourse::Source source) {
  if (!available) {
    display.setColor(ZenDisplayDriver::LIGHT);
    display.drawTextCentered(display.width() / 2, y, "No course");
    return;
  }

  if (source == zen::GpsCourse::LAST || source == zen::GpsCourse::TRAVEL) {
    char label[16];
    snprintf(label, sizeof(label), "%s: %s",
             source == zen::GpsCourse::TRAVEL ? "Travel" : "Last",
             zen::GpsCourse::label(zen::GpsCourse::direction(course_millideg)));
    display.drawTextCentered(display.width() / 2, y, label);
    return;
  }

  const int pitch = display.getCharWidth() * 2 - 1;
  int visible = (display.width() - display.getCharWidth()) / pitch;
  if (visible > 31) visible = 31;
  if ((visible & 1) == 0) visible--;
  if (visible < 5) visible = 5;
  int half = visible / 2;
  uint8_t centre = zen::GpsCourseTape::index(course_millideg);
  for (int i = -half; i <= half; i++) {
    const char* label = zen::GpsCourseTape::label(
        zen::GpsCourseTape::offset(centre, (int8_t)i));
    int x = display.width() / 2 + i * pitch;
    int text_w = display.getTextWidth(label);
    if (i == 0) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.fillRect(x - text_w / 2 - 2, y - 1,
                       text_w + 4, display.getLineHeight() + 1);
      display.setColor(ZenDisplayDriver::DARK);
    } else {
      display.setColor(ZenDisplayDriver::LIGHT);
    }
    display.drawTextCentered(x, y, label);
  }
  display.setColor(ZenDisplayDriver::LIGHT);
}
#endif

class SplashScreen : public ZenUIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];
  char _zen_ver[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // MeshCore upstream version shown large (e.g. "1.15")
    strncpy(_version_info, MESHCORE_VERSION, sizeof(_version_info) - 1);
    _version_info[sizeof(_version_info) - 1] = '\0';

    // Zen firmware version: strip the commit-hash suffix build.sh always
    // appends as the LAST dash-segment (v1.15-solo.1-abcdef -> v1.15-solo.1).
    // Must be the last dash, not the first: a tag like v1.21-rc1 has a dash
    // of its own before the commit hash gets appended.
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strrchr(ver, '-');
    int plen = dash ? (int)(dash - ver) : (int)strlen(ver);
    if (plen >= (int)sizeof(_zen_ver)) plen = sizeof(_zen_ver) - 1;
    memcpy(_zen_ver, ver, plen);
    _zen_ver[plen] = '\0';

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(ZenDisplayDriver& display) override {
    display.setTextSize(1);
    const int lh = display.getLineHeight();
    const int step = display.lineStep();

    // meshcore logo
    display.setColor(ZenDisplayDriver::LIGHT);
    int logoWidth = 128;
    int logo_y = 3;
    display.drawXbm((display.width() - logoWidth) / 2, logo_y, meshcore_logo, logoWidth, 13);

    // version info at sz2
    int ver_y = logo_y + 13 + 2;
    display.setTextSize(2);
    int lh2 = display.getLineHeight();
    display.drawTextCentered(display.width()/2, ver_y, _version_info);

    // Live boot-stage diagnostic at sz1, below the MeshCore version. This is
    // more useful on-device than the compile date when a cold boot stalls.
    int date_y = ver_y + lh2 + 2;
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, date_y, _task->bootStage());

#ifdef FIRMWARE_ZEN_BUILD
    int zen_y = date_y + step;
    display.fillRect(0, zen_y - 1, display.width(), lh + 2);
    display.setColor(ZenDisplayDriver::DARK);
    char zen_label[24];
    if (_zen_ver[0])
      snprintf(zen_label, sizeof(zen_label), "Zen %s", _zen_ver);
    else
      snprintf(zen_label, sizeof(zen_label), "Zen");
    display.drawTextCentered(display.width()/2, zen_y, zen_label);
    display.setColor(ZenDisplayDriver::LIGHT);
#endif

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

class ChildUnlockScreen : public ZenUIScreen {
  UITask* _task;
  DigitEditor _pin;
  zen::PinAttemptLimiter _limiter;
public:
  ChildUnlockScreen(UITask* task) : _task(task) {}
  void onShow() override { _pin.begin(0, 0, 999999, 6, 0); }
  int render(ZenDisplayDriver& display) override {
    display.setTextSize(1);
    display.drawCenteredHeader("Parent PIN");
    display.setCursor(4, display.height() / 2 - display.getLineHeight() / 2);
    display.print("PIN:");
    childmode::renderPinEditor(display, _pin, display.valCol(),
                               display.height() / 2 - display.getLineHeight() / 2);
    return 0;
  }
  bool handleInput(char c) override {
    uint32_t wait = _limiter.remainingSeconds(millis());
    if (wait) {
      DigitEditor::Result blocked_input = _pin.handleInput(c);
      if (blocked_input == DigitEditor::CANCELLED) _task->gotoHomeScreen();
      else {
        char message[24];
        snprintf(message, sizeof(message), "Try again in %lus", (unsigned long)wait);
        _task->showAlert(message, 900);
      }
      return true;
    }
    DigitEditor::Result r = _pin.handleInput(c);
    if (r == DigitEditor::DONE) {
      ZenPrefs* p = _task->getNodePrefs();
      if (p && childmode::pinHash((uint32_t)_pin.value) == p->child_mode_pin_hash) {
        _task->setChildAdminUnlocked(true);
        _limiter.reset();
        _task->gotoHomeScreen();
      } else {
        _task->operationWarning(zen::Operation::CHILD_PIN,
                                zen::OperationReason::INCORRECT_PIN);
        uint32_t delay = _limiter.failed(millis());
        if (delay) {
          char message[24];
          snprintf(message, sizeof(message), "Try again in %lus", (unsigned long)delay);
          _task->showAlert(message, 900);
        }
        _pin.begin(0, 0, 999999, 6, 0);
      }
      return true;
    }
    if (r == DigitEditor::CANCELLED) { _task->gotoHomeScreen(); return true; }
    return true;
  }
};

// ── Screen fragments — included into THIS translation unit only ───────────────
// These headers are not standalone: they are compiled solely as part of
// UITask.cpp, in the order below. Two consequences a new screen must respect:
//   • Order matters. A `static inline` helper (drawList, msgReplyBody, geo::…)
//     or a shared scratch buffer (FullscreenMsgView's s_wrap_*) is only visible
//     to fragments included *after* the one that defines it. Add new screens
//     after their dependencies.
//   • Single-TU only. Some fragments define external-linkage symbols at file
//     scope (e.g. NearbyScreen::FILTER_LABELS), so including any of them from a
//     second .cpp is a duplicate-symbol link error. Keep them UITask-internal;
//     anything genuinely shareable belongs in a real header (icons.h, GeoUtils.h).
#include "FullscreenMsgView.h"
#include "SensorPlaceholders.h"
#include "SettingsScreen.h"
#include "ConversationStore.h" // RAM conversation rings used by MessagesScreen
#include "MessagesScreen.h"

// ── Custom screens (separate files to ease upstream merges) ───────────────────
#include "RingtoneEditorScreen.h"
#if ZEN_FEATURE_ADMIN
#include "AdminScreen.h"
#endif
#include "NearbyScreen.h"
#include "AutoAdvertScreen.h"
#include "DiagnosticsScreen.h"
#include "RepeaterScreen.h"
#include "ToolsScreen.h"

#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3200
#endif

// Render the time starting at top_y; returns the y just below the time block
// so the caller can flow the date and message-count row beneath it.
//
// On a tall portrait panel (e-ink in portrait — height > width) HH and MM are
// stacked on two lines in the huge built-in font (size 4, ~56 px tall) so the
// digits fill the narrow width. On wide panels (OLED, landscape e-ink) the
// classic single-line "HH:MM" at size 2 is kept.
static int drawClockTime(ZenDisplayDriver& d, int top_y, const struct tm* ti,
                         bool h12, bool show_sec) {
  const bool tall = d.height() > d.width();   // true only on portrait e-ink

  if (tall) {
    int hh = ti->tm_hour;
    const char* ap = nullptr;
    if (h12) { ap = (hh < 12) ? "AM" : "PM"; hh %= 12; if (hh == 0) hh = 12; }
    const int cx = d.width() / 2;
    char hbuf[4], mbuf[4];
    snprintf(hbuf, sizeof(hbuf), "%02d", hh);
    snprintf(mbuf, sizeof(mbuf), "%02d", ti->tm_min);

    int y = top_y;
    d.setTextSize(4);
    const int lhb = d.getLineHeight();
    // The built-in GFX font advances 6 px per char but the glyph is only 5 px
    // wide, so getTextWidth() over-reports by one trailing blank column and
    // drawTextCentered() would bias the digits ~half a column to the left.
    // Centre on the visible width (minus that trailing column) instead.
    const int trail = d.getCharWidth() / 6;   // one built-in column at this size
    auto drawBig = [&](const char* s, int yy) {
      int w = (int)d.getTextWidth(s) - trail;
      d.setCursor(cx - w / 2, yy);
      d.print(s);
    };
    drawBig(hbuf, y);  y += lhb + 2;
    drawBig(mbuf, y);  y += lhb + 2;
    if (ap) { d.setTextSize(2); d.drawTextCentered(cx, y, ap); y += d.getLineHeight() + 1; }
    d.setTextSize(1);
    return y;
  }

  // Wide layout: single inline line at size 2.
  char buf[16];
  d.setTextSize(2);
  const int lh2 = d.getLineHeight();
  if (h12) {
    int hh = ti->tm_hour % 12; if (hh == 0) hh = 12;
    const char* ap = (ti->tm_hour < 12) ? "AM" : "PM";
    if (show_sec) snprintf(buf, sizeof(buf), "%d:%02d:%02d%s", hh, ti->tm_min, ti->tm_sec, ap);
    else          snprintf(buf, sizeof(buf), "%d:%02d %s", hh, ti->tm_min, ap);
  } else {
    if (show_sec) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
    else          snprintf(buf, sizeof(buf), "%02d:%02d", ti->tm_hour, ti->tm_min);
  }
  d.drawTextCentered(d.width() / 2, top_y, buf);
  d.setTextSize(1);
  return top_y + lh2 + 2;
}

// Draw the boot-sync marker in the same clock/date region and return the normal
// date baseline. This keeps the separator and message-count row fixed in place.
static int drawClockSync(ZenDisplayDriver& d, int top_y, bool h12) {
  const bool tall = d.height() > d.width();
  int date_y;
  if (tall) {
    d.setTextSize(4);
    date_y = top_y + 2 * (d.getLineHeight() + 2);
    if (h12) {
      d.setTextSize(2);
      date_y += d.getLineHeight() + 1;
    }
  } else {
    d.setTextSize(2);
    date_y = top_y + d.getLineHeight() + 2;
  }

  d.setTextSize(2);
  int region_bottom = date_y + d.lineStep();
  int y = top_y + (region_bottom - top_y - d.getLineHeight()) / 2;
  d.drawTextCentered(d.width() / 2, y, "SYNC TIME");
  d.setTextSize(1);
  return date_y;
}

#include "SensorPage.h"

// ── HomeScreen ────────────────────────────────────────────────────────────────
class HomeScreen : public ZenUIScreen {
  enum HomePage {
    CLOCK,
    FAVOURITES,
    RADIO,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
    SETTINGS,
    TOOLS,
    QUICK_MSG,
    SENSORS,
    EMERGENCY,
    Count    // keep as last
  };

  // Selected slot on the four-entry Favourites page.
  uint8_t _fav_sel = 0;
  uint8_t _msg_mode_sel = 0;  // 0=Direct, 1=Channel, 2=Room Servers
  uint8_t _settings_sel = 0, _settings_scroll = 0;
  uint8_t _tools_sel = 0, _tools_scroll = 0;
  PopupMenu _msg_menu;
  PopupMenu _fav_menu;
  PopupMenu _emergency_menu;
  bool _emergency_menu_disables = false;
  SensorPage _sensor_page;
  static const uint32_t HOME_IDLE_RETURN_MS = 5UL * 60UL * 1000UL;
  uint32_t _home_idle_deadline = 0;

  void noteHomeInteraction() {
    _home_idle_deadline = millis() + HOME_IDLE_RETURN_MS;
  }

  void returnToClockIfIdle() {
    if (_page == CLOCK || _home_idle_deadline == 0 ||
        (int32_t)(millis() - _home_idle_deadline) < 0) return;
    _page = CLOCK;
    _sensor_page.close();
    _msg_menu.active = false;
    _fav_menu.active = false;
    _pin_menu.active = false;
    _pin_target_slot = -1;
  }

  void pruneStaleFavouriteSlots() {
    if (!_node_prefs) return;
    bool changed = false;
    for (uint8_t i = 0; i < ZenPrefs::FAVOURITES_DIAL_COUNT; i++) {
      const uint8_t* prefix = _node_prefs->favourite_contacts[i];
      bool filled = false;
      for (uint8_t b = 0; b < ZenPrefs::FAVOURITE_PREFIX_LEN; b++)
        if (prefix[b]) { filled = true; break; }
      if (!filled) continue;
      bool found = false;
      for (int index = 0; ; index++) {
        ContactInfo contact;
        if (!the_mesh.getContactByIdx(index, contact)) break;
        if (!memcmp(contact.id.pub_key, prefix, ZenPrefs::FAVOURITE_PREFIX_LEN)) {
          found = true;
          break;
        }
      }
      if (!found) {
        memset(_node_prefs->favourite_contacts[i], 0, ZenPrefs::FAVOURITE_PREFIX_LEN);
        changed = true;
      }
    }
    if (changed) _task->requestPrefsSave();
  }

  template <class LabelFn>
  void renderHomeList(ZenDisplayDriver& display, int content_y, int count,
                      int selected, int& scroll, LabelFn label) {
    drawListAt(display, content_y, count, selected, scroll,
        [&](int index, int y, bool active, int reserve) {
      drawRowSelection(display, y, active, reserve);
      display.drawTextEllipsized(2, y, display.width() - 4 - reserve,
                                 label(index), active);
    });
  }

  bool messageChannelsVisible() const {
    ZenPrefs* p = _task->getNodePrefs();
    return zen::Policy::channelsVisible(p, _task->isChildModeLocked());
  }
  bool messageRoomsVisible() const {
    ZenPrefs* p = _task->getNodePrefs();
    return !_task->isChildModeLocked() || (p && p->child_rooms_enabled);
  }
  int messageModeCount() const {
    return 1 + (messageChannelsVisible() ? 1 : 0) + (messageRoomsVisible() ? 1 : 0);
  }
  int messageModeAt(int pos) const {
    if (pos <= 0) return 0;
    if (messageChannelsVisible()) return pos == 1 ? 1 : 2;
    return 2;
  }
  int messageModePosition() const {
    if (_msg_mode_sel == 0) return 0;
    if (_msg_mode_sel == 1) return messageChannelsVisible() ? 1 : 0;
    if (!messageRoomsVisible()) return 0;
    return messageChannelsVisible() ? 2 : 1;
  }

  // Dial shortcuts are presentation only. The contact-list favourite flag is
  // the authoritative Child Mode permission, so only approved chats may be pinned.
  void buildPinPicker(int slot) {
    _pin_target_slot = slot;
    _pin_count = 0;
    // 1) Upstream-favourited chat contacts.
    for (int idx = 0; _pin_count < PIN_PICKER_MAX; idx++) {
      ContactInfo c;
      if (!the_mesh.getContactByIdx(idx, c)) break;
      if (c.type != ADV_TYPE_CHAT) continue;
      if (!(c.flags & 0x01)) continue;
      memcpy(_pin_keys[_pin_count], c.id.pub_key, ZenPrefs::FAVOURITE_PREFIX_LEN);
      snprintf(_pin_labels[_pin_count], sizeof(_pin_labels[_pin_count]), "%s", c.name);
      _pin_count++;
    }
    if (_pin_count == 0) {
      _task->showAlert("No contacts", 1000);
      _pin_target_slot = -1;
      return;
    }
    _pin_menu.begin("Pick contact", 3);
    for (int i = 0; i < _pin_count; i++) _pin_menu.addItem(_pin_labels[i]);
  }

  // In-place pin picker (opens when Enter hits an empty Favourites tile).
  static const int PIN_PICKER_MAX = 12;
  PopupMenu _pin_menu;
  uint8_t   _pin_keys[PIN_PICKER_MAX][ZenPrefs::FAVOURITE_PREFIX_LEN];
  char      _pin_labels[PIN_PICKER_MAX][32]; // ContactInfo::name, retained as UTF-8
  int       _pin_count = 0;
  int       _pin_target_slot = -1;

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  ZenPrefs* _node_prefs;
  uint8_t _page;
  int pageBit(int page) const {
    if (page == SENSORS)    return ZenPrefs::HPB_SENSORS;
    if (page == CLOCK)      return ZenPrefs::HPB_CLOCK;
    if (page == FAVOURITES) return ZenPrefs::HPB_FAVOURITES;
    if (page == RADIO)     return ZenPrefs::HPB_RADIO;
    if (page == BLUETOOTH) return ZenPrefs::HPB_BLUETOOTH;
    if (page == ADVERT)    return ZenPrefs::HPB_ADVERT;
#if ENV_INCLUDE_GPS == 1
    if (page == GPS)       return ZenPrefs::HPB_GPS;
#endif
    if (page == TOOLS)     return ZenPrefs::HPB_TOOLS;
    return -1;  // SETTINGS, QUICK_MSG always visible (no mask bit)
  }

  // Maps page_order bit-index back to the HomePage enum value for this build.
  // Returns -1 if the page is not compiled in.
  int bitToPage(int bit) const {
    switch (bit) {
      case ZenPrefs::HPB_SENSORS:    return SENSORS;
      case ZenPrefs::HPB_CLOCK:      return CLOCK;
      case ZenPrefs::HPB_FAVOURITES: return FAVOURITES;
      case ZenPrefs::HPB_RADIO:     return RADIO;
      case ZenPrefs::HPB_BLUETOOTH: return BLUETOOTH;
      case ZenPrefs::HPB_ADVERT:    return ADVERT;
#if ENV_INCLUDE_GPS == 1
      case ZenPrefs::HPB_GPS:       return GPS;
#endif
      case ZenPrefs::HPB_TOOLS:     return TOOLS;
      case ZenPrefs::HPB_SETTINGS:  return SETTINGS;
      case ZenPrefs::HPB_QUICK_MSG: return QUICK_MSG;
      default: return -1;
    }
  }

  bool isPageVisible(int page) const {
    if (page == EMERGENCY) return _task->isLowPowerMode();
#if ENV_INCLUDE_GPS == 1
    // Emergency Mode exposes the volatile GPS control even when the GPS home
    // page is normally hidden. Its saved mode remains unchanged.
    if (page == GPS && _task->isEmergencyMode()) return true;
#endif
    // Bluetooth is also a volatile emergency control; showing it here does
    // not alter its saved home-page visibility or enabled preference.
    if (page == BLUETOOTH && _task->isEmergencyMode() && !_task->isChildModeLocked()) return true;
    int bit = pageBit(page);
    if (bit < 0) return true;
    return homepage::visible(_node_prefs, (uint8_t)bit, _task->isChildModeLocked());
  }

  // Build ordered list of all visible pages, respecting page_order when set.
  // Returns count; out[] receives HomePage enum values.
  int buildVisibleOrder(int* out) const {
    uint8_t bits[ZenPrefs::PAGE_ORDER_LEN];
    int bit_count = homepage::visibleOrder(_node_prefs, _task->isChildModeLocked(),
                                           bits, ZenPrefs::PAGE_ORDER_LEN);
    int n = 0;
    for (int i = 0; i < bit_count && n < (int)Count; i++) {
      int pg = bitToPage(bits[i]);
      if (pg >= 0 && pg < (int)Count && isPageVisible(pg)) out[n++] = pg;
    }
    // Emergency is session-only and deliberately has no persisted page bit.
    if (isPageVisible(EMERGENCY) && n < (int)Count) out[n++] = EMERGENCY;
    return n;
  }

  int navPage(int from, int dir) const {
    int order[(int)Count]; int n = buildVisibleOrder(order);
    if (n == 0) return from;
    int cur = 0;
    for (int i = 0; i < n; i++) if (order[i] == from) { cur = i; break; }
    return order[((cur + dir) % n + n) % n];
  }

  int renderBatteryIndicator(ZenDisplayDriver& display, uint16_t batteryMilliVolts,
                             int right_x) {
    int pct = zen::BatteryPolicy::percent(batteryMilliVolts);

    uint8_t mode = (_node_prefs && _node_prefs->batt_display_mode < 3)
                     ? _node_prefs->batt_display_mode : 0;

    display.setTextSize(1);
    display.setColor(ZenDisplayDriver::LIGHT);

    const int lh      = display.getLineHeight();
    const int cw      = display.getCharWidth();
    const int ind     = cw + 2;    // single-char indicator width
    const int ind_h   = display.isSingleFont() ? lh - 2 : lh;
    const int ind_gap = display.isLandscape() ? 3 : 1;  // gap between indicator boxes

    int battLeftX;
    if (mode == 1) {  // percent
      char buf[6];
      snprintf(buf, sizeof(buf),"%d%%", pct);
      battLeftX = right_x - display.getTextWidth(buf);
      display.setCursor(battLeftX, 0);
      display.print(buf);
    } else if (mode == 2) {  // voltage
      char buf[8];
      snprintf(buf, sizeof(buf),"%u.%02uV", batteryMilliVolts / 1000, (batteryMilliVolts % 1000) / 10);
      battLeftX = right_x - display.getTextWidth(buf);
      display.setCursor(battLeftX, 0);
      display.print(buf);
    } else {  // icon — scales with lh, same box height as the status icons beside it (ind_h)
      const int iconH = ind_h;
      const int iconW = lh * 2 - 4;
      const int bm = display.isLandscape() ? 3 : 2;  // inner margin: 3px on landscape e-ink, 2px on OLED/portrait
      battLeftX = right_x - iconW - 2;
      display.drawRect(battLeftX, 0, iconW, iconH);
      // Nub height/2, vertically centred by remaining-space/2 rather than a flat
      // iconH/4 margin — the flat form only centres when iconH is a multiple of
      // 4 (true for the old built-in font's lh=8, false for misc-fixed's 7/9),
      // so it visibly drifted off-centre once the box height changed.
      const int nub_h = iconH / 2;
      display.fillRect(battLeftX + iconW, (iconH - nub_h) / 2, 2, nub_h);
      const int innerW = iconW - 2 * bm;
      int fillW = (pct * innerW) / 100;
      display.fillRect(battLeftX + bm, bm, fillW, iconH - 2 * bm);
    }

    // Secondary status icons, laid out right→left in PRIORITY order so a crowded
    // bar sheds its least-important cues instead of crushing the node name. Once
    // an icon won't fit above the reserved name area, every lower-priority icon
    // after it is dropped too (the list is ordered high→low). A blinking icon
    // still reserves its slot while off, so the name width doesn't flicker.
    //
    // Priority: mute > BT > GPS fix > auto-advert > repeater. Battery remains
    // ahead of the signal indicator. The background modes stay outside any BT gate — they
    // keep running with Bluetooth off, so their cue must not vanish with it.
    LocationProvider* loc = _sensors ? _sensors->getLocationProvider() : nullptr;
    // Reflect the receiver's live power state, including temporary boot-time
    // sync and periodic acquisition windows while the configured mode sleeps.
    bool gps_on  = loc && loc->isEnabled();
    bool mute_on = _task->isNotificationAudioMuted();
    bool advert_visible = the_mesh.advertIndicatorActive();
    struct Sicon { bool active; const MiniIcon* icon; bool boxed; bool blink; };
    const Sicon icons[] = {
      { mute_on,                                                   &ICON_MUTE,        true, false },
      { _task->isBluetoothEnabled(),
                                  &ICON_BLUETOOTH, _task->isBLEConnected(), false },
      { gps_on,                   &ICON_GPS,        gps_on && loc->isValid(),                            false },
      { advert_visible,                                             &ICON_ADVERT,      true, false },
      { _node_prefs && _node_prefs->isRepeatEn(),                 &ICON_REPEATER,    true, true  },
    };

    int x = battLeftX;
    const int name_min = display.getCharWidth() * 5;   // always keep ~5 chars for the name
    for (const Sicon& s : icons) {
      if (!s.active) continue;
      int ix = x - ind - ind_gap;
      if (ix < name_min) break;                        // out of room — drop this + all lower priority
      if (!s.blink || blinkOn()) {
        if (s.boxed) drawBoxedIcon(display, ix, ind, ind_h, *s.icon);
        else         drawSlotIcon(display, ix, ind, ind_h, *s.icon);
      }
      x = ix;
    }
    return x;
  }

  int renderRepeaterSignal(ZenDisplayDriver& display) {
    const int w = 9;
    const int x = display.width() - w;
    const int h = display.isSingleFont() ? display.getLineHeight() - 2
                                         : display.getLineHeight();
    uint8_t bars = the_mesh.repeaterSignalBars();
    if (!bars) {
      // A compact diagonal cross means no fresh relayed/repeater sample.
      for (int i = 0; i < 6; i++) {
        display.fillRect(x + 1 + i, i, 1, 1);
        display.fillRect(x + 6 - i, i, 1, 1);
      }
    } else {
      for (uint8_t i = 0; i < 3; i++) {
        int bar_h = 2 + i * 2;
        if (i < bars) display.fillRect(x + i * 3, h - bar_h, 2, bar_h);
        else display.drawRect(x + i * 3, h - bar_h, 2, bar_h);
      }
    }
    return x;
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, ZenPrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs),
       _sensor_page(task), _page(0) {  }

  void onShow() override {
    noteHomeInteraction();
    pruneStaleFavouriteSlots();
  }

  void resetSession() {
    _page = CLOCK;
    _sensor_page.close();
    _msg_menu.active = false;
    _fav_menu.active = false;
    _pin_menu.active = false;
    _pin_target_slot = -1;
  }

  // Shared top bar keeps status visibility, priority and battery formatting
  // consistent across the home carousel.
  void renderTopBar(ZenDisplayDriver& display) {
    display.setColor(ZenDisplayDriver::LIGHT);
    int signal_left = renderRepeaterSignal(display);
    int right_edge = renderBatteryIndicator(display, _task->getBattMilliVolts(),
                                             signal_left - 2);
    display.setColor(ZenDisplayDriver::LIGHT);
    display.drawTextEllipsized(0, 0, right_edge - 2,
                              _task->isLowPowerMode() ? "LOW POWER" : _node_prefs->node_name);
  }

  // Small 5x5 glyph shown in the page-indicator row for each HomePage.
  static const MiniIcon* pageIcon(int page) {
    switch (page) {
      case SENSORS:    return &ICON_PG_SENSORS;
      case EMERGENCY:  return &ICON_PG_POWER;
      case CLOCK:      return &ICON_PG_CLOCK;
      case FAVOURITES: return &ICON_PG_STAR;
      case RADIO:      return &ICON_PG_RADIO;
      case BLUETOOTH:  return &ICON_PG_BT;
      case ADVERT:     return &ICON_PG_ADVERT;
#if ENV_INCLUDE_GPS == 1
      case GPS:        return &ICON_PG_GPS;
#endif
      case SETTINGS:   return &ICON_PG_SETTINGS;
      case TOOLS:      return &ICON_PG_TOOLS;
      case QUICK_MSG:  return &ICON_PG_MSG;
    }
    return nullptr;
  }

  int render(ZenDisplayDriver& display) override {
    returnToClockIfIdle();
    if (_page == HomePage::SENSORS && _sensor_page.passwordEditing())
      return _sensor_page.renderPassword(display);
    char tmp[80];
    display.setTextSize(1);
    const int lh      = display.getLineHeight();  // line height at sz1
    const int step    = display.lineStep();        // lh + 2
    // Page-indicator row: small (5px) page icons replace the old dots. Centre and
    // gap scale with the font so the band clears the header above and content
    // below (identical to the old lh+4 / +6 dots layout at 1x).
    const int pg_half   = (5 * miniIconScale(display) + 1) / 2;
    const int dots_y    = lh + pg_half + 1;       // icon-row centre, below the header
    const int content_y = dots_y + pg_half + 3;   // first content row, below the icons

    // Shared carousel top bar: node name, status indicators and battery.
    renderTopBar(display);

    // ensure current page is visible (e.g. after settings change)
    if (!isPageVisible(_page)) _page = navPage(_page, +1);

    // Current page indicator — a row of small page icons, one per visible page,
    // with the current page underlined.
    {
      int order[(int)Count]; int n = buildVisibleOrder(order);
      int curr_vis = 0;
      for (int i = 0; i < n; i++) if (order[i] == _page) { curr_vis = i; break; }
      const int s        = miniIconScale(display);
      const int icon_w   = 5 * s;
      int pitch = icon_w + 5 * s;                       // comfortable spacing
      if (n > 1) {                                      // shrink to fit if many pages
        int fit = (display.width() - icon_w) / (n - 1);
        if (fit < pitch) pitch = fit;
      }
      int x = display.width() / 2 - pitch * (n - 1) / 2;
      for (int i = 0; i < n; i++) {
        const MiniIcon* ic = pageIcon(order[i]);
        if (ic) miniIconDrawCentered(display, x, dots_y, *ic);
        if (i == curr_vis)                              // underline the current page
          display.fillRect(x - icon_w / 2, dots_y + pg_half + 1, icon_w, s);
        x += pitch;
      }
    }

    if (_page == HomePage::CLOCK) {
      uint32_t unix_ts = _rtc->getCurrentTime();
      int date_y = 0;
      bool show_message_count = true;
      if (_task->isTimeSyncPending()) {
        display.setColor(ZenDisplayDriver::LIGHT);
        bool h12 = _node_prefs && _node_prefs->clock_12h;
        date_y = drawClockSync(display, content_y, h12);
      } else if (unix_ts < 1000000000UL) {
        show_message_count = false;
        display.setColor(ZenDisplayDriver::LIGHT);
        display.setTextSize(1);
        int mid_y = content_y;
        display.drawTextCentered(display.width() / 2, mid_y, "! No time sync");
        display.drawTextCentered(display.width() / 2, mid_y + step, "Enable GPS or");
        display.drawTextCentered(display.width() / 2, mid_y + step * 2, "connect app");
      } else {
        int16_t tz = _task->localOffsetMinutes(unix_ts);
        unix_ts += (int32_t)tz * 60;
        time_t t = (time_t)unix_ts;
        struct tm* ti = gmtime(&t);

        char buf[24];
        display.setColor(ZenDisplayDriver::LIGHT);
        bool show_sec = !Features::IS_EINK && (!_node_prefs || !_node_prefs->clock_hide_seconds);
        bool h12 = _node_prefs && _node_prefs->clock_12h;
        date_y = drawClockTime(display, content_y, ti, h12, show_sec);

        display.setTextSize(1);
        static const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char* mo[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        snprintf(buf, sizeof(buf),"%s %d %s %d", wd[ti->tm_wday], ti->tm_mday, mo[ti->tm_mon], 1900 + ti->tm_year);
        display.drawTextCentered(display.width() / 2, date_y, buf);

      }

      if (show_message_count) {
        int sep_y  = date_y + lh + 1;
        int dash0  = sep_y + display.sepH() + 2;
        display.fillRect(0, sep_y, display.width(), display.sepH());

        display.setCursor(0, dash0);
        display.print("Messages");
        char unread_text[8];
        int unread = _task->getDMUnreadTotal() + _task->getChannelUnreadCount() +
                     _task->getRoomUnreadCount();
        snprintf(unread_text, sizeof(unread_text), "%d", unread);
        display.setCursor(display.width() - display.getTextWidth(unread_text) - 1, dash0);
        display.print(unread_text);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(ZenDisplayDriver::LIGHT);
      // freq / sf
      display.setCursor(0, content_y);
      snprintf(tmp, sizeof(tmp),"FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, content_y + step);
      snprintf(tmp, sizeof(tmp),"BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power, noise floor
      display.setCursor(0, content_y + step * 2);
      snprintf(tmp, sizeof(tmp),"TX: %ddBm", (int)_node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, content_y + step * 3);
      snprintf(tmp, sizeof(tmp),"Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.setTextSize(1);
      display.drawXbm((display.width() - 32) / 2, content_y,
          _task->isBluetoothEnabled() ? bluetooth_on : bluetooth_off, 32, 32);
      const int text_y = content_y + 32 + 3;
      // The pairing PIN is BLE-specific: show it while BLE is on but not yet
      // bonded. (Gating on a plain isConnected() broke this on dual builds,
      // where it's hardcoded true.)
      const bool waiting_for_pair = _task->isBluetoothEnabled() && !_task->isBLEConnected() && the_mesh.getBLEPin() != 0;
      if (waiting_for_pair && !display.isLandscape()) {
        char pin_buf[16];
        snprintf(pin_buf, sizeof(pin_buf), "PIN: %d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, text_y, pin_buf);
      } else if (waiting_for_pair) {
        char pin_buf[16];
        snprintf(pin_buf, sizeof(pin_buf), "PIN: %d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, text_y, pin_buf);
        display.drawTextCentered(display.width() / 2, text_y + step, TOGGLE_HINT);
      } else {
        display.drawTextCentered(display.width() / 2, text_y, TOGGLE_HINT);
      }
    } else if (_page == HomePage::ADVERT) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.drawXbm((display.width() - 32) / 2, content_y, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, content_y + 32 + 3, ADVERT_HINT);
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = content_y;
      bool gps_state = _task->getGPSState();
      uint8_t polling = zen::GpsMode::pollingFromPrefs(
          _node_prefs ? _node_prefs->gps_interval : 0,
          _node_prefs && _node_prefs->gps_adaptive);
      snprintf(buf, sizeof(buf), "%s", zen::GpsMode::pollingLabel(polling));
      display.drawTextLeftAlign(0, y, buf);
      const char* receiver_state = "Off";
      char receiver_buf[16];
      if (nmea != NULL && nmea->isEnabled())
        receiver_state = nmea->isValid() ? "Fix" : "Search";
      else if (gps_state) {
        uint32_t retry_ms;
        if (_task->getGpsAdaptiveRetry(retry_ms)) {
          uint32_t minutes = (retry_ms + 59999UL) / 60000UL;
          if (minutes < 1) minutes = 1;
          snprintf(receiver_buf, sizeof(receiver_buf), "Retry %lum", (unsigned long)minutes);
          receiver_state = receiver_buf;
        } else {
        uint32_t age;
        if (_task->getGpsFixAgeMs(age)) {
          uint32_t minutes = age / 60000UL;
          if (minutes < 1) strcpy(receiver_buf, "Sleep <1m");
          else if (minutes < 120) snprintf(receiver_buf, sizeof(receiver_buf), "Sleep %lum", (unsigned long)minutes);
          else snprintf(receiver_buf, sizeof(receiver_buf), "Sleep %luh", (unsigned long)(minutes / 60));
          receiver_state = receiver_buf;
        } else receiver_state = "Sleep";
        }
      }
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state && !hw_gps_state && (nmea == NULL || !nmea->isEnabled()))
        receiver_state = "HW Off";
#endif
      display.drawTextRightAlign(display.width()-1, y, receiver_state);
      if (nmea == NULL) {
        y += step;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        y += step;
        const char* pos_label = "Pos";
        // Keep the label on the common zero-pixel left edge and anchor the
        // complete coordinate pair to the common right edge. Reduce precision
        // only until the two regions have one character of separation.
        int pos_width = display.width() - 1 - display.getTextWidth(pos_label) - display.getCharWidth();
        double lat = nmea->getLatitude() / 1000000.0;
        double lon = nmea->getLongitude() / 1000000.0;
        int precision = 4;
        do {
          snprintf(buf, sizeof(buf), "%.*f %.*f", precision, lat, precision, lon);
        } while (precision > 1 && display.getTextWidth(buf) > pos_width && --precision);
        display.drawTextLeftAlign(0, y, pos_label);
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += step;
        display.drawTextLeftAlign(0, y, "Alt");
        snprintf(buf, sizeof(buf), "%.1f", nmea->getAltitude() / 1000.);
        display.drawTextLeftAlign(display.getCharWidth() * 4, y, buf);
        snprintf(buf, sizeof(buf), "Sat %ld", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y += step;
        long course = LONG_MIN;
        zen::GpsCourse::Source source = _task->getGpsCourse(course);
        drawGpsCourseTape(display, y, source != zen::GpsCourse::NONE, course, source);
      }
#endif
    } else if (_page == HomePage::SETTINGS) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.setTextSize(1);
      if (_task->isChildModeLocked()) {
        display.drawSelectionRow(0, content_y - 1, display.width(), step - 1, true);
        display.drawTextEllipsized(2, content_y, display.width() - 4, "Parent unlock");
      } else {
        int scroll = _settings_scroll;
        renderHomeList(display, content_y, _task->getSettingsSectionCount(),
                       _settings_sel, scroll,
                       [&](int i) { return _task->getSettingsSectionLabel(i); });
        _settings_scroll = (uint8_t)scroll;
      }
    } else if (_page == HomePage::SENSORS) {
      _sensor_page.render(display, content_y);
    } else if (_page == HomePage::EMERGENCY) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.setTextSize(1);
      // Five rows share the small OLED content area. Use the glyph height
      // without the normal two-pixel list gap so the Bluetooth row is visible.
      const int emergency_step = lh;
      int status_y = content_y + emergency_step * 2;
      if (_task->isEmergencyMode()) {
        uint32_t remaining = _task->emergencyRemainingSeconds();
        char buf[32];
        display.drawTextCentered(display.width() / 2, content_y, "Emergency Mode");
        snprintf(buf, sizeof(buf), "%lum %02lus remaining",
                 (unsigned long)(remaining / 60), (unsigned long)(remaining % 60));
        display.drawTextCentered(display.width() / 2, content_y + emergency_step, buf);
      } else {
        display.drawTextCentered(display.width() / 2, content_y, "Low Power Emergency");
        display.drawTextCentered(display.width() / 2, content_y + emergency_step, "Select to Enable");
      }
      display.drawTextCentered(display.width() / 2, status_y,
                               the_mesh.radioAvailable() ? "Radio On" : "Radio Off");
      status_y += emergency_step;
#if ENV_INCLUDE_GPS == 1
      display.drawTextCentered(display.width() / 2, status_y,
                               _task->getGPSState() ? "GPS On" : "GPS Off");
      status_y += emergency_step;
#endif
      display.drawTextCentered(display.width() / 2, status_y,
                               _task->isBluetoothEnabled() ? "Bluetooth On" : "Bluetooth Off");
    } else if (_page == HomePage::TOOLS) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.setTextSize(1);
      int scroll = _tools_scroll;
      renderHomeList(display, content_y, _task->getToolsItemCount(),
                     _tools_sel, scroll,
                     [&](int i) { return _task->getToolsItemLabel(i); });
      _tools_scroll = (uint8_t)scroll;
    } else if (_page == HomePage::QUICK_MSG) {
      display.setColor(ZenDisplayDriver::LIGHT);
      display.setTextSize(1);
      const char* labels[] = { "Direct Message", "Channel", "Room Servers" };
      int badges[] = {
        _task->getDMUnreadTotal(),
        _task->getChannelUnreadCount(),
        _task->getRoomUnreadCount()
      };
      int count = messageModeCount();
      for (int pos = 0; pos < count; pos++) {
        int mode = messageModeAt(pos);
        int y = content_y + pos * step;
        bool selected = pos == messageModePosition();
        display.drawSelectionRow(0, y - 1, display.width(), step - 1, selected);
        display.setCursor(2, y);
        display.print(labels[mode]);
        if (badges[mode] > 0)
          display.drawUnreadBadge(display.width() - 1, y, badges[mode], selected);
      }
      if (_msg_menu.active) _msg_menu.render(display);
    } else if (_page == HomePage::FAVOURITES) {
      // Four full-width pinned-contact rows. The compact row height deliberately
      // uses the whole area below the page indicators so all entries fit on OLED.
      // No title — node name + battery (top bar) and the page-dots indicator above
      // serve as the page identity.
      display.setColor(ZenDisplayDriver::LIGHT);
      display.setTextSize(1);

      const int grid_y  = content_y;
      const int grid_h  = display.height() - grid_y;
      const int cell_w  = display.width();
      const int cell_h  = grid_h / ZenPrefs::FAVOURITES_DIAL_COUNT;
      const int line_h  = display.getLineHeight();

      if (_fav_sel >= ZenPrefs::FAVOURITES_DIAL_COUNT) _fav_sel = 0;

      for (uint8_t i = 0; i < ZenPrefs::FAVOURITES_DIAL_COUNT; i++) {
        int cx  = 0;
        int cy  = grid_y + i * cell_h;
        bool sel = (i == _fav_sel);
        display.drawSelectionRow(cx, cy, cell_w - 1, cell_h - 1, sel);

        // Empty slot → all-zero prefix. Real keys collide with this with probability 2^-48.
        const uint8_t* prefix = _node_prefs ? _node_prefs->favourite_contacts[i] : nullptr;
        bool filled = false;
        if (prefix) {
          for (uint8_t b = 0; b < ZenPrefs::FAVOURITE_PREFIX_LEN; b++)
            if (prefix[b] != 0) { filled = true; break; }
        }

        ContactInfo ci;
        bool has_contact = false;
        if (filled) {
          for (int idx = 0; ; idx++) {
            ContactInfo c;
            if (!the_mesh.getContactByIdx(idx, c)) break;
            if (memcmp(c.id.pub_key, prefix, ZenPrefs::FAVOURITE_PREFIX_LEN) == 0) {
              ci = c; has_contact = true; break;
            }
          }
        }

        bool allowed_contact = has_contact &&
            zen::Policy::contactAllowed(_node_prefs, _task->isChildModeLocked(),
                                         &ci, ADV_TYPE_CHAT);
        if (allowed_contact) {
          // Reserve space for the unread badge so the name's ellipsis lands
          // before it instead of underneath. Badge and name share one baseline.
          uint8_t unread = _task->getDMUnread(ci.id.pub_key);
          int  bw = unread > 0 ? display.unreadBadgeWidth(unread) + 3 : 0;  // badge + 3 px gap
          int name_y     = cy + (cell_h - line_h) / 2;
          int name_max_w = cell_w - 4 - bw;
          if (name_max_w < 6) name_max_w = 6;
          display.drawTextEllipsized(cx + 2, name_y, name_max_w, ci.name,
                                     sel && !_fav_menu.active && !_pin_menu.active);
          if (unread > 0)
            display.drawUnreadBadge(cx + cell_w - 2, name_y, unread, sel);
        } else {
          int plus_y = cy + (cell_h - line_h) / 2;
          display.drawTextCentered(cx + cell_w / 2, plus_y, "+");
        }
        if (sel) display.setColor(ZenDisplayDriver::LIGHT);
      }
      if (_pin_menu.active) _pin_menu.render(display);
      if (_fav_menu.active) _fav_menu.render(display);
    }
    if (_emergency_menu.active) _emergency_menu.render(display);
    bool auto_adv = _node_prefs && _node_prefs->advert_auto_interval_sec > 0;
    // Any blinking status-bar indicator needs a 1 s refresh to animate evenly.
    bool repeating  = _node_prefs && _node_prefs->isRepeatEn();
    bool need_blink = auto_adv || repeating;
    int refresh_ms;
    if (Features::IS_EINK) {
      // slow display: poll every 30 s; inbound msgs force immediate refresh via notify()
      refresh_ms = Features::HOME_REFRESH_MS;
    } else if (_page == HomePage::CLOCK) {
      bool show_sec = !_node_prefs || !_node_prefs->clock_hide_seconds;
      refresh_ms = need_blink ? 1000 : (show_sec ? 1000 : 60000);
    } else if (_page == HomePage::EMERGENCY && _task->isEmergencyMode()) {
      refresh_ms = 1000;
    } else {
      refresh_ms = need_blink ? 1000 : 5000;
    }
    // Reuse the next scheduled home render as the five-minute deadline. This
    // adds no polling or wake loop and avoids up to 30 s of overshoot on e-ink.
    if (_page != HomePage::CLOCK && _home_idle_deadline != 0) {
      int32_t remaining = (int32_t)(_home_idle_deadline - millis());
      if (remaining > 0 && remaining < refresh_ms) refresh_ms = remaining;
    }
    return refresh_ms;
  }

  void onSensorLoginResult(const uint8_t* key, bool success) {
    _sensor_page.onLoginResult(key, success);
  }
  void onSensorLoginTimeout(const uint8_t* key) {
    _sensor_page.onLoginTimeout(key);
  }
  void tickSensor() { _sensor_page.tick(); }

  bool handleInput(char c) override {
    noteHomeInteraction();
    if (_emergency_menu.active) {
      auto result = _emergency_menu.handleInput(c);
      if (result == PopupMenu::SELECTED && _emergency_menu.selectedIndex() == 1) {
        if (_emergency_menu_disables) _task->endEmergencyMode();
        else _task->beginEmergencyMode();
      }
      return true;
    }
    if (_page == HomePage::SENSORS) {
      if (_task->isChildModeLocked()) { _sensor_page.close(); _page = CLOCK; return true; }
      if (_sensor_page.handleInput(c)) return true;
      if (c == KEY_LEFT || c == KEY_RIGHT || c == KEY_PREV || c == KEY_NEXT || c == KEY_CANCEL)
        _sensor_page.close();
    }
    if (_page == HomePage::QUICK_MSG && _msg_menu.active) {
      auto result = _msg_menu.handleInput(c);
      if (result == PopupMenu::SELECTED) {
        int count = _task->markMessageCategoryRead(_msg_mode_sel);
        char alert[32];
        snprintf(alert, sizeof(alert), "%d marked read", count);
        _task->showAlert(alert, 800);
      }
      return true;
    }

    // Favourites is a single vertical list; UP/DOWN select its four rows while
    // LEFT/RIGHT remain dedicated to carousel page navigation.
    if (_page == HomePage::FAVOURITES) {
      // Hold Enter exposes the same compact action-menu convention used by the
      // other lists. Editing remains unavailable while Child Mode is locked.
      if (_fav_menu.active) {
        auto res = _fav_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          bool filled = !_task->isFavouriteSlotEmpty(_fav_sel);
          int action = _fav_menu.selectedIndex();
          if (!filled || action == 0) {
            buildPinPicker(_fav_sel); // Add, or Change on a populated slot.
          } else {
            uint8_t previous[ZenPrefs::FAVOURITE_PREFIX_LEN];
            memcpy(previous, _node_prefs->favourite_contacts[_fav_sel], sizeof(previous));
            _task->clearFavouriteSlot(_fav_sel);
            if (!the_mesh.saveZenPrefs()) {
              memcpy(_node_prefs->favourite_contacts[_fav_sel], previous, sizeof(previous));
              return true;
            }
            char alert[24];
            snprintf(alert, sizeof(alert), "Unpinned (slot %d)", _fav_sel + 1);
            _task->showAlert(alert, 800);
          }
        }
        return true;
      }
      // Pin picker consumes all input while open.
      if (_pin_menu.active) {
        auto res = _pin_menu.handleInput(c);
        if (res == PopupMenu::SELECTED && _pin_target_slot >= 0) {
          int idx = _pin_menu.selectedIndex();
          if (idx >= 0 && idx < _pin_count) {
            // If this contact is already pinned elsewhere, vacate that slot first.
            int existing = _task->findFavouriteSlot(_pin_keys[idx]);
            ZenPrefs* p = _task->getNodePrefs();
            uint8_t previous[sizeof(p->favourite_contacts)];
            if (p) memcpy(previous, p->favourite_contacts, sizeof(previous));
            bool changed = !p || memcmp(p->favourite_contacts[_pin_target_slot], _pin_keys[idx],
                                        ZenPrefs::FAVOURITE_PREFIX_LEN) != 0;
            if (existing >= 0 && existing != _pin_target_slot) {
              _task->clearFavouriteSlot(existing);
              changed = true;
            }
            if (changed) {
              changed = _task->setFavouriteSlot(_pin_target_slot, _pin_keys[idx]);
              if (changed && !the_mesh.saveZenPrefs()) {
                memcpy(p->favourite_contacts, previous, sizeof(previous));
                return true;
              }
            }
            if (changed) {
              char alert[24];
              snprintf(alert, sizeof(alert), "Pinned to slot %d", _pin_target_slot + 1);
              _task->showAlert(alert, 800);
            }
          }
        }
        if (res != PopupMenu::NONE) _pin_target_slot = -1;
        return true;
      }
      if (c == KEY_UP) {
        _fav_sel = _fav_sel > 0 ? _fav_sel - 1 : ZenPrefs::FAVOURITES_DIAL_COUNT - 1;
        return true;
      }
      if (c == KEY_DOWN) {
        _fav_sel = _fav_sel + 1 < ZenPrefs::FAVOURITES_DIAL_COUNT ? _fav_sel + 1 : 0;
        return true;
      }
      if (c == KEY_CONTEXT_MENU) {
        if (_task->isChildModeLocked()) {
          _task->operationWarning(zen::Operation::CHILD_MODE,
                                  zen::OperationReason::PARENT_ONLY);
          return true;
        }
        bool filled = !_task->isFavouriteSlotEmpty(_fav_sel);
        _fav_menu.begin("Favourite", filled ? 2 : 1);
        _fav_menu.addItem(filled ? "Change" : "Add");
        if (filled) _fav_menu.addItem("Remove");
        return true;
      }
      if (c == KEY_ENTER) {
        // Filled slot → open the DM directly. Empty slot waits for phase 3
        // (mini-picker); for now show the pin hint.
        ZenPrefs* p = _task->getNodePrefs();
        const uint8_t* pfx = (p && _fav_sel < ZenPrefs::FAVOURITES_DIAL_COUNT)
                             ? p->favourite_contacts[_fav_sel] : nullptr;
        bool filled = false;
        if (pfx) for (uint8_t b = 0; b < ZenPrefs::FAVOURITE_PREFIX_LEN; b++)
          if (pfx[b]) { filled = true; break; }
        if (filled) {
          for (int idx = 0; ; idx++) {
            ContactInfo c2;
            if (!the_mesh.getContactByIdx(idx, c2)) break;
            if (memcmp(c2.id.pub_key, pfx, ZenPrefs::FAVOURITE_PREFIX_LEN) == 0) {
              if (c2.type != ADV_TYPE_CHAT) {
                uint8_t previous[ZenPrefs::FAVOURITE_PREFIX_LEN];
                memcpy(previous, _node_prefs->favourite_contacts[_fav_sel], sizeof(previous));
                _task->clearFavouriteSlot(_fav_sel);
                if (!the_mesh.saveZenPrefs()) {
                  memcpy(_node_prefs->favourite_contacts[_fav_sel], previous, sizeof(previous));
                  return true;
                }
                _task->operationWarning(zen::Operation::FAVOURITES,
                    zen::OperationReason::INVALID_ENTRY_REMOVED);
                return true;
              }
              _task->openContactDM(c2);
              return true;
            }
          }
          _task->operationError(zen::Operation::FAVOURITES,
              zen::OperationOutcome::NOT_FOUND,
              zen::OperationReason::CONTACT_NOT_FOUND);
        } else {
          // Empty slot → open in-place pin picker.
          if (_task->isChildModeLocked())
            _task->operationWarning(zen::Operation::CHILD_MODE,
                                    zen::OperationReason::PARENT_ONLY);
          else buildPinPicker(_fav_sel);
        }
        return true;
      }
      // Edge LEFT/RIGHT and unhandled keys fall through to page nav below.
    }

    if (_page == HomePage::QUICK_MSG) {
      int count = messageModeCount();
      int pos = messageModePosition();
      if (c == KEY_UP || c == KEY_DOWN) {
        pos = c == KEY_UP ? (pos > 0 ? pos - 1 : count - 1)
                          : (pos < count - 1 ? pos + 1 : 0);
        _msg_mode_sel = (uint8_t)messageModeAt(pos);
        return true;
      }
      if (c == KEY_ENTER) {
        _task->gotoMessagesCategory((uint8_t)messageModeAt(messageModePosition()));
        return true;
      }
      if (c == KEY_CONTEXT_MENU) {
        if (_task->isChildModeLocked()) return true;
        _msg_mode_sel = (uint8_t)messageModeAt(messageModePosition());
        static const char* TITLES[] = { "DM options", "Channel options", "Room options" };
        _msg_menu.begin(TITLES[_msg_mode_sel], 1);
        _msg_menu.addItem("Mark all read");
        return true;
      }
    }

    if (_page == HomePage::SETTINGS) {
      if (_task->isChildModeLocked()) {
        if (c == KEY_ENTER) _task->gotoChildUnlockScreen();
        if (c == KEY_UP || c == KEY_DOWN || c == KEY_ENTER) return true;
      } else {
        int count = _task->getSettingsSectionCount();
        if (c == KEY_UP && count > 0) {
          _settings_sel = _settings_sel > 0 ? _settings_sel - 1 : count - 1;
          return true;
        }
        if (c == KEY_DOWN && count > 0) {
          _settings_sel = _settings_sel + 1 < count ? _settings_sel + 1 : 0;
          return true;
        }
        if (c == KEY_ENTER && count > 0) {
          _task->openSettingsSection(_settings_sel);
          return true;
        }
      }
    }

    if (_page == HomePage::TOOLS) {
      int count = _task->getToolsItemCount();
      if (c == KEY_UP && count > 0) {
        _tools_sel = _tools_sel > 0 ? _tools_sel - 1 : count - 1;
        return true;
      }
      if (c == KEY_DOWN && count > 0) {
        _tools_sel = _tools_sel + 1 < count ? _tools_sel + 1 : 0;
        return true;
      }
      if (c == KEY_ENTER && count > 0) {
        _task->openToolsItem(_tools_sel);
        return true;
      }
    }

    if (_page == HomePage::EMERGENCY && c == KEY_ENTER) {
      if (_task->isEmergencyMode()) {
        _emergency_menu_disables = true;
        _emergency_menu.begin("Disable Emergency?", 2);
        _emergency_menu.addItem("No");
        _emergency_menu.addItem("Yes");
      } else {
        _emergency_menu_disables = false;
        _emergency_menu.begin("Enable for 10 min?", 2);
        _emergency_menu.addItem("No");
        _emergency_menu.addItem("Yes");
      }
      return true;
    }

    // Back on Clock blanks the display. The wake path consumes the Back press
    // that turns it on, so one press can never immediately wake and re-sleep it.
    if (c == KEY_CANCEL && _page == HomePage::CLOCK) {
      _task->sleepDisplay();
      return true;
    }

    // Treat Clock as the carousel's home page: Back/Escape from any other
    // home card jumps straight there. Active popups consume Cancel above first,
    // so closing a menu never unexpectedly changes pages.
    if (c == KEY_CANCEL && _page != HomePage::CLOCK && isPageVisible(HomePage::CLOCK)) {
      _page = HomePage::CLOCK;
      return true;
    }

    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = navPage(_page, -1);
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = navPage(_page, +1);
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::RADIO) {
      _task->gotoRadioSettings();
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::BLUETOOTH) {
      _task->gotoBluetoothSettings();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isChildModeLocked()) {
        _task->operationWarning(zen::Operation::CHILD_MODE,
            zen::OperationReason::BLUETOOTH_UNAVAILABLE);
        return true;
      }
      bool was_enabled = _task->isBluetoothEnabled();
      if (_task->isLowPowerMode() && !_task->isEmergencyMode()) {
        _task->operationWarning(zen::Operation::BLUETOOTH,
            zen::OperationReason::EMERGENCY_REQUIRED);
      } else if (_task->isBluetoothEnabled()) {
        _task->disableBluetooth();
        if (_task->isBluetoothEnabled())
          _task->operationError(zen::Operation::BLUETOOTH,
              zen::OperationOutcome::TRANSPORT_FAILURE,
              zen::OperationReason::APPLY_FAILED);
      } else {
        _task->enableBluetooth();
        if (!_task->isBluetoothEnabled())
          _task->operationError(zen::Operation::BLUETOOTH,
              zen::OperationOutcome::TRANSPORT_FAILURE,
              zen::OperationReason::APPLY_FAILED);
      }
      if (_task->isBluetoothEnabled() == was_enabled)
        return true;
      _task->notifyHomeAction();
      _task->showAlert(_task->isBluetoothEnabled() ? "Bluetooth: On" : "Bluetooth: Off", 900);
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::ADVERT) {
      _task->gotoAutoAdvertScreen();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      if (the_mesh.advert()) {
        _task->notifyHomeAction();
        _task->showAlert("Advert sent", 1000);
      } else {
        _task->operationError(zen::Operation::ADVERT,
            zen::OperationOutcome::TRANSPORT_FAILURE,
            zen::OperationReason::SEND_FAILED);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_CONTEXT_MENU && _page == HomePage::GPS) {
      _task->gotoGpsPollingSettings();
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::CLOCK) {
      _task->openPreferredTranscript();
      return true;
    }
    return false;
  }
};


void UITask::begin(ZenDisplayDriver* display, SensorManager* sensors,
                   zen::GpsService* gps, ZenPrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _gps = gps;
  _node_prefs = node_prefs;
  _local_time.bind(node_prefs);
  _zen_runtime.begin(_node_prefs);
  _power.requests().display_on = _display && _display->isOn();
  _kb.prefs = node_prefs;
  uint32_t aoff = autoOffMillis();
  _auto_off = millis() + (aoff > 0 ? aoff : AUTO_OFF_MILLIS);

#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  // Wire1 is already brought up by sensors.begin() (EnvironmentSensorManager).
  // The controller performs one boot probe and does not retry an absent accessory.
  _cardkb.begin(Wire1, CARDKB_ADDRESS);
  _cardkb_was_present = _cardkb.isPresent();
#endif
  _kb.setExternalKeyboardConnected(isCardKBConnected());
  // Zen display builds activate GPS through the coordinator. main.cpp retains
  // the legacy bootstrap only for headless/non-Zen companions.
  applyGpsPrefs();
  beginBootTimeSync();

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if UI_HAS_JOYSTICK
  // The directional joystick + Back share the same ZenMomentaryButton machinery as
  // user_btn but were never begin()'d — they only worked because the pins
  // default to INPUT and the board has external pulls. That left them on the
  // polling path: with BUTTON_USE_INTERRUPTS (e-ink) they'd silently never
  // attach a GPIOTE channel, so edges landing during a blocking panel refresh
  // were lost. begin() sets pinMode and claims an IRQ slot for each.
  joystick_left.begin();
  joystick_right.begin();
  back_btn.begin();
#if UI_HAS_JOYSTICK_UPDOWN
  joystick_up.begin();
  joystick_down.begin();
#endif
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  if (_display != NULL) {
    turnDisplayOn();
  }

#ifdef PIN_BUZZER
  buzzer.setVolume(_node_prefs->buzzer_volume);
  buzzer.begin();
  // Restore the brief startup acknowledgement without bypassing the saved
  // Notification Off setting. Auto is audible here because no client is yet
  // connected during UI startup.
  buzzer.quiet(getNotificationMode() == zen::NotificationPolicy::OFF);
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;
  _batt_mv = AbstractUITask::getBattMilliVolts();  // seed EMA with first reading

  // Initialize ping state
  _ping_active = false;
  _ping_tag = 0;
  _ping_sent_ms = 0;
  _ping_snr_out_x4 = 0;
  _ping_snr_back_x4 = 0;
  _ping_rtt_ms = 0;

  // Screens live for the firmware lifetime. Static ownership avoids heap
  // fragmentation and makes allocation failure impossible on the nRF52840.
  static SplashScreen splash_instance(this);
  static HomeScreen home_instance(this, &rtc_clock, sensors, node_prefs);
  static SettingsScreen settings_instance(this, &_kb);
  static MessagesScreen messages_instance(this, &_kb);
  static ChildUnlockScreen child_unlock_instance(this);
  static ToolsScreen tools_instance(this);
  static RingtoneEditorScreen ringtone_instance(this, node_prefs);
  splash = &splash_instance;
  home = &home_instance;
  settings = &settings_instance;
  messages_screen = &messages_instance;
  child_unlock = &child_unlock_instance;
  tools_screen = &tools_instance;
  ringtone_edit = &ringtone_instance;
#if ZEN_FEATURE_ADMIN
  static AdminScreen admin_instance(this);
  admin_screen = &admin_instance;
#endif
  static NearbyScreen nearby_instance(this);
  static AutoAdvertScreen advert_instance(this, node_prefs);
  nearby_screen = &nearby_instance;
  auto_advert_screen = &advert_instance;
  static DiagnosticsScreen diagnostics_instance(this);
  diag_screen = &diagnostics_instance;
#if ZEN_FEATURE_REPEATER
  static RepeaterScreen repeater_instance(this);
  repeater_screen = &repeater_instance;
#endif
  // Child Mode can be restored from the Zen preferences overlay on a cold
  // boot. Apply it only after every screen it may reset has been constructed.
  // The previous order dereferenced messages_screen while it was still null.
  applyChildMode();
  applyBrightness();
  applyRotation();
  applyFullRefreshInterval();
  setCurrScreen(splash);
}

void UITask::beginBootTimeSync() {
  LocationProvider* loc = _sensors ? _sensors->getLocationProvider() : nullptr;
  bool configured_on = _node_prefs && _node_prefs->gps_enabled;
  zen::TimeLocationCoordinator::Actions actions = _time_location.begin(
      rtc_clock.getSetGeneration(), loc != nullptr, configured_on, millis());
  if (!loc) return;

  loc->syncTime();
  _power.requests().gps_time_sync = actions.request_gps_time;
  reconcilePower();
}

void UITask::onTimeSynchronized(zen::TimeSyncSource source) {
  _time_location.noteSynchronized(source, rtc_clock.getSetGeneration(), millis());
  _next_refresh = 0;
}

void UITask::tickBootTimeSync() {
  LocationProvider* loc = _sensors ? _sensors->getLocationProvider() : nullptr;
  bool configured_on = _node_prefs && _node_prefs->gps_enabled;
  bool low_power = isLowPowerMode();
  // A suspended temporary claim remains owned while Low Power is active. Feed
  // that logical state to BootTimeSync so it can still observe an external RTC
  // generation change without mistaking the restriction for a manual GPS-off.
  bool enabled = low_power && _power.requests().gps_time_sync
      ? true : loc && loc->isEnabled();
  zen::TimeLocationCoordinator::Actions actions = _time_location.tick(
      rtc_clock.getSetGeneration(), configured_on, enabled, !low_power,
      _node_prefs ? _node_prefs->gps_interval : 0, _gps,
      loc ? loc->waitingTimeSync() : true, millis());
  if (actions.request_gps_time && loc && _sensors) {
    loc->syncTime();
    _power.requests().gps_time_sync = true;
    reconcilePower();
  } else if (actions.release_gps_time) {
    _power.requests().gps_time_sync = false;
    reconcilePower();
  }
  if (actions.refresh_ui) _next_refresh = 0;
  if (actions.retry_window_ended)
    operationWarning(zen::Operation::TIME_SYNC,
        zen::OperationReason::GPS_RETRIES_ENDED);
}

// onShow() is invoked by setCurrScreen(), so most navigators are just that.
void UITask::gotoSettingsScreen() {
  ((SettingsScreen*)settings)->openSection(0);
  setCurrScreen(settings);
}
int UITask::getSettingsSectionCount() const {
  return ((SettingsScreen*)settings)->sectionCount() + 1;
}
const char* UITask::getSettingsSectionLabel(int index) const {
  int section_count = ((SettingsScreen*)settings)->sectionCount();
  if (index == section_count) return "Advert";
  return ((SettingsScreen*)settings)->sectionLabel(index);
}
void UITask::openSettingsSection(int index) {
  if (index == ((SettingsScreen*)settings)->sectionCount()) {
    gotoAutoAdvertScreen();
    return;
  }
  ((SettingsScreen*)settings)->openSection(index);
  setCurrScreen(settings);
}
void UITask::gotoRadioSettings() {
  ((SettingsScreen*)settings)->openRadioSettings();
  setCurrScreen(settings);
}
void UITask::gotoBluetoothSettings() {
  ((SettingsScreen*)settings)->openBluetoothSettings();
  setCurrScreen(settings);
}
#if ENV_INCLUDE_GPS == 1
void UITask::gotoGpsPollingSettings() {
  ((SettingsScreen*)settings)->openGpsPolling();
  setCurrScreen(settings);
}
#endif
void UITask::gotoChildUnlockScreen() { setCurrScreen(child_unlock); }
void UITask::setChildAdminUnlocked(bool unlocked) {
  bool was_unlocked = _zen_runtime.parentUnlocked();
  _zen_runtime.setParentUnlocked(unlocked);
  if (was_unlocked && isChildModeLocked()) {
    // No privileged screen, modal or queued input survives the parent session.
    ((MessagesScreen*)messages_screen)->reset();
    ((HomeScreen*)home)->resetSession();
    char discarded;
    while (dequeueKey(discarded)) { }
    gotoHomeScreen();
  }
  applyChildMode();
}
void UITask::applyChildMode() {
  if (!_interfaceManager) return;
  bool child_locked = isChildModeLocked();
  if (_zen_runtime.recordChildLockState(_node_prefs)) {
    // Counts accumulated before the restricted session cannot be attributed
    // safely to an allowed sender (room count is aggregate), so start the
    // child-visible notification state clean. Message history is untouched.
    _msgcount = 0;
    _message_unread.reset();
    _alert_expiry = 0;
  }
  if (child_locked) {
    if (messages_screen)
      ((MessagesScreen*)messages_screen)->cancelDmResends();
  }
  _power.restrictions().child_locked = child_locked;
  reconcilePower();
  _next_refresh = 0;
}
void UITask::gotoToolsScreen() {
  returnToScreen(tools_screen);
}
int UITask::getToolsItemCount() const { return ToolsScreen::itemCount(); }
const char* UITask::getToolsItemLabel(int index) const {
  return ToolsScreen::itemLabel(index);
}
void UITask::openToolsItem(int index) {
  _screen_history.push(home);
  ((ToolsScreen*)tools_screen)->openItem(index);
}
void UITask::gotoNearbyScreen()    { setCurrScreen(nearby_screen); }
void UITask::gotoDiscoverScreen() {
  setCurrScreen(nearby_screen);
  ((NearbyScreen*)nearby_screen)->startDiscoverScan();
}

void UITask::openAdminFor(const ContactInfo& ci) {
#if ZEN_FEATURE_ADMIN
  if (isChildModeLocked() || (ci.type != ADV_TYPE_REPEATER && ci.type != ADV_TYPE_ROOM
                              && ci.type != ADV_TYPE_SENSOR)) return;
  // Sensor telemetry and login use the same contact-response transport. End a
  // card request before the independently matched admin login begins.
  if (ci.type == ADV_TYPE_SENSOR) the_mesh.cancelSensorTelemetry();
  openScreen(admin_screen, nearby_screen); // onShow resets before startFor()
  ((AdminScreen*)admin_screen)->startFor(ci);
#else
  (void)ci;
#endif
}
void UITask::returnFromAdmin() {
  ((NearbyScreen*)nearby_screen)->resumeFromAdmin();
  returnToScreen(nearby_screen);
}
void UITask::gotoDiagnosticsScreen() { setCurrScreen(diag_screen); }
void UITask::gotoRepeaterScreen()  { if (zen::Features::REPEATER) setCurrScreen(repeater_screen); }

// Ringtone takes a slot argument that onShow() can't carry — pass it after the
// reset (setCurrScreen's onShow runs first, then this layers the slot on top).
void UITask::gotoRingtoneEditor(int slot) {
  setCurrScreen(ringtone_edit);
  ((RingtoneEditorScreen*)ringtone_edit)->selectSlot(slot);
}

void UITask::gotoAutoAdvertScreen() { setCurrScreen(auto_advert_screen); }

// Public method to handle ping result callback
void UITask::handlePingResult(uint32_t tag, int16_t snr_out_x4, int16_t snr_back_x4, uint32_t rtt_ms) {
  if (_ping_active && _ping_tag == tag) {
    _ping_snr_out_x4 = snr_out_x4;
    _ping_snr_back_x4 = snr_back_x4;
    _ping_rtt_ms = rtt_ms;
    // Release the in-flight slot immediately; the UI keeps the result values.
    clearPing();
  }
}

// Static ping callback (for MyMesh)
static void onPingResult(uint32_t tag, int16_t snr_out_x4, int16_t snr_back_x4, uint32_t rtt_ms) {
  AbstractUITask* ui = the_mesh.getUITask();
  if (ui) {
    UITask* task = static_cast<UITask*>(ui);
    task->handlePingResult(tag, snr_out_x4, snr_back_x4, rtt_ms);
  }
}

void UITask::clearPing() {
  if (_ping_tag != 0) {
    the_mesh.clearPingResult(_ping_tag);
  }
  _ping_active = false;
  _ping_tag = 0;
}

bool UITask::startPing(const uint8_t* pub_key) {
  if (_ping_active || !pub_key) return false;
  if (_node_prefs && _node_prefs->path_hash_mode > 1) {
    operationError(zen::Operation::PING,
        zen::OperationOutcome::TRANSPORT_FAILURE,
        zen::OperationReason::UNSUPPORTED_PATH);
    showAlert("Ping not supported with 3-byte path hashes", 3000);
    return false;
  }

  _ping_active = true;
  _ping_tag = 0;
  _ping_sent_ms = millis();
  _ping_snr_out_x4 = 0;
  _ping_snr_back_x4 = 0;
  _ping_rtt_ms = 0;

  // Always install the callback before sending so the response cannot race it.
  the_mesh.setPingCallback(onPingResult, NULL);
  _ping_tag = the_mesh.sendPing(pub_key, _node_prefs ? _node_prefs->path_hash_mode + 1 : 1);
  if (_ping_tag == 0) {
    clearPing();
    return false;
  }
  return true;
}

void UITask::playMelody(const char* melody) {
#ifdef PIN_BUZZER
  // The ringtone editor shares the deliberate-preview bypass with melody
  // selection; it must not clear Quiet Time, DND or Notifications Off.
  buzzer.playForced(melody);
#endif
}

void UITask::previewMelody(uint8_t selection, uint8_t empty_fallback) {
#ifdef PIN_BUZZER
  // Manual previews are deliberate playback, independent of notification mute.
  SoundNotifier sn(buzzer, _node_prefs, _notif_mel_buf, sizeof(_notif_mel_buf));
  sn.preview(selection, empty_fallback);
#else
  (void)selection;
  (void)empty_fallback;
#endif
}

void UITask::stopMelody() {
#ifdef PIN_BUZZER
  buzzer.stop();
#endif
}

bool UITask::isMelodyPlaying() {
#ifdef PIN_BUZZER
  return buzzer.isPlaying();
#else
  return false;
#endif
}

void UITask::gotoMessagesScreen() {
  ((MessagesScreen*)messages_screen)->reset();
  setCurrScreen(messages_screen);
}

zen::PathAttemptSnapshot UITask::latestPathAttempt(const uint8_t* pub_key) const {
  return ((MessagesScreen*)messages_screen)->latestPathAttempt(pub_key);
}

void UITask::gotoMessagesCategory(uint8_t category) {
  ((MessagesScreen*)messages_screen)->enterCategory(category);
  setCurrScreen(messages_screen);
}

void UITask::openContactDM(const ContactInfo& ci) {
  if (!allowOnDeviceContactMessage(ci)) {
    operationWarning(zen::Operation::CHILD_MODE,
                     zen::OperationReason::CONTACT_NOT_ALLOWED);
    return;
  }
  ((MessagesScreen*)messages_screen)->reset();
  ((MessagesScreen*)messages_screen)->enterDM(ci);
  setCurrScreen(messages_screen);
}

bool UITask::allowOnDeviceChannelMessage(uint8_t index) const {
  if (!isChildModeLocked()) return true;
  ChannelDetails channel;
  if (!_node_prefs || !the_mesh.getChannel(index, channel)) return false;
  return zen::Policy::channelAllowed(_node_prefs, true, index,
                                      channel.name, channel.channel.secret);
}

bool UITask::setFavouriteSlot(int slot, const uint8_t* pub_key) {
  if (!_node_prefs || slot < 0 || slot >= ZenPrefs::FAVOURITES_DIAL_COUNT || !pub_key)
    return false;
  ContactInfo* contact = the_mesh.lookupContactByPubKey(pub_key, ZenPrefs::FAVOURITE_PREFIX_LEN);
  if (!contact || contact->type != ADV_TYPE_CHAT || !zen::Policy::favouriteContact(*contact)) {
    operationWarning(zen::Operation::FAVOURITES,
                     zen::OperationReason::FAVOURITE_REQUIRED);
    return false;
  }
  memcpy(_node_prefs->favourite_contacts[slot], pub_key, ZenPrefs::FAVOURITE_PREFIX_LEN);
  return true;
}

void UITask::openPreferredTranscript() {
  MessagesScreen* screen = (MessagesScreen*)messages_screen;
  enum TranscriptType : uint8_t { TRANSCRIPT_NONE, TRANSCRIPT_DM, TRANSCRIPT_ROOM, TRANSCRIPT_CHANNEL };
  TranscriptType best_type = TRANSCRIPT_NONE;
  uint8_t best_key[ZenPrefs::FAVOURITE_PREFIX_LEN] = {0};
  uint8_t best_channel = 0;
  uint32_t best_activity = 0;

  // First pass: newest transcript that still contains unread traffic.
  for (int i = 0; i < the_mesh.getNumContacts(); i++) {
    ContactInfo contact;
    if (!the_mesh.getContactByIdx(i, contact)) continue;
    bool room = contact.type == ADV_TYPE_ROOM;
    if (contact.type != ADV_TYPE_CHAT && !room) continue;
    if (!zen::Policy::contactAllowed(_node_prefs, isChildModeLocked(), &contact,
                                      room ? ADV_TYPE_ROOM : ADV_TYPE_CHAT)) continue;
    bool unread = room ? (getRoomUnread(contact.id.pub_key) > 0)
                       : (getDMUnread(contact.id.pub_key) > 0);
    if (!unread) continue;
    uint32_t activity = screen->latestDmActivity(contact.id.pub_key, true);
    if (activity >= best_activity && activity != 0) {
      best_activity = activity;
      best_type = room ? TRANSCRIPT_ROOM : TRANSCRIPT_DM;
      memcpy(best_key, contact.id.pub_key, sizeof(best_key));
    }
  }
  for (uint8_t i = 0; i < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails channel;
    if (!screen->channelUnread(i) || !the_mesh.getChannel(i, channel)) continue;
    if (!zen::Policy::channelAllowed(_node_prefs, isChildModeLocked(), i,
                                      channel.name, channel.channel.secret)) continue;
    uint32_t activity = screen->latestChannelActivity(i);
    if (activity >= best_activity && activity != 0) {
      best_activity = activity;
      best_type = TRANSCRIPT_CHANNEL;
      best_channel = i;
    }
  }

  // Second pass: with nothing unread, choose the newest transcript regardless
  // of whether its last entry was sent or received.
  if (best_type == TRANSCRIPT_NONE) {
    for (int i = 0; i < the_mesh.getNumContacts(); i++) {
      ContactInfo contact;
      if (!the_mesh.getContactByIdx(i, contact)) continue;
      bool room = contact.type == ADV_TYPE_ROOM;
      if (contact.type != ADV_TYPE_CHAT && !room) continue;
      if (!zen::Policy::contactAllowed(_node_prefs, isChildModeLocked(), &contact,
                                        room ? ADV_TYPE_ROOM : ADV_TYPE_CHAT)) continue;
      uint32_t activity = screen->latestDmActivity(contact.id.pub_key);
      if (activity >= best_activity && activity != 0) {
        best_activity = activity;
        best_type = room ? TRANSCRIPT_ROOM : TRANSCRIPT_DM;
        memcpy(best_key, contact.id.pub_key, sizeof(best_key));
      }
    }
    for (uint8_t i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails channel;
      if (!the_mesh.getChannel(i, channel)) continue;
      if (!zen::Policy::channelAllowed(_node_prefs, isChildModeLocked(), i,
                                        channel.name, channel.channel.secret)) continue;
      uint32_t activity = screen->latestChannelActivity(i);
      if (activity >= best_activity && activity != 0) {
        best_activity = activity;
        best_type = TRANSCRIPT_CHANNEL;
        best_channel = i;
      }
    }
  }

  if (best_type == TRANSCRIPT_CHANNEL) {
    screen->reset();
    screen->enterChannel(best_channel);
    setCurrScreen(messages_screen);
    return;
  }
  if (best_type == TRANSCRIPT_DM || best_type == TRANSCRIPT_ROOM) {
    ContactInfo* contact = the_mesh.lookupContactByPubKey(best_key, sizeof(best_key));
    if (contact) {
      screen->reset();
      screen->enterDM(*contact);
      setCurrScreen(messages_screen);
      return;
    }
  }
  showAlert("No recent messages", 1000);
}

int UITask::getRecentDMContacts(uint8_t out[][ZenPrefs::FAVOURITE_PREFIX_LEN], int max) const {
  return ((MessagesScreen*)messages_screen)->getRecentDMContacts(out, max);
}

void UITask::addChannelMsg(uint8_t channel_idx, const char* text, uint32_t timestamp) {
  bool present = notificationAllowed(UIEventType::channelMessage, 0, nullptr, channel_idx);
  ((MessagesScreen*)messages_screen)->addChannelMsg(channel_idx, text, timestamp, present);
}

int UITask::getChannelUnreadCount() const {
  return ((MessagesScreen*)messages_screen)->getTotalChannelUnread(isChildModeLocked());
}

int UITask::markMessageCategoryRead(uint8_t category) {
  int count = 0;
  if (category == 0) {
    count = getDMUnreadTotal();
    clearAllDMUnread();
  } else if (category == 1) {
    count = getChannelUnreadCount();
    ((MessagesScreen*)messages_screen)->clearAllChannelUnread();
  } else if (category == 2) {
    count = getRoomUnreadCount();
    clearRoomUnread();
  }
  _next_refresh = 0;
  return count;
}

void UITask::onMsgAck(uint32_t ack_crc) {
  if (((MessagesScreen*)messages_screen)->markDmDelivered(ack_crc))
    _next_refresh = 0;
}

void UITask::onChannelRelayed(uint32_t seq) {
  ((MessagesScreen*)messages_screen)->markChannelRelayed(seq);
}

void UITask::onChannelRelayExpired(uint32_t seq, uint8_t heard, bool transmitted) {
  bool tracked = ((MessagesScreen*)messages_screen)->markChannelRelayExpired(seq);
  if (zen::RelayEchoTiming::noRelayHeard(tracked, transmitted, heard))
    operationWarning(zen::Operation::CHANNEL, zen::OperationReason::NO_RELAY);
}

void UITask::onNodeLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) {
  if (!_remote_node.complete(zen::RemoteNodeCoordinator::LOGIN_OWNER, pub_key,
      success ? zen::RemoteNodeOperation::SUCCESS
              : zen::RemoteNodeOperation::REJECTED)) return;
  zen::NodeLoginCoordinator::Attempt attempt;
  if (!_node_login.complete(pub_key, attempt)) return;
  zen::Operation operation = attempt.owner == zen::NodeLoginCoordinator::ADMIN
      ? zen::Operation::ADMIN_LOGIN
      : (attempt.owner == zen::NodeLoginCoordinator::SENSOR
          ? zen::Operation::SENSOR_LOGIN : zen::Operation::ROOM_LOGIN);
  if (success)
    publishRemoteOperation(operation, zen::RemoteNodeOperation::SUCCESS);
  if (success && attempt.owner == zen::NodeLoginCoordinator::MESSAGES) {
    _node_login.markLoggedIn(pub_key);
    if (!the_mesh.saveRoomPassword(pub_key, attempt.password))
      operationWarning(zen::Operation::STORAGE, zen::OperationReason::SAVE_FAILED);
  } else if (!success && attempt.owner == zen::NodeLoginCoordinator::MESSAGES && attempt.used_saved_password) {
    if (!the_mesh.forgetRoomPassword(pub_key))
      operationWarning(zen::Operation::STORAGE, zen::OperationReason::SAVE_FAILED);
  }
#if ZEN_FEATURE_ADMIN
  if (attempt.owner == zen::NodeLoginCoordinator::ADMIN)
    ((AdminScreen*)admin_screen)->onNodeLoginResult(pub_key, success, permissions);
  else
#endif
  if (attempt.owner == zen::NodeLoginCoordinator::SENSOR)
    ((HomeScreen*)home)->onSensorLoginResult(pub_key, success);
  else
    ((MessagesScreen*)messages_screen)->onNodeLoginResult(pub_key, success, permissions);
  // Unlike the keypress-driven showAlert() calls elsewhere, this fires from a
  // background mesh response with no keypress to schedule a redraw — without
  // forcing one, the alert's short expiry can lapse before the next scheduled
  // refresh ever draws it.
  _next_refresh = 0;
}

bool UITask::startNodeLogin(zen::NodeLoginCoordinator::Owner owner,
                            const ContactInfo& contact, const char* password,
                            bool used_saved_password) {
  if (_node_login.active() || _remote_node.active()) return false;
  uint32_t est_timeout = 0;
  if (!the_mesh.sendNodeLogin(contact, password, est_timeout)) return false;
  bool has_known_path = contact.out_path_len != OUT_PATH_UNKNOWN;
  uint32_t deadline = zen::RemoteNodeOperation::responseDeadline(millis(), est_timeout);
  if (_remote_node.begin(zen::RemoteNodeCoordinator::LOGIN_OWNER,
                         zen::RemoteNodeOperation::LOGIN, contact.id.pub_key,
                         has_known_path, zen::RemoteNodeOperation::RETRY_SAFE,
                         deadline) &&
      _node_login.begin(owner, contact.id.pub_key, password, used_saved_password)) {
    publishRemoteOperation(owner == zen::NodeLoginCoordinator::ADMIN
            ? zen::Operation::ADMIN_LOGIN
            : (owner == zen::NodeLoginCoordinator::SENSOR
                ? zen::Operation::SENSOR_LOGIN : zen::Operation::ROOM_LOGIN),
        zen::RemoteNodeOperation::PENDING);
    return true;
  }
  _remote_node.cancel(zen::RemoteNodeCoordinator::LOGIN_OWNER);
  the_mesh.cancelUiPendingLogin(contact.id.pub_key);
  return false;
}

bool UITask::retryNodeLogin(zen::NodeLoginCoordinator::Attempt& attempt) {
  ContactInfo* contact = the_mesh.lookupContactByPubKey(attempt.pub_key,
                                                        sizeof(attempt.pub_key));
  if (!contact) {
    _remote_node.fail(zen::RemoteNodeCoordinator::LOGIN_OWNER,
                      zen::RemoteNodeOperation::NODE_MISSING);
    return false;
  }

  zen::RemoteNodeOperation::Action retry = _remote_node.timeout(
      zen::RemoteNodeCoordinator::LOGIN_OWNER, millis(),
      contact->out_path_len != OUT_PATH_UNKNOWN);
  if (retry == zen::RemoteNodeOperation::FINISH_TIMEOUT ||
      retry == zen::RemoteNodeOperation::FINISH_UNKNOWN) return false;
  uint32_t est_timeout = 0;
  if (!the_mesh.sendNodeLogin(*contact, attempt.password, est_timeout,
                              retry == zen::RemoteNodeOperation::RETRY_FLOOD)) {
    _remote_node.fail(zen::RemoteNodeCoordinator::LOGIN_OWNER,
                      zen::RemoteNodeOperation::SEND_FAILED);
    return false;
  }
  uint32_t deadline = zen::RemoteNodeOperation::responseDeadline(millis(), est_timeout);
  if (_remote_node.restart(zen::RemoteNodeCoordinator::LOGIN_OWNER, deadline) &&
      _node_login.restart(attempt)) {
    publishRemoteOperation(attempt.owner == zen::NodeLoginCoordinator::ADMIN
            ? zen::Operation::ADMIN_LOGIN
            : (attempt.owner == zen::NodeLoginCoordinator::SENSOR
                ? zen::Operation::SENSOR_LOGIN : zen::Operation::ROOM_LOGIN),
        zen::RemoteNodeOperation::PENDING);
    return true;
  }
  _remote_node.cancel(zen::RemoteNodeCoordinator::LOGIN_OWNER);
  the_mesh.cancelUiPendingLogin(attempt.pub_key);
  return false;
}

void UITask::onNodeLoginCancelled(const uint8_t* prefix) {
  _remote_node.cancel(zen::RemoteNodeCoordinator::LOGIN_OWNER, prefix);
  zen::NodeLoginCoordinator::Attempt attempt;
  if (!_node_login.complete(prefix, attempt)) return;
#if ZEN_FEATURE_ADMIN
  if (attempt.owner == zen::NodeLoginCoordinator::ADMIN)
    ((AdminScreen*)admin_screen)->onNodeLoginTimeout(prefix);
  else
#endif
  if (attempt.owner == zen::NodeLoginCoordinator::SENSOR)
    ((HomeScreen*)home)->onSensorLoginTimeout(prefix);
  else
    ((MessagesScreen*)messages_screen)->onNodeLoginTimeout(prefix);
  _next_refresh = 0;
}

void UITask::cancelNodeLogin(zen::NodeLoginCoordinator::Owner owner, const uint8_t* pub_key) {
  if (_node_login.cancel(owner, pub_key)) {
    _remote_node.cancel(zen::RemoteNodeCoordinator::LOGIN_OWNER, pub_key);
    the_mesh.cancelUiPendingLogin(pub_key);
  }
}

void UITask::logoutRoom(const uint8_t* pub_key) {
  _node_login.forgetLoggedIn(pub_key);
  the_mesh.logoutRoom(pub_key);
}

void UITask::onAdminReply(const uint8_t* pub_key, const char* text) {
#if ZEN_FEATURE_ADMIN
  ((AdminScreen*)admin_screen)->onAdminReply(pub_key, text);
#else
  (void)pub_key; (void)text;
#endif
  _next_refresh = 0;   // same reasoning as onNodeLoginResult above
}

void UITask::onSensorTelemetry() {
  if (_remote_node.complete(zen::RemoteNodeCoordinator::SENSOR_OWNER,
                            _remote_node.operation().key(),
                            zen::RemoteNodeOperation::SUCCESS))
    publishRemoteOperation(zen::Operation::TELEMETRY,
                           zen::RemoteNodeOperation::SUCCESS);
  _next_refresh = 0;  // redraw the visible card, never wake the display
}

bool UITask::addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text, uint32_t sender_timestamp) {
  bool added = ((MessagesScreen*)messages_screen)->addDMMsg(pub_key, outgoing, text,
                                                            sender_timestamp);
  if (added) reconcileDMUnread();
  return added;
}

int UITask::addOwnChannelMsg(uint8_t channel_idx, const char* text,
                             int text_len, uint32_t timestamp) {
  char entry[sizeof(ChHistEntry::text)];
  static const char prefix[] = "Me: ";
  memcpy(entry, prefix, sizeof(prefix) - 1);
  size_t available = sizeof(entry) - sizeof(prefix);
  size_t requested = text_len < 0 ? strlen(text) : (size_t)text_len;
  if (requested > available) requested = available;
  size_t copied = mesh::validUtf8PrefixLength(text, requested);
  memcpy(entry + sizeof(prefix) - 1, text, copied);
  entry[sizeof(prefix) - 1 + copied] = '\0';
  int pos = ((MessagesScreen*)messages_screen)->addChannelMsg(channel_idx, entry,
                                                               timestamp, false);
  _next_refresh = 0;
  return pos;
}

void UITask::armChannelRelay(int history_pos, uint32_t seq) {
  ((MessagesScreen*)messages_screen)->armChannelRelay(history_pos, seq);
}

int UITask::getDMUnreadTotal() const {
  return _message_unread.total(zen::MessageUnreadCoordinator::DIRECT,
      [this](const uint8_t* key) {
        return ((MessagesScreen*)messages_screen)->dmHistCountForContact(key);
      });
}

uint8_t UITask::getDMUnread(const uint8_t* pub_key) const {
  int held = ((MessagesScreen*)messages_screen)->dmHistCountForContact(pub_key);
  return _message_unread.get(zen::MessageUnreadCoordinator::DIRECT, pub_key, held);
}

int UITask::getRoomUnreadCount() const {
  return _message_unread.total(zen::MessageUnreadCoordinator::ROOM,
      [this](const uint8_t* key) {
        return ((MessagesScreen*)messages_screen)->dmHistCountForContact(key);
      });
}

uint8_t UITask::getRoomUnread(const uint8_t* pub_key) const {
  int held = ((MessagesScreen*)messages_screen)->dmHistCountForContact(pub_key);
  return _message_unread.get(zen::MessageUnreadCoordinator::ROOM, pub_key, held);
}

void UITask::clearRoomUnread(const uint8_t* pub_key) {
  _message_unread.clear(zen::MessageUnreadCoordinator::ROOM, pub_key);
}

void UITask::clearRoomUnread() {
  _message_unread.clearAll(zen::MessageUnreadCoordinator::ROOM);
}

void UITask::reconcileDMUnread() {
  auto retained = [this](const uint8_t* key) {
    return ((MessagesScreen*)messages_screen)->dmHistCountForContact(key);
  };
  _message_unread.reconcile(zen::MessageUnreadCoordinator::DIRECT, retained);
  _message_unread.reconcile(zen::MessageUnreadCoordinator::ROOM, retained);
}

void UITask::showAlert(const char* text, int duration_millis) {
  showAlertPriority(text, duration_millis, 0);
}

void UITask::showAlertPriority(const char* text, int duration_millis,
                               uint8_t priority) {
  if (!_notification_popup.accept(priority, millis(), _alert_expiry)) return;
  snprintf(_alert, sizeof(_alert), "%s", text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::publishRemoteOperation(zen::Operation operation,
                                    zen::RemoteNodeOperation::Result result,
                                    zen::OperationStatus* screen_status) {
  publishOperation(zen::RemoteOperationResultAdapter::convert(
      operation, _remote_node.operation(), result), screen_status);
}

bool UITask::remoteStatus(zen::RemoteNodeCoordinator::Owner owner,
                          char* out, size_t size) const {
  if (!out || !size || !_remote_node.ownedBy(owner)) return false;
  const zen::RemoteNodeOperation& op = _remote_node.operation();
  snprintf(out, size, "Attempt %u %s", op.attempt(),
           op.route() == zen::RemoteNodeOperation::PATH ? "path" : "flood");
  return true;
}

void UITask::publishOperation(const zen::OperationResult& result,
                              zen::OperationStatus* screen_status) {
  if (screen_status) screen_status->set(result);
  uint32_t now = isTimeSyncPending() ? 0 : rtc_clock.getCurrentTime();
  zen::OperationResultCoordinator::Decision decision = _operation_results.publish(
      _diagnostic_log, now, millis(), result);
  if (!decision.popup) return;
  char message[80];
  snprintf(message, sizeof(message), "%s: %s\n%s",
           decision.severity == zen::DiagnosticLog::ERROR ? "Error" :
           (decision.severity == zen::DiagnosticLog::WARNING ? "Warning" : "Success"),
           zen::OperationResultCatalog::operationName(result.operation),
           zen::OperationResultCatalog::status(result));
  zen::NotificationEvent event = zen::NotificationProfiles::event(
      decision.severity == zen::DiagnosticLog::ERROR
          ? zen::NotificationType::ERROR : zen::NotificationType::WARNING);
  event.urgent_wake = (result.flags & zen::RESULT_SCREEN_WAKE) != 0;
  event.popup = message;
  event.popup_ms = decision.severity == zen::DiagnosticLog::ERROR ? 2200 : 1800;
  dispatchNotification(event);
}

bool UITask::notificationAllowed(UIEventType event, uint8_t contact_type,
                                 const uint8_t* pub_key, int channel_idx) const {
  if (!isChildModeLocked()) return true;

  if (event == UIEventType::advertReceivedFlood ||
      event == UIEventType::advertReceivedZeroHop ||
      event == UIEventType::newContact)
    return zen::NotificationEligibility::advert(true);

  if (event == UIEventType::contactMessage || event == UIEventType::roomMessage) {
    if (!pub_key) return false;
    ContactInfo* contact = the_mesh.lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint8_t expected_type = event == UIEventType::roomMessage ? ADV_TYPE_ROOM : ADV_TYPE_CHAT;
    return zen::NotificationEligibility::contact(
        _node_prefs, true, contact, contact_type, expected_type);
  }

  if (event == UIEventType::channelMessage) {
    if (channel_idx < 0 || channel_idx >= MAX_GROUP_CHANNELS) return false;
    ChannelDetails channel;
    return the_mesh.getChannel(channel_idx, channel) &&
           zen::NotificationEligibility::channel(
               _node_prefs, true, channel_idx, channel.name,
               channel.channel.secret);
  }

  return true;
}

bool UITask::isQuietTimeActive() const {
  return zen::Features::QUIET_TIME &&
         quiettime::active(_node_prefs, rtc_clock.getCurrentTime(), !isTimeSyncPending());
}

bool UITask::isNotificationQuietActive() const {
  return _dnd_active || isQuietTimeActive();
}

bool UITask::isNotificationAudioMuted() const {
#ifdef PIN_BUZZER
  return zen::NotificationPolicy::audioMuted(
      (zen::NotificationPolicy::Mode)getNotificationMode(), isClientConnected(),
      isNotificationQuietActive());
#else
  return false;
#endif
}

zen::NotificationContext UITask::notificationContext() const {
  zen::NotificationContext context;
  context.mode = (zen::NotificationPolicy::Mode)getNotificationMode();
  context.screen_wake = zen::NotificationPolicy::screenWake(
      _node_prefs ? _node_prefs->notification_screen_wake : 1);
  context.client_connected = isClientConnected();
  context.silent = _dnd_active;
  context.quiet_time = isQuietTimeActive();
  context.low_power = isLowPowerMode();
  context.emergency = isEmergencyMode();
  return context;
}

zen::NotificationDecision UITask::dispatchNotification(
    const zen::NotificationEvent& event, bool present_outputs) {
  zen::NotificationDecision decision =
      zen::NotificationCoordinator::decide(event, notificationContext());
  if (present_outputs) executeNotification(event, decision);
  return decision;
}

void UITask::executeNotification(
    const zen::NotificationEvent& event,
    const zen::NotificationDecision& decision) {
  if (decision.show_visual && event.popup) {
    uint8_t priority = zen::NotificationCoordinator::popupPriority(event.type);
    showAlertPriority(event.popup, event.popup_ms ? event.popup_ms : 3000,
                      priority);
    wakeForNotification(decision.wake_screen);
  }
  if (decision.play_sound || decision.vibrate)
    presentNotification(event, decision.play_sound, decision.vibrate);
}

void UITask::notifyHomeAction() {
  // Homepage control feedback is optional audio, not a delivery ACK. Leave
  // message/routing acknowledgements unchanged while honoring Silent, Quiet
  // Time, Notifications Off and connected Auto here.
  if (!isNotificationAudioMuted()) notify(UIEventType::ack);
}

static zen::NotificationType notificationTypeFor(UIEventType event) {
  switch (event) {
    case UIEventType::contactMessage: return zen::NotificationType::DIRECT_MESSAGE;
    case UIEventType::channelMessage: return zen::NotificationType::CHANNEL_MESSAGE;
    case UIEventType::roomMessage: return zen::NotificationType::ROOM_MESSAGE;
    case UIEventType::advertReceivedFlood: return zen::NotificationType::ADVERT_FLOOD;
    case UIEventType::advertReceivedZeroHop: return zen::NotificationType::ADVERT_LOCAL;
    case UIEventType::newContact: return zen::NotificationType::NEW_CONTACT;
    case UIEventType::ack: return zen::NotificationType::UI_FEEDBACK;
    case UIEventType::none:
    default: return zen::NotificationType::NONE;
  }
}

void UITask::notify(UIEventType event) {
  zen::NotificationEvent notification =
      zen::NotificationProfiles::event(notificationTypeFor(event));
  notification.eligible = notificationAllowed(event);
  dispatchNotification(notification);
}

void UITask::onNewContact(const ContactInfo& contact) {
  // This callback only follows successful insertion into the contact table.
  // It is separate from routine adverts and requested Discover replies.
  char alert[80];
  snprintf(alert, sizeof(alert), "New contact: %.30s", contact.name);
  zen::NotificationEvent event = zen::NotificationProfiles::event(
      zen::NotificationType::NEW_CONTACT);
  event.eligible = notificationAllowed(UIEventType::newContact);
  event.popup = alert;
  event.popup_ms = 5000;
  dispatchNotification(event);
}

void UITask::presentNotification(const zen::NotificationEvent& event,
                                 bool play_sound, bool vibrate) {
#if defined(PIN_BUZZER)
if (play_sound) {
  SoundNotifier sn(buzzer, _node_prefs, _notif_mel_buf, sizeof(_notif_mel_buf));
  sn.play(event);
}
#else
  (void)play_sound;
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (vibrate && event.type != zen::NotificationType::NONE) {
    vibration.trigger();
  }
#else
  (void)vibrate;
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    clearRoomUnread();
    _message_unread.reset();
    ((MessagesScreen*)messages_screen)->clearAllChannelUnread();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount, uint8_t contact_type, const uint8_t* pub_key) {
  // Legacy callers split message presentation and sound into newMsg()/notify().
  // Apply the same presentation decision; delivery/unread state remains intact.
  char alert[80];
  snprintf(alert, sizeof(alert), "Msg: %.20s", from_name);
  zen::NotificationEvent event = zen::NotificationProfiles::event(
      contact_type == ADV_TYPE_ROOM ? zen::NotificationType::ROOM_MESSAGE
                                    : zen::NotificationType::DIRECT_MESSAGE);
  UIEventType ui_type = contact_type == ADV_TYPE_ROOM
      ? UIEventType::roomMessage : UIEventType::contactMessage;
  event.eligible = notificationAllowed(ui_type, contact_type, pub_key);
  event.popup = alert;
  event.popup_ms = 3000;
  if (pub_key) { memcpy(event.prefix, pub_key, 4); event.prefix_valid = true; }
  // Sound remains the responsibility of the legacy follow-up notify() call.
  processMessageNotification(event, path_len, from_name, text, msgcount,
                             contact_type, pub_key, false);
}

void UITask::incomingMessage(UIEventType event, uint8_t path_len,
                             const char* from_name, const char* text, int msgcount,
                             uint8_t contact_type, const uint8_t* pub_key,
                             int channel_idx) {
  uint8_t source_state = 0;
  if (event == UIEventType::contactMessage && pub_key)
    source_state = zen::NotificationPreferences::dmState(_node_prefs, pub_key);
  else if (event == UIEventType::channelMessage && channel_idx >= 0 && channel_idx < 64)
    source_state = zen::NotificationPreferences::channelState(_node_prefs, channel_idx);
  char alert[80];
  snprintf(alert, sizeof(alert), "Msg: %.20s", from_name);
  zen::NotificationEvent notification = zen::NotificationProfiles::event(
      notificationTypeFor(event));
  notification.eligible = notificationAllowed(event, contact_type, pub_key, channel_idx);
  notification.source = (zen::NotificationPolicy::Source)source_state;
  notification.popup = alert;
  notification.popup_ms = 3000;
  notification.channel = channel_idx;
  if (pub_key) {
    memcpy(notification.prefix, pub_key, 4);
    notification.prefix_valid = true;
  }
  processMessageNotification(notification, path_len, from_name, text, msgcount,
                             contact_type, pub_key, true);
}

void UITask::processMessageNotification(
    const zen::NotificationEvent& event, uint8_t path_len,
    const char* from_name, const char* text, int msgcount,
    uint8_t contact_type, const uint8_t* pub_key, bool include_sound) {
  zen::NotificationDecision decision = dispatchNotification(event, false);
  if (decision.record_unread)
    handleNewMsg(path_len, from_name, text, msgcount, contact_type, pub_key);
  if (!include_sound) {
    decision.play_sound = false;
    decision.vibrate = false;
  }
  executeNotification(event, decision);
}

void UITask::handleNewMsg(uint8_t path_len, const char* from_name, const char* text,
                          int msgcount, uint8_t contact_type, const uint8_t* pub_key) {
  (void)path_len;
  (void)text;

  // Capture visibility before notification handling can wake the panel. An
  // open transcript is only "read" when it was already physically visible.
  bool viewing_contact = pub_key != nullptr
      && ((MessagesScreen*)messages_screen)->isViewingContact(pub_key);

  // The mesh queue count includes deliberately silent traffic. Keep the normal
  // exact count outside child mode, but count only allowed messages while locked.
  if (isChildModeLocked()) {
    if (_msgcount < 999) _msgcount++;
  } else {
    _msgcount = msgcount;
  }
  if (contact_type == ADV_TYPE_ROOM && pub_key != nullptr)
    _message_unread.record(zen::MessageUnreadCoordinator::ROOM, pub_key,
                           viewing_contact);
  if (contact_type == ADV_TYPE_CHAT && pub_key != nullptr) {
    _message_unread.record(zen::MessageUnreadCoordinator::DIRECT, pub_key,
                           viewing_contact);
  }
}

void UITask::wakeForNotification(bool allow_wake) {
  if (!_display) return;
  uint32_t normal = autoOffMillis();
  auto action = _notification_wake.present(
      _display->isOn(), allow_wake, millis(), normal);
  if (action.turn_on) turnDisplayOn();
  if (action.deadline) _auto_off = action.deadline;
  if (action.refresh) _next_refresh = 100;
}

void UITask::notifyLowBattery() {
  uint32_t now = isTimeSyncPending() ? 0 : rtc_clock.getCurrentTime();
  publishOperation(zen::OperationResult::make(
      zen::Operation::BATTERY, zen::OperationOutcome::WARNING,
      zen::OperationReason::LOW_BATTERY, zen::RESULT_BACKGROUND));
  zen::NotificationEvent event = zen::NotificationProfiles::event(
      zen::NotificationType::LOW_BATTERY);
  event.popup = "Low Battery";
  event.popup_ms = 5000;
  dispatchNotification(event);
}

void UITask::syncPowerInputs() {
  auto& request = _power.requests();
  request.cardkb_present = isCardKBConnected();
  if (_node_prefs) {
    request.gps_saved_on = _node_prefs->gps_enabled != 0;
    request.bluetooth_saved_on = _node_prefs->bluetooth_enabled != 0;
    request.brightness = _node_prefs->display_brightness;
  }
  _power.restrictions().child_locked = isChildModeLocked();
}

void UITask::reconcilePower(bool force) {
  if (_applying_power) return;
  syncPowerInputs();
  if (force) _power.invalidateApplied();
  zen::PeripheralPowerCoordinator::Transition transition = _power.transition();
  if (!transition.changes) return;

  _applying_power = true;
  bool relock_parent = false;
  bool gps_failed = false;
  bool bluetooth_failed = false;
  bool display_failed = false;
  bool cardkb_failed = false;
  bool radio_failed = false;
  auto applied = transition.target;
  const auto previous = _power.hasAppliedState()
      ? _power.applied() : zen::PeripheralPowerCoordinator::EffectiveState();

  // Stop input before blanking the panel. A resumed CardKB deliberately waits
  // for key release, preventing the wake press from leaking into the UI.
#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  if ((transition.changes & zen::PeripheralPowerCoordinator::CHANGE_CARDKB) &&
      !transition.target.cardkb_polling) {
    _cardkb.suspend();
    discardCardKBKeys();
  }
#endif

  if (transition.changes & (zen::PeripheralPowerCoordinator::CHANGE_RADIO |
                            zen::PeripheralPowerCoordinator::CHANGE_POWER_CONTEXT)) {
    the_mesh.applyPowerState(transition.target.low_power,
                             transition.target.emergency);
    if (the_mesh.isLowPowerMode() != transition.target.low_power ||
        the_mesh.isEmergencyMode() != transition.target.emergency ||
        the_mesh.radioAvailable() != transition.target.radio_on) {
      applied.low_power = previous.low_power;
      applied.emergency = previous.emergency;
      applied.radio_on = previous.radio_on;
      radio_failed = true;
    }
  }

  if (_gps && (transition.changes &
      (zen::PeripheralPowerCoordinator::CHANGE_GPS_POLICY |
       zen::PeripheralPowerCoordinator::CHANGE_GPS_FORCE))) {
    bool policy_ok = true;
    bool force_ok = true;
    if (transition.changes & zen::PeripheralPowerCoordinator::CHANGE_GPS_POLICY) {
      zen::GpsService::Configuration config;
      config.enabled = transition.target.gps_policy_on;
      config.adaptive = _node_prefs && _node_prefs->gps_adaptive;
      config.interval_seconds = _node_prefs ? _node_prefs->gps_interval : 0;
      if (_power.requests().gps_session == 1)
        config.purpose = transition.target.emergency
            ? zen::GpsService::GPS_EMERGENCY : zen::GpsService::GPS_MANUAL;
      policy_ok = _gps->applyConfiguration(config);
    }
    if (transition.changes & zen::PeripheralPowerCoordinator::CHANGE_GPS_FORCE)
      force_ok = _gps->setPowerClaim(
          transition.target.gps_force_on,
          transition.target.emergency && _power.requests().emergency_gps
              ? zen::GpsService::GPS_EMERGENCY : zen::GpsService::GPS_TIME_SYNC);
    if (!policy_ok) applied.gps_policy_on = previous.gps_policy_on;
    if (!force_ok) applied.gps_force_on = previous.gps_force_on;
    gps_failed = !policy_ok || !force_ok;
  }

  if (transition.changes & zen::PeripheralPowerCoordinator::CHANGE_BLUETOOTH) {
    if (!applyBluetoothTarget(transition.target.bluetooth_on)) {
      applied.bluetooth_on = isBluetoothEnabled();
      bluetooth_failed = true;
    }
  }

  if (_display && (transition.changes & zen::PeripheralPowerCoordinator::CHANGE_BRIGHTNESS))
    _display->setBrightness(transition.target.brightness);

  if (_display && (transition.changes & zen::PeripheralPowerCoordinator::CHANGE_DISPLAY)) {
    if (transition.target.display_on) _display->turnOn();
    else {
      _display->turnOff();
      _notification_wake.displayOff();
      relock_parent = _node_prefs && _node_prefs->child_mode_enabled &&
                      _zen_runtime.parentUnlocked();
    }
    if (_display->isOn() != transition.target.display_on) {
      applied.display_on = _display->isOn();
      display_failed = true;
    }
  }

#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  if ((transition.changes & zen::PeripheralPowerCoordinator::CHANGE_CARDKB) &&
      transition.target.cardkb_polling) _cardkb.resume();
  if ((transition.changes & zen::PeripheralPowerCoordinator::CHANGE_CARDKB) &&
      _cardkb.isActive() != transition.target.cardkb_polling) {
    applied.cardkb_polling = _cardkb.isActive();
    cardkb_failed = true;
  }
#endif

  _power.markApplied(applied);
  _applying_power = false;
  // Relocking changes the Bluetooth restriction, so do it after completing
  // this transition and let applyChildMode() start a second reconciliation.
  if (relock_parent) setChildAdminUnlocked(false);
  // Diagnostics can request a notification wake, so emit them only after the
  // transition guard is released. The retained applied/effective mismatch is
  // retried on the next state event rather than by a background loop.
  if (gps_failed) operationError(zen::Operation::GPS,
      zen::OperationOutcome::TRANSPORT_FAILURE, zen::OperationReason::POWER_APPLY_FAILED);
  if (bluetooth_failed) operationError(zen::Operation::BLUETOOTH,
      zen::OperationOutcome::TRANSPORT_FAILURE, zen::OperationReason::APPLY_FAILED);
  if (display_failed) operationError(zen::Operation::SCREEN,
      zen::OperationOutcome::TRANSPORT_FAILURE, zen::OperationReason::POWER_APPLY_FAILED);
  if (cardkb_failed) operationError(zen::Operation::CARDKB,
      zen::OperationOutcome::TRANSPORT_FAILURE, zen::OperationReason::POWER_APPLY_FAILED);
  if (radio_failed) operationError(zen::Operation::RADIO,
      zen::OperationOutcome::TRANSPORT_FAILURE, zen::OperationReason::POWER_APPLY_FAILED);
}

bool UITask::applyBluetoothTarget(bool enabled) {
  if (!_interfaceManager) return false;
  switch (_bluetooth_policy.action(enabled, isBluetoothEnabled())) {
    case zen::BluetoothPolicyAdapter::ENABLE:
      _interfaceManager->enableBluetooth();
      break;
    case zen::BluetoothPolicyAdapter::DISABLE:
      _interfaceManager->disableBluetooth();
      break;
    default:
      break;
  }
  return isBluetoothEnabled() == enabled;
}

void UITask::observeBluetoothState() {
  zen::BluetoothPolicyAdapter::Event event = _bluetooth_policy.observe(
      isBluetoothEnabled(), isBLEConnected());
  if (event != zen::BluetoothPolicyAdapter::CONNECTED &&
      event != zen::BluetoothPolicyAdapter::DISCONNECTED) return;

  // Connection observations are diagnostic only. MeshCore owns the callbacks,
  // reconnect behaviour and bonds; Zen neither delays nor modifies them.
  zen::OperationResult result = zen::OperationResult::make(
      zen::Operation::BLUETOOTH, zen::OperationOutcome::SUCCESS,
      event == zen::BluetoothPolicyAdapter::DISCONNECTED
          ? zen::OperationReason::DISCONNECTED : zen::OperationReason::NONE,
      zen::RESULT_BACKGROUND | zen::RESULT_LOG_SUCCESS);
  publishOperation(result);
}

void UITask::setDisplayRequested(bool on) {
  _power.requests().display_on = on;
  reconcilePower();
}

void UITask::setLowPowerMode(bool active) {
  if (isLowPowerMode() == active) return;
  if (!active && _emergency_window.active()) {
    _emergency_window.cancel();
    _power.requests().emergency_gps = false;
    _power.requests().bluetooth_session = zen::PeripheralPowerCoordinator::NO_OVERRIDE;
  }
  _power.restrictions().low_power = active;
  _power.restrictions().emergency = _emergency_window.active();

  if (active) {
    publishOperation(zen::OperationResult::make(
        zen::Operation::POWER, zen::OperationOutcome::WARNING,
        zen::OperationReason::LOW_POWER_MODE, zen::RESULT_BACKGROUND));
    ((MessagesScreen*)messages_screen)->cancelDmResends();
    the_mesh.cancelSensorTelemetry();
#if ZEN_FEATURE_ADMIN
    the_mesh.cancelAdminCommand();
#endif
    _power.requests().emergency_gps = false;
    _power.requests().bluetooth_session = zen::PeripheralPowerCoordinator::NO_OVERRIDE;
    reconcilePower();
    zen::NotificationEvent event = zen::NotificationProfiles::event(
        zen::NotificationType::WARNING);
    event.popup = "Low Power";
    event.popup_ms = 5000;
    dispatchNotification(event);
  } else {
    _power.requests().gps_session = zen::PeripheralPowerCoordinator::NO_OVERRIDE;
    _power.requests().bluetooth_session = zen::PeripheralPowerCoordinator::NO_OVERRIDE;
    reconcilePower();
    showAlert("Power Restored", 2000);
    _next_refresh = 0;
  }
}

void UITask::setEmergencyMode(bool active) {
  if (active) {
    if (!isLowPowerMode() || _emergency_window.active()) return;
    _emergency_window.start(millis());
    _power.requests().emergency_gps = false;
    _power.requests().bluetooth_session = 0;
    _power.restrictions().emergency = true;
    reconcilePower();
    showAlert("Emergency Enabled", 1200);
  } else {
    bool was_active = the_mesh.isEmergencyMode();
    _emergency_window.cancel();
    _power.requests().emergency_gps = false;
    _power.requests().bluetooth_session = 0;
    _power.restrictions().emergency = false;
    ((MessagesScreen*)messages_screen)->cancelDmResends();
    reconcilePower();
    if (was_active) showAlert("Emergency Ended", 1200);
  }
  _next_refresh = 0;
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  unsigned long cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

// Centred alert box. Long text used to be drawn as one drawTextCentered line
// that overflowed the border on both sides (e.g. "GPS on, tracking started"
// is already wider than a 128 px OLED); wrap it to up to three lines inside
// the box instead. Uses the shared wrap scratch (s_wrap_*) — single-threaded
// render path, same contract as the message views.
void UITask::renderAlertOverlay() {
  _display->setTextSize(1);
  const int lh    = _display->getLineHeight();
  const int pad   = 3;
  const int box_w = _display->width() - 8;
  const int box_x = 4;
  int nl = FullscreenMsgView::wrapLines(*_display, _alert, box_w - pad * 2, s_wrap_lines, 3);
  if (nl < 1) nl = 1;
  int box_h = nl * lh + pad * 2;
  int box_y = (_display->height() - box_h) / 2;
  _display->setColor(ZenDisplayDriver::DARK);
  _display->fillRect(box_x, box_y, box_w, box_h);
  _display->setColor(ZenDisplayDriver::LIGHT);
  _display->drawRect(box_x, box_y, box_w, box_h);
  for (int i = 0; i < nl; i++)
    _display->drawTextCentered(_display->width() / 2, box_y + pad + i * lh, s_wrap_lines[i]);
}

void UITask::openScreen(ZenUIScreen* destination, ZenUIScreen* return_to) {
  if (return_to) _screen_history.push(return_to);
  setCurrScreen(destination);
}

void UITask::returnToScreen(ZenUIScreen* fallback) {
  ZenUIScreen* destination = _screen_history.pop();
  setCurrScreen(destination ? destination : fallback);
}

void UITask::setCurrScreen(ZenUIScreen* c) {
  // Fail safe on a null target: a screen pointer left uninitialised (member
  // declared + navigator wired, but the `new XScreen()` line forgotten in
  // begin()) stays nullptr thanks to the in-class initialisers. Bail here so
  // that mistake is an inert no-op instead of a null deref in render()/poll().
  if (ScreenTransition::apply(curr, c))
    _next_refresh = 100;
}

void UITask::showBootStage(const char* stage) {
  if (!stage) return;
  _boot_stage = stage;
  if (!_display || !bootSplashActive() || !_display->isOn()) return;
  _display->startFrame();
  curr->render(*_display);
  _display->endFrame();
}

bool UITask::savePrefsIfDirty(bool& dirty) {
  if (!dirty) return false;
  // Keep the dirty flag set when persistence fails so a later exit/shutdown
  // can retry the write. MyMesh reports the failure through the shared event
  // path, avoiding a second popup here.
  if (!the_mesh.savePrefs()) {
    _deferred_prefs_save = true;
    _deferred_prefs_attempts = 0;
    _deferred_prefs_save_ms = millis() + 5000;
    return false;
  }
  dirty = false;
  return true;
}

void UITask::requestPrefsSave() {
  _deferred_prefs_save = true;
  _deferred_prefs_attempts = 0;
  _deferred_prefs_save_ms = millis() + 1000;
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){
  // Settings normally commit on Back. A shutdown is also an exit, and may be
  // initiated asynchronously by the battery guard while that screen is open.
  if (settings) ((SettingsScreen*)settings)->prepareForShutdown();
  the_mesh.savePrefs();
  the_mesh.flushDirtyContacts();

  // Auto-save the live GPS trail before power-off when the user enabled it
  // (Tools › Trail › Settings › Auto-save). This covers the low-battery
  // auto-shutdown, which otherwise loses the whole route. Overwrites /trail
  // (same file as the manual Trail › Save); guarded on count()>0 so an empty
  // trail can't wipe a previously saved one.

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - buzzer_timer) < 2500)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _power.restrictions().shutting_down = true;
    reconcilePower();
    radio_driver.powerOff();
    // Power GPS down through its provider before SYSTEMOFF — GPIO pins retain
    // state in NRF52 SYSTEMOFF, so otherwise the module keeps draining the
    // battery. The provider handles the enable + reset pins and the correct
    // active level. gps_enabled is persisted; applyGpsPrefs() restores it on
    // the next boot.
    if (_sensors) {
      LocationProvider* loc = _sensors->getLocationProvider();
      if (loc) loc->stop();
    }
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::enqueueKey(char c, bool cardkb) {
  if (c == 0) return;
  uint8_t next = (_kq_head + 1) % KEY_QUEUE_SIZE;
  if (next == _kq_tail) return;  // full: drop newest rather than clobber unprocessed keys
  _key_queue[_kq_head] = { c, cardkb };
  _kq_head = next;
}

bool UITask::dequeueKey(char& c) {
  if (_kq_tail == _kq_head) return false;
  c = _key_queue[_kq_tail].key;
  _kq_tail = (_kq_tail + 1) % KEY_QUEUE_SIZE;
  return true;
}

void UITask::discardCardKBKeys() {
  QueuedKey kept[KEY_QUEUE_SIZE];
  uint8_t count = 0;
  while (_kq_tail != _kq_head) {
    QueuedKey event = _key_queue[_kq_tail];
    _kq_tail = (_kq_tail + 1) % KEY_QUEUE_SIZE;
    if (!event.cardkb) kept[count++] = event;
  }
  _kq_head = _kq_tail = 0;
  for (uint8_t i = 0; i < count; i++) {
    _key_queue[_kq_head] = kept[i];
    _kq_head = (_kq_head + 1) % KEY_QUEUE_SIZE;
  }
}

void UITask::turnDisplayOn() {
  if (!_display) return;
  setDisplayRequested(true);
}

void UITask::turnDisplayOff() {
  if (!_display) return;
  setDisplayRequested(false);
}

// Poll an optional CardKB (I2C keyboard, addr 0x5F) on Wire1/Grove, feeding
// the same key queue as every physical button. Most of its output needs no
// translation at all: CardKB's own arrow/Enter/Esc byte codes are already
// identical to this UI's KEY_LEFT/UP/DOWN/RIGHT/ENTER/CANCEL (0xB4-0xB7, 13,
// 27), and Backspace (0x08) / printable ASCII (0x20-0x7E) collide with
// nothing that existed before. Plain Enter/arrows act like the physical
// centre button/joystick (grid commit/navigate) -- except in Compact mode's
// plain grid state (see below), which is designed to need no joystick at all.
// Tab (0x09, otherwise unused) is the Hold-Enter equivalent everywhere,
// including the non-keyboard Hold-Enter menus and inside the on-screen
// keyboard itself (shift-lock, clear-all and emoji picker) -- it used to
// need a separate Fn+Tab for the latter, but that was pure redundancy: plain
// Tab already covered every case Fn+Tab did, just not while the keyboard was
// showing, so the carve-out was dropped instead of the shortcut. In Compact
// mode's plain grid state Tab means something more useful instead (opens the
// placeholder picker directly -- see below). Fn still gives two other clean,
// stateless modifiers:
//  - Fn+Enter (0xA3) submits the field (KEY_KB_ENTER) without needing to
//    navigate to the special row's DONE cell. The placeholder and emoji
//    popups are modal and consume it first (dismiss them with Enter/Esc), same as
//    they consume every other key.
//  - Fn+M opens the message emoji picker.
// Transport, debounce, Fn decoding and suspend/resume state live in
// CardKBController; this function only applies UI-context-specific behaviour.
void UITask::pollCardKB() {
#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  // The Tracker controls wake the display. Suspending CardKB I2C traffic while
  // it is off avoids a permanent accessory-input cost during normal idle time.
  if (!_display || !_display->isOn()) return;

  CardKBController::Event event;
  if (!_cardkb.poll(event)) return;
  // A connected CardKB automatically hides the letter grid and guarantees
  // joystick-free operation: while the compact grid is
  // the active surface (KeyboardWidget::inPlainGridState() -- showing, no
  // popup open, not already mid cursor-move) arrows drive the text cursor
  // directly instead of a grid selection nobody could see anyway, and plain
  // Tab opens the placeholder picker directly instead of the row/col-dependent
  // Hold-Enter dispatch (which would be meaningless here -- row/col are never
  // deliberately navigated to in this mode). Cursor mode and the placeholder
  // and emoji popups all render their own visible feedback regardless of
  // Compact, so none of this applies once inPlainGridState() is false --
  // arrows/Tab fall through to their normal meaning there (e.g. arrows drive
  // the active popup's own selection).
  bool compact_grid = isCardKBConnected() && _kb.inPlainGridState();

  cardkbinput::Translation input = cardkbinput::translate(event, compact_grid);
  if (input.kind == cardkbinput::MOVE_CURSOR) {
    char woke = checkDisplayOn(input.key);
    if (woke) _kb.moveCursorDirect(input.key);
    return;
  }
  if (input.kind == cardkbinput::OPEN_SUGGESTIONS) {
    if (_kb.openPlaceholders()) checkDisplayOn(KEY_CONTEXT_MENU);
    return;
  }
  if (input.kind == cardkbinput::OPEN_EMOJI) {
    char woke = checkDisplayOn(KEY_CONTEXT_MENU);
    if (woke) _kb.openEmojiPicker();
    return;
  }
  if (input.kind == cardkbinput::FORWARD_KEY)
    enqueueKey(checkDisplayOn(input.key), true);
#endif
}

void UITask::loop() {
  observeBluetoothState();
  if (_emergency_window.active() && !_emergency_window.update(millis()))
    setEmergencyMode(false);
  tickBootTimeSync();
  if (home) ((HomeScreen*)home)->tickSensor();
  zen::NodeLoginCoordinator::Attempt login_timeout;
  if (_remote_node.expired(zen::RemoteNodeCoordinator::LOGIN_OWNER, millis()) &&
      _node_login.take(login_timeout)) {
    the_mesh.cancelUiPendingLogin(login_timeout.pub_key);
    if (retryNodeLogin(login_timeout)) {
      _next_refresh = 0;
    } else {
#if ZEN_FEATURE_ADMIN
      if (login_timeout.owner == zen::NodeLoginCoordinator::ADMIN)
        ((AdminScreen*)admin_screen)->onNodeLoginTimeout(login_timeout.pub_key);
      else
#endif
      if (login_timeout.owner == zen::NodeLoginCoordinator::SENSOR) {
        ((HomeScreen*)home)->onSensorLoginTimeout(login_timeout.pub_key);
      } else if (login_timeout.owner == zen::NodeLoginCoordinator::MESSAGES
          && login_timeout.password[0] == '\0') {
        // A blank room credential means "authenticate from the server ACL".
        // Room servers silently discard unauthorised anonymous requests, so no
        // response cannot prove that the credential itself was wrong. Let the
        // user enter provisionally after the normal reply window; the server's
        // ACL remains authoritative for every message and cannot be bypassed by
        // this local UI state.
        _node_login.markLoggedIn(login_timeout.pub_key);
        ((MessagesScreen*)messages_screen)->onNodeLoginResult(login_timeout.pub_key, true, 0);
      } else {
        ((MessagesScreen*)messages_screen)->onNodeLoginTimeout(login_timeout.pub_key);
      }
      _next_refresh = 0;
    }
  }
  // Background delivery: resend pending on-device DMs whose ACK timed out, and
  // finalise the ✗ marker — runs regardless of which screen is active.
  if (!isLowPowerMode() || isEmergencyMode())
    if (((MessagesScreen*)messages_screen)->tickDmResends())
      operationError(zen::Operation::MESSAGE, zen::OperationOutcome::TIMEOUT,
                     zen::OperationReason::NO_ACK, nullptr,
                     zen::RESULT_BACKGROUND);
#if UI_HAS_JOYSTICK
  uint8_t joy_rot = _node_prefs ? _node_prefs->joystick_rotation : JOYSTICK_ROTATION;
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    enqueueKey(checkDisplayOn(KEY_ENTER, false));
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    enqueueKey(handleLongPress(KEY_ENTER, false));  // REVISIT: could be mapped to different key code
  }
  // Drain each direction fully: a burst of taps captured during a blocking
  // refresh replays as several CLICKs, queued here and applied before one
  // redraw (see enqueueKey / the dispatch at the end of loop()).
#if UI_HAS_JOYSTICK_UPDOWN
  while (joystick_up.check() == BUTTON_EVENT_CLICK)
    enqueueKey(checkDisplayOn(rotateJoystickKey(KEY_UP, joy_rot), false));
  while (joystick_down.check() == BUTTON_EVENT_CLICK)
    enqueueKey(checkDisplayOn(rotateJoystickKey(KEY_DOWN, joy_rot), false));
#endif
  while ((ev = joystick_left.check()) != BUTTON_EVENT_NONE) {
    if (ev == BUTTON_EVENT_CLICK) enqueueKey(checkDisplayOn(rotateJoystickKey(KEY_LEFT, joy_rot), false));
    else { if (ev == BUTTON_EVENT_LONG_PRESS) enqueueKey(handleLongPress(rotateJoystickKey(KEY_LEFT, joy_rot), false)); break; }
  }
  while ((ev = joystick_right.check()) != BUTTON_EVENT_NONE) {
    if (ev == BUTTON_EVENT_CLICK) enqueueKey(checkDisplayOn(rotateJoystickKey(KEY_RIGHT, joy_rot), false));
    else { if (ev == BUTTON_EVENT_LONG_PRESS) enqueueKey(handleLongPress(rotateJoystickKey(KEY_RIGHT, joy_rot), false)); break; }
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    enqueueKey(checkDisplayOn(KEY_CANCEL));
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    enqueueKey(checkDisplayOn(KEY_DOUBLE_CANCEL));
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    enqueueKey(handleTripleClick(KEY_SELECT));
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    enqueueKey(checkDisplayOn(KEY_NEXT));
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    enqueueKey(handleLongPress(KEY_ENTER));
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    enqueueKey(handleDoubleClick(KEY_PREV));
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    enqueueKey(handleTripleClick(KEY_SELECT));
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (millis() - _analogue_pin_read_millis > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      enqueueKey(checkDisplayOn(KEY_NEXT));
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      enqueueKey(handleLongPress(KEY_ENTER));
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      enqueueKey(handleDoubleClick(KEY_PREV));
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      enqueueKey(handleTripleClick(KEY_SELECT));
    }
    _analogue_pin_read_millis = millis();
  }
#endif
  pollCardKB();
#if defined(CARDKB_ADDRESS) && ZEN_FEATURE_CARDKB
  if (_cardkb_was_present && !_cardkb.isPresent()) {
    operationWarning(zen::Operation::CARDKB,
                     zen::OperationReason::DISCONNECTED);
    _cardkb_was_present = false;
    reconcilePower();
  }
#endif
  // Presence can be cleared after repeated I2C failures. Mirror it every loop
  // so the full virtual keyboard returns automatically if CardKB disconnects.
  _kb.setExternalKeyboardConnected(isCardKBConnected());
#if defined(BACKLIGHT_BTN)
  if ((int32_t)(millis() - next_backlight_btn_check) >= 0) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (_kq_head != _kq_tail) {
    if (curr) {
      // Apply the whole queued burst, then redraw once — N taps captured during
      // a blocking refresh become N navigation steps at the cost of one refresh.
      char k;
      while (dequeueKey(k)) curr->handleInput(k);
      { uint32_t aoff = autoOffMillis(); if (aoff > 0) _auto_off = millis() + aoff; }  // extend auto-off timer
      // Note timing no longer depends on render cadence (TIMER1 IRQ advances
      // notes directly — see buzzer.cpp), so a redraw right after a keypress
      // can't clip a note; no need to hold it back while buzzer.isPlaying().
      _next_refresh = 100;  // trigger refresh immediately
    } else _kq_head = _kq_tail = 0;
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  // Shortcut settings should feel immediate. Persist after the alert has had
  // time to render; repeated toggles inside the window collapse to one write.
  if (_deferred_prefs_save
      && (int32_t)(millis() - _deferred_prefs_save_ms) >= 0) {
    if (the_mesh.savePrefs()) {
      _deferred_prefs_save = false;
      _deferred_prefs_attempts = 0;
    } else if (++_deferred_prefs_attempts < 3) {
      static const uint32_t RETRY_MS[2] = {30000UL, 120000UL};
      _deferred_prefs_save_ms = millis() + RETRY_MS[_deferred_prefs_attempts - 1];
    } else {
      // Avoid an endless flash-write loop. A later edit or orderly shutdown
      // will try the complete current preference state again.
      _deferred_prefs_save = false;
    }
  }

  if (curr) curr->poll();

  // Alarm + countdown run regardless of the current screen / display state, so
  // they're driven here (not via the current screen's poll()).

  if (_display != NULL && _display->isOn()) {
    uint32_t frame_now = millis();
    if (zen::TimeDeadline::due(frame_now, _next_refresh) && curr) {
      _display->startFrame();
      _display->beginMarqueeFrame();
      _kb.beginFrame();
      int delay_millis = curr->render(*_display);
      int marquee_delay = _display->marqueeDelay();
      if (marquee_delay > 0 && (delay_millis <= 0 || marquee_delay < delay_millis))
        delay_millis = marquee_delay;
      // Skip the alert overlay (new-message toast) while the keyboard is the
      // thing actually on screen this frame -- it's shared across Messages/
      // Bot/Settings/Admin/etc., so this covers every screen that uses it for
      // full-screen text entry, not just message compose. Otherwise a message
      // arriving mid-typing blanks out the letter grid for 3s with no way to
      // see what's being typed.
      frame_now = millis();
      if (zen::TimeDeadline::active(frame_now, _alert_expiry) && !_kb.isVisible()) {
        renderAlertOverlay();
        // Keep refreshing the underlying screen at its own cadence (capped at the
        // alert's expiry) so layouts that settle over a frame — e.g. the message-
        // history scrollbar reserve — don't stay stuck behind the alert. Unchanged
        // frames are skipped by the display CRC, so e-ink isn't thrashed.
        _next_refresh = frame_now + delay_millis;
        if (zen::TimeDeadline::after(_next_refresh, _alert_expiry)) _next_refresh = _alert_expiry;
      } else {
        _next_refresh = frame_now + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered() && !_notification_wake.active()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
    if ((_notification_wake.active() || autoOffMillis() > 0) &&
        (int32_t)(millis() - _auto_off) >= 0) {
      turnDisplayOff();
#ifdef PIN_LED
      digitalWrite(PIN_LED, LOW);  // turn off status LED with display to save power
#endif
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

  if ((int32_t)(millis() - next_batt_chck) >= 0) {
    uint16_t raw = AbstractUITask::getBattMilliVolts();
    if (raw > 0) {
      // EMA filter: alpha=0.2 (80% old, 20% new) — smooths ADC noise from uneven load
      _batt_mv = (_batt_mv == 0) ? raw : (uint16_t)((_batt_mv * 4u + raw) / 5u);
    }
    bool external_power = board.isExternalPowered();
    _battery_runtime.update(millis(), _batt_mv, external_power, isEmergencyMode());
    // Don't shut down while on external power (charging) — avoids a shutdown loop.
    if (zen::BatteryPolicy::shouldShutdown(_batt_mv, external_power)) {
      if (_display != NULL) {
        _display->startFrame();
        _display->setTextSize(1);
        _display->setColor(ZenDisplayDriver::LIGHT);
        int mid = _display->height() / 2;
        int step = _display->lineStep();
        _display->drawTextCentered(_display->width() / 2, mid - step, "Low Battery");
        _display->drawTextCentered(_display->width() / 2, mid, "Shutting down");
        _display->endFrame();
        if (_display->isEink() == false) { delay(2000); }
      }
      shutdown();
    }
    bool low_power = _low_power_latch.update(_batt_mv, external_power);
    if (low_power != isLowPowerMode()) setLowPowerMode(low_power);
    if (_low_battery_reminder.due(millis(), _batt_mv, external_power))
      notifyLowBattery();
    next_batt_chck = millis() + 8000;
  }

}

// CONTRACT: every ZenPrefs field that keys on a contact pubkey/prefix is
// cleared here, so a removed contact can't leave a dangling reference. If you
// add such a field, add its cleanup below (and mark the field in ZenPrefs.h).
// Currently covered: favourite_contacts, dm_notif[], dm_melody[]. Also clears
// MessageUnreadCoordinator (RAM-only, not a
// ZenPrefs field, so no savePrefs() needed for it) -- same 4-byte-prefix
// shape and same 16-slot starvation risk as dm_notif/dm_melody above. Called
// for both explicit removal and silent auto-eviction (see MyMesh
// CMD_REMOVE_CONTACT / onContactOverwrite).
void UITask::onContactRemoved(const uint8_t* pub_key) {
  if (!_node_prefs || !pub_key) return;
  bool changed = false;

  forgetDMContact(pub_key);

  int slot = findFavouriteSlot(pub_key);
  if (slot >= 0) { clearFavouriteSlot(slot); changed = true; }

  // Per-contact mute/melody overrides — only 16 slots shared across every
  // contact, so an orphaned entry isn't just stale, it can eventually starve
  // new overrides for contacts that still exist. Keyed by a 4-byte prefix
  // (narrower than the 6-byte one above), so compare only that many bytes.
  changed |= zen::NotificationPreferences::removeContact(_node_prefs, pub_key);

  if (changed) the_mesh.savePrefs();
}

// CONTRACT: every ZenPrefs field that keys on a channel index is cleared here,
// so a channel re-added at a freed slot can't inherit the old one's settings.
// If you add such a field, add its cleanup below (and mark it in ZenPrefs.h).
// Currently covered: ch_notif_melody_*, ch_notif_override/ch_notif_muted and
// ch_fav_bitmask.
void UITask::onChannelRemoved(uint8_t channel_idx) {
  if (!_node_prefs) return;
  bool changed = false;

  uint64_t mask = 1ULL << channel_idx;
  changed |= zen::NotificationPreferences::removeChannel(_node_prefs, channel_idx);
  if (_node_prefs->ch_fav_bitmask & mask) {
    _node_prefs->ch_fav_bitmask &= ~mask;
    changed = true;
  }

  if (changed) the_mesh.savePrefs();
}

bool UITask::currentLocation(int32_t& lat, int32_t& lon) const {
  LocationProvider* loc = _sensors ? _sensors->getLocationProvider() : nullptr;
  if (loc && loc->isValid()) {
    lat = (int32_t)loc->getLatitude();
    lon = (int32_t)loc->getLongitude();
    return true;
  }
  return false;
}

char UITask::checkDisplayOn(char c, bool allow_wake) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      // Joystick directions and Enter still generate short GPIO interrupts so
      // their click state remains correct, but they must not light the panel or
      // leak an action into the selected screen. Back is routed here with
      // allow_wake=true; notification/alarm wake uses turnDisplayOn() directly.
      if (!allow_wake) return 0;
      turnDisplayOn();
      the_mesh.onUserDisplayWake();
      _time_location.onUserWake(_gps);
#ifdef PIN_LED
      digitalWrite(PIN_LED, LOW);  // ensure LED is off when waking display (userLedHandler takes over)
#endif
      c = 0;
    }
    // Any physical interaction takes ownership of a notification-only wake
    // and restores the user's normal display timeout.
    _notification_wake.interaction();
    uint32_t aoff = autoOffMillis();
    if (aoff > 0) _auto_off = millis() + aoff;  // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c, bool allow_wake) {
  // Same checkDisplayOn() gate every other input path goes through (see
  // pollCardKB()'s Fn+letter handling for the same shape) -- without it, a long
  // press while the display is off neither wakes it nor extends auto-off, and
  // it delivers KEY_CONTEXT_MENU to the invisible screen (found already open
  // at the next wake instead of the press being consumed as a wake).
  c = checkDisplayOn(c, allow_wake);
  if (c == 0) return 0;
  if (millis() - ui_started_at < 8000 &&
      zen::Policy::recoveryAllowed(isChildModeLocked())) {   // startup long press -> CLI/rescue
    the_mesh.enterCLIRescue();
    return 0;
  }
  if (c == KEY_ENTER) return KEY_CONTEXT_MENU;
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  checkDisplayOn(c);
  _dnd_active = !_dnd_active;
  showAlert(_dnd_active ? "Silent On" : "Silent Off", 800);
  // Use the same short feedback as Advert, Bluetooth and GPS. Do not imply
  // that sound is back when Mode, Auto or Quiet Time still mutes it.
  if (!_dnd_active) notifyHomeAction();
  _next_refresh = 0;
  return 0;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

uint8_t UITask::getGPSMode() const {
  if (isLowPowerMode()) return _power.requests().emergency_gps ? 1 : 0;
  if (!_node_prefs) return 0;
  return zen::GpsMode::fromPrefs(_node_prefs->gps_enabled != 0,
                                  _node_prefs->gps_interval,
                                  _node_prefs->gps_adaptive != 0);
}

void UITask::setGPSMode(uint8_t mode) {
  if (_sensors == NULL || _node_prefs == NULL || mode >= zen::GpsMode::COUNT) {
    operationError(zen::Operation::GPS, zen::OperationOutcome::NOT_FOUND,
                   zen::OperationReason::SETTING_UNAVAILABLE);
    return;
  }

  _node_prefs->gps_enabled = mode == 0 ? 0 : 1;
  _node_prefs->gps_interval = zen::GpsMode::interval(mode);
  _node_prefs->gps_adaptive = zen::GpsMode::isAdaptive(mode);

  applyGpsPrefs();
  if (!the_mesh.savePrefs()) return;
  notify(UIEventType::ack);

  char alert[32];
  snprintf(alert, sizeof(alert), "GPS: %s", zen::GpsMode::label(mode));
  showAlert(alert, 900);
  _next_refresh = 0;
}

void UITask::applyGpsPrefs() {
  if (_sensors == NULL || _node_prefs == NULL) return;
  _power.requests().gps_session = zen::PeripheralPowerCoordinator::NO_OVERRIDE;
  // Cadence/adaptive changes do not alter the boolean power target, so force
  // one reconciliation to apply the complete GPS configuration atomically.
  reconcilePower(true);
  _next_refresh = 0;
}

void UITask::applyBluetoothPrefs() {
  if (!_node_prefs) return;
  _power.requests().bluetooth_session = zen::PeripheralPowerCoordinator::NO_OVERRIDE;
  reconcilePower();
  _next_refresh = 0;
}

void UITask::enableBluetooth() {
  _power.requests().bluetooth_session = 1;
  reconcilePower();
}

void UITask::disableBluetooth() {
  _power.requests().bluetooth_session = 0;
  reconcilePower();
}

bool UITask::hasGPS() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) return true;
    }
  }
  return false;
}

void UITask::toggleGPS() {
  if (isLowPowerMode()) {
    if (!isEmergencyMode()) {
      operationWarning(zen::Operation::GPS,
                       zen::OperationReason::EMERGENCY_REQUIRED);
      return;
    }
    _power.requests().emergency_gps = !_power.requests().emergency_gps;
    reconcilePower();
    showAlert(_power.requests().emergency_gps ? "GPS: On" : "GPS: Off", 900);
    _next_refresh = 0;
    return;
  }
  if (!_sensors) {
    operationError(zen::Operation::GPS, zen::OperationOutcome::NOT_FOUND,
                   zen::OperationReason::SETTING_UNAVAILABLE);
    return;
  }

  // The home-page toggle is intentionally session-only. Persistent GPS power
  // and polling changes are staged and written exclusively by Settings.
  bool enable = !getGPSState();
  _power.requests().gps_session = enable ? 1 : 0;
  reconcilePower();
  if (getGPSState() != enable) {
    operationError(zen::Operation::GPS, zen::OperationOutcome::TRANSPORT_FAILURE,
                   zen::OperationReason::APPLY_FAILED);
    return;
  }
  notifyHomeAction();
  showAlert(enable ? "GPS: On" : "GPS: Off", 900);
  _next_refresh = 0;
}


// Set a user GPIO pin to a specific mode (0=Off 1=In 2=Out-low 3=Out-high
// 4=Analog), apply it to the actual pin, and persist. The Off->In->Out->...
// cycling itself lives in GpioScreen; the bot's !gpioN on/off and boot
// restore also route through here.

void UITask::applyTxPower() {
  if (_node_prefs == NULL || isLowPowerMode()) return;
  radio_driver.setTxPower(_node_prefs->tx_power_dbm);
}

void UITask::applyRadioParams() {
  if (_node_prefs == NULL || isLowPowerMode()) return;
  the_mesh.applyRadioParams();
}

void UITask::applyBrightness() {
  reconcilePower();
}

void UITask::applyRotation() {
  if (_display != NULL && _node_prefs != NULL) {
    _display->setDisplayRotation(_node_prefs->display_rotation);
    _next_refresh = 0;
  }
}

void UITask::applyFullRefreshInterval() {
  if (_display != NULL && _node_prefs != NULL) {
    static const uint8_t OPTS[] = { 0, 5, 10, 20, 30 };
    static const int OPTS_COUNT = 5;
    uint8_t idx = _node_prefs->eink_full_refresh_every;
    if (idx >= OPTS_COUNT) idx = 0;
    _display->setFullRefreshInterval(OPTS[idx]);
  }
}

void UITask::onPrefsRestored() {
  applyRadioParams();
  applyTxPower();
  applyGpsPrefs();
  applyBluetoothPrefs();
  applyBrightness();
  applyRotation();
  applyFullRefreshInterval();
  _next_refresh = 0;
}

void UITask::onPrefsCommitted(uint16_t effects) {
  // Runtime state is derived only after the complete MeshCore-compatible
  // preference record is durable. Apply only affected peripherals: retuning
  // the radio or restarting GPS/Bluetooth for an unrelated display edit is
  // both disruptive and wasteful.
  if (effects & PREFS_APPLY_GPS) applyGpsPrefs();
  if (effects & PREFS_APPLY_BLUETOOTH) applyBluetoothPrefs();
  if (effects & PREFS_APPLY_BRIGHTNESS) applyBrightness();
  if (effects & PREFS_APPLY_ROTATION) applyRotation();
  if (effects & PREFS_APPLY_EINK) applyFullRefreshInterval();
  if ((effects & PREFS_APPLY_BUZZER) && _node_prefs)
    buzzer.setVolume(_node_prefs->buzzer_volume);
  _next_refresh = 0;
}

void UITask::setBrightnessLevel(uint8_t level) {
  if (_node_prefs == NULL) return;
  if (level > 4) level = 4;
  _node_prefs->display_brightness = level;
  applyBrightness();
  _next_refresh = 0;
}

void UITask::setBuzzerVolumeLevel(uint8_t level) {
#ifdef PIN_BUZZER
  if (_node_prefs == NULL) return;
  if (level > 4) level = 4;
  _node_prefs->buzzer_volume = level;
  buzzer.setVolume(level);
  if (level > 0) buzzer.playForced("Vol:d=16,o=6,b=120:c");
  _next_refresh = 0;
#endif
}

int UITask::getNotificationMode() const {
  if (!_node_prefs) return zen::NotificationPolicy::ON;
  return zen::NotificationPolicy::mode(_node_prefs->buzzer_quiet != 0,
                                        _node_prefs->buzzer_auto != 0);
}

void UITask::cycleNotificationMode(int direction) {
  if (!_node_prefs) return;
  int mode = getNotificationMode();
  mode = wrapSelection(mode, 3, direction);
  _node_prefs->buzzer_auto = (mode == 2) ? 1 : 0;
  _node_prefs->buzzer_quiet = mode == 1 ? 1 : 0;
  _next_refresh = 0;
}
