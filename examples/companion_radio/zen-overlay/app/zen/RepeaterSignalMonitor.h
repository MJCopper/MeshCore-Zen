#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>

namespace zen {

// Bounded, RAM-only reception evidence. No timers here transmit packets.
class RepeaterSignalMonitor {
public:
  static constexpr uint32_t WINDOW_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t SCAN_INTERVAL_MS = 30UL * 60UL * 1000UL;
  static constexpr uint32_t DUPLICATE_MS = 30000;
  static constexpr unsigned LINK_COUNT = 5, SAMPLE_COUNT = 4, HISTORY_COUNT = 16;
  enum Source : uint8_t { ROUTED, ADVERT, DISCOVERY };
  enum Status : uint8_t { NONE, FRESH, STALE, UNAVAILABLE };
  struct Snapshot {
    Status status = NONE;
    uint8_t bars = 0, count = 0;
    bool identified = false;
    uint8_t key[32]{};
    int16_t snr_x4 = 0, rssi = 0;
    uint32_t age_ms = 0;
  };
  struct Event {
    uint32_t at = 0;
    uint8_t key[32]{};
    int16_t snr_x4 = 0, rssi = 0;
    Source source = ROUTED;
    Snapshot selected;
  };
private:
  struct Sample {
    uint32_t at = 0;
    int16_t snr = 0, rssi = 0;
    uint8_t token[8]{};
    bool token_valid = false;
  };
  struct Link {
    uint8_t key[32]{};
    Sample samples[SAMPLE_COUNT];
    uint32_t last = 0;
    uint8_t count = 0, head = 0, shown = 0, pending = 0, confirmations = 0, mask = 0;
    bool used = false;
  };
  // Slot zero is deliberately unattributed; path hashes are not identities.
  Link _links[LINK_COUNT + 1];
  Event _history[HISTORY_COUNT];
  uint8_t _history_count = 0, _history_head = 0;
  bool _ever = false, _have_scan = false, _discovering = false, _discover_seen = false;
  uint32_t _last_scan = 0, _failed_scans = 0;
  float _freq = 0, _bw = 0;
  uint8_t _sf = 7, _cr = 0;

  struct Average { int sum = 0, rssi = 0; uint8_t count = 0, mask = 0; uint32_t age = UINT32_MAX; };
  static Average average(const Link& link, uint32_t now) {
    Average a;
    for (unsigned i = 0; i < link.count; ++i) {
      const auto& s = link.samples[i];
      uint32_t age = now - s.at;
      if (age >= WINDOW_MS) continue;
      a.sum += s.snr; a.rssi += s.rssi; ++a.count; a.mask |= 1 << i;
      if (age < a.age) a.age = age;
    }
    return a;
  }
  static int floorX4(uint8_t sf) { return -10 - 10 * (int(sf) - 5); }
  static uint8_t classify(int margin, int count) {
    if (margin >= 70 * count) return 4;
    if (margin > 58 * count) return 3;
    if (margin > 50 * count) return 2;
    return 1;
  }
  static int boundary(uint8_t lower_bar) {
    return lower_bar == 1 ? 50 : lower_bar == 2 ? 58 : 70;
  }
  void smooth(Link& link, const Average& a) {
    const int margin = a.sum - floorX4(_sf) * a.count;
    const uint8_t raw = classify(margin, a.count);
    if (!link.shown || link.mask != a.mask) {
      // First evidence or expired evidence bypasses the transition latch.
      link.shown = raw; link.pending = link.confirmations = 0;
    } else if (raw == link.shown) {
      link.pending = link.confirmations = 0;
    } else {
      const bool up = raw > link.shown;
      const int edge = boundary(up ? raw - 1 : link.shown - 1);
      // Keep original strictness at 5/7 dB, inclusiveness at 10 dB.
      const bool beyond = up ? (raw == 4 ? margin >= (edge+4)*a.count
                                                       : margin > (edge+4)*a.count)
                            : (link.shown == 4 ? margin < (edge-4)*a.count
                                              : margin <= (edge-4)*a.count);
      if (!beyond) link.pending = link.confirmations = 0;
      else {
        if (link.pending != raw) { link.pending = raw; link.confirmations = 0; }
        if (++link.confirmations >= 2) {
          link.shown = raw; link.pending = link.confirmations = 0;
        }
      }
    }
    link.mask = a.mask;
  }
public:
  void reset() { *this = RepeaterSignalMonitor(); }
  bool configure(float freq, float bw, uint8_t sf, uint8_t cr) {
    if (_freq == freq && _bw == bw && _sf == sf && _cr == cr) return false;
    // A configuration change clears evidence but cannot evade the probe budget.
    bool scanned = _have_scan; uint32_t last = _last_scan;
    reset(); _have_scan = scanned; _last_scan = last;
    _freq = freq; _bw = bw; _sf = sf; _cr = cr;
    return true;
  }
  static bool qualifiesRoute(bool flood, bool direct, uint8_t paths) {
    return (flood && paths > 0) || (direct && paths > 1);
  }
  Snapshot snapshot(uint32_t now, bool available = true) const {
    Snapshot result;
    if (!available) { result.status = UNAVAILABLE; return result; }
    if (_sf < 5 || _sf > 12) return result;
    int best = -1; Average chosen;
    for (unsigned i = 1; i <= LINK_COUNT; ++i) {
      Average a = average(_links[i], now);
      if (a.count && (best < 0 || a.sum * chosen.count > chosen.sum * a.count ||
          (a.sum * chosen.count == chosen.sum * a.count && a.age < chosen.age))) {
        best = i; chosen = a;
      }
    }
    if (best < 0) { chosen = average(_links[0], now); if (chosen.count) best = 0; }
    if (best < 0) {
      result.status = _ever ? STALE : NONE;
      if (_ever) {
        result.age_ms = UINT32_MAX;
        for (const auto& link : _links)
          for (unsigned i=0;i<link.count;++i)
            if (now-link.samples[i].at < result.age_ms) result.age_ms=now-link.samples[i].at;
      }
      return result;
    }
    const auto& link = _links[best];
    result.status = FRESH; result.identified = best != 0;
    memcpy(result.key, link.key, 32);
    result.count = chosen.count; result.age_ms = chosen.age;
    result.snr_x4 = chosen.sum / chosen.count; result.rssi = chosen.rssi / chosen.count;
    result.bars = link.mask == chosen.mask ? link.shown
        : classify(chosen.sum - floorX4(_sf)*chosen.count, chosen.count);
    return result;
  }
  bool noteSample(const uint8_t* key, Source source, int snr_x4, int rssi,
                  uint32_t now, const uint8_t* token = nullptr, const uint32_t* received_at = nullptr) {
    if (_sf < 5 || _sf > 12) return false;
    if (source == DISCOVERY && !_discovering) return false;
    if ((source == ROUTED) != (key == nullptr)) return false;
    unsigned slot = 0;
    if (key) {
      slot = 0;
      for (unsigned i = 1; i <= LINK_COUNT; ++i)
        if (_links[i].used && !memcmp(_links[i].key, key, 32)) { slot = i; break; }
      if (!slot) {
        for (unsigned i = 1; i <= LINK_COUNT; ++i)
          if (!_links[i].used) { slot = i; break; }
      }
      if (!slot) {
        slot = 1;
        for (unsigned i = 2; i <= LINK_COUNT; ++i)
          if (now-_links[i].last > now-_links[slot].last) slot = i;
      }
      if (!_links[slot].used || memcmp(_links[slot].key, key, 32)) {
        _links[slot] = Link(); memcpy(_links[slot].key, key, 32);
      }
    }
    auto& link = _links[slot];
    for (unsigned i = 0; token && i < link.count; ++i) {
      const auto& s = link.samples[i];
      if (s.token_valid && now - s.at < DUPLICATE_MS && !memcmp(token, s.token, 8)) return false;
    }
    const Average before = average(link, now);
    // Expiry resets confirmations; adding another sample must not bypass hysteresis.
    if (!before.count || before.mask != link.mask) {
      link.shown = before.count ? classify(before.sum-floorX4(_sf)*before.count,before.count) : 0;
      link.pending = link.confirmations = 0;
    }
    auto& s = link.samples[link.head];
    s = Sample(); s.at = received_at ? *received_at : now;
    s.snr = snr_x4 < INT8_MIN ? INT8_MIN : snr_x4 > INT8_MAX ? INT8_MAX : snr_x4;
    s.rssi = rssi < -255 ? -255 : rssi > 0 ? 0 : rssi;
    if (token) { memcpy(s.token, token, 8); s.token_valid = true; }
    link.head = (link.head+1) % SAMPLE_COUNT;
    if (link.count < SAMPLE_COUNT) ++link.count;
    link.used = true; link.last = now;
    auto a = average(link, now);
    link.mask = a.mask; // mask growth is new evidence, not expiry
    smooth(link, a);
    _ever = true;
    if (source == DISCOVERY) _discover_seen = true;
    Event& e = _history[_history_head]; e = Event();
    e.at = s.at; e.source = source; e.snr_x4 = s.snr; e.rssi = s.rssi;
    if (key) memcpy(e.key, key, 32);
    e.selected = snapshot(now);
    _history_head = (_history_head+1) % HISTORY_COUNT;
    if (_history_count < HISTORY_COUNT) ++_history_count;
    return true;
  }
  bool shouldScanOnUserWake(uint32_t now, bool available) const {
    return available && !fresh(now) && (!_have_scan || now-_last_scan >= SCAN_INTERVAL_MS);
  }
  bool fresh(uint32_t now) const { return snapshot(now).status == FRESH; }
  void noteScanAttempt(uint32_t now) { _have_scan = true; _last_scan = now; }
  void beginDiscovery() { _discovering = true; _discover_seen = false; }
  void cancelDiscovery() { _discovering = false; }
  void finishDiscovery(uint32_t) {
    if (_discovering && !_discover_seen) ++_failed_scans;
    _discovering = false;
  }
  uint32_t failedScans() const { return _failed_scans; }
  uint8_t historyCount() const { return _history_count; }
  bool history(unsigned newest_index, Event& out) const {
    if (newest_index >= _history_count) return false;
    out = _history[(_history_head + HISTORY_COUNT - 1 - newest_index) % HISTORY_COUNT];
    return true;
  }
  static const char* statusName(Status status) {
    return status == FRESH ? "fresh" : status == STALE ? "stale" : status == UNAVAILABLE ? "radio-off" : "none";
  }
  void formatSummary(char* out, size_t size, uint32_t now, bool available) const {
    auto s = snapshot(now, available);
    snprintf(out,size,"%s bars=%u %s=%02x%02x%02x%02x snr4=%d rssi=%d age=%lus n=%u fail=%lu hist=%u",
        statusName(s.status),s.bars,s.identified?"key":"fallback",s.key[0],s.key[1],s.key[2],s.key[3],
        s.snr_x4,s.rssi,(unsigned long)(s.age_ms/1000),s.count,(unsigned long)_failed_scans,_history_count);
  }
  void formatHistory(char* out, size_t size, unsigned index, uint32_t now) const {
    Event e;
    if (!history(index,e)) { snprintf(out,size,"No signal history at %u",index); return; }
    formatEvent(out,size,index,now,e);
  }
  static void formatEvent(char* out,size_t size,unsigned index,uint32_t now,const Event& e) {
    snprintf(out,size,"%u %s=%02x%02x%02x%02x rx4=%d rssi=%d age=%lus selected=%02x%02x%02x%02x/%s bars=%u avg4=%d n=%u sample-age=%lus",
        index,e.source==ROUTED?"routed":e.source==ADVERT?"advert":"discover",e.key[0],e.key[1],e.key[2],e.key[3],
        e.snr_x4,e.rssi,(unsigned long)((now-e.at)/1000),e.selected.key[0],e.selected.key[1],e.selected.key[2],e.selected.key[3],
        e.selected.identified?"id":"fallback",e.selected.bars,e.selected.snr_x4,e.selected.count,(unsigned long)(e.selected.age_ms/1000));
  }
};
static_assert(sizeof(RepeaterSignalMonitor) <= 3072, "Signal history must remain bounded");
} // namespace zen
