#pragma once
#include "../GeoUtils.h"
#include "TabBar.h"
#include "../solo/SignalFormat.h"

// ── Nearby Nodes ──────────────────────────────────────────────────────────────
// One list / detail / action-menu interaction path over two sources:
//   SRC_STORED — contacts known to the mesh (distance / bearing / last-heard)
//   SRC_SCAN   — live NODE_DISCOVER_REQ results (RSSI / SNR / status)
// Filter (type) and sort are independent axes and combine freely. The action
// menu (Hold Enter) is identical everywhere; only the per-row column and the
// detail fields differ between sources.
class NearbyScreen : public UIScreen {
  UITask* _task;

  // ── filter (type axis) ──────────────────────────────────────────────────────
  enum Filter : uint8_t { F_ALL, F_FAV, F_COMP, F_RPT, F_ROOM, F_SNSR, F_COUNT };
  static const char* FILTER_LABELS[F_COUNT];

  // ── sort axis ───────────────────────────────────────────────────────────────
  enum Sort : uint8_t { SORT_DIST, SORT_TIME };

  // ── source ──────────────────────────────────────────────────────────────────
  enum Source : uint8_t { SRC_STORED, SRC_SCAN };

  // ── action-menu actions (matched by id, not by row index) ────────────────────
  enum Action : uint8_t { ACT_PING, ACT_ADD, ACT_DELETE, ACT_FAV, ACT_PIN,
                          ACT_ADMIN, ACT_SORT, ACT_SCAN };

  // Returning from a node's Admin action preserves this browsing context.
  bool _resume_admin = false;
  bool _discover_entry = false; // entered from the dedicated Tools item
  uint8_t _discover_prev_filter = F_ALL;

  // ── unified list entry ───────────────────────────────────────────────────────
  struct Entry {
    char     name[32];
    uint8_t  type;
    uint8_t  pub_key[PUB_KEY_SIZE];
    bool     has_key;
    bool     favourite;
    // stored-source fields
    int32_t  lat_e6, lon_e6;
    float    dist_km;
    uint32_t lastmod;
    int      contact_idx;
    // scan-source fields
    int8_t   rssi, snr_x4, remote_snr_x4;
    bool     is_known;
  };

  static const int MAX_NEARBY = 32;
  Entry   _entries[MAX_NEARBY];
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
  unsigned long _scan_started_ms;
  static const unsigned long SCAN_DURATION_MS = 8000UL;

  // ── popups ────────────────────────────────────────────────────────────────────
  PopupMenu _menu;            // unified action menu (Hold Enter), list + detail
  PopupMenu _ping_menu;       // ping (special: read-only result rows)
  PopupMenu _confirm;         // delete-contact confirmation (destructive → 2-step)

  Action  _menu_actions[10];  // parallel to _menu rows — stable action ids
  int     _menu_action_count;
  char    _sort_label[16];    // dynamic label for the Sort row
  char    _fav_label[12];
  char    _pin_label[24];

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

  // The selected entry, or nullptr when the list is empty.
  const Entry* selected() const {
    return (_count > 0 && _sel < _count) ? &_entries[_sel] : nullptr;
  }

  bool typeMatchesFilter(uint8_t type, uint8_t flags, bool have_flags) const {
    switch (_filter) {
      case F_FAV:  return have_flags && (flags & 0x01);
      case F_COMP: return type == ADV_TYPE_CHAT;
      case F_RPT:  return type == ADV_TYPE_REPEATER;
      case F_ROOM: return type == ADV_TYPE_ROOM;
      case F_SNSR: return type == ADV_TYPE_SENSOR;
      case F_ALL:
      default:     return true;
    }
  }

  // ── data refresh ──────────────────────────────────────────────────────────────
  void refreshStored() {
    _count = 0;
    _own_lat = _own_lon = 0;
    _own_gps = _task->currentLocation(_own_lat, _own_lon);

    int nc = the_mesh.getNumContacts();
    for (int i = 0; i < nc && _count < MAX_NEARBY; i++) {
      ContactInfo ci;
      if (!the_mesh.getContactByIdx(i, ci)) continue;
      if (!typeMatchesFilter(ci.type, ci.flags, true)) continue;

      Entry& e = _entries[_count++];
      strncpy(e.name, ci.name, sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      memcpy(e.pub_key, ci.id.pub_key, PUB_KEY_SIZE);
      e.has_key = true;
      e.favourite = (ci.flags & 0x01) != 0;
      e.lat_e6  = ci.gps_lat;
      e.lon_e6  = ci.gps_lon;
      bool remote_gps = (ci.gps_lat != 0 || ci.gps_lon != 0);
      e.dist_km = (_own_gps && remote_gps)
                    ? geo::haversineKm(_own_lat, _own_lon, ci.gps_lat, ci.gps_lon)
                    : -1.0f;
      e.type        = ci.type;
      e.contact_idx = i;
      e.lastmod     = ci.lastmod;
      e.is_known    = true;
    }

    mergeRecentlyHeard();
    sortStored();
    clampSelection();
  }

  // Rebuild the stored list (re-merge live shares + re-sort) while keeping the
  // currently highlighted node selected across the rebuild — by contact index
  // for a stored node, or by name for a non-contact live sender. Returns false
  // if that node is no longer in the list (caller decides whether to drop the
  // detail/nav view). Shared by the list, detail and navigate refresh paths.
  bool refreshKeepingSelection() {
    int  saved_idx  = (_sel < _count) ? _entries[_sel].contact_idx : -1;
    char saved_name[sizeof(_entries[0].name)]; saved_name[0] = '\0';
    if (_sel < _count) {
      strncpy(saved_name, _entries[_sel].name, sizeof(saved_name) - 1);
      saved_name[sizeof(saved_name) - 1] = '\0';
    }
    refreshStored();
    if (saved_idx >= 0) {
      for (int i = 0; i < _count; i++)
        if (_entries[i].contact_idx == saved_idx) { _sel = i; return true; }
    }
    return false;
  }

  // Fold passively-heard adverts that aren't already listed (contact or live) into
  // the list as name+age rows — no full key/GPS, so informational only. This is
  // what absorbs the old standalone "Recent adverts" home page. Gated to the All
  // filter because AdvertPath carries no node type to filter or sort-by-dist on.
  void mergeRecentlyHeard() {
    if (_filter != F_ALL || !_task) return;
    static AdvertPath heard[8];   // scratch — refreshStored isn't reentrant
    int n = the_mesh.getRecentlyHeard(heard, 8);
    for (int i = 0; i < n && _count < MAX_NEARBY; i++) {
      AdvertPath& a = heard[i];
      if (a.name[0] == 0) continue;
      bool dup = false;
      for (int j = 0; j < _count; j++) {
        if (_entries[j].has_key &&
            memcmp(_entries[j].pub_key, a.pubkey_prefix, sizeof(a.pubkey_prefix)) == 0) { dup = true; break; }
        if (strncmp(_entries[j].name, a.name, sizeof(a.name) - 1) == 0) { dup = true; break; }
      }
      if (dup) continue;
      Entry& e = _entries[_count++];
      memset(&e, 0, sizeof(e));
      strncpy(e.name, a.name, sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      e.type        = ADV_TYPE_CHAT;   // unknown from AdvertPath — best-effort label
      e.has_key     = false;
      e.dist_km     = -1.0f;
      e.lastmod     = a.recv_timestamp;
      e.contact_idx = -1;
      e.is_known    = false;
    }
  }

  void sortStored() {
    uint32_t now_ts = rtc_clock.getCurrentTime();
    for (int i = 0; i < _count - 1; i++) {
      int best = i;
      for (int j = i + 1; j < _count; j++) {
        if (_entries[j].favourite != _entries[best].favourite) {
          if (_entries[j].favourite) best = j;
          continue;
        }
        if (_sort == SORT_TIME) {
          // lastmod=0 or lastmod>now (RTC not synced) → "unknown" → sort to bottom.
          uint32_t tj = (_entries[j].lastmod > 0 && now_ts >= _entries[j].lastmod) ? _entries[j].lastmod : 0;
          uint32_t tb = (_entries[best].lastmod > 0 && now_ts >= _entries[best].lastmod) ? _entries[best].lastmod : 0;
          if (tj > 0 && (tb == 0 || tj > tb)) best = j;  // descending — most recent first
        } else {
          float dj = _entries[j].dist_km, db = _entries[best].dist_km;
          if (dj >= 0.0f && (db < 0.0f || dj < db)) best = j;  // ascending — closest first
        }
      }
      if (best != i) { Entry tmp = _entries[i]; _entries[i] = _entries[best]; _entries[best] = tmp; }
    }
  }

  void refreshScan() {
    static DiscoverResult dr[DISCOVER_RESULTS_MAX];  // scratch — refresh isn't reentrant
    int n = the_mesh.getDiscoverResults(dr, DISCOVER_RESULTS_MAX);
    _count = 0;
    for (int i = 0; i < n && _count < MAX_NEARBY; i++) {
      if (!typeMatchesFilter(dr[i].type, 0, false)) continue;
      Entry& e = _entries[_count++];
      strncpy(e.name, dr[i].name, sizeof(e.name) - 1);
      e.name[sizeof(e.name) - 1] = '\0';
      e.type          = dr[i].type;
      memcpy(e.pub_key, dr[i].pub_key, PUB_KEY_SIZE);
      e.has_key       = true;
      e.rssi          = dr[i].rssi;
      e.snr_x4        = dr[i].snr_x4;
      e.remote_snr_x4 = dr[i].remote_snr_x4;
      e.is_known      = dr[i].is_known;
      e.favourite     = false;
      e.lat_e6 = e.lon_e6 = 0;
      e.dist_km = -1.0f;
      e.lastmod = 0;
      e.contact_idx = -1;
    }
    // strongest first
    for (int i = 0; i < _count - 1; i++) {
      int best = i;
      for (int j = i + 1; j < _count; j++)
        if (_entries[j].rssi > _entries[best].rssi) best = j;
      if (best != i) { Entry tmp = _entries[i]; _entries[i] = _entries[best]; _entries[best] = tmp; }
    }
    clampSelection();
  }

  void refresh() { if (_source == SRC_SCAN) refreshScan(); else refreshStored(); }

  void clampSelection() {
    if (_count == 0)            { _sel = _scroll = 0; }
    else if (_sel >= _count)    { _sel = _count - 1; if (_scroll > _sel) _scroll = _sel; }
  }

  // ── live scan ────────────────────────────────────────────────────────────────
  void enterScan() {
    _source        = SRC_SCAN;
    _detail        = false;
    _scanning      = true;
    _scan_started_ms = millis();
    _sel = _scroll = 0;
    the_mesh.sendNodeDiscoverReq();
    refreshScan();
  }

  void leaveScan() {
    _source = SRC_STORED;
    _detail = false;
    _sel = _scroll = 0;
    refreshStored();
  }

  // Toggle the given contact in the Favourites dial: unpin if already pinned, else
  // pin to the first empty slot. Persisted immediately (same as the Messages picker).
  void toggleFavourite(const uint8_t* pub_key) {
    int slot = _task->findFavouriteSlot(pub_key);
    if (slot >= 0) {
      _task->clearFavouriteSlot(slot);
      the_mesh.savePrefs();
      char alert[24];
      snprintf(alert, sizeof(alert), "Unpinned (slot %d)", slot + 1);
      _task->showAlert(alert, 1000);
      return;
    }
    for (int s = 0; s < NodePrefs::FAVOURITES_DIAL_COUNT; s++) {
      if (_task->isFavouriteSlotEmpty(s)) {
        _task->setFavouriteSlot(s, pub_key);
        the_mesh.savePrefs();
        char alert[24];
        snprintf(alert, sizeof(alert), "Pinned to slot %d", s + 1);
        _task->showAlert(alert, 1000);
        return;
      }
    }
    _task->logFailure("Favourites", "List full");
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
      if (_task) _task->logFailure("Ping", "Send failed");
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
      _task->logFailure("Ping", "No reply");
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
    bool is_pinned = e && has_key && _task->findFavouriteSlot(e->pub_key) >= 0;
    // Admin needs a real saved server/sensor contact, not a scan result or a
    // name-only live-share row.
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
    if (can_add)            add("Add contact", ACT_ADD);
    if (stored && is_contact && has_key) {
      snprintf(_fav_label, sizeof(_fav_label), "Fav: %s", e->favourite ? "On" : "Off");
      add(_fav_label, ACT_FAV);
    }
    if (is_contact && has_key && e->type == ADV_TYPE_CHAT) {
      snprintf(_pin_label, sizeof(_pin_label), "%s", is_pinned ? "Unpin from dial" : "Pin to dial");
      add(_pin_label, ACT_PIN);
    }
#if SOLO_FEAT_ADMIN
    if (is_admin_target && !_task->isChildModeLocked()) add("Admin", ACT_ADMIN);
#endif
    if (is_contact && has_key) add("Delete contact", ACT_DELETE);
    if (stored) add(_sort_label, ACT_SORT);   // sort is meaningless for live-scan rows
    if (!stored) add("Rescan", ACT_SCAN);
  }

  void runAction(Action a) {
    switch (a) {
      case ACT_PING: {
        const Entry* e = selected();
        rebuildPingMenu();
        _ping_menu.active = true;
        if (e && e->has_key) startPingForKey(e->pub_key);
        break;
      }
      case ACT_ADD: {
        const Entry* e = selected();
        if (e && e->has_key) {
          if (the_mesh.addDiscoveredContact(e->pub_key, e->name, e->type)) {
            _task->showAlert("Contact added", 1200);
            refresh();   // now a known contact — re-sort / re-mark this pass
          } else {
            _task->logFailure("Contacts", "List full");
          }
        }
        break;
      }
      case ACT_FAV: {
        toggleStarred();
        break;
      }
      case ACT_PIN: {
        const Entry* e = selected();
        if (e && e->has_key) toggleFavourite(e->pub_key);
        break;
      }
      case ACT_DELETE:   startDeleteConfirm(); break;
      case ACT_ADMIN: {
        const Entry* e = selected();
        ContactInfo ci;
        if (e && e->contact_idx >= 0 && the_mesh.getContactByIdx(e->contact_idx, ci))
          _task->openAdminFor(ci);
        break;
      }
      case ACT_SORT:     break;  // adjusted in-place via LEFT/RIGHT, not ENTER
      case ACT_SCAN:     enterScan();            break;
    }
  }

  // ── detail rendering ──────────────────────────────────────────────────────────
  void renderStoredDetail(DisplayDriver& display) {
    const Entry& e = _entries[_sel];
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

  void renderScanDetail(DisplayDriver& display) {
    const Entry& e = _entries[_sel];
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
  bool renderActivePopup(DisplayDriver& display) {
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
  void drawFilterTabs(DisplayDriver& display) {
    tabbar::draw(display, FILTER_LABELS, F_COUNT, _filter, display.menuHintWidth());
    display.drawContextMenuHint(DisplayDriver::LIGHT, ctxMenuOpen());   // Nodes list has a Hold-Enter menu
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
    if (_resume_admin) {
      _resume_admin = false;
      _menu.active = false;
      return;  // preserve the originating filter, selection and scroll position
    }
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

  int render(DisplayDriver& display) override {
    display.setTextSize(1);

    // Periodic refresh of the selected entry while in detail or navigate view,
    // preserving the selection across the list rebuild. Navigate refreshes
    // faster so a moving live target (a contact sharing [LOC]) tracks smoothly.
    // Re-selection keys on contact index for stored nodes, and on name for a
    // non-contact live sender — without the latter, navigating to someone who
    // only shares on a channel would drop out on the first refresh.
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
      // Re-merge + re-sort periodically so newly-heard contacts and fresh live
      // [LOC] shares bubble to the right spot under either sort (distances move
      // as you and they do, not just recency). Keep the highlighted node put.
      refreshKeepingSelection();
      _list_refresh_ms = millis();
    }

    int item_h = display.lineStep();

    display.setColor(DisplayDriver::LIGHT);
    const char* flt = (_filter != F_ALL) ? FILTER_LABELS[_filter] : nullptr;
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
        const Entry& e = _entries[idx];

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
          solo::formatQuarterDb(right, sizeof(right), e.snr_x4);
          // Reserve only this value's actual pixel width, leaving every other
          // pixel to the repeater name/key and anchoring SNR at the right edge.
          int snr_width = display.getTextWidth(right);
          int snr_col = display.width() - reserve - 2 - snr_width;
          display.drawTextEllipsized(tx, y, snr_col - tx - 2, shown,
                                     sel && !ctxMenuOpen());
          display.setColor(sel ? DisplayDriver::DARK : DisplayDriver::LIGHT);
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
        _task->gotoToolsScreen();
      } else if (_source == SRC_SCAN) leaveScan();
      else                     _task->gotoToolsScreen();
      return true;
    }
    if (c == KEY_CONTEXT_MENU) { openActionMenu(); return true; }
    // drawList() reclamps _scroll from _sel every render.
    if (c == KEY_UP   && _count > 0) { _sel = (_sel > 0) ? _sel - 1 : _count - 1; return true; }
    if (c == KEY_DOWN && _count > 0) { _sel = (_sel < _count - 1) ? _sel + 1 : 0; return true; }
    if (c == KEY_ENTER) {
      if (_count == 0) return true;
      _detail = true;
      _detail_refresh_ms = millis();
      return true;
    }
    if (!_discover_entry && keyIsPrev(c)) { _filter = (_filter + F_COUNT - 1) % F_COUNT; refresh(); return true; }
    if (!_discover_entry && keyIsNext(c)) { _filter = (_filter + 1) % F_COUNT;          refresh(); return true; }
    return false;
  }
};

const char* NearbyScreen::FILTER_LABELS[F_COUNT] = { "All", "Fav", "Comp", "Rpt", "Room", "Snsr" };
