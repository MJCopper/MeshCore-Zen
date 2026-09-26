#include "MyMesh.h"
#include "zen/RepeaterTiming.h"
#include "zen/ZenPrefsDefaults.h"
#include "zen/DeviceTimePolicy.h"
#include "zen/BuiltinMelodies.h"
#include "zen/ConfigMaintenance.h"
#include "MsgExpand.h"
#include "GeoUtils.h"
#include "Features.h"

#include <Arduino.h> // needed for PlatformIO
#include <Mesh.h>
#include <helpers/UTF8Helpers.h>

#ifdef DISPLAY_CLASS
#include "helpers/ui/ZenDisplayDriver.h"
#include "UITask.h"
#endif

#define CMD_APP_START                 1
#define CMD_SEND_TXT_MSG              2
#define CMD_SEND_CHANNEL_TXT_MSG      3
#define CMD_GET_CONTACTS              4 // with optional 'since' (for efficient sync)
#define CMD_GET_DEVICE_TIME           5
#define CMD_SET_DEVICE_TIME           6
#define CMD_SEND_SELF_ADVERT          7
#define CMD_SET_ADVERT_NAME           8
#define CMD_ADD_UPDATE_CONTACT        9
#define CMD_SYNC_NEXT_MESSAGE         10
#define CMD_SET_RADIO_PARAMS          11
#define CMD_SET_RADIO_TX_POWER        12
#define CMD_RESET_PATH                13
#define CMD_SET_ADVERT_LATLON         14
#define CMD_REMOVE_CONTACT            15
#define CMD_SHARE_CONTACT             16
#define CMD_EXPORT_CONTACT            17
#define CMD_IMPORT_CONTACT            18
#define CMD_REBOOT                    19
#define CMD_GET_BATT_AND_STORAGE      20   // was CMD_GET_BATTERY_VOLTAGE
#define CMD_SET_TUNING_PARAMS         21
#define CMD_DEVICE_QUERY              22
#define CMD_EXPORT_PRIVATE_KEY        23
#define CMD_IMPORT_PRIVATE_KEY        24
#define CMD_SEND_RAW_DATA             25
#define CMD_SEND_LOGIN                26
#define CMD_SEND_STATUS_REQ           27
#define CMD_HAS_CONNECTION            28
#define CMD_LOGOUT                    29 // 'Disconnect'
#define CMD_GET_CONTACT_BY_KEY        30
#define CMD_GET_CHANNEL               31
#define CMD_SET_CHANNEL               32
#define CMD_SIGN_START                33
#define CMD_SIGN_DATA                 34
#define CMD_SIGN_FINISH               35
#define CMD_SEND_TRACE_PATH           36
#define CMD_SET_DEVICE_PIN            37
#define CMD_SET_OTHER_PARAMS          38
#define CMD_SEND_TELEMETRY_REQ        39  // can deprecate this
#define CMD_GET_CUSTOM_VARS           40
#define CMD_SET_CUSTOM_VAR            41
#define CMD_GET_ADVERT_PATH           42
#define CMD_GET_TUNING_PARAMS         43
// NOTE: CMD range 44..49 parked, potentially for WiFi operations
#define CMD_SEND_BINARY_REQ           50
#define CMD_FACTORY_RESET             51
#define CMD_SEND_PATH_DISCOVERY_REQ   52
#define CMD_SET_FLOOD_SCOPE_KEY       54   // v8+
#define CMD_SEND_CONTROL_DATA         55   // v8+
#define CMD_GET_STATS                 56   // v8+, second byte is stats type
#define CMD_SEND_ANON_REQ             57
#define CMD_SET_AUTOADD_CONFIG        58
#define CMD_GET_AUTOADD_CONFIG        59
#define CMD_GET_ALLOWED_REPEAT_FREQ   60
#define CMD_SET_PATH_HASH_MODE        61
#define CMD_SEND_CHANNEL_DATA         62
#define CMD_SET_DEFAULT_FLOOD_SCOPE   63
#define CMD_GET_DEFAULT_FLOOD_SCOPE   64
#define CMD_SEND_RAW_PACKET           65

// Stats sub-types for CMD_GET_STATS
#define STATS_TYPE_CORE               0
#define STATS_TYPE_RADIO              1
#define STATS_TYPE_PACKETS             2

#define RESP_CODE_OK                  0
#define RESP_CODE_ERR                 1
#define RESP_CODE_CONTACTS_START      2  // first reply to CMD_GET_CONTACTS
#define RESP_CODE_CONTACT             3  // multiple of these (after CMD_GET_CONTACTS)
#define RESP_CODE_END_OF_CONTACTS     4  // last reply to CMD_GET_CONTACTS
#define RESP_CODE_SELF_INFO           5  // reply to CMD_APP_START
#define RESP_CODE_SENT                6  // reply to CMD_SEND_TXT_MSG
#define RESP_CODE_CONTACT_MSG_RECV    7  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CHANNEL_MSG_RECV    8  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CURR_TIME           9  // a reply to CMD_GET_DEVICE_TIME
#define RESP_CODE_NO_MORE_MESSAGES    10 // a reply to CMD_SYNC_NEXT_MESSAGE
#define RESP_CODE_EXPORT_CONTACT      11
#define RESP_CODE_BATT_AND_STORAGE    12 // a reply to a CMD_GET_BATT_AND_STORAGE
#define RESP_CODE_DEVICE_INFO         13 // a reply to CMD_DEVICE_QUERY
#define RESP_CODE_PRIVATE_KEY         14 // a reply to CMD_EXPORT_PRIVATE_KEY
#define RESP_CODE_DISABLED            15
#define RESP_CODE_CONTACT_MSG_RECV_V3 16 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_MSG_RECV_V3 17 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_INFO        18 // a reply to CMD_GET_CHANNEL
#define RESP_CODE_SIGN_START          19
#define RESP_CODE_SIGNATURE           20
#define RESP_CODE_CUSTOM_VARS         21
#define RESP_CODE_ADVERT_PATH         22
#define RESP_CODE_TUNING_PARAMS       23
#define RESP_CODE_STATS               24   // v8+, second byte is stats type
#define RESP_CODE_AUTOADD_CONFIG      25
#define RESP_ALLOWED_REPEAT_FREQ      26
#define RESP_CODE_CHANNEL_DATA_RECV   27
#define RESP_CODE_DEFAULT_FLOOD_SCOPE 28

#define MAX_CHANNEL_DATA_LENGTH       (MAX_FRAME_SIZE - 9)

#define SEND_TIMEOUT_BASE_MILLIS        500
#define FLOOD_SEND_TIMEOUT_FACTOR       16.0f
#define DIRECT_SEND_PERHOP_FACTOR       6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS 250
#define LAZY_CONTACTS_WRITE_DELAY       5000

#define PUBLIC_GROUP_PSK                "izOH6cXN6mrJ5e26oRXNcg=="

// these are _pushed_ to client app at any time
#define PUSH_CODE_ADVERT                0x80
#define PUSH_CODE_PATH_UPDATED          0x81
#define PUSH_CODE_SEND_CONFIRMED        0x82
#define PUSH_CODE_MSG_WAITING           0x83
#define PUSH_CODE_RAW_DATA              0x84
#define PUSH_CODE_LOGIN_SUCCESS         0x85
#define PUSH_CODE_LOGIN_FAIL            0x86
#define PUSH_CODE_STATUS_RESPONSE       0x87
#define PUSH_CODE_LOG_RX_DATA           0x88
#define PUSH_CODE_TRACE_DATA            0x89
#define PUSH_CODE_NEW_ADVERT            0x8A
#define PUSH_CODE_TELEMETRY_RESPONSE    0x8B
#define PUSH_CODE_BINARY_RESPONSE       0x8C
#define PUSH_CODE_PATH_DISCOVERY_RESPONSE 0x8D
#define PUSH_CODE_CONTROL_DATA          0x8E   // v8+
#define PUSH_CODE_CONTACT_DELETED       0x8F // used to notify client app of deleted contact when overwriting oldest
#define PUSH_CODE_CONTACTS_FULL         0x90 // used to notify client app that contacts storage is full

#define ERR_CODE_UNSUPPORTED_CMD        1
#define ERR_CODE_NOT_FOUND              2
#define ERR_CODE_TABLE_FULL             3
#define ERR_CODE_BAD_STATE              4
#define ERR_CODE_FILE_IO_ERROR          5
#define ERR_CODE_ILLEGAL_ARG            6

#define MAX_SIGN_DATA_LEN               (8 * 1024) // 8K

// Auto-add config bitmask
// Bit 0: If set, overwrite oldest non-favourite contact when contacts file is full
// Bits 1-4: these indicate which contact types to auto-add when manual_contact_mode = 0x01
#define AUTO_ADD_OVERWRITE_OLDEST (1 << 0)  // 0x01 - overwrite oldest non-favourite when full
#define AUTO_ADD_CHAT             (1 << 1)  // 0x02 - auto-add Chat (Companion) (ADV_TYPE_CHAT)
#define AUTO_ADD_REPEATER         (1 << 2)  // 0x04 - auto-add Repeater (ADV_TYPE_REPEATER)
#define AUTO_ADD_ROOM_SERVER      (1 << 3)  // 0x08 - auto-add Room Server (ADV_TYPE_ROOM)
#define AUTO_ADD_SENSOR           (1 << 4)  // 0x10 - auto-add Sensor (ADV_TYPE_SENSOR)

void MyMesh::writeOKFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_OK;
  _serial->writeFrame(buf, 1);
}
void MyMesh::writeErrFrame(uint8_t err_code) {
  uint8_t buf[2];
  buf[0] = RESP_CODE_ERR;
  buf[1] = err_code;
  _serial->writeFrame(buf, 2);
}
void MyMesh::writeDisabledFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_DISABLED;
  _serial->writeFrame(buf, 1);
}

void MyMesh::writeContactRespFrame(uint8_t code, const ContactInfo &contact) {
  int i = 0;
  out_frame[i++] = code;
  memcpy(&out_frame[i], contact.id.pub_key, PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  out_frame[i++] = contact.type;
  out_frame[i++] = contact.flags;
  out_frame[i++] = contact.out_path_len;
  memcpy(&out_frame[i], contact.out_path, MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  StrHelper::strzcpy((char *)&out_frame[i], contact.name, 32);
  i += 32;
  memcpy(&out_frame[i], &contact.last_advert_timestamp, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lat, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lon, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.lastmod, 4);
  i += 4;
  _serial->writeFrame(out_frame, i);
}

void MyMesh::updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len) {
  int i = 0;
  uint8_t code = frame[i++]; // eg. CMD_ADD_UPDATE_CONTACT
  memcpy(contact.id.pub_key, &frame[i], PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  contact.type = frame[i++];
  contact.flags = frame[i++];
  contact.out_path_len = frame[i++];
  memcpy(contact.out_path, &frame[i], MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  memcpy(contact.name, &frame[i], 32);
  // The frame comes from the app and nothing guarantees a NUL inside the 32
  // bytes; an unterminated name would later overrun strcpy/strlen consumers
  // (e.g. the AdvertPath name cache).
  contact.name[sizeof(contact.name) - 1] = '\0';
  i += 32;
  memcpy(&contact.last_advert_timestamp, &frame[i], 4);
  i += 4;
  if (len >= i + 8) { // optional fields
    memcpy(&contact.gps_lat, &frame[i], 4);
    i += 4;
    memcpy(&contact.gps_lon, &frame[i], 4);
    i += 4;
    if (len >= i + 4) {
      memcpy(&last_mod, &frame[i], 4);
    }
  }
}

bool MyMesh::Frame::isChannelMsg() const {
  return buf[0] == RESP_CODE_CHANNEL_MSG_RECV || buf[0] == RESP_CODE_CHANNEL_MSG_RECV_V3 ||
         buf[0] == RESP_CODE_CHANNEL_DATA_RECV;
}

void MyMesh::addToOfflineQueue(const uint8_t frame[], int len) {
  if (offline_queue_len >= OFFLINE_QUEUE_SIZE) {
    MESH_DEBUG_PRINTLN("WARN: offline_queue is full!");
    int pos = 0;
    while (pos < offline_queue_len) {
      if (offline_queue[pos].isChannelMsg()) {
        for (int i = pos; i < offline_queue_len - 1; i++) { // delete oldest channel msg from queue
          offline_queue[i] = offline_queue[i + 1];
        }
        MESH_DEBUG_PRINTLN("INFO: removed oldest channel message from queue.");
        offline_queue[offline_queue_len - 1].len = len;
        memcpy(offline_queue[offline_queue_len - 1].buf, frame, len);
        return;
      }
      pos++;
    }
    MESH_DEBUG_PRINTLN("INFO: no channel messages to remove from queue.");
  } else {
    offline_queue[offline_queue_len].len = len;
    memcpy(offline_queue[offline_queue_len].buf, frame, len);
    offline_queue_len++;
  }
}

int MyMesh::getFromOfflineQueue(uint8_t frame[]) {
  if (offline_queue_len > 0) {         // check offline queue
    size_t len = offline_queue[0].len; // take from top of queue
    memcpy(frame, offline_queue[0].buf, len);

    offline_queue_len--;
    for (int i = 0; i < offline_queue_len; i++) { // delete top item from queue
      offline_queue[i] = offline_queue[i + 1];
    }
    return len;
  }
  return 0; // queue is empty
}

float MyMesh::getAirtimeBudgetFactor() const {
  return _prefs.airtime_factor;
}

int MyMesh::getInterferenceThreshold() const {
  return 0; // disabled for now, until currentRSSI() problem is resolved
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  const float base = _prefs.isRepeatEn() ? zen::RepeaterTiming::RX_DELAY_BASE : _prefs.rx_delay_base;
  if (base <= 0.0f) return 0;
  return (int)((pow(base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t airtime = _radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2);
  uint32_t t = zen::RepeaterTiming::delayWindow(airtime, zen::RepeaterTiming::FLOOD_TX_FACTOR);
  uint32_t d = getRNG()->nextInt(0, 5*t + 1);
  // Yield filter (Tools > Repeater): scale the flood retransmit delay so a
  // mobile companion waits longer and lets better-sited fixed repeaters win the
  // flood first. Only forwarded floods reach here — own sends pass their own
  // delay to sendFlood() — so this never slows the companion's own traffic.
  return d * zen::RepeaterTiming::YIELD_MULTIPLIER;
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t airtime = _radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2);
  uint32_t t = zen::RepeaterTiming::delayWindow(airtime, zen::RepeaterTiming::DIRECT_TX_FACTOR);
  return getRNG()->nextInt(0, 5*t + 1);
}

uint8_t MyMesh::getExtraAckTransmitCount() const {
  return _prefs.multi_acks;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
  // A non-empty flood path proves at least one relay; a direct path needs more
  // than its destination hop to prove an intermediate relay. This excludes
  // zero-hop companion traffic while sampling normal routed mesh activity.
  if (raw && len >= 2) {
    uint8_t route = raw[0] & PH_ROUTE_MASK;
    int path_pos = (route == ROUTE_TYPE_TRANSPORT_FLOOD ||
                    route == ROUTE_TYPE_TRANSPORT_DIRECT) ? 5 : 1;
    if (path_pos < len) {
      uint8_t path_count = raw[path_pos] & 63;
      bool relayed = zen::RepeaterSignalMonitor::qualifiesRoute(
          route == ROUTE_TYPE_FLOOD || route == ROUTE_TYPE_TRANSPORT_FLOOD,
          route == ROUTE_TYPE_DIRECT || route == ROUTE_TYPE_TRANSPORT_DIRECT,
          path_count);
      if (relayed) _repeater_signal.noteSample((int)(snr * 4), millis());
    }
  }
  if (_serial->isConnected() && len + 3 <= MAX_FRAME_SIZE) {
    int i = 0;
    out_frame[i++] = PUSH_CODE_LOG_RX_DATA;
    out_frame[i++] = (int8_t)(snr * 4);
    out_frame[i++] = (int8_t)(rssi);
    memcpy(&out_frame[i], raw, len);
    i += len;

    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::isAutoAddEnabled() const {
  return (_prefs.manual_add_contacts & 1) == 0;
}

bool MyMesh::shouldAutoAddContactType(uint8_t contact_type) const {
  if ((_prefs.manual_add_contacts & 1) == 0) {
    return true;
  }

  uint8_t type_bit = 0;
  switch (contact_type) {
    case ADV_TYPE_CHAT:
      type_bit = AUTO_ADD_CHAT;
      break;
    case ADV_TYPE_REPEATER:
      type_bit = AUTO_ADD_REPEATER;
      break;
    case ADV_TYPE_ROOM:
      type_bit = AUTO_ADD_ROOM_SERVER;
      break;
    case ADV_TYPE_SENSOR:
      type_bit = AUTO_ADD_SENSOR;
      break;
    default:
      return false;  // Unknown type, don't auto-add
  }

  return (_prefs.autoadd_config & type_bit) != 0;
}

bool MyMesh::shouldOverwriteWhenFull() const {
  return (_prefs.autoadd_config & AUTO_ADD_OVERWRITE_OLDEST) != 0;
}

uint8_t MyMesh::getAutoAddMaxHops() const {
  return _prefs.autoadd_max_hops;
}

void MyMesh::onContactOverwrite(const uint8_t* pub_key) {
    _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE); // delete from storage
  if (_ui) _ui->onContactRemoved(pub_key); // same cleanup as an explicit CMD_REMOVE_CONTACT
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACT_DELETED;
    memcpy(&out_frame[1], pub_key, PUB_KEY_SIZE);
    _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
  }
}

void MyMesh::onContactsFull() {
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACTS_FULL;
    _serial->writeFrame(out_frame, 1);
  }
}

void MyMesh::onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) {
  // Advert presentation belongs to Zen's application layer. BaseChatMesh only
  // reports the discovered contact; it does not own popup/sound policy.
  if (_ui)
    _ui->notify((path_len & 63) ? UIEventType::advertReceivedFlood
                                : UIEventType::advertReceivedZeroHop);
  // Routed adverts were already sampled by logRxRaw(); this covers a directly
  // heard repeater advert without double-weighting the same packet.
  if (contact.type == ADV_TYPE_REPEATER && (path_len & 63) == 0)
    _repeater_signal.noteSample((int)(_radio->getLastSNR() * 4), millis());
  if (_serial->isConnected()) {
    if (is_new) {
      writeContactRespFrame(PUSH_CODE_NEW_ADVERT, contact);
    } else {
      out_frame[0] = PUSH_CODE_ADVERT;
      memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
      _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
    }
  }

  // add inbound-path to mem cache
  if (path && mesh::Packet::isValidPathLen(path_len)) {  // check path is valid
    AdvertPath* p = advert_paths;
    uint32_t oldest = 0xFFFFFFFF;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {   // check if already in table, otherwise evict oldest
      if (memcmp(advert_paths[i].pubkey_prefix, contact.id.pub_key, sizeof(AdvertPath::pubkey_prefix)) == 0) {
        p = &advert_paths[i];   // found
        break;
      }
      if (advert_paths[i].recv_timestamp < oldest) {
        oldest = advert_paths[i].recv_timestamp;
        p = &advert_paths[i];
      }
    }

    memcpy(p->pubkey_prefix, contact.id.pub_key, sizeof(p->pubkey_prefix));
    strcpy(p->name, contact.name);
    p->recv_timestamp = getRTCClock()->getCurrentTime();
    p->path_len = mesh::Packet::copyPath(p->path, path, path_len);
  }

  if (!is_new) dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY); // only schedule lazy write for contacts that are in contacts[]
}

static int sort_by_recent(const void *a, const void *b) {
  return ((AdvertPath *) b)->recv_timestamp - ((AdvertPath *) a)->recv_timestamp;
}

int MyMesh::getRecentlyHeard(AdvertPath dest[], int max_num) {
  if (max_num > ADVERT_PATH_TABLE_SIZE) max_num = ADVERT_PATH_TABLE_SIZE;
  qsort(advert_paths, ADVERT_PATH_TABLE_SIZE, sizeof(advert_paths[0]), sort_by_recent);

  for (int i = 0; i < max_num; i++) {
    dest[i] = advert_paths[i];
  }
  return max_num;
}

bool MyMesh::addDiscoveredContact(const uint8_t* pub_key, const char* name, uint8_t type) {
  if (lookupContactByPubKey(pub_key, PUB_KEY_SIZE)) return true;  // already a contact
  ContactInfo c;
  memset(&c, 0, sizeof(c));
  c.id = mesh::Identity(pub_key);
  strncpy(c.name, name ? name : "", sizeof(c.name) - 1);
  c.name[sizeof(c.name) - 1] = '\0';
  c.type = type;
  c.out_path_len = OUT_PATH_UNKNOWN;   // no path yet → flood until one is learned
  c.lastmod = getRTCClock()->getCurrentTime();
  c.shared_secret_valid = false;
  if (!addContact(c)) return false;    // contacts table full
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  return true;
}

// Full delete sequence shared by CMD_REMOVE_CONTACT and the on-device Nearby menu:
// drop the contact, its persisted blob and any saved room login, run the UI cleanup
// (favourite slot / Locator / Live Share target), then schedule the lazy write.
bool MyMesh::deleteContactByKey(const uint8_t* pub_key) {
  ContactInfo* recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
  if (!recipient || !removeContact(*recipient)) return false;
  _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE);
  forgetRoomPassword(pub_key);
  if (_ui) _ui->onContactRemoved(pub_key);
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  return true;
}

bool MyMesh::setContactFavourite(const uint8_t* pub_key, bool favourite) {
  ContactInfo* contact = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
  if (!contact) return false;
  bool current = (contact->flags & 0x01) != 0;
  if (current == favourite) return true;
  if (favourite) contact->flags |= 0x01;
  else           contact->flags &= ~0x01;
  uint32_t changed_at = getRTCClock()->getCurrentTimeUnique();
  // Incremental companion sync uses `lastmod > since`. Preserve that strict
  // ordering even for two edits in one RTC second or a clock correction.
  if (changed_at <= contact->lastmod && contact->lastmod != UINT32_MAX)
    changed_at = contact->lastmod + 1;
  contact->lastmod = changed_at;
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  return true;
}

void MyMesh::onContactPathUpdated(const ContactInfo &contact) {
  out_frame[0] = PUSH_CODE_PATH_UPDATED;
  memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
  _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE); // NOTE: app may not be connected
}

ContactInfo*  MyMesh::processAck(const uint8_t *data) {
  // 0 marks an empty/cleared table slot, not a real ACK -- a bare memcmp below
  // would otherwise match the first free slot against an all-zero `data` (which
  // *is* remotely reachable, ack_crc comes off the air) and misattribute the
  // ACK to whatever contact that slot last belonged to.
  static const uint8_t zero_ack[4] = {0, 0, 0, 0};
  if (memcmp(data, zero_ack, 4) == 0) return checkConnectionsAck(data);

  // see if matches any in a table
  for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++) {
    if (expected_ack_table[i].ack == 0) continue;   // empty slot
    if (memcmp(data, &expected_ack_table[i].ack, 4) == 0) { // got an ACK from recipient
      out_frame[0] = PUSH_CODE_SEND_CONFIRMED;
      memcpy(&out_frame[1], data, 4);
      uint32_t trip_time = _ms->getMillis() - expected_ack_table[i].msg_sent;
      memcpy(&out_frame[5], &trip_time, 4);
      _serial->writeFrame(out_frame, 9);

      ContactInfo* contact = expected_ack_table[i].contact;
      // NOTE: the same ACK can be received multiple times!
      expected_ack_table[i].ack = 0;            // clear expected hash, now that we have received ACK
      expected_ack_table[i].contact = nullptr;  // and the stale pointer along with it
      if (_ui) {
        uint32_t ack_crc;
        memcpy(&ack_crc, data, sizeof(ack_crc));
        _ui->onMsgAck(ack_crc);
      }
      _transport_trace.add(zen::TransportTrace::ACK_RECEIVED,
                           contact ? contact->id.pub_key : nullptr);
      return contact;
    }
  }
  return checkConnectionsAck(data);
}

int MyMesh::sendUIMessage(const ContactInfo& recipient, uint32_t timestamp,
                          uint8_t attempt, const char* text,
                          uint32_t& expected_ack, uint32_t& est_timeout) {
  int result = sendMessage(recipient, timestamp, attempt, text,
                           expected_ack, est_timeout);
  _transport_trace.add(result == MSG_SEND_FAILED
                           ? zen::TransportTrace::SEND_REJECTED
                           : zen::TransportTrace::DM_QUEUED,
                       recipient.id.pub_key,
                       result == MSG_SEND_SENT_FLOOD ? 1 : 0);
  if (result != MSG_SEND_FAILED && expected_ack) {
    // Use MeshCore's normal ACK ownership for standalone sends as well as app
    // sends. This keeps ACK/path-return processing in one transport lifecycle;
    // Zen only observes the confirmation to update its RAM transcript.
    expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis();
    expected_ack_table[next_ack_idx].ack = expected_ack;
    expected_ack_table[next_ack_idx].contact = lookupContactByPubKey(
        recipient.id.pub_key, PUB_KEY_SIZE);
    next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
  }
  return result;
}

void MyMesh::queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt,
                          uint32_t sender_timestamp, const uint8_t *extra, int extra_len, const char *text) {
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV;
  }
  memcpy(&out_frame[i], from.id.pub_key, 6);
  i += 6; // just 6-byte prefix
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = txt_type;
  memcpy(&out_frame[i], &sender_timestamp, 4);
  i += 4;
  if (extra_len > 0) {
    memcpy(&out_frame[i], extra, extra_len);
    i += extra_len;
  }
  int tlen = strlen(text);
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = mesh::validUtf8PrefixLength(text, MAX_FRAME_SIZE - i);
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }

#ifdef DISPLAY_CLASS
  // we only want to show text messages on display, not cli data
  bool should_display = txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN;
  if (should_display && _ui && _ui->allowOnDeviceContactMessage(from)) {
    // Add to the on-device conversation history. Room servers (ADV_TYPE_ROOM) are
    // viewed through the same history list as chat contacts (keyed by the server's
    // pubkey), so their posts must be stored too — otherwise an incoming room
    // message fires the notification and reaches the app via the offline queue but
    // never shows when the room is opened directly on the device.
    bool added = true;
    if (from.type == ADV_TYPE_CHAT) {
      added = _ui->addDMMsg(from.id.pub_key, false, text, sender_timestamp);
    } else if (from.type == ADV_TYPE_ROOM) {
      // A room carries many guests, so prefix the post with its author so the UI
      // can attribute each line. The signed message's `extra` holds the sender's
      // pubkey prefix; resolve it to a contact name, falling back to a short hex.
      char labeled[MAX_TEXT_LEN + 40];  // room text + "Sender: " (history store truncates)
      if (extra && extra_len >= 4) {
        ContactInfo* sc = lookupContactByPubKey(extra, extra_len);
        if (sc && sc->name[0])
          snprintf(labeled, sizeof(labeled), "%s: %s", sc->name, text);
        else
          snprintf(labeled, sizeof(labeled), "%02X%02X: %s", extra[0], extra[1], text);
      } else {
        snprintf(labeled, sizeof(labeled), "%s", text);
      }
      added = _ui->addDMMsg(from.id.pub_key, false, labeled, sender_timestamp);
    }
    // A retry reuses timestamp + text but has a fresh packet hash. Only the
    // first stored copy should affect unread state or notify the user.
    if (added)
      _ui->incomingMessage(from.type == ADV_TYPE_ROOM ? UIEventType::roomMessage : UIEventType::contactMessage,
                           path_len, from.name, text, offline_queue_len, from.type, from.id.pub_key);
  }
#endif
}

bool MyMesh::filterRecvFloodPacket(mesh::Packet* packet) {
  // UI relayed-into-mesh marker: match heard echoes of recent channel sends.
  // Gated on _relay_active so the hash is only computed while a send is pending.
  if (_relay_active > 0) {
    uint8_t h[MAX_HASH_SIZE];
    bool hashed = false;
    for (int i = 0; i < RELAY_RING; i++) {
      RelaySlot& s = _relay[i];
      if (!s.pending || !s.transmitted || s.len != packet->payload_len) continue;
      if (!hashed) { packet->calculatePacketHash(h); hashed = true; }
      if (memcmp(h, s.hash, MAX_HASH_SIZE) == 0) {
        if (s.heard < 255) s.heard++;
        if (_ui) _ui->onChannelRelayed(s.seq);
        break;
      }
    }
  }
  // REVISIT: try to determine which Region (from transport_codes[1]) that Sender is indicating for replies/responses
  //    if unknown, fallback to finding Region from transport_codes[0], the 'scope' used by Sender
  return false;
}

// Loop guard for flood packets, ported from simple_repeater's LOOP_DETECT_MODERATE
// thresholds (max times this node's hash may already appear in the path, by hash size).
// A companion moves around, so it re-enters its own flood's path more easily than a
// fixed repeater — hardcoded rather than configurable since there's no CLI here.
// Indexed by getPathHashSize() = (path_len>>6)+1, so 1..4. Index 0 is unused
// (hash size is never 0); index 4 covers path_mode 3, which tryParsePacket
// currently rejects — kept in-bounds so this can't OOB-read if that guard is
// ever relaxed.
static const uint8_t REPEAT_LOOP_MAX[] = { 0, /*1-byte*/ 2, /*2-byte*/ 1, /*3-byte*/ 1, /*4-byte*/ 1 };
// Caps how many hops an ADVERT flood gets repeated, matching simple_repeater's default
// flood_max_advert — adverts are the most frequent flood traffic, so this is the one
// depth limit worth keeping even without the rest of simple_repeater's flood_max knobs.
static const uint8_t REPEAT_MAX_ADVERT_HOPS = 8;

bool MyMesh::isRepeatLooped(const mesh::Packet* packet) const {
  uint8_t hash_size = packet->getPathHashSize();
  if (hash_size >= sizeof(REPEAT_LOOP_MAX)) return true;  // unknown hash size: treat as looped, don't forward
  uint8_t hash_count = packet->getPathHashCount();
  uint8_t n = 0;
  const uint8_t* path = packet->path;
  while (hash_count > 0) {
    if (self_id.isHashMatch(path, hash_size)) n++;
    hash_count--;
    path += hash_size;
  }
  return n >= REPEAT_LOOP_MAX[hash_size];
}

bool MyMesh::allowPacketForward(const mesh::Packet* packet) {
  if (!zen::Features::REPEATER || !_prefs.isRepeatEn()) return false;
  // Apply only the standard automatic flood-safety limits. User-configurable
  // advert, global-hop and SNR filters were removed because they could silently
  // prevent otherwise valid mesh delivery.
  if (packet->isRouteFlood()) {
    if (packet->getPayloadType() == PAYLOAD_TYPE_ADVERT && packet->getPathHashCount() >= REPEAT_MAX_ADVERT_HOPS) return false;
    if (isRepeatLooped(packet)) return false;
  }
  return true;
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, _prefs.path_hash_mode + 1);
  }
}

void MyMesh::sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: dynamic send_scope, depending on recipient and current 'home' Region
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);  // app has explicitly requested un-scoped
  } else {
    TransportKey default_scope;
    if (_prefs.default_scope_name[0])
      memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
    else
      memset(&default_scope.key, 0, sizeof(default_scope.key));

    auto scope = send_scope.isNull() ? &default_scope : &send_scope;
    sendFloodScoped(*scope, pkt, delay_millis);
  }
}
void MyMesh::sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: have per-channel send_scope
  trackRelaySend(pkt);
  if (send_unscoped) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);  // app has explicitly requested un-scoped
  } else {
    TransportKey default_scope;
    if (_prefs.default_scope_name[0])
      memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
    else
      memset(&default_scope.key, 0, sizeof(default_scope.key));

    auto scope = send_scope.isNull() ? &default_scope : &send_scope;
    sendFloodScoped(*scope, pkt, delay_millis);
  }
}

bool MyMesh::useDefaultFloodScopeNow() {
  if (!hasTemporaryFloodScopeOverride()) return false;
  memset(send_scope.key, 0, sizeof(send_scope.key));
  send_unscoped = false;
  return true;
}

bool MyMesh::setDefaultFloodScope(const char* name, const uint8_t* key) {
  if (!name || (!key && name[0])) return false;
  size_t len = strnlen(name, sizeof(_prefs.default_scope_name));
  if (len >= sizeof(_prefs.default_scope_name)) return false;
  char previous_name[sizeof(_prefs.default_scope_name)];
  uint8_t previous_key[sizeof(_prefs.default_scope_key)];
  memcpy(previous_name, _prefs.default_scope_name, sizeof(previous_name));
  memcpy(previous_key, _prefs.default_scope_key, sizeof(previous_key));
  memset(_prefs.default_scope_name, 0, sizeof(_prefs.default_scope_name));
  memset(_prefs.default_scope_key, 0, sizeof(_prefs.default_scope_key));
  if (len) {
    memcpy(_prefs.default_scope_name, name, len);
    memcpy(_prefs.default_scope_key, key, sizeof(_prefs.default_scope_key));
  }
  if (memcmp(previous_name, _prefs.default_scope_name,
             sizeof(previous_name)) == 0 &&
      memcmp(previous_key, _prefs.default_scope_key,
             sizeof(previous_key)) == 0) return true;
  if (!saveBaselinePrefsFromCompanion()) {
    memcpy(_prefs.default_scope_name, previous_name, sizeof(previous_name));
    memcpy(_prefs.default_scope_key, previous_key, sizeof(previous_key));
    if (_ui) _ui->onOperationResult(zen::OperationResult::make(
        zen::Operation::FLOOD_SCOPE, zen::OperationOutcome::TRANSPORT_FAILURE,
        zen::OperationReason::SAVE_FAILED, zen::RESULT_BACKGROUND));
    return false;
  }
  return true;
}

bool MyMesh::setDefaultFloodScopeName(const char* name) {
  char clean[sizeof(_prefs.default_scope_name)];
  if (!zen::FloodScopeView::normaliseName(name, clean, sizeof(clean))) return false;
  if (!clean[0]) return setDefaultFloodScope("", nullptr);
  char keyed_name[sizeof(clean) + 1];
  snprintf(keyed_name, sizeof(keyed_name), "#%s", clean);
  TransportKeyStore keys;
  TransportKey key;
  keys.getAutoKeyFor(0, keyed_name, key);
  return setDefaultFloodScope(clean, key.key);
}


void MyMesh::onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const char *text) {
  _transport_trace.add(zen::TransportTrace::MESSAGE_RECEIVED, from.id.pub_key,
                       pkt->isRouteFlood() ? 1 : 0);
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_PLAIN, pkt, sender_timestamp, NULL, 0, text);

  // hop count of the received message. getPathHashCount() (low 6 bits of path_len)
  // is the number of repeaters traversed — the same value the mesh uses for flood
  // retransmit priority. 0 = heard directly. (Raw path_len is a size/count
  // bitfield for transport packets, so it must not be used directly.)
}

void MyMesh::onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                               const char *text) {
  _transport_trace.add(zen::TransportTrace::ADMIN_RECEIVED, from.id.pub_key,
                       pkt->isRouteFlood() ? 1 : 0);
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_CLI_DATA, pkt, sender_timestamp, NULL, 0, text);
  // If the on-device Admin screen sent this command (not the BLE app's CLI
  // terminal), also hand the reply straight to the UI -- queueMessage() above
  // never displays TXT_TYPE_CLI_DATA on-device (see should_display), since that
  // path also serves the app's terminal, which must keep working unaffected.
#if ZEN_FEATURE_ADMIN
  if (_ui && _admin_session.complete(from.id.pub_key, text, millis())) {
    _ui->onAdminReply(from.id.pub_key, text + 3);
  }
#endif
}

void MyMesh::onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                                 const uint8_t *sender_prefix, const char *text) {
  markConnectionActive(from);
  // from.sync_since change needs to be persisted
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  queueMessage(from, TXT_TYPE_SIGNED_PLAIN, pkt, sender_timestamp, sender_prefix, 4, text);

  // Room-server auto-reply bot — only ever fires for the room server contact
  // itself (signed posts are how a room relays its members' messages back to
  // us); a defensive type check lives in tryBotReplyRoom/tryBotRoomCommand too.
}

void MyMesh::onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                                  const char *text) {
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
  }

  // findChannelIdx() returns -1 for an unknown secret (e.g. a packet that
  // routed to us through a stale hash collision, or a stored channel slot
  // whose secret was corrupted). Casting -1 to uint8_t would give 255, and
  // every downstream path (offline queue, UI hist, bot reply) would then
  // operate on a bogus channel index. Drop the message instead.
  int idx = findChannelIdx(channel);
  if (idx < 0) {
    MESH_DEBUG_PRINTLN("onChannelMessageRecv: unknown channel secret — dropping message");
    return;
  }
  uint8_t channel_idx = (uint8_t)idx;
  out_frame[i++] = channel_idx;
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;

  out_frame[i++] = TXT_TYPE_PLAIN;
  memcpy(&out_frame[i], &timestamp, 4);
  i += 4;
  int tlen = strlen(text);
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = mesh::validUtf8PrefixLength(text, MAX_FRAME_SIZE - i);
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
#ifdef DISPLAY_CLASS
  bool show_on_device = _ui && _ui->allowOnDeviceChannelMessage(channel_idx);
  if (show_on_device) _ui->addChannelMsg(channel_idx, text, timestamp);
  const char *channel_name = "Unknown";
  ChannelDetails channel_details;
  if (getChannel(channel_idx, channel_details)) {
    channel_name = channel_details.name;
  }
  if (show_on_device) _ui->incomingMessage(UIEventType::channelMessage, path_len, channel_name,
                                text, offline_queue_len, 0, nullptr, channel_idx);

#endif

  // hop count for !hops (see onMessageRecv); not the wire path_len above.
}

void MyMesh::onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                               const uint8_t *data, size_t data_len) {
  if (data_len > MAX_CHANNEL_DATA_LENGTH) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: dropping payload_len=%d exceeds frame limit=%d",
                       (uint32_t)data_len, (uint32_t)MAX_CHANNEL_DATA_LENGTH);
    return;
  }

  int i = 0;
  out_frame[i++] = RESP_CODE_CHANNEL_DATA_RECV;
  out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
  out_frame[i++] = 0; // reserved1
  out_frame[i++] = 0; // reserved2

  int didx = findChannelIdx(channel);
  if (didx < 0) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: unknown channel secret — dropping packet");
    return;
  }
  uint8_t channel_idx = (uint8_t)didx;
  out_frame[i++] = channel_idx;
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = (uint8_t)(data_type & 0xFF);
  out_frame[i++] = (uint8_t)(data_type >> 8);
  out_frame[i++] = (uint8_t)data_len;

  int copy_len = (int)data_len;
  if (copy_len > 0) {
    memcpy(&out_frame[i], data, copy_len);
    i += copy_len;
  }
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
}

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
  if (data[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t permissions = 0;
    uint8_t cp = contact.flags >> 1; // LSB used as 'favourite' bit (so only use upper bits)

    if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_ALL) {
      permissions = TELEM_PERM_BASE;
    } else if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_FLAGS) {
      permissions = cp & TELEM_PERM_BASE;
    }

    if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_LOCATION;
    } else if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_LOCATION;
    }

    if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_ENVIRONMENT;
    } else if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_ENVIRONMENT;
    }

    uint8_t perm_mask = ~(data[1]);    // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions
    permissions &= perm_mask;

    if (permissions & TELEM_PERM_BASE) { // only respond if base permission bit is set
      telemetry.reset();
      telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      // query other sensors -- target specific
      sensors.querySensors(permissions, telemetry);

      memcpy(reply, &sender_timestamp,
             4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

      uint8_t tlen = telemetry.getSize();
      memcpy(&reply[4], telemetry.getBuffer(), tlen);
      return 4 + tlen;
    }
  }
  return 0; // unknown
}

void MyMesh::onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) {
  _transport_trace.add(zen::TransportTrace::RESPONSE_RECEIVED,
                       contact.id.pub_key, len);
  if (len < 4) return;
  uint32_t tag;
  memcpy(&tag, data, 4);

  // UI telemetry owns its tag and full identity, independently of app requests.
  if (sensorReplyState() == SENSOR_WAITING &&
      zen::RemoteTelemetryAdapter::matches(_sensor_key, _sensor_tag,
                                            contact.id.pub_key, tag)) {
    sensorTelemetry.load(data + 4, len - 4);
    _sensor_state = SENSOR_READY;
    if (_ui) _ui->onSensorTelemetry();
    return;
  }

  if (pending_login && memcmp(&pending_login, contact.id.pub_key, 4) == 0) { // check for login response
    // yes, is response to pending sendLogin()
    pending_login = 0;

    int i = 0;
    bool login_ok = false;
    if (len >= 6 && memcmp(&data[4], "OK", 2) == 0) { // legacy Repeater login OK response
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = 0; // legacy: is_admin = false
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6;                                     // pub_key_prefix
      login_ok = true;
    } else if (len >= 13 && data[4] == RESP_SERVER_LOGIN_OK) { // new login response
      uint16_t keep_alive_secs = ((uint16_t)data[5]) * 16;
      if (keep_alive_secs > 0) {
        startConnection(contact, keep_alive_secs);
      }
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = data[6]; // permissions (eg. is_admin)
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
      memcpy(&out_frame[i], &tag, 4);
      i += 4; // NEW: include server timestamp
      out_frame[i++] = data[7]; // NEW (v7): ACL permissions
      out_frame[i++] = data[12]; // FIRMWARE_VER_LEVEL
      login_ok = true;
    } else {
      out_frame[i++] = PUSH_CODE_LOGIN_FAIL;
      out_frame[i++] = 0; // reserved
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
    }
    // Persist app-entered room passwords too, so the device can later post
    // to that room standalone (after reboot, no phone) without re-prompting --
    // same store the on-device login path uses. Rooms only; a failed login
    // forgets any stale saved password, mirroring onNodeLoginResult().
    if (contact.type == ADV_TYPE_ROOM) {
      if (login_ok) saveRoomPassword(contact.id.pub_key, pending_login_pw);
      else          forgetRoomPassword(contact.id.pub_key);
    }
    _serial->writeFrame(out_frame, i);
  } else if (_ui_login_pending &&
      memcmp(_ui_pending_login_key, contact.id.pub_key, PUB_KEY_SIZE) == 0
      && !(pending_telemetry && tag == pending_telemetry)
      && !(pending_req && tag == pending_req)
      && zen::RemoteLoginAdapter::valid(data, len)) {
    _ui_login_pending = false;

    bool success;
    uint8_t permissions = 0;
    if (memcmp(&data[4], "OK", 2) == 0) { // legacy Repeater login OK response
      success = true;
    } else if (data[4] == RESP_SERVER_LOGIN_OK) { // new login response
      uint16_t keep_alive_secs = ((uint16_t)data[5]) * 16;
      if (keep_alive_secs > 0) {
        startConnection(contact, keep_alive_secs);
      }
      success = true;
      permissions = zen::RemoteLoginAdapter::permissions(
          data[6], data[7]); // reconcile legacy role with ACL permissions
    } else {
      success = false;
    }
    _ui->onNodeLoginResult(contact.id.pub_key, success, permissions);
  } else if (len > 4 && // check for status response
             pending_status &&
             memcmp(&pending_status, contact.id.pub_key, 4) == 0 // legacy matching scheme
                                                                 // FUTURE: tag == pending_status
  ) {
    pending_status = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_STATUS_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_telemetry) {  // check for matching response tag
    pending_telemetry = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_req) {  // check for matching response tag
    pending_req = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_BINARY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], &tag, 4);   // app needs to match this to RESP_CODE_SENT.tag
    i += 4;
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  }
}

#define ROOM_PW_FILE "/room_pw"
#define ROOM_PW_TMP  "/room_pw.tmp"
#define ROOM_PW_BAK  "/room_pw.bak"
#define MAX_SAVED_ROOM_PASSWORDS 16

namespace {
  struct RoomPwRec {
    uint8_t key[4];   // pub-key prefix
    char pw[16];      // up to 15 chars + NUL
  };
}

bool MyMesh::getRoomPassword(const uint8_t* pub_key, char* out_password, uint8_t max_len) {
  File f = _zen_store.openRead(ROOM_PW_FILE);
  if (!f) return false;

  RoomPwRec rec;
  bool found = false;
  while (f.read((uint8_t *)&rec, sizeof(rec)) == sizeof(rec)) {
    if (memcmp(rec.key, pub_key, 4) == 0) {
      strncpy(out_password, rec.pw, max_len - 1);
      out_password[max_len - 1] = 0;
      found = true;
      break;
    }
  }
  f.close();
  return found;
}

bool MyMesh::saveRoomPassword(const uint8_t* pub_key, const char* password) {
  RoomPwRec new_rec;
  memcpy(new_rec.key, pub_key, 4);
  strncpy(new_rec.pw, password, sizeof(new_rec.pw) - 1);
  new_rec.pw[sizeof(new_rec.pw) - 1] = 0;

  // The table is tiny (<= MAX_SAVED_ROOM_PASSWORDS * 20 bytes), so just load
  // it whole, update/append/evict in RAM, then rewrite -- simpler and just
  // as crash-safe as a record seek given how rarely this runs (once per new
  // room login).
  RoomPwRec recs[MAX_SAVED_ROOM_PASSWORDS];
  int count = 0;
  File rf = _zen_store.openRead(ROOM_PW_FILE);
  if (rf) {
    RoomPwRec rec;
    while (count < MAX_SAVED_ROOM_PASSWORDS && rf.read((uint8_t *)&rec, sizeof(rec)) == sizeof(rec)) {
      if (memcmp(rec.key, pub_key, 4) == 0 &&
          strncmp(rec.pw, new_rec.pw, sizeof(rec.pw)) == 0) {
        rf.close();
        return true;  // reconnecting with the same password needs no flash write
      }
      if (memcmp(rec.key, pub_key, 4) != 0) { // drop stale entry for this key -- replaced below
        recs[count++] = rec;
      }
    }
    rf.close();
  }

  if (count < MAX_SAVED_ROOM_PASSWORDS) {
    recs[count++] = new_rec;
  } else { // table full and not already present -- evict oldest (front)
    memmove(&recs[0], &recs[1], sizeof(RoomPwRec) * (MAX_SAVED_ROOM_PASSWORDS - 1));
    recs[MAX_SAVED_ROOM_PASSWORDS - 1] = new_rec;
  }

  // Write to a temp file and atomically swap it over /room_pw, so an
  // interrupted save leaves the previous good table intact rather than a
  // truncated mix (mirrors how contacts/channels are persisted).
  size_t want = sizeof(RoomPwRec) * count;
  return _zen_store.replaceVerified(ROOM_PW_TMP, ROOM_PW_FILE, ROOM_PW_BAK,
                                    (const uint8_t*)recs, want);
}

bool MyMesh::forgetRoomPassword(const uint8_t* pub_key) {
  RoomPwRec recs[MAX_SAVED_ROOM_PASSWORDS];
  int count = 0;
  File rf = _zen_store.openRead(ROOM_PW_FILE);
  if (!rf) return true;

  RoomPwRec rec;
  bool removed = false;
  while (count < MAX_SAVED_ROOM_PASSWORDS && rf.read((uint8_t *)&rec, sizeof(rec)) == sizeof(rec)) {
    if (memcmp(rec.key, pub_key, 4) == 0) {
      removed = true;
    } else {
      recs[count++] = rec;
    }
  }
  rf.close();
  if (!removed) return true; // nothing to do, avoid a pointless rewrite

  size_t want = sizeof(RoomPwRec) * count;
  return _zen_store.replaceVerified(ROOM_PW_TMP, ROOM_PW_FILE, ROOM_PW_BAK,
                                    (const uint8_t*)recs, want);
}

bool MyMesh::onContactPathRecv(ContactInfo& contact, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) {
  _transport_trace.add(zen::TransportTrace::PATH_RECEIVED,
                       contact.id.pub_key, extra_type);
  if (extra_type == PAYLOAD_TYPE_RESPONSE && extra_len > 4) {
    uint32_t tag;
    memcpy(&tag, extra, 4);

    if (tag == pending_discovery) {  // check for matching response tag)
      pending_discovery = 0;

      if (!mesh::Packet::isValidPathLen(in_path_len) || !mesh::Packet::isValidPathLen(out_path_len)) {
        MESH_DEBUG_PRINTLN("onContactPathRecv, invalid path sizes: %d, %d", in_path_len, out_path_len);
      } else {
        int i = 0;
        out_frame[i++] = PUSH_CODE_PATH_DISCOVERY_RESPONSE;
        out_frame[i++] = 0; // reserved
        memcpy(&out_frame[i], contact.id.pub_key, 6);
        i += 6; // pub_key_prefix
        out_frame[i++] = out_path_len;
        i += mesh::Packet::writePath(&out_frame[i], out_path, out_path_len);
        out_frame[i++] = in_path_len;
        i += mesh::Packet::writePath(&out_frame[i], in_path, in_path_len);
        // NOTE: telemetry data in 'extra' is discarded at present

        _serial->writeFrame(out_frame, i);
      }
      return false;  // DON'T send reciprocal path!
    }
  }
  // let base class handle received path and data
  return BaseChatMesh::onContactPathRecv(contact, in_path, in_path_len, out_path, out_path_len, extra_type, extra, extra_len);
}

#define CTL_TYPE_NODE_DISCOVER_REQ  0x80
#define CTL_TYPE_NODE_DISCOVER_RESP 0x90

bool MyMesh::sendNodeDiscoverReq(bool silent) {
  bool scan_active = _pending_node_discover_tag != 0 &&
                     !millisHasNowPassed(_pending_node_discover_until);
  // A user-opened Discover screen may replace a silent wake scan immediately;
  // silent scans never displace an interactive one.
  if (!radioAvailable() || (scan_active && (silent || !_pending_node_discover_silent)))
    return false;
  if (scan_active) _repeater_signal.cancelDiscovery();
  uint8_t data[10];
  data[0] = CTL_TYPE_NODE_DISCOVER_REQ;
  // Zen's on-device Discover screen is repeater-only. Keep this standalone
  // request narrow so sensors do not spend airtime and power replying to
  // results the UI will discard. App discovery uses its existing path.
  data[1] = (1 << ADV_TYPE_REPEATER);
  getRNG()->random(&data[2], 4);
  memcpy(&_pending_node_discover_tag, &data[2], 4);
  _pending_node_discover_until = futureMillis(8000);
  _pending_node_discover_silent = silent;
  if (!silent) _discover_count = 0;
  uint32_t since = 0;
  memcpy(&data[6], &since, 4);
  auto pkt = createControlData(data, sizeof(data));
  if (!pkt) {
    _pending_node_discover_tag = 0;
    _pending_node_discover_silent = false;
    return false;
  }
  sendZeroHop(pkt);
  _repeater_signal.beginDiscovery();
  return true;
}

void MyMesh::onUserDisplayWake() {
  uint32_t now = millis();
  // Low Power never spends airtime on probing, including during Emergency.
  // Notification and alarm wakes do not call this entry point.
  if (_low_power_mode || !_repeater_signal.shouldScanOnUserWake(now, radioAvailable())) return;
  if (sendNodeDiscoverReq(true)) _repeater_signal.noteScanAttempt(now);
}

int MyMesh::getDiscoverResults(DiscoverResult dest[], int max_count) {
  int n = min(_discover_count, max_count);
  memcpy(dest, _discover_results, n * sizeof(DiscoverResult));
  return n;
}

// ── Ping/Trace functionality ─────────────────────────────────────────────────

uint32_t MyMesh::sendPing(const uint8_t* dest_pubkey, uint8_t hash_width) {
  if (hash_width == 0 || hash_width > 2) return 0;

  // Generate random tag and auth code
  uint32_t tag, auth;
  getRNG()->random((uint8_t*)&tag, 4);
  getRNG()->random((uint8_t*)&auth, 4);

  // Find a free slot in ping results
  int slot = -1;
  for (int i = 0; i < PING_RESULT_MAX; i++) {
    if (!_ping_results[i].received && _ping_results[i].tag == 0) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    return 0;
  }

  // Initialize ping result tracking
  PingResult& result = _ping_results[slot];
  result.tag = tag;
  result.auth_code = auth;
  result.snr_out_x4 = 0;
  result.snr_back_x4 = 0;
  result.rtt_ms = 0;
  result.received = false;
  result.sent_ms = millis();

  // Create path hash from destination public key
  uint8_t path_len = hash_width;
  uint8_t path[MAX_PATH_SIZE];
  memcpy(path, dest_pubkey, path_len);

  // Create and send trace packet
  auto pkt = createTrace(tag, auth, hash_width - 1); // flags = hash_width - 1
  if (pkt) {
    sendDirect(pkt, path, path_len);
    return tag;
  }

  // Failed to create packet
  memset(&result, 0, sizeof(result));
  return 0;
}

void MyMesh::setPingCallback(PingCallback cb, void* arg) {
  _ping_callback = cb;
  _ping_callback_arg = arg;
}

void MyMesh::clearPingResult(uint32_t tag) {
  if (tag == 0) return;
  for (int i = 0; i < PING_RESULT_MAX; i++) {
    if (_ping_results[i].tag == tag) {
      memset(&_ping_results[i], 0, sizeof(_ping_results[i]));
      return;
    }
  }
}

MyMesh::PingResult* MyMesh::getPingResult(uint32_t tag) {
  for (int i = 0; i < PING_RESULT_MAX; i++) {
    if (_ping_results[i].tag == tag) {
      return &_ping_results[i];
    }
  }
  return NULL;
}

// True when this (tag, responder) pair was already seen recently; records it
// otherwise. A discover response is often heard more than once — the zero-hop
// direct copy and a re-flooded copy relayed by another repeater carry
// different packet hashes, so the mesh duplicate filter passes both. Without
// this, the standalone scan appended the same node twice and an app-driven
// discover had both copies forwarded, so the app listed one repeater as two.
bool MyMesh::isDupDiscoverResp(uint32_t tag, const uint8_t* pub_key) {
  for (int i = 0; i < DISCOVER_SEEN_MAX; i++) {
    DiscoverSeen& e = _disc_seen[i];
    if (e.until != 0 && !millisHasNowPassed(e.until)
        && e.tag == tag && memcmp(e.pk, pub_key, sizeof(e.pk)) == 0) {
      return true;
    }
  }
  DiscoverSeen& s = _disc_seen[_disc_seen_head];
  _disc_seen_head = (_disc_seen_head + 1) % DISCOVER_SEEN_MAX;
  s.tag = tag;
  memcpy(s.pk, pub_key, sizeof(s.pk));
  s.until = futureMillis(10000);   // copies of one response arrive within seconds
  return false;
}

void MyMesh::onControlDataRecv(mesh::Packet *packet) {
  // If we have an active standalone discover, check if this is a matching response.
  // Tag matching provides isolation — no isBLEConnected check needed.
  if ((packet->payload[0] & 0xF0) == CTL_TYPE_NODE_DISCOVER_RESP &&
      packet->payload_len >= 6 + PUB_KEY_SIZE &&
      _pending_node_discover_tag != 0 &&
      !millisHasNowPassed(_pending_node_discover_until)) {
    uint32_t tag;
    memcpy(&tag, &packet->payload[2], 4);
    if (tag == _pending_node_discover_tag) {
      uint8_t node_type = packet->payload[0] & 0x0F;
      const uint8_t* pub_key = &packet->payload[6];
      if (isDupDiscoverResp(tag, pub_key)) return;  // second copy of the same response
      if (node_type == ADV_TYPE_REPEATER)
        _repeater_signal.noteDiscovery((int8_t)(_radio->getLastSNR() * 4));
      if (_pending_node_discover_silent) return;
      ContactInfo* known = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
      if (known) {
        known->lastmod = getRTCClock()->getCurrentTime();
      }
      if (_discover_count < DISCOVER_RESULTS_MAX) {
        DiscoverResult& r = _discover_results[_discover_count++];
        if (known) {
          strncpy(r.name, known->name, sizeof(r.name) - 1);
          r.name[sizeof(r.name) - 1] = '\0';
          r.is_known = true;
        } else {
          r.name[0] = '\0';
          r.is_known = false;
        }
        r.type = node_type;
        r.rssi = (int8_t)_radio->getLastRSSI();
        r.snr_x4 = (int8_t)(_radio->getLastSNR() * 4);
        r.remote_snr_x4 = (int8_t)packet->payload[1];
        memcpy(r.pub_key, pub_key, PUB_KEY_SIZE);
        r.timestamp = getRTCClock()->getCurrentTime();
      }
      return;  // our discover — don't forward to BLE app
    }
  }

  // App-driven discover: drop duplicate copies of the same response before
  // forwarding, so the app doesn't list one responder twice. (Our own
  // standalone discover already returned above on a tag match.)
  if ((packet->payload[0] & 0xF0) == CTL_TYPE_NODE_DISCOVER_RESP &&
      packet->payload_len >= 6 + PUB_KEY_SIZE) {
    uint32_t fwd_tag;
    memcpy(&fwd_tag, &packet->payload[2], 4);
    if (isDupDiscoverResp(fwd_tag, &packet->payload[6])) return;
  }

  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_CONTROL_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = packet->path_len;
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), data received while app offline");
  }
}

void MyMesh::onRawDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_RAW_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = 0xFF; // reserved (possibly path_len in future)
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), data received while app offline");
  }
}

void MyMesh::onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                         const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) {
  uint8_t path_sz = flags & 0x03;  // NEW v1.11+
  
  // Check if this is a response to our local ping
  for (int i = 0; i < PING_RESULT_MAX; i++) {
    if (_ping_results[i].tag == tag && _ping_results[i].auth_code == auth_code && !_ping_results[i].received) {
      PingResult& result = _ping_results[i];
      
      // Extract SNR values
      // path_snrs contains signed SNR values in dB×4 for each hop (in order).
      // The last value is the SNR at the destination hearing our request (snr_out).
      // The final SNR from packet is the SNR at us hearing the response (snr_back).
      uint8_t snr_count = path_len >> path_sz;
      
      if (snr_count > 0) {
        // Keep the encoded quarter-dB value; the UI converts it back to dB.
        result.snr_out_x4 = (int16_t)(int8_t)path_snrs[0];
      } else {
        result.snr_out_x4 = 0;
      }
      
      // SNR back is from the final SNR in the packet (SNR at us receiving response).
      result.snr_back_x4 = (int16_t)(packet->getSNR() * 4);
      
      // Calculate RTT
      unsigned long now = millis();
      if (now >= result.sent_ms) {
        result.rtt_ms = now - result.sent_ms;
      } else {
        result.rtt_ms = 0xFFFFFFFF; // Overflow
      }
      
      result.received = true;
      
      // Notify callback if set
      if (_ping_callback) {
        _ping_callback(tag, result.snr_out_x4, result.snr_back_x4, result.rtt_ms);
      }
      // The UI copies the result out of the callback, so reuse the slot immediately.
      memset(&result, 0, sizeof(result));
      break;
    }
  }

  // Forward to serial app regardless (for compatibility)
  if (12 + path_len + (path_len >> path_sz) + 1 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onTraceRecv(), path_len is too long: %d", (uint32_t)path_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_TRACE_DATA;
  out_frame[i++] = 0; // reserved
  out_frame[i++] = path_len;
  out_frame[i++] = flags;
  memcpy(&out_frame[i], &tag, 4);
  i += 4;
  memcpy(&out_frame[i], &auth_code, 4);
  i += 4;
  memcpy(&out_frame[i], path_hashes, path_len);
  i += path_len;

  memcpy(&out_frame[i], path_snrs, path_len >> path_sz);
  i += path_len >> path_sz;
  out_frame[i++] = (int8_t)(packet->getSNR() * 4); // extra/final SNR (to this node)

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onTraceRecv(), data received while app offline");
  }
}

uint32_t MyMesh::calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const {
  return SEND_TIMEOUT_BASE_MILLIS + (FLOOD_SEND_TIMEOUT_FACTOR * pkt_airtime_millis);
}
uint32_t MyMesh::calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const {
  uint8_t path_hash_count = path_len & 63;
  return SEND_TIMEOUT_BASE_MILLIS +
         ((pkt_airtime_millis * DIRECT_SEND_PERHOP_FACTOR + DIRECT_SEND_PERHOP_EXTRA_MILLIS) *
          (path_hash_count + 1));
}

void MyMesh::onSendTimeout() {
  // Required BaseChatMesh hook. Delivery retries are managed by the UI policy.
}

// A queued send can wait for CAD or duty-cycle budget. Only start its relay
// window after radio transmission completes; allow time for the repeater's
// randomised flood delay, which scales with packet airtime.
void MyMesh::logTx(mesh::Packet* packet, int len) {
  (void)len;
  _transport_trace.add(zen::TransportTrace::TX_COMPLETE, nullptr,
                       packet ? packet->getPayloadType() : 0);
  if (_relay_active == 0) return;
  uint8_t hash[MAX_HASH_SIZE];
  bool hashed = false;
  for (int i = 0; i < RELAY_RING; i++) {
    RelaySlot& s = _relay[i];
    if (!s.pending || s.transmitted || s.len != packet->payload_len) continue;
    if (!hashed) { packet->calculatePacketHash(hash); hashed = true; }
    if (memcmp(hash, s.hash, MAX_HASH_SIZE) != 0) continue;
    uint32_t airtime = _radio->getEstAirtimeFor(packet->getRawLength());
    s.deadline = futureMillis(zen::RelayEchoTiming::echoWindow(airtime));
    s.transmitted = true;
    break;
  }
}

void MyMesh::logTxFail(mesh::Packet* packet, int len) {
  (void)len;
  _transport_trace.add(zen::TransportTrace::TX_FAILED, nullptr,
                       packet ? packet->getPayloadType() : 0);
}

// Arm the UI "relayed into mesh" tracker for a channel send.
// A repeater rebroadcast heard within the window = relayed; no echo = simply not
// shown as relayed (NOT a failure — direct/0-hop neighbours never echo).
void MyMesh::trackRelaySend(const mesh::Packet* pkt) {
  RelaySlot& s = _relay[_relay_head];
  if (!s.pending) _relay_active++;   // overwriting an empty slot adds one pending
  pkt->calculatePacketHash(s.hash);
  s.len = pkt->payload_len;
  s.deadline = futureMillis(zen::RelayEchoTiming::QUEUE_WINDOW_MS);
  _relay_seq = (_relay_seq == 0xFFFFFFFFu) ? 1 : _relay_seq + 1;   // never 0 (0 = "no relay")
  s.seq = _relay_seq;
  s.heard = 0;
  s.transmitted = false;
  s.pending = true;
  _last_relay_seq = _relay_seq;
  _relay_head = (_relay_head + 1) % RELAY_RING;
}

MyMesh::MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui)
    // Sized to match simple_repeater's pool (32), not the old client-only 16: with the
    // on-device Repeater toggle, queued retransmits (adverts/channel flood from
    // neighbours) can now hold packet-pool slots for their retransmit delay window. A
    // pool too small for that starves Dispatcher::checkRecv()'s allocNew(), which then
    // silently drops every incoming packet — DMs and channels included — until a slot
    // frees up.
    : BaseChatMesh(radio, *new ArduinoMillis(), rng, rtc, *new StaticPoolPacketManager(32), tables),
      _serial(NULL), telemetry(MAX_PACKET_PAYLOAD - 4), _store(&store), _ui(ui), _iter(0) {
  _iter_started = false;
  _cli_rescue = false;
  for (int i = 0; i < RELAY_RING; i++) _relay[i].pending = false;
  _relay_head = 0;
  _relay_active = 0;
  _relay_seq = 0;
  _last_relay_seq = 0;
  offline_queue_len = 0;
  app_target_ver = 0;
  _next_auto_advert_ms = 0;
  _advert_indicator_until_ms = 0;
  clearPendingReqs();
  next_ack_idx = 0;
  sign_data = NULL;
  dirty_contacts_expiry = 0;
  memset(advert_paths, 0, sizeof(advert_paths));
  memset(send_scope.key, 0, sizeof(send_scope.key));
  _discover_count = 0;
  _pending_node_discover_tag = 0;
  _pending_node_discover_until = 0;
  _pending_node_discover_silent = false;
  _repeater_signal.reset();
  memset(_disc_seen, 0, sizeof(_disc_seen));
  _disc_seen_head = 0;

  // Ping state
  _ping_callback = NULL;
  _ping_callback_arg = NULL;
  memset(_ping_results, 0, sizeof(_ping_results));

  send_unscoped = false;

  // defaults
  // NodePrefs is a ConfigSerializer object with live nested parent pointers;
  // never byte-clear it. Reset only the derived Zen-owned storage region.
  resetZenFields(_prefs);
  _prefs.airtime_factor = 1.0;
  strcpy(_prefs.node_name, "NONAME");
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.gps_enabled = 0;       // GPS disabled by default
  _prefs.gps_interval = 0;      // Adaptive uses the continuous interval value
  _prefs.radio_fem_rxgain = 1;
  _prefs.radio_fem_txgain = 0;
  _prefs.setRepeatEn(false);
  _prefs.gps_adaptive = 1;      // Retain Adaptive while GPS power defaults off
  _prefs.display_brightness = 4; // full brightness (level 5 in the UI) by default
  _prefs.buzzer_volume = 4;      // max volume by default
  _prefs.ringtone_bpm_idx = 2;   // 120 bpm default
  _prefs.ringtone_len = 0;       // no custom ringtone by default
  _prefs.ringtone2_bpm_idx = 2;  // 120 bpm default
  _prefs.notif_melody_dm = zen::BuiltinMelodies::MESSAGE;
  _prefs.notif_melody_ch = zen::BuiltinMelodies::KERPLOP;
  _prefs.notif_melody_ad = zen::BuiltinMelodies::NONE;
  _prefs.notif_melody_new_contact = zen::BuiltinMelodies::NONE;
  _prefs.notification_screen_wake = 1; // message wakes are enabled by default
  _prefs.advert_sound_scope = ADVERT_SOUND_SCOPE_ALL;  // apply to all adverts when a sound is selected
  _prefs.home_pages_mask = ZenPrefs::HP_ALL;  // all available carousel pages visible by default
  zen::PrefsDefaults::apply(_prefs);
  _prefs.bot_enabled = 0;
  _prefs.bot_channel_enabled = 0;
  _prefs.bot_channel_idx = 0;
  _prefs.bot_trigger[0] = '\0';
  _prefs.bot_reply_dm[0] = '\0';
  _prefs.bot_reply_ch[0] = '\0';
  _prefs.bot_trigger_ch[0] = '\0';
  _prefs.bot_commands_enabled = 0;
  _prefs.bot_quiet_start = 0;
  _prefs.bot_quiet_end = 0;       // start==end → quiet hours disabled
  _prefs.dm_show_all = 1;        // show all contacts by default
  _prefs.reserved_dm_resend_count = 0;
  memset(_prefs.dm_notif, 0, sizeof(_prefs.dm_notif));
  _prefs.auto_off_secs = 15;    // 15 seconds auto-off by default
  _prefs.clock_hide_seconds = Features::CLOCK_HIDE_SECONDS_DEFAULT ? 1 : 0;
  _prefs.clock_12h = 1;        // 12-hour clock with AM/PM by default
  _prefs.tz_offset_hours = 0;  // UTC by default
  _prefs.timezone_mode = zen::TimezonePolicy::MANUAL;
  _prefs.timezone_city = zen::TimezonePolicy::DEFAULT_CITY;
  _prefs.timezone_manual_min = 0;
  _prefs.low_batt_mv = 0;  // reserved legacy field; ignored by BatteryPolicy
  _prefs.batt_display_mode = 0; // icon by default
  //_prefs.rx_delay_base = 10.0f;  enable once new algo fixed
#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default
#endif
#endif
}

void MyMesh::begin(bool has_display) {
  BaseChatMesh::begin();

  _identity_loaded_at_boot = _store->loadMainIdentity(self_id);
  if (!_identity_loaded_at_boot) {
    self_id = radio_new_identity(); // create new random identity
    int count = 0;
    while (count < 10 && (self_id.pub_key[0] == 0x00 || self_id.pub_key[0] == 0xFF)) { // reserved id hashes
      self_id = radio_new_identity();
      count++;
    }
    _store->saveMainIdentity(self_id);
  }

// if name is provided as a build flag, use that as default node name instead
#ifdef ADVERT_NAME
  strcpy(_prefs.node_name, ADVERT_NAME);
#else
  // use hex of first 4 bytes of identity public key as default node name
  char pub_key_hex[10];
  mesh::Utils::toHex(pub_key_hex, self_id.pub_key, 4);
  strcpy(_prefs.node_name, pub_key_hex);
#endif

  // if build provides default-scope, init with that
#ifdef DEFAULT_FLOOD_SCOPE_NAME
  strcpy(_prefs.default_scope_name, DEFAULT_FLOOD_SCOPE_NAME);
  {
    TransportKeyStore temp;
    TransportKey key;
    temp.getAutoKeyFor(0, "#" DEFAULT_FLOOD_SCOPE_NAME, key);
    memcpy(_prefs.default_scope_key, key.key, sizeof(key.key));
  }
#endif

  // MeshCore loads its own preferences directly. Zen then overlays only its
  // independent UI/application record from external flash.
  _store->loadPrefs(_prefs);
  sensors.node_lat = _prefs.node_lat;
  sensors.node_lon = _prefs.node_lon;

  // Keep MeshCore's original post-load validation exactly at the baseline
  // boundary. It protects radio use but does not write the repaired values.
  _prefs.rx_delay_base = constrain(_prefs.rx_delay_base, 0, 20.0f);
  _prefs.airtime_factor = constrain(_prefs.airtime_factor, 0, 9.0f);
  _prefs.freq = constrain(_prefs.freq, 150.0f, 2500.0f);
  _prefs.bw = constrain(_prefs.bw, 7.8f, 500.0f);
  _prefs.sf = constrain(_prefs.sf, 5, 12);
  _prefs.cr = constrain(_prefs.cr, 5, 8);
  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0UL, 86400UL);
  _baseline_fingerprint = zen::BaselinePrefsFingerprint::calculate(_prefs);
  _baseline_fingerprint_valid = true;
  _zen_store.begin(_store->getSecondaryFS(), _store->getPrimaryFS());
  bool zen_prefs_loaded = _zen_store.load(_prefs);

  // Constructor defaults are already the current Zen factory defaults. A
  // missing record is a fresh installation, not a schema-zero legacy record:
  // start it at the current schema so old migrations cannot reinterpret its
  // fields before the first save.
  if (!zen_prefs_loaded) {
    _prefs.zen_config_schema = zen::ConfigMaintenance::CURRENT_SCHEMA;
    _prefs.timezone_mode = zen::TimezonePolicy::DEFAULT_MODE;
    _prefs.timezone_city = zen::TimezonePolicy::DEFAULT_CITY;
    _prefs.timezone_manual_min = 0;
  }
  bool prefs_changed = zen::ConfigMaintenance::apply(_prefs) ||
                       !zen_prefs_loaded || _zen_store.recoveryNeeded();
  if (prefs_changed && !_zen_store.writeLocked()) {
    savePrefs();
  }

#ifdef BLE_PIN_CODE // 123456 by default
  if (_prefs.ble_pin == 0) {
#ifdef DISPLAY_CLASS
    if (has_display && BLE_PIN_CODE == 123456) {
      StdRNG rng;
      _active_ble_pin = rng.nextInt(100000, 999999); // random pin each session
    } else {
      _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
    }
#else
    _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
#endif
  } else {
    _active_ble_pin = _prefs.ble_pin;
  }
#else
  _active_ble_pin = 0;
#endif

  resetContacts();
  _store->loadContacts(this);
  bootstrapRTCfromContacts();
  // Preserve MeshCore's exact channel bootstrap and load sequence.
  addChannel("Public", PUBLIC_GROUP_PSK);
  _store->loadChannels(this);

  applyRadioParams();
  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");
}

void MyMesh::applyRadioParams() {
  radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  // Keep the baseline companion ordering: setParams() may reconfigure the
  // radio, so the persisted TX power must be applied afterwards at boot and
  // after every preset change.
  radio_driver.setTxPower(_prefs.tx_power_dbm);
}

void MyMesh::applyPowerState(bool low_power, bool emergency) {
  emergency = low_power && emergency;
  if (_low_power_mode == low_power && _emergency_mode == emergency) return;

  const bool was_available = radioAvailable();
  _low_power_mode = low_power;
  _emergency_mode = emergency;
  const bool available = radioAvailable();

  if (!available) {
    clearPendingReqs();
    // MeshCore exclusively owns its outbound queue. Suspending the peripheral
    // must not free, reorder or reinterpret packets already accepted by the
    // dispatcher.
    radio_driver.powerOff();
  } else if (!was_available) {
    // Restore the normal MeshCore wrapper and interrupt state. recvRaw() arms
    // continuous receive during the next application mesh loop.
    radio_driver.begin();
    applyRadioParams();
    radio_driver.setTxPower(_prefs.tx_power_dbm);
    radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  }

  if (!low_power)
    _next_auto_advert_ms = _prefs.advert_auto_interval_sec
        ? futureMillis(_prefs.advert_auto_interval_sec * 1000UL) : 0;
}

const char *MyMesh::getNodeName() {
  return _prefs.node_name;
}
ZenPrefs *MyMesh::getNodePrefs() {
  return &_prefs;
}
uint32_t MyMesh::getBLEPin() {
  return _active_ble_pin;
}

struct FreqRange {
  uint32_t lower_freq, upper_freq;
};

static FreqRange repeat_freq_ranges[] = {
  #ifdef ALLOWED_REPEAT_FREQ_RANGE
  ALLOWED_REPEAT_FREQ_RANGE
  #else
  { 433000, 433000 },
  { 869495, 869495 },
  { 918000, 918000 }
  #endif
};

bool MyMesh::isValidClientRepeatFreq(uint32_t f) const {
  for (int i = 0; i < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]); i++) {
    auto r = &repeat_freq_ranges[i];
    if (f >= r->lower_freq && f <= r->upper_freq) return true;
  }
  return false;
}

void MyMesh::startInterface(BaseSerialInterface &serial) {
  _serial = &serial;
  serial.enable();
}

static bool isAllZero(const uint8_t* buf, size_t n) {
  for (size_t i = 0; i < n; i++) if (buf[i]) return false;
  return true;
}

// Shared by the BLE CMD_SET_CHANNEL handler below and the on-device
// Channels add/edit/delete UI (ChannelsView) -- one place computing "was this
// a delete" so the two callers can't drift on the cleanup step.
MyMesh::ChannelSaveResult MyMesh::setChannelLocal(uint8_t idx, const ChannelDetails& ch) {
  ChannelDetails current;
  if (!getChannel(idx, current)) return CHANNEL_INVALID_SLOT;
  if (zen::ChannelSlotPolicy::duplicate<ChannelDetails>(*this, MAX_GROUP_CHANNELS,
                                                           idx, ch.channel.secret) >= 0)
    return CHANNEL_DUPLICATE;
  if (strncmp(current.name, ch.name, sizeof(ch.name)) == 0 &&
      memcmp(current.channel.secret, ch.channel.secret, sizeof(ch.channel.secret)) == 0)
    return CHANNEL_SAVED;
  if (!zen::ChannelSlotPolicy::saveWithRollback(idx, ch, current,
      [this](int slot, const ChannelDetails& value) { return setChannel(slot, value); },
      [this]() { return saveChannels(); })) return CHANNEL_SAVE_FAILED;
  // An all-zero secret is this codebase's "empty slot" sentinel (same check
  // loadChannels()/saveChannels() use) -- drop anything that referenced it by
  // index, the same way onContactRemoved() does for contacts.
  if (_ui && isAllZero(ch.channel.secret, sizeof(ch.channel.secret)))
    _ui->onChannelRemoved(idx);
  return CHANNEL_SAVED;
}

void MyMesh::handleCmdFrame(size_t len) {
  if (cmd_frame[0] == CMD_DEVICE_QUERY && len >= 2) { // sent when app establishes connection
    app_target_ver = cmd_frame[1];                    // which version of protocol does app understand

    int i = 0;
    out_frame[i++] = RESP_CODE_DEVICE_INFO;
    out_frame[i++] = FIRMWARE_VER_CODE;
    out_frame[i++] = MAX_CONTACTS / 2;   // v3+
    out_frame[i++] = MAX_GROUP_CHANNELS; // v3+
    memcpy(&out_frame[i], &_prefs.ble_pin, 4);
    i += 4;
    memset(&out_frame[i], 0, 12);
    strcpy((char *)&out_frame[i], FIRMWARE_BUILD_DATE);
    i += 12;
    StrHelper::strzcpy((char *)&out_frame[i], board.getManufacturerName(), 40);
    i += 40;
    StrHelper::strzcpy((char *)&out_frame[i], FIRMWARE_VERSION, 20);
    i += 20;
    out_frame[i++] = _prefs.isRepeatEn() ? 1 : 0;   // v9+
    out_frame[i++] = _prefs.path_hash_mode;  // v10+
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_APP_START &&
             len >= 8) { // sent when app establishes connection, respond with node ID
    //  cmd_frame[1..7]  reserved future
    char *app_name = (char *)&cmd_frame[8];
    cmd_frame[len] = 0; // make app_name null terminated
    MESH_DEBUG_PRINTLN("App %s connected", app_name);

    _iter_started = false; // stop any left-over ContactsIterator
    int i = 0;
    out_frame[i++] = RESP_CODE_SELF_INFO;
    out_frame[i++] = ADV_TYPE_CHAT; // what this node Advert identifies as (maybe node's pronouns too?? :-)
    out_frame[i++] = _prefs.tx_power_dbm;
    out_frame[i++] = MAX_LORA_TX_POWER;
    memcpy(&out_frame[i], self_id.pub_key, PUB_KEY_SIZE);
    i += PUB_KEY_SIZE;

    int32_t lat, lon;
    lat = (sensors.node_lat * 1000000.0);
    lon = (sensors.node_lon * 1000000.0);
    memcpy(&out_frame[i], &lat, 4);
    i += 4;
    memcpy(&out_frame[i], &lon, 4);
    i += 4;
    out_frame[i++] = _prefs.multi_acks; // new v7+
    out_frame[i++] = _prefs.advert_loc_policy;
    out_frame[i++] = (_prefs.telemetry_mode_env << 4) | (_prefs.telemetry_mode_loc << 2) |
                     (_prefs.telemetry_mode_base); // v5+
    out_frame[i++] = _prefs.manual_add_contacts;

    uint32_t freq = _prefs.freq * 1000;
    memcpy(&out_frame[i], &freq, 4);
    i += 4;
    uint32_t bw = _prefs.bw * 1000;
    memcpy(&out_frame[i], &bw, 4);
    i += 4;
    out_frame[i++] = _prefs.sf;
    out_frame[i++] = _prefs.cr;

    int tlen = strlen(_prefs.node_name); // revisit: UTF_8 ??
    memcpy(&out_frame[i], _prefs.node_name, tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_TXT_MSG && len >= 14) {
    int i = 1;
    uint8_t txt_type = cmd_frame[i++];
    uint8_t attempt = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    uint8_t *pub_key_prefix = &cmd_frame[i];
    i += 6;
    ContactInfo *recipient = lookupContactByPubKey(pub_key_prefix, 6);
    if (recipient && (txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_CLI_DATA)) {
      char *text = (char *)&cmd_frame[i];
      int tlen = len - i;
      uint32_t est_timeout;
      text[tlen] = 0; // ensure null
      int result;
      uint32_t expected_ack;
      if (txt_type == TXT_TYPE_CLI_DATA) {
        // Do not reject or consume the app's request. Stop the local wait,
        // so app and local edits cannot race, and drain late replies.
        bool local_waiting = _admin_session.pending();
        uint8_t local_key[PUB_KEY_SIZE];
        memcpy(local_key, _admin_session.key(), sizeof(local_key));
        msg_timestamp = getRTCClock()->getCurrentTimeUnique(); // Use node's RTC instead of app timestamp to avoid tripping replay protection
        result = sendCommandData(*recipient, msg_timestamp, attempt, text, est_timeout);
        _admin_session.appCommand(millis(), result == MSG_SEND_FAILED ? 0 : est_timeout + 4000);
        if (local_waiting && _ui) _ui->onAdminReply(local_key, "App command active; result uncertain");
        expected_ack = 0; // no Ack expected
      } else {
        result = sendMessage(*recipient, msg_timestamp, attempt, text, expected_ack, est_timeout);
        if (result != MSG_SEND_FAILED && _ui)
          _ui->addDMMsg(recipient->id.pub_key, true, text, msg_timestamp);
      }
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        if (expected_ack) {
          expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis(); // add to circular table
          expected_ack_table[next_ack_idx].ack = expected_ack;
          expected_ack_table[next_ack_idx].contact = recipient;
          next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
        }

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &expected_ack, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(recipient == NULL
                        ? ERR_CODE_NOT_FOUND
                        : ERR_CODE_UNSUPPORTED_CMD); // unknown recipient, or unsupported TXT_TYPE_*
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_TXT_MSG) { // send GroupChannel text msg
    int i = 1;
    uint8_t txt_type = cmd_frame[i++]; // should be TXT_TYPE_PLAIN
    uint8_t channel_idx = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    const char *text = (char *)&cmd_frame[i];

    if (txt_type != TXT_TYPE_PLAIN) {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    } else {
      ChannelDetails channel;
      bool success = getChannel(channel_idx, channel);
      if (success && sendGroupMessage(msg_timestamp, channel.channel, _prefs.node_name, text, len - i)) {
        writeOKFrame();
#ifdef DISPLAY_CLASS
        if (_ui) {
          int text_len = len - i;
          if (text_len > MAX_TEXT_LEN) text_len = MAX_TEXT_LEN;
          int pos = _ui->addOwnChannelMsg(channel_idx, text, text_len, msg_timestamp);
          if (pos >= 0) _ui->armChannelRelay(pos, lastChannelRelaySeq());
        }
#endif
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
      }
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_DATA) { // send GroupChannel datagram
    if (len < 4) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }
    int i = 1;
    uint8_t channel_idx = cmd_frame[i++];
    uint8_t path_len = cmd_frame[i++];

    // validate path len, allowing 0xFF for flood
    if (!mesh::Packet::isValidPathLen(path_len) && path_len != OUT_PATH_UNKNOWN) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA invalid path size: %d", path_len);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }

    // parse provided path if not flood
    uint8_t path[MAX_PATH_SIZE];
    if (path_len != OUT_PATH_UNKNOWN) {
      i += mesh::Packet::writePath(path, &cmd_frame[i], path_len);
    }

    uint16_t data_type = ((uint16_t)cmd_frame[i]) | (((uint16_t)cmd_frame[i + 1]) << 8);
    i += 2;
    const uint8_t *payload = &cmd_frame[i];
    int payload_len = (len > (size_t)i) ? (int)(len - i) : 0;

    ChannelDetails channel;
    if (!getChannel(channel_idx, channel)) {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    } else if (data_type == DATA_TYPE_RESERVED) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (payload_len > MAX_CHANNEL_DATA_LENGTH) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA payload too long: %d > %d", payload_len, MAX_CHANNEL_DATA_LENGTH);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (sendGroupData(channel.channel, path, path_len, data_type, payload, payload_len)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACTS) { // get Contact list
    if (_iter_started) {
      writeErrFrame(ERR_CODE_BAD_STATE); // iterator is currently busy
    } else {
      if (len >= 5) { // has optional 'since' param
        memcpy(&_iter_filter_since, &cmd_frame[1], 4);
      } else {
        _iter_filter_since = 0;
      }

      uint8_t reply[5];
      reply[0] = RESP_CODE_CONTACTS_START;
      uint32_t count = getNumContacts(); // total, NOT filtered count
      memcpy(&reply[1], &count, 4);
      _serial->writeFrame(reply, 5);

      // start iterator
      _iter = startContactsIterator();
      _iter_started = true;
      _most_recent_lastmod = 0;
    }
  } else if (cmd_frame[0] == CMD_SET_ADVERT_NAME && len >= 2) {
    int nlen = len - 1;
    if (nlen > sizeof(_prefs.node_name) - 1) nlen = sizeof(_prefs.node_name) - 1; // max len
    memcpy(_prefs.node_name, &cmd_frame[1], nlen);
    _prefs.node_name[nlen] = 0; // null terminator
    saveBaselinePrefsFromCompanionAndReply();
  } else if (cmd_frame[0] == CMD_SET_ADVERT_LATLON && len >= 9) {
    int32_t lat, lon, alt = 0;
    memcpy(&lat, &cmd_frame[1], 4);
    memcpy(&lon, &cmd_frame[5], 4);
    if (len >= 13) {
      memcpy(&alt, &cmd_frame[9], 4); // for FUTURE support
    }
    if (lat <= 90 * 1E6 && lat >= -90 * 1E6 && lon <= 180 * 1E6 && lon >= -180 * 1E6) {
      sensors.node_lat = ((double)lat) / 1000000.0;
      sensors.node_lon = ((double)lon) / 1000000.0;
      saveBaselinePrefsFromCompanionAndReply();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid geo coordinate
    }
  } else if (cmd_frame[0] == CMD_GET_DEVICE_TIME) {
    uint8_t reply[5];
    reply[0] = RESP_CODE_CURR_TIME;
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply[1], &now, 4);
    _serial->writeFrame(reply, 5);
  } else if (cmd_frame[0] == CMD_SET_DEVICE_TIME && len >= 5) {
    uint32_t secs;
    memcpy(&secs, &cmd_frame[1], 4);
    if (zen::DeviceTimePolicy::valid(secs)) {
      // Companion time is authoritative. Always call setCurrentTime(), even
      // for a zero-delta correction, so BootTimeSync observes a live source
      // and clears SYNC TIME without waiting for GPS.
      getRTCClock()->setCurrentTime(secs);
      if (_ui) _ui->onTimeSynchronized(zen::TimeSyncSource::COMPANION);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SEND_SELF_ADVERT) {
    // Optional parameter: 1 = flood, 0 (or omitted) = zero hop.
    if (sendConfiguredSelfAdvert(len >= 2 && cmd_frame[1] == 1)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_RESET_PATH && len >= 1 + 32) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      // recipient->lastmod = ??   shouldn't be needed, app already has this version of contact
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // unknown contact
    }
  } else if (cmd_frame[0] == CMD_ADD_UPDATE_CONTACT && len >= 1 + 32 + 2 + 1) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint32_t last_mod = getRTCClock()->getCurrentTime();  // fallback value if not present in cmd_frame
    if (recipient) {
      updateContactFromFrame(*recipient, last_mod, cmd_frame, len);
      recipient->lastmod = last_mod;
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      ContactInfo contact;
      updateContactFromFrame(contact, last_mod, cmd_frame, len);
      contact.lastmod = last_mod;
      contact.sync_since = 0;
      if (addContact(contact)) {
        dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_REMOVE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient && removeContact(*recipient)) {
      _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE);
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found, or unable to remove
    }
  } else if (cmd_frame[0] == CMD_SHARE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      if (shareContactZeroHop(*recipient)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // unable to send
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACT_BY_KEY) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *contact = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (contact) {
      writeContactRespFrame(RESP_CODE_CONTACT, *contact);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found
    }
  } else if (cmd_frame[0] == CMD_EXPORT_CONTACT) {
    if (len < 1 + PUB_KEY_SIZE) {
      // export SELF
      mesh::Packet* pkt = createConfiguredSelfAdvert();
      if (pkt) {
        pkt->header |= ROUTE_TYPE_FLOOD; // would normally be sent in this mode

        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        uint8_t out_len = pkt->writeTo(&out_frame[1]);
        releasePacket(pkt); // undo the obtainNewPacket()
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // Error
      }
    } else {
      uint8_t *pub_key = &cmd_frame[1];
      ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
      uint8_t out_len;
      if (recipient && (out_len = exportContact(*recipient, &out_frame[1])) > 0) {
        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // not found
      }
    }
  } else if (cmd_frame[0] == CMD_IMPORT_CONTACT && len > 2 + 32 + 64) {
    if (importContact(&cmd_frame[1], len - 1)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SYNC_NEXT_MESSAGE) {
    int out_len;
    if ((out_len = getFromOfflineQueue(out_frame)) > 0) {
      _serial->writeFrame(out_frame, out_len);
#ifdef DISPLAY_CLASS
      if (_ui) _ui->msgRead(offline_queue_len);
#endif
    } else {
      out_frame[0] = RESP_CODE_NO_MORE_MESSAGES;
      _serial->writeFrame(out_frame, 1);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_PARAMS) {
    int i = 1;
    uint32_t freq;
    memcpy(&freq, &cmd_frame[i], 4);
    i += 4;
    uint32_t bw;
    memcpy(&bw, &cmd_frame[i], 4);
    i += 4;
    uint8_t sf = cmd_frame[i++];
    uint8_t cr = cmd_frame[i++];
    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

    // Dedicated-band requirement for app-driven repeat disabled, to match the
    // on-device Repeater toggle (Tools > Repeater), which repeats on whatever
    // frequency is already set with no band restriction. Uncomment to restore
    // the old behaviour (repeat=1 only accepted on repeat_freq_ranges, i.e.
    // 433.000/869.495/918.000 MHz exactly, by default).
    // if (repeat && !isValidClientRepeatFreq(freq)) {
    //   writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    // } else
    if (freq >= 150000 && freq <= 2500000 && sf >= 5 && sf <= 12 && cr >= 5 && cr <= 8 && bw >= 7000 &&
        bw <= 500000) {
      _prefs.sf = sf;
      _prefs.cr = cr;
      _prefs.freq = (float)freq / 1000.0;
      _prefs.bw = (float)bw / 1000.0;
      _prefs.setRepeatEn(repeat != 0);
      if (!saveBaselinePrefsFromCompanion()) {
        writeErrFrame(ERR_CODE_FILE_IO_ERROR);
        return;
      }
      applyRadioParams();
      MESH_DEBUG_PRINTLN("OK: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);
      writeOKFrame();

    } else {
      MESH_DEBUG_PRINTLN("Error: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_TX_POWER) {
    int8_t power = (int8_t)cmd_frame[1];
    if (power < -9 || power > MAX_LORA_TX_POWER) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.tx_power_dbm = power;
      if (saveBaselinePrefsFromCompanion()) {
        radio_driver.setTxPower(_prefs.tx_power_dbm);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_FILE_IO_ERROR);
      }
    }
  } else if (cmd_frame[0] == CMD_SET_TUNING_PARAMS) {
    int i = 1;
    uint32_t rx, af;
    memcpy(&rx, &cmd_frame[i], 4);
    i += 4;
    memcpy(&af, &cmd_frame[i], 4);
    i += 4;
    _prefs.rx_delay_base = ((float)rx) / 1000.0f;
    _prefs.airtime_factor = ((float)af) / 1000.0f;
    saveBaselinePrefsFromCompanionAndReply();
  } else if (cmd_frame[0] == CMD_GET_TUNING_PARAMS) {
    uint32_t rx = _prefs.rx_delay_base * 1000, af = _prefs.airtime_factor * 1000;
    int i = 0;
    out_frame[i++] = RESP_CODE_TUNING_PARAMS;
    memcpy(&out_frame[i], &rx, 4); i += 4;
    memcpy(&out_frame[i], &af, 4); i += 4;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SET_OTHER_PARAMS) {
    _prefs.manual_add_contacts = cmd_frame[1];
    if (len >= 3) {
      _prefs.telemetry_mode_base = cmd_frame[2] & 0x03; // v5+
      _prefs.telemetry_mode_loc = (cmd_frame[2] >> 2) & 0x03;
      _prefs.telemetry_mode_env = (cmd_frame[2] >> 4) & 0x03;

      if (len >= 4) {
        _prefs.advert_loc_policy = cmd_frame[3];
        if (len >= 5) {
          _prefs.multi_acks = cmd_frame[4];
        }
      }
    }
    saveBaselinePrefsFromCompanionAndReply();
  } else if (cmd_frame[0] == CMD_SET_PATH_HASH_MODE && cmd_frame[1] == 0 && len >= 3) {
    if (cmd_frame[2] >= 3) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.path_hash_mode = cmd_frame[2];
      saveBaselinePrefsFromCompanionAndReply();
    }
  } else if (cmd_frame[0] == CMD_REBOOT && memcmp(&cmd_frame[1], "reboot", 6) == 0) {
    if (dirty_contacts_expiry) saveContacts();
    board.reboot();
  } else if (cmd_frame[0] == CMD_GET_BATT_AND_STORAGE) {
    uint8_t reply[11];
    int i = 0;
    reply[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t battery_millivolts = board.getBattMilliVolts();
    StorageStatus storage = getStorageStatus(true);
    uint32_t used = storage.used_kb;
    uint32_t total = storage.total_kb;
    memcpy(&reply[i], &battery_millivolts, 2); i += 2;
    memcpy(&reply[i], &used, 4); i += 4;
    memcpy(&reply[i], &total, 4); i += 4;
    _serial->writeFrame(reply, i);
  } else if (cmd_frame[0] == CMD_EXPORT_PRIVATE_KEY) {
#if ENABLE_PRIVATE_KEY_EXPORT
    uint8_t reply[65];
    reply[0] = RESP_CODE_PRIVATE_KEY;
    self_id.writeTo(&reply[1], 64);
    _serial->writeFrame(reply, 65);
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_IMPORT_PRIVATE_KEY && len >= 65) {
#if ENABLE_PRIVATE_KEY_IMPORT
    if (!mesh::LocalIdentity::validatePrivateKey(&cmd_frame[1])) {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid key
    } else {
        mesh::LocalIdentity identity;
        identity.readFrom(&cmd_frame[1], 64);
        // A restore may send contacts before identity. MeshCore's normal lazy
        // write must complete before its identity handler resets and reloads
        // the contact table, otherwise the just-imported RAM entries vanish.
        flushDirtyContacts();
        if (_store->saveMainIdentity(identity)) {
          self_id = identity;
          writeOKFrame();
          // re-load contacts, to invalidate ecdh shared_secrets
          resetContacts();
          _store->loadContacts(this);
        } else {
          writeErrFrame(ERR_CODE_FILE_IO_ERROR);
        }
    }
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_SEND_RAW_DATA && len >= 6) {
    int i = 1;
    int8_t path_len = cmd_frame[i++];
    if (path_len >= 0 && i + path_len + 4 <= len) { // minimum 4 byte payload
      uint8_t *path = &cmd_frame[i];
      i += path_len;
      auto pkt = createRawData(&cmd_frame[i], len - i);
      if (pkt) {
        sendDirect(pkt, path, path_len);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    } else {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // flood, not supported (yet)
    }
  } else if (cmd_frame[0] == CMD_SEND_LOGIN && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    char *password = (char *)&cmd_frame[1 + PUB_KEY_SIZE];
    cmd_frame[len] = 0; // ensure null terminator in password
    if (recipient) {
      uint32_t est_timeout;
      int result = sendLogin(*recipient, password, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        memcpy(&pending_login, recipient->id.pub_key, 4); // match this to onContactResponse()
        // App login takes priority over an ambiguous UI login. Cancel the UI
        // wait explicitly without treating its saved credential as rejected.
        if (_ui_login_pending) {
          uint8_t cancelled[PUB_KEY_SIZE];
          memcpy(cancelled, _ui_pending_login_key, sizeof(cancelled));
          _ui_login_pending = false;
          _ui->onNodeLoginCancelled(cancelled);
        }
        _app_login_deadline = millis() + est_timeout + 4000;
        strncpy(pending_login_pw, password, sizeof(pending_login_pw) - 1); // saved on success if it's a room
        pending_login_pw[sizeof(pending_login_pw) - 1] = 0;
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &pending_login, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_ANON_REQ && len > 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    ContactInfo anon;
    if (recipient == NULL) { // FIRMWARE_VER_CODE 13+,  allow non-contact requests
      memset(&anon, 0, sizeof(anon));
      memcpy(anon.id.pub_key, pub_key, PUB_KEY_SIZE);
      anon.out_path_len = 0;   // default to zero-hop direct
      anon.type = ADV_TYPE_NONE;  // unknown

      if (addContact(anon)) recipient = &anon;
    }
    uint8_t *data = &cmd_frame[1 + PUB_KEY_SIZE];
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendAnonReq(*recipient, data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL); // contacts full
    }
  } else if (cmd_frame[0] == CMD_SEND_STATUS_REQ && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_STATUS, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        // FUTURE:  pending_status = tag;  // match this in onContactResponse()
        memcpy(&pending_status, recipient->id.pub_key, 4); // legacy matching scheme
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_PATH_DISCOVERY_REQ && cmd_frame[1] == 0 && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[2];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      // 'Path Discovery' is just a special case of flood + Telemetry req
      uint8_t req_data[9];
      req_data[0] = REQ_TYPE_GET_TELEMETRY_DATA;
      req_data[1] = ~(TELEM_PERM_BASE);  // NEW: inverse permissions mask (ie. we only want BASE telemetry)
      memset(&req_data[2], 0, 3);  // reserved
      getRNG()->random(&req_data[5], 4);   // random blob to help make packet-hash unique
      auto save = recipient->out_path_len;    // temporarily force sendRequest() to flood
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      int result = sendRequest(*recipient, req_data, sizeof(req_data), tag, est_timeout);
      recipient->out_path_len = save;
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_discovery = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len >= 4 + PUB_KEY_SIZE) {  // can deprecate, in favour of CMD_SEND_BINARY_REQ
    uint8_t *pub_key = &cmd_frame[4];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_TELEMETRY_DATA, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_telemetry = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len == 4) {  // 'self' telemetry request
    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    // query other sensors -- target specific
    sensors.querySensors(0xFF, telemetry);

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], self_id.pub_key, 6);
    i += 6; // pub_key_prefix
    uint8_t tlen = telemetry.getSize();
    memcpy(&out_frame[i], telemetry.getBuffer(), tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_BINARY_REQ && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint8_t *req_data = &cmd_frame[1 + PUB_KEY_SIZE];
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, req_data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_HAS_CONNECTION && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    if (hasConnectionTo(pub_key)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_LOGOUT && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    stopConnection(pub_key);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_CHANNEL && len >= 2) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    if (getChannel(channel_idx, channel)) {
      int i = 0;
      out_frame[i++] = RESP_CODE_CHANNEL_INFO;
      out_frame[i++] = channel_idx;
      strcpy((char *)&out_frame[i], channel.name);
      i += 32;
      memcpy(&out_frame[i], channel.channel.secret, 16);
      i += 16; // NOTE: only 128-bit supported
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 32) {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // baseline supports 128-bit channel keys
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 16) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    StrHelper::strncpy(channel.name, (char *)&cmd_frame[2], 32);
    memset(channel.channel.secret, 0, sizeof(channel.channel.secret));
    memcpy(channel.channel.secret, &cmd_frame[2 + 32], 16); // 128-bit key
    if (setChannel(channel_idx, channel)) {
      saveChannels();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    }
  } else if (cmd_frame[0] == CMD_SIGN_START) {
    out_frame[0] = RESP_CODE_SIGN_START;
    out_frame[1] = 0; // reserved
    uint32_t len = MAX_SIGN_DATA_LEN;
    memcpy(&out_frame[2], &len, 4);
    _serial->writeFrame(out_frame, 6);

    if (sign_data) {
      free(sign_data);
    }
    sign_data = (uint8_t *)malloc(MAX_SIGN_DATA_LEN);
    sign_data_len = 0;
  } else if (cmd_frame[0] == CMD_SIGN_DATA && len > 1) {
    if (sign_data == NULL || sign_data_len + (len - 1) > MAX_SIGN_DATA_LEN) {
      writeErrFrame(sign_data == NULL ? ERR_CODE_BAD_STATE : ERR_CODE_TABLE_FULL); // error: too long
    } else {
      memcpy(&sign_data[sign_data_len], &cmd_frame[1], len - 1);
      sign_data_len += (len - 1);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SIGN_FINISH) {
    if (sign_data) {
      self_id.sign(&out_frame[1], sign_data, sign_data_len);

      free(sign_data); // don't need sign_data now
      sign_data = NULL;

      out_frame[0] = RESP_CODE_SIGNATURE;
      _serial->writeFrame(out_frame, 1 + SIGNATURE_SIZE);
    } else {
      writeErrFrame(ERR_CODE_BAD_STATE);
    }
  } else if (cmd_frame[0] == CMD_SEND_TRACE_PATH && len > 10 && len - 10 < MAX_PACKET_PAYLOAD-5) {
    uint8_t path_len = len - 10;
    uint8_t flags = cmd_frame[9];
    uint8_t path_sz = flags & 0x03;  // NEW v1.11+
    if ((path_len >> path_sz) > MAX_PATH_SIZE || (path_len % (1 << path_sz)) != 0) { // make sure is multiple of path_sz
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      uint32_t tag, auth;
      memcpy(&tag, &cmd_frame[1], 4);
      memcpy(&auth, &cmd_frame[5], 4);
      auto pkt = createTrace(tag, auth, flags);
      if (pkt) {
        sendDirect(pkt, &cmd_frame[10], path_len);

        uint32_t t = _radio->getEstAirtimeFor(pkt->payload_len + pkt->path_len + 2);
        uint32_t est_timeout = calcDirectTimeoutMillisFor(t, path_len >> path_sz);

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_SET_DEVICE_PIN && len >= 5) {

    // get pin from command frame
    uint32_t pin;
    memcpy(&pin, &cmd_frame[1], 4);

    // ensure pin is zero, or a valid 6 digit pin
    if (pin == 0 || (pin >= 100000 && pin <= 999999)) {
      _prefs.ble_pin = pin;
      saveBaselinePrefsFromCompanionAndReply();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_CUSTOM_VARS) {
    out_frame[0] = RESP_CODE_CUSTOM_VARS;
    char *dp = (char *)&out_frame[1];
    for (int i = 0; i < sensors.getNumSettings() && dp - (char *)&out_frame[1] < 140; i++) {
      if (i > 0) {
        *dp++ = ',';
      }
      strcpy(dp, sensors.getSettingName(i));
      dp = strchr(dp, 0);
      *dp++ = ':';
      strcpy(dp, sensors.getSettingValue(i));
      dp = strchr(dp, 0);
    }
    _serial->writeFrame(out_frame, dp - (char *)out_frame);
  } else if (cmd_frame[0] == CMD_SET_CUSTOM_VAR && len >= 4) {
    cmd_frame[len] = 0;
    char *sp = (char *)&cmd_frame[1];
    char *np = strchr(sp, ':'); // look for separator char
    if (np) {
      *np++ = 0; // modify 'cmd_frame', replace ':' with null
      bool success = sensors.setSettingValue(sp, np);
      if (success) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_ADVERT_PATH && len >= PUB_KEY_SIZE+2) {
    // FUTURE use:  uint8_t reserved = cmd_frame[1];
    uint8_t *pub_key = &cmd_frame[2];
    AdvertPath* found = NULL;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {
      auto p = &advert_paths[i];
      if (memcmp(p->pubkey_prefix, pub_key, sizeof(p->pubkey_prefix)) == 0) {
        found = p;
        break;
      }
    }
    if (found) {
      int i = 0;
      out_frame[i++] = RESP_CODE_ADVERT_PATH;
      memcpy(&out_frame[i], &found->recv_timestamp, 4); i += 4;
      out_frame[i++] = found->path_len;
      i += mesh::Packet::writePath(&out_frame[i], found->path, found->path_len);
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_STATS && len >= 2) {
    uint8_t stats_type = cmd_frame[1];
    if (stats_type == STATS_TYPE_CORE) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_CORE;
      uint16_t battery_mv = board.getBattMilliVolts();
      uint32_t uptime_secs = _ms->getMillis() / 1000;
      uint8_t queue_len = (uint8_t)_mgr->getOutboundTotal();
      memcpy(&out_frame[i], &battery_mv, 2); i += 2;
      memcpy(&out_frame[i], &uptime_secs, 4); i += 4;
      memcpy(&out_frame[i], &_err_flags, 2); i += 2;
      out_frame[i++] = queue_len;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_RADIO) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_RADIO;
      int16_t noise_floor = (int16_t)_radio->getNoiseFloor();
      int8_t last_rssi = (int8_t)radio_driver.getLastRSSI();
      int8_t last_snr = (int8_t)(radio_driver.getLastSNR() * 4); // scaled by 4 for 0.25 dB precision
      uint32_t tx_air_secs = getTotalAirTime() / 1000;
      uint32_t rx_air_secs = getReceiveAirTime() / 1000;
      memcpy(&out_frame[i], &noise_floor, 2); i += 2;
      out_frame[i++] = last_rssi;
      out_frame[i++] = last_snr;
      memcpy(&out_frame[i], &tx_air_secs, 4); i += 4;
      memcpy(&out_frame[i], &rx_air_secs, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_PACKETS) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_PACKETS;
      uint32_t recv = radio_driver.getPacketsRecv();
      uint32_t sent = radio_driver.getPacketsSent();
      uint32_t n_sent_flood = getNumSentFlood();
      uint32_t n_sent_direct = getNumSentDirect();
      uint32_t n_recv_flood = getNumRecvFlood();
      uint32_t n_recv_direct = getNumRecvDirect();
      uint32_t n_recv_errors = radio_driver.getPacketsRecvErrors();
      memcpy(&out_frame[i], &recv, 4); i += 4;
      memcpy(&out_frame[i], &sent, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_errors, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid stats sub-type
    }
  } else if (cmd_frame[0] == CMD_FACTORY_RESET && memcmp(&cmd_frame[1], "reset", 5) == 0) {
    if (_serial) {
      MESH_DEBUG_PRINTLN("Factory reset: disabling serial interface to prevent reconnects (BLE/WiFi)");
      _serial->disable(); // Phone app disconnects before we can send OK frame so it's safe here
    }
    bool success = _store->formatFileSystem();
    if (success) {
      writeOKFrame();
      delay(1000);
      board.reboot();  // doesn't return
    } else {
      writeErrFrame(ERR_CODE_FILE_IO_ERROR);
    }
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 0) {
    if (len >= 2 + 16) {
      memcpy(send_scope.key, &cmd_frame[2], sizeof(send_scope.key));  // set scope override TransportKey
    } else {
      memset(send_scope.key, 0, sizeof(send_scope.key));  // reset scope override
    }
    send_unscoped = false;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 1) {  // ver 12+
    send_unscoped = true;
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_DEFAULT_FLOOD_SCOPE && len >= 1) {
    if (len >= 1+31+16) {
      // strnlen, not strlen: the name field is always 31 bytes in the frame
      // even if the actual name is shorter, so we must bound the search to
      // avoid reading into the key (or past the frame) when no NUL is present.
      int n = (int)strnlen((char *) &cmd_frame[1], 31);
      if (n > 0 && n < 31) {
        if (setDefaultFloodScope((char*)&cmd_frame[1], &cmd_frame[1+31])) writeOKFrame();
        else writeErrFrame(ERR_CODE_FILE_IO_ERROR);
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      if (setDefaultFloodScope("", nullptr)) writeOKFrame();
      else writeErrFrame(ERR_CODE_FILE_IO_ERROR);
    }
  } else if (cmd_frame[0] == CMD_GET_DEFAULT_FLOOD_SCOPE) {
    out_frame[0] = RESP_CODE_DEFAULT_FLOOD_SCOPE;
    if (strlen(_prefs.default_scope_name) > 0) {
      memcpy(&out_frame[1], _prefs.default_scope_name, 31);
      memcpy(&out_frame[1+31], _prefs.default_scope_key, 16);
      _serial->writeFrame(out_frame, 1+31+16);
    } else {
      _serial->writeFrame(out_frame, 1);   // no name or key means null
    }
  } else if (cmd_frame[0] == CMD_SEND_CONTROL_DATA && len >= 2 && (cmd_frame[1] & 0x80) != 0) {
    auto resp = createControlData(&cmd_frame[1], len - 1);
    if (resp) {
      sendZeroHop(resp);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_SET_AUTOADD_CONFIG) {
    _prefs.autoadd_config = cmd_frame[1];
    if (len >= 3) {
      _prefs.autoadd_max_hops = min(cmd_frame[2], (uint8_t)64);
    }
    saveBaselinePrefsFromCompanionAndReply();
  } else if (cmd_frame[0] == CMD_GET_AUTOADD_CONFIG) {
    int i = 0;
    out_frame[i++] = RESP_CODE_AUTOADD_CONFIG;
    out_frame[i++] = _prefs.autoadd_config;
    out_frame[i++] = _prefs.autoadd_max_hops;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_GET_ALLOWED_REPEAT_FREQ) {
    int i = 0;
    out_frame[i++] = RESP_ALLOWED_REPEAT_FREQ;
    for (int k = 0; k < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]) && i + 8 < sizeof(out_frame); k++) {
      auto r = &repeat_freq_ranges[k];
      memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
      memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
    }
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_RAW_PACKET && len >= 4) {
    auto pkt = obtainNewPacket();
    if (pkt) {
      uint8_t priority = cmd_frame[1];
      if (tryParsePacket(pkt, &cmd_frame[2], len - 2)) {
        sendPacket(pkt, priority, 0);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    MESH_DEBUG_PRINTLN("ERROR: unknown command: %02X", cmd_frame[0]);
  }
}

static bool save_filter(const ContactInfo& c) {
  return c.type != ADV_TYPE_NONE;   // don't save the transient/anon entries
}

void MyMesh::saveContacts() {
  _store->saveContacts(this, save_filter);
}

void MyMesh::enterCLIRescue() {
  _cli_rescue = true;
  cli_command[0] = 0;
  Serial.println("========= CLI Rescue =========");
}

void MyMesh::checkCLIRescueCmd() {
  int len = strlen(cli_command);
  while (Serial.available() && len < sizeof(cli_command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      cli_command[len++] = c;
      cli_command[len] = 0;
    }
    Serial.print(c);  // echo
  }
  if (len == sizeof(cli_command)-1) {  // command buffer full
    cli_command[sizeof(cli_command)-1] = '\r';
  }

  if (len > 0 && cli_command[len - 1] == '\r') {  // received complete line
    cli_command[len - 1] = 0;  // replace newline with C string null terminator

    if (memcmp(cli_command, "set ", 4) == 0) {
      const char* config = &cli_command[4];
      if (memcmp(config, "pin ", 4) == 0) {
        _prefs.ble_pin = atoi(&config[4]);
        if (saveBaselinePrefsFromCompanion())
          Serial.printf("  > pin is now %06d\n", _prefs.ble_pin);
        else {
          Serial.println("  Error: settings save failed");
          if (_ui) _ui->onOperationResult(zen::OperationResult::make(
            zen::Operation::STORAGE, zen::OperationOutcome::TRANSPORT_FAILURE,
            zen::OperationReason::SAVE_FAILED));
        }
      } else {
        Serial.printf("  Error: unknown config: %s\n", config);
      }
    } else if (strcmp(cli_command, "rebuild") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        _store->saveMainIdentity(self_id);
        savePrefs();
        saveContacts();
        saveChannels();
        Serial.println("  > erase and rebuild done");
      } else {
        Serial.println("  Error: erase failed");
        if (_ui) _ui->onOperationResult(zen::OperationResult::make(
          zen::Operation::STORAGE, zen::OperationOutcome::TRANSPORT_FAILURE,
          zen::OperationReason::DELETE_FAILED));
      }
    } else if (strcmp(cli_command, "erase") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        Serial.println("  > erase done");
      } else {
        Serial.println("  Error: erase failed");
        if (_ui) _ui->onOperationResult(zen::OperationResult::make(
          zen::Operation::STORAGE, zen::OperationOutcome::TRANSPORT_FAILURE,
          zen::OperationReason::DELETE_FAILED));
      }
    } else if (memcmp(cli_command, "ls", 2) == 0) {

      // get path from command e.g: "ls /adafruit"
      const char *path = &cli_command[3];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }
      Serial.printf("Listing files in %s\n", path);

      // log each file and directory
      File root = _store->openRead(path);
      if (is_fs2 == false) {
        if (root) {
          File file = root.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  UserData%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] UserData%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root.openNextFile();
          }
          root.close();
        }
      }

      if (is_fs2 == true || strlen(path) == 0 || strcmp(path, "/") == 0) {
        if (_store->getSecondaryFS() != nullptr) {
          File root2 = _store->openRead(_store->getSecondaryFS(), path);
          File file = root2.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  ExtraFS%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] ExtraFS%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root2.openNextFile();
          }
          root2.close();
        }
      }
    } else if (memcmp(cli_command, "cat", 3) == 0) {

      // get path from command e.g: "cat /contacts3"
      const char *path = &cli_command[4];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      } else {
        Serial.println("Invalid path provided, must start with UserData/ or ExtraFS/");
        cli_command[0] = 0;
        return;
      }

      // log file content as hex
      File file = _store->openRead(path);
      if (is_fs2 == true) {
        file = _store->openRead(_store->getSecondaryFS(), path);
      }
      if(file){

        // get file content
        int file_size = file.available();
        uint8_t buffer[file_size];
        file.read(buffer, file_size);

        // print hex
        mesh::Utils::printHex(Serial, buffer, file_size);
        Serial.print("\n");

        file.close();

      }

    } else if (memcmp(cli_command, "rm ", 3) == 0) {
      // get path from command e.g: "rm /adv_blobs"
      const char *path = &cli_command[3];
      MESH_DEBUG_PRINTLN("Removing file: %s", path);
      // ensure path is not empty, or root dir
      if(!path || strlen(path) == 0 || strcmp(path, "/") == 0){
        Serial.println("Invalid path provided");
      } else {
      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }

        // remove file
        bool removed;
        if (is_fs2) {
          MESH_DEBUG_PRINTLN("Removing file from ExtraFS: %s", path);
          removed = _store->removeFile(_store->getSecondaryFS(), path);
        } else {
          MESH_DEBUG_PRINTLN("Removing file from UserData: %s", path);
          removed = _store->removeFile(path);
        }
        if(removed){
          Serial.println("File removed");
        } else {
          Serial.println("Failed to remove file");
          if (_ui) _ui->onOperationResult(zen::OperationResult::make(
            zen::Operation::STORAGE, zen::OperationOutcome::TRANSPORT_FAILURE,
            zen::OperationReason::DELETE_FAILED));
        }

      }

    } else if (strcmp(cli_command, "reboot") == 0) {
      if (_ui) _ui->shutdown(true);
      else {
        savePrefs();
        flushDirtyContacts();
        board.reboot();
      }
    } else {
      Serial.println("  Error: unknown command");
    }

    cli_command[0] = 0;  // reset command buffer
  }
}

void MyMesh::checkSerialInterface() {
  size_t len = _serial->checkRecvFrame(cmd_frame);
  if (len > 0) {
    handleCmdFrame(len);
  } else if (_iter_started              // check if our ContactsIterator is 'running'
             && !_serial->isWriteBusy() // don't spam the Serial Interface too quickly!
  ) {
    ContactInfo contact;
    bool found = false;
    while (_iter.hasNext(this, contact)) {
      if (contact.type != ADV_TYPE_NONE) {
        found = true;
        break;
      }
    }

    if (found) {
      if (contact.lastmod > _iter_filter_since) { // apply the 'since' filter
        writeContactRespFrame(RESP_CODE_CONTACT, contact);
        if (contact.lastmod > _most_recent_lastmod) {
          _most_recent_lastmod = contact.lastmod; // save for the RESP_CODE_END_OF_CONTACTS frame
        }
      }
    } else { // EOF
      out_frame[0] = RESP_CODE_END_OF_CONTACTS;
      memcpy(&out_frame[1], &_most_recent_lastmod,
             4); // include the most recent lastmod, so app can update their 'since'
      _serial->writeFrame(out_frame, 5);
      _iter_started = false;
    }
  //} else if (!_serial->isWriteBusy()) {
  //  checkConnections();    // TODO - deprecate the 'Connections' stuff
  }
}

void MyMesh::loop() {
  // Low Power controls whether Zen invokes the baseline mesh loop; it does not
  // insert a second state machine into the MeshCore dispatcher.
  if (radioAvailable()) BaseChatMesh::loop();

  if (_pending_node_discover_tag != 0 &&
      millisHasNowPassed(_pending_node_discover_until)) {
    _repeater_signal.finishDiscovery(millis());
    _pending_node_discover_tag = 0;
    _pending_node_discover_silent = false;
  }

  if (_low_power_mode && !_emergency_mode) return;

  // Close UI relay-count windows. A zero count becomes a failed-to-hear marker;
  // one or more echoes retain their final count.
  if (_relay_active > 0) {
    for (int i = 0; i < RELAY_RING; i++) {
      if (_relay[i].pending && millisHasNowPassed(_relay[i].deadline)) {
        if (_ui) _ui->onChannelRelayExpired(_relay[i].seq, _relay[i].heard,
                                            _relay[i].transmitted);
        _relay[i].pending = false;
        _relay_active--;
      }
    }
  }


  if (_cli_rescue) {
    checkCLIRescueCmd();
  } else {
    checkSerialInterface();
  }

  // is there are pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    saveContacts();
    dirty_contacts_expiry = 0;
  }

  if (!_low_power_mode && _prefs.advert_auto_interval_sec > 0 && millisHasNowPassed(_next_auto_advert_ms)) {
    if (!sendConfiguredSelfAdvert(false) && _ui)
      _ui->onOperationResult(zen::OperationResult::make(
          zen::Operation::ADVERT, zen::OperationOutcome::TRANSPORT_FAILURE,
          zen::OperationReason::SEND_FAILED, zen::RESULT_BACKGROUND));
    _next_auto_advert_ms = futureMillis(_prefs.advert_auto_interval_sec * 1000UL);
  }

#ifdef DISPLAY_CLASS
  // Drive the BT status indicator, pairing PIN and GPX-export collision warning
  // from the actual Bluetooth connection state.
  if (_ui) _ui->setHasConnection(_ui->isBLEConnected());
#endif
}

bool MyMesh::advert() {
  if (_low_power_mode) return false;
  return sendConfiguredSelfAdvert(false);
}

// One privacy gate for every self-advert producer. advert_loc_policy is the
// existing MeshCore setting exposed by companion apps; any location-sharing
// mode includes the current stored/fixed position, while NONE emits no
// coordinates at all.
mesh::Packet* MyMesh::createConfiguredSelfAdvert() {
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE)
    return createSelfAdvert(_prefs.node_name);
  return createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
}

// Shared transmit path for timed, on-device, and companion-app adverts.
// The caller chooses reach; payload/privacy and UI state cannot drift between
// triggers. Contact export deliberately uses createConfiguredSelfAdvert()
// directly because it serializes a packet without transmitting it.
bool MyMesh::sendConfiguredSelfAdvert(bool flood) {
  mesh::Packet* pkt = createConfiguredSelfAdvert();
  if (!pkt) return false;

  if (flood) {
    TransportKey default_scope;
    if (_prefs.default_scope_name[0])
      memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
    else
      memset(&default_scope.key, 0, sizeof(default_scope.key));
    sendFloodScoped(default_scope, pkt, 0);
  } else {
    sendZeroHop(pkt);
  }
  noteAdvertQueued();
  return true;
}

// The RF send itself is normally shorter than a UI refresh interval, so expose
// a brief wrap-safe pulse after an advert is queued. This is presentation state
// only: it does not alter advert timing, routing, or the outbound queue.
void MyMesh::noteAdvertQueued() {
  _advert_indicator_until_ms = futureMillis(5000);
}

bool MyMesh::advertIndicatorActive() const {
  return _advert_indicator_until_ms != 0 &&
         (int32_t)(_advert_indicator_until_ms - millis()) > 0;
}

// To check if there is pending work
bool MyMesh::hasPendingWork() const {
  return _mgr->getOutboundTotal() > 0 || dirty_contacts_expiry != 0;
}
