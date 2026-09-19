#pragma once

#include <Packet.h>

// One outstanding TRACE loop over the repeaters that carried a Public request.
// It starts and ends at the sensor, without addressing the companion.
class RepeaterTraceProbe {
  static const uint8_t MAX_REPEATERS = 4;
  static const uint8_t MAX_TRACE_PATH_BYTES = (2 * MAX_REPEATERS - 1) * 3;
  static const uint32_t TIMEOUT_MILLIS = 30000;

  uint8_t _path[MAX_TRACE_PATH_BYTES];
  uint8_t _path_bytes;
  uint8_t _repeater_count;
  uint8_t _flags;
  uint32_t _tag;
  uint32_t _auth_code;
  uint32_t _started_at;
  bool _active;

public:
  enum StartResult { STARTED, BUSY, NO_REPEATERS, TOO_LONG, INVALID_PATH };

  RepeaterTraceProbe();

  StartResult start(const mesh::Packet* request, uint32_t tag, uint32_t auth_code,
                    uint32_t now_millis);
  void cancel() { _active = false; }
  bool isTimedOut(uint32_t now_millis);
  bool complete(const mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                const uint8_t* path_hashes, uint8_t path_len, uint32_t now_millis,
                uint32_t& elapsed_millis, uint8_t& repeater_count);

  const uint8_t* path() const { return _path; }
  uint8_t pathBytes() const { return _path_bytes; }
  uint8_t flags() const { return _flags; }
};
