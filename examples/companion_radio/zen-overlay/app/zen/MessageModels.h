#pragma once

#include <stdint.h>
#include "MessageAckTracker.h"
#include "NodeRouteRetry.h"

namespace zen {

enum MessageDeliveryStatus : uint8_t { DELIVERY_NONE = 0, DELIVERY_PENDING, DELIVERY_OK, DELIVERY_FAIL };
enum MessageDeliveryRoute : uint8_t {
  ROUTE_NONE = 0, ROUTE_DIRECT, ROUTE_PATH, ROUTE_FLOOD, ROUTE_RELAY
};

static const int MESSAGE_TEXT_BUFFER = MAX_TEXT_LEN + 1;

struct ChannelMessageRecord {
  uint8_t ch_idx;
  char text[MESSAGE_TEXT_BUFFER];
  uint32_t timestamp;
  uint32_t activity_seq;
  uint8_t relay_status;
  uint8_t relay_count;
  uint32_t relay_seq;
};

struct DirectMessageRecord {
  uint8_t prefix[4];
  uint8_t outgoing;
  char text[MESSAGE_TEXT_BUFFER];
  uint32_t timestamp;
  uint32_t activity_seq;
  uint8_t ack_status;
  uint8_t delivery_route;
  uint32_t ack_tag;
  MessageAckTracker acknowledgements;
  uint32_t ack_deadline_ms;
  uint32_t msg_ts;
  uint8_t attempt;
  uint8_t path_hops;
  uint8_t path_hash_bytes;
  uint8_t fallback_from_path;
  uint8_t fallback_hops;
  NodeRouteRetry route_retry;
};

} // namespace zen

using AckState = zen::MessageDeliveryStatus;
static const AckState ACK_NONE = zen::DELIVERY_NONE;
static const AckState ACK_PENDING = zen::DELIVERY_PENDING;
static const AckState ACK_OK = zen::DELIVERY_OK;
static const AckState ACK_FAIL = zen::DELIVERY_FAIL;
using DeliveryRoute = zen::MessageDeliveryRoute;
static const uint8_t DELIVERY_ROUTE_NONE = zen::ROUTE_NONE;
static const uint8_t DELIVERY_ROUTE_DIRECT = zen::ROUTE_DIRECT;
static const uint8_t DELIVERY_ROUTE_PATH = zen::ROUTE_PATH;
static const uint8_t DELIVERY_ROUTE_FLOOD = zen::ROUTE_FLOOD;
static const uint8_t DELIVERY_ROUTE_RELAY = zen::ROUTE_RELAY;
static const int MSG_TEXT_BUF = zen::MESSAGE_TEXT_BUFFER;
using ChHistEntry = zen::ChannelMessageRecord;
using DmHistEntry = zen::DirectMessageRecord;
