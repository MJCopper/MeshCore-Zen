#pragma once

#include "TraceRouteCandidates.h"
#include "TraceTimingPolicy.h"

// One bounded transaction, retaining both transmitted attempts until final commit.
class RepeaterTraceProbe {
public:
  enum StartResult { STARTED, BUSY, NO_REPEATERS, TOO_LONG, INVALID_PATH };
  enum ReceiveResult { IGNORED, STORED, COMPLETE };
  enum Failure { NO_RESULT, RADIO_BUSY, TRANSMIT_FAILED, RETRY_NOT_SENT };
  struct Result {
    uint32_t elapsed_millis;
    TraceRoute route;
    uint8_t snrs[2 * TraceRoute::MAX_REPEATERS];
    uint8_t attempt;
  };

  StartResult start(const mesh::Packet* request, uint32_t tag, uint32_t auth, uint32_t now);
  bool beginRetry(uint32_t tag, uint32_t auth, uint32_t now, const TraceRoute* alternate = NULL);
  bool isActive() const { return _state != IDLE; }
  bool retryPending() const { return _state == RETRY_PENDING; }
  bool retryStarted() const { return _current == 1; }
  bool waitingForQueue() const { return _state == QUEUED; }
  bool currentTransmitted() const { return _attempts[_current].transmitted; }
  void resetQueueClock(uint32_t now) { _phase_at = now; }
  void retryAfterLocalFailure() { if (_current == 0) _state = RETRY_PENDING; }
  bool matchProgress(const mesh::Packet* packet, uint8_t& attempt, uint32_t& elapsed, uint32_t now) const;
  bool isFinalizing() const { return _state == FINAL_GRACE || _state == FAILURE_PENDING; }
  bool failurePending() const { return _state == FAILURE_PENDING; }
  Failure failure() const { return _failure; }
  void cancel();
  void setQueuedPacket(mesh::Packet* packet) { _packet = packet; }
  mesh::Packet* queuedPacket() const { return _state == QUEUED ? _packet : NULL; }
  void setAirtimeEstimate(uint32_t airtime);
  bool isQueueTimedOut(uint32_t now) const;
  void queueFailed(uint32_t now);
  bool onTxStarted(const mesh::Packet* packet, uint32_t now);
  void onTxComplete(const mesh::Packet* packet);
  bool onTxFailed(const mesh::Packet* packet, uint32_t now);
  bool isTimedOut(uint32_t now);
  void advanceFinal(uint32_t now);
  bool reportExpired(uint32_t now) const;
  ReceiveResult receive(const mesh::Packet* packet, uint32_t tag, uint32_t auth, uint8_t flags,
                        const uint8_t* snrs, const uint8_t* hashes, uint8_t length,
                        uint32_t now, Result& result);
  bool takeFallback(Result& result);
  const TraceRoute& route() const { return _attempts[_current].route; }
  const uint8_t* path() const { return route().path; }
  uint8_t pathBytes() const { return route().bytes; }
  uint8_t flags() const { return route().flags; }
  uint32_t tag() const { return _attempts[_current].tag; }
  uint32_t authCode() const { return _attempts[_current].auth; }

private:
  enum State { IDLE, QUEUED, IN_FLIGHT, RETRY_PENDING, FINAL_GRACE, FAILURE_PENDING };
  struct Attempt {
    TraceRoute route;
    uint32_t tag, auth, started_at, timeout;
    bool transmitted;
  };
  Attempt _attempts[2]{};
  Result _fallback{};
  mesh::Packet* _packet = NULL;
  uint32_t _phase_at = 0;
  uint8_t _current = 0;
  State _state = IDLE;
  Failure _failure = NO_RESULT;
  bool _fallback_valid = false;

  void fail(Failure reason, uint32_t now);
};
