#pragma once
#include "TraceTimingPolicy.h"
#include "TraceRoute.h"
#include "TraceScope.h"
#include <string.h>

namespace zen {

// One current transaction plus two retired peers. Login readiness and its authenticated return path are owned by one operation.
class TraceDiscovery {
public:
  enum Phase { IDLE, QUEUE, TX, WAIT, COLLECT, FAILED, LOGIN_QUEUE, LOGIN_TX, LOGIN_WAIT, ADVERT_WAIT };
  enum Error { TIMEOUT, BUSY, TX_FAILED, CLOCK_FAILED, LOGIN_SILENT, LOGIN_ROLE, BUDGET };
private:
  struct Peer { uint8_t key[32]{}; uint32_t tags[2]{}; bool sent[2]{}; bool valid=false; } _peers[3];
  Phase _phase=IDLE;
  Error _error=TIMEOUT;
  mesh::Packet* _packet=nullptr;
  uint32_t _stage_end=0,_control_end=0,_next_at=0,_started=0,_timeout=0,_tx_timeout=0;
  uint8_t _attempt=0,_failures=0,_login_attempt=0;
  bool _public=true,_login_sent=false,_login_completed=false;
  uint32_t _discovery_reply=6000,_login_reply=8000,_queue_lease=6000;
  uint32_t _last_server_login=0;
  bool _server_clock_regressed=false;
  void stage(Phase phase,uint32_t now,uint32_t budget) {
    _phase=phase; _packet=nullptr; _failures=0; _next_at=now; _stage_end=now+budget;
    if((int32_t)(_stage_end-_control_end)>0) _stage_end=_control_end;
  }
  void miss(uint32_t now,Error error) {
    _error=error; _packet=nullptr;
    if(login()) {
      if((error==LOGIN_SILENT || error==TX_FAILED) &&
         _login_attempt+1<TraceTimingPolicy::LOGIN_MAX_ATTEMPTS && controlRemaining(now)) {
        ++_login_attempt;
        stage(LOGIN_QUEUE,now,TraceTimingPolicy::LOGIN_QUEUE_TIMEOUT_MILLIS);
        _next_at=now+TraceTimingPolicy::localBackoff(_login_attempt-1);
      } else _phase=ADVERT_WAIT;
      return;
    }
    if(_attempt==0 && (int32_t)(_control_end-now)>0) {
      if(_public) stage(LOGIN_QUEUE,now,TraceTimingPolicy::LOGIN_QUEUE_TIMEOUT_MILLIS);
      else { _attempt=1; stage(QUEUE,now,_queue_lease); }
    } else _phase=ADVERT_WAIT;
  }
public:
  void begin(const uint8_t* key,uint32_t now,bool public_guest=true,uint32_t control_end=0,
             uint32_t discovery_reply=6000,uint32_t login_reply=8000,uint32_t queue_lease=6000) {
    if(memcmp(_peers[0].key,key,32)) _last_server_login=0;
    _peers[2]=_peers[1]; _peers[1]=_peers[0]; _peers[0]=Peer{};
    memcpy(_peers[0].key,key,32); _peers[0].valid=true;
    _public=public_guest; _attempt=0; _login_attempt=0; _login_sent=_login_completed=false;
    _discovery_reply=discovery_reply; _login_reply=login_reply; _queue_lease=queue_lease;
    _control_end=control_end?control_end:now+TraceTimingPolicy::CONTROL_TIMEOUT_MILLIS;
    _server_clock_regressed=false; _error=TIMEOUT; stage(QUEUE,now,_queue_lease);
  }
  void resetQueueClock(uint32_t now) { if(_phase==QUEUE) stage(QUEUE,now,_queue_lease); }
  bool serverClockRegressed() const { return _server_clock_regressed; }
  Error error() const { return _error; }
  Phase phase() const { return _phase; }
  bool active() const { return _phase==QUEUE || _phase==TX || _phase==WAIT; }
  bool login() const { return _phase==LOGIN_QUEUE || _phase==LOGIN_TX || _phase==LOGIN_WAIT; }
  bool searching() const { return active() || login() || _phase==ADVERT_WAIT; }
  bool pending() const { return searching() || _phase==FAILED; }
  uint8_t attempt() const { return _attempt+1; }
  uint8_t loginAttempt() const { return _login_attempt+1; }
  uint32_t controlRemaining(uint32_t now) const {
    return (int32_t)(_control_end-now)>0?_control_end-now:0;
  }
  uint32_t remaining(uint32_t now) const {
    uint32_t left=(int32_t)(_stage_end-now)>0?_stage_end-now:0;
    if(_phase==WAIT || _phase==LOGIN_WAIT) {
      uint32_t age=now-_started, receive=age>=_timeout?0:_timeout-age;
      if(receive<left) left=receive;
    }
    return left;
  }
  const uint8_t* key() const { return _peers[0].key; }
  const uint8_t* peer(uint8_t i) const { return i<3 && _peers[i].valid?_peers[i].key:nullptr; }
  bool owns(const uint8_t* key,uint32_t tag) const {
    for(const auto& p:_peers) if(p.valid && !memcmp(key,p.key,32))
      for(uint8_t i=0;i<2;i++) if(p.sent[i] && p.tags[i]==tag) return true;
    return false;
  }
  bool current(const uint8_t* key,uint32_t tag) const {
    if((!searching() && _phase!=COLLECT) || memcmp(key,_peers[0].key,32)) return false;
    for(uint8_t i=0;i<2;i++) if(_peers[0].sent[i] && _peers[0].tags[i]==tag) return true;
    return false;
  }
  mesh::Packet* queuedPacket() const { return _phase==QUEUE || _phase==LOGIN_QUEUE?_packet:nullptr; }
  bool queueExpired(uint32_t now) const { return queuedPacket() && !remaining(now); }
  uint32_t queueDue() const { return _next_at; }
  bool canStart(uint32_t now) const {
    return controlRemaining(now)>=uint64_t(_tx_timeout)+ (login()?_login_reply:_discovery_reply);
  }
  bool queueWaiting(uint32_t now) const { return (_phase==QUEUE || _phase==LOGIN_QUEUE) && (int32_t)(now-_next_at)>=0; }
  bool ready(uint32_t now) const { return (_phase==QUEUE || _phase==LOGIN_QUEUE) && !_packet && remaining(now) && (int32_t)(now-_next_at)>=0; }
  void queued(mesh::Packet* packet,uint32_t tag,uint32_t timeout,uint32_t transmit_timeout=0) {
    _packet=packet; _timeout=timeout; _tx_timeout=transmit_timeout?transmit_timeout:timeout;
    if(_phase==QUEUE) _peers[0].tags[_attempt]=tag;
  }
  void limitTimeout(uint32_t limit) { if(_timeout>limit) _timeout=limit; }
  void rejected(uint32_t now,uint32_t jitter) {
    _next_at=now+TraceTimingPolicy::localBackoff(_failures)+jitter%251;
    if(_failures<255) ++_failures;
  }
  void tick(uint32_t now) {
    if(!active() && !login()) return;
    if(!controlRemaining(now) || !remaining(now))
      miss(now,(_phase==LOGIN_TX || _phase==TX)?TX_FAILED:_phase==LOGIN_WAIT?LOGIN_SILENT:
          (_phase==QUEUE || _phase==LOGIN_QUEUE)?BUSY:TIMEOUT);
  }
  void started(mesh::Packet* packet,uint32_t now) {
    if(!packet || packet!=_packet) return;
    if(_phase==QUEUE) {
      if(!remaining(now)) { miss(now,BUSY); return; }
      _peers[0].sent[_attempt]=true; _phase=TX;
      _stage_end=now+_tx_timeout;
      if((int32_t)(_stage_end-_control_end)>0) _stage_end=_control_end;
    }
    else if(_phase==LOGIN_QUEUE) {
      if(!remaining(now)) { miss(now,BUSY); return; }
      _login_sent=true; _phase=LOGIN_TX; _stage_end=now+_tx_timeout;
      if((int32_t)(_stage_end-_control_end)>0) _stage_end=_control_end;
    }
    else return;
    _started=now;
  }
  void completed(mesh::Packet* p,uint32_t now) {
    if(!p || p!=_packet) return;
    if(_phase==LOGIN_TX || _phase==TX) {
      bool was_login=_phase==LOGIN_TX;
      // Queueing and actual airtime do not consume the reply window. A late
      // completion cannot resurrect an expired transmission or control lease.
      tick(now);
      if(_phase!=(was_login?LOGIN_TX:TX)) return;
      _phase=was_login?LOGIN_WAIT:WAIT; _started=now;
      if(was_login) _login_completed=true;
      _timeout=was_login?_login_reply:_discovery_reply;
      _stage_end=now+_timeout;
      if((int32_t)(_stage_end-_control_end)>0) _stage_end=_control_end;
    }
    _packet=nullptr;
  }
  void failed(mesh::Packet* p,uint32_t now) { if(p && p==_packet && (active() || login())) miss(now,TX_FAILED); }
  bool loginResponse(const uint8_t* key,const uint8_t* data,size_t len,uint32_t now) {
    if(!login() || !_login_sent || !_login_completed || !controlRemaining(now) ||
       (_phase==LOGIN_WAIT && !remaining(now)) || memcmp(key,this->key(),32) ||
       !data || len<13 || data[4]!=0 || data[5]!=0 || data[6]>1) return false;
    uint32_t server; memcpy(&server,data,4);
    if(!server) return false;
    _server_clock_regressed=server<=_last_server_login;
    if(server>_last_server_login) _last_server_login=server;
    if(data[6] || (data[7]&3)==3) { _error=LOGIN_ROLE; _phase=ADVERT_WAIT; _packet=nullptr; return true; }
    _error=TIMEOUT; _attempt=1; stage(QUEUE,now,_queue_lease); return true;
  }
  void budgetExhausted() { _error=BUDGET; _phase=ADVERT_WAIT; _packet=nullptr; }
  void abandon() { _phase=ADVERT_WAIT; _packet=nullptr; }
  void clockFailed() { _error=CLOCK_FAILED; _phase=ADVERT_WAIT; _packet=nullptr; }
  void collect() { _phase=COLLECT; _packet=nullptr; }
  void cancel() { _phase=IDLE; _packet=nullptr; }
  const char* failureText() const {
    return _error==BUDGET?"Trace: insufficient time for discovery":_error==BUSY?"Trace: discovery radio busy":_error==TX_FAILED?"Trace: discovery transmit failed":
        _error==CLOCK_FAILED?"Trace: request clock unavailable":_error==LOGIN_SILENT?"Trace: guest login not acknowledged":
        _error==LOGIN_ROLE?"Trace: unexpected login role":"Trace: discovery timed out";
  }
};
} // namespace zen
