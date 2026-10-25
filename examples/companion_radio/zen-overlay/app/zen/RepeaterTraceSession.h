#pragma once
#include "RepeaterTraceProbe.h"
#include "TraceRouteHistory.h"
#include "TraceDiscovery.h"
#include "TraceTransactionBudget.h"

namespace zen {
class RepeaterTraceTransport {
public:
  virtual uint32_t traceNow() const=0;
  virtual uint32_t traceRandom()=0;
  virtual uint32_t traceDiscoveryTag()=0;
  virtual uint32_t traceAirtime(unsigned bytes) const=0;
  virtual uint32_t traceFloodTimeout(uint32_t airtime) const { return TraceTimingPolicy::floodTimeout(airtime); }
  virtual mesh::Packet* traceQueueDiscovery(const uint8_t* key,uint32_t tag)=0;
  virtual mesh::Packet* traceQueueLogin(const uint8_t*,uint32_t) { return nullptr; }
  virtual bool tracePublicLogin() const { return false; }
  virtual bool traceAdvertRoute(const uint8_t*,TraceRoute&,const TraceRoute* =nullptr) { return false; }
  virtual unsigned traceScopeBytes() const { return 0; }
  virtual mesh::Packet* traceQueueLoop(const TraceRoute& route,uint32_t tag,uint32_t auth)=0;
  virtual bool traceCancelQueued(mesh::Packet* packet)=0;
};

class RepeaterTraceSession {
public:
  enum Phase : uint8_t { IDLE,DISCOVERY_QUEUE,DISCOVERY_TX,DISCOVERY_WAIT,
    LOGIN_QUEUE,LOGIN_TX,LOGIN_WAIT,SETTLING,TRACE_QUEUE,TRACE_WAIT,RETRY_PAUSE,
    FINAL_GRACE,COMPLETE,FAILED,CANCELLED };
  enum Error : uint8_t { OK,DISCOVERY_TIMEOUT,RADIO_BUSY,TRANSMIT_FAILED,
    RETRY_NOT_SENT,TRACE_TIMEOUT,UNSUPPORTED_PATH,PATH_TOO_LONG,RADIO_CHANGED,
    NODE_REMOVED,POWER_RESTRICTED,USER_CANCELLED,BUDGET_EXHAUSTED,CLOCK_FAILED,
    GUEST_UNAVAILABLE };
private:
  RepeaterTraceTransport& _tx;
  Phase _phase=IDLE;Error _error=OK;
  uint8_t _key[32]{};
  TraceDiscovery _control;
  TraceTransactionBudget _budget;
  TraceRoute _routes[3];
  uint32_t _seen[3]{},_sequence[3]{},_observations=0;
  int8_t _snr[3]{};
  uint8_t _route_count=0,_local_failures=0;
  uint32_t _next_at=0,_settle_at=0,_last_candidate=0,_settle_delay=0;
  uint32_t _profile=0,_trace_profile=0,_deadline=0,_queue_used=0,_sample_at=0;
  mesh::Packet* _owned=nullptr;
  RepeaterTraceProbe _probe;RepeaterTraceProbe::Result _result{};TraceRouteHistory _history;
  bool _has_result=false,_advert=false,_login_path=false;
  uint8_t _attempt_source[2]{}; // discovery, login PATH, advert hint
  unsigned _flood_submissions=0,_login_submissions=0,_trace_submissions=0;
  uint8_t _furthest[2]{};
  static bool due(uint32_t now,uint32_t deadline){return int32_t(now-deadline)>=0;}
  uint32_t identifier(uint32_t avoid=0) {
    uint32_t n=_tx.traceRandom();if(!n || n==avoid){n=avoid+1;if(!n)n=1;}return n;
  }
  void sampleQueues(uint32_t now) {
    bool control=_control.queueWaiting(now);
    if(control || (_phase==TRACE_QUEUE && due(now,_next_at))) {
      uint32_t begin=_sample_at,due_at=control?_control.queueDue():_next_at;
      if(int32_t(due_at-begin)>0)begin=due_at;
      if(int32_t(now-begin)>0)_queue_used=TraceTimingPolicy::bounded(uint64_t(_queue_used)+(now-begin));
    }
    _sample_at=now;
  }
  void discardQueued(){if(_owned)_tx.traceCancelQueued(_owned);_owned=nullptr;}
  void finish(Error error) {
    discardQueued();_probe.cancel();_control.cancel();_error=error;
    _phase=error==OK?COMPLETE:FAILED;
  }
  void syncControl() {
    switch(_control.phase()) {
      case TraceDiscovery::QUEUE:_phase=DISCOVERY_QUEUE;break;
      case TraceDiscovery::TX:_phase=DISCOVERY_TX;break;
      case TraceDiscovery::WAIT:_phase=DISCOVERY_WAIT;break;
      case TraceDiscovery::LOGIN_QUEUE:_phase=LOGIN_QUEUE;break;
      case TraceDiscovery::LOGIN_TX:_phase=LOGIN_TX;break;
      case TraceDiscovery::LOGIN_WAIT:_phase=LOGIN_WAIT;break;
      default:break;
    }
  }
  void beginSettle(uint32_t now) {
    discardQueued();_control.collect();_phase=SETTLING;_settle_at=_last_candidate=now;
    _settle_delay=TraceTimingPolicy::settleDelay(_tx.traceAirtime(_routes[0].maxPacketLength()),_tx.traceRandom());
  }
  bool addRoute(const TraceRoute& route,uint32_t now,int8_t snr=INT8_MIN) {
    unsigned i=0;while(i<_route_count && !_routes[i].equals(route))++i;
    if(i==_route_count && _route_count<3)_routes[_route_count++]=route;
    if(i>=_route_count)return false;
    _seen[i]=now;_snr[i]=snr;_sequence[i]=++_observations;_last_candidate=now;return true;
  }
  bool advertFallback(Error error) {
    TraceRoute hint;
    if(_tx.traceAdvertRoute(_key,hint) &&
       _budget.fits(hint,_tx.traceAirtime(hint.maxPacketLength()),2500,_tx.traceNow(),_deadline,_queue_used)) {
      _advert=true;addRoute(hint,_tx.traceNow());beginSettle(_tx.traceNow());return true;
    }
    finish(error);return false;
  }
  unsigned bestRoute(uint32_t now,bool alternate) const {
    unsigned best=0;
    for(unsigned i=1;i<_route_count;++i) {
      const auto& a=_routes[i];const auto& b=_routes[best];
      unsigned ar=_history.rank(a,0,_trace_profile,now),br=_history.rank(b,0,_trace_profile,now);
      uint32_t aa=_tx.traceAirtime(a.maxPacketLength()),ba=_tx.traceAirtime(b.maxPacketLength());
      uint64_t ad=alternate?uint64_t(traceNovelEdges(a,_probe.route()))*b.repeaters*(ba+1ULL):0;
      uint64_t bd=alternate?uint64_t(traceNovelEdges(b,_probe.route()))*a.repeaters*(aa+1ULL):0;
      bool better=ar>br;
      if(ar==br) {
        if(ad!=bd)better=ad>bd;
        else if(aa!=ba)better=aa<ba;
        else if(_seen[i]!=_seen[best])better=uint32_t(now-_seen[i])<uint32_t(now-_seen[best]);
        else if(_snr[i]!=_snr[best])better=_snr[i]>_snr[best];
        else better=_sequence[i]>_sequence[best];
      }
      if(better)best=i;
    }
    return best;
  }

  void commit(const RepeaterTraceProbe::Result& result) {
    _result=result;_has_result=true;
    _advert=_attempt_source[result.attempt]==2;_login_path=_attempt_source[result.attempt]==1;
    _history.record(result.route,0,_trace_profile,_tx.traceNow(),true,result.elapsed_millis);
    finish(OK);
  }
  void backoff(uint32_t now) {
    _next_at=now+TraceTimingPolicy::localBackoff(_local_failures)+_tx.traceRandom()%251;
    if(_local_failures<2)++_local_failures;
  }
public:
  explicit RepeaterTraceSession(RepeaterTraceTransport& tx):_tx(tx){}
  bool active() const{return _phase>=DISCOVERY_QUEUE && _phase<=FINAL_GRACE;}
  Phase phase() const{return _phase;} Error error() const{return _error;}
  const uint8_t* key() const{return _key;}
  const uint8_t* peer(unsigned index) const{return _control.peer(index);}
  bool hasResult() const{return _has_result;}
  const RepeaterTraceProbe::Result& result() const{return _result;}
  unsigned discoveryAttempt() const{return _control.attempt();}
  unsigned loginAttempt() const{return _control.loginAttempt();}
  unsigned traceAttempt() const{return _probe.retryStarted()?2:1;}
  unsigned floodSubmissions() const{return _flood_submissions;}
  unsigned loginSubmissions() const{return _login_submissions;}
  unsigned traceSubmissions() const{return _trace_submissions;}
  uint8_t furthest(unsigned attempt) const{return attempt<2?_furthest[attempt]:0;}
  bool advertRoute() const{return _advert;}
  bool loginPath() const{return _login_path;}
  uint32_t queueUsedMillis() const { return _queue_used; }
  uint32_t maximumMillis() const{return _budget.duration;}
  uint32_t remainingMillis() const{return active()?TraceTransactionBudget::remaining(_tx.traceNow(),_deadline):0;}
  uint32_t displayDeadline() const{return _deadline+1000;}
  bool ownsTarget(const uint8_t* key) const{return active()&&key&&!memcmp(key,_key,32);}
  bool start(const uint8_t* key,bool allowed,uint8_t width) {
    if(active()||!key)return false;
    _budget=TraceTransactionBudget();_deadline=0;
    _probe=RepeaterTraceProbe();_has_result=false;_result=RepeaterTraceProbe::Result();
    if(memcmp(key,_key,32))_history.clear();
    memcpy(_key,key,32);_error=OK;_route_count=0;_owned=nullptr;
    _flood_submissions=_login_submissions=_trace_submissions=0;_queue_used=0;
    _furthest[0]=_furthest[1]=0;_advert=_login_path=false;_trace_profile=_profile;
    _attempt_source[0]=_attempt_source[1]=0;
    if(!allowed){_phase=FAILED;_error=POWER_RESTRICTED;return false;}
    if(width<1||width>2){_phase=FAILED;_error=UNSUPPORTED_PATH;return false;}
    TraceRoute sizing;bool known=_tx.traceAdvertRoute(key,sizing);
    if(!known){sizing.repeaters=10;sizing.flags=width-1;sizing.bytes=19*width;}
    unsigned scope=_tx.traceScopeBytes();
    if(!_budget.calculate(_tx.tracePublicLogin(),known,sizing.repeaters,
        _tx.traceAirtime(22+scope),_tx.traceAirtime(53+scope),_tx.traceAirtime(sizing.maxPacketLength()))) {
      _phase=FAILED;_error=BUDGET_EXHAUSTED;return false;
    }
    uint32_t now=_tx.traceNow();_deadline=now+_budget.duration;_sample_at=now;_next_at=now;
    uint32_t reserve=_budget.probe_reserve;
    if(uint64_t(reserve)+_budget.discovery_wait+TraceTimingPolicy::transmitWatchdog(_tx.traceAirtime(22+scope))>=_budget.duration)
      reserve=TraceTimingPolicy::attemptTimeout(1,_tx.traceAirtime(sizing.maxPacketLength()))+5500;
    _control.begin(key,now,_tx.tracePublicLogin(),_deadline-reserve,
                   _budget.discovery_wait,_budget.login_wait,8000);
    syncControl();return true;
  }
  void cancel(Error reason=USER_CANCELLED) {
    if(!active())return;discardQueued();_probe.cancel();_control.cancel();_error=reason;_phase=CANCELLED;
  }
  void contextChanged() {
    bool had=_has_result;cancel(RADIO_CHANGED);
    if(had){_phase=CANCELLED;_error=RADIO_CHANGED;}
    _has_result=false;_history.clear();++_profile;
  }
  bool pathResponse(const uint8_t* key,uint32_t tag,const uint8_t* path,uint8_t encoded,int8_t snr=INT8_MIN) {
    if(!key || !_control.owns(key,tag))return false;
    if(!active() || due(_tx.traceNow(),_deadline) || !_control.current(key,tag))return true;
    TraceRoute route;auto validity=TraceRoute::fromOutbound(path,encoded,key,route);
    if(validity!=TraceRoute::VALID){
      if(_control.searching())advertFallback(validity==TraceRoute::TOO_LONG?PATH_TOO_LONG:UNSUPPORTED_PATH);
      return true;
    }
    addRoute(route,_tx.traceNow(),snr);
    if(_control.searching())beginSettle(_tx.traceNow());return true;
  }
  // Called only for an authenticated full-identity response, never raw RX bytes.
  bool loginResponse(const uint8_t* key,const uint8_t* data,size_t size,const uint8_t* path=nullptr,uint8_t encoded=0) {
    if(!active() || !_control.login())return false;
    if(!_control.loginResponse(key,data,size,_tx.traceNow()))return false;
    discardQueued();
    if(_control.phase()==TraceDiscovery::QUEUE && path) {
      TraceRoute route;
      if(TraceRoute::fromOutbound(path,encoded,key,route)==TraceRoute::VALID) {
        _login_path=true;addRoute(route,_tx.traceNow());beginSettle(_tx.traceNow());return true;
      }
    }
    syncControl();return true;
  }
  void txStarted(mesh::Packet* packet,uint32_t now) {
    if(!active()||packet!=_owned)return;
    sampleQueues(now);
    if(_control.active()||_control.login()){_control.started(packet,now);syncControl();}
    else if(_probe.onTxStarted(packet,now))_phase=TRACE_WAIT;
  }
  void txComplete(mesh::Packet* packet) {
    if(!active())return;
    sampleQueues(_tx.traceNow());
    _control.completed(packet,_tx.traceNow());syncControl();
    _probe.onTxComplete(packet);if(packet==_owned)_owned=nullptr;
  }
  void txFailed(mesh::Packet* packet,uint32_t now) {
    if(!active() || !packet || packet!=_owned)return;
    sampleQueues(now);_owned=nullptr;
    if(_control.active()||_control.login()){_control.failed(packet,now);syncControl();}
    else if(_probe.onTxFailed(packet,now)) {
      if(!_probe.retryStarted())_probe.retryAfterLocalFailure();
      _phase=_probe.retryPending()?TRACE_WAIT:FINAL_GRACE;
    }
  }
  bool receive(const mesh::Packet* packet,uint32_t tag,uint32_t auth,uint8_t flags,
               const uint8_t* snrs,const uint8_t* hashes,uint8_t length) {
    if(!active() || due(_tx.traceNow(),_deadline) || !_probe.isActive())return false;
    RepeaterTraceProbe::Result result;
    auto r=_probe.receive(packet,tag,auth,flags,snrs,hashes,length,_tx.traceNow(),result);
    if(r==RepeaterTraceProbe::COMPLETE)commit(result);return r!=RepeaterTraceProbe::IGNORED;
  }
  void observe(const mesh::Packet* packet) {
    uint8_t attempt;uint32_t elapsed;
    if(_probe.matchProgress(packet,attempt,elapsed,_tx.traceNow()) && packet->path_len>_furthest[attempt])
      _furthest[attempt]=packet->path_len;
  }
  void tick() {
    if(!active())return;
    uint32_t now=_tx.traceNow();sampleQueues(now);
    if(due(now,_deadline) || _queue_used>=TraceTransactionBudget::QUEUE_MS) {
      RepeaterTraceProbe::Result fallback;
      if(_probe.takeFallback(fallback))commit(fallback);else finish(_queue_used>=TraceTransactionBudget::QUEUE_MS?RADIO_BUSY:BUDGET_EXHAUSTED);return;
    }
    if(_control.searching()) {
      if(_control.queuedPacket() && (!_control.canStart(now) || _control.queueExpired(now)))discardQueued();
      _control.tick(now);syncControl();
      if(_control.phase()==TraceDiscovery::ADVERT_WAIT) {
        auto e=_control.error();
        advertFallback(e==TraceDiscovery::BUSY?RADIO_BUSY:e==TraceDiscovery::TX_FAILED?TRANSMIT_FAILED:
          e==TraceDiscovery::LOGIN_SILENT || e==TraceDiscovery::LOGIN_ROLE?GUEST_UNAVAILABLE:DISCOVERY_TIMEOUT);return;
      }
      if(_control.ready(now)) {
        uint32_t tag=_tx.traceDiscoveryTag();
        if(!tag){_control.clockFailed();advertFallback(CLOCK_FAILED);return;}
        bool login=_control.login();
        _owned=login?_tx.traceQueueLogin(_key,tag):_tx.traceQueueDiscovery(_key,tag);
        if(!_owned){_control.rejected(now,_tx.traceRandom());return;}
        uint32_t watchdog=TraceTimingPolicy::transmitWatchdog(_tx.traceAirtime(_owned->getRawLength()));
        if(uint64_t(watchdog)+(login?_budget.login_wait:_budget.discovery_wait)>_control.controlRemaining(now)) {
          discardQueued();_control.budgetExhausted();return;
        }
        _control.queued(_owned,tag,_control.remaining(now),watchdog);
        if(login)++_login_submissions;else ++_flood_submissions;
      }
      return;
    }
    if(_phase==SETTLING) {
      uint32_t quiet=TraceTimingPolicy::quietInterval(_tx.traceAirtime(_routes[0].maxPacketLength()));
      if(now-_settle_at<2500 && (now-_settle_at<_settle_delay || now-_last_candidate<quiet))return;
      auto route=_routes[bestRoute(now,false)];
      if(!_budget.fits(route,_tx.traceAirtime(route.maxPacketLength()),0,now,_deadline,_queue_used)) {
        finish(BUDGET_EXHAUSTED);return;
      }
      uint32_t tag=identifier(),auth=identifier(tag);
      if(_probe.start(route,tag,auth,now)!=RepeaterTraceProbe::STARTED){finish(UNSUPPORTED_PATH);return;}
      _attempt_source[0]=_advert?2:_login_path?1:0;
      _phase=TRACE_QUEUE;_next_at=now;_local_failures=0;
    }
    if(_probe.isTimedOut(now)) {
      _history.record(_probe.route(),0,_trace_profile,now,false);
    }
    if(_probe.retryPending()) {
      discardQueued();TraceRoute original=_probe.route(),retry=_routes[bestRoute(now,true)];
      TraceRoute hint;bool hinted=false;
      if(retry.equals(original) && _tx.traceAdvertRoute(_key,hint,&original)){retry=hint;hinted=true;}
      uint32_t delay=1000+_tx.traceRandom()%1001;
      auto fits=[&](const TraceRoute& r){return _budget.fits(r,_tx.traceAirtime(r.maxPacketLength()),delay,now,_deadline,_queue_used);};
      if(!fits(retry)){retry=original;hinted=false;}
      if(!fits(retry)) {_probe.queueFailed(now);_phase=FINAL_GRACE;}
      else {
        uint32_t tag=identifier(_probe.tag()),auth=identifier(_probe.authCode());
        if(!_probe.beginRetry(tag,auth,now,&retry)){finish(RETRY_NOT_SENT);return;}
        _attempt_source[1]=hinted?2:retry.equals(original)?_attempt_source[0]:0;
        _advert=_attempt_source[1]==2;_login_path=_attempt_source[1]==1;
        _phase=RETRY_PAUSE;_next_at=now+delay;_local_failures=0;
      }
    }
    if(_phase==RETRY_PAUSE){if(!due(now,_next_at))return;_probe.resetQueueClock(now);_phase=TRACE_QUEUE;}
    if(_probe.isFinalizing()) {
      RepeaterTraceProbe::Result fallback;
      if(_probe.takeFallback(fallback)){commit(fallback);return;}
      _phase=FINAL_GRACE;_probe.advanceFinal(now);
      if(_probe.failurePending())finish(_probe.failure()==RepeaterTraceProbe::RETRY_NOT_SENT?RETRY_NOT_SENT:
        _probe.failure()==RepeaterTraceProbe::RADIO_BUSY?RADIO_BUSY:
        _probe.failure()==RepeaterTraceProbe::TRANSMIT_FAILED?TRANSMIT_FAILED:TRACE_TIMEOUT);
      return;
    }
    if(_phase==TRACE_QUEUE) {
      if(_probe.isQueueTimedOut(now)){discardQueued();_probe.queueFailed(now);return;}
      if(!_owned && due(now,_next_at)) {
        _probe.setAirtimeEstimate(_tx.traceAirtime(_probe.route().maxPacketLength()));
        _owned=_tx.traceQueueLoop(_probe.route(),_probe.tag(),_probe.authCode());
        if(_owned){_probe.setQueuedPacket(_owned);++_trace_submissions;}else backoff(now);
      }
    }
  }
  static const char* errorName(Error error) {
    switch(error) {
      case OK:return "Complete";case DISCOVERY_TIMEOUT:return "Discovery/login: no route";
      case RADIO_BUSY:return "Radio queue busy";case TRANSMIT_FAILED:return "Radio transmit failed";
      case RETRY_NOT_SENT:return "Trace retry not sent";case TRACE_TIMEOUT:return "Trace: no result after retry";
      case UNSUPPORTED_PATH:return "Unsupported path/hash width";case PATH_TOO_LONG:return "Path exceeds ten repeaters";
      case RADIO_CHANGED:return "Radio settings changed";case NODE_REMOVED:return "Target removed";
      case POWER_RESTRICTED:return "Radio/power unavailable";case BUDGET_EXHAUSTED:return "Trace: time budget exhausted";
      case CLOCK_FAILED:return "Request clock unavailable";case GUEST_UNAVAILABLE:return "Guest login unavailable";
      default:return "Cancelled";
    }
  }
  const char* phaseName() const {
    switch(_phase) {
      case DISCOVERY_QUEUE:return "Discovery: queued";case DISCOVERY_TX:return "Discovery: transmitting";
      case DISCOVERY_WAIT:return "Discovering route";case LOGIN_QUEUE:return "Guest login: queued";
      case LOGIN_TX:return "Guest login: transmitting";case LOGIN_WAIT:return "Guest login: awaiting reply";
      case SETTLING:return _advert?"Advert route: settling":_login_path?"Login path: settling":"Route found: settling";
      case TRACE_QUEUE:return "Trace: waiting for radio";case TRACE_WAIT:return "Tracing loop";
      case RETRY_PAUSE:return "Waiting to retry trace";case FINAL_GRACE:return "Waiting for late result";
      case IDLE:return "Ready";default:return errorName(_error);
    }
  }
  static int16_t outward(const RepeaterTraceProbe::Result& r,unsigned edge) {
    return edge<r.route.repeaters?(int8_t)r.snrs[edge]:0;
  }
  static int16_t returning(const RepeaterTraceProbe::Result& r,unsigned edge) {
    return edge<r.route.repeaters?(int8_t)r.snrs[2*r.route.repeaters-1-edge]:0;
  }
};
} // namespace zen
