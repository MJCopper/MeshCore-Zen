#pragma once
#include "RepeaterTraceProbe.h"
#include "TraceRouteHistory.h"

namespace zen {
class RepeaterTraceTransport {
public:
  virtual uint32_t traceNow() const=0;
  virtual uint32_t traceRandom()=0;
  virtual uint32_t traceDiscoveryTag()=0; // same monotonic RTC clock as ordinary requests
  virtual uint32_t traceAirtime(unsigned bytes) const=0;
  virtual uint32_t traceFloodTimeout(uint32_t airtime) const=0;
  virtual mesh::Packet* traceQueueDiscovery(const uint8_t* key,uint32_t tag)=0;
  virtual mesh::Packet* traceQueueLoop(const TraceRoute& route,uint32_t tag,uint32_t auth)=0;
  virtual bool traceCancelQueued(mesh::Packet* packet)=0;
};

// One user-owned transaction. All timestamps are relative/wrap-safe; no heap,
// no contact mutations, no channel report, and at most two emitting submissions
// in each phase. The loop probe and timing policy come from the sensor Bot.
class RepeaterTraceSession {
public:
  enum Phase : uint8_t { IDLE, DISCOVERY_QUEUE, DISCOVERY_WAIT, SETTLING, TRACE_QUEUE,
    TRACE_WAIT, RETRY_PAUSE, FINAL_GRACE, COMPLETE, FAILED, CANCELLED };
  enum Error : uint8_t { OK, DISCOVERY_TIMEOUT, RADIO_BUSY, TRANSMIT_FAILED,
    RETRY_NOT_SENT, TRACE_TIMEOUT, UNSUPPORTED_PATH, PATH_TOO_LONG, RADIO_CHANGED,
    NODE_REMOVED, POWER_RESTRICTED, USER_CANCELLED };
private:
  RepeaterTraceTransport& _tx;
  Phase _phase=IDLE;
  Error _error=OK;
  uint8_t _key[32]{};
  struct DiscoveryAttempt { uint32_t tag=0,started=0,timeout=0; bool transmitted=false; } _discovery[2];
  TraceRoute _routes[3];
  uint32_t _seen[3]{},_sequence[3]{},_observations=0;
  int8_t _snr[3]{};
  uint8_t _route_count=0,_discovery_index=0,_local_failures=0;
  uint32_t _phase_at=0,_next_at=0,_settle_at=0,_last_candidate=0,_settle_delay=0;
  uint32_t _profile=0,_trace_profile=0;
  mesh::Packet* _owned=nullptr;
  RepeaterTraceProbe _probe;
  RepeaterTraceProbe::Result _result{};
  TraceRouteHistory _history;
  bool _has_result=false;
  uint8_t _furthest[2]{};
  unsigned _flood_submissions=0,_trace_submissions=0;
  static bool due(uint32_t now,uint32_t deadline) { return int32_t(now-deadline)>=0; }
  uint32_t identifier(uint32_t avoid=0) {
    uint32_t n=_tx.traceRandom();
    if (!n || n==avoid) { n=avoid+1; if (!n) n=1; }
    return n;
  }
  void discardQueued() { if (_owned) _tx.traceCancelQueued(_owned); _owned=nullptr; }
  void finish(Error error) {
    discardQueued(); _probe.cancel(); _error=error;
    _phase=error==OK?COMPLETE:FAILED;
  }
  void discoveryQueue(uint8_t index,uint32_t now) {
    discardQueued(); _discovery_index=index;
    _discovery[index]=DiscoveryAttempt();
    _discovery[index].tag=_tx.traceDiscoveryTag();
    _phase=DISCOVERY_QUEUE; _phase_at=now; _next_at=now; _local_failures=0;
  }
  void discoveryMiss(uint32_t now,Error reason) {
    discardQueued();
    if (_route_count) { beginSettle(now); return; }
    if (!_discovery_index) discoveryQueue(1,now);
    else finish(reason);
  }
  void beginSettle(uint32_t now) {
    discardQueued(); _phase=SETTLING; _settle_at=now; _last_candidate=now;
    _settle_delay=TraceTimingPolicy::settleDelay(_tx.traceAirtime(64),_tx.traceRandom());
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
    _result=result; _has_result=true;
    _history.record(result.route,0,_trace_profile,_tx.traceNow(),true,result.elapsed_millis);
    finish(OK);
  }
  void backoff(uint32_t now) {
    _next_at=now+TraceTimingPolicy::localBackoff(_local_failures)+_tx.traceRandom()%251;
    if (_local_failures<2) ++_local_failures;
  }
public:
  explicit RepeaterTraceSession(RepeaterTraceTransport& tx):_tx(tx) {}
  bool active() const { return _phase>=DISCOVERY_QUEUE&&_phase<=FINAL_GRACE; }
  uint32_t displayDeadline() const {
    if(_phase==DISCOVERY_QUEUE) return _phase_at+TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS+5000;
    if(_phase==DISCOVERY_WAIT) {
      const auto& d=_discovery[_discovery_index];return d.started+d.timeout+5000;
    }
    if(_phase==SETTLING) return _settle_at+7500;
    if(_phase==RETRY_PAUSE) return _next_at+5000;
    return _probe.waitDeadline()+5000;
  }
  Phase phase() const { return _phase; }
  Error error() const { return _error; }
  const uint8_t* key() const { return _key; }
  bool hasResult() const { return _has_result; }
  const RepeaterTraceProbe::Result& result() const { return _result; }
  unsigned discoveryAttempt() const { return _discovery_index+1; }
  unsigned traceAttempt() const { return _probe.retryStarted()?2:1; }
  unsigned floodSubmissions() const { return _flood_submissions; }
  unsigned traceSubmissions() const { return _trace_submissions; }
  uint8_t furthest(unsigned attempt) const { return attempt<2?_furthest[attempt]:0; }
  bool ownsTarget(const uint8_t* key) const { return active()&&key&&!memcmp(key,_key,32); }
  bool start(const uint8_t* key,bool allowed,uint8_t hash_width) {
    if(active()||!key) return false;
    _probe=RepeaterTraceProbe();_has_result=false;_result=RepeaterTraceProbe::Result();
    if(memcmp(_key,key,32)) _history.clear();
    memcpy(_key,key,32);_error=OK;_route_count=0;_owned=nullptr;
    _flood_submissions=_trace_submissions=0;_furthest[0]=_furthest[1]=0;
    _discovery[0]=DiscoveryAttempt();_discovery[1]=DiscoveryAttempt();
    _trace_profile=_profile;
    if(!allowed) { _phase=FAILED;_error=POWER_RESTRICTED;return false; }
    if(hash_width<1||hash_width>2) { _phase=FAILED;_error=UNSUPPORTED_PATH;return false; }
    discoveryQueue(0,_tx.traceNow());return true;
  }
  void cancel(Error reason=USER_CANCELLED) {
    if(!active()) return;
    discardQueued();_probe.cancel();_error=reason;_phase=CANCELLED;
  }
  void contextChanged() {
    bool had_result=_has_result;
    cancel(RADIO_CHANGED);
    if(had_result){_phase=CANCELLED;_error=RADIO_CHANGED;}
    _has_result=false;_history.clear();++_profile;
  }
  // Authenticated/decrypted PATH response. Original outbound path is supplied
  // by the target, not inferred from the return flood or an advert.
  bool pathResponse(const uint8_t* key,uint32_t tag,const uint8_t* path,uint8_t encoded,int8_t snr=INT8_MIN) {
    if(!key||memcmp(key,_key,32)) return false;
    bool matching=false;
    for(const auto& d:_discovery) if(d.transmitted&&d.tag==tag) matching=true;
    if(!matching) return false;
    if(!active()) return true; // absorb our late PATH without updating saved contacts
    TraceRoute route;
    auto validity=TraceRoute::fromOutbound(path,encoded,_key,route);
    if(validity!=TraceRoute::VALID) {
      if(_phase==DISCOVERY_WAIT||_phase==DISCOVERY_QUEUE)
        finish(validity==TraceRoute::TOO_LONG?PATH_TOO_LONG:UNSUPPORTED_PATH);
      return true;
    }
    unsigned index=0;while(index<_route_count&&!_routes[index].equals(route))++index;
    if(index==_route_count&&_route_count<3)_routes[_route_count++]=route;
    if(index<_route_count){_seen[index]=_tx.traceNow();_snr[index]=snr;_sequence[index]=++_observations;}
    _last_candidate=_tx.traceNow();
    if(_phase==DISCOVERY_WAIT||_phase==DISCOVERY_QUEUE) beginSettle(_tx.traceNow());
    return true;
  }
  void txStarted(mesh::Packet* packet,uint32_t now) {
    if(!active()||packet!=_owned) return;
    if(_phase==DISCOVERY_QUEUE) {
      auto& d=_discovery[_discovery_index];d.started=now;d.transmitted=true;
      d.timeout=_tx.traceFloodTimeout(_tx.traceAirtime(packet->getRawLength()));
      _phase=DISCOVERY_WAIT;
    } else if(_probe.onTxStarted(packet,now)) _phase=TRACE_WAIT;
  }
  void txComplete(mesh::Packet* packet) {
    _probe.onTxComplete(packet);if(packet==_owned) _owned=nullptr;
  }
  void txFailed(mesh::Packet* packet,uint32_t now) {
    if(packet!=_owned) return;
    _owned=nullptr;
    if(_phase==DISCOVERY_QUEUE||_phase==DISCOVERY_WAIT) discoveryMiss(now,TRANSMIT_FAILED);
    else if(_probe.onTxFailed(packet,now)) {
      if(!_probe.retryStarted()) _probe.retryAfterLocalFailure();
      _phase=_probe.retryPending()?TRACE_WAIT:FINAL_GRACE;
    }
  }
  bool receive(const mesh::Packet* packet,uint32_t tag,uint32_t auth,uint8_t flags,
               const uint8_t* snrs,const uint8_t* hashes,uint8_t length) {
    if(!active()||!_probe.isActive()) return false;
    RepeaterTraceProbe::Result result;
    auto r=_probe.receive(packet,tag,auth,flags,snrs,hashes,length,_tx.traceNow(),result);
    if(r==RepeaterTraceProbe::COMPLETE) commit(result);
    return r!=RepeaterTraceProbe::IGNORED;
  }
  void observe(const mesh::Packet* packet) {
    uint8_t attempt;uint32_t elapsed;
    if(_probe.matchProgress(packet,attempt,elapsed,_tx.traceNow()) && packet->path_len>_furthest[attempt])
      _furthest[attempt]=packet->path_len;
  }
  void tick() {
    if(!active()) return;
    const uint32_t now=_tx.traceNow();
    if(_phase==DISCOVERY_QUEUE) {
      if(now-_phase_at>=TraceTimingPolicy::QUEUE_TIMEOUT_MILLIS) { discoveryMiss(now,RADIO_BUSY);return; }
      if(!_owned&&due(now,_next_at)) {
        _owned=_tx.traceQueueDiscovery(_key,_discovery[_discovery_index].tag);
        if(_owned) ++_flood_submissions;else backoff(now);
      }
      return;
    }
    if(_phase==DISCOVERY_WAIT) {
      const auto& d=_discovery[_discovery_index];
      if(now-d.started>=d.timeout) discoveryMiss(now,DISCOVERY_TIMEOUT);
      return;
    }
    if(_phase==SETTLING) {
      uint32_t quiet=TraceTimingPolicy::quietInterval(_tx.traceAirtime(64));
      if(now-_settle_at<2500 && (now-_settle_at<_settle_delay || now-_last_candidate<quiet)) return;
      uint32_t tag=identifier(),auth=identifier(tag);
      if(_probe.start(_routes[bestRoute(now,false)],tag,auth,now)!=RepeaterTraceProbe::STARTED) {
        finish(UNSUPPORTED_PATH);return;
      }
      _phase=TRACE_QUEUE;_next_at=now;_local_failures=0;
    }
    if(_probe.isTimedOut(now))
      _history.record(_probe.route(),0,_trace_profile,now,false);
    if(_probe.retryPending()) {
      discardQueued();
      uint32_t tag=identifier(_probe.tag()),auth=identifier(_probe.authCode());
      if(!_probe.beginRetry(tag,auth,now,&_routes[bestRoute(now,true)])) { finish(RETRY_NOT_SENT);return; }
      _phase=RETRY_PAUSE;_next_at=now+1000+_tx.traceRandom()%1001;_local_failures=0;
    }
    if(_phase==RETRY_PAUSE) {
      if(!due(now,_next_at)) return;
      _probe.resetQueueClock(now);_phase=TRACE_QUEUE;
    }
    if(_probe.isFinalizing()) {
      RepeaterTraceProbe::Result result;
      if(_probe.takeFallback(result)) { commit(result);return; }
      _phase=FINAL_GRACE;_probe.advanceFinal(now);
      if(_probe.failurePending()) {
        Error error=_probe.failure()==RepeaterTraceProbe::RADIO_BUSY?RADIO_BUSY:
          _probe.failure()==RepeaterTraceProbe::TRANSMIT_FAILED?TRANSMIT_FAILED:
          _probe.failure()==RepeaterTraceProbe::RETRY_NOT_SENT?RETRY_NOT_SENT:TRACE_TIMEOUT;
        finish(error);
      }
      return;
    }
    if(_phase==TRACE_QUEUE) {
      if(_probe.isQueueTimedOut(now)) { discardQueued();_probe.queueFailed(now);return; }
      if(!_owned&&due(now,_next_at)) {
        _probe.setAirtimeEstimate(_tx.traceAirtime(_probe.route().maxPacketLength()));
        _owned=_tx.traceQueueLoop(_probe.route(),_probe.tag(),_probe.authCode());
        if(_owned) { _probe.setQueuedPacket(_owned);++_trace_submissions; }
        else backoff(now);
      }
    }
  }
  static const char* errorName(Error error) {
    switch(error) {
      case OK:return "Complete";case DISCOVERY_TIMEOUT:return "Discovery: no reply after retry";
      case RADIO_BUSY:return "Radio queue busy";case TRANSMIT_FAILED:return "Radio transmit failed";
      case RETRY_NOT_SENT:return "Trace retry not sent";case TRACE_TIMEOUT:return "Trace: no result after retry";
      case UNSUPPORTED_PATH:return "Unsupported path/hash width";case PATH_TOO_LONG:return "Path exceeds ten repeaters";
      case RADIO_CHANGED:return "Radio settings changed";case NODE_REMOVED:return "Target removed";
      case POWER_RESTRICTED:return "Radio/power unavailable";default:return "Cancelled";
    }
  }
  const char* phaseName() const {
    switch(_phase) {
      case DISCOVERY_QUEUE:return "Discovery: waiting for radio";case DISCOVERY_WAIT:return "Discovering route";
      case SETTLING:return "Route found: settling";case TRACE_QUEUE:return "Trace: waiting for radio";
      case TRACE_WAIT:return "Tracing loop";case RETRY_PAUSE:return "Waiting to retry trace";
      case FINAL_GRACE:return "Waiting for late result";case IDLE:return "Ready";
      default:return errorName(_error);
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
