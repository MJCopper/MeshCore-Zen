#pragma once
#include "../GeoUtils.h"
#include "TabBar.h"
#include "PathDetailsView.h"
#include "../zen/SignalFormat.h"
#include "RepeaterTraceView.h"

// ── Nearby Nodes ──────────────────────────────────────────────────────────────
// One list / detail / action-menu interaction path over two sources:
//   SRC_STORED — infrastructure contacts known to the mesh (distance / bearing / last-heard)
//   SRC_SCAN   — live NODE_DISCOVER_REQ results (RSSI / SNR / status)
// Filter (type) and sort are independent axes and combine freely. The action
// menu (Hold Enter) is identical everywhere; only the per-row column and the
// detail fields differ between sources.
class NearbyScreen : public ZenUIScreen {
  UITask* _task;

  // ── filter (type axis) ──────────────────────────────────────────────────────
  using Filters=zen::NodeListModel;
  static constexpr uint8_t F_ALL=Filters::ALL, F_FAV=Filters::FAV,
    F_RPT=Filters::RPT, F_ROOM=Filters::ROOM, F_SNSR=Filters::SNSR,
    F_COUNT=Filters::FILTER_COUNT;

  // ── sort axis ───────────────────────────────────────────────────────────────
  enum Sort : uint8_t { SORT_DIST, SORT_TIME };

  // ── source ──────────────────────────────────────────────────────────────────
  enum Source : uint8_t { SRC_STORED, SRC_SCAN };

  // ── action-menu actions (matched by id, not by row index) ────────────────────
  enum Action : uint8_t { ACT_PING, ACT_PATH, ACT_ADD, ACT_DELETE, ACT_FAV,
                          ACT_ADMIN, ACT_SORT, ACT_SCAN, ACT_TRACE };

  // Returning from a node's Admin action preserves this browsing context.
  bool _resume_admin = false;
  bool _discover_entry = false; // entered from the dedicated Tools item
  uint8_t _discover_prev_filter = F_ALL;

  // ── unified list entry ───────────────────────────────────────────────────────
  using Entry = zen::NodeListModel::Record;
  zen::NodeListModel _nodes;
  struct Provider {
    NearbyScreen* screen;
    unsigned count(zen::NodeListModel::Kind kind) { return the_mesh.nodeListCount(kind); }
    uint32_t generation() { return the_mesh.nodeListDiscoveryGeneration(); }
    bool read(zen::NodeListModel::Kind kind,unsigned index,Entry& out) {
      if(!the_mesh.readNodeListRecord(kind,index,out))return false;
      if(screen->_own_gps && (out.lat_e6 || out.lon_e6))
        out.dist_km=geo::haversineKm(screen->_own_lat,screen->_own_lon,out.lat_e6,out.lon_e6);
      return true;
    }
  };
  int     _count;
  int     _sel;
  int     _scroll;
  bool    _detail;
  int32_t _own_lat, _own_lon;
  bool    _own_gps;

  Source  _source;
  uint8_t _filter;
  uint8_t _sort;

  unsigned long _detail_refresh_ms;
  unsigned long _list_refresh_ms = 0;
  static const unsigned long DETAIL_REFRESH_MS    = 10000UL;
  static const unsigned long TIME_LIST_REFRESH_MS = 3000UL;

  // ── live-scan state ──────────────────────────────────────────────────────────
  bool          _scanning;
  uint32_t _scan_generation=0;
  unsigned long _scan_started_ms;
  static const unsigned long SCAN_DURATION_MS = 8000UL;

  // ── popups ────────────────────────────────────────────────────────────────────
  PopupMenu _menu;            // unified action menu (Hold Enter), list + detail
  PathDetailsView _path_view;
  RepeaterTraceView _trace_view;
  uint8_t _trace_key[32]{};
  char _trace_name[64]{};
  PopupMenu _ping_menu;       // ping (special: read-only result rows)
  PopupMenu _confirm;         // delete-contact confirmation (destructive → 2-step)

  Action  _menu_actions[10];  // parallel to _menu rows — stable action ids
  int     _menu_action_count;
  char    _sort_label[16];    // dynamic label for the Sort row
  char    _fav_label[12];

  // ── ping state ───────────────────────────────────────────────────────────────
  char _ping_time_str[24];
  char _ping_snr_out_str[24];
  char _ping_snr_back_str[24];
  bool _pinging;
  unsigned long _ping_started_ms;
  static const unsigned long PING_TIMEOUT_MS = 3000UL;

  // ── helpers ──────────────────────────────────────────────────────────────────
  static void pubKeyToBase64(const uint8_t* key, char* out, int out_len) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int j = 0;
    for (int i = 0; i < PUB_KEY_SIZE && j + 5 < out_len; i += 3) {
      uint32_t b = ((uint32_t)key[i] << 16)
                 | (i+1 < PUB_KEY_SIZE ? (uint32_t)key[i+1] << 8 : 0)
                 | (i+2 < PUB_KEY_SIZE ? (uint32_t)key[i+2]      : 0);
      out[j++] = T[(b >> 18) & 63];
      out[j++] = T[(b >> 12) & 63];
      if (i+1 < PUB_KEY_SIZE) out[j++] = T[(b >> 6) & 63];
      if (i+2 < PUB_KEY_SIZE) out[j++] = T[b & 63];
    }
    out[j] = '\0';
  }

  bool useImperial() const { return _task && _task->useImperial(); }

  // Full "X ago" form for the detail view, built on the shared short-age tag
  // so there's one bucket ladder (see geo::fmtAgeShort).
  static void fmtAge(char* buf, int n, uint32_t lastmod) {
    char s[8];
    geo::fmtAgeShort(s, sizeof(s), rtc_clock.getCurrentTime(), lastmod);
    if (!s[0]) { snprintf(buf, n, "unknown"); return; }
    snprintf(buf, n, "%s ago", s);
  }

  static const char* typeName(uint8_t t) {
    switch (t) {
      case ADV_TYPE_CHAT:     return "Companion";
      case ADV_TYPE_REPEATER: return "Repeater";
      case ADV_TYPE_ROOM:     return "Room";
      case ADV_TYPE_SENSOR:   return "Sensor";
      default:                return "Unknown";
    }
  }

  static const char* typeShort(uint8_t t) {
    switch (t) {
      case ADV_TYPE_REPEATER: return "Rpt";
      case ADV_TYPE_SENSOR:   return "Snsr";
      case ADV_TYPE_ROOM:     return "Room";
      case ADV_TYPE_CHAT:     return "Comp";
      default:                return "?";
    }
  }

  // Only the full-identity snapshot may be used by an action. Re-resolve it
  // against the authoritative source, even after PopupMenu closes itself.
  const Entry* selected() {
    Provider provider{this};
    return _nodes.validate(provider)?_nodes.selected():nullptr;
  }

  void selectRow() {
    Provider provider{this};
    _nodes.select(provider,_sel);
  }

  bool rebuild(bool scan) {
    _own_lat=_own_lon=0;
    _own_gps=_task->currentLocation(_own_lat,_own_lon);
    Provider provider{this};
    bool had_target=_nodes.selected()!=nullptr;
    int retained=_nodes.rebuild(provider,scan,zen::NodeListModel::Filter(_filter),
                               _sort==SORT_TIME,rtc_clock.getCurrentTime());
    _count=_nodes.count();
    if(retained>=0)_sel=retained;
    clampSelection();
    if(retained<0) {
      if(had_target) {
        // Do not let a rebuild retarget an open view or confirmation.
        _detail=false;_menu.active=false;_confirm.active=false;closePingMenu();
      }
      selectRow();
    }
    return retained>=0;
  }

  void refreshStored() { rebuild(false); }
  bool refreshKeepingSelection() { return rebuild(false); }
  void refreshScan() { rebuild(true); }

  void validateViewTarget() {
    const Entry* previous=_nodes.selected();
    const uint8_t previous_type=previous?previous->type:0;
    const Entry* current=previous?selected():nullptr;
    if(previous && (!current || current->type!=previous_type)) {
      // A type change also invalidates the action set (e.g. Trace -> room).
      _detail=false;_menu.active=false;_confirm.active=false;closePingMenu();
      refresh();
      return;
    }
    // Cache sorting, eviction and contact compaction can invalidate an index
    // without removing the selected node. Rebuild before rendering or input.
    Provider provider{this};
    Entry value{};
    for(int i=0;i<_count;++i) if(!_nodes.read(provider,i,value)) {
      refresh();return;
    }
  }

  void refresh() { if (_source == SRC_SCAN) refreshScan(); else refreshStored(); }

  void clampSelection() {
    if (_count == 0)            { _sel = _scroll = 0; }
    else if (_sel >= _count)    { _sel = _count - 1; if (_scroll > _sel) _scroll = _sel; }
  }

  // ── live scan ────────────────────────────────────────────────────────────────
  void enterScan() {
    _nodes.clearSelection();
    _source        = SRC_SCAN;
    _detail        = false;
    _scanning      = true;
    if(++_scan_generation==0) ++_scan_generation;
    _scan_started_ms = millis();
    _sel = _scroll = 0;
    if(!the_mesh.sendNodeDiscoverReq()) _scanning=false;
    refreshScan();
  }

  void leaveScan() {
    _nodes.clearSelection();
    _scanning=false;
    _source = SRC_STORED;
    _detail = false;
    _sel = _scroll = 0;
    refreshStored();
  }

  void toggleStarred() {
    const Entry* entry = selected();
    if (!entry || !entryIsContact(entry) || !entry->has_key) return;
    bool favourite = !entry->favourite;
    if (!the_mesh.setContactFavourite(entry->pub_key, favourite)) return;
    snprintf(_fav_label, sizeof(_fav_label), "Fav: %s", favourite ? "On" : "Off");
    refreshKeepingSelection();
  }

  // Deleting a contact is destructive → confirm first (default highlight = Cancel).
  void startDeleteConfirm() {
    const Entry* e = selected();
    if (!e || !e->has_key || !entryIsContact(e)) return;
    _confirm.begin("Delete contact?", 2);
    _confirm.addItem("Delete");
    _confirm.addItem("Cancel");
    _confirm.setSelected(1);
    _confirm.active = true;
  }

  void doDeleteSelected() {
    const Entry* e = selected();
    if (!e || !e->has_key || !entryIsContact(e)) return;
    uint8_t key[PUB_KEY_SIZE];
    memcpy(key, e->pub_key, PUB_KEY_SIZE);
    if (the_mesh.deleteContactByKey(key)) {
      _task->showAlert("Contact deleted", 1200);
      _detail = false;   // the node this detail/nav view showed is gone
      refresh();
      clampSelection();
    }
  }

  // ── ping ──────────────────────────────────────────────────────────────────────
  void resetPingLines() {
    _ping_time_str[0] = '\0';
    _ping_snr_out_str[0] = '\0';
    _ping_snr_back_str[0] = '\0';
  }

  int pingRowCount() const {
    return 1 + (_ping_time_str[0] ? 1 : 0)
             + (_ping_snr_out_str[0] ? 1 : 0)
             + (_ping_snr_back_str[0] ? 1 : 0);
  }

  void rebuildPingMenu() {
    int keep = _ping_menu.selectedIndex();  // preserve selection across a rebuild
    _ping_menu.begin("Ping", 4);
    _ping_menu.addItem("Send");
    if (_ping_time_str[0])      _ping_menu.addItem(_ping_time_str);
    if (_ping_snr_out_str[0])   _ping_menu.addItem(_ping_snr_out_str);
    if (_ping_snr_back_str[0])  _ping_menu.addItem(_ping_snr_back_str);
    _ping_menu.setSelected(keep);
  }

  void closePingMenu(bool clear_task = true) {
    _ping_menu.active = false;
    _pinging = false;
    if (clear_task && _task) _task->clearPing();
    resetPingLines();
  }

  void startPingForKey(const uint8_t* pub_key) {
    resetPingLines();
    snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: ...");
    if (_task && _task->startPing(pub_key)) {
      _pinging = true;
      _ping_started_ms = millis();
    } else {
      snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: send fail");
      if (_task) _task->operationError(zen::Operation::PING,
          zen::OperationOutcome::TRANSPORT_FAILURE, zen::OperationReason::SEND_FAILED);
    }
    if (_ping_menu.active) rebuildPingMenu();   // surface "RTT: ..." immediately
  }

  void updatePingMenuState() {
    if (!_ping_menu.active || !_task || (!_pinging && !_task->isPingActive())) return;

    int16_t snr_out = 0, snr_back = 0;
    uint32_t rtt = 0;
    _task->getPingResult(snr_out, snr_back, rtt);

    if (_pinging && (snr_out != 0 || snr_back != 0 || rtt != 0)) {
      if (rtt > 0 && rtt < 10000) {
        snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: %lums", rtt);
      } else {
        snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: timeout");
      }
      if (snr_out != 0)
        snprintf(_ping_snr_out_str, sizeof(_ping_snr_out_str), "SNR out: %.1f", snr_out / 4.0f);
      if (snr_back != 0)
        snprintf(_ping_snr_back_str, sizeof(_ping_snr_back_str), "SNR back: %.1f", snr_back / 4.0f);
      _pinging = false;
    } else if (_pinging && millis() - _ping_started_ms >= PING_TIMEOUT_MS) {
      snprintf(_ping_time_str, sizeof(_ping_time_str), "RTT: timeout");
      _ping_snr_out_str[0] = '\0';
      _ping_snr_back_str[0] = '\0';
      _pinging = false;
      _task->operationError(zen::Operation::PING,
          zen::OperationOutcome::TIMEOUT, zen::OperationReason::NO_REPLY);
      if (_task) _task->clearPing();
    }
    if (pingRowCount() != _ping_menu.count()) rebuildPingMenu();
  }

  // Ping popup input. Result rows are read-only, so UP/DOWN are swallowed to keep
  // the highlight on "Send".
  void handlePingMenuInput(char c) {
    if (c == KEY_UP || c == KEY_DOWN) return;
    auto res = _ping_menu.handleInput(c);
    if (res == PopupMenu::SELECTED) {
      const Entry* e = selected();
      if (!_pinging && e && e->has_key) startPingForKey(e->pub_key);
      _ping_menu.active = true;   // stay open so Ping can be repeated
    } else if (res == PopupMenu::CANCELLED) {
      closePingMenu();
    }
  }

  // ── action menu (Hold Enter) — same everywhere ──────────────────────────────
  void buildSortLabel() {
    snprintf(_sort_label, sizeof(_sort_label),
             "Sort: %s", _sort == SORT_TIME ? "Recent" : "Dist");
  }

  // A row is already a contact when it carries a contacts[] index (stored source)
  // or the live-scan flagged it known. Full 32-byte key required to add/delete.
  bool entryIsContact(const Entry* e) const {
    return e && ((e->contact_idx >= 0) || (_source == SRC_SCAN && e->is_known));
  }

  void openActionMenu() {
    const Entry* e = selected();
    bool stored  = (_source == SRC_STORED);
    bool has_key = e && e->has_key;
    bool is_contact = entryIsContact(e);
    bool can_add = e && has_key && !is_contact;   // a new node we can save
    // Admin needs a real saved server/sensor contact, not a scan result or a
    // unverified discovery result.
    bool is_admin_target = e && stored && e->contact_idx >= 0
                           && (e->type == ADV_TYPE_REPEATER || e->type == ADV_TYPE_ROOM
                               || e->type == ADV_TYPE_SENSOR);

    buildSortLabel();
    _menu_action_count = 0;
    _menu.begin("Options", 10);
    auto add = [&](const char* label, Action a) {
      if (a == ACT_SORT || a == ACT_FAV) _menu.addValueItem(label);
      else _menu.addItem(label);
      _menu_actions[_menu_action_count++] = a;
    };

    if (has_key) add("Ping",          ACT_PING);
    if(e&&has_key&&e->type==ADV_TYPE_REPEATER&&!_task->isChildModeLocked()) add("Trace",ACT_TRACE);
    if (stored && is_contact && has_key && !_task->isChildModeLocked())
      add("Path details", ACT_PATH);
    if (can_add)            add("Add contact", ACT_ADD);
    if (stored && is_contact && has_key) {
      snprintf(_fav_label, sizeof(_fav_label), "Fav: %s", e->favourite ? "On" : "Off");
      add(_fav_label, ACT_FAV);
    }
#if ZEN_FEATURE_ADMIN
    if (is_admin_target && !_task->isChildModeLocked()) add("Admin", ACT_ADMIN);
#endif
    if (is_contact && has_key) add("Delete contact", ACT_DELETE);
    if (stored) add(_sort_label, ACT_SORT);   // sort is meaningless for live-scan rows
    if (!stored) add("Rescan", ACT_SCAN);
  }

  void runAction(Action a) {
    switch (a) {
      case ACT_TRACE: {
        const Entry* e=selected();
        if(e&&e->has_key) {
          if(_task->startRepeaterTrace(e->pub_key,e->name)) {
            memcpy(_trace_key,e->pub_key,sizeof(_trace_key));
            snprintf(_trace_name,sizeof(_trace_name),"%s",e->name);
            _trace_view.open();
          }

        }
        break;
      }
      case ACT_PING: {
        const Entry* e = selected();
        rebuildPingMenu();
        _ping_menu.active = true;
        if (e && e->has_key) startPingForKey(e->pub_key);
        break;
      }
      case ACT_PATH: {
        const Entry* e = selected();
        if (e && e->has_key)
          _path_view.open(e->pub_key, _task->latestPathAttempt(e->pub_key));
        break;
      }
      case ACT_ADD: {
        const Entry* e = selected();
        if (e && e->has_key) {
          if (the_mesh.addDiscoveredContact(e->pub_key, e->name, e->type)) {
            _task->showAlert("Contact added", 1200);
            refresh();   // now a known contact — re-sort / re-mark this pass
          } else {
            _task->operationError(zen::Operation::CONTACTS,
                zen::OperationOutcome::REJECTED, zen::OperationReason::LIST_FULL);
          }
        }
        break;
      }
      case ACT_FAV: {
        toggleStarred();
        break;
      }
      case ACT_DELETE:   startDeleteConfirm(); break;
      case ACT_ADMIN: {
        const Entry* e = selected();
        ContactInfo ci;
        if (e && e->has_key) {
          const ContactInfo* contact=the_mesh.lookupContactByPubKey(e->pub_key,32);
          if(contact) { ci=*contact;_task->openAdminFor(ci); }
        }
        break;
      }
      case ACT_SORT:     break;  // adjusted in-place via LEFT/RIGHT, not ENTER
      case ACT_SCAN:     enterScan();            break;
    }
  }

  // ── detail rendering ──────────────────────────────────────────────────────────
  void renderStoredDetail(ZenDisplayDriver& display) {
    const Entry* target=selected();
    if(!target)return;
    const Entry& e=*target;
    const int hdr  = display.listStart();   // content top (gap below the header separator)
    display.drawInvertedHeader(e.name, true, ctxMenuOpen());

    int step = display.lineStep();
    if (step * 5 > display.height() - hdr) step = (display.height() - hdr) / 5;
    char buf[32];
    snprintf(buf, sizeof(buf), "Lat: %.5f", e.lat_e6 / 1e6);
    display.setCursor(2, hdr); display.print(buf);
    snprintf(buf, sizeof(buf), "Lon: %.5f", e.lon_e6 / 1e6);
    display.setCursor(2, hdr + step); display.print(buf);

    if (e.dist_km >= 0.0f) {
      char dist[12];
      geo::fmtDist(dist, sizeof(dist), e.dist_km, useImperial());
      int az = geo::bearingDeg(_own_lat, _own_lon, e.lat_e6, e.lon_e6);
      snprintf(buf, sizeof(buf), "Dist: %s %s", dist, geo::bearingCardinal(az));
    } else {
      snprintf(buf, sizeof(buf), "Dist: no GPS");
    }
    display.setCursor(2, hdr + step * 2); display.print(buf);
    snprintf(buf, sizeof(buf), "Type: %s", typeName(e.type));
    display.setCursor(2, hdr + step * 3); display.print(buf);
    char age[16];
    fmtAge(age, sizeof(age), e.lastmod);
    snprintf(buf, sizeof(buf), "Seen: %s", age);
    display.drawTextEllipsized(2, hdr + step * 4, display.width() - 4, buf);
  }

  void renderScanDetail(ZenDisplayDriver& display) {
    const Entry* target=selected();
    if(!target)return;
    const Entry& e=*target;
    const int hdr = display.listStart();   // content top (gap below the header separator)

    char label[32];
    if (e.name[0]) { strncpy(label, e.name, 31); label[31] = '\0'; }
    else           { snprintf(label, sizeof(label), "[%s]", typeName(e.type)); }
    display.drawInvertedHeader(label, true, ctxMenuOpen());

    char b64[48];
    pubKeyToBase64(e.pub_key, b64, sizeof(b64));
    int max_chars = (display.width() - 4) / display.getCharWidth();
    char b64_line[48];
    if (max_chars < 4) {
      b64_line[0] = '\0';
    } else if ((int)strlen(b64) > max_chars) {
      strncpy(b64_line, b64, max_chars - 3);
      b64_line[max_chars - 3] = '\0';
      strcat(b64_line, "...");
    } else {
      strncpy(b64_line, b64, sizeof(b64_line) - 1);
      b64_line[sizeof(b64_line) - 1] = '\0';
    }
    if (b64_line[0]) { display.setCursor(2, hdr); display.print(b64_line); }

    int step = display.lineStep();
    if (step * 5 > display.height() - hdr) step = (display.height() - hdr) / 5;
    char buf[32];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)e.rssi);
    display.setCursor(2, hdr + step);     display.print(buf);
    snprintf(buf, sizeof(buf), "SNR:  %.1f dB", e.snr_x4 / 4.0f);
    display.setCursor(2, hdr + step * 2); display.print(buf);
    snprintf(buf, sizeof(buf), "Rem:  %.1f dB", e.remote_snr_x4 / 4.0f);
    display.setCursor(2, hdr + step * 3); display.print(buf);
    display.setCursor(2, hdr + step * 4);
    display.print(e.is_known ? "Status: known" : "Status: new");
  }

  // Any Hold-Enter popup on screen → highlight the ≡ hint so it reads as the
  // source of the open menu.
  bool ctxMenuOpen() const { return _menu.active || _confirm.active || _ping_menu.active; }

  // Draw whichever popup is active over the current view. Returns true if one was.
  bool renderActivePopup(ZenDisplayDriver& display) {
    updatePingMenuState();
    if (_ping_menu.active)   { _ping_menu.render(display);   return true; }
    if (_confirm.active)     { _confirm.render(display);     return true; }
    if (_menu.active)        { _menu.render(display);        return true; }
    return false;
  }

  // Header as a circular filter tab bar (shared geometry — see TabBar.h): the
  // active filter sits centred as a filled pill, neighbours fan out either
  // side and wrap around (the tab before "All" is "Snsr", and vice-versa),
  // matching the wrap-around LEFT/RIGHT cycle.
  void drawFilterTabs(ZenDisplayDriver& display) {
    tabbar::draw(display, Filters::filterLabels(), F_COUNT, _filter, display.menuHintWidth());
    display.drawContextMenuHint(ZenDisplayDriver::LIGHT, ctxMenuOpen());   // Nodes list has a Hold-Enter menu
  }

public:
  NearbyScreen(UITask* task)
    : _task(task), _count(0), _sel(0), _scroll(0), _detail(false),
      _own_lat(0), _own_lon(0), _own_gps(false),
      _source(SRC_STORED), _filter(F_ALL), _sort(SORT_DIST),
      _detail_refresh_ms(0), _scanning(false), _scan_started_ms(0),
      _menu_action_count(0), _pinging(false), _ping_started_ms(0) {
    resetPingLines();
    _sort_label[0] = '\0';
  }

  void onShow() override {
    _path_view.active = false;
    if (_resume_admin) {
      _resume_admin = false;
      _menu.active = false;
      return;  // preserve the originating filter, selection and scroll position
    }
    _nodes.clearSelection();
    _sel = _scroll = 0;
    _detail = false;
    _source = SRC_STORED;
    _filter = F_ALL;
    // Sort persists across visits; Nodes always starts on the All filter.
    _scanning = false;
    _menu.active = false;
    _ping_menu.active = false;
    _confirm.active = false;
    _pinging = false;
    _discover_entry = false;
    resetPingLines();
    _task->clearPing();
    refreshStored();
  }

  void resumeFromAdmin() { _resume_admin = true; }

  // Entered from Tools > Discover after onShow() resets the shared Nodes view.
  void startDiscoverScan() {
    _discover_entry = true;
    _discover_prev_filter = _filter;
    _filter = F_RPT;
    enterScan();
  }

  void observeDisplayActivity(zen::DisplayActivity& activity) const {
    bool pending=_scanning && millis()-_scan_started_ms<SCAN_DURATION_MS;
    activity.observe({zen::DisplayActivity::DISCOVERY,_scan_generation},pending,
                     _scan_started_ms+SCAN_DURATION_MS);
  }
  void onHide() override {
    _scanning=false;
    if(_trace_view.active) { _task->cancelRepeaterTrace();_trace_view.active=false;_menu.active=false; }
  }
  int render(ZenDisplayDriver& display) override {
    if(_trace_view.active) {
      if(_task->isChildModeLocked()) { _task->cancelRepeaterTrace();_trace_view.active=false;_menu.active=false; }
      else {
        int refresh=_trace_view.render(display,the_mesh.repeaterTrace(),the_mesh.repeaterTraceName(),
          [](const uint8_t* key,unsigned width,char* out,size_t size) { the_mesh.traceHopName(key,width,out,size); });
        display.drawContextMenuHint(ZenDisplayDriver::LIGHT,ctxMenuOpen());
        if(renderActivePopup(display)) return 50;
        return refresh;
      }
    }
    validateViewTarget();
    display.setTextSize(1);
    if (_path_view.active && _task->isChildModeLocked()) _path_view.active = false;
    if (_path_view.active) {
      _path_view.setAttempt(_task->latestPathAttempt(_path_view.key()));
      return _path_view.render(display);
    }

    // Refresh live fields and ordering while preserving the full identity.
    if (_detail && _source == SRC_STORED &&
        millis() - _detail_refresh_ms >= DETAIL_REFRESH_MS) {
      if (!refreshKeepingSelection()) _detail = false;
      _detail_refresh_ms = millis();
    }

    // ── navigate-to-node view ────────────────────────────────────────────────
    // ── detail view ──────────────────────────────────────────────────────────
    if (_detail && _sel < _count) {
      if (_source == SRC_SCAN) renderScanDetail(display);
      else                     renderStoredDetail(display);
      renderActivePopup(display);
      return _ping_menu.active ? 50 : 2000;
    }

    // ── list view ────────────────────────────────────────────────────────────
    if (_source == SRC_SCAN) {
      refreshScan();
      if (_scanning && millis() - _scan_started_ms >= SCAN_DURATION_MS) _scanning = false;
    } else if (millis() - _list_refresh_ms >= TIME_LIST_REFRESH_MS) {
      // Rebuild + re-sort periodically so newly-heard contacts and fresh live
      // [LOC] shares bubble to the right spot under either sort (distances move
      // as you and they do, not just recency). Keep the highlighted node put.
      refreshKeepingSelection();
      _list_refresh_ms = millis();
    }

    int item_h = display.lineStep();

    display.setColor(ZenDisplayDriver::LIGHT);
    const char* flt = (_filter != F_ALL) ? Filters::filterLabels()[_filter] : nullptr;
    if (_source == SRC_SCAN) {
      char title[28];
      const char* base = _scanning ? "Scanning" : "Scan";
      if (_discover_entry) {
        if (!_scanning && _count == 0) snprintf(title, sizeof(title), "No Repeaters");
        else                           snprintf(title, sizeof(title), "%s (%d)", base, _count);
      } else if (flt) {
        if (!_scanning && _count == 0) snprintf(title, sizeof(title), "Scan %s: None", flt);
        else                           snprintf(title, sizeof(title), "%s %s (%d)", base, flt, _count);
      } else if (_scanning)            snprintf(title, sizeof(title), "Scanning (%d)", _count);
      else if (_count == 0)            snprintf(title, sizeof(title), "Scan: None");
      else                             snprintf(title, sizeof(title), "Scan (%d)", _count);
      display.drawCenteredHeader(title, true, ctxMenuOpen());
    } else {
      // Stored list: the filter is a visible tab strip (LEFT/RIGHT switches tabs).
      drawFilterTabs(display);
    }

    if (_count == 0) {
      char empty[24];
      if (_source == SRC_SCAN) {
        const char* msg;
        if (_scanning)   msg = "Waiting...";
        else if (flt)  { snprintf(empty, sizeof(empty), "No %s nodes", flt); msg = empty; }
        else             msg = "No nodes found";
        display.drawTextCentered(display.width() / 2, display.height() / 2, msg);
      } else {
        const char* hint;
        if (flt) { snprintf(empty, sizeof(empty), "No %s contacts", flt); hint = "Left/Right: Filter"; }
        else     { snprintf(empty, sizeof(empty), "No contacts found");   hint = "Enter: Discover";   }
        display.drawTextCentered(display.width() / 2, display.height() / 2 - display.lineStep() / 2, empty);
        display.drawTextCentered(display.width() / 2, display.height() / 2 + display.lineStep() / 2, hint);
      }
    } else {
      drawList(display, _count, _sel, _scroll, [&](int idx, int y, bool sel, int reserve) {
        Entry e{};
        Provider provider{this};
        if(!_nodes.read(provider,idx,e))return;

        drawRowSelection(display, y, sel, reserve);

        char fallback[32];
        const char* shown = e.name;
        int tx = 2;
        // A star is MeshCore's favourite flag; carousel pinning is a separate
        // shortcut and deliberately has no second row glyph.
        if (e.favourite) {
          int iw = ICON_PG_STAR.w * miniIconScale(display);
          miniIconDrawCentered(display, tx + iw / 2, y + display.getLineHeight() / 2 - 1, ICON_PG_STAR);
          tx += iw + 2;
        }
        if (_source == SRC_SCAN && !e.name[0]) {  // unknown node → "[Type]"
          snprintf(fallback, sizeof(fallback), "[%s]", typeName(e.type));
          shown = fallback;
        }
        // Stored Nodes devote the complete row to the name. Discovery retains
        // a compact locally-received SNR column because it is scan-specific.
        if (_source == SRC_SCAN) {
          char right[10];
          zen::formatQuarterDb(right, sizeof(right), e.snr_x4);
          // Reserve only this value's actual pixel width, leaving every other
          // pixel to the repeater name/key and anchoring SNR at the right edge.
          int snr_width = display.getTextWidth(right);
          int snr_col = display.width() - reserve - 2 - snr_width;
          display.drawTextEllipsized(tx, y, snr_col - tx - 2, shown,
                                     sel && !ctxMenuOpen());
          display.setColor(sel ? ZenDisplayDriver::DARK : ZenDisplayDriver::LIGHT);
          display.drawTextRightAlign(display.width() - reserve - 2, y, right);
        } else {
          display.drawTextEllipsized(tx, y, display.width() - reserve - tx - 2,
                                     shown, sel && !ctxMenuOpen());
        }
      });
    }

    if (renderActivePopup(display)) return 50;
    if (_source == SRC_SCAN) return _scanning ? 200 : 2000;
    return _count == 0 ? 3000 : 2000;
  }

  bool handleInput(char c) override {
    if(_trace_view.active) {
      if(_task->isChildModeLocked()) {
        _task->cancelRepeaterTrace();_trace_view.active=false;_menu.active=false;
        return true;
      }
      if(_menu.active) {
        if(_menu.handleInput(c)==PopupMenu::SELECTED) {
          _task->cancelRepeaterTrace();
          if(_task->startRepeaterTrace(_trace_key,_trace_name)) _trace_view.open();
        }
        return true;
      }
      if(c==KEY_CONTEXT_MENU) {
        _menu.begin("Trace options",1);_menu.addItem("Re-Trace");
      }
      else if(c==KEY_CANCEL) { _task->cancelRepeaterTrace();_trace_view.active=false; }
      else if(c==KEY_UP) _trace_view.scroll(-1);
      else if(c==KEY_DOWN) _trace_view.scroll(1);
      return true;
    }
    if (_path_view.active && _task->isChildModeLocked()) _path_view.active = false;
    if (_path_view.active) return _path_view.handleInput(c);
    validateViewTarget();
    // ── navigate-to-node view — any nav key returns to detail ─────────────────
    // ── popups (same handling in list and detail) ─────────────────────────────
    if (_confirm.active) {
      auto res = _confirm.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        if (_confirm.selectedIndex() == 0) doDeleteSelected();
        _confirm.active = false;
      } else if (res == PopupMenu::CANCELLED) {
        _confirm.active = false;
      }
      return true;
    }
    if (_ping_menu.active)   { handlePingMenuInput(c); return true; }
    if (_menu.active) {
      // LEFT/RIGHT on the Sort row toggles the value in-place and rebuilds the
      // label; the popup stays open so the user can keep tapping. Enter advances
      // the value like Right. Other rows swallow Left/Right.
      bool value_enter = c == KEY_ENTER && _menu.selectedIndex() >= 0 &&
                         _menu.selectedIndex() < _menu_action_count &&
                         (_menu_actions[_menu.selectedIndex()] == ACT_SORT ||
                          _menu_actions[_menu.selectedIndex()] == ACT_FAV);
      if (keyIsPrev(c) || keyIsNext(c) || value_enter) {
        int i = _menu.selectedIndex();
        if (i >= 0 && i < _menu_action_count) {
          if (_menu_actions[i] == ACT_SORT) {
            _sort = (_sort == SORT_DIST) ? SORT_TIME : SORT_DIST;
            buildSortLabel();
            refreshKeepingSelection();
          } else if (_menu_actions[i] == ACT_FAV) {
            toggleStarred();
          }
        }
        return true;
      }
      auto res = _menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int i = _menu.selectedIndex();
        if (i >= 0 && i < _menu_action_count &&
            _menu_actions[i] != ACT_SORT && _menu_actions[i] != ACT_FAV)
          runAction(_menu_actions[i]);
      }
      return true;
    }

    // ── detail view ───────────────────────────────────────────────────────────
    if (_detail) {
      if (c == KEY_CANCEL)            { _detail = false; closePingMenu(); return true; }
      if (c == KEY_CONTEXT_MENU)      { openActionMenu(); return true; }
      return true;
    }

    // ── list view ───────────────────────────────────────────────────────────
    if (c == KEY_CANCEL) {
      if (_source == SRC_SCAN && _discover_entry) {
        _filter = _discover_prev_filter;
        _discover_entry = false;
        leaveScan();
        _task->gotoToolsScreen();
      } else if (_source == SRC_SCAN) leaveScan();
      else                     _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) { openActionMenu(); return true; }
    // drawList() reclamps _scroll from _sel every render.
    if (c == KEY_UP   && _count > 0) { _sel = (_sel > 0) ? _sel - 1 : _count - 1; selectRow(); return true; }
    if (c == KEY_DOWN && _count > 0) { _sel = (_sel < _count - 1) ? _sel + 1 : 0; selectRow(); return true; }
    if (c == KEY_ENTER) {
      if (_count == 0) return true;
      _detail = true;
      _detail_refresh_ms = millis();
      return true;
    }
    if (!_discover_entry && keyIsPrev(c)) { _filter = (_filter + F_COUNT - 1) % F_COUNT; _nodes.clearSelection(); refresh(); return true; }
    if (!_discover_entry && keyIsNext(c)) { _filter = (_filter + 1) % F_COUNT; _nodes.clearSelection(); refresh(); return true; }
    return false;
  }
};
