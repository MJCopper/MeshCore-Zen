#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SignalFormat.h"

namespace zen {
namespace admin {

enum Group { STATUS, SETTINGS, RADIO, ROUTING, ROOM, CONSOLE, ACTIONS, GROUP_COUNT };
enum TargetKind { TARGET_REPEATER, TARGET_ROOM, TARGET_SENSOR };
enum Kind { READ, TEXT, WRITE_TEXT, NUMBER, TOGGLE, FREQUENCY, BANDWIDTH, SF, CR, ACTION };
struct Field {
  Group group;
  const char* label;
  const char* get;
  const char* set;
  Kind kind;
  float min, max, step;
};
static const char* const GROUP_LABELS[] = {
  "Status", "Settings", "Radio", "Routing", "Room", "Console", "Actions"
};
// Commands verified against CommonCLI in the MeshCore baseline. Keep this
// table separate from rendering and transport; unsupported replies stay raw.
static const Field FIELDS[] = {
  {STATUS, "Version", "ver", nullptr, READ, 0, 0, 0},
  {STATUS, "Board", "board", nullptr, READ, 0, 0, 0},
  {STATUS, "Clock", "clock", nullptr, READ, 0, 0, 0},
  {STATUS, "Role", "get role", nullptr, READ, 0, 0, 0},
  {STATUS, "Public key", "get public.key", nullptr, READ, 0, 0, 0},
  {STATUS, "Neighbours", "neighbors", nullptr, READ, 0, 0, 0},
  {SETTINGS, "Name", "get name", "set name", TEXT, 0, 31, 0},
  {SETTINGS, "Owner info", "get owner.info", "set owner.info", TEXT, 0, 119, 0},
  {SETTINGS, "Admin password", nullptr, "password", WRITE_TEXT, 0, 15, 0},
  {SETTINGS, "Latitude", "get lat", "set lat", NUMBER, -90, 90, 0.001f},
  {SETTINGS, "Longitude", "get lon", "set lon", NUMBER, -180, 180, 0.001f},
  {SETTINGS, "Local advert (min)", "get advert.interval", "set advert.interval", NUMBER, 0, 240, 2},
  {SETTINGS, "Flood advert (hr)", "get flood.advert.interval", "set flood.advert.interval", NUMBER, 0, 168, 1},
  {RADIO, "Frequency (MHz)", "get radio", "set radio", FREQUENCY, 150, 2500, 0.001f},
  {RADIO, "Bandwidth (kHz)", "get radio", "set radio", BANDWIDTH, 0, 0, 0},
  {RADIO, "Spreading factor", "get radio", "set radio", SF, 5, 12, 1},
  {RADIO, "Coding rate", "get radio", "set radio", CR, 5, 8, 1},
  {RADIO, "TX power (dBm)", "get tx", "set tx", NUMBER, -9, 30, 1},
  {RADIO, "RX boosted gain", "get radio.rxgain", "set radio.rxgain", TOGGLE, 0, 1, 1},
  {ROUTING, "Forwarding", "get repeat", "set repeat", TOGGLE, 0, 1, 1},
  {ROUTING, "Path hash mode", "get path.hash.mode", "set path.hash.mode", NUMBER, 0, 2, 1},
  {ROUTING, "Max flood hops", "get flood.max", "set flood.max", NUMBER, 0, 64, 1},
  {ROUTING, "Max unscoped hops", "get flood.max.unscoped", "set flood.max.unscoped", NUMBER, 0, 64, 1},
  {ROUTING, "Max advert hops", "get flood.max.advert", "set flood.max.advert", NUMBER, 0, 64, 1},
  {ROUTING, "RX delay", "get rxdelay", "set rxdelay", NUMBER, 0, 20, 0.1f},
  {ROUTING, "TX delay", "get txdelay", "set txdelay", NUMBER, 0, 2, 0.1f},
  {ROUTING, "Direct TX delay", "get direct.txdelay", "set direct.txdelay", NUMBER, 0, 2, 0.1f},
  {ROUTING, "Duty cycle (%)", "get dutycycle", "set dutycycle", NUMBER, 1, 100, 1},
  {ROUTING, "Channel detection", "get cad", "set cad", TOGGLE, 0, 1, 1},
  {ROUTING, "Multi ACKs", "get multi.acks", "set multi.acks", NUMBER, 0, 1, 1},
  {ROUTING, "Interference", "get int.thresh", "set int.thresh", NUMBER, 0, 255, 1},
  {ROUTING, "AGC reset (sec)", "get agc.reset.interval", "set agc.reset.interval", NUMBER, 0, 1020, 4},
  {ROOM, "Guest password", "get guest.password", "set guest.password", TEXT, 0, 15, 0},
  {ROOM, "Read-only access", "get allow.read.only", "set allow.read.only", TOGGLE, 0, 1, 1},
  {ACTIONS, "Send advert", "advert", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Zero-hop advert", "advert.zerohop", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Sync clock", "clock sync", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Clear stats", "clear stats", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Start OTA", "start ota", nullptr, ACTION, 0, 0, 0},
  {ACTIONS, "Reboot", "reboot", nullptr, ACTION, 0, 0, 0},
};
static const int FIELD_COUNT = sizeof(FIELDS) / sizeof(FIELDS[0]);

inline bool groupAllowed(TargetKind target, Group group) {
  if (group == ROOM) return target == TARGET_ROOM;
  return true;
}
inline int groupCount(TargetKind target) {
  int count = 0;
  for (int group = 0; group < GROUP_COUNT; group++)
    if (groupAllowed(target, (Group)group)) count++;
  return count;
}
inline Group groupAt(TargetKind target, int visible_index) {
  for (int group = 0; group < GROUP_COUNT; group++) {
    if (!groupAllowed(target, (Group)group)) continue;
    if (visible_index-- == 0) return (Group)group;
  }
  return STATUS;
}

inline bool radioTuple(Kind kind) { return kind >= FREQUENCY && kind <= CR; }
inline const char* value(const char* reply) {
  return reply && reply[0] == '>' && reply[1] == ' ' ? reply + 2 : nullptr;
}
// Status commands in the MeshCore CLI return their text directly, while
// `get` commands prefix values with "> ". Read-only fields accept both wire
// formats; editable fields continue to require the explicit value prefix.
inline const char* readValue(const char* reply) {
  const char* parsed = value(reply);
  return parsed ? parsed : (reply ? reply : "");
}
inline bool formatNeighbourMetrics(char* out, size_t size, uint32_t age_seconds,
                                   int snr_x4) {
  char age[12];
  if (age_seconds < 5) snprintf(age, sizeof(age), "now");
  else if (age_seconds < 60) snprintf(age, sizeof(age), "%lus",
                                      (unsigned long)age_seconds);
  else if (age_seconds < 3600) snprintf(age, sizeof(age), "%lum",
                                        (unsigned long)(age_seconds / 60));
  else if (age_seconds < 86400) snprintf(age, sizeof(age), "%luh",
                                         (unsigned long)(age_seconds / 3600));
  else snprintf(age, sizeof(age), "%lud",
                (unsigned long)(age_seconds / 86400));
  char signal[10];
  if (!formatQuarterDb(signal, sizeof(signal), snr_x4)) return false;
  int n = snprintf(out, size, "%s %s", age, signal);
  return n >= 0 && (size_t)n < size;
}
inline bool formatNeighbourLine(char* out, size_t size, const char* name,
                                size_t name_len, uint32_t age_seconds, int snr_x4,
                                size_t line_limit = 19) {
  char metrics[20];
  if (!out || !size || !name ||
      !formatNeighbourMetrics(metrics, sizeof(metrics), age_seconds, snr_x4)) return false;
  size_t metrics_len = strlen(metrics);
  if (line_limit <= metrics_len + 1) return false;
  size_t keep = line_limit - metrics_len - 1;
  if (keep > name_len) keep = name_len;
  // Do not truncate a UTF-8 name inside a continuation sequence.
  while (keep && keep < name_len && (((uint8_t)name[keep] & 0xC0) == 0x80)) keep--;
  size_t padding = line_limit - keep - metrics_len;
  int n = snprintf(out, size, "%.*s%*s%s", (int)keep, name, (int)padding, "", metrics);
  return n >= 0 && (size_t)n < size && (size_t)n <= line_limit;
}
inline bool number(const char* text, float min, float max, float& result) {
  char* end;
  float parsed = strtof(text, &end);
  if (*end == '%') end++;
  if (end == text || *end || !isfinite(parsed) || parsed < min || parsed > max) return false;
  result = parsed;
  return true;
}
inline bool parseRadio(const char* text, float& freq, float& bw, uint8_t& sf, uint8_t& cr) {
  int s, c, end = 0;
  float f, b;
  if (sscanf(text, "%f,%f,%d,%d%n", &f, &b, &s, &c, &end) != 4 || text[end] ||
      !isfinite(f) || !isfinite(b) || f < 150 || f > 2500 || b < 7 || b > 500 ||
      s < 5 || s > 12 || c < 5 || c > 8) return false;
  freq = f; bw = b; sf = (uint8_t)s; cr = (uint8_t)c;
  return true;
}
inline bool formatRadio(char* out, size_t size, float f, float b, uint8_t s, uint8_t c) {
  int n = snprintf(out, size, "%.3f,%.3f,%u,%u", f, b, s, c);
  return n >= 0 && (size_t)n < size;
}
inline bool formatCommand(char* out, size_t size, const char* prefix, const char* val) {
  int n = snprintf(out, size, "%s %s", prefix, val);
  return n >= 0 && (size_t)n < size;
}
inline bool confirmed(const char* reply) {
  return strcmp(reply, "OK") == 0 || strncmp(reply, "OK - ", 5) == 0;
}
inline bool valuesEqual(const Field& field, const char* expected, const char* actual) {
  if (!expected || !actual) return false;
  if (radioTuple(field.kind)) {
    float ef, eb, af, ab;
    uint8_t es, ec, as, ac;
    return parseRadio(expected, ef, eb, es, ec) &&
           parseRadio(actual, af, ab, as, ac) &&
           fabsf(ef - af) < 0.0005f && fabsf(eb - ab) < 0.0005f &&
           es == as && ec == ac;
  }
  if (field.kind == NUMBER) {
    float e, a;
    if (!number(expected, field.min, field.max, e) ||
        !number(actual, field.min, field.max, a)) return false;
    float tolerance = field.step > 0 ? field.step * 0.01f : 0.0001f;
    return fabsf(e - a) <= tolerance;
  }
  return strcmp(expected, actual) == 0;
}
inline float stepNumber(const Field& field, float value, int direction) {
  float next = value + direction * field.step;
  // Zero disables advertising; positive intervals have baseline minima.
  float first = !strcmp(field.get, "get advert.interval") ? 60 :
                (!strcmp(field.get, "get flood.advert.interval") ? 3 : 0);
  if (first && next > 0 && next < first) next = direction > 0 ? first : 0;
  return next >= field.min && next <= field.max ? next : value;
}

} // namespace admin
} // namespace zen
