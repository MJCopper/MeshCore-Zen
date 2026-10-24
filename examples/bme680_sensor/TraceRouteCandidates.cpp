#include "TraceRouteCandidates.h"
#include "TraceRouteHistory.h"
#include <Utils.h>
#include <string.h>

TraceRoute::Validation TraceRoute::fromRequest(const mesh::Packet* packet, TraceRoute& route) {
  if (!packet || !packet->isRouteFlood() || packet->getPayloadType() != PAYLOAD_TYPE_GRP_TXT ||
      !mesh::Packet::isValidPathLen(packet->path_len)) return INVALID;
  uint8_t count = packet->getPathHashCount(), size = packet->getPathHashSize();
  if (!count) return EMPTY;
  if (count > MAX_REPEATERS) return TOO_LONG;
  if (size > 2) return INVALID;
  route.repeaters = count;
  route.flags = size - 1;
  route.bytes = (2 * count - 1) * size;
  for (uint8_t i = 0; i < 2 * count - 1; i++) {
    uint8_t source = i < count ? count - 1 - i : i - count + 1;
    memcpy(&route.path[i * size], &packet->path[source * size], size);
  }
  return VALID;
}

TraceRoute::Validation TraceRoute::fromOutbound(const uint8_t* via, uint8_t encoded,
                                               const uint8_t* target, TraceRoute& route) {
  if (!target || !mesh::Packet::isValidPathLen(encoded)) return INVALID;
  uint8_t count = encoded & 63, width = (encoded >> 6) + 1;
  if (width > 2 || (count && !via)) return INVALID;
  if (count + 1 > MAX_REPEATERS) return TOO_LONG;
  route = TraceRoute{};
  route.repeaters = count + 1;
  route.flags = width - 1;
  route.bytes = (2 * route.repeaters - 1) * width;
  for (uint8_t i = 0; i < count; i++) memcpy(route.path + i * width, via + i * width, width);
  memcpy(route.path + count * width, target, width);
  for (uint8_t i = 0; i < count; i++)
    memcpy(route.path + (count + 1 + i) * width, via + (count - 1 - i) * width, width);
  return VALID;
}

bool TraceRoute::equals(const TraceRoute& other) const {
  return flags == other.flags && bytes == other.bytes && memcmp(path, other.path, bytes) == 0;
}

void TraceRouteCandidates::begin(const mesh::Packet* request, uint32_t now, const mesh::PacketScope& scope) {
  // Called after channel MAC validation; later copies must match that ciphertext.
  clear();
  _scope = scope;
  if (TraceRoute::fromRequest(request, _original) != TraceRoute::VALID ||
      request->payload_len < 3 || request->payload_len > MAX_PACKET_PAYLOAD) return;
  _payload_length = request->payload_len;
  mesh::Utils::sha256(_fingerprint, sizeof(_fingerprint), request->payload, request->payload_len);
  _active = true;
  _last_seen = now;
  _sequence = 0;
}

void TraceRouteCandidates::observe(const mesh::Packet* packet, uint32_t now, const mesh::PacketScope& scope) {
  if (!_active || !_scope.equals(scope) || !packet || !packet->isRouteFlood() ||
      packet->getPayloadType() != PAYLOAD_TYPE_GRP_TXT || packet->payload_len != _payload_length) return;
  uint8_t fingerprint[sizeof(_fingerprint)];
  mesh::Utils::sha256(fingerprint, sizeof(fingerprint), packet->payload, packet->payload_len);
  if (memcmp(fingerprint, _fingerprint, sizeof(fingerprint)) != 0) return;
  TraceRoute route{};
  if (TraceRoute::fromRequest(packet, route) != TraceRoute::VALID) return;
  observeDiscovered(route, now, (int8_t)(packet->getSNR() * 4));
}

void TraceRouteCandidates::beginDiscovered(const TraceRoute& route, uint32_t now,
                                          const mesh::PacketScope& scope) {
  clear(); _original = route; _scope = scope; _last_seen = now; _sequence = 0;
}

void TraceRouteCandidates::observeDiscovered(const TraceRoute& route, uint32_t now, int8_t snr) {
  _last_seen = now;
  if (route.equals(_original)) return;
  Candidate* target = NULL;
  for (uint8_t i = 0; i < _count; i++)
    if (_candidates[i].route.equals(route)) { target = &_candidates[i]; break; }
  if (!target) {
    if (_count < 3) target = &_candidates[_count++];
    else {
      target = &_candidates[0];
      for (auto& candidate : _candidates)
        if (candidate.sequence < target->sequence) target = &candidate;
    }
    *target = Candidate{};
    target->route = route;
  }
  target->seen = now;
  target->sequence = ++_sequence;
  target->snr = snr;
  if (target->observations != 255) target->observations++;
}

// Compare directed edges, treating the sensor as a distinct endpoint. Prefix
// matches across hash widths are conservative hints, not proven node identities.
static bool sameVertex(const TraceRoute& a, int i, const TraceRoute& b, int j) {
  int an = 2 * a.repeaters - 1, bn = 2 * b.repeaters - 1;
  bool as = i < 0 || i == an, bs = j < 0 || j == bn;
  if (as || bs) return as == bs;
  uint8_t width = a.flags < b.flags ? 1 << a.flags : 1 << b.flags;
  return memcmp(a.path + i * (1 << a.flags), b.path + j * (1 << b.flags), width) == 0;
}

static uint8_t novelEdges(const TraceRoute& route, const TraceRoute& original) {
  uint8_t novel = 0;
  for (int i = -1; i < 2 * route.repeaters - 1; i++) {
    bool found = false;
    for (int j = -1; j < 2 * original.repeaters - 1; j++)
      if (sameVertex(route, i, original, j) && sameVertex(route, i + 1, original, j + 1)) { found = true; break; }
    if (!found) novel++;
  }
  return novel;
}

const TraceRoute* TraceRouteCandidates::best(const TraceRouteHistory& history, uint8_t channel,
                      uint32_t profile, uint32_t now, const uint32_t airtimes[3], const mesh::PacketScope& scope, const uint8_t* target_key) const {
  int chosen = -1;
  for (uint8_t i = 0; i < _count; i++) {
    if (chosen < 0) { chosen = i; continue; }
    const auto& a = _candidates[i];
    const auto& b = _candidates[chosen];
    uint8_t ar = history.rank(a.route, channel, profile, now, scope, target_key), br = history.rank(b.route, channel, profile, now, scope, target_key);
    uint64_t ad = (uint64_t)novelEdges(a.route, _original) * b.route.repeaters * (airtimes[chosen] + 1ULL);
    uint64_t bd = (uint64_t)novelEdges(b.route, _original) * a.route.repeaters * (airtimes[i] + 1ULL);
    bool better = ar > br;
    if (ar == br) {
      if (ad != bd) better = ad > bd;
      else if (airtimes[i] != airtimes[chosen]) better = airtimes[i] < airtimes[chosen];
      else if (a.seen != b.seen) better = (uint32_t)(now - a.seen) < (uint32_t)(now - b.seen);
      else if (a.snr != b.snr) better = a.snr > b.snr;
      else better = a.sequence > b.sequence;
    }
    if (better) chosen = i;
  }
  return chosen < 0 ? NULL : &_candidates[chosen].route;
}
