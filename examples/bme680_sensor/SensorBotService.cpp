#include "SensorBotService.h"
#include <helpers/TransportKeyStore.h>

static const uint32_t TRACE_REPLY_DELAY_MILLIS = 1000;

static const char* airQualityLabel(float score) {
  return score <= 50 ? "Good" :
         score <= 100 ? "Moderate" :
         score <= 150 ? "Poor" :
         score <= 200 ? "Unhealthy" :
         score <= 300 ? "Very poor" : "Hazardous";
}

static void formatAirQuality(char* dest, size_t size, float score, uint8_t sample_count) {
  if (sample_count < 10) {
    snprintf(dest, size, "Air Quality: Warming Up (%u/10)", sample_count);
  } else {
    snprintf(dest, size, "Air Quality: %.0f/500 (%s)", score, airQualityLabel(score));
  }
}

static void appendResponseLine(char* response, size_t size, const char* line) {
  size_t used = strlen(response);
  if (used && used < size - 1) response[used++] = '\n';
  if (used < size - 1) StrHelper::strncpy(&response[used], line, size - used);
}

void SensorBotService::begin(FILESYSTEM* fs) {
  repeater_names.begin(fs);
  guest_policy.begin(fs);
  request_clock.begin(fs,_transport.identityKey());
  advert_routes.begin(fs,_transport.identityKey(),_transport.now(),_transport.wallTime(),_transport.trustedWallTime());
  _transport.rng().random(reinterpret_cast<uint8_t*>(&next_trace_tag), sizeof(next_trace_tag));
  _profile = _transport.radioProfile();
  refreshAirtime();
}

bool SensorBotService::cancelQueued(mesh::Packet* packet) {
  if(!packet || !_transport.cancelQueuedPacket(packet)) return false;
  for(uint8_t i=0;i<5;i++) if(_control_packets[i]==packet) {
    _control_packets[i]=nullptr; trace_diagnostics.control[i].cancelled=true;
  }
  return true;
}

void SensorBotService::refreshAirtime() {
  for(unsigned i=1;i<=MAX_TRANS_UNIT;i++) _airtime[i]=_transport.estimateAirtime(i);
  _air_profile=_transport.radioProfile();
}

void SensorBotService::sampleQueues(uint32_t now) {
  auto since=[&](uint32_t due) {
    uint32_t begin=(int32_t)(due-_queue_sample)>0?due:_queue_sample;
    return (int32_t)(now-begin)>0?now-begin:0;
  };
  _count_work_queue=false; _count_report_queue=false;
  if(_transaction) {
    uint32_t elapsed=0;
    if(_prelude && trace_ack.active() && !trace_ack.inFlight()) {
      elapsed=since(trace_ack.due()); _count_work_queue=(int32_t)(now-trace_ack.due())>=0;
    } else if(!_prelude && discovery.queueWaiting(now)) {
      elapsed=since(discovery.queueDue()); _count_work_queue=true;
    } else if(!_prelude && !_settling && trace_probe.waitingForQueue()) {
      uint32_t due=(int32_t)(_probe_not_before-_next_queue_at)>0?_probe_not_before:_next_queue_at;
      elapsed=since(due); _count_work_queue=(int32_t)(now-due)>=0;
    }
    _work_queue_used=TraceTimingPolicy::bounded(uint64_t(_work_queue_used)+elapsed);
    if(trace_report.active() && !trace_report.inFlight()) {
      _report_queue_used=TraceTimingPolicy::bounded(uint64_t(_report_queue_used)+since(trace_report.due()));
      _count_report_queue=(int32_t)(now-trace_report.due())>=0;
    }
  }
  _queue_sample=now;
}

bool SensorBotService::tracePolicyCommand(const char* command,char* reply) {
  // Private/local CLI replies have a 160-byte capacity, as in SensorMesh.
  if(!strcmp(command,"get trace.radio")) { snprintf(reply,160,"> %s",_trace_radio); return true; }
  if(!strcmp(command,"get trace.timing")) {
    snprintf(reply,160,"> max=%lu report=%lu probe=%lu flood=%lu login=%lu queue=%lu/8000 report_queue=%lu/2000 capped=%u probes=%u login_path=%u",
        (unsigned long)_budget.duration,(unsigned long)_budget.report_reserve,(unsigned long)_budget.probe_reserve,
        (unsigned long)_budget.discovery_wait,(unsigned long)_budget.login_wait,(unsigned long)_work_queue_used,
        (unsigned long)_report_queue_used,_budget.capped,_budget.planned_probes,trace_diagnostics.login_path);
    return true;
  }
  if(!strcmp(command,"get trace.airtime")) {
    snprintf(reply,160,"> control=%lu login=%lu probe=%lu public=%lu profile=%lu remaining=%lu",
        (unsigned long)_budget.control_air,(unsigned long)_budget.login_air,(unsigned long)_budget.probe_air,
        (unsigned long)_budget.public_air,(unsigned long)_trace_profile,
        (unsigned long)(_transaction?TraceTransactionBudget::remaining(_transport.now(),_deadline):0));
    return true;
  }
  if(!strcmp(command,"get trace.diag")) {
    snprintf(reply,160,"> rx=%lu decrypted=%lu login_reply=%lu login_ignored=%lu role=%u paths=%lu tag_miss=%lu route_bad=%lu scope_miss=%lu",
        (unsigned long)trace_diagnostics.candidate_replies,(unsigned long)trace_diagnostics.decrypted_replies,
        (unsigned long)trace_diagnostics.accepted_logins,(unsigned long)trace_diagnostics.ignored_logins,
        trace_diagnostics.login_role,(unsigned long)trace_diagnostics.received_paths,
        (unsigned long)trace_diagnostics.unmatched_tags,(unsigned long)trace_diagnostics.route_rejections,
        (unsigned long)trace_diagnostics.scope_mismatches);
    return true;
  }
  if(!strcmp(command,"get trace.peer")) {
    char key[65],scope[33];
    for(unsigned i=0;i<32;i++) sprintf(key+2*i,"%02x",discovery.key()[i]);
    for(unsigned i=0;i<16;i++) sprintf(scope+2*i,"%02x",control_context.scope_key[i]);
    snprintf(reply,160,"> target=%s scope=%s width=%u",key,scope,control_context.hash_size);
    return true;
  }
  if(!strncmp(command,"get trace.control ",18) && strlen(command)==19 && command[18]>='0' && command[18]<='4') {
    unsigned index=command[18]-'0'; const auto& c=trace_diagnostics.control[index];
    snprintf(reply,160,"> %u tag=%lu queued=%lu started=%lu completed=%lu sent=%u failed=%u uncertain=%u clock_ok=%u",
        index,(unsigned long)c.tag,(unsigned long)c.queued,(unsigned long)c.started,(unsigned long)c.completed,
        c.sent,c.failed,c.uncertain,trace_diagnostics.clock_healthy);
    return true;
  }
  if(!strcmp(command,"get trace.login")) {
    sprintf(reply,"> %s",guest_policy.enabled()?"public":"off"); return true;
  }
  if(!strncmp(command,"set trace.login ",16)) {
    auto value=command+16;
    if(strcmp(value,"public") && strcmp(value,"off")) strcpy(reply,"ERR: use public or off");
    else strcpy(reply,guest_policy.set(!strcmp(value,"public"))?"OK":"ERR: policy save failed");
    return true;
  }
  return false;
}

bool SensorBotService::onLoginResponse(const uint8_t* key,const uint8_t* data,size_t length,const mesh::Packet* packet,const uint8_t* path,uint8_t encoded) {
  ++trace_diagnostics.decrypted_replies;
  if(!data || length<13 || data[4]!=0 || data[5]!=0 || data[6]>1) return false;
  // Login has a server timestamp, not our request tag. Absorb recognizable late
  // login frames rather than accidentally accepting one as a discovery PATH.
  if(!_transaction || _prelude || !workRemaining(_transport.now()) ||
     _trace_profile!=_transport.radioProfile()) return true;
  auto queued=discovery.queuedPacket();
  bool accepted=discovery.loginResponse(key,data,length,_transport.now());
  if(accepted && queued) cancelQueued(queued);
  if(accepted) ++trace_diagnostics.accepted_logins;
  else ++trace_diagnostics.ignored_logins;
  if(accepted && packet) {
    trace_diagnostics.peer_clock_regressed=discovery.serverClockRegressed();
    trace_diagnostics.login_role=data[7]&3;
    TransportKey scope{}; memcpy(scope.key,control_context.scope_key,16);
    if(scope.isNull()?packet->hasTransportCodes():!packet->hasTransportCodes() ||
       scope.calcTransportCode(packet)!=packet->transport_codes[0]) ++trace_diagnostics.scope_mismatches;
  }
  if(accepted && discovery.phase()==TraceDiscovery::QUEUE && path) {
    auto validity=TraceRoute::fromOutbound(path,encoded,key,_login_route);
    if(validity==TraceRoute::VALID) {
      discovery.collect(); _login_route_pending=true;
      trace_diagnostics.login_path=true;
    } else ++trace_diagnostics.route_rejections;
  }
  return true;
}

bool SensorBotService::useAdvertRoute(uint32_t now) {
  TraceRoute route{};
  if(!_transport.stableRadioConfiguration() || !workRemaining(now) ||
     !advert_routes.best(discovery.key(),_transport.stableRadioProfile(),trace_scope,now,route,nullptr,&trace_diagnostics.advert_probation)) return false;
  auto packet=discovery.queuedPacket();
  discovery.collect(); if(packet) cancelQueued(packet);
  _advert_route=true; _attempt_advert[0]=true; trace_diagnostics.advert_route=true;
  startDiscovered(route,now); return true;
}

bool SensorBotService::beginAcknowledgement(uint32_t now,const char* destination,const char* fallback,const uint8_t* target_key) {
  if(_air_profile!=_transport.radioProfile()) return false;
  uint32_t air=cachedAirtime(2+4+1+2+((5+PublicResponseQueue::MAX_PART_LENGTH+15)/16)*16);
  _transport.describeTraceRadio(_trace_radio,sizeof(_trace_radio));
  TraceRoute sizing{}; bool known=!_targeted;
  if(_targeted) {
    known=target_key && _transport.stableRadioConfiguration() && advert_routes.best(target_key,
        _transport.stableRadioProfile(),trace_scope,now,sizing);
    if(known) {
      TraceRoute other{};
      if(advert_routes.best(target_key,_transport.stableRadioProfile(),trace_scope,now,other,&sizing) &&
         other.maxPacketLength()>sizing.maxPacketLength()) sizing=other;
    } else {
      sizing.repeaters=TraceRoute::MAX_REPEATERS; sizing.flags=control_context.hash_size-1;
      sizing.bytes=(2*sizing.repeaters-1)*control_context.hash_size;
    }
  } else sizing=trace_probe.route();
  TransportKey scope{}; memcpy(scope.key,control_context.scope_key,16);
  uint8_t extra=scope.isNull()?0:4;
  if(!_budget.calculate(_targeted,guest_policy.enabled(),known,sizing.repeaters,
      cachedAirtime(22+extra),cachedAirtime(53+extra),cachedAirtime(sizing.maxPacketLength()),air)) return false;
  if(!trace_ack.begin(now,_budget.seconds(),TraceTimingPolicy::transmitWatchdog(air),
      _transport.nodeName(),destination,fallback)) return false;
  _advert_route=false; _attempt_advert[0]=_attempt_advert[1]=false;
  if(!++_trace_epoch) ++_trace_epoch;
  _accepted_at=now; _deadline=now+_budget.duration;
  _queue_sample=now; _work_queue_used=_report_queue_used=0;
  _count_work_queue=true; _count_report_queue=false;
  _transaction=_prelude=true;
  return true;
}

void SensorBotService::endTraceWork(uint32_t now) {
  if (!_transaction || !traceBusy() || workRemaining(now) || trace_report.active()) return;
  RepeaterTraceProbe::Result fallback;
  if (trace_probe.takeFallback(fallback)) finishTrace(fallback);
  else discoveryFailure(discovery.pending() || (_targeted && _prelude) ?
      discovery.failureText() : "Trace: no result before timeout");
}

bool SensorBotService::expireTransaction(uint32_t now) {
  if (!_transaction || TraceTransactionBudget::remaining(now, _deadline)) return false;
  bool uncertain = trace_report.inFlight();
  if(uncertain) ++trace_diagnostics.report_failures;
  closeTrace();
  trace_report.expire(uncertain);
  trace_diagnostics.delivery = trace_report.outcome();
  return true;
}

bool SensorBotService::allowPhase(uint32_t now, uint64_t needed) {
  if (expireTransaction(now)) return false;
  if (_transaction && needed > workRemaining(now)) {
    ++trace_diagnostics.budget_omissions;
    RepeaterTraceProbe::Result fallback;
    if(trace_probe.takeFallback(fallback)) finishTrace(fallback);
    else discoveryFailure("Trace: insufficient time for route");
    return false;
  }
  return true;
}

void SensorBotService::pumpAcknowledgement(uint32_t now) {
  if (!_prelude) return;
  if (trace_ack.expired(now)) {
    auto queued = (mesh::Packet*)trace_ack.queuedPacket();
    trace_ack.cancel();
    if (queued) cancelQueued(queued);
  }
  if (trace_ack.ready(now)) {
    auto packet = createPublicPart(trace_ack.text(), trace_context, false);
    if (!packet) { trace_diagnostics.allocations_failed++; trace_ack.retry(now); }
    else {
      _transport.tagTracePacket(packet,_trace_epoch,_deadline);
      uint32_t airtime = cachedAirtime(packet->getRawLength());
      if (!airtime || TraceTimingPolicy::transmitWatchdog(airtime) > workRemaining(now) ||
          !trace_ack.queued(packet, TraceTimingPolicy::transmitWatchdog(airtime))) {
        _transport.releasePacket(packet);
        trace_ack.cancel(); // An inconsistent estimate cannot exceed the advertised bound.
      } else _transport.sendReply(packet, 0, trace_context);
      if (trace_ack.queuedPacket() == packet && !_transport.isQueuedPacket(packet)) {
        trace_diagnostics.queue_failures++;
        trace_ack.retry(now);
      }
    }
  }
  if (!trace_ack.active()) {
    _prelude = false;
    if (_targeted) discovery.resetQueueClock(now);
    else {
      trace_probe.resetQueueClock(now);
      _settle_at = _next_queue_at = _probe_not_before = now;
    }
  }
}

void SensorBotService::loop() {
  uint32_t now = _transport.now();
  beforeRadioLoop();
  // Mesh/Dispatcher has now had its completion/watchdog poll. Do not preempt a
  // legitimate completion merely because a slow application delayed polling.
  if (trace_report.inFlight() && trace_report.expired(_transport.now())) {
    closeTrace();
    trace_report.expire(true);
    trace_diagnostics.report_failures++;
    trace_diagnostics.delivery = trace_report.outcome();
  }
  pumpAcknowledgement(_transport.now());
  repeater_names.loop(_transport.now());
  advert_routes.loop(_transport.now());
  if (!_prelude) {
    if(_login_route_pending) { _login_route_pending=false; startDiscovered(_login_route,_transport.now()); }
    pumpDiscovery(_transport.now());
    now = _transport.now();
    if (_settling && ((uint32_t)(now - _settle_at) >= 2500 ||
        ((uint32_t)(now - _settle_at) >= _settle_delay &&
         (uint32_t)(now - trace_routes.lastSeen()) >= _quiet))) {
      _settling = false;
      trace_probe.resetQueueClock(now);
      _next_queue_at = now;
    }
    if (trace_probe.isTimedOut(now)) {
      trace_diagnostics.timeouts++;
      trace_history.record(trace_probe.route(), trace_context.channel, _trace_profile, now, false, 0, trace_scope, historyTarget());
      if(_targeted) advert_routes.outcome(discovery.key(),trace_probe.route(),_transport.stableRadioProfile(),trace_scope,now,false);
    }
    if (trace_probe.retryPending()) {
      uint32_t tag, auth;
      traceIdentifiers(tag, auth);
      const TraceRoute original = trace_probe.route();
      uint32_t airtimes[3]{};
      for (uint8_t i = 0; i < trace_routes.count(); i++)
        airtimes[i] = cachedAirtime(trace_routes.candidate(i).maxPacketLength());
      auto alternate = trace_routes.best(trace_history, trace_context.channel, _trace_profile, now, airtimes, trace_scope, historyTarget());
      if (alternate && trace_history.rank(trace_probe.route(), trace_context.channel, _trace_profile, now, trace_scope, historyTarget()) >
                       trace_history.rank(*alternate, trace_context.channel, _trace_profile, now, trace_scope, historyTarget())) alternate = NULL;
      TraceRoute advert_alternate{};
      bool retry_advert=_advert_route && !alternate;
      if(_targeted && _transport.stableRadioConfiguration() && !alternate && advert_routes.best(discovery.key(),
          _transport.stableRadioProfile(),trace_scope,now,advert_alternate,&trace_probe.route())) {
        alternate=&advert_alternate; retry_advert=true;
      }
      uint32_t retry_delay = _transport.rng().nextInt(1000, 2001);
      auto fits = [&](const TraceRoute& route) {
        return _budget.fitsProbe(route.repeaters, cachedAirtime(route.maxPacketLength()),
                                 retry_delay, now, _deadline, _work_queue_used);
      };
      if (alternate && !fits(*alternate)) {
        alternate = NULL;
        retry_advert = _attempt_advert[0];
        ++trace_diagnostics.budget_omissions;
      }
      const TraceRoute& retry_route = alternate ? *alternate : original;
      if (!fits(retry_route)) {
        ++trace_diagnostics.budget_omissions;
        _attempt_advert[1] = false;
        _advert_route = _attempt_advert[0];
        trace_diagnostics.advert_route = _advert_route;
        trace_probe.queueFailed(now);
      } else if (!trace_probe.beginRetry(tag, auth, now, alternate)) {
        trace_probe.queueFailed(now);
      } else {
        _attempt_advert[1] = retry_advert;
        _advert_route = retry_advert;
        trace_diagnostics.advert_route = retry_advert;
        _local_failures = 0;
        _next_queue_at = now;
        admitTrace(now, retry_delay);
      }
    }
    if (!_settling) admitTrace(now);
    if (trace_probe.isFinalizing()) {
      RepeaterTraceProbe::Result result;
      if (trace_probe.takeFallback(result)) finishTrace(result);
      else trace_probe.advanceFinal(now);
    }
    if (trace_probe.failurePending() && !trace_report.active()) queueTraceFailure();
    pumpTraceReport(now);
  }
  char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
  SensorReplyContext context;
  if (public_replies.takeDue(now, part, &context) &&
      !sendPublicPart(part, context, 0))
    public_replies.schedule(part, now + 1000, context);
  if (!traceBusy()) _transaction = false;
  sampleQueues(_transport.now());
}

void SensorBotService::beforeRadioLoop() {
  // Expire queued work before MeshCore can start it on a later loop iteration.
  // In-flight packets remain exclusively owned by the dispatcher watchdog.
  uint32_t now = _transport.now();
  sampleQueues(now);
  if(!traceBusy() && !_radio_tx_active && _air_profile!=_transport.radioProfile()) refreshAirtime();
  if(_transaction && _count_work_queue && _work_queue_used>=TraceTransactionBudget::WORK_QUEUE_MILLIS) {
    discoveryFailure("Trace: radio queue budget exhausted"); return;
  }
  if(_transaction && _count_report_queue && _report_queue_used>=TraceTransactionBudget::REPORT_QUEUE_MILLIS) {
    closeTrace(); trace_report.expire(); return;
  }
  if(!traceBusy() && _profile!=_transport.radioProfile()) {
    trace_history.clear(); advert_routes.clear(); _profile=_transport.radioProfile();
  }
  if (expireTransaction(now)) return;
  endTraceWork(now);
  if (traceBusy() && _trace_profile != _transport.radioProfile()) {
    trace_history.clear(); advert_routes.clear(); _profile = _trace_profile = _transport.radioProfile();
    if (trace_report.active() && !trace_probe.isActive() && !discovery.pending()) {
      // A result may already be on air; never follow it with a conflicting
      // failure or transmit its remaining parts on another radio profile.
      closeTrace(); trace_report.expire(true);
    } else discoveryFailure("Trace: radio configuration changed");
  }
  if (_prelude) {
    // Before Dispatcher polling, remove an acknowledgement whose queue lease ended.
    if (!trace_ack.inFlight() && trace_ack.expired(now)) {
      auto packet = (mesh::Packet*)trace_ack.queuedPacket();
      trace_ack.cancel();
      if (packet) cancelQueued(packet);
    }
    return;
  }
  if (discovery.queuedPacket() && !discovery.canStart(now)) {
    auto packet=discovery.queuedPacket(); cancelQueued(packet); discovery.budgetExhausted(); ++trace_diagnostics.budget_omissions;
  }
  if (discovery.queueExpired(now)) {
    auto packet = discovery.queuedPacket();
    if (packet) cancelQueued(packet);
  }
  discovery.tick(now);
  if (!_settling && trace_probe.isQueueTimedOut(now)) {
    auto packet = trace_probe.queuedPacket();
    if (packet) cancelQueued(packet);
    trace_diagnostics.queue_failures++;
    trace_probe.queueFailed(now);
  }
  if (trace_probe.failurePending() && trace_probe.reportExpired(now)) {
    closeTrace();
    trace_report.expire();
  }
  if (!trace_report.inFlight() && trace_report.expired(_transport.now())) {
    closeTrace();  // only queued packets are cancelled; in-flight belongs to Dispatcher
    trace_report.expire();
  }
  trace_diagnostics.delivery = trace_report.outcome();
}

void SensorBotService::admitTrace(uint32_t now, uint32_t delay) {
  if (_prelude || expireTransaction(now)) return;
  if (delay) _probe_not_before = now + delay;
  if (!trace_probe.waitingForQueue() || trace_probe.queuedPacket() ||
      (int32_t)(now - _next_queue_at) < 0) return;
  uint32_t remaining = (int32_t)(_probe_not_before - now) > 0 ? _probe_not_before - now : 0;
  uint32_t probe_air=cachedAirtime(trace_probe.route().maxPacketLength());
  uint64_t needed=(uint64_t)remaining+TraceTimingPolicy::attemptTimeout(trace_probe.route().repeaters,probe_air);
  if (!allowPhase(now, needed)) return;
  auto queued = queueTrace(remaining);
  if (queued != TRACE_QUEUED) {
    trace_probe.setQueuedPacket(NULL);
    if (queued == TRACE_NO_PACKET) trace_diagnostics.allocations_failed++;
    else trace_diagnostics.queue_failures++;
    _next_queue_at = now + TraceTimingPolicy::localBackoff(_local_failures) + _transport.rng().nextInt(0, 251);
    if (_local_failures < 255) _local_failures++;
  }
}

void SensorBotService::pumpTraceReport(uint32_t now, bool queue_early) {
  if (_prelude || expireTransaction(now)) return;
  if (trace_report.ready(now) || (queue_early && trace_report.waiting())) {
    uint8_t payload[MAX_PACKET_PAYLOAD]{};
    uint32_t time = _transport.uniqueTime();
    memcpy(payload, &time, 4);
    size_t length = strlen(trace_report.text());
    memcpy(payload + 5, trace_report.text(), length);
    auto packet = _transport.createGroupDatagram(PAYLOAD_TYPE_GRP_TXT,
                  public_bot.channel(trace_report.channel()), payload, 5 + length);
    if (!packet) {
      trace_diagnostics.allocations_failed++;
      trace_report.retry(now, _transport.rng().nextInt(0, 251));
    } else {
      _transport.tagTracePacket(packet,_trace_epoch,_deadline);
      trace_report.queued(packet, TraceTimingPolicy::transmitWatchdog(
          cachedAirtime(packet->getRawLength())));
      _transport.sendReply(packet, trace_report.delay(now), trace_report.context());
      if (!_transport.isQueuedPacket(packet)) {
        trace_diagnostics.queue_failures++;
        trace_report.retry(now);
      }
    }
  }
  trace_diagnostics.delivery = trace_report.outcome();
}

void SensorBotService::traceIdentifiers(uint32_t& tag, uint32_t& auth) {
  // Monotonic tags prevent a stale attempt from matching a later transaction.
  tag = ++next_trace_tag;
  _transport.rng().random(reinterpret_cast<uint8_t*>(&auth), sizeof(auth));
}

SensorBotService::QueueTraceResult SensorBotService::queueTrace(uint32_t delay_millis) {
  trace_probe.setAirtimeEstimate(cachedAirtime(trace_probe.route().maxPacketLength()));
  auto trace = _transport.createTrace(trace_probe.tag(), trace_probe.authCode(), trace_probe.flags());
  if (!trace) return TRACE_NO_PACKET;
  trace_probe.setQueuedPacket(trace);
  _transport.sendDirect(trace, trace_probe.path(), trace_probe.pathBytes(), delay_millis);
  return _transport.isQueuedPacket(trace) ? TRACE_QUEUED : TRACE_QUEUE_REJECTED;
}

void SensorBotService::observeRoute(const mesh::Packet* packet) {
  // Hash matches are candidate frames only, never proof of a peer identity.
  if(packet && _targeted && packet->payload_len>=2 &&
     (packet->getPayloadType()==PAYLOAD_TYPE_PATH || packet->getPayloadType()==PAYLOAD_TYPE_RESPONSE) &&
     packet->payload[0]==_transport.identityKey()[0] && packet->payload[1]==discovery.key()[0])
    ++trace_diagnostics.candidate_replies;
  if (!_targeted && packet && trace_probe.isActive() && !trace_probe.retryStarted() &&
      packet->isRouteFlood() && packet->getPayloadType() == PAYLOAD_TYPE_GRP_TXT) {
    trace_routes.observe(packet, _transport.now(), _transport.requestScope(*packet));
  }
  trace_diagnostics.observe(packet, trace_probe, _transport.now());
}

void SensorBotService::onTxStarted(mesh::Packet* packet, uint32_t now_millis) {
  if (!packet) return;
  sampleQueues(now_millis);
  _radio_tx_active=true;
  if (expireTransaction(now_millis)) return;
  if (_transaction) {
    if (discovery.queuedPacket() == packet) {
      uint8_t index=discovery.login()?(discovery.loginAttempt()==1?1:discovery.loginAttempt()+1):discovery.attempt()==1?0:2;
      trace_diagnostics.control[index].started=now_millis;
      trace_diagnostics.control[index].sent=true;
      if(discovery.login()) trace_diagnostics.login_starts++;
      else trace_diagnostics.discovery_starts++;
    }
  }
  trace_ack.started(packet, now_millis);
  discovery.started(packet, now_millis);
  if (trace_report.started(packet, now_millis) && trace_report.failure()) {
    // Failure is committed once it goes on air; never send a contradictory result.
    trace_probe.cancel();
    trace_routes.clear();
  }
  if (trace_probe.onTxStarted(packet, now_millis)) {
    trace_diagnostics.starts++;
    trace_diagnostics.probe_submissions++;
  }
  sampleQueues(now_millis);
}

void SensorBotService::logTx(mesh::Packet* packet, int) {
  _radio_tx_active=false;
  sampleQueues(_transport.now());
  for(uint8_t i=0;i<5;i++) if(packet && packet==_control_packets[i]) {
    trace_diagnostics.control[i].completed=_transport.now(); _control_packets[i]=nullptr;
  }
  trace_ack.completed(packet);
  discovery.completed(packet,_transport.now());
  trace_probe.onTxComplete(packet);
  trace_report.completed(packet, _transport.now());
  trace_diagnostics.delivery = trace_report.outcome();
  public_replies.onFirstSent(packet, _transport.now());
  sampleQueues(_transport.now());
}

void SensorBotService::logTxFail(mesh::Packet* packet, int) {
  _radio_tx_active=false;
  sampleQueues(_transport.now());
  for(uint8_t i=0;i<5;i++) if(packet && packet==_control_packets[i]) {
    auto& c=trace_diagnostics.control[i]; c.completed=_transport.now(); c.failed=true;
    c.uncertain=c.sent || !_transport.failedStartDefinitelyNotSent(); _control_packets[i]=nullptr;
  }
  trace_ack.failed(packet, _transport.now(), _transport.failedStartDefinitelyNotSent());
  discovery.failed(packet, _transport.now());
  if (trace_report.failed(packet, _transport.now(), _transport.failedStartDefinitelyNotSent())) {
    trace_diagnostics.report_failures++;
    if (trace_report.outcome() == TraceReportDelivery::UNCERTAIN) {
      trace_probe.cancel();
      trace_routes.clear();
    }
  }
  trace_diagnostics.delivery = trace_report.outcome();
  bool transmitted = trace_probe.currentTransmitted();
  if (trace_probe.onTxFailed(packet, _transport.now())) {
    if (!transmitted) trace_diagnostics.probe_submissions++;
    trace_diagnostics.local_failures++;
    trace_probe.retryAfterLocalFailure();
  }
  public_replies.onFirstFailed(packet);
}

void SensorBotService::closeTrace(bool preserve_deadline) {
  _transport.cancelTraceCopies(_trace_epoch);
  for(auto& packet:_control_packets) packet=nullptr;
  _login_route_pending=false;
  auto acknowledgement = (mesh::Packet*)trace_ack.queuedPacket();
  trace_ack.cancel(); _prelude = false;
  if (acknowledgement) cancelQueued(acknowledgement);
  if (!preserve_deadline) _transaction = false;
  auto discovery_packet = discovery.queuedPacket();
  discovery.cancel();
  if (discovery_packet) cancelQueued(discovery_packet);
  auto pending = trace_probe.queuedPacket();
  auto failure = (mesh::Packet*)trace_report.queuedPacket();
  trace_report.reset();
  _settling = false;
  trace_probe.cancel();
  trace_routes.clear();
  if (pending) cancelQueued(pending);
  if (failure) cancelQueued(failure);
}

void SensorBotService::finishTrace(const RepeaterTraceProbe::Result& result) {
  closeTrace(true);
  if(_targeted) advert_routes.outcome(discovery.key(),result.route,_transport.stableRadioProfile(),trace_scope,_transport.now(),true);
  trace_diagnostics.advert_route=_attempt_advert[result.attempt];
  trace_diagnostics.selected = result;
  trace_diagnostics.has_result = true;
  sendTraceResult(result);
}

void SensorBotService::queueTraceFailure() {
  const char* text;
  switch (trace_probe.failure()) {
    case RepeaterTraceProbe::RADIO_BUSY: text = "Trace: radio busy"; break;
    case RepeaterTraceProbe::TRANSMIT_FAILED: text = "Trace: transmit failed"; break;
    case RepeaterTraceProbe::RETRY_NOT_SENT: text = "Trace: retry not sent"; break;
    default: text = "Trace: no result after retry"; break;
  }
  trace_report.begin(text, _transport.nodeName(), trace_context, _transport.now(),
      workRemaining(_transport.now()) ? _transport.rng().nextInt(500, 2001) : 0,
      TraceTransactionBudget::remaining(_transport.now(), _deadline) < _budget.report_reserve ?
          TraceTransactionBudget::remaining(_transport.now(), _deadline) : _budget.report_reserve, true);
  pumpTraceReport(_transport.now(), true);
}

mesh::Packet* SensorBotService::createPublicPart(const char* response, const SensorReplyContext& context, bool prefix) {
  uint8_t payload[MAX_PACKET_PAYLOAD];
  uint32_t timestamp = _transport.uniqueTime();
  memcpy(payload, &timestamp, sizeof(timestamp));
  payload[4] = 0;
  int prefix_len = prefix ? snprintf(reinterpret_cast<char*>(&payload[5]), sizeof(payload) - 5,
                            "%s: ", _transport.nodeName()) : 0;
  if (prefix_len < 0 || prefix_len >= (int)(sizeof(payload) - 5)) return NULL;
  size_t response_len = strlen(response);
  if (prefix_len + response_len > PublicResponseQueue::MAX_PART_LENGTH) return NULL;
  memcpy(&payload[5 + prefix_len], response, response_len);

  return _transport.createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, public_bot.channel(context.channel), payload,
                             5 + prefix_len + response_len);
}

bool SensorBotService::sendPublicPart(const char* response, const SensorReplyContext& context, uint32_t delay_millis) {
  auto reply_packet = createPublicPart(response, context);
  if (!reply_packet) return false;
  _transport.sendReply(reply_packet, delay_millis, context);
  return _transport.isQueuedPacket(reply_packet);
}

void SensorBotService::sendPublicResponse(const char* response, const SensorReplyContext& context,
                                          uint32_t extra_delay_millis, const char* separator) {
  size_t prefix_len = strlen(_transport.nodeName()) + 2;
  if (prefix_len >= PublicResponseQueue::MAX_PART_LENGTH) return;
  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  bool multipart = PublicResponseQueue::split(response,
                                               PublicResponseQueue::MAX_PART_LENGTH - prefix_len,
                                               first, second, separator);
  uint32_t first_delay = extra_delay_millis + _transport.rng().nextInt(500, 2001);
  auto first_packet = createPublicPart(first, context);
  if (!first_packet) return;
  if (multipart && !public_replies.scheduleAfterFirst(second, first_packet, context)) {
    _transport.releasePacket(first_packet);
    sendPublicPart("Reply queue busy", context, first_delay);
    return;
  }
  _transport.sendReply(first_packet, first_delay, context);
  if (!_transport.isQueuedPacket(first_packet)) public_replies.onFirstFailed(first_packet);
}

void SensorBotService::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                  const uint8_t* app_data, size_t app_data_len) {
  repeater_names.onAdvert(id, timestamp, app_data, app_data_len, _transport.now());
  if(_transport.stableRadioConfiguration() && advert_routes.observe(packet,id,timestamp,app_data,app_data_len,
      _transport.now(),_transport.wallTime(),_transport.trustedWallTime(),_transport.stableRadioProfile(),
      packet?_transport.requestScope(*packet):mesh::PacketScope{})) ++trace_diagnostics.advert_observations;
}

void SensorBotService::onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                                  const uint8_t* path_snrs, const uint8_t* path_hashes, uint8_t path_len) {
  if (_prelude || expireTransaction(_transport.now()) ||
      (_transaction && !workRemaining(_transport.now()))) return;
  RepeaterTraceProbe::Result result;
  auto received = trace_probe.receive(packet, tag, auth_code, flags, path_snrs, path_hashes, path_len,
                                     _transport.now(), result);
  if (received != RepeaterTraceProbe::IGNORED) {
    if (!trace_diagnostics.progress[result.attempt].complete) {
      trace_diagnostics.progress[result.attempt].complete = true;
      trace_history.record(result.route, trace_context.channel, _trace_profile, _transport.now(), true, result.elapsed_millis, trace_scope, historyTarget());
    }
    if (received == RepeaterTraceProbe::COMPLETE) finishTrace(result);
  }
}

void SensorBotService::sendTraceResult(const RepeaterTraceProbe::Result& result) {
  char response[2 * PublicResponseQueue::MAX_PART_LENGTH + 1];
  snprintf(response, sizeof(response), "Trace: %lu ms RTT (avg SNR)", (unsigned long)result.elapsed_millis);
  uint8_t hash_size = 1 << result.route.flags;
  for (uint8_t i = 0; i < result.route.repeaters; i++) {
    char label[13];
    char line[40];
    repeater_names.formatLabel(&result.route.path[i * hash_size], hash_size, label, sizeof(label));
    snprintf(line, sizeof(line), "%u %s %+.1f dB", i + 1, label, result.averageLinkSnrDb(i));
    appendResponseLine(response, sizeof(response), line);
  }
  trace_report.begin(response, _transport.nodeName(), trace_context, _transport.now(),
      TRACE_REPLY_DELAY_MILLIS + _transport.rng().nextInt(500, 2001),
      TraceTransactionBudget::remaining(_transport.now(),_deadline)<_budget.report_reserve?
          TraceTransactionBudget::remaining(_transport.now(),_deadline):_budget.report_reserve);
  pumpTraceReport(_transport.now(), true);
}

int SensorBotService::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
  return public_bot.findChannel(hash, channels, max_matches);
}

void SensorBotService::onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                     uint8_t* data, size_t len) {
  uint8_t metric_mask;
  bool busy_trace, targeted;
  uint8_t prefix[2]{};
  PublicChannelSensorBot::TraceAction action;
  char requester_name[32]{};
  int channel_index = public_bot.channelIndex(channel);
  if (channel_index < 0) return;
  if (!public_bot.accept(type, data, len, _transport.now(), traceBusy(),
                         metric_mask, busy_trace, channel_index, prefix, &targeted, &action,
                         requester_name, sizeof(requester_name))) return;

  const auto scope = _transport.requestScope(*packet);
  const auto context = _transport.replyContext(*packet, channel_index, scope);

  if (metric_mask == PublicChannelSensorBot::REQUEST_TRACE && action != PublicChannelSensorBot::TRACE_START) {
    char response[120];
    if (action == PublicChannelSensorBot::TRACE_CANCEL) {
      bool was_busy = traceBusy();
      if (was_busy) { closeTrace(); trace_report.cancel(); }
      trace_diagnostics.delivery = trace_report.outcome();
      StrHelper::strncpy(response, was_busy ? "Trace: cancelled" : "Trace: idle", sizeof(response));
    } else formatTraceStatus(response, sizeof(response));
    sendPublicResponse(response, context);
    return;
  }

  if (busy_trace) {
    if (!busy_trace_reported) {
      busy_trace_reported = true;
      if (discovery.pending()) {
        char status[96], response[128];
        formatTraceStatus(status, sizeof(status));
        snprintf(response, sizeof(response), "Trace: already running; %s", status + 7);
        sendPublicResponse(response, context);
      } else sendPublicResponse("Trace: already running", context);
    }
    return;
  }

  if (metric_mask == PublicChannelSensorBot::REQUEST_TRACE) {
    _targeted = targeted;
    if (targeted) {
      uint8_t key[PUB_KEY_SIZE]{};
      char destination[32], fallback[5];
      auto found = repeater_names.resolvePrefix(prefix, key, destination, sizeof(destination));
      if (found != RepeaterNameCache::UNIQUE) {
        char error[64];
        snprintf(error, sizeof(error), "Trace: %s repeater %02x%02x",
            found == RepeaterNameCache::UNKNOWN ? "unknown" : "ambiguous", prefix[0], prefix[1]);
        sendPublicResponse(error, context);
        return;
      }
      if (context.hash_size > 2) {
        sendPublicResponse("Trace: unsupported path hash width", context);
        return;
      }
      // Replies and discovery use the same captured scope; unresolved command
      // scope may have selected a usable default for this outbound transaction.
      snprintf(fallback, sizeof(fallback), "%02x%02x", prefix[0], prefix[1]);
      auto selected_control=_transport.controlContext(context);
      if(selected_control.hash_size<1 || selected_control.hash_size>2) {
        sendPublicResponse("Trace: unsupported path hash width",context); return;
      }
      trace_context=context; control_context=selected_control;
      trace_scope=mesh::PacketScope{};
      TransportKey preflight_scope{}; memcpy(preflight_scope.key,control_context.scope_key,16);
      if(!preflight_scope.isNull()) { trace_scope.kind=mesh::PacketScope::RESOLVED; memcpy(trace_scope.key,preflight_scope.key,16); }
      if (!beginAcknowledgement(_transport.now(), destination, fallback,key)) {
        sendPublicResponse("Trace: unsupported timing configuration", context); return;
      }
      trace_context = context;
      control_context = selected_control;
      trace_scope = mesh::PacketScope{};
      TransportKey selected{}; memcpy(selected.key, control_context.scope_key, sizeof(selected.key));
      if (!selected.isNull()) {
        trace_scope.kind = mesh::PacketScope::RESOLVED;
        memcpy(trace_scope.key, selected.key, sizeof(selected.key));
      }
      _trace_profile = _transport.radioProfile();
      if (_profile != _trace_profile) { trace_history.clear(); advert_routes.clear(); }
      _profile = _trace_profile;
      trace_diagnostics.begin(); trace_report.reset(); trace_routes.clear();
      busy_trace_reported = false; _settling = false;
      _advert_route=false;
      uint32_t reserve=_budget.probe_reserve;
      uint32_t available=workRemaining(_accepted_at);
      if(reserve+_budget.discovery_wait+TraceTimingPolicy::transmitWatchdog(_budget.control_air)>available)
        reserve=TraceTimingPolicy::attemptTimeout(1,_budget.probe_air)+5500;
      discovery.begin(key,_accepted_at,guest_policy.enabled(),_deadline-_budget.report_reserve-reserve,
          _budget.discovery_wait,_budget.login_wait,8000);
      pumpAcknowledgement(_transport.now());
      return;
    }
    uint32_t profile = _transport.radioProfile();
    if (profile != _profile) { trace_history.clear(); _profile = profile; }
    uint32_t tag, auth_code;
    traceIdentifiers(tag, auth_code);
    switch (trace_probe.start(packet, tag, auth_code, _transport.now())) {
      case RepeaterTraceProbe::STARTED: {
        trace_context=context; control_context=context;
        if (!beginAcknowledgement(_transport.now(), requester_name, "requester")) {
          trace_probe.cancel();
          sendPublicResponse("Trace: unsupported timing configuration", context); return;
        }
        busy_trace_reported = false;
        trace_context = context;
        trace_scope = scope;
        trace_diagnostics.begin();
        trace_report.reset();
        uint32_t now = _transport.now();
        trace_routes.begin(packet, now, scope);
        _settling = _settle_enabled;
        _settle_at = _next_queue_at = _probe_not_before = now;
        _trace_profile = _profile;
        _local_failures = 0;
        uint32_t airtime = cachedAirtime(packet->getRawLength());
        _settle_delay = TraceTimingPolicy::settleDelay(airtime, _transport.rng().nextInt(0, 251));
        _quiet = TraceTimingPolicy::quietInterval(airtime);
        pumpAcknowledgement(now);
        if (!_prelude && !_settling) admitTrace(now);
        return;
      }
      case RepeaterTraceProbe::BUSY:
        sendPublicResponse("Trace: already running", context);
        return;
      case RepeaterTraceProbe::NO_REPEATERS:
        sendPublicResponse("Trace: no repeaters in request path", context);
        return;
      case RepeaterTraceProbe::TOO_LONG:
        sendPublicResponse("Trace: path too long (max 10 repeaters)", context);
        return;
      case RepeaterTraceProbe::INVALID_PATH:
        sendPublicResponse("Trace: request path unavailable", context);
        return;
    }
  }

  char response[2 * PublicResponseQueue::MAX_PART_LENGTH + 1];
  char air_quality[48];
  char line[64];
  response[0] = 0;
  if (metric_mask == PublicChannelSensorBot::REQUEST_PING) {
    StrHelper::strncpy(response, "Pong", sizeof(response));
  } else if (metric_mask == PublicChannelSensorBot::REQUEST_PATH) {
    repeater_names.formatPath(packet, response, sizeof(response));
  } else if (metric_mask & PublicChannelSensorBot::METRIC_VOLTAGE && _telemetry.voltage > 0.0f) {
    snprintf(line, sizeof(line), "Voltage: %.2f V", _telemetry.voltage);
    appendResponseLine(response, sizeof(response), line);
  }

  const uint8_t bme_metrics = PublicChannelSensorBot::METRIC_ALL &
                              ~PublicChannelSensorBot::METRIC_VOLTAGE;
  if ((metric_mask & bme_metrics) && !_telemetry.ready()) {
    appendResponseLine(response, sizeof(response), "Sensor data is not ready");
  } else if (metric_mask != PublicChannelSensorBot::REQUEST_PING &&
             metric_mask != PublicChannelSensorBot::REQUEST_PATH && _telemetry.ready()) {
    formatAirQuality(air_quality, sizeof(air_quality), _telemetry.air_quality, _telemetry.samples());
    if (metric_mask & PublicChannelSensorBot::METRIC_TEMPERATURE) {
      snprintf(line, sizeof(line), "Temperature: %.1f C", _telemetry.temperature);
      appendResponseLine(response, sizeof(response), line);
    }
    if (metric_mask & PublicChannelSensorBot::METRIC_HUMIDITY) {
      snprintf(line, sizeof(line), "Humidity: %.1f%%", _telemetry.humidity);
      appendResponseLine(response, sizeof(response), line);
    }
    if (metric_mask & PublicChannelSensorBot::METRIC_PRESSURE) {
      snprintf(line, sizeof(line), "Pressure: %.1f hPa", _telemetry.pressure);
      appendResponseLine(response, sizeof(response), line);
    }
    if (metric_mask & PublicChannelSensorBot::METRIC_AIR_QUALITY)
      appendResponseLine(response, sizeof(response), air_quality);
  }
  if (response[0] == 0)
    StrHelper::strncpy(response, "Sensor data is not ready", sizeof(response));

  sendPublicResponse(response, context, 0,
                     metric_mask == PublicChannelSensorBot::REQUEST_PATH ? " > " : NULL);
}


void SensorBotService::pumpDiscovery(uint32_t now) {
  if(_prelude || expireTransaction(now) || !discovery.pending()) return;
  if(_trace_profile!=_transport.radioProfile()) { discoveryFailure("Trace: radio configuration changed"); return; }
  discovery.tick(now);
  if(discovery.phase()==TraceDiscovery::ADVERT_WAIT) {
    if(!useAdvertRoute(now)) discoveryFailure(discovery.failureText());
    return;
  }
  if(!discovery.ready(now)) return;
  uint32_t tag;
  trace_diagnostics.wall_clock_trusted=_transport.trustedWallTime();
  if(!request_clock.next(_transport.uniqueTime(),tag)) {
    trace_diagnostics.clock_healthy=false; discovery.clockFailed(); useAdvertRoute(now); return;
  }
  trace_diagnostics.request_timestamp=tag;
  now=_transport.now();
  if(expireTransaction(now)) return;
  discovery.tick(now);
  if(!discovery.ready(now)) { if(discovery.phase()==TraceDiscovery::ADVERT_WAIT) useAdvertRoute(now); return; }
  auto packet=discovery.login()?_transport.queueGuestLogin(discovery.key(),tag,control_context):
      _transport.queueDiscovery(discovery.key(),tag,control_context);
  if(!packet) {
    trace_diagnostics.allocations_failed++;
    discovery.rejected(now,_transport.rng().nextInt(0,251)); return;
  }
  now=_transport.now();
  auto watchdog=TraceTimingPolicy::transmitWatchdog(cachedAirtime(packet->getRawLength()));
  uint32_t reply=discovery.login()?_budget.login_wait:_budget.discovery_wait;
  if(uint64_t(watchdog)+reply>discovery.controlRemaining(now)) {
    cancelQueued(packet); discovery.budgetExhausted(); ++trace_diagnostics.budget_omissions; return;
  }
  uint8_t index=discovery.login()?(discovery.loginAttempt()==1?1:discovery.loginAttempt()+1):discovery.attempt()==1?0:2;
  _transport.tagTracePacket(packet,_trace_epoch,_deadline);
  _control_packets[index]=packet;
  trace_diagnostics.control[index].queued=now; trace_diagnostics.control[index].tag=tag;
  discovery.queued(packet,tag,discovery.remaining(now),watchdog);
}

void SensorBotService::discoveryFailure(const char* text) {
  closeTrace(true);
  trace_report.begin(text, _transport.nodeName(), trace_context, _transport.now(),
      workRemaining(_transport.now()) ? _transport.rng().nextInt(500, 2001) : 0,
      TraceTransactionBudget::remaining(_transport.now(), _deadline) < _budget.report_reserve ?
          TraceTransactionBudget::remaining(_transport.now(), _deadline) : _budget.report_reserve, true);
  pumpTraceReport(_transport.now(), true);
}

bool SensorBotService::onDiscoveryPath(const uint8_t* key, mesh::Packet* packet,
                  const uint8_t* path, uint8_t encoded, const uint8_t* extra, uint8_t length) {
  if (!extra || length < 4) return false;
  uint32_t tag; memcpy(&tag, extra, 4);
  ++trace_diagnostics.received_paths;
  if (!discovery.owns(key, tag)) { ++trace_diagnostics.unmatched_tags; return false; }
  if (expireTransaction(_transport.now()) || _prelude ||
      (_transaction && !workRemaining(_transport.now())) || !discovery.current(key, tag)) {
    ++trace_diagnostics.inactive_replies; return true;
  }
  if (_trace_profile != _transport.radioProfile()) { ++trace_diagnostics.profile_rejections; return true; }
  TransportKey scope{}; memcpy(scope.key, control_context.scope_key, sizeof(scope.key));
  if (scope.isNull() ? packet->hasTransportCodes() :
      !packet->hasTransportCodes() || scope.calcTransportCode(packet) != packet->transport_codes[0]) ++trace_diagnostics.scope_mismatches;
  TraceRoute route{};
  auto validity = TraceRoute::fromOutbound(path, encoded, key, route);
  if (validity != TraceRoute::VALID) {
    ++trace_diagnostics.route_rejections;
    if(discovery.searching()) {
      auto pending=discovery.queuedPacket(); discovery.abandon();
      if(pending) cancelQueued(pending);
    }
    return true;
  }
  uint32_t now = _transport.now();
  if (discovery.searching()) {
    auto pending = discovery.queuedPacket();
    if (pending) cancelQueued(pending);
    discovery.collect();
    startDiscovered(route, now);
  } else if (trace_probe.isActive() && !trace_probe.retryStarted()) {
    trace_routes.observeDiscovered(route, now, packet->_snr);
  }
  return true;
}

void SensorBotService::startDiscovered(const TraceRoute& route, uint32_t now) {
  uint64_t needed=TraceTimingPolicy::attemptTimeout(route.repeaters,cachedAirtime(route.maxPacketLength()))+
      (_settle_enabled?2500ULL:0);
  if(needed>workRemaining(now)) {
    TraceRoute alternate{};
    if(_targeted && advert_routes.best(discovery.key(),_transport.stableRadioProfile(),trace_scope,now,alternate,&route) &&
       TraceTimingPolicy::attemptTimeout(alternate.repeaters,cachedAirtime(alternate.maxPacketLength()))+
       (_settle_enabled?2500ULL:0)<=workRemaining(now)) {
      _advert_route=true; _attempt_advert[0]=true; startDiscovered(alternate,now);
    } else discoveryFailure("Trace: insufficient time for route");
    return;
  }
  uint32_t tag, auth; traceIdentifiers(tag, auth);
  if (trace_probe.startRoute(route, tag, auth, now) != RepeaterTraceProbe::STARTED) {
    discoveryFailure("Trace: discovered path unsupported");
    return;
  }
  trace_routes.beginDiscovered(route, now, trace_scope);
  _settling = _settle_enabled;
  _settle_at = _next_queue_at = _probe_not_before = now;
  _local_failures = 0;
  uint32_t airtime = cachedAirtime(route.maxPacketLength());
  _settle_delay = TraceTimingPolicy::settleDelay(airtime, _transport.rng().nextInt(0, 251));
  _quiet = TraceTimingPolicy::quietInterval(airtime);
  if (!_settling) admitTrace(now);
}

void SensorBotService::formatTraceStatus(char* response, size_t size) {
  uint32_t now = _transport.now();
  if (_prelude) {
    StrHelper::strncpy(response, "Trace: acknowledging request", size);
  } else if (discovery.login()) {
    snprintf(response,size,"Trace: public guest login %u/3 %s (%lu s left)",discovery.loginAttempt(),
        discovery.phase()==TraceDiscovery::LOGIN_QUEUE?"queued":
        discovery.phase()==TraceDiscovery::LOGIN_TX?"transmitting":"awaiting reply",
        (unsigned long)((discovery.remaining(now)+999)/1000));
  } else if (discovery.phase()==TraceDiscovery::ADVERT_WAIT) {
    StrHelper::strncpy(response,"Trace: waiting for advert route",size);
  } else if (discovery.pending()) {
    if (discovery.phase() == TraceDiscovery::FAILED) {
      StrHelper::strncpy(response, "Trace: discovery failed, reporting", size);
    } else {
      snprintf(response, size, "Trace: discovery %u/2%s (%lu s left)", discovery.attempt(),
          discovery.phase() == TraceDiscovery::QUEUE ? " queued" : "",
          (unsigned long)((discovery.remaining(now) + 999) / 1000));
    }
  } else if (_settling && trace_probe.isActive()) {
    StrHelper::strncpy(response, _advert_route?"Trace: settling advert route":"Trace: settling", size);
  } else if (trace_report.active()) {
    StrHelper::strncpy(response, trace_report.failure() ? "Trace: reporting failure" : "Trace: reporting result", size);
  } else if (trace_probe.isActive()) {
    snprintf(response, size, _advert_route?"Trace: advert probe %u/2%s (%lu s left)":"Trace: probe %u/2%s (%lu s left)", trace_probe.retryStarted() ? 2 : 1,
        trace_probe.isFinalizing() ? " finalizing" : trace_probe.waitingForQueue() ? " queued" : "",
        (unsigned long)((trace_probe.remaining(now) + 999) / 1000));
  } else StrHelper::strncpy(response, "Trace: idle", size);
  if (_transaction && traceBusy()) {
    size_t used = strlen(response);
    if (used < size) snprintf(response + used, size - used, "; %lu s overall left",
        (unsigned long)((TraceTransactionBudget::remaining(now, _deadline) + 999) / 1000));
  }
}
