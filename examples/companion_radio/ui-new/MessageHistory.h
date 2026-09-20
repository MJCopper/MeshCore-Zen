#pragma once
// Message history store for MessagesScreen: two RAM ring buffers (channel + DM)
// with their per-entry delivery state (channel "relayed into mesh" echo, DM
// end-to-end ACK + auto-resend) and the per-channel unread counters. Pure
// storage + queries — no UI/phase state lives here. The screen keeps selection,
// scroll, the unread "viewing session" bookkeeping, the room-login table, and
// all rendering, and reaches entries through the accessors below.
//
// Single-TU fragment: included by UITask.cpp before MessagesScreen.h. AckState,
// MSG_TEXT_BUF and the two entry structs are file-scope (not nested) so the
// phase machine in MessagesScreen keeps referring to them unqualified.

#include "../solo/MessageAckTracker.h"
#include "../solo/NodeRouteRetry.h"
#include "../solo/PathDetails.h"

// Outgoing-message delivery state. DM: a real end-to-end ACK (✓ delivered to
// the recipient). Channel: only a "relayed into mesh" echo from a repeater (no
// recipient ACK exists for floods); a missing echo is shown as a local failure.
enum AckState : uint8_t { ACK_NONE = 0, ACK_PENDING, ACK_OK, ACK_FAIL };
enum DeliveryRoute : uint8_t {
  DELIVERY_ROUTE_NONE = 0,
  DELIVERY_ROUTE_DIRECT,
  DELIVERY_ROUTE_PATH,
  DELIVERY_ROUTE_FLOOD,
  DELIVERY_ROUTE_RELAY,
};

// History text holds a full received message. Channel messages carry the sender
// embedded as "Name: body" in the payload, so a message can be up to the
// over-the-air maximum (MAX_TEXT_LEN). Size the buffers to that + NUL, otherwise
// long messages containing multi-byte UTF-8 get their tail clipped.
static const int MSG_TEXT_BUF = MAX_TEXT_LEN + 1;

struct ChHistEntry {
  uint8_t  ch_idx;
  char     text[MSG_TEXT_BUF];
  uint32_t timestamp;
  uint32_t activity_seq;   // RAM-only local insertion order across both history rings
  uint8_t  relay_status;   // pending while listening; OK with count, FAIL at zero
  uint8_t  relay_count;    // matching repeater echoes heard during the window
  uint32_t relay_seq;      // MyMesh relay seq to match against onChannelRelayed()
};

struct DmHistEntry {
  uint8_t  prefix[4];
  uint8_t  outgoing;
  char     text[MSG_TEXT_BUF];
  uint32_t timestamp;
  uint32_t activity_seq;   // RAM-only local insertion order across both history rings
  uint8_t  ack_status;       // AckState; meaningful only when outgoing
  uint8_t  delivery_route;   // DeliveryRoute used by the latest transmission
  uint32_t ack_tag;          // expected_ack CRC to match against onMsgAck()
  solo::MessageAckTracker acknowledgements;
  uint32_t ack_deadline_ms;  // millis() by which a pending ACK must arrive
  // Sender-perspective message timestamp: the send timestamp for outgoing
  // (reused verbatim on resend so the recipient treats it as a retry), or the
  // sender_timestamp for incoming (used to dedup retried copies). 0 = unknown.
  uint32_t msg_ts;
  uint8_t  attempt;          // last attempt number sent (outgoing); next resend = attempt+1
  uint8_t  path_hops;        // last learned route shape, retained after flood fallback
  uint8_t  path_hash_bytes;
  uint8_t  fallback_from_path;
  uint8_t  fallback_hops;
  solo::NodeRouteRetry route_retry;
};

class MessageHistory {
public:
  // Shared ring for all channels combined; each slot carries a full MSG_TEXT_BUF,
  // so this ring dominates RAM — kept modest to leave heap headroom (see
  // DM_HIST_MAX). The DM ring is likewise bounded.
  static const int CH_HIST_MAX = 48;
  static const int DM_HIST_MAX = 32;

  MessageHistory()
    : _hist_head(0), _hist_count(0), _dm_hist_head(0), _dm_hist_count(0),
      _activity_seq(0), _dm_maintenance_pending(false),
      _next_dm_maintenance_ms(0) {
    memset(_ch_unread, 0, sizeof(_ch_unread));
  }

  // ── Channel ring ──────────────────────────────────────────────────────────

  // Append a channel message. `viewing` = the user is currently in this
  // channel's history (so it isn't counted unread). `timestamp` is the sender's
  // own send time (0 = unknown — use receipt time). Returns the ring position
  // (an opaque handle for armChannelRelay / chAtPos), or -1 if rejected.
  int addChannelMsg(uint8_t ch_idx, const char* text, bool viewing, uint32_t timestamp = 0) {
    // Guard against bogus channel indices (e.g. findChannelIdx() returned -1
    // and was cast to uint8_t → 255). Storing such an entry would burn a ring
    // slot for a message that no visible channel can ever surface.
    if (ch_idx >= MAX_GROUP_CHANNELS) return -1;
    int pos;
    if (_hist_count < CH_HIST_MAX) {
      pos = (_hist_head + _hist_count) % CH_HIST_MAX;
      _hist_count++;
    } else {
      pos = _hist_head;
      // Evicting the oldest entry — drop its share of the unread counter so
      // the badge can't claim a message the ring no longer holds. The counter
      // refers to the channel's NEWEST unread messages, so losing the oldest
      // entry only costs an unread one when everything the ring still holds
      // for that channel is unread; otherwise the evicted entry was already
      // read and decrementing would undercount the newer unread ones.
      uint8_t evicted = _hist[pos].ch_idx;
      if (evicted < MAX_GROUP_CHANNELS && _ch_unread[evicted] > 0 &&
          _ch_unread[evicted] >= histCountForChannel(evicted)) {
        _ch_unread[evicted]--;
      }
      _hist_head = (_hist_head + 1) % CH_HIST_MAX;
    }
    _hist[pos].ch_idx = ch_idx;
    _hist[pos].timestamp = timestamp ? timestamp : rtc_clock.getCurrentTime();
    _hist[pos].activity_seq = ++_activity_seq;
    strncpy(_hist[pos].text, text, sizeof(_hist[pos].text) - 1);
    _hist[pos].text[sizeof(_hist[pos].text) - 1] = '\0';
    _hist[pos].relay_status = ACK_NONE;
    _hist[pos].relay_count = 0;
    _hist[pos].relay_seq = 0;

    if (!viewing && _ch_unread[ch_idx] < 99) _ch_unread[ch_idx]++;
    return pos;
  }

  // count history entries for a specific channel
  int histCountForChannel(int ch_idx) const {
    int n = 0;
    for (int i = 0; i < _hist_count; i++) {
      if (_hist[(_hist_head + i) % CH_HIST_MAX].ch_idx == (uint8_t)ch_idx) n++;
    }
    return n;
  }

  // get ring-buffer position of j-th history entry for channel (newest first)
  int histEntryForChannel(int ch_idx, int j) const {
    int n = 0;
    for (int i = _hist_count - 1; i >= 0; i--) {
      int pos = (_hist_head + i) % CH_HIST_MAX;
      if (_hist[pos].ch_idx == (uint8_t)ch_idx) {
        if (n == j) return pos;
        n++;
      }
    }
    return -1;
  }

  uint32_t latestChannelActivity(int ch_idx) const {
    int pos = histEntryForChannel(ch_idx, 0);
    return pos >= 0 ? _hist[pos].activity_seq : 0;
  }

  // Called when a repeater echo of one of our channel sends is heard.
  void markChannelRelayed(uint32_t seq) {
    if (seq == 0) return;
    for (int i = 0; i < _hist_count; i++) {
      ChHistEntry& e = _hist[(_hist_head + i) % CH_HIST_MAX];
      if ((e.relay_status == ACK_PENDING || e.relay_status == ACK_OK) &&
          e.relay_seq == seq) {
        if (e.relay_count < 255) e.relay_count++;
        e.relay_status = ACK_OK;
        return;
      }
    }
  }

  bool markChannelRelayExpired(uint32_t seq) {
    if (seq == 0) return false;
    for (int i = 0; i < _hist_count; i++) {
      ChHistEntry& e = _hist[(_hist_head + i) % CH_HIST_MAX];
      if (e.relay_seq == seq) {
        if (e.relay_count == 0) e.relay_status = ACK_FAIL;
        return true;
      }
    }
    return false;
  }

  // Arm the "relayed into mesh" marker on a just-sent entry (pos from
  // addChannelMsg) — MyMesh tracked the flood it originated and will report a
  // heard repeater echo by seq.
  void armChannelRelay(int pos, uint32_t seq) {
    if (pos < 0 || pos >= CH_HIST_MAX) return;
    _hist[pos].relay_status = ACK_PENDING;
    _hist[pos].relay_count  = 0;
    _hist[pos].relay_seq    = seq;
  }

  ChHistEntry&       chAtPos(int pos)       { return _hist[pos]; }
  const ChHistEntry& chAtPos(int pos) const { return _hist[pos]; }

  // ── Per-channel unread counters ─────────────────────────────────────────────
  // Always clamped to what the ring still holds for that channel. The raw
  // counter and the ring can drift apart — MessagesScreen's viewing-session
  // bookkeeping re-applies the unread snapshot taken when the channel was
  // opened, so entries evicted mid-session would otherwise leave the badge
  // promising messages the history list can no longer show (badge says 7,
  // opening the channel shows an empty list). Capping on read keeps the badge
  // honest whatever the raw counter says.
  uint8_t chUnread(int ch) const {
    if (ch < 0 || ch >= MAX_GROUP_CHANNELS) return 0;
    int held = histCountForChannel(ch);
    return _ch_unread[ch] < held ? _ch_unread[ch] : (uint8_t)held;
  }
  void setChUnread(int ch, uint8_t v) {
    if (ch >= 0 && ch < MAX_GROUP_CHANNELS) _ch_unread[ch] = v;
  }
  void clearAllChannelUnread() { memset(_ch_unread, 0, sizeof(_ch_unread)); }
  int  getTotalChannelUnread() const {
    // Same clamp as chUnread(), but counting ring occupancy for every channel
    // in one pass instead of re-walking the ring once per channel.
    uint8_t held[MAX_GROUP_CHANNELS];
    memset(held, 0, sizeof(held));
    for (int i = 0; i < _hist_count; i++) {
      uint8_t ch = _hist[(_hist_head + i) % CH_HIST_MAX].ch_idx;
      if (ch < MAX_GROUP_CHANNELS && held[ch] < 255) held[ch]++;
    }
    int total = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++)
      total += (_ch_unread[i] < held[i]) ? _ch_unread[i] : held[i];
    return total;
  }

  // ── DM ring ─────────────────────────────────────────────────────────────────

  // ack_tag != 0 marks an outgoing DM as awaiting an end-to-end ACK by
  // ack_deadline_ms; 0 means "sent, no confirmation possible" (no path / incoming).
  // msg_ts = sender-perspective timestamp (send ts for outgoing / sender_timestamp
  // for incoming); initial_direct selects the fixed direct-then-flood policy.
  int storeDMMsg(const uint8_t* pub_key, bool outgoing, const char* text,
                 uint32_t ack_tag = 0, uint32_t ack_deadline_ms = 0,
                 uint32_t msg_ts = 0, uint8_t initial_route = DELIVERY_ROUTE_NONE) {
    int pos;
    if (_dm_hist_count < DM_HIST_MAX) {
      pos = (_dm_hist_head + _dm_hist_count) % DM_HIST_MAX;
      _dm_hist_count++;
    } else {
      pos = _dm_hist_head;
      _dm_hist_head = (_dm_hist_head + 1) % DM_HIST_MAX;
    }
    memcpy(_dm_hist[pos].prefix, pub_key, 4);
    _dm_hist[pos].outgoing = outgoing ? 1 : 0;
    // Prefer the sender's own timestamp — a room-sync replay or an
    // offline-queued message held by a repeater can arrive long after it was
    // actually sent, so "now" would mislabel every backlog message as fresh.
    // Fall back to receipt time only when the sender's timestamp is unknown.
    _dm_hist[pos].timestamp = msg_ts ? msg_ts : rtc_clock.getCurrentTime();
    _dm_hist[pos].activity_seq = ++_activity_seq;
    strncpy(_dm_hist[pos].text, text, sizeof(DmHistEntry::text) - 1);
    _dm_hist[pos].text[sizeof(DmHistEntry::text) - 1] = '\0';
    _dm_hist[pos].ack_status      = (outgoing && ack_tag) ? ACK_PENDING : ACK_NONE;
    _dm_hist[pos].delivery_route  = outgoing ? initial_route : DELIVERY_ROUTE_NONE;
    _dm_hist[pos].ack_tag         = ack_tag;
    _dm_hist[pos].acknowledgements = solo::MessageAckTracker();
    _dm_hist[pos].acknowledgements.record(0, ack_tag, initial_route);
    _dm_hist[pos].ack_deadline_ms = ack_deadline_ms;
    _dm_hist[pos].msg_ts          = msg_ts;
    _dm_hist[pos].attempt         = 0;
    _dm_hist[pos].path_hops = 0;
    _dm_hist[pos].path_hash_bytes = 0;
    _dm_hist[pos].fallback_from_path = 0;
    _dm_hist[pos].fallback_hops = 0;
    if (initial_route == DELIVERY_ROUTE_PATH) {
      ContactInfo route_contact;
      if (contactByPrefix(pub_key, route_contact)) {
        solo::PathShape shape = solo::pathShape(route_contact.out_path_len,
                                                sizeof(route_contact.out_path));
        if (shape.valid && shape.known) {
          _dm_hist[pos].path_hops = shape.hops;
          _dm_hist[pos].path_hash_bytes = shape.hash_bytes;
        }
      }
    }
    bool initial_direct = initial_route == DELIVERY_ROUTE_DIRECT ||
                          initial_route == DELIVERY_ROUTE_PATH;
    if (outgoing && ack_tag) _dm_hist[pos].route_retry.begin(initial_direct);
    else _dm_hist[pos].route_retry.reset();
    scheduleDmMaintenance();
    return pos;
  }

  bool addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text,
                uint32_t sender_timestamp = 0) {
    // Drop retried copies of an incoming DM: a resend reuses the sender's
    // timestamp and text but carries a fresh packet hash, so the mesh dup-filter
    // lets it through. Match on prefix + sender_timestamp + text to suppress it.
    if (!outgoing && sender_timestamp != 0) {
      for (int i = 0; i < _dm_hist_count; i++) {
        const DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
        if (!e.outgoing && e.msg_ts == sender_timestamp &&
            memcmp(e.prefix, pub_key, 4) == 0 && strcmp(e.text, text) == 0)
          return false;  // duplicate retry — already in history
      }
    }
    storeDMMsg(pub_key, outgoing, text, 0, 0, outgoing ? 0 : sender_timestamp,
               DELIVERY_ROUTE_NONE);
    return true;
  }

  // Store one logical message for app-owned sends. A later app retry reuses
  // timestamp and text, so update the existing entry rather than adding a new
  // transcript row. Retry counters stay zero: only the app may retransmit it.
  bool storeAppDM(const uint8_t* pub_key, const char* text, uint32_t msg_ts,
                  uint8_t attempt, uint32_t ack_tag,
                  uint32_t ack_deadline_ms, uint8_t route) {
    for (int i = 0; i < _dm_hist_count; i++) {
      DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (!e.outgoing || e.msg_ts != msg_ts || memcmp(e.prefix, pub_key, 4) ||
          strcmp(e.text, text)) continue;
      e.attempt = attempt;
      e.ack_tag = ack_tag;
      e.ack_deadline_ms = ack_deadline_ms;
      e.ack_status = ack_tag ? ACK_PENDING : ACK_NONE;
      e.delivery_route = route;
      if (route == DELIVERY_ROUTE_PATH) {
        ContactInfo current;
        if (contactByPrefix(pub_key, current)) {
          solo::PathShape shape = solo::pathShape(current.out_path_len,
                                                  sizeof(current.out_path));
          if (shape.valid && shape.known) {
            e.path_hops = shape.hops;
            e.path_hash_bytes = shape.hash_bytes;
          }
        }
      }
      e.acknowledgements.record(attempt, ack_tag, route);
      e.route_retry.reset();
      scheduleDmMaintenance();
      return false;
    }
    int pos = storeDMMsg(pub_key, true, text, ack_tag, ack_deadline_ms,
                         msg_ts, route);
    DmHistEntry& e = _dm_hist[pos];
    e.attempt = attempt;
    e.route_retry.reset();
    scheduleDmMaintenance();
    return true;
  }

  int dmHistCountForContact(const uint8_t* prefix) const {
    int n = 0;
    for (int i = 0; i < _dm_hist_count; i++)
      if (memcmp(_dm_hist[(_dm_hist_head + i) % DM_HIST_MAX].prefix, prefix, 4) == 0) n++;
    return n;
  }

  int dmHistEntryForContact(const uint8_t* prefix, int j) const { // j=0 = newest
    int n = 0;
    for (int i = _dm_hist_count - 1; i >= 0; i--) {
      int pos = (_dm_hist_head + i) % DM_HIST_MAX;
      if (memcmp(_dm_hist[pos].prefix, prefix, 4) == 0) {
        if (n == j) return pos;
        n++;
      }
    }
    return -1;
  }

  // Newest outgoing message in this conversation whose automatic delivery
  // policy has finished. The effective-state check also catches an expired
  // final ACK window just before the maintenance tick records ACK_FAIL.
  int latestFailedDMForContact(const uint8_t* prefix) const {
    for (int i = _dm_hist_count - 1; i >= 0; i--) {
      int pos = (_dm_hist_head + i) % DM_HIST_MAX;
      const DmHistEntry& e = _dm_hist[pos];
      if (e.outgoing && memcmp(e.prefix, prefix, 4) == 0 &&
          dmEffectiveStatus(e) == ACK_FAIL)
        return pos;
    }
    return -1;
  }

  int latestFailedChannel(int ch_idx) const {
    for (int i = _hist_count - 1; i >= 0; i--) {
      int pos = (_hist_head + i) % CH_HIST_MAX;
      const ChHistEntry& e = _hist[pos];
      if (e.ch_idx == (uint8_t)ch_idx && e.relay_status == ACK_FAIL)
        return pos;
    }
    return -1;
  }

  // Keep an immediate send failure in the transcript so it can be retried.
  int storeFailedDM(const uint8_t* pub_key, const char* text, uint32_t msg_ts,
                    uint8_t route) {
    int pos = storeDMMsg(pub_key, true, text, 0, 0, msg_ts, route);
    _dm_hist[pos].ack_status = ACK_FAIL;
    return pos;
  }

  // Restart the complete fixed retry policy on an existing failed row. Keep
  // its timestamp and text for recipient-side deduplication, but advance the
  // wire attempt number so mesh duplicate suppression sees a fresh packet.
  bool resendFailedDM(int pos, const uint8_t* expected_prefix) {
    if (pos < 0 || pos >= DM_HIST_MAX) return false;
    DmHistEntry& e = _dm_hist[pos];
    if (!e.outgoing || memcmp(e.prefix, expected_prefix, 4) != 0 ||
        dmEffectiveStatus(e) != ACK_FAIL || e.attempt == 255)
      return false;

    ContactInfo c;
    if (!contactByPrefix(e.prefix, c)) return false;
    bool direct = c.out_path_len != OUT_PATH_UNKNOWN;
    uint32_t expected_ack = 0, est_timeout = 0;
    uint8_t next_attempt = e.attempt + 1;
    if (the_mesh.sendMessage(c, e.msg_ts, next_attempt, e.text,
                             expected_ack, est_timeout) <= 0 || !expected_ack)
      return false;

    e.attempt = next_attempt;
    e.ack_status = ACK_PENDING;
    e.ack_tag = expected_ack;
    e.ack_deadline_ms = millis() + est_timeout + 4000;
    e.delivery_route = direct
        ? (c.out_path_len == 0 ? DELIVERY_ROUTE_DIRECT : DELIVERY_ROUTE_PATH)
        : DELIVERY_ROUTE_FLOOD;
    if (e.delivery_route == DELIVERY_ROUTE_PATH) {
      solo::PathShape shape = solo::pathShape(c.out_path_len, sizeof(c.out_path));
      if (shape.valid) { e.path_hops = shape.hops; e.path_hash_bytes = shape.hash_bytes; }
    }
    e.acknowledgements.record(e.attempt, expected_ack, e.delivery_route);
    e.route_retry.begin(direct);
    scheduleDmMaintenance();
    return true;
  }

  uint32_t latestDmActivity(const uint8_t* prefix, bool incoming_only = false) const {
    for (int i = _dm_hist_count - 1; i >= 0; i--) {
      const DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (memcmp(e.prefix, prefix, 4) == 0 && (!incoming_only || !e.outgoing))
        return e.activity_seq;
    }
    return 0;
  }

  // Effective status for display. A pending ACK only reads as failed once its
  // deadline has passed AND no automatic retries remain. Until then the entry
  // stays pending (tickDmResends() retries / finalises it). Safety net for
  // when the tick hasn't run yet; the tick is the authority that writes ACK_FAIL.
  AckState dmEffectiveStatus(const DmHistEntry& e) const {
    if (e.ack_status == ACK_PENDING && e.route_retry.exhausted() &&
        (int32_t)(millis() - e.ack_deadline_ms) >= 0)
      return ACK_FAIL;
    return (AckState)e.ack_status;
  }

  // Called when an end-to-end ACK arrives (routed from MyMesh::onAckRecv).
  // Marks the matching pending outgoing DM as delivered.
  bool markDmDelivered(uint32_t ack_crc, uint8_t* matched_prefix = nullptr) {
    if (ack_crc == 0) return false;
    for (int i = 0; i < _dm_hist_count; i++) {
      DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      uint8_t route;
      if (e.outgoing && e.acknowledgements.match(ack_crc, route)) {
        if (matched_prefix) memcpy(matched_prefix, e.prefix, sizeof(e.prefix));
        e.ack_status = ACK_OK;
        e.delivery_route = route;
        if (route == DELIVERY_ROUTE_PATH || route == DELIVERY_ROUTE_DIRECT)
          e.fallback_from_path = 0; // a late ACK proved the earlier route worked
        e.route_retry.reset();
        scheduleDmMaintenance();
        return true;
      }
    }
    return false;
  }

  // Periodic resend driver for outgoing DMs whose ACK deadline lapsed with no
  // ACK: resend with the next attempt# (reusing the original timestamp so the
  // recipient dedups). Known routes get one direct retry, then three forced
  // flood tries; unknown routes get two flood retries, then the entry fails (✗).
  uint8_t tickDmResends() {
    uint8_t newly_failed = 0;
    uint32_t now = millis();
    if (!_dm_maintenance_pending ||
        (int32_t)(now - _next_dm_maintenance_ms) < 0) return 0;
    for (int i = 0; i < _dm_hist_count; i++) {
      DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (!e.outgoing || e.ack_status != ACK_PENDING) continue;
      if ((int32_t)(now - e.ack_deadline_ms) < 0) continue;   // still waiting
      if (e.route_retry.exhausted()) {
        e.ack_status = ACK_FAIL;
        newly_failed++;
        continue;
      }
      ContactInfo c;
      if (!contactByPrefix(e.prefix, c)) {
        e.ack_status = ACK_FAIL;
        newly_failed++;
        continue;
      }

      solo::NodeRouteRetry::Action retry = e.route_retry.next(
          c.out_path_len != OUT_PATH_UNKNOWN);
      if (retry == solo::NodeRouteRetry::EXHAUSTED) {
        e.ack_status = ACK_FAIL;
        newly_failed++;
        continue;
      }
      bool send_direct = retry == solo::NodeRouteRetry::RETRY_PATH;
      if (!send_direct) {
        if ((e.delivery_route == DELIVERY_ROUTE_PATH ||
             e.delivery_route == DELIVERY_ROUTE_DIRECT) && !e.fallback_from_path) {
          e.fallback_from_path = 1;
          e.fallback_hops = e.path_hops;
        }
        // Force every fallback attempt to flood, even if a path-return packet
        // learned a fresh route after an earlier flood whose ACK was missed.
        the_mesh.clearContactPath(e.prefix, sizeof(e.prefix));
        c.out_path_len = OUT_PATH_UNKNOWN;
        memset(c.out_path, 0, sizeof(c.out_path));
      }
      uint32_t expected_ack = 0, est_timeout = 0;
      uint8_t next_attempt = e.attempt + 1;
      if (the_mesh.sendMessage(c, e.msg_ts, next_attempt, e.text,
                               expected_ack, est_timeout) > 0 && expected_ack) {
        e.attempt         = next_attempt;
        e.ack_tag         = expected_ack;   // each attempt has a distinct ACK CRC
        e.ack_deadline_ms = now + est_timeout + 4000;
        e.delivery_route = send_direct
            ? (c.out_path_len == 0 ? DELIVERY_ROUTE_DIRECT : DELIVERY_ROUTE_PATH)
            : DELIVERY_ROUTE_FLOOD;
        if (e.delivery_route == DELIVERY_ROUTE_PATH) {
          solo::PathShape shape = solo::pathShape(c.out_path_len, sizeof(c.out_path));
          if (shape.valid) { e.path_hops = shape.hops; e.path_hash_bytes = shape.hash_bytes; }
        }
        e.acknowledgements.record(e.attempt, expected_ack, e.delivery_route);
      } else {
        e.ack_status = ACK_FAIL;            // couldn't compose/send — give up
        newly_failed++;
      }
    }
    scheduleDmMaintenance();
    return newly_failed;
  }

  // Low power deliberately ends automatic delivery. Rows remain failed and
  // therefore retain the existing manual-resend action after power returns.
  void cancelDmResends() {
    for (int i = 0; i < _dm_hist_count; i++) {
      DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (e.outgoing && e.ack_status == ACK_PENDING) {
        e.ack_status = ACK_FAIL;
        e.route_retry.reset();
      }
    }
    _dm_maintenance_pending = false;
  }

  // Recent DM contacts, newest first, deduped. Resolves the 4-byte _dm_hist
  // prefix to a 6-byte pub_key prefix by walking the contact list once per
  // unique sender. Returns the number filled.
  int getRecentDMContacts(uint8_t out[][NodePrefs::FAVOURITE_PREFIX_LEN], int max) const {
    int n = 0;
    for (int i = _dm_hist_count - 1; i >= 0 && n < max; i--) {
      int pos = (_dm_hist_head + i) % DM_HIST_MAX;
      const uint8_t* p4 = _dm_hist[pos].prefix;
      // Skip if already collected.
      bool dup = false;
      for (int j = 0; j < n; j++) if (memcmp(out[j], p4, 4) == 0) { dup = true; break; }
      if (dup) continue;
      // Find a real contact whose pub_key starts with this 4-byte prefix.
      for (int idx = 0; ; idx++) {
        ContactInfo c;
        if (!the_mesh.getContactByIdx(idx, c)) break;
        if (memcmp(c.id.pub_key, p4, 4) == 0) {
          memcpy(out[n], c.id.pub_key, NodePrefs::FAVOURITE_PREFIX_LEN);
          n++;
          break;
        }
      }
    }
    return n;
  }

  DmHistEntry&       dmAtPos(int pos)       { return _dm_hist[pos]; }
  const DmHistEntry& dmAtPos(int pos) const { return _dm_hist[pos]; }

  solo::PathAttemptSnapshot latestPathAttempt(const uint8_t* prefix) const {
    solo::PathAttemptSnapshot result;
    for (int i = _dm_hist_count - 1; i >= 0; i--) {
      const DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (!e.outgoing || memcmp(e.prefix, prefix, 4)) continue;
      result.route = e.delivery_route;
      result.tries = e.attempt + 1;
      result.hops = e.path_hops;
      result.hash_bytes = e.path_hash_bytes;
      result.result = dmEffectiveStatus(e);
      result.fallback_from_path = e.fallback_from_path != 0;
      result.fallback_hops = e.fallback_hops;
      break;
    }
    return result;
  }

private:
  // Recompute only when delivery state changes. UITask may call
  // tickDmResends() every loop, but the idle path above is then constant-time
  // instead of walking the whole history ring.
  void scheduleDmMaintenance() {
    _dm_maintenance_pending = false;
    uint32_t now = millis();
    uint32_t shortest = 0;
    for (int i = 0; i < _dm_hist_count; i++) {
      const DmHistEntry& e = _dm_hist[(_dm_hist_head + i) % DM_HIST_MAX];
      if (!e.outgoing || e.ack_status != ACK_PENDING) continue;
      uint32_t wait = (int32_t)(e.ack_deadline_ms - now) > 0
          ? e.ack_deadline_ms - now : 0;
      if (!_dm_maintenance_pending || wait < shortest) shortest = wait;
      _dm_maintenance_pending = true;
    }
    if (_dm_maintenance_pending) _next_dm_maintenance_ms = now + shortest;
  }

  // Look up a contact by 4-byte pub_key prefix (as stored in DmHistEntry).
  bool contactByPrefix(const uint8_t* prefix, ContactInfo& out) const {
    int total = the_mesh.getNumContacts();
    for (int i = 0; i < total; i++) {
      ContactInfo c;
      if (!the_mesh.getContactByIdx(i, c)) continue;
      if (memcmp(c.id.pub_key, prefix, 4) == 0) { out = c; return true; }
    }
    return false;
  }

  ChHistEntry _hist[CH_HIST_MAX];
  int _hist_head, _hist_count;
  uint8_t _ch_unread[MAX_GROUP_CHANNELS];

  DmHistEntry _dm_hist[DM_HIST_MAX];
  int _dm_hist_head, _dm_hist_count;
  uint32_t _activity_seq;
  bool _dm_maintenance_pending;
  uint32_t _next_dm_maintenance_ms;
};
