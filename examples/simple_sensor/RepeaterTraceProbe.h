#pragma once

#include <Packet.h>

// One outstanding TRACE loop over the repeaters that carried a Public request.
// It starts and ends at the sensor, without addressing the companion.
class RepeaterTraceProbe {
  static const uint8_t MAX_REPEATERS = 10;
  static const uint8_t MAX_TRACE_PATH_BYTES = (2 * MAX_REPEATERS - 1) * 2;
  static const uint32_t BASE_TIMEOUT_MILLIS = 5000;
  static const uint32_t PER_REPEATER_TIMEOUT_MILLIS = 2500;
  static const uint32_t QUEUE_TIMEOUT_MILLIS = 10000;

  enum State { IDLE, QUEUED, IN_FLIGHT, RETRY_PENDING };

  uint8_t _path[MAX_TRACE_PATH_BYTES];
  uint8_t _path_bytes;
  uint8_t _repeater_count;
  uint8_t _flags;
  uint32_t _tag;
  uint32_t _auth_code;
  uint32_t _first_tag;
  uint32_t _first_auth_code;
  uint32_t _first_started_at;
  uint32_t _started_at;
  uint32_t _queued_at;
  uint32_t _timeout_millis;
  mesh::Packet* _packet;
  State _state;
  bool _retried;
  bool _fallback_valid;

public:
  struct Result {
    uint32_t elapsed_millis;
    uint8_t repeater_count;
    uint8_t snrs[MAX_REPEATERS];
  };

private:
  Result _fallback;

public:
  enum StartResult { STARTED, BUSY, NO_REPEATERS, TOO_LONG, INVALID_PATH };
  enum LateResult { NOT_FIRST, RETRY_QUEUED, FALLBACK_SAVED };

  RepeaterTraceProbe();

  StartResult start(const mesh::Packet* request, uint32_t tag, uint32_t auth_code,
                    uint32_t now_millis);
  bool beginRetry(uint32_t tag, uint32_t auth_code, uint32_t now_millis);
  void cancel() { _packet = NULL; _state = IDLE; }
  bool isActive() const { return _state != IDLE; }
  bool retryPending() const { return _state == RETRY_PENDING; }
  void setQueuedPacket(mesh::Packet* packet) { _packet = packet; }
  mesh::Packet* queuedPacket() const { return _state == QUEUED ? _packet : NULL; }
  bool isQueueTimedOut(uint32_t now_millis) const;
  bool onTxStarted(const mesh::Packet* packet, uint32_t now_millis);
  void onTxComplete(const mesh::Packet* packet);
  bool onTxFailed(const mesh::Packet* packet);
  bool isTimedOut(uint32_t now_millis);
  bool complete(const mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                const uint8_t* path_hashes, uint8_t path_len, uint32_t now_millis,
                uint32_t& elapsed_millis, uint8_t& repeater_count);
  LateResult captureLateFirst(const mesh::Packet* packet, uint32_t tag, uint32_t auth_code,
                              uint8_t flags, const uint8_t* path_snrs,
                              const uint8_t* path_hashes, uint8_t path_len,
                              uint32_t now_millis);
  bool takeFallback(Result& result);

  const uint8_t* path() const { return _path; }
  uint8_t pathBytes() const { return _path_bytes; }
  uint8_t flags() const { return _flags; }
  uint32_t tag() const { return _tag; }
  uint32_t authCode() const { return _auth_code; }
};
