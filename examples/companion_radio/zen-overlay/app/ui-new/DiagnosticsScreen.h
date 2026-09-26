#pragma once
#include <helpers/ui/ZenDisplayDriver.h>
#include <helpers/ui/ZenUIScreen.h>
#include <helpers/DeviceDiag.h>
#include <Arduino.h>
#include <stdlib.h>
#include <stdarg.h>
#include "icons.h"
#include "FullscreenMsgView.h"
#include "PopupMenu.h"
#include "TabBar.h"
#include "../MyMesh.h"

extern MyMesh the_mesh;

// Device diagnostics, organised as a circular tab carousel (shared TabBar.h,
// same idiom as BotScreen/NearbyScreen). LEFT/RIGHT switches tabs; UP/DOWN
// scrolls within the active tab:
//   Live   — baseline MeshCore packet totals, heap/stack headroom and radio
//            signal. Hold Enter resets the baseline counters.
//   System — static device identity: firmware version + build date, device
//            model, node name, and the active radio parameters.
//   Font   — a rendering test card: one sample line per script the UI font
//            claims to cover (Latin, Greek, Cyrillic, digits,
//            symbols), so the on-device font can be eyeballed for coverage.
// Zen deliberately consumes only diagnostics exposed by the baseline
// Dispatcher. It does not replace forwarding to collect additional metrics.
class DiagnosticsScreen : public ZenUIScreen {
  UITask* _task;
  int _scroll = 0;
  int _event_sel = 0;
  uint8_t _tab = 0;        // persists across visits (like BotScreen's _tab)
  PopupMenu _reset_menu;   // Live tab, Hold Enter → 1-item "Reset counters" action menu (Back dismisses)
  FullscreenMsgView _event_view;
  char _event_title[16]{};
  char _event_detail[128]{};
  MyMesh::StorageStatus _internal_storage{};
  MyMesh::StorageStatus _contacts_storage{};

  enum Tab : uint8_t {
    TAB_LIVE, TAB_EVENTS, TAB_TRANSPORT, TAB_BATTERY, TAB_LOCATION, TAB_POWER, TAB_SYSTEM,
    TAB_FONT, TAB_COUNT
  };
  static const char* const TAB_LABELS[TAB_COUNT];

  struct Row { const char* label; char value[20]; };
  static const int MAX_ROWS = 14;
  Row _rows[MAX_ROWS];
  int _row_count = 0;

  // Full-width text lines (System / Font tabs) — long values (device model,
  // node name) don't fit the Live tab's narrow right-aligned value column, so
  // these tabs draw one ellipsized line each instead of a label/value pair.
  static const int MAX_LINES = 16;
  char _lines[MAX_LINES][40];
  int _line_count = 0;

  void addRow(const char* label, const char* value) {
    if (_row_count >= MAX_ROWS) return;
    _rows[_row_count].label = label;
    strncpy(_rows[_row_count].value, value, sizeof(_rows[_row_count].value) - 1);
    _rows[_row_count].value[sizeof(_rows[_row_count].value) - 1] = 0;
    _row_count++;
  }
  // "12345/12345" rather than "rx12345 tx12345" — at 5-digit counts (a busy
  // repeater after weeks of uptime) the longer form collides with the value
  // column on a 128px OLED when paired with a long label like "Ack/Path".
  // The "Total rx/tx" label spells out the rx/tx order once for every row below it.
  void addRxTxRow(const char* label, uint32_t rx, uint32_t tx) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%lu/%lu", (unsigned long)rx, (unsigned long)tx);
    addRow(label, buf);
  }

  void addLine(const char* fmt, ...) {
    if (_line_count >= MAX_LINES) return;
    va_list ap; va_start(ap, fmt);
    vsnprintf(_lines[_line_count], sizeof(_lines[_line_count]), fmt, ap);
    va_end(ap);
    _line_count++;
  }

  void buildLiveRows() {
    _row_count = 0;

    uint32_t up_secs = millis() / 1000;
    char buf[20];
    uint32_t d = up_secs / 86400, h = (up_secs % 86400) / 3600, m = (up_secs % 3600) / 60, s = up_secs % 60;
    if (d > 0) snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", (unsigned long)d, (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else       snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    addRow("Uptime", buf);

    addRxTxRow("Total rx/tx",
               the_mesh.getNumRecvFlood() + the_mesh.getNumRecvDirect(),
               the_mesh.getNumSentFlood() + the_mesh.getNumSentDirect());
    addRxTxRow("Flood", the_mesh.getNumRecvFlood(), the_mesh.getNumSentFlood());
    addRxTxRow("Direct", the_mesh.getNumRecvDirect(), the_mesh.getNumSentDirect());

    uint32_t heap_free, heap_total;
    DeviceDiag::getHeapStats(heap_free, heap_total);
    if (heap_total > 0) snprintf(buf, sizeof(buf), "%lu/%luKB", (unsigned long)(heap_free / 1024), (unsigned long)(heap_total / 1024));
    else                strcpy(buf, "N/A");
    addRow("Heap free", buf);

    uint32_t stack_free = DeviceDiag::getStackFreeBytes();
    snprintf(buf, sizeof(buf), "%luB", (unsigned long)stack_free);
    addRow("Stack free", buf);

    snprintf(buf, sizeof(buf), "%d dBm", (int)radio_driver.getNoiseFloor());
    addRow("Noise floor", buf);
    snprintf(buf, sizeof(buf), "%d/%.1f", (int)radio_driver.getLastRSSI(), radio_driver.getLastSNR());
    addRow("RSSI/SNR", buf);

  }

  void buildSystemLines() {
    _line_count = 0;

    // Firmware version, minus the commit-hash suffix build.sh appends as the
    // LAST dash-segment (v1.16-solo.0-abcdef -> v1.16-solo.0) — same strip the
    // boot screen does; the last dash, not the first, since a tag like
    // v1.21-rc1 has a dash of its own before the hash.
    const char* ver = FIRMWARE_VERSION;
    const char* dash = strrchr(ver, '-');
    int plen = dash ? (int)(dash - ver) : (int)strlen(ver);
    char sv[24];
    if (plen >= (int)sizeof(sv)) plen = sizeof(sv) - 1;
    memcpy(sv, ver, plen); sv[plen] = '\0';

    addLine("FW %s", sv);
    addLine("Built %s", FIRMWARE_BUILD_DATE);
    addLine("Dev %s", board.getManufacturerName());
    addLine("Node %s", the_mesh.getNodeName());
    char identity[9]{};
    the_mesh.getPublicKeyPrefix(identity, sizeof(identity));
    addLine("Identity %s", identity);
    addLine("ID boot %s", the_mesh.identityLoadedAtBoot() ? "Loaded" : "Generated");

    ZenPrefs* p = the_mesh.getNodePrefs();
    if (p) {
      addLine("Freq %.3f MHz", p->freq);
      addLine("SF%u BW%.0f CR%u", (unsigned)p->sf, p->bw, (unsigned)p->cr);
      addLine("TX %d dBm", (int)p->tx_power_dbm);
    }
    const char* scope = the_mesh.getDefaultFloodScopeName();
    addLine("Scope default: %.30s", the_mesh.hasDefaultFloodScope() ?
            (scope[0] ? scope : "Key set") : "None");
    MyMesh::FloodScopeState scope_state = the_mesh.getFloodScopeState();
    addLine("Msg flood: %s", scope_state == zen::FloodScopeView::APP_OVERRIDE ? "App override" :
                             scope_state == zen::FloodScopeView::APP_UNSCOPED ? "App unscoped" :
                             scope_state == zen::FloodScopeView::DEFAULT ? "Default" : "Unscoped");
    addLine("Advert flood: %s", the_mesh.hasDefaultFloodScope() ? "Default" : "Unscoped");
    if (_internal_storage.available)
      addLine("Internal: %lu/%luKB%s", (unsigned long)_internal_storage.used_kb,
              (unsigned long)_internal_storage.total_kb,
              _internal_storage.low_space ? " LOW" : "");
    else addLine("Internal: Unavailable");
    if (_contacts_storage.available)
      addLine("Contacts: %lu/%luKB%s", (unsigned long)_contacts_storage.used_kb,
              (unsigned long)_contacts_storage.total_kb,
              _contacts_storage.low_space ? " LOW" : "");
    else addLine("Contacts: Unavailable");
    addLine("Back up via companion app");
  }

  void buildEventLines() {
    _line_count = 0;
    const zen::DiagnosticLog& log = _task->diagnosticLog();
    if (!log.size()) { addLine("No events"); return; }
    for (uint8_t i = 0; i < log.size() && _line_count < MAX_LINES; i++) {
      const zen::DiagnosticLog::Entry* event = log.newest(i);
      if (!event) continue;
      char time[6] = "--:--";
      if (event->timestamp) {
        int32_t local = (int32_t)(event->timestamp % 86400UL) +
            (int32_t)_task->localOffsetMinutes(event->timestamp) * 60;
        while (local < 0) local += 86400;
        local %= 86400;
        snprintf(time, sizeof(time), "%02ld:%02ld", (long)(local / 3600),
                 (long)((local / 60) % 60));
      }
      if (event->count > 1)
        addLine("%c %s %s: %s x%u",
                zen::DiagnosticLog::severity(event->result) == zen::DiagnosticLog::ERROR ? 'E' :
                (zen::DiagnosticLog::severity(event->result) == zen::DiagnosticLog::WARNING ? 'W' : 'I'),
                time, zen::OperationResultCatalog::operationName(event->result.operation),
                zen::OperationResultCatalog::status(event->result),
                (unsigned)event->count);
      else
        addLine("%c %s %s: %s",
                zen::DiagnosticLog::severity(event->result) == zen::DiagnosticLog::ERROR ? 'E' :
                (zen::DiagnosticLog::severity(event->result) == zen::DiagnosticLog::WARNING ? 'W' : 'I'),
                time, zen::OperationResultCatalog::operationName(event->result.operation),
                zen::OperationResultCatalog::status(event->result));
    }
  }

  void buildTransportLines() {
    _line_count = 0;
    const zen::TransportTrace& trace = the_mesh.transportTrace();
    if (!trace.size()) { addLine("No transport events"); return; }
    for (uint8_t i = 0; i < trace.size() && _line_count < MAX_LINES; i++) {
      const zen::TransportTrace::Entry* entry = trace.newest(i);
      if (!entry) break;
      addLine("%s %02X%02X %c", zen::TransportTrace::name(
          (zen::TransportTrace::Event)entry->event), entry->key[0], entry->key[1],
          entry->detail ? 'F' : 'P');
    }
  }

  void eventTime(const zen::DiagnosticLog::Entry& event, char* out, size_t size) const {
    if (!event.timestamp) { snprintf(out, size, "--:--"); return; }
    int32_t local = (int32_t)(event.timestamp % 86400UL) +
        (int32_t)_task->localOffsetMinutes(event.timestamp) * 60;
    while (local < 0) local += 86400;
    local %= 86400;
    snprintf(out, size, "%02ld:%02ld", (long)(local / 3600),
             (long)((local / 60) % 60));
  }

  void openSelectedEvent() {
    const zen::DiagnosticLog& log = _task->diagnosticLog();
    const zen::DiagnosticLog::Entry* event = log.newest((uint8_t)_event_sel);
    if (!event) return;
    zen::DiagnosticLog::Severity level = zen::DiagnosticLog::severity(event->result);
    const char* severity = level == zen::DiagnosticLog::ERROR ? "Error" :
                           (level == zen::DiagnosticLog::WARNING ? "Warning" : "Info");
    snprintf(_event_title, sizeof(_event_title), "%s", severity);
    char time[6];
    eventTime(*event, time, sizeof(time));
    snprintf(_event_detail, sizeof(_event_detail),
             "Time: %s\nOperation: %s\nReason: %s\nCount: %u",
             time, zen::OperationResultCatalog::operationName(event->result.operation),
             zen::OperationResultCatalog::status(event->result),
             (unsigned)event->count);
    if (event->result.context.value) {
      char detail[24];
      snprintf(detail, sizeof(detail), "\nValue: %d", event->result.context.value);
      strncat(_event_detail, detail, sizeof(_event_detail) - strlen(_event_detail) - 1);
    }
    _event_view.begin(false);
  }

  void buildBatteryRows() {
    _row_count = 0;
    uint16_t mv = _task->getBattMilliVolts();
    char buf[20];
    if (mv) snprintf(buf, sizeof(buf), "%u.%02u V", mv / 1000, (mv % 1000) / 10);
    else strcpy(buf, "Unknown");
    addRow("Voltage", buf);
    if (mv) snprintf(buf, sizeof(buf), "%d%%", zen::BatteryPolicy::percent(mv));
    else strcpy(buf, "Unknown");
    addRow("Percentage", buf);

    switch (_task->batteryRuntimeState()) {
      case zen::BatteryRuntimeEstimator::CHARGING: strcpy(buf, "Charging"); break;
      case zen::BatteryRuntimeEstimator::PAUSED: strcpy(buf, "Paused"); break;
      case zen::BatteryRuntimeEstimator::EMPTY: strcpy(buf, "0 min"); break;
      case zen::BatteryRuntimeEstimator::MODEL:
      case zen::BatteryRuntimeEstimator::ESTIMATE: {
        uint32_t seconds = _task->batteryRuntimeSeconds();
        uint32_t hours = seconds / 3600;
        uint32_t minutes = (seconds % 3600) / 60;
        if (hours >= 48) snprintf(buf, sizeof(buf), "%lud %luh",
                                  (unsigned long)(hours / 24), (unsigned long)(hours % 24));
        else if (hours) snprintf(buf, sizeof(buf), "%luh %lum",
                                 (unsigned long)hours, (unsigned long)minutes);
        else snprintf(buf, sizeof(buf), "%lu min", (unsigned long)minutes);
        break;
      }
      default: strcpy(buf, "Unknown"); break;
    }
    addRow("Remaining", buf);
  }

  void buildPowerRows() {
    _row_count = 0;
    auto effective = _task->effectivePowerState();
    auto applied = _task->appliedPowerState();
    uint16_t pending = _task->pendingPowerChanges();
    addRow("Mode", effective.emergency ? "Emergency" :
                   (effective.low_power ? "Low Power" : "Normal"));
    addRow("Display", applied.display_on ? "On" : "Off");
    addRow("CardKB", applied.cardkb_polling ? "Active" : "Sleeping");
    addRow("GPS", applied.gps_force_on ? "Temporary" :
                  (applied.gps_policy_on ? "Configured" : "Off"));
    addRow("Bluetooth", applied.bluetooth_on ? "On" : "Off");
    addRow("Radio", applied.radio_on ? "On" : "Off");
    char buf[20];
    if (pending) snprintf(buf, sizeof(buf), "Pending %04X", pending);
    else strcpy(buf, "Applied");
    addRow("State", buf);
  }

  void buildLocationRows() {
    _row_count = 0;
    const auto& location = _task->timeLocationStatus();
    char buf[20];
    addRow("Time", location.sync_pending ? "Waiting" : "Synced");
    addRow("Source", zen::TimeLocationCoordinator::sourceName(
        location.sync_source));
    if (!location.sync_pending && location.last_sync_ms) {
      uint32_t age = millis() - location.last_sync_ms;
      snprintf(buf, sizeof(buf), "%lum", (unsigned long)(age / 60000UL));
    } else strcpy(buf, "--");
    addRow("Sync age", buf);
    addRow("GPS use", zen::TimeLocationCoordinator::purposeName(
        location.gps.purpose));
    addRow("Receiver", location.gps.receiver_active ? "On" : "Off");
    addRow("Fix", location.gps.fix_valid ? "Valid" : "None");
    if (location.fix_age_available) {
      if (location.fix_age_ms < 60000UL)
        snprintf(buf, sizeof(buf), "%lus", (unsigned long)(location.fix_age_ms / 1000UL));
      else snprintf(buf, sizeof(buf), "%lum", (unsigned long)(location.fix_age_ms / 60000UL));
    } else strcpy(buf, "--");
    addRow("Fix age", buf);
    snprintf(buf, sizeof(buf), "%ld", location.gps.satellites);
    addRow("Satellites", location.gps.fix_valid ? buf : "--");
    if (location.gps.hdop >= 0)
      snprintf(buf, sizeof(buf), "%ld.%ld", location.gps.hdop / 10,
               labs(location.gps.hdop % 10));
    else strcpy(buf, "--");
    addRow("HDOP", buf);
    addRow("Adaptive", zen::TimeLocationCoordinator::adaptiveName(
        location.gps.adaptive_phase));
    if (!location.gps.available || !location.gps.configured) strcpy(buf, "--");
    else if (location.gps.receiver_active) strcpy(buf, "Now");
    else if (!location.gps.next_acquire_ms) strcpy(buf, "--");
    else {
      int32_t remaining = (int32_t)(location.gps.next_acquire_ms - millis());
      if (remaining <= 0) strcpy(buf, "Due");
      else snprintf(buf, sizeof(buf), "%lum", (unsigned long)((remaining + 59999) / 60000));
    }
    addRow("Next", buf);
    if (location.course_source != zen::GpsCourse::NONE)
      snprintf(buf, sizeof(buf), "%s %s",
          zen::TimeLocationCoordinator::courseSourceName(location.course_source),
          zen::GpsCourse::label(zen::GpsCourse::direction(location.course_millideg)));
    else strcpy(buf, "None");
    addRow("Course", buf);
    addRow("Last event", zen::TimeLocationCoordinator::eventName(
        location.last_gps_event.type));
  }

  // Representative font samples for checking the display on real hardware.
  void buildFontLines() {
    _line_count = 0;
    addLine("Latin ABCabc xyz");
    addLine("Grk ΑΒΓ αβγξω");          // ΑΒΓ αβγξω
    addLine("Cyr АБВ абвжя");          // АБВ абвжя
    addLine("Num 0123456789");
    addLine("Sym @#&*()[]{}/\\+=");
  }

  // Shared scrollable renderer for the label/value Live tab.
  void renderRows(ZenDisplayDriver& display) {
    const int item_h  = display.lineStep();
    const int start_y = display.listStart();
    int visible = display.listVisible(item_h);
    if (visible < 1) visible = 1;
    clampScroll(_row_count, visible);

    const int reserve = scrollIndicatorReserve(display, _row_count, visible);
    for (int i = 0; i < visible && (_scroll + i) < _row_count; i++) {
      const Row& r = _rows[_scroll + i];
      int y = start_y + i * item_h;
      display.setCursor(2, y);
      display.print(r.label);
      display.drawTextRightAlign(display.width() - reserve - 2, y, r.value);
    }
    drawScrollIndicator(display, start_y, visible * item_h, _row_count, visible, _scroll);
  }

  // Shared scrollable renderer for the full-width System / Font tabs.
  void renderLines(ZenDisplayDriver& display) {
    const int item_h  = display.lineStep();
    const int start_y = display.listStart();
    int visible = display.listVisible(item_h);
    if (visible < 1) visible = 1;
    clampScroll(_line_count, visible);

    const int reserve = scrollIndicatorReserve(display, _line_count, visible);
    for (int i = 0; i < visible && (_scroll + i) < _line_count; i++) {
      int y = start_y + i * item_h;
      display.drawTextEllipsized(2, y, display.width() - reserve - 4, _lines[_scroll + i]);
    }
    drawScrollIndicator(display, start_y, visible * item_h, _line_count, visible, _scroll);
  }

  // Events are selectable and use every available character without adding an
  // ellipsis. The detail view exposes the complete stored fields.
  void renderEventLines(ZenDisplayDriver& display) {
    const int item_h = display.lineStep();
    const int start_y = display.listStart();
    int visible = display.listVisible(item_h);
    if (visible < 1) visible = 1;
    if (_line_count <= 0) _event_sel = 0;
    else if (_event_sel >= _line_count) _event_sel = _line_count - 1;
    if (_event_sel < _scroll) _scroll = _event_sel;
    if (_event_sel >= _scroll + visible) _scroll = _event_sel - visible + 1;
    clampScroll(_line_count, visible);

    const int reserve = scrollIndicatorReserve(display, _line_count, visible);
    const int max_width = display.width() - reserve - 4;
    for (int i = 0; i < visible && (_scroll + i) < _line_count; i++) {
      int index = _scroll + i;
      int y = start_y + i * item_h;
      bool selected = _line_count > 0 && index == _event_sel;
      drawRowSelection(display, y, selected, reserve);
      char text[sizeof(_lines[0])];
      strncpy(text, _lines[index], sizeof(text) - 1);
      text[sizeof(text) - 1] = 0;
      while (text[0] && display.getTextWidth(text) > max_width)
        text[strlen(text) - 1] = 0;
      display.setCursor(2, y);
      display.print(text);
      if (selected) display.setColor(ZenDisplayDriver::LIGHT);
    }
    drawScrollIndicator(display, start_y, visible * item_h,
                        _line_count, visible, _scroll);
  }

  void clampScroll(int total, int visible) {
    int max_scroll = total - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (_scroll > max_scroll) _scroll = max_scroll;
    if (_scroll < 0) _scroll = 0;
  }

public:
  DiagnosticsScreen(UITask* task) : _task(task) {}

  void onShow() override {
    // A filesystem traverse can touch many flash blocks. Sample only when the
    // user opens Diagnostics, never from the periodic render or radio loop.
    _internal_storage = the_mesh.getStorageStatus(false);
    _contacts_storage = the_mesh.getStorageStatus(true);
    if (!_internal_storage.available || !_contacts_storage.available)
      _task->operationWarning(zen::Operation::STORAGE,
          zen::OperationReason::STATUS_UNAVAILABLE);
    else if (_internal_storage.low_space || _contacts_storage.low_space)
      _task->operationWarning(zen::Operation::STORAGE,
          zen::OperationReason::LOW_SPACE);
  }

  int render(ZenDisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(ZenDisplayDriver::LIGHT);
    if (_event_view.active)
      return _event_view.render(display, _event_title, _event_detail, false, false);
    tabbar::draw(display, TAB_LABELS, TAB_COUNT, _tab);

    switch (_tab) {
      case TAB_EVENTS: buildEventLines(); renderEventLines(display); break;
      case TAB_TRANSPORT: buildTransportLines(); renderLines(display); break;
      case TAB_BATTERY: buildBatteryRows(); renderRows(display); break;
      case TAB_LOCATION: buildLocationRows(); renderRows(display); break;
      case TAB_POWER:   buildPowerRows();   renderRows(display); break;
      case TAB_SYSTEM: buildSystemLines(); renderLines(display); break;
      case TAB_FONT:   buildFontLines();   renderLines(display); break;
      default:         buildLiveRows();    renderRows(display);  break;
    }

    display.setColor(ZenDisplayDriver::LIGHT);
    if (_reset_menu.active) _reset_menu.render(display);
    // Live counters refresh once a second; the static System/Font cards don't
    // change, so they can idle. The reset popup wants a snappier redraw.
    if (_reset_menu.active) return 50;
    return (_tab == TAB_LIVE || _tab == TAB_BATTERY || _tab == TAB_LOCATION ||
            _tab == TAB_POWER)
        ? 1000 : 2000;
  }

  bool handleInput(char c) override {
    if (_event_view.active) {
      FullscreenMsgView::Result result = _event_view.handleInput(c);
      if (result == FullscreenMsgView::CLOSE)
        _event_view.active = false;
      return true;
    }
    if (_reset_menu.active) {
      auto res = _reset_menu.handleInput(c);
      if (res == PopupMenu::SELECTED && _reset_menu.selectedIndex() == 0) {
        if (_tab == TAB_EVENTS) {
          _task->clearDiagnosticLog();
          _event_sel = _scroll = 0;
          _task->showAlert("Events cleared", 800);
        } else if (_tab == TAB_TRANSPORT) {
          the_mesh.clearTransportTrace();
          _scroll = 0;
          _task->showAlert("Trace cleared", 800);
        } else {
          the_mesh.resetStats(); // baseline Dispatcher packet counters
          _task->showAlert("Counters reset", 800);
        }
      }
      return true;
    }
    if (keyIsPrev(c)) { _tab = (_tab + TAB_COUNT - 1) % TAB_COUNT; _scroll = 0; return true; }
    if (keyIsNext(c)) { _tab = (_tab + 1) % TAB_COUNT;            _scroll = 0; return true; }
    if (c == KEY_UP) {
      if (_tab == TAB_EVENTS) { if (_event_sel > 0) _event_sel--; }
      else if (_scroll > 0) _scroll--;
      return true;
    }
    if (c == KEY_DOWN) {
      if (_tab == TAB_EVENTS) {
        int count = _task->diagnosticLog().size();
        if (_event_sel + 1 < count) _event_sel++;
      } else _scroll++;
      return true;
    }
    if (c == KEY_ENTER && _tab == TAB_EVENTS && _task->diagnosticLog().size()) {
      openSelectedEvent();
      return true;
    }
    if (c == KEY_CONTEXT_MENU &&
        (_tab == TAB_LIVE || _tab == TAB_EVENTS || _tab == TAB_TRANSPORT)) {
      bool clear = _tab == TAB_EVENTS || _tab == TAB_TRANSPORT;
      _reset_menu.beginConfirm(clear ? "Clear events?" : "Reset counters?",
                               clear ? "Clear" : "Reset");
      return true;
    }
    if (c == KEY_CANCEL) { _task->gotoToolsScreen(); return true; }
    return false;
  }
};

const char* const DiagnosticsScreen::TAB_LABELS[DiagnosticsScreen::TAB_COUNT] = {
  "Live", "Events", "Transport", "Battery", "Location", "Power", "System", "Font"
};
