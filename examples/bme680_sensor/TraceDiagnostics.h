#pragma once

#include "RepeaterTraceProbe.h"
#include "TraceReportDelivery.h"
#include <string.h>

// Local evidence only. No logging, flash writes or unsolicited radio messages.
struct TraceDiagnostics {
  struct Progress {
    uint32_t elapsed = 0;
    uint8_t snrs[2 * TraceRoute::MAX_REPEATERS - 1]{};
    uint8_t furthest = 0;
    bool complete = false;
  } progress[2];
  RepeaterTraceProbe::Result selected{};
  bool has_result = false;
  uint32_t allocations_failed = 0, starts = 0, local_failures = 0, timeouts = 0;
  uint32_t queue_failures = 0, report_failures = 0, probe_submissions = 0;
  TraceReportDelivery::Outcome delivery = TraceReportDelivery::NONE;
  void begin() { *this = TraceDiagnostics{}; }
  void observe(const mesh::Packet* packet, const RepeaterTraceProbe& probe, uint32_t now) {
    uint8_t index;
    uint32_t elapsed;
    if (!probe.matchProgress(packet, index, elapsed, now)) return;
    auto& value = progress[index];
    if (packet->path_len <= value.furthest) return;
    value.furthest = packet->path_len;
    value.elapsed = elapsed;
    memcpy(value.snrs, packet->path, packet->path_len);
  }
};
