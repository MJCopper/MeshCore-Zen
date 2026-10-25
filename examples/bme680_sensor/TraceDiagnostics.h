#pragma once

#include "RepeaterTraceProbe.h"
#include "TraceReportDelivery.h"
#include <string.h>

// Local evidence only. No logging, flash writes or unsolicited radio messages.
struct TraceDiagnostics {
  struct Control {
    uint32_t queued=0, started=0, completed=0, tag=0;
    bool sent=false, failed=false, uncertain=false, cancelled=false;
  } control[5]; // initial discovery, login, final discovery, login retries 1 and 2
  uint32_t received_paths=0, unmatched_tags=0, inactive_replies=0, profile_rejections=0;
  struct Progress {
    uint32_t elapsed = 0;
    uint8_t snrs[2 * TraceRoute::MAX_REPEATERS - 1]{};
    uint8_t furthest = 0;
    bool complete = false;
  } progress[2];
  RepeaterTraceProbe::Result selected{};
  bool has_result = false;
  bool login_path = false;
  bool advert_route = false, advert_probation = false, clock_healthy = true, wall_clock_trusted = false;
  uint32_t discovery_starts = 0, login_starts = 0, scope_mismatches = 0, advert_observations = 0;
  uint32_t candidate_replies = 0, decrypted_replies = 0, accepted_logins = 0, ignored_logins = 0;
  bool peer_clock_regressed = false;
  uint8_t login_role = 255;
  uint32_t request_timestamp = 0, route_rejections = 0;
  uint32_t budget_omissions = 0;
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
