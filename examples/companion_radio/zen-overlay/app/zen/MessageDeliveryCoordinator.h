#pragma once

#include "MessageModels.h"

namespace zen {

// Delivery policy only. ConversationStore owns records; the transport adapter
// performs radio calls; this coordinator owns ACK/relay transitions and retry
// escalation shared by UI- and app-originated messages.
struct MessageDeliveryCoordinator {
  static uint32_t deadline(uint32_t now, uint32_t estimate) {
    return now + estimate + 4000;
  }
  static uint8_t routeForPath(uint8_t path_len) {
    return path_len == OUT_PATH_UNKNOWN ? ROUTE_FLOOD
         : (path_len == 0 ? ROUTE_DIRECT : ROUTE_PATH);
  }
  static MessageDeliveryStatus effective(const DirectMessageRecord& entry,
                                         uint32_t now) {
    if (entry.ack_status == DELIVERY_PENDING && entry.route_retry.exhausted() &&
        (int32_t)(now - entry.ack_deadline_ms) >= 0) return DELIVERY_FAIL;
    return (MessageDeliveryStatus)entry.ack_status;
  }
  static NodeRouteRetry::Action nextRetry(DirectMessageRecord& entry,
                                          bool path_available) {
    return entry.route_retry.next(path_available);
  }
  static void recordAttempt(DirectMessageRecord& entry, uint8_t attempt,
                            uint32_t ack, uint32_t deadline_ms, uint8_t route) {
    entry.attempt = attempt;
    entry.ack_tag = ack;
    entry.ack_deadline_ms = deadline_ms;
    entry.delivery_route = route;
    entry.ack_status = ack ? DELIVERY_PENDING : DELIVERY_NONE;
    entry.acknowledgements.record(attempt, ack, route);
  }
  static void begin(DirectMessageRecord& entry, bool outgoing, uint32_t ack,
                    uint32_t deadline_ms, uint8_t route) {
    entry.acknowledgements = MessageAckTracker();
    recordAttempt(entry, 0, outgoing ? ack : 0, deadline_ms,
                  outgoing ? route : ROUTE_NONE);
    if (outgoing && ack)
      entry.route_retry.begin(route == ROUTE_DIRECT || route == ROUTE_PATH);
    else
      entry.route_retry.reset();
  }
  static bool acknowledge(DirectMessageRecord& entry, uint32_t ack_crc,
                          uint8_t& route) {
    if (!entry.outgoing || !entry.acknowledgements.match(ack_crc, route)) return false;
    entry.ack_status = DELIVERY_OK;
    entry.delivery_route = route;
    if (route == ROUTE_PATH || route == ROUTE_DIRECT) entry.fallback_from_path = 0;
    entry.route_retry.reset();
    return true;
  }
  static void fail(DirectMessageRecord& entry) {
    entry.ack_status = DELIVERY_FAIL;
    entry.route_retry.reset();
  }
  static void armRelay(ChannelMessageRecord& entry, uint32_t seq) {
    entry.relay_status = DELIVERY_PENDING;
    entry.relay_count = 0;
    entry.relay_seq = seq;
  }
  static bool hearRelay(ChannelMessageRecord& entry, uint32_t seq) {
    if (!seq || entry.relay_seq != seq ||
        (entry.relay_status != DELIVERY_PENDING && entry.relay_status != DELIVERY_OK)) return false;
    if (entry.relay_count < 255) entry.relay_count++;
    entry.relay_status = DELIVERY_OK;
    return true;
  }
  static bool expireRelay(ChannelMessageRecord& entry, uint32_t seq) {
    if (!seq || entry.relay_seq != seq) return false;
    if (!entry.relay_count) entry.relay_status = DELIVERY_FAIL;
    return true;
  }
};

} // namespace zen
