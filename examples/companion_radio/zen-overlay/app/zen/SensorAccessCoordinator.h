#pragma once

#include <stdint.h>
#include "RemoteNodeOperation.h"

namespace zen {

// UI-only state for obtaining sensor telemetry. It schedules actions but owns
// no radio or display objects, keeping the login/retry policy independently
// testable and out of packet-receive callbacks.
class SensorAccessCoordinator {
public:
  enum Phase : uint8_t {
    TELEMETRY_WAIT, ACL_WAIT, LOGIN_OFFER, PASSWORD, PASSWORD_WAIT,
    RETRY_DELAY, AUTH_TELEMETRY_WAIT, ERROR
  };
  enum Action : uint8_t {
    NONE, START_BLANK_LOGIN, SEND_TELEMETRY, CLEAR_PATH_AND_SEND_TELEMETRY
  };
  static const uint32_t POST_LOGIN_DELAY_MS = 1000;

private:
  Phase _phase = TELEMETRY_WAIT;
  bool _access_confirmed = false;
  uint32_t _retry_at = 0;

public:
  Phase phase() const { return _phase; }
  bool accessConfirmed() const { return _access_confirmed; }
  bool passwordEditing() const { return _phase == PASSWORD; }

  void reset() {
    _phase = TELEMETRY_WAIT;
    _access_confirmed = false;
    _retry_at = 0;
  }
  void beginTelemetry(bool has_known_path = false, const uint8_t* key = nullptr,
                      uint32_t deadline = 0) {
    (void)has_known_path; (void)key; (void)deadline;
    _phase = _access_confirmed ? AUTH_TELEMETRY_WAIT : TELEMETRY_WAIT;
  }
  void telemetryStarted(uint32_t deadline = 0) {
    (void)deadline;
    _phase = _access_confirmed ? AUTH_TELEMETRY_WAIT : TELEMETRY_WAIT;
  }
  void telemetrySendFailed() {
    _phase = ERROR;
  }
  Action telemetryTimedOut(RemoteNodeOperation::Action retry) {
    // The mesh reply state remains FAILED while login and the delayed retry
    // are in progress. Ignore that stale state outside a telemetry wait.
    bool authenticated = _phase == AUTH_TELEMETRY_WAIT;
    if (_phase != TELEMETRY_WAIT && !authenticated) return NONE;
    if (retry == RemoteNodeOperation::RETRY_PATH) return SEND_TELEMETRY;
    if (retry == RemoteNodeOperation::RETRY_FLOOD) return CLEAR_PATH_AND_SEND_TELEMETRY;
    if (authenticated) {
      _phase = ERROR;
      return NONE;
    }
    _phase = ACL_WAIT;
    return START_BLANK_LOGIN;
  }
  Action telemetryTimedOut() {
    return telemetryTimedOut(RemoteNodeOperation::FINISH_TIMEOUT);
  }
  void loginStarted(bool password_entered) {
    _phase = password_entered ? PASSWORD_WAIT : ACL_WAIT;
  }
  void loginStartFailed(bool password_entered) {
    _phase = password_entered ? PASSWORD : LOGIN_OFFER;
  }
  void offerPassword() { _phase = PASSWORD; }
  void cancelPassword() { _phase = LOGIN_OFFER; }
  void loginFailed(bool password_entered) {
    _phase = password_entered ? PASSWORD : LOGIN_OFFER;
  }
  void loginSucceeded(uint32_t now) {
    _access_confirmed = true;
    _retry_at = now + POST_LOGIN_DELAY_MS;
    _phase = RETRY_DELAY;
  }
  Action tick(uint32_t now) {
    if (_phase != RETRY_DELAY || (int32_t)(now - _retry_at) < 0) return NONE;
    _phase = AUTH_TELEMETRY_WAIT;
    return SEND_TELEMETRY;
  }
};

} // namespace zen
