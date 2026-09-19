#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp after SettingsScreen.h is defined.

#include "icons.h"     // scalable mini-icons (delivery markers)
#include "ChannelsView.h"  // on-device channel add/edit form (Channels tab)
#include "MessageEditorSupport.h"
#include "../solo/NotificationPreferences.h"
#include "../solo/BuiltinMelodies.h"
#include "MessageTranscriptView.h"
#include "PathDetailsView.h"
#include "RecentParticipants.h"
#include "../solo/QuickReplies.h"
#include "../solo/MessageDraftStore.h"

class MessagesScreen : public UIScreen {
  UITask* _task;

  enum Phase { MODE_SELECT, CONTACT_PICK, ROOM_LOGIN_WAIT, DM_HIST, MSG_PICK, CHANNEL_PICK, CHANNEL_HIST, KEYBOARD };
  Phase _phase;

  // MODE_SELECT
  int _mode_sel;  // 0=Direct, 1=Channel, 2=Room Servers

  // CONTACT_PICK
  int _contact_sel, _contact_scroll;
  int _num_contacts;
  uint16_t _sorted[MAX_CONTACTS];
  ContactInfo _sel_contact;
  bool _room_mode;  // true = picking a room server, false = picking a DM contact
  bool _login_mode; // true while KEYBOARD is collecting a room-login password

  // CHANNEL_PICK
  int _channel_sel, _channel_scroll;
  int _num_channels;
  uint8_t _channel_indices[MAX_GROUP_CHANNELS];
  int _sel_channel_idx;
  bool _sending_to_channel;

  // Carries the just-sent DM's ACK tag + deadline + send timestamp from
  // sendText() to afterSend().
  uint32_t _last_ack_tag = 0;
  uint32_t _last_ack_deadline_ms = 0;
  uint32_t _last_send_ts = 0;
  uint8_t _last_send_route = DELIVERY_ROUTE_NONE;

  // MSG_PICK (shared)
  int _msg_sel, _msg_scroll;
  // Values below BUILTIN_COUNT address firmware replies; following values
  // address the five editable custom slots.
  uint8_t _active_replies[solo::QuickReplies::MAX_VISIBLE_COUNT];
  int _active_msg_count;
  bool _quick_msgs_bypassed = false;
  enum EntryOrigin : uint8_t { ORIGIN_NORMAL, ORIGIN_HOME_CATEGORY,
                               ORIGIN_DIRECT_DM, ORIGIN_DIRECT_CHANNEL };
  EntryOrigin _entry_origin = ORIGIN_NORMAL;

  // CHANNEL_HIST — selection + the unread "viewing session" bookkeeping. The
  // history ring itself and the per-channel unread counters live in _history.
  int _hist_sel, _hist_scroll;
  FullscreenMsgView _fs;
  int  _unread_at_entry;    // channel unread count when entering CHANNEL_HIST
  int  _viewing_max_seen;  // highest _hist_sel reached in current session

  // KEYBOARD
  KeyboardWidget* _kb;

  // Context menu (opened by KEY_CONTEXT_MENU in CHANNEL_PICK / CONTACT_PICK / histories)
  PopupMenu _ctx_menu;
  bool      _ctx_dirty;
  // Channel the open channel-context menu acts on, frozen at open time. The
  // Fav toggle can remove the highlighted channel from a fav-only list, so
  // re-reading _channel_indices[_channel_sel] mid-interaction could silently
  // retarget the menu at a different channel.
  uint8_t   _ctx_ch_idx = 0;
  char      _ctx_notif_item[22];
  char      _ctx_melody_item[20];
  char      _ctx_fav_item[12];
  char      _ctx_pin_item[28];   // "Pin to dial" or "Unpin (slot N)"
  char      _ctx_ch_fav_item[12]; // "Fav" or "Unfav"
  char      _pin_slot_labels[NodePrefs::FAVOURITES_COUNT][40];  // "Slot N: " + full UTF-8 contact name
  bool      _pin_picker_active;  // true while the slot-picker submenu is open
  bool      _channel_delete_confirm_active = false;
  bool      _retry_menu_active = false; // transcript Hold-Enter action menu
  bool      _participant_picker_active = false;
  int       _retry_hist_pos = -1;       // frozen row selected when menu opened
  enum TranscriptAct : uint8_t { TRANS_REPLY_TO, TRANS_RESEND, TRANS_QUICK };
  uint8_t   _transcript_act[3];
  int       _transcript_act_n = 0;
  RecentParticipants _recent_participants;
  char      _reply_prefix[36];   // "@[nick] " built when reply is triggered
  bool      _reply_mode;         // true while composing a reply (prefix is prepended)

  // Share-compose mode: launched from elsewhere (e.g. a waypoint) with a
  // prepared message; the user picks a recipient and the text lands prefilled
  // in the keyboard to confirm/edit before sending.
  bool      _share_mode = false;
  char      _share_text[160] = "";
  // Live-share target picker: reuse the recipient chooser to set the persistent
  // auto-share target (channel / DM) instead of composing a message.
  bool      _pick_target = false;
  // Bot channel picker: same idea, but jumps straight to CHANNEL_PICK (the
  // bot only ever targets a channel, never a DM) to set bot_channel_idx.
  bool      _pick_bot_channel = false;
  // Bot room picker: jumps straight to CONTACT_PICK with room_mode on (the
  // bot's room target is always a room server) to set bot_room_prefix.
  bool      _pick_bot_room = false;

  // The message-history rings (channel + DM), their per-entry delivery state and
  // per-channel unread counters live in this store (see MessageHistory.h). The
  // phase machine below keeps only the view state — selection, scroll, the
  // fullscreen readers — and reaches entries through _history's accessors. The
  // shared types (AckState, ChHistEntry, DmHistEntry, MSG_TEXT_BUF) are file-
  // scope, so they're still referred to unqualified throughout this screen.
  MessageHistory _history;
  PathDetailsView _path_view;
  solo::MessageDraftStore _drafts;

  // DM_HIST view state (the ring itself is in _history).
  int _dm_hist_sel, _dm_hist_scroll;
  FullscreenMsgView _dm_fs;
  MessageTranscriptView _dm_transcript;
  MessageTranscriptView _channel_transcript;

  int _hist_visible = 2;  // updated in render(); for history list scroll clamping

  void expandMsg(const char* tmpl, char* out, int out_len) const {
    double lat = 0, lon = 0;
    bool gps_valid = false;
#if ENV_INCLUDE_GPS == 1
    LocationProvider* loc = sensors.getLocationProvider();
    if (loc && loc->isValid()) {
      lat = loc->getLatitude() / 1000000.0;
      lon = loc->getLongitude() / 1000000.0;
      gps_valid = true;
    }
#endif
    float batt = (float)board.getBattMilliVolts() / 1000.0f;
    uint32_t now = rtc_clock.getCurrentTime();
    ::expandMsg(tmpl, out, out_len, lat, lon, gps_valid,
                now, _task->localOffsetMinutes(now),
                &sensors, batt);
  }

  bool buildReplyPrefixForName(const char* name) {
    if (!name || !name[0] || strcmp(name, "Me") == 0) return false;
    snprintf(_reply_prefix, sizeof(_reply_prefix), "@[%.31s] ", name);
    return true;
  }

  // Split a DM-history entry into the name to show as the author and the body to
  // show beneath it. Room servers carry many guests, so incoming room posts are
  // stored "Sender: text" (MyMesh::queueMessage); split that off so each line is
  // attributed to its guest. Outgoing → "Me"; plain DMs (or no separator) keep
  // the contact name and the text unchanged.
  //
  // Does NOT strip a leading "@[nick] " reply prefix. Both transcript and
  // fullscreen renderers parse it so they can show the addressee separately.
  const char* dmDisplayParts(const DmHistEntry& e, bool is_room, const char* contact_name,
                             char* sender_buf, int sender_cap) const {
    const char* body = e.text;
    if (e.outgoing) {
      strncpy(sender_buf, "Me", sender_cap - 1);
    } else if (is_room) {
      const char* sep = strstr(body, ": ");
      if (sep) {
        int nl = (int)(sep - body);
        if (nl > sender_cap - 1) nl = sender_cap - 1;
        strncpy(sender_buf, body, nl);
        sender_buf[nl] = '\0';
        return sep + 2;   // body after "Sender: "
      }
      strncpy(sender_buf, contact_name, sender_cap - 1);
    } else {
      strncpy(sender_buf, contact_name, sender_cap - 1);
    }
    sender_buf[sender_cap - 1] = '\0';
    return body;
  }

  // Build "@[nick] " prefix from a channel message text ("nick: body") into _reply_prefix.
  // Returns false if sender is "Me" (own message — no reply prefix needed).
  bool buildChannelReplyPrefix(const char* text) {
    const char* sep = strstr(text, ": ");
    if (!sep) return false;
    int slen = (int)(sep - text);
    if (slen == 2 && strncmp(text, "Me", 2) == 0) return false;
    char nick[32];
    if (slen > (int)sizeof(nick) - 1) slen = sizeof(nick) - 1;
    memcpy(nick, text, slen);
    nick[slen] = '\0';
    return buildReplyPrefixForName(nick);
  }

  // Build "@[nick] " into _reply_prefix for a reply to a DM/room post, raw
  // (UTF-8) so it goes out over the air intact — like buildChannelReplyPrefix,
  // and unlike the old per-site code which ran the name through the lossy
  // display transliterator. Addresses the same author the history shows
  // (dmDisplayParts): in a room every post is stored "Author: text", so the
  // addressee is that author, not the room server's own name (_sel_contact.name);
  // a plain 1:1 DM uses the contact name. Caller ensures the post is incoming.
  void buildDmReplyPrefix(const DmHistEntry& e) {
    char nick[32];
    dmDisplayParts(e, _sel_contact.type == ADV_TYPE_ROOM, _sel_contact.name, nick, sizeof(nick));
    buildReplyPrefixForName(nick);
  }

  void startReply(bool to_channel) {
    _sending_to_channel = to_channel;
    beginCustomMessage(true);
  }

  // Recipient chosen while sharing — open the keyboard with the prepared text.
  void beginShareCompose(bool channel) {
    _sending_to_channel = channel;
    _reply_mode = false;
    messageeditor::begin(*_kb, _share_text, &sensors);
    _phase = KEYBOARD;
  }

  // Build the fullscreen-message options popup when replying is meaningful.
  void buildFsMenu(const char* body, bool reply_allowed) {
    (void)body;
    if (!reply_allowed) return;
    _ctx_menu.begin("Options", 1);
    _ctx_menu.addItem("Reply");
  }

  // Dispatch the selected fullscreen-options row. `channel` picks which
  // fullscreen view to close when starting a reply.
  void dispatchFsAction(bool channel) {
    _ctx_menu.active = false;
    _retry_menu_active = false;
    _participant_picker_active = false;
    (channel ? _fs : _dm_fs).active = false;
    startReply(channel);
  }

  void setupMsgPick() {
    _msg_sel = _msg_scroll = 0;
    _active_msg_count = 0;
    for (uint8_t i = 0; i < solo::QuickReplies::BUILTIN_COUNT; i++)
      _active_replies[_active_msg_count++] = i;
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      for (uint8_t i = 0; i < solo::QuickReplies::CUSTOM_COUNT; i++) {
        if (p->custom_msgs[i][0] != '\0')
          _active_replies[_active_msg_count++] = solo::QuickReplies::BUILTIN_COUNT + i;
      }
    }
  }

  const char* quickReplyText(uint8_t entry) const {
    if (entry < solo::QuickReplies::BUILTIN_COUNT)
      return solo::QuickReplies::builtin(entry);
    NodePrefs* p = _task->getNodePrefs();
    uint8_t slot = entry - solo::QuickReplies::BUILTIN_COUNT;
    return p && slot < solo::QuickReplies::CUSTOM_COUNT ? p->custom_msgs[slot] : "";
  }

  // Conversation controls deliberately avoid a selectable compose row: a short
  // Enter opens the editor. Hold Enter opens quick replies directly,
  // except when this transcript has a failed send and needs a resend action.
  int sendTextLimit() const {
    return (int)solo::MessageTextPolicy::limit(MAX_TEXT_LEN,
        _sending_to_channel ? the_mesh.getNodeName() : nullptr);
  }

  void beginCustomMessage(bool replying = false) {
    char initial[solo::MessageDraftStore::TEXT_CAPACITY] = "";
    uint8_t reply_prefix_len = 0;
    bool restored = false;
    if (!replying) {
      restored = _sending_to_channel
          ? _drafts.loadChannel((uint8_t)_sel_channel_idx, initial,
                                sizeof(initial), &reply_prefix_len)
          : _drafts.loadContact(_sel_contact.id.pub_key, initial,
                                sizeof(initial), &reply_prefix_len);
    }
    _reply_mode = replying || (restored && reply_prefix_len > 0);
    if (_reply_mode && restored) {
      memcpy(_reply_prefix, initial, reply_prefix_len);
      _reply_prefix[reply_prefix_len] = '\0';
    }
    _quick_msgs_bypassed = true;
    messageeditor::begin(*_kb, replying ? _reply_prefix : initial,
                         sendTextLimit(), &sensors);
    _phase = KEYBOARD;
  }

  void saveCurrentDraft() {
    uint8_t prefix_len = _reply_mode ? (uint8_t)strlen(_reply_prefix) : 0;
    const char* text = _kb->len > prefix_len ? _kb->buf : "";
    if (_sending_to_channel)
      _drafts.saveChannel((uint8_t)_sel_channel_idx, text, prefix_len);
    else
      _drafts.saveContact(_sel_contact.id.pub_key, text, prefix_len);
  }

  void clearCurrentDraft() {
    if (_sending_to_channel) _drafts.clearChannel((uint8_t)_sel_channel_idx);
    else _drafts.clearContact(_sel_contact.id.pub_key);
  }

  void beginQuickMessagePick() {
    _reply_mode = false;
    _quick_msgs_bypassed = false;
    setupMsgPick();
    _phase = MSG_PICK;
  }

  int collectRecentParticipants(bool channel) {
    _recent_participants.clear();
    if (channel) {
      int count = _history.histCountForChannel(_sel_channel_idx);
      for (int i = 0; i < count && _recent_participants.count() < RecentParticipants::MAX_PARTICIPANTS; i++) {
        int pos = _history.histEntryForChannel(_sel_channel_idx, i);
        if (pos < 0) continue;
        const char* text = _history.chAtPos(pos).text;
        const char* sep = strstr(text, ": ");
        if (sep) _recent_participants.add(text, (int)(sep - text));
      }
    } else if (_sel_contact.type == ADV_TYPE_ROOM) {
      int count = _history.dmHistCountForContact(_sel_contact.id.pub_key);
      for (int i = 0; i < count && _recent_participants.count() < RecentParticipants::MAX_PARTICIPANTS; i++) {
        int pos = _history.dmHistEntryForContact(_sel_contact.id.pub_key, i);
        if (pos < 0) continue;
        const DmHistEntry& e = _history.dmAtPos(pos);
        if (e.outgoing) continue;
        const char* sep = strstr(e.text, ": ");
        if (sep) _recent_participants.add(e.text, (int)(sep - e.text));
      }
    }
    return _recent_participants.count();
  }

  void openParticipantPicker() {
    _participant_picker_active = true;
    _ctx_menu.begin("Reply to", RecentParticipants::MAX_PARTICIPANTS);
    for (int i = 0; i < _recent_participants.count(); i++)
      _ctx_menu.addItem(_recent_participants.name(i));
  }

  void dispatchParticipantPicker(bool channel) {
    int selected = _ctx_menu.selectedIndex();
    _ctx_menu.active = false;
    _participant_picker_active = false;
    if (selected < 0 || selected >= _recent_participants.count() ||
        !buildReplyPrefixForName(_recent_participants.name(selected))) return;
    startReply(channel);
  }

  void beginTranscriptActions(bool channel) {
    int failed = channel ? _history.latestFailedChannel(_sel_channel_idx)
                         : _history.latestFailedDMForContact(_sel_contact.id.pub_key);
    bool group_conversation = channel || _sel_contact.type == ADV_TYPE_ROOM;
    int recent = group_conversation ? collectRecentParticipants(channel) : 0;
    if (!group_conversation && failed < 0) {
      _sending_to_channel = channel;
      beginQuickMessagePick();
      return;
    }
    _retry_hist_pos = failed;
    _retry_menu_active = true;
    _participant_picker_active = false;
    _transcript_act_n = 0;
    _ctx_menu.begin("Send options", 3);
    if (recent > 0) {
      _ctx_menu.addItem("Reply to...");
      _transcript_act[_transcript_act_n++] = TRANS_REPLY_TO;
    }
    if (failed >= 0) {
      _ctx_menu.addItem(channel ? "Resend anyway" : "Resend failed");
      _transcript_act[_transcript_act_n++] = TRANS_RESEND;
    }
    _ctx_menu.addItem("Quick replies");
    _transcript_act[_transcript_act_n++] = TRANS_QUICK;
  }

  void dispatchTranscriptAction(bool channel) {
    int selected = _ctx_menu.selectedIndex();
    TranscriptAct action = (TranscriptAct)_transcript_act[
        (selected >= 0 && selected < _transcript_act_n) ? selected : 0];
    _ctx_menu.active = false;
    _retry_menu_active = false;
    if (action == TRANS_REPLY_TO) {
      openParticipantPicker();
      return;
    }
    if (action == TRANS_QUICK) {
      _sending_to_channel = channel;
      beginQuickMessagePick();
      return;
    }

    bool ok = false;
    if (channel) {
      ChannelDetails ch;
      if (_retry_hist_pos >= 0 && the_mesh.getChannel(_sel_channel_idx, ch) &&
          (!_task->isChildModeLocked() ||
           channelAllowedForChild((uint8_t)_sel_channel_idx, ch))) {
        ChHistEntry& e = _history.chAtPos(_retry_hist_pos);
        if (e.ch_idx == (uint8_t)_sel_channel_idx && e.relay_status == ACK_FAIL) {
          const char* body = strncmp(e.text, "Me: ", 4) == 0 ? e.text + 4 : e.text;
          ok = the_mesh.sendGroupMessage(rtc_clock.getCurrentTime(), ch.channel,
                                         the_mesh.getNodeName(), body, strlen(body));
          if (ok) _history.armChannelRelay(_retry_hist_pos, the_mesh.lastChannelRelaySeq());
        }
      }
    } else {
      ContactInfo* current = the_mesh.lookupContactByPubKey(_sel_contact.id.pub_key, PUB_KEY_SIZE);
      uint8_t expected = _sel_contact.type == ADV_TYPE_ROOM ? ADV_TYPE_ROOM : ADV_TYPE_CHAT;
      if (current && solo::Policy::contactAllowed(_task->getNodePrefs(),
                                                  _task->isChildModeLocked(), current,
                                                  expected))
        ok = _history.resendFailedDM(_retry_hist_pos, _sel_contact.id.pub_key);
    }
    _retry_hist_pos = -1;
    if (!ok) _task->logFailure("Message", "Resend not queued");
    else _task->showAlert("Retrying...", 900);
  }

  void afterSend(bool ok, const char* msg) {
    _reply_mode = false;
    _share_mode = false;
    if (ok && _sending_to_channel) {
      _hist_sel = 0;
      _hist_scroll = 0;
      _channel_transcript.reset();
      _phase = CHANNEL_HIST;  // set before addChannelMsg so viewing=true, no unread bump
      char entry[sizeof(ChHistEntry::text)];
      snprintf(entry, sizeof(entry), "Me: %s", msg);
      int pos = addChannelMsg(_sel_channel_idx, entry);
      // Arm the "relayed into mesh" marker on this exact entry — MyMesh tracked
      // the flood it just originated and reports a heard repeater echo by seq.
      if (pos >= 0) _history.armChannelRelay(pos, the_mesh.lastChannelRelaySeq());
      // After inserting sent msg at index 0, the unread index range is stale.
      // User is active in this channel — treat as fully read.
      _history.setChUnread(_sel_channel_idx, 0);
      _unread_at_entry = 0;
      _viewing_max_seen = 0;
      _task->showAlert("Sent", 600);
    } else if (ok) {
      _history.storeDMMsg(_sel_contact.id.pub_key, true, msg, _last_ack_tag,
                          _last_ack_deadline_ms, _last_send_ts,
                          _last_send_route);
      _task->reconcileDMUnread();
      _dm_hist_sel = 0;
      _dm_hist_scroll = 0;
      _dm_transcript.reset();
      _phase = DM_HIST;
      _task->showAlert("Sent", 600);
    } else {
      if (_sending_to_channel) {
        _hist_sel = _hist_scroll = 0;
        _channel_transcript.reset();
        _phase = CHANNEL_HIST;
        char entry[sizeof(ChHistEntry::text)];
        snprintf(entry, sizeof(entry), "Me: %s", msg);
        int pos = addChannelMsg(_sel_channel_idx, entry);
        if (pos >= 0) _history.chAtPos(pos).relay_status = ACK_FAIL;
        _history.setChUnread(_sel_channel_idx, 0);
        _unread_at_entry = 0;
        _viewing_max_seen = 0;
      } else {
        _history.storeFailedDM(_sel_contact.id.pub_key, msg, _last_send_ts,
                               _last_send_route);
        _task->reconcileDMUnread();
        _dm_hist_sel = _dm_hist_scroll = 0;
        _dm_transcript.reset();
        _phase = DM_HIST;
      }
      _task->logFailure("Message", "Send not queued");
    }
  }

  bool sendText(const char* msg) {
    _last_ack_tag = 0;
    _last_ack_deadline_ms = 0;
    _last_send_ts = 0;
    _last_send_route = DELIVERY_ROUTE_NONE;
    if (strlen(msg) > (size_t)sendTextLimit()) return false;
    if (_sending_to_channel) {
      ChannelDetails ch;
      if (!the_mesh.getChannel(_sel_channel_idx, ch)) return false;
      if (_task->isChildModeLocked() &&
          !channelAllowedForChild((uint8_t)_sel_channel_idx, ch)) return false;
      return the_mesh.sendGroupMessage(rtc_clock.getCurrentTime(), ch.channel,
                                       the_mesh.getNodeName(), msg, strlen(msg));
    } else {
      // Paths and favourites may have changed since this transcript opened.
      ContactInfo* current = the_mesh.lookupContactByPubKey(_sel_contact.id.pub_key, PUB_KEY_SIZE);
      uint8_t expected = _sel_contact.type == ADV_TYPE_ROOM ? ADV_TYPE_ROOM : ADV_TYPE_CHAT;
      if (!current || !solo::Policy::contactAllowed(_task->getNodePrefs(),
                                                    _task->isChildModeLocked(), current,
                                                    expected))
        return false;
      _sel_contact = *current;
      uint32_t send_ts = rtc_clock.getCurrentTime();
      uint32_t expected_ack = 0, est_timeout = 0;
      _last_send_route = _sel_contact.out_path_len == OUT_PATH_UNKNOWN
                           ? DELIVERY_ROUTE_FLOOD
                           : (_sel_contact.out_path_len == 0
                                ? DELIVERY_ROUTE_DIRECT : DELIVERY_ROUTE_PATH);
      _last_send_ts = send_ts;
      bool ok = the_mesh.sendMessage(_sel_contact, send_ts, 0,
                                     msg, expected_ack, est_timeout) > 0;
      if (ok && expected_ack) {
        _last_ack_tag = expected_ack;
        // Generous margin over the base estimate so a slow multi-hop ACK isn't
        // prematurely shown as failed.
        _last_ack_deadline_ms = millis() + est_timeout + 4000;
      }
      return ok;
    }
  }

  void buildContactList() {
    NodePrefs* p = _task->getNodePrefs();
    ContactInfo c;
    int total = the_mesh.getNumContacts();
    _num_contacts = 0;
    bool show_all = _room_mode
        ? !(p && (p->room_fav_only || _task->isChildModeLocked()))
        :  (p && p->dm_show_all && !_task->isChildModeLocked());
    // Compact parallel sort keys avoid three int[MAX_CONTACTS] arrays on the
    // UI task's small stack.
    uint8_t counts[MAX_CONTACTS];
    uint8_t unreads[MAX_CONTACTS];
    uint8_t favourites[MAX_CONTACTS];
    for (int i = 0; i < total; i++) {
        if (!the_mesh.getContactByIdx(i, c)) continue;
        if (_room_mode) {
          if (c.type != ADV_TYPE_ROOM) continue;
        } else {
        bool has_dm_unread = _task->getDMUnread(c.id.pub_key) > 0;
        bool has_direct_dm = _task->hasDirectDMContact(c.id.pub_key);
        // A contact can advertise a different role after sending a DM. The
        // unread table is authoritative for that received conversation, so do
        // not strand it merely because the contact is no longer typed Chat.
        // Keep that proven DM entry after opening it clears the unread counter.
        // Favourite flags are intentionally not used here: rooms/repeaters can
        // also be favourited, but belong in their own pickers.
        if (c.type != ADV_TYPE_CHAT &&
            (_task->isChildModeLocked() || (!has_dm_unread && !has_direct_dm))) continue;
        }
        if (_task->isChildModeLocked() &&
            !solo::Policy::contactAllowed(p, true, &c,
                                          _room_mode ? ADV_TYPE_ROOM : ADV_TYPE_CHAT))
          continue;
        // The user-facing filter is authoritative: All shows every eligible DM
        // contact; Fav shows only upstream-starred eligible DM contacts.
        if (!show_all && !(c.flags & 0x01)) continue;
        counts[_num_contacts] = _room_mode ? 0 : _history.dmHistCountForContact(c.id.pub_key);
        unreads[_num_contacts] = _room_mode ? _task->getRoomUnread(c.id.pub_key)
                                            : _task->getDMUnread(c.id.pub_key);
        favourites[_num_contacts] = (c.flags & 0x01) ? 1 : 0;
        _sorted[_num_contacts++] = i;
    }
    // Favourites lead, then unread conversations, then most-used history.
    for (int i = 1; i < _num_contacts; i++) {
        uint16_t key = _sorted[i];
        int kc = counts[i], ku = unreads[i], kf = favourites[i];
        int j = i;
        while (j > 0 && (favourites[j-1] < kf ||
                         (favourites[j-1] == kf && unreads[j-1] < ku) ||
                         (favourites[j-1] == kf && unreads[j-1] == ku && counts[j-1] < kc))) {
          _sorted[j] = _sorted[j-1];
          counts[j] = counts[j-1];
          unreads[j] = unreads[j-1];
          favourites[j] = favourites[j-1];
          j--;
        }
        _sorted[j] = key; counts[j] = kc; unreads[j] = ku; favourites[j] = kf;
    }
  }

  bool channelAllowedForChild(uint8_t index, const ChannelDetails& ch) const {
    return solo::Policy::channelAllowed(_task->getNodePrefs(), _task->isChildModeLocked(),
                                        index, ch.name, ch.channel.secret);
  }

  bool channelAllowedForChild(uint8_t index) const {
    ChannelDetails ch;
    return the_mesh.getChannel(index, ch) && channelAllowedForChild(index, ch);
  }

  bool channelsModeVisible() const {
    NodePrefs* p = _task->getNodePrefs();
    return solo::Policy::channelsVisible(p, _task->isChildModeLocked());
  }
  bool roomsModeVisible() const {
    NodePrefs* p = _task->getNodePrefs();
    return !_task->isChildModeLocked() || (p && p->child_rooms_enabled);
  }

  int modeOptionCount() const {
    return 1 + (channelsModeVisible() ? 1 : 0) + (roomsModeVisible() ? 1 : 0);
  }
  int modeAtPosition(int pos) const {
    if (pos <= 0) return 0;
    if (channelsModeVisible()) return pos == 1 ? 1 : 2;
    return 2;
  }
  int modePosition() const {
    if (_mode_sel == 0) return 0;
    if (_mode_sel == 1) return channelsModeVisible() ? 1 : 0;
    if (!roomsModeVisible()) return 0;
    return channelsModeVisible() ? 2 : 1;
  }

  void buildChannelList() {
    NodePrefs* p = _task->getNodePrefs();
    bool child_locked = _task->isChildModeLocked();
    bool fav_only = (p && (p->ch_fav_only || child_locked));
    _num_channels = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      ChannelDetails ch;
      if (!the_mesh.getChannel(i, ch) || ch.name[0] == '\0') continue;
      if (fav_only && !(p->ch_fav_bitmask & (1ULL << i))) continue;
      if (child_locked && !channelAllowedForChild((uint8_t)i, ch)) continue;
      _channel_indices[_num_channels++] = (uint8_t)i;
    }
    // Stable partition: favourite channels first, preserving slot order.
    int front = 0;
    for (int i = 0; p && i < _num_channels; i++) {
      if (!(p->ch_fav_bitmask & (1ULL << _channel_indices[i]))) continue;
      uint8_t idx = _channel_indices[i];
      for (int j = i; j > front; j--) _channel_indices[j] = _channel_indices[j - 1];
      _channel_indices[front++] = idx;
    }
  }

  bool toggleContactFavourite(const ContactInfo& contact) {
    bool favourite = !(contact.flags & 0x01);
    if (!the_mesh.setContactFavourite(contact.id.pub_key, favourite)) return false;
    snprintf(_ctx_fav_item, sizeof(_ctx_fav_item), "Fav: %s", favourite ? "On" : "Off");
    return true;
  }

  // Returns per-channel notification state: 0=follow global, 1=off, 2=local in Auto
  // Per-contact/channel notification + melody overrides share two storage
  // shapes, so the eight accessors below are thin wrappers over two primitives:
  //
  //  • Channels — a 3-state packed into a pair of channel-index bitmasks: a
  //    "presence" mask (is there an override at all) + a "variant" mask. The two
  //    uses disagree on which state the variant bit means, so the caller passes
  //    the state value that corresponds to variant-set (v_set).
  //  • DMs — a small {prefix[4], value} table: find by 4-byte prefix, update or
  //    clear (clear frees the slot), else insert into the first free slot, else
  //    overwrite slot 0. value 0 == "no override" == empty slot.

  // Channel notif: state 1 = muted (variant bit set), 2 = force-on (variant clear).
  uint8_t chNotifState(uint8_t ch_idx) const {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::channelState(p, ch_idx);
  }
  bool setChNotifState(uint8_t ch_idx, uint8_t state) {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::setChannelState(p, ch_idx, state);
  }

  uint8_t dmNotifState(const uint8_t* pub_key) const {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::dmState(p, pub_key);
  }
  bool setDmNotifState(const uint8_t* pub_key, uint8_t state) {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::setDmState(p, pub_key, state);
  }

  // Melody override 0 follows the global sound; other values are catalogue ID + 1.
  static const char* melodyOverrideLabel(uint8_t value) {
    return value ? solo::BuiltinMelodies::label(value - 1) : "Global";
  }

  uint8_t chNotifMelody(uint8_t ch_idx) const {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::channelMelody(p, ch_idx);
  }
  bool setChNotifMelody(uint8_t ch_idx, uint8_t slot) {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::setChannelMelody(p, ch_idx, slot);
  }

  uint8_t dmMelodySlot(const uint8_t* pub_key) const {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::dmMelody(p, pub_key);
  }
  bool setDmMelody(const uint8_t* pub_key, uint8_t slot) {
    NodePrefs* p = _task->getNodePrefs();
    return solo::NotificationPreferences::setDmMelody(p, pub_key, slot);
  }

  // On-device channel Add/Edit form (Channels tab) — owned by this screen and
  // delegated to while active(), the same relationship WaypointsView has with
  // TrailScreen.
  ChannelsView _ch_view;

public:
  solo::PathAttemptSnapshot latestPathAttempt(const uint8_t* pub_key) const {
    return _history.latestPathAttempt(pub_key);
  }

  MessagesScreen(UITask* task, KeyboardWidget* kb)
    : _task(task), _kb(kb), _phase(MODE_SELECT), _mode_sel(0),
      _contact_sel(0), _contact_scroll(0), _num_contacts(0), _room_mode(false), _login_mode(false),
      _channel_sel(0), _channel_scroll(0), _num_channels(0),
      _sel_channel_idx(0), _sending_to_channel(false),
      _msg_sel(0), _msg_scroll(0), _active_msg_count(0),
      _hist_sel(0), _hist_scroll(0),
      _unread_at_entry(0), _viewing_max_seen(0),
      _dm_hist_sel(-1), _dm_hist_scroll(0),
      _ctx_dirty(false), _pin_picker_active(false), _reply_mode(false),
      _ch_view(task) {
    // The history rings + per-channel unread counters init in MessageHistory.
  }

  // A blank name is still an occupied slot when it has a channel key.
  int findFreeChannelSlot() const {
    return solo::ChannelSlotPolicy::firstFree<ChannelDetails>(the_mesh, MAX_GROUP_CHANNELS);
  }

  // CHANNEL_PICK row count including the synthetic "+ Add channel" row
  // (suppressed while picking a channel for the bot).
  int channelPickTotal() const {
    return _num_channels + ((_pick_bot_channel || _task->isChildModeLocked()) ? 0 : 1);
  }

  // Public entry points (routed from MyMesh / the bot via UITask) — thin
  // forwarders to the history store. addChannelMsg computes the "viewing" flag
  // (a phase-machine fact the store can't see) and returns the ring position so
  // the outgoing path can attach a relay seq to that exact entry.
  int addChannelMsg(uint8_t ch_idx, const char* text, uint32_t timestamp = 0,
                    bool count_unread = true) {
    bool visible_transcript = _task->isMessagesScreenVisible()
        && _phase == CHANNEL_HIST && _sel_channel_idx == (int)ch_idx;
    bool viewing = !count_unread || visible_transcript;
    int pos = _history.addChannelMsg(ch_idx, text, viewing, timestamp);
    // MessageTranscriptView notices the total line-count growth on its next
    // render and preserves an older reading position without this phase layer
    // having to estimate the new message's wrapped height.
    if (visible_transcript && pos >= 0) _channel_transcript.messageAdded();
    return pos;
  }
  void markChannelRelayed(uint32_t seq) { _history.markChannelRelayed(seq); }
  void markChannelRelayExpired(uint32_t seq) { _history.markChannelRelayExpired(seq); }
  void armChannelRelay(int pos, uint32_t seq) { _history.armChannelRelay(pos, seq); }
  void addAppDMMsg(const uint8_t* pub_key, const char* text, uint32_t timestamp,
                   uint8_t attempt, uint32_t ack_tag, uint32_t ack_deadline_ms,
                   uint8_t path_len) {
    uint8_t route = path_len == OUT_PATH_UNKNOWN ? DELIVERY_ROUTE_FLOOD
                    : (path_len == 0 ? DELIVERY_ROUTE_DIRECT : DELIVERY_ROUTE_PATH);
    bool viewing = isViewingContact(pub_key);
    bool added = _history.storeAppDM(pub_key, text, timestamp, attempt,
                                     ack_tag, ack_deadline_ms, route);
    if (viewing && added) _dm_transcript.messageAdded();
  }
  bool addDMMsg(const uint8_t* pub_key, bool outgoing, const char* text,
                uint32_t sender_timestamp = 0) {
    bool viewing = isViewingContact(pub_key);
    bool added = _history.addDMMsg(pub_key, outgoing, text, sender_timestamp);
    if (viewing && added) _dm_transcript.messageAdded();
    if (!outgoing && _phase == CONTACT_PICK && !_room_mode) {
      // The sender may have been discovered after this picker was built, or
      // may now advertise a different contact type. Refresh on receipt so the
      // badge and its openable row appear together without leaving/re-entering.
      buildContactList();
      _contact_sel = _contact_scroll = 0;
    }
    return added;
  }
  bool isViewingContact(const uint8_t* pub_key) const {
    return pub_key != nullptr && _task->isMessagesScreenVisible()
        && _phase == DM_HIST
        && memcmp(_sel_contact.id.pub_key, pub_key, 4) == 0;
  }
  bool markDmDelivered(uint32_t ack_crc, uint8_t* prefix = nullptr) {
    return _history.markDmDelivered(ack_crc, prefix);
  }

  bool isRoomLoggedIn(const uint8_t* pub_key) const {
    return _task->isRoomLoggedIn(pub_key);
  }

  bool startNodeLogin(const char* password, bool used_saved_password = false) {
    bool sent = _task->startNodeLogin(solo::NodeLoginCoordinator::MESSAGES,
                                      _sel_contact, password, used_saved_password);
    if (!sent)
      _task->logFailure("Room login", _task->nodeLoginBusy() ? "Login busy" : "Send failed");
    if (sent) _task->showAlert("Logging in...", 800);
    if (sent) _phase = ROOM_LOGIN_WAIT;
    return sent;
  }

  // Routed by the shared owner-aware coordinator, even if another screen has
  // become current since this attempt began.
  void onNodeLoginResult(const uint8_t* pub_key, bool success, uint8_t permissions) {
    (void)permissions;
    if (_phase != ROOM_LOGIN_WAIT || memcmp(_sel_contact.id.pub_key, pub_key, 4) != 0) return;
    if (success) {
      if (_pick_bot_room) { commitPickBotRoom(_sel_contact); return; }
      openDmHistory();
      if (_share_mode) beginShareCompose(false);
    } else {
      _task->logFailure("Room login", "Login rejected");
      _login_mode = true;
      _kb->begin("", 15);
      _kb->clearPlaceholders();
      _phase = KEYBOARD;
    }
    if (success) _task->showAlert("Login OK", 1200);
  }

  void onNodeLoginTimeout(const uint8_t* pub_key) {
    if (_phase != ROOM_LOGIN_WAIT || memcmp(_sel_contact.id.pub_key, pub_key, 4) != 0) return;
    _task->logFailure("Room login", "No reply");
    _login_mode = true;
    _kb->begin("", 15);
    _kb->clearPlaceholders();
    _phase = KEYBOARD;
  }

  // Open the message history for the currently selected contact/room and reset
  // the scroll/selection view state. Shared by the Enter-on-contact path and the
  // post-login auto-enter.
  void openDmHistory() {
    _task->clearDMUnread(_sel_contact.id.pub_key);
    if (_sel_contact.type == ADV_TYPE_ROOM)
      _task->clearRoomUnread(_sel_contact.id.pub_key);
    _dm_hist_sel = -1;
    _dm_hist_scroll = 0;
    _dm_transcript.reset();
    _dm_fs.active = false;
    _phase = DM_HIST;
  }

  // Background tick (called every UI loop, regardless of the active screen) that
  // drives auto-resend of on-device DMs — forwarded to the history store.
  uint8_t tickDmResends() { return _history.tickDmResends(); }
  void cancelDmResends() { _history.cancelDmResends(); }

  int getDMUnreadTotal() const {
    return _task->getDMUnreadTotal();
  }

  int getTotalChannelUnread(bool child_visible_only = false) const {
    if (!child_visible_only) return _history.getTotalChannelUnread();
    int total = 0;
    for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
      if (_history.chUnread(i) && channelAllowedForChild((uint8_t)i))
        total += _history.chUnread(i);
    }
    return total;
  }

  // How many DM ring entries this contact/room currently holds -- lets UITask
  // clamp its separate _dm_unread_table counters to what the shared 32-slot DM
  // ring actually still has, the same self-healing shape as the channel fix.
  int dmHistCountForContact(const uint8_t* pub_key) const { return _history.dmHistCountForContact(pub_key); }

  void markReadAlert(int n) {
    char msg[32];
    snprintf(msg, sizeof(msg), "%d marked read", n);
    _task->showAlert(msg, 800);
  }

  void clearAllChannelUnread() { _history.clearAllChannelUnread(); }

  // Update the viewing-session unread bookkeeping (UI state) and push the
  // resulting count down into the store. The ring lives in _history, but this
  // "what has the user seen on screen" logic is pure phase-machine state.
  void updateChannelUnread(int visible_oldest = -1) {
    if (_sel_channel_idx < 0 || _sel_channel_idx >= MAX_GROUP_CHANNELS) return;
    // histEntryForChannel is newest-first: index 0 = newest (unread), higher = older.
    // Count everything actually rendered on screen as seen — not just the
    // highlighted row — so a taller screen that fits more boxes at once marks
    // more read up front, instead of requiring a press per row.
    int seen_to = visible_oldest;
    if (seen_to < 0 && _fs.active) seen_to = _hist_sel;
    if (seen_to < 0) return;
    if (seen_to > _viewing_max_seen) _viewing_max_seen = seen_to;
    // Each step down from 0 sees one more message; seen count = max_seen + 1.
    int remaining = _unread_at_entry - (_viewing_max_seen + 1);
    _history.setChUnread(_sel_channel_idx, (uint8_t)(remaining > 0 ? remaining : 0));
  }

  void reset() {
    _path_view.active = false;
    _phase = MODE_SELECT;
    _mode_sel = 0;
    _contact_sel = _contact_scroll = 0;
    _msg_sel = _msg_scroll = 0;
    _channel_sel = _channel_scroll = 0;
    _sending_to_channel = false;

    _room_mode = false;
    _login_mode = false;
    buildContactList();
    buildChannelList();

    _ctx_menu.active = false;
    _retry_menu_active = false;
    _participant_picker_active = false;
    _ctx_dirty = false;
    _share_mode = false;
    _pick_target = false;
    _pick_bot_channel = false;
    _pick_bot_room = false;
    _pin_picker_active = false;
    _channel_delete_confirm_active = false;
    _entry_origin = ORIGIN_NORMAL;
    _quick_msgs_bypassed = false;
    _unread_at_entry = 0;
    _viewing_max_seen = 0;
    _ch_view.reset();
  }

  // Enter a recipient category selected directly on the Messages home page.
  // Back from the category picker returns home instead of exposing the legacy
  // MODE_SELECT landing screen, which remains available to share/picker flows.
  void enterCategory(uint8_t category) {
    reset();
    _entry_origin = ORIGIN_HOME_CATEGORY;
    _mode_sel = category <= 2 ? category : 0;
    if (_mode_sel == 1) {
      if (!channelsModeVisible()) _mode_sel = 0;
      else {
        buildChannelList();
        _channel_sel = _channel_scroll = 0;
        _phase = CHANNEL_PICK;
        return;
      }
    }
    _room_mode = (_mode_sel == 2);
    buildContactList();
    _contact_sel = _contact_scroll = 0;
    _phase = CONTACT_PICK;
  }

  // Recent DM contacts, newest first, deduped (forwarded to the history store).
  int getRecentDMContacts(uint8_t out[][NodePrefs::FAVOURITE_PREFIX_LEN], int max) const {
    return _history.getRecentDMContacts(out, max);
  }

  // Jump straight into a contact's DM history (used by the Favourites dial).
  // Caller must have already reset() the screen. Marks the entry so KEY_CANCEL
  // from DM_HIST returns to the home screen instead of falling back through
  // CONTACT_PICK → MODE_SELECT.
  // Enter the screen pre-loaded to share `text` (e.g. a "[WAY]lat,lon label"
  // waypoint). The user picks Direct/Channel then a recipient; selecting one
  // jumps straight to the keyboard prefilled with the text (see beginShareCompose).
  void startShare(const char* text) {
    reset();
    _share_mode = true;
    strncpy(_share_text, text, sizeof(_share_text) - 1);
    _share_text[sizeof(_share_text) - 1] = '\0';
    _phase = MODE_SELECT;
  }

  // Open the recipient chooser to set the live-share target (channel/DM). On
  // selection the target is stored in NodePrefs and the Map screen is restored.
  void startPickTarget() {
    reset();
    _pick_target = true;
    _phase = MODE_SELECT;
  }

  void commitPickTargetChannel(int ch_idx) {
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      p->loc_share_target_type = 0;
      p->loc_share_channel_idx = (uint8_t)ch_idx;
      the_mesh.savePrefs();
    }
    _pick_target = false;
    _task->showAlert("Share target set", 1200);
    _task->gotoLiveShareScreen();
  }

  // Open the channel chooser to set the auto-reply bot's channel. Skips
  // MODE_SELECT (the bot only ever targets a channel) and lands straight on
  // CHANNEL_PICK, browsing real channel names instead of a bare index cycle.
  void startPickBotChannel() {
    reset();
    _pick_bot_channel = true;
    _phase = CHANNEL_PICK;
  }

  void commitPickBotChannel(int ch_idx) {
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      p->bot_channel_enabled = 1;
      p->bot_channel_idx = (uint8_t)ch_idx;
      the_mesh.savePrefs();
    }
    _pick_bot_channel = false;
    _task->showAlert("Bot channel set", 1200);
    _task->gotoBotScreen();
  }

  // Open the room chooser to set the auto-reply bot's room. Enter routes
  // through the same login handling the normal room-open flow uses (see the
  // CONTACT_PICK/room_mode Enter handler below) — the bot can't ever post to
  // a room it has no password for, so picking one is a good moment to prompt.
  void startPickBotRoom() {
    reset();
    _pick_bot_room = true;
    _room_mode = true;
    buildContactList();   // rebuild now that _room_mode is on (reset() built the DM list)
    _contact_sel = _contact_scroll = 0;
    _phase = CONTACT_PICK;
  }

  void commitPickBotRoom(const ContactInfo& ci) {
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      p->bot_room_enabled = 1;
      memcpy(p->bot_room_prefix, ci.id.pub_key, NodePrefs::FAVOURITE_PREFIX_LEN);
      the_mesh.savePrefs();
    }
    _pick_bot_room = false;
    _room_mode = false;
    _task->showAlert("Bot room set", 1200);
    _task->gotoBotScreen();
  }

  void commitPickTargetDM(const ContactInfo& ci) {
    // A room server can't be a live-share target — a [LOC] DM to it is never
    // reposted to the room's members. Reject the pick and keep the chooser open.
    if (ci.type == ADV_TYPE_ROOM) {
      _task->logWarning("Room", "Not supported");
      return;
    }
    NodePrefs* p = _task->getNodePrefs();
    if (p) {
      p->loc_share_target_type = 1;
      memcpy(p->loc_share_dm_prefix, ci.id.pub_key, NodePrefs::FAVOURITE_PREFIX_LEN);
      the_mesh.savePrefs();
    }
    _pick_target = false;
    _task->showAlert("Share target set", 1200);
    _task->gotoLiveShareScreen();
  }

  void enterDM(const ContactInfo& ci) {
    _sel_contact = ci;
    _task->clearDMUnread(ci.id.pub_key);
    if (ci.type == ADV_TYPE_ROOM) _task->clearRoomUnread(ci.id.pub_key);
    _dm_hist_sel = -1;
    _dm_hist_scroll = 0;
    _dm_transcript.reset();
    _dm_fs.active = false;
    _room_mode = ci.type == ADV_TYPE_ROOM;
    _phase = DM_HIST;
    _entry_origin = ORIGIN_DIRECT_DM;
  }

  // Jump directly into a channel transcript from the Clock shortcut. The
  // caller resets the screen and validates that the channel is still allowed.
  void enterChannel(uint8_t channel_idx) {
    _sel_channel_idx = channel_idx;
    _unread_at_entry = (int)_history.chUnread(channel_idx);
    _hist_scroll = 0;
    int count = _history.histCountForChannel(channel_idx);
    _hist_sel = count > 0 ? 0 : -1;
    _viewing_max_seen = -1;
    _channel_transcript.reset();
    _fs.active = false;
    _phase = CHANNEL_HIST;
    _entry_origin = ORIGIN_DIRECT_CHANNEL;
  }

  int channelHistCount(uint8_t channel_idx) const {
    return _history.histCountForChannel(channel_idx);
  }

  uint8_t channelUnread(uint8_t channel_idx) const {
    return _history.chUnread(channel_idx);
  }

  uint32_t latestChannelActivity(uint8_t channel_idx) const {
    return _history.latestChannelActivity(channel_idx);
  }

  uint32_t latestDmActivity(const uint8_t* pub_key, bool incoming_only = false) const {
    return _history.latestDmActivity(pub_key, incoming_only);
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    if (_path_view.active && _task->isChildModeLocked()) _path_view.active = false;
    if (_path_view.active) {
      _path_view.setAttempt(_history.latestPathAttempt(_path_view.key()));
      return _path_view.render(display);
    }

    // Channel Add/Edit form owns the screen while active.
    if (_ch_view.active()) return _ch_view.render(display);

    // Navigate-to-location view sits over everything else while active.

    int lh      = display.getLineHeight();
    int item_h  = display.lineStep();
    int start_y = display.listStart();

    if (_phase == MODE_SELECT) {
      display.drawCenteredHeader("Messages", true, _ctx_menu.active);
      const char* opts[] = { "Direct message", "Channels", "Room Servers" };
      int badges[3] = {
        getDMUnreadTotal(),
        _task->getChannelUnreadCount(),
        _task->getRoomUnreadCount()
      };
      int option_count = modeOptionCount();
      for (int pos = 0; pos < option_count; pos++) {
        int mode = modeAtPosition(pos);
        int y = start_y + pos * item_h;
        bool sel = (mode == _mode_sel);
        display.drawSelectionRow(0, y - 1, display.width(), item_h - 1, sel);
        display.setCursor(2, y);
        display.print(opts[mode]);
        if (badges[mode] > 0)
          display.drawUnreadBadge(display.width() - 1, y, badges[mode], sel);
      }
      display.setColor(DisplayDriver::LIGHT);
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == CONTACT_PICK) {
      display.drawCenteredHeader(_room_mode ? "Select Room" : "Select Contact", true, _ctx_menu.active);

      if (_num_contacts == 0) {
        display.drawTextCentered(display.width()/2, display.height()/2, _room_mode ? "No room servers" : "No favourites");
        return 5000;
      }

      drawList(display, _num_contacts, _contact_sel, _contact_scroll, [&](int list_idx, int y, bool sel, int reserve) {
        int mesh_idx = _sorted[list_idx];

        drawRowSelection(display, y, sel, reserve);

        ContactInfo c;
        if (the_mesh.getContactByIdx(mesh_idx, c)) {
          uint8_t dm_unread = _room_mode ? _task->getRoomUnread(c.id.pub_key)
                                         : _task->getDMUnread(c.id.pub_key);
          int bw = dm_unread > 0 ? display.unreadBadgeWidth(dm_unread) + 2 : 0;
          int sw = (c.flags & 0x01) ? favStarWidth(display) : 0;
          display.drawTextEllipsized(2, y, display.width() - 2 - bw - sw - reserve,
                                     c.name, sel && !_ctx_menu.active);
          if (sw) drawFavStar(display, display.width() - reserve - bw - sw, y);
          if (dm_unread > 0)
            display.drawUnreadBadge(display.width() - reserve, y, dm_unread, sel);
        }
      });

      // Context menu overlay
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == ROOM_LOGIN_WAIT) {
      display.drawCenteredHeader("Room Login");
      display.drawTextEllipsized(2, start_y, display.width() - 4, _sel_contact.name);
      display.drawTextCentered(display.width() / 2, start_y + item_h * 2, "Logging in...");

    } else if (_phase == CHANNEL_PICK) {
      display.drawCenteredHeader("Select Channel", true, _ctx_menu.active);

      // "+ Add channel" is a synthetic trailing row — suppressed while picking
      // a channel for the bot, so that picker's list stays unchanged.
      bool show_add = !_pick_bot_channel && !_task->isChildModeLocked();
      int total = _num_channels + (show_add ? 1 : 0);

      if (total == 0) {
        display.drawTextCentered(display.width()/2, display.height()/2, "No channels");
        return 5000;
      }

      drawList(display, total, _channel_sel, _channel_scroll, [&](int list_idx, int y, bool sel, int reserve) {
        drawRowSelection(display, y, sel, reserve);
        if (list_idx == _num_channels) {                 // the synthetic "Add" row
          display.setCursor(2, y); display.print("+ Add channel");
          display.setColor(DisplayDriver::LIGHT);
          return;
        }
        ChannelDetails ch;
        if (the_mesh.getChannel(_channel_indices[list_idx], ch)) {
          uint8_t unread = _history.chUnread(_channel_indices[list_idx]);
          int bw = unread > 0 ? display.unreadBadgeWidth(unread) + 2 : 0;
          bool favourite = _task->getNodePrefs() &&
              (_task->getNodePrefs()->ch_fav_bitmask & (1ULL << _channel_indices[list_idx]));
          int sw = favourite ? favStarWidth(display) : 0;
          display.drawTextEllipsized(2, y, display.width() - 4 - bw - sw - reserve,
                                     ch.name, sel && !_ctx_menu.active);
          if (sw) drawFavStar(display, display.width() - reserve - bw - sw, y);
          if (unread > 0)
            display.drawUnreadBadge(display.width() - reserve, y, unread, sel);
        }
      });

      // Context menu overlay
      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == DM_HIST) {
      display.setTextSize(1);
      const char* contact_name = _sel_contact.name;
      if (_dm_fs.active && _dm_hist_sel >= 0) {
        int ring_pos = _history.dmHistEntryForContact(_sel_contact.id.pub_key, _dm_hist_sel);
        if (ring_pos >= 0) {
          const DmHistEntry& e = _history.dmAtPos(ring_pos);
          char sender_buf[33];
          // No skipReplyPrefix() here -- _dm_fs.render() parses "@[nick] " itself
          // (for the "To:" header); stripping it here first would hide it there.
          const char* body = dmDisplayParts(e, _sel_contact.type == ADV_TYPE_ROOM,
                                            contact_name, sender_buf, sizeof(sender_buf));
          const char* sender = sender_buf;
          int dm_count = _history.dmHistCountForContact(_sel_contact.id.pub_key);
          int ret = _dm_fs.render(display, sender, body,
                                  _dm_hist_sel < dm_count - 1,
                                  _dm_hist_sel > 0);
          if (e.outgoing) {  // delivery marker in the (inverted) header bar
            display.setColor(DisplayDriver::DARK);
            drawDeliveryMarker(display, 2 + display.getTextWidth(sender) + 3, 1,
                               _history.dmEffectiveStatus(e), e.delivery_route,
                               e.attempt + 1);
            display.setColor(DisplayDriver::LIGHT);
          }
          if (_ctx_menu.active) _ctx_menu.render(display);
          return ret;
        }
        return 500;
      }

      int hist_start_y = display.headerH();

      char title[40];
      snprintf(title, sizeof(title), "%s", contact_name);
      display.drawCenteredHeader(title, true, _ctx_menu.active);

      int dm_count = _history.dmHistCountForContact(_sel_contact.id.pub_key);
      uint32_t now_ts = rtc_clock.getCurrentTime();
      bool is_room = (_sel_contact.type == ADV_TYPE_ROOM);

      TranscriptRenderResult tr = _dm_transcript.render(
          display, dm_count, hist_start_y,
          [&](int item, TranscriptMessage& msg) -> bool {
            int ring_pos = _history.dmHistEntryForContact(_sel_contact.id.pub_key, item);
            if (ring_pos < 0) return false;
            const DmHistEntry& e = _history.dmAtPos(ring_pos);
            char sender[33];
            const char* raw_body = dmDisplayParts(e, is_room, contact_name, sender, sizeof(sender));
            const char* body = msgReplyBody(raw_body, msg.reply_to, sizeof(msg.reply_to));
            strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
            msg.sender[sizeof(msg.sender) - 1] = '\0';
            strncpy(msg.body, body, sizeof(msg.body) - 1);
            msg.body[sizeof(msg.body) - 1] = '\0';
            geo::fmtAgeShort(msg.age, sizeof(msg.age), now_ts, e.timestamp);
            msg.delivery = e.outgoing ? (uint8_t)_history.dmEffectiveStatus(e) : 0;
            msg.route = e.outgoing ? e.delivery_route : DELIVERY_ROUTE_NONE;
            msg.sends = e.attempt + 1;
            return true;
          });
      _hist_visible = tr.visible_count;
      // Rendering is the point at which a sleeping transcript becomes viewed
      // again. Messages received while the panel was off remain unread until
      // this first visible frame.
      _task->clearDMUnread(_sel_contact.id.pub_key);
      if (is_room) _task->clearRoomUnread(_sel_contact.id.pub_key);

      if (dm_count == 0) {
        display.setColor(DisplayDriver::LIGHT);
        display.drawTextCentered(display.width()/2, display.height()/2, "No messages yet");
      }

      if (_ctx_menu.active) _ctx_menu.render(display);
      return dm_count > 0 ? 500 : 2000;

    } else if (_phase == CHANNEL_HIST) {
      if (_fs.active && _hist_sel >= 0) {
        int fs_hist_count = _history.histCountForChannel(_sel_channel_idx);
        int ring_pos = _history.histEntryForChannel(_sel_channel_idx, _hist_sel);
        if (ring_pos >= 0) {
          const char* ftext = _history.chAtPos(ring_pos).text;
          const char* fsep = strstr(ftext, ": ");
          char fsender[33], fmsg[MSG_TEXT_BUF];
          if (fsep) {
            int nl = fsep - ftext; if (nl > (int)sizeof(fsender) - 1) nl = sizeof(fsender) - 1;
            strncpy(fsender, ftext, nl); fsender[nl] = '\0';
            strncpy(fmsg, fsep + 2, sizeof(fmsg) - 1); fmsg[sizeof(fmsg)-1] = '\0';
          } else {
            strcpy(fsender, "?");
            strncpy(fmsg, ftext, sizeof(fmsg) - 1); fmsg[sizeof(fmsg)-1] = '\0';
          }
          int ret = _fs.render(display, fsender, fmsg,
                               _hist_sel < fs_hist_count - 1,
                               _hist_sel > 0);
          if (strcmp(fsender, "Me") == 0 && _history.chAtPos(ring_pos).relay_status != ACK_NONE) {
            display.setColor(DisplayDriver::DARK);
            drawDeliveryMarker(display, 2 + display.getTextWidth(fsender) + 3, 1,
                               _history.chAtPos(ring_pos).relay_status,
                               DELIVERY_ROUTE_RELAY,
                               _history.chAtPos(ring_pos).relay_count);
            display.setColor(DisplayDriver::LIGHT);
          }
          if (_ctx_menu.active) _ctx_menu.render(display);
          return ret;
        }
        return 2000;
      }

      int hist_start_y = display.headerH();

      ChannelDetails ch;
      the_mesh.getChannel(_sel_channel_idx, ch);
      char title[24];
      snprintf(title, sizeof(title), "%.23s", ch.name);
      display.drawCenteredHeader(title, true, _ctx_menu.active);

      int ch_hist_count = _history.histCountForChannel(_sel_channel_idx);
      uint32_t now_ts = rtc_clock.getCurrentTime();

      TranscriptRenderResult tr = _channel_transcript.render(
          display, ch_hist_count, hist_start_y,
          [&](int item, TranscriptMessage& msg) -> bool {
            int ring_pos = _history.histEntryForChannel(_sel_channel_idx, item);
            if (ring_pos < 0) return false;
            const ChHistEntry& e = _history.chAtPos(ring_pos);
            const char* sep = strstr(e.text, ": ");
            if (sep) {
              int n = (int)(sep - e.text);
              if (n > (int)sizeof(msg.sender) - 1) n = sizeof(msg.sender) - 1;
              memcpy(msg.sender, e.text, n);
              msg.sender[n] = '\0';
            } else {
              strcpy(msg.sender, "?");
            }
            const char* body = msgReplyBody(sep ? sep + 2 : e.text,
                                            msg.reply_to, sizeof(msg.reply_to));
            strncpy(msg.body, body, sizeof(msg.body) - 1);
            msg.body[sizeof(msg.body) - 1] = '\0';
            geo::fmtAgeShort(msg.age, sizeof(msg.age), now_ts, e.timestamp);
            bool outgoing = strcmp(msg.sender, "Me") == 0;
            msg.delivery = outgoing ? e.relay_status : ACK_NONE;
            msg.route = outgoing ? DELIVERY_ROUTE_RELAY : DELIVERY_ROUTE_NONE;
            msg.sends = e.relay_count;
            return true;
          });
      _hist_visible = tr.visible_count;
      updateChannelUnread(tr.oldest_visible);

      if (ch_hist_count == 0) {
        display.setColor(DisplayDriver::LIGHT);
        display.drawTextCentered(display.width()/2, display.height()/2, "No messages yet");
      }

      if (_ctx_menu.active) _ctx_menu.render(display);

    } else if (_phase == KEYBOARD) {
      return _kb->render(display);

    } else { // MSG_PICK
      char title[24];
      if (_reply_mode) {
        int rlen = (int)strlen(_reply_prefix) - 4; // exclude "@[" and "] "
        if (rlen < 0) rlen = 0;
        if (rlen > 20) rlen = 20;
        char nick_raw[32];
        snprintf(nick_raw, sizeof(nick_raw), "%.*s", rlen, _reply_prefix + 2);
        snprintf(title, sizeof(title), "RE:%s", nick_raw);
      } else if (_sending_to_channel) {
        ChannelDetails ch;
        the_mesh.getChannel(_sel_channel_idx, ch);
        snprintf(title, sizeof(title), "%s", ch.name);
      } else {
        snprintf(title, sizeof(title), "TO:%s", _sel_contact.name);
      }
      display.drawCenteredHeader(title);

      int total_msg_items = _active_msg_count;
      if (total_msg_items == 0) {
        display.drawTextCentered(display.width() / 2, display.height() / 2,
                                 "No quick replies");
        return 2000;
      }
      drawList(display, total_msg_items, _msg_sel, _msg_scroll, [&](int idx, int y, bool sel, int reserve) {
        drawRowSelection(display, y, sel, reserve);
        const char* tmpl = quickReplyText(_active_replies[idx]);
        display.drawTextEllipsized(2, y, display.width() - 4 - reserve, tmpl, sel);
      });
    }
    return 2000;
  }

  bool handleInput(char c) override {
    if (_path_view.active && _task->isChildModeLocked()) _path_view.active = false;
    if (_path_view.active) return _path_view.handleInput(c);
    // Channel Add/Edit form consumes all input while active.
    if (_ch_view.active()) return _ch_view.handleInput(c);

    // Navigate view: Back or Enter returns to the message it was opened from.
    if (_phase == MODE_SELECT) {
      // Context menu (Mark-all-read) takes precedence while active.
      if (_ctx_menu.active) {
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          int cleared = 0;
          if (_mode_sel == 0) {
            cleared = getDMUnreadTotal();
            _task->clearAllDMUnread();
          } else if (_mode_sel == 1) {
            cleared = _task->getChannelUnreadCount();
            clearAllChannelUnread();
          } else {
            cleared = _task->getRoomUnreadCount();
            _task->clearRoomUnread();
          }
          markReadAlert(cleared);
        }
        return true;
      }
      if (c == KEY_CANCEL) { _task->gotoHomeScreen(); return true; }
      if (c == KEY_UP || c == KEY_DOWN) {
        int count = modeOptionCount();
        int pos = modePosition();
        pos = c == KEY_UP ? (pos > 0 ? pos - 1 : count - 1)
                          : (pos < count - 1 ? pos + 1 : 0);
        _mode_sel = modeAtPosition(pos);
        return true;
      }
      if (c == KEY_CONTEXT_MENU && !_task->isChildModeLocked()) {
        // PopupMenu stores the title pointer verbatim — use static strings.
        static const char* MODE_TITLES[] = { "DM options", "Channel options", "Room options" };
        _ctx_menu.begin(MODE_TITLES[_mode_sel < 3 ? _mode_sel : 0], 1);
        _ctx_menu.addItem("Mark all read");
        return true;
      }
      if (c == KEY_ENTER) {
        if (_mode_sel == 1) {
          if (!channelsModeVisible()) return true;
          buildChannelList();
          _channel_sel = _channel_scroll = 0;
          _phase = CHANNEL_PICK;
        } else {
          _room_mode = (_mode_sel == 2);
          buildContactList();
          _contact_sel = _contact_scroll = 0;
          _phase = CONTACT_PICK;
        }
        return true;
      }

    } else if (_phase == CONTACT_PICK) {
      // Context menu consumes all input while open
      if (_ctx_menu.active) {
        if (_room_mode) {
          int favourite_row = _ctx_menu.count() - 1;
          if ((keyIsPrev(c) || keyIsNext(c)) && _ctx_menu.selectedIndex() == favourite_row) {
            ContactInfo ci;
            if (_num_contacts > 0 && the_mesh.getContactByIdx(_sorted[_contact_sel], ci))
              toggleContactFavourite(ci);
            return true;
          }
          auto res = _ctx_menu.handleInput(c);
          if (res == PopupMenu::VALUE_NEXT && _ctx_menu.selectedIndex() == favourite_row) {
            ContactInfo ci;
            if (_num_contacts > 0 && the_mesh.getContactByIdx(_sorted[_contact_sel], ci))
              toggleContactFavourite(ci);
            return true;
          }
          if (res == PopupMenu::SELECTED && _num_contacts > 0) {
            if (the_mesh.getContactByIdx(_sorted[_contact_sel], _sel_contact)) {
              int sel = _ctx_menu.selectedIndex();
              if (sel == 0) {
                _login_mode = true;
                _kb->begin("", 15); // room/repeater password: max 15 chars
                _kb->clearPlaceholders();   // message placeholders are not valid in a password
                _phase = KEYBOARD;
              } else if (sel < favourite_row) {
                // Logout: only reachable when isRoomLoggedIn() added this item.
                _task->logoutRoom(_sel_contact.id.pub_key);
                _task->showAlert("Logged out", 1000);
              }
            }
          }
          if (res != PopupMenu::NONE && _phase == CONTACT_PICK) {
            buildContactList();
            if (_contact_sel >= _num_contacts)
              _contact_sel = _num_contacts > 0 ? _num_contacts - 1 : 0;
          }
          return true;
        }
        // LEFT/RIGHT cycle Notif/Melody/Favourite in place.
        if (!_pin_picker_active && _num_contacts > 0) {
          bool left  = keyIsPrev(c);
          int value_sel = _ctx_menu.selectedIndex();
          if (c == KEY_ENTER && value_sel == 2) {
            ContactInfo ci;
            NodePrefs* p = _task->getNodePrefs();
            if (p && the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
              uint8_t selection = solo::BuiltinMelodies::resolveOverride(
                  dmMelodySlot(ci.id.pub_key), p->notif_melody_dm);
              _task->previewMelody(selection, solo::BuiltinMelodies::MESSAGE);
            }
            return true;
          }
          bool right = keyIsNext(c) || (c == KEY_ENTER && (value_sel == 1 || value_sel == 3));
          if (left || right) {
            static const char* NOTIF_LABELS[] = { "Default", "Off", "Local" };
            ContactInfo ci;
            if (the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
              int sel = _ctx_menu.selectedIndex();
              if (sel == 1) {
                uint8_t v = dmNotifState(ci.id.pub_key);
                v = right ? (v + 1) % 3 : (v + 2) % 3;
                if (!setDmNotifState(ci.id.pub_key, v)) {
                  _task->logWarning("Ringtone", "Override list full");
                  return true;
                }
                snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s", NOTIF_LABELS[v]);
                _ctx_dirty = true;
              } else if (sel == 2) {
                uint8_t v = dmMelodySlot(ci.id.pub_key);
                v = right ? (v + 1) % (solo::BuiltinMelodies::COUNT + 1)
                          : (v + solo::BuiltinMelodies::COUNT) % (solo::BuiltinMelodies::COUNT + 1);
                if (!setDmMelody(ci.id.pub_key, v)) {
                  _task->logWarning("Ringtone", "Override list full");
                  return true;
                }
                snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s", melodyOverrideLabel(v));
                _ctx_dirty = true;
              } else if (sel == 3) {
                toggleContactFavourite(ci);
              }
            }
            return true;
          }
        }
        auto res = _ctx_menu.handleInput(c);
        if (_pin_picker_active) {
          // Slot picker sub-menu maps directly to the four visible dial slots.
          if (res == PopupMenu::SELECTED && _num_contacts > 0) {
            ContactInfo ci;
            if (the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
              int slot = _ctx_menu.selectedIndex();
              if (_task->setFavouriteSlot(slot, ci.id.pub_key)) {
                the_mesh.savePrefs();
                char alert[24];
                snprintf(alert, sizeof(alert), "Pinned to slot %d", slot + 1);
                _task->showAlert(alert, 800);
              }
            }
          }
          if (res != PopupMenu::NONE) _pin_picker_active = false;
          return true;
        }
        if (res == PopupMenu::SELECTED && _num_contacts > 0) {
          ContactInfo ci;
          if (the_mesh.getContactByIdx(_sorted[_contact_sel], ci)) {
            int sel = _ctx_menu.selectedIndex();
            if (sel == 0) {
              int cleared = (int)_task->getDMUnread(ci.id.pub_key);
              _task->clearDMUnread(ci.id.pub_key);
              markReadAlert(cleared);
            } else if (sel == 5) {
              _path_view.open(ci.id.pub_key, _history.latestPathAttempt(ci.id.pub_key));
            } else if (sel == 4) {
              // Pin / Unpin
              int pinned_slot = _task->findFavouriteSlot(ci.id.pub_key);
              if (pinned_slot >= 0) {
                _task->clearFavouriteSlot(pinned_slot);
                the_mesh.savePrefs();
                char alert[24];
                snprintf(alert, sizeof(alert), "Unpinned (slot %d)", pinned_slot + 1);
                _task->showAlert(alert, 800);
              } else {
                for (int s = 0; s < NodePrefs::FAVOURITES_DIAL_COUNT; s++) {
                  if (_task->isFavouriteSlotEmpty(s)) {
                    snprintf(_pin_slot_labels[s], sizeof(_pin_slot_labels[s]), "Slot %d: empty", s + 1);
                  } else {
                    char nm[32] = "?"; // ContactInfo::name, retained as UTF-8
                    NodePrefs* p = _task->getNodePrefs();
                    if (p) {
                      const uint8_t* pfx = p->favourite_contacts[s];
                      for (int idx = 0; ; idx++) {
                        ContactInfo c2;
                        if (!the_mesh.getContactByIdx(idx, c2)) break;
                        if (memcmp(c2.id.pub_key, pfx, NodePrefs::FAVOURITE_PREFIX_LEN) == 0) {
                          snprintf(nm, sizeof(nm), "%s", c2.name);
                          break;
                        }
                      }
                    }
                    snprintf(_pin_slot_labels[s], sizeof(_pin_slot_labels[s]), "Slot %d: %s", s + 1, nm);
                  }
                }
                _ctx_menu.begin("Pick slot", 3);
                for (int s = 0; s < NodePrefs::FAVOURITES_DIAL_COUNT; s++) _ctx_menu.addItem(_pin_slot_labels[s]);
                _pin_picker_active = true;
              }
            }
            // Value rows are handled in place by Left/Right/Enter above.
          }
        }
        if (res != PopupMenu::NONE) {
          _task->savePrefsIfDirty(_ctx_dirty);
          if (!_pin_picker_active) {
            buildContactList();
            if (_contact_sel >= _num_contacts)
              _contact_sel = _num_contacts > 0 ? _num_contacts - 1 : 0;
          }
        }
        return true;
      }
      if (c == KEY_CANCEL) {
        if (_pick_bot_room) { _pick_bot_room = false; _room_mode = false; _task->gotoBotScreen(); return true; }
        if (_entry_origin == ORIGIN_HOME_CATEGORY) { _entry_origin = ORIGIN_NORMAL; _task->gotoHomeScreen(); return true; }
        _room_mode = false;
        _phase = MODE_SELECT;
        return true;
      }
      // drawList() reclamps _contact_scroll from _contact_sel every render.
      if (c == KEY_UP   && _num_contacts > 0) { _contact_sel = (_contact_sel > 0) ? _contact_sel - 1 : _num_contacts - 1; return true; }
      if (c == KEY_DOWN && _num_contacts > 0) { _contact_sel = (_contact_sel < _num_contacts - 1) ? _contact_sel + 1 : 0; return true; }
      if (c == KEY_ENTER && _num_contacts > 0) {
        if (the_mesh.getContactByIdx(_sorted[_contact_sel], _sel_contact)) {
          if (_pick_target) { commitPickTargetDM(_sel_contact); return true; }
          if (_pick_bot_room) {
            if (!isRoomLoggedIn(_sel_contact.id.pub_key)) {
              char saved_pw[16];
              if (the_mesh.getRoomPassword(_sel_contact.id.pub_key, saved_pw, sizeof(saved_pw))) {
                // Known password, just not re-established this boot — retry
                // silently in the background; the bot target is set below
                // regardless of this attempt's outcome (self-heals like any
                // other saved room password would on the next real open).
                startNodeLogin(saved_pw, true);
              } else {
                // Never logged in — the bot could never post here without a
                // password, so prompt for one now instead of picking an
                // unusable target. Commits once submitted (see the KEYBOARD/
                // _login_mode handler), whether or not login itself succeeds.
                _login_mode = true;
                _kb->begin("", 15); // room/repeater password: max 15 chars
                _kb->clearPlaceholders();
                _phase = KEYBOARD;
                return true;
              }
            }
            if (!isRoomLoggedIn(_sel_contact.id.pub_key)) return true;
            commitPickBotRoom(_sel_contact);
            return true;
          }
          if (_room_mode && !isRoomLoggedIn(_sel_contact.id.pub_key)) {
            // Posting to a room requires a login handshake first (even with a
            // blank password) — go straight to the password prompt instead of
            // a history view that would silently fail to send.
            char saved_pw[16];
            if (the_mesh.getRoomPassword(_sel_contact.id.pub_key, saved_pw, sizeof(saved_pw))) {
              // Logged in to this room before, on an earlier boot -- retry
              // with the remembered password instead of prompting again.
              startNodeLogin(saved_pw, true);
            } else {
              _login_mode = true;
              _kb->begin("", 15); // room/repeater password: max 15 chars
              _kb->clearPlaceholders();   // message placeholders are not valid in a password
              _phase = KEYBOARD;
            }
            return true;
          }
          openDmHistory();
          if (_share_mode) beginShareCompose(false);
        }
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _num_contacts > 0 && _room_mode && !_task->isChildModeLocked()) {
        ContactInfo ci;
        bool logged_in = the_mesh.getContactByIdx(_sorted[_contact_sel], ci) && isRoomLoggedIn(ci.id.pub_key);
        snprintf(_ctx_fav_item, sizeof(_ctx_fav_item), "Fav: %s", (ci.flags & 0x01) ? "On" : "Off");
        _ctx_menu.begin("Room options", logged_in ? 3 : 2);
        _ctx_menu.addItem("Login...");
        if (logged_in) _ctx_menu.addItem("Logout");
        _ctx_menu.addValueItem(_ctx_fav_item);
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _num_contacts > 0 && !_room_mode && !_task->isChildModeLocked()) {
        static const char* NOTIF_LABELS[] = { "Default", "Off", "Local" };
        ContactInfo ci;
        the_mesh.getContactByIdx(_sorted[_contact_sel], ci);
        snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s",
                 NOTIF_LABELS[dmNotifState(ci.id.pub_key)]);
        snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s",
                 melodyOverrideLabel(dmMelodySlot(ci.id.pub_key)));
        int pinned_slot = _task->findFavouriteSlot(ci.id.pub_key);
        if (pinned_slot >= 0) snprintf(_ctx_pin_item, sizeof(_ctx_pin_item), "Unpin (slot %d)", pinned_slot + 1);
        else                  snprintf(_ctx_pin_item, sizeof(_ctx_pin_item), "Pin to dial");
        snprintf(_ctx_fav_item, sizeof(_ctx_fav_item), "Fav: %s", (ci.flags & 0x01) ? "On" : "Off");
        _ctx_menu.begin("Contact options", 6);
        _ctx_menu.addItem("Mark as read");
        _ctx_menu.addValueItem(_ctx_notif_item);
        _ctx_menu.addValueItem(_ctx_melody_item);
        _ctx_menu.addValueItem(_ctx_fav_item);
        _ctx_menu.addItem(_ctx_pin_item);
        _ctx_menu.addItem("Path details");
        _ctx_dirty = false;
        return true;
      }

    } else if (_phase == CHANNEL_PICK) {
      // Context menu consumes all input while open
      if (_ctx_menu.active) {
        // LEFT/RIGHT cycle Notif/Melody/Fav in-place (menu stays open).
        if (!_channel_delete_confirm_active && _num_channels > 0) {
          bool left  = keyIsPrev(c);
          int value_sel = _ctx_menu.selectedIndex();
          if (c == KEY_ENTER && value_sel == 2) {
            NodePrefs* p = _task->getNodePrefs();
            if (p) {
              uint8_t selection = solo::BuiltinMelodies::resolveOverride(
                  chNotifMelody(_ctx_ch_idx), p->notif_melody_ch);
              _task->previewMelody(selection, solo::BuiltinMelodies::KERPLOP);
            }
            return true;
          }
          bool right = keyIsNext(c) ||
                       (c == KEY_ENTER && (value_sel == 1 || value_sel == 3));
          if (left || right) {
            static const char* NOTIF_LABELS[] = { "Default", "Off", "Local" };
            uint8_t ch_idx = _ctx_ch_idx;   // frozen at menu open — see declaration
            int sel = _ctx_menu.selectedIndex();
            if (sel == 1) {
              uint8_t v = chNotifState(ch_idx);
              v = right ? (v + 1) % 3 : (v + 2) % 3;
              setChNotifState(ch_idx, v);
              snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s", NOTIF_LABELS[v]);
              _ctx_dirty = true;
            } else if (sel == 2) {
              uint8_t v = chNotifMelody(ch_idx);
              v = right ? (v + 1) % (solo::BuiltinMelodies::COUNT + 1)
                        : (v + solo::BuiltinMelodies::COUNT) % (solo::BuiltinMelodies::COUNT + 1);
              setChNotifMelody(ch_idx, v);
              snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s", melodyOverrideLabel(v));
              _ctx_dirty = true;
            } else if (sel == 3) {
              NodePrefs* p2 = _task->getNodePrefs();
              if (p2) {
                p2->ch_fav_bitmask ^= (1ULL << ch_idx);
                bool is_fav = (p2->ch_fav_bitmask & (1ULL << ch_idx));
                snprintf(_ctx_ch_fav_item, sizeof(_ctx_ch_fav_item), is_fav ? "Fav: Yes" : "Fav: No");
                _ctx_dirty = true;
                // List rebuild is deferred to menu close: with the fav-only
                // filter on, un-favouriting this channel removes it from the
                // list, and rebuilding under the open menu would shift
                // _channel_sel onto a different channel mid-interaction.
              }
            }
            return true;
          }
        }
        auto res = _ctx_menu.handleInput(c);
        if (_channel_delete_confirm_active) {
          if (res == PopupMenu::SELECTED && _ctx_menu.selectedIndex() == 0) {
            ChannelDetails ch;
            memset(&ch, 0, sizeof(ch));
            if (the_mesh.setChannelLocal(_ctx_ch_idx, ch) == MyMesh::CHANNEL_SAVED)
              _task->showAlert("Channel deleted", 1000);
            else
              _task->logFailure("Channel", "Delete failed");
          }
          if (res != PopupMenu::NONE) {
            _channel_delete_confirm_active = false;
            _task->savePrefsIfDirty(_ctx_dirty);
            buildChannelList();
            if (_channel_sel >= _num_channels)
              _channel_sel = _num_channels > 0 ? _num_channels - 1 : 0;
          }
          return true;
        }
        if (res == PopupMenu::SELECTED && _num_channels > 0) {
          uint8_t ch_idx = _ctx_ch_idx;   // frozen at menu open — see declaration
          int sel = _ctx_menu.selectedIndex();
          if (sel == 0) {
            int cleared = (int)_history.chUnread(ch_idx);
            _history.setChUnread(ch_idx, 0);
            markReadAlert(cleared);
          } else if (sel == 4) {              // Edit
            ChannelDetails ch;
            if (the_mesh.getChannel(ch_idx, ch)) _ch_view.openEdit(ch_idx, ch.name);
          } else if (sel == 5) {              // Delete
            _ctx_menu.beginConfirm("Delete channel?", "Delete");
            _channel_delete_confirm_active = true;
            return true;
          }
          // Value rows are handled in place by Left/Right/Enter above.
        }
        if (res != PopupMenu::NONE) {
          _task->savePrefsIfDirty(_ctx_dirty);
          // Apply any Fav/Edit/Delete change to the visible list now that the menu is done.
          buildChannelList();
          if (_channel_sel >= _num_channels) _channel_sel = _num_channels > 0 ? _num_channels - 1 : 0;
        }
        return true;
      }
      if (c == KEY_CANCEL) {
        if (_pick_bot_channel) { _pick_bot_channel = false; _task->gotoBotScreen(); return true; }
        if (_entry_origin == ORIGIN_HOME_CATEGORY) { _entry_origin = ORIGIN_NORMAL; _task->gotoHomeScreen(); return true; }
        _phase = MODE_SELECT;
        return true;
      }
      // drawList() reclamps _channel_scroll from _channel_sel every render.
      { int total = channelPickTotal();
        if (c == KEY_UP   && total > 0) { _channel_sel = (_channel_sel > 0) ? _channel_sel - 1 : total - 1; return true; }
        if (c == KEY_DOWN && total > 0) { _channel_sel = (_channel_sel < total - 1) ? _channel_sel + 1 : 0; return true; }
      }
      if (c == KEY_ENTER && _channel_sel == _num_channels && !_pick_bot_channel) {
        if (_task->isChildModeLocked()) return true;
        int idx = findFreeChannelSlot();
        if (idx < 0) _task->logWarning("Channel", "List full");
        else         _ch_view.openAdd(idx);
        return true;
      }
      if (c == KEY_ENTER && _num_channels > 0 && _channel_sel < _num_channels) {
        _sel_channel_idx = _channel_indices[_channel_sel];
        if (_task->isChildModeLocked() &&
            !channelAllowedForChild((uint8_t)_sel_channel_idx)) {
          buildChannelList();
          _channel_sel = _channel_scroll = 0;
          _task->logWarning("Child Mode", "Parent only");
          return true;
        }
        if (_pick_target) { commitPickTargetChannel(_sel_channel_idx); return true; }
        if (_pick_bot_channel) { commitPickBotChannel(_sel_channel_idx); return true; }
        int hc = _history.histCountForChannel(_sel_channel_idx);
        _unread_at_entry = (int)_history.chUnread(_sel_channel_idx);
        _hist_scroll = 0;
        _hist_sel = hc > 0 ? 0 : -1;
        _viewing_max_seen = -1;
        _channel_transcript.reset();
        _phase = CHANNEL_HIST;
        // Not updateChannelUnread() here: _hist_visible is still whatever this
        // channel's first render() hasn't computed yet (stale, shared with
        // DM_HIST) — calling it now could ratchet _viewing_max_seen past what's
        // actually about to be shown. render() calls it once that's fresh.
        if (_share_mode) beginShareCompose(true);
        return true;
      }
      if (c == KEY_CONTEXT_MENU && _num_channels > 0 && _channel_sel < _num_channels &&
          !_task->isChildModeLocked()) {
        uint8_t ch_idx = _channel_indices[_channel_sel];
        _ctx_ch_idx = ch_idx;   // freeze the menu's target channel
        static const char* NOTIF_LABELS[] = { "Default", "Off", "Local" };
        snprintf(_ctx_notif_item, sizeof(_ctx_notif_item), "Notif: %s",
                 NOTIF_LABELS[chNotifState(ch_idx)]);
        snprintf(_ctx_melody_item, sizeof(_ctx_melody_item), "Melody: %s",
                 melodyOverrideLabel(chNotifMelody(ch_idx)));
        { NodePrefs* p2 = _task->getNodePrefs();
          bool is_fav = p2 && (p2->ch_fav_bitmask & (1ULL << ch_idx));
          snprintf(_ctx_ch_fav_item, sizeof(_ctx_ch_fav_item), is_fav ? "Fav: Yes" : "Fav: No"); }
        _ctx_menu.begin("Channel options", 6);
        _ctx_menu.addItem("Mark all read");
        _ctx_menu.addValueItem(_ctx_notif_item);
        _ctx_menu.addValueItem(_ctx_melody_item);
        _ctx_menu.addValueItem(_ctx_ch_fav_item);
        _ctx_menu.addItem("Edit");
        _ctx_menu.addItem("Delete");
        _ctx_dirty = false;
        return true;
      }

    } else if (_phase == DM_HIST) {
      int dm_count = _history.dmHistCountForContact(_sel_contact.id.pub_key);
      if (_dm_fs.active) {
        if (_ctx_menu.active) {
          auto res = _ctx_menu.handleInput(c);
          if (res == PopupMenu::SELECTED) {
            dispatchFsAction(false);
          } else if (res != PopupMenu::NONE) {
            _ctx_menu.active = false;
          }
          return true;
        }
        auto res = _dm_fs.handleInput(c);
        if (res == FullscreenMsgView::PREV) {
          if (_dm_hist_sel < dm_count - 1) { _dm_hist_sel++; _dm_fs.scroll = 0; }
        } else if (res == FullscreenMsgView::NEXT) {
          if (_dm_hist_sel > 0) { _dm_hist_sel--; _dm_fs.scroll = 0; }
        } else if (res == FullscreenMsgView::CLOSE) {
          _dm_fs.active = false;
        } else if (res == FullscreenMsgView::REPLY) {
          int ring_pos = _history.dmHistEntryForContact(_sel_contact.id.pub_key, _dm_hist_sel);
          if (ring_pos >= 0) {
            bool reply_ok = !_history.dmAtPos(ring_pos).outgoing;
            if (reply_ok) buildDmReplyPrefix(_history.dmAtPos(ring_pos));
            buildFsMenu(_history.dmAtPos(ring_pos).text, reply_ok);
          }
        }
        return true;
      }
      if (_ctx_menu.active) {
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          if (_participant_picker_active) dispatchParticipantPicker(false);
          else if (_retry_menu_active) dispatchTranscriptAction(false);
          else dispatchFsAction(false);
        } else if (res != PopupMenu::NONE) {
          _ctx_menu.active = false;
          _retry_menu_active = false;
          _participant_picker_active = false;
        }
        return true;
      }
      if (c == KEY_CANCEL) {
        if (_entry_origin == ORIGIN_DIRECT_DM) {
          _entry_origin = ORIGIN_NORMAL;
          _task->gotoHomeScreen();
        } else {
          _phase = CONTACT_PICK;
        }
        return true;
      }
      if (c == KEY_UP)   { _dm_transcript.scrollOlder(); return true; }
      if (c == KEY_DOWN) { _dm_transcript.scrollNewer(); return true; }
      if (c == KEY_ENTER) {
        _sending_to_channel = false;
        beginCustomMessage();
        return true;
      }
      if (c == KEY_CONTEXT_MENU) {
        beginTranscriptActions(false);
        return true;
      }

    } else if (_phase == CHANNEL_HIST) {
      int ch_hist_count = _history.histCountForChannel(_sel_channel_idx);
      if (_fs.active) {
        if (_ctx_menu.active) {
          auto res = _ctx_menu.handleInput(c);
          if (res == PopupMenu::SELECTED) {
            dispatchFsAction(true);
          } else if (res != PopupMenu::NONE) {
            _ctx_menu.active = false;
          }
          return true;
        }
        auto res = _fs.handleInput(c);
        if (res == FullscreenMsgView::PREV) {
          if (_hist_sel < ch_hist_count - 1) { _hist_sel++; _fs.scroll = 0; updateChannelUnread(); }
        } else if (res == FullscreenMsgView::NEXT) {
          if (_hist_sel > 0) { _hist_sel--; _fs.scroll = 0; updateChannelUnread(); }
        } else if (res == FullscreenMsgView::CLOSE) {
          _fs.active = false;
        } else if (res == FullscreenMsgView::REPLY) {
          int ring_pos = _history.histEntryForChannel(_sel_channel_idx, _hist_sel);
          if (ring_pos >= 0)
            buildFsMenu(_history.chAtPos(ring_pos).text, buildChannelReplyPrefix(_history.chAtPos(ring_pos).text));
        }
        return true;
      }
      if (_ctx_menu.active) {
        auto res = _ctx_menu.handleInput(c);
        if (res == PopupMenu::SELECTED) {
          if (_participant_picker_active) dispatchParticipantPicker(true);
          else if (_retry_menu_active) dispatchTranscriptAction(true);
          else dispatchFsAction(true);
        } else if (res != PopupMenu::NONE) {
          _ctx_menu.active = false;
          _retry_menu_active = false;
          _participant_picker_active = false;
        }
        return true;
      }
      if (c == KEY_CANCEL) {
        if (_entry_origin == ORIGIN_DIRECT_CHANNEL) {
          _entry_origin = ORIGIN_NORMAL;
          _task->gotoHomeScreen();
        } else {
          _phase = CHANNEL_PICK;
        }
        return true;
      }
      if (c == KEY_UP)   { _channel_transcript.scrollOlder(); return true; }
      if (c == KEY_DOWN) { _channel_transcript.scrollNewer(); return true; }
      if (c == KEY_ENTER) {
        _sending_to_channel = true;
        beginCustomMessage();
        return true;
      }
      if (c == KEY_CONTEXT_MENU) {
        beginTranscriptActions(true);
        return true;
      }

    } else if (_phase == ROOM_LOGIN_WAIT) {
      if (c == KEY_CANCEL) {
        _task->cancelNodeLogin(solo::NodeLoginCoordinator::MESSAGES, _sel_contact.id.pub_key);
        _phase = CONTACT_PICK;
      }
      return true;
    } else if (_phase == KEYBOARD) {
      auto res = _kb->handleInput(c);
      if (_login_mode) {
        if (res == KeyboardWidget::CANCELLED) {
          _login_mode = false;
          _phase = CONTACT_PICK;
        } else if (res == KeyboardWidget::DONE) {
          // Blank password is valid (guest/no-password rooms) — unlike normal
          // message text, an empty submit here is a deliberate "log in with no
          // password" attempt, so it isn't suppressed like an empty message is.
          // Keep this as a password editor if the packet cannot be queued.
          // Previously _login_mode was cleared first, leaving an identical
          // looking keyboard that was actually handling normal message text.
          if (startNodeLogin(_kb->buf)) _login_mode = false;
        }
        return true;
      }
      if (res == KeyboardWidget::CANCELLED) {
        if (_share_mode) { _share_mode = false; _task->gotoHomeScreen(); }
        else if (_quick_msgs_bypassed) {
          saveCurrentDraft();
          _quick_msgs_bypassed = false;
          _reply_mode = false;
          _phase = _sending_to_channel ? CHANNEL_HIST : DM_HIST;
        } else {
          _phase = MSG_PICK;
        }
      } else if (res == KeyboardWidget::DONE) {
        int prefix_len = _reply_mode ? (int)strlen(_reply_prefix) : 0;
        if (_kb->len > prefix_len) {
          // Expand only the body — prefix "@[nick] " is preserved verbatim, so a nick
          // that happens to contain a placeholder token isn't substituted.
          // Placeholder expansion must obey the same ceiling as typed text;
          // otherwise an apparently valid draft can grow beyond what a later
          // retry packet is able to carry.
          char expanded[messageeditor::SEND_TEXT_LIMIT + 1];
          if (prefix_len > 0) {
            memcpy(expanded, _kb->buf, prefix_len);
            expandMsg(_kb->buf + prefix_len, expanded + prefix_len, sizeof(expanded) - prefix_len);
          } else {
            expandMsg(_kb->buf, expanded, sizeof(expanded));
          }
          solo::MessageTextPolicy::trim(expanded, sendTextLimit());
          clearCurrentDraft();
          bool ok = sendText(expanded);
          afterSend(ok, expanded);
        }
      }
      return true;

    } else { // MSG_PICK
      int total_msg_items = _active_msg_count;
      if (c == KEY_CANCEL) {
        _reply_mode = false;
        _phase = _sending_to_channel ? CHANNEL_HIST : DM_HIST;
        return true;
      }
      if (total_msg_items == 0) return true;
      // drawList() reclamps _msg_scroll from _msg_sel every render.
      if (c == KEY_UP)   { _msg_sel = (_msg_sel > 0) ? _msg_sel - 1 : total_msg_items - 1; return true; }
      if (c == KEY_DOWN) { _msg_sel = (_msg_sel < total_msg_items - 1) ? _msg_sel + 1 : 0; return true; }
      if (c == KEY_ENTER) {
        const char* tmpl = quickReplyText(_active_replies[_msg_sel]);
        char msg[MSG_TEXT_BUF];
        expandMsg(tmpl, msg, sizeof(msg));
        solo::MessageTextPolicy::trim(msg, sendTextLimit());
        bool ok = sendText(msg);
        afterSend(ok, msg);
        return true;
      }
    }
    return false;
  }
};
