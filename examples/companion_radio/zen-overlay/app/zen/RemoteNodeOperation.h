#pragma once

#include <stdint.h>
#include <string.h>
#include "NodeRouteRetry.h"

namespace zen {

// Transport-independent lifecycle for an on-device request to a remote node.
// Packet encoding stays in MyMesh; screens only react to the actions and final
// results exposed here. The fixed-size state is suitable for the nRF52 and
// deliberately allocates no heap memory.
class RemoteNodeOperation {
public:
  enum Kind : uint8_t { NONE, LOGIN, TELEMETRY, ADMIN_READ, ADMIN_WRITE, ADMIN_ACTION };
  enum Route : uint8_t { PATH, FLOOD };
  enum RetryMode : uint8_t { RETRY_SAFE, DO_NOT_RETRY };
  enum Action : uint8_t { WAIT, RETRY_PATH, RETRY_FLOOD, FINISH_TIMEOUT, FINISH_UNKNOWN };
  enum Result : uint8_t {
    PENDING, SUCCESS, REJECTED, PERMISSION_DENIED, MALFORMED_REPLY,
    NODE_MISSING, BUSY, SEND_FAILED, TIMEOUT, RETRIES_EXHAUSTED,
    CANCELLED, RESULT_UNKNOWN
  };

private:
  Kind _kind = NONE;
  Route _route = FLOOD;
  RetryMode _retry_mode = RETRY_SAFE;
  Result _result = CANCELLED;
  uint8_t _key[32]{};
  uint32_t _deadline = 0;
  uint8_t _attempt = 0;
  bool _active = false;
  NodeRouteRetry _retries;

public:
  static const uint32_t RESPONSE_GRACE_MS = 4000;
  static uint32_t responseDeadline(uint32_t now, uint32_t wire_timeout) {
    return now + wire_timeout + RESPONSE_GRACE_MS;
  }
  static const char* resultName(Result result) {
    switch (result) {
      case SUCCESS: return "Success";
      case REJECTED: return "Rejected";
      case PERMISSION_DENIED: return "Permission denied";
      case MALFORMED_REPLY: return "Malformed reply";
      case NODE_MISSING: return "Node missing";
      case BUSY: return "Busy";
      case SEND_FAILED: return "Send failed";
      case TIMEOUT: return "Timeout";
      case RETRIES_EXHAUSTED: return "Retries exhausted";
      case CANCELLED: return "Cancelled";
      case RESULT_UNKNOWN: return "Result unknown";
      default: return "Pending";
    }
  }
  static const char* compactResultName(Result result) {
    switch (result) {
      case SUCCESS: return "OK";
      case REJECTED: return "Rejected";
      case PERMISSION_DENIED: return "Denied";
      case MALFORMED_REPLY: return "Malformed";
      case NODE_MISSING: return "Missing";
      case BUSY: return "Busy";
      case SEND_FAILED: return "Send fail";
      case TIMEOUT: return "Timeout";
      case RETRIES_EXHAUSTED: return "Exhausted";
      case CANCELLED: return "Cancelled";
      case RESULT_UNKNOWN: return "Unknown";
      default: return "Pending";
    }
  }

  void begin(Kind kind, const uint8_t* key, bool known_path,
             RetryMode retry_mode, uint32_t deadline, uint8_t key_len = 32) {
    _kind = kind;
    memset(_key, 0, sizeof(_key));
    if (key) memcpy(_key, key, key_len < sizeof(_key) ? key_len : sizeof(_key));
    _route = known_path ? PATH : FLOOD;
    _retry_mode = retry_mode;
    _result = PENDING;
    _deadline = deadline;
    _attempt = 1;
    _active = true;
    _retries.begin(known_path);
  }

  bool active() const { return _active; }
  Kind kind() const { return _kind; }
  Route route() const { return _route; }
  Result result() const { return _result; }
  uint8_t attempt() const { return _attempt; }
  const uint8_t* key() const { return _key; }
  bool matches(const uint8_t* key, uint8_t key_len = 32) const {
    return _active && key && memcmp(_key, key,
        key_len < sizeof(_key) ? key_len : sizeof(_key)) == 0;
  }
  bool expired(uint32_t now) const {
    return _active && (int32_t)(now - _deadline) >= 0;
  }

  Action onTimeout(uint32_t now, bool path_available = true) {
    if (!expired(now)) return WAIT;
    _active = false;
    if (_retry_mode == DO_NOT_RETRY) {
      _result = RESULT_UNKNOWN;
      return FINISH_UNKNOWN;
    }
    NodeRouteRetry::Action next = _retries.next(path_available);
    if (next == NodeRouteRetry::RETRY_PATH) {
      _route = PATH;
      return RETRY_PATH;
    }
    if (next == NodeRouteRetry::RETRY_FLOOD) {
      _route = FLOOD;
      return RETRY_FLOOD;
    }
    _result = RETRIES_EXHAUSTED;
    return FINISH_TIMEOUT;
  }

  bool restart(Route route, uint32_t deadline) {
    if (_active || _result != PENDING) return false;
    _route = route;
    _deadline = deadline;
    _attempt++;
    _active = true;
    return true;
  }

  bool finish(Result result) {
    if (!_active) return false;
    _active = false;
    _result = result;
    return true;
  }

  void settle(Result result) {
    _active = false;
    _result = result;
  }

  void cancel() {
    _active = false;
    _result = CANCELLED;
  }
};

} // namespace zen
