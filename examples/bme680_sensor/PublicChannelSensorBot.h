#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <Utils.h>

#ifndef SENSOR_BOT_CHANNEL_NAME
  #define SENSOR_BOT_CHANNEL_NAME ""
#endif

// Passive channel command decoder with duplicate and rate limiting.
// Sensor acquisition and packet transmission remain application concerns.
class PublicChannelSensorBot {
  static const uint32_t REPLY_WINDOW_MILLIS = 60000;
  static const int MAX_REPLIES_PER_WINDOW = 4;
  static const int RECENT_COMMANDS = 8;

  mesh::GroupChannel _channel;
  mesh::GroupChannel _public_channel;
  uint32_t _recent_commands[RECENT_COMMANDS];
  uint8_t _next_recent;
  uint32_t _reply_times[MAX_REPLIES_PER_WINDOW];
  uint8_t _next_reply;
  uint8_t _reply_count;

  static bool equalsIgnoreCase(const char* lhs, const char* rhs);

public:
  enum Metric : uint8_t {
    METRIC_TEMPERATURE = 1 << 0,
    METRIC_HUMIDITY = 1 << 1,
    METRIC_PRESSURE = 1 << 2,
    METRIC_AIR_QUALITY = 1 << 3,
    METRIC_VOLTAGE = 1 << 4,
    REQUEST_PING = 1 << 5,
    REQUEST_PATH = 1 << 6,
    REQUEST_TRACE = 1 << 7,
    METRIC_ALL = METRIC_TEMPERATURE | METRIC_HUMIDITY |
                 METRIC_PRESSURE | METRIC_AIR_QUALITY | METRIC_VOLTAGE,
  };

  enum TraceAction { TRACE_START, TRACE_STATUS, TRACE_CANCEL };

  // Public is always watched. A hashtag name adds its standard channel key.
  PublicChannelSensorBot(const char* channel_name = SENSOR_BOT_CHANNEL_NAME);

  int findChannel(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) const;

  // Returns true once for an accepted command and places the selected metrics
  // or a standalone ping/path/trace request in metric_mask. A busy trace can
  // be reported once by the caller without consuming a command-rate slot.
  bool accept(uint8_t type, uint8_t* data, size_t len, uint32_t now_millis,
              bool trace_busy, uint8_t& metric_mask, bool& busy_trace,
              uint8_t channel_index = 0, uint8_t* trace_prefix = NULL, bool* targeted = NULL,
              TraceAction* trace_action = NULL, char* requester_name = NULL,
              size_t requester_name_size = 0);
  int channelIndex(const mesh::GroupChannel& source) const;
  const mesh::GroupChannel& channel(uint8_t index = 0) const {
    return index == 1 ? _public_channel : _channel;
  }
};
