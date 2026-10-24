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
  _transport.rng().random(reinterpret_cast<uint8_t*>(&next_trace_tag), sizeof(next_trace_tag));
  _profile = _transport.radioProfile();
}

void SensorBotService::loop() {
  uint32_t now = _transport.now();
  beforeRadioLoop();
  repeater_names.loop(now);
  pumpDiscovery(now);
  if (trace_probe.isActive() && _profile != _transport.radioProfile()) {
    trace_history.clear();
    _profile = _transport.radioProfile();
    // A route observed on the old radio configuration cannot be retried on the new one.
    if (trace_probe.isActive()) {
      auto packet = trace_probe.queuedPacket();
      if (packet) _transport.cancelQueuedPacket(packet);
      trace_probe.queueFailed(now);
      _settling = false;
    }
  }
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
  }
  if (trace_probe.retryPending()) {
    uint32_t tag, auth;
    traceIdentifiers(tag, auth);
    uint32_t airtimes[3]{};
    for (uint8_t i = 0; i < trace_routes.count(); i++)
      airtimes[i] = _transport.estimateAirtime(trace_routes.candidate(i).maxPacketLength());
    auto alternate = trace_routes.best(trace_history, trace_context.channel, _trace_profile, now, airtimes, trace_scope, historyTarget());
    if (alternate && trace_history.rank(trace_probe.route(), trace_context.channel, _trace_profile, now, trace_scope, historyTarget()) >
                     trace_history.rank(*alternate, trace_context.channel, _trace_profile, now, trace_scope, historyTarget())) alternate = NULL;
    if (!trace_probe.beginRetry(tag, auth, now, alternate)) trace_probe.queueFailed(now);
    else {
      _local_failures = 0;
      _next_queue_at = now;
      admitTrace(now, _transport.rng().nextInt(1000, 2001));
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
  char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
  SensorReplyContext context;
  if (public_replies.takeDue(now, part, &context) &&
      !sendPublicPart(part, context, 0))
    public_replies.schedule(part, now + 1000, context);
}

void SensorBotService::beforeRadioLoop() {
  // Expire queued work before MeshCore can start it on a later loop iteration.
  // In-flight packets remain exclusively owned by the dispatcher watchdog.
  uint32_t now = _transport.now();
  if (_targeted && (discovery.active() || trace_probe.isActive()) &&
      _trace_profile != _transport.radioProfile()) {
    trace_history.clear(); _profile = _transport.radioProfile();
    discoveryFailure("Trace: radio configuration changed");
  }
  if (discovery.queueExpired(now)) {
    auto packet = discovery.queuedPacket();
    if (packet) _transport.cancelQueuedPacket(packet);
    discovery.tick(now);
  }
  if (!_settling && trace_probe.isQueueTimedOut(now)) {
    auto packet = trace_probe.queuedPacket();
    if (packet) _transport.cancelQueuedPacket(packet);
    trace_diagnostics.queue_failures++;
    trace_probe.queueFailed(now);
  }
  if (trace_probe.failurePending() && trace_probe.reportExpired(now)) {
    closeTrace();
    trace_report.expire();
  }
  if (trace_report.expired(now)) {
    auto packet = (mesh::Packet*)trace_report.queuedPacket();
    trace_report.expire();
    if (packet) _transport.cancelQueuedPacket(packet);
    trace_probe.cancel();
    trace_routes.clear();
  }
  trace_diagnostics.delivery = trace_report.outcome();
}

void SensorBotService::admitTrace(uint32_t now, uint32_t delay) {
  if (delay) _probe_not_before = now + delay;
  if (!trace_probe.waitingForQueue() || trace_probe.queuedPacket() ||
      (int32_t)(now - _next_queue_at) < 0) return;
  uint32_t remaining = (int32_t)(_probe_not_before - now) > 0 ? _probe_not_before - now : 0;
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
      trace_report.queued(packet);
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
  trace_probe.setAirtimeEstimate(_transport.estimateAirtime(trace_probe.route().maxPacketLength()));
  auto trace = _transport.createTrace(trace_probe.tag(), trace_probe.authCode(), trace_probe.flags());
  if (!trace) return TRACE_NO_PACKET;
  trace_probe.setQueuedPacket(trace);
  _transport.sendDirect(trace, trace_probe.path(), trace_probe.pathBytes(), delay_millis);
  return _transport.isQueuedPacket(trace) ? TRACE_QUEUED : TRACE_QUEUE_REJECTED;
}

void SensorBotService::observeRoute(const mesh::Packet* packet) {
  if (!_targeted && packet && trace_probe.isActive() && !trace_probe.retryStarted() &&
      packet->isRouteFlood() && packet->getPayloadType() == PAYLOAD_TYPE_GRP_TXT) {
    trace_routes.observe(packet, _transport.now(), _transport.requestScope(*packet));
  }
  trace_diagnostics.observe(packet, trace_probe, _transport.now());
}

void SensorBotService::onTxStarted(mesh::Packet* packet, uint32_t now_millis) {
  discovery.started(packet, now_millis);
  if (trace_report.started(packet) && trace_report.failure()) {
    // Failure is committed once it goes on air; never send a contradictory result.
    trace_probe.cancel();
    trace_routes.clear();
  }
  if (trace_probe.onTxStarted(packet, now_millis)) {
    trace_diagnostics.starts++;
    trace_diagnostics.probe_submissions++;
  }
}

void SensorBotService::logTx(mesh::Packet* packet, int) {
  discovery.completed(packet);
  trace_probe.onTxComplete(packet);
  trace_report.completed(packet, _transport.now());
  trace_diagnostics.delivery = trace_report.outcome();
  public_replies.onFirstSent(packet, _transport.now());
}

void SensorBotService::logTxFail(mesh::Packet* packet, int) {
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

void SensorBotService::closeTrace() {
  auto discovery_packet = discovery.queuedPacket();
  discovery.cancel();
  if (discovery_packet) _transport.cancelQueuedPacket(discovery_packet);
  auto pending = trace_probe.queuedPacket();
  auto failure = (mesh::Packet*)trace_report.queuedPacket();
  trace_report.reset();
  _settling = false;
  trace_probe.cancel();
  trace_routes.clear();
  if (pending) _transport.cancelQueuedPacket(pending);
  if (failure) _transport.cancelQueuedPacket(failure);
}

void SensorBotService::finishTrace(const RepeaterTraceProbe::Result& result) {
  closeTrace();
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
      _transport.rng().nextInt(500, 2001), TraceTimingPolicy::REPORT_TIMEOUT_MILLIS, true);
  pumpTraceReport(_transport.now(), true);
}

mesh::Packet* SensorBotService::createPublicPart(const char* response, const SensorReplyContext& context) {
  uint8_t payload[MAX_PACKET_PAYLOAD];
  uint32_t timestamp = _transport.uniqueTime();
  memcpy(payload, &timestamp, sizeof(timestamp));
  payload[4] = 0;
  int prefix_len = snprintf(reinterpret_cast<char*>(&payload[5]), sizeof(payload) - 5,
                            "%s: ", _transport.nodeName());
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

void SensorBotService::onAdvertRecv(mesh::Packet*, const mesh::Identity& id, uint32_t timestamp,
                  const uint8_t* app_data, size_t app_data_len) {
  repeater_names.onAdvert(id, timestamp, app_data, app_data_len, _transport.now());
}

void SensorBotService::onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                                  const uint8_t* path_snrs, const uint8_t* path_hashes, uint8_t path_len) {
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
      TraceTimingPolicy::reportBudget(_transport.estimateAirtime(MAX_TRANS_UNIT)));
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
  int channel_index = public_bot.channelIndex(channel);
  if (channel_index < 0) return;
  if (!public_bot.accept(type, data, len, _transport.now(), traceBusy(),
                         metric_mask, busy_trace, channel_index, prefix, &targeted)) return;

  const auto scope = _transport.requestScope(*packet);
  const auto context = _transport.replyContext(*packet, channel_index, scope);

  if (busy_trace) {
    if (!busy_trace_reported) {
      busy_trace_reported = true;
      sendPublicResponse("Trace: already running", context);
    }
    return;
  }

  if (metric_mask == PublicChannelSensorBot::REQUEST_TRACE) {
    _targeted = targeted;
    if (targeted) {
      uint8_t key[PUB_KEY_SIZE]{};
      auto found = repeater_names.resolvePrefix(prefix, key);
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
      trace_context = context;
      trace_scope = mesh::PacketScope{};
      TransportKey selected{}; memcpy(selected.key, context.scope_key, sizeof(selected.key));
      if (!selected.isNull()) {
        trace_scope.kind = mesh::PacketScope::RESOLVED;
        memcpy(trace_scope.key, selected.key, sizeof(selected.key));
      }
      _trace_profile = _transport.radioProfile();
      if (_profile != _trace_profile) trace_history.clear();
      _profile = _trace_profile;
      trace_diagnostics.begin(); trace_report.reset(); trace_routes.clear();
      busy_trace_reported = false; _settling = false;
      discovery.begin(key, _transport.now());
      pumpDiscovery(_transport.now());
      return;
    }
    uint32_t profile = _transport.radioProfile();
    if (profile != _profile) { trace_history.clear(); _profile = profile; }
    uint32_t tag, auth_code;
    traceIdentifiers(tag, auth_code);
    switch (trace_probe.start(packet, tag, auth_code, _transport.now())) {
      case RepeaterTraceProbe::STARTED: {
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
        uint32_t airtime = _transport.estimateAirtime(packet->getRawLength());
        _settle_delay = TraceTimingPolicy::settleDelay(airtime, _transport.rng().nextInt(0, 251));
        _quiet = TraceTimingPolicy::quietInterval(airtime);
        if (!_settling) admitTrace(now);
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
  if (!discovery.active() && discovery.phase() != TraceDiscovery::FAILED) return;
  if (_trace_profile != _transport.radioProfile()) {
    trace_history.clear(); _profile = _transport.radioProfile();
    discoveryFailure("Trace: radio configuration changed");
    return;
  }
  discovery.tick(now);
  if (discovery.phase() == TraceDiscovery::FAILED) {
    discoveryFailure(discovery.failureText());
    return;
  }
  if (!discovery.ready(now)) return;
  uint32_t tag = _transport.uniqueTime();  // authenticated request replay clock
  auto packet = _transport.queueDiscovery(discovery.key(), tag, trace_context);
  if (!packet) {
    trace_diagnostics.allocations_failed++;
    discovery.rejected(now, _transport.rng().nextInt(0, 251));
    return;
  }
  discovery.queued(packet, tag, TraceTimingPolicy::attemptTimeout(TraceRoute::MAX_REPEATERS,
                         _transport.estimateAirtime(MAX_TRANS_UNIT)));
}

void SensorBotService::discoveryFailure(const char* text) {
  closeTrace();
  trace_report.begin(text, _transport.nodeName(), trace_context, _transport.now(),
      _transport.rng().nextInt(500, 2001), TraceTimingPolicy::REPORT_TIMEOUT_MILLIS, true);
  pumpTraceReport(_transport.now(), true);
}

bool SensorBotService::onDiscoveryPath(const uint8_t* key, mesh::Packet* packet,
                  const uint8_t* path, uint8_t encoded, const uint8_t* extra, uint8_t length) {
  if (!extra || length < 4) return false;
  uint32_t tag; memcpy(&tag, extra, 4);
  if (!discovery.owns(key, tag)) return false;
  if (!discovery.current(key, tag)) return true;
  if (_trace_profile != _transport.radioProfile()) return true;
  TransportKey scope{}; memcpy(scope.key, trace_context.scope_key, sizeof(scope.key));
  if (scope.isNull() ? packet->hasTransportCodes() :
      !packet->hasTransportCodes() || scope.calcTransportCode(packet) != packet->transport_codes[0]) return true;
  TraceRoute route{};
  auto validity = TraceRoute::fromOutbound(path, encoded, key, route);
  if (validity != TraceRoute::VALID) {
    if (discovery.active()) discoveryFailure(validity == TraceRoute::TOO_LONG ?
        "Trace: path too long (max 10 repeaters)" : "Trace: discovered path unsupported");
    return true;
  }
  uint32_t now = _transport.now();
  if (discovery.active()) {
    auto pending = discovery.queuedPacket();
    if (pending) _transport.cancelQueuedPacket(pending);
    discovery.collect();
    startDiscovered(route, now);
  } else if (trace_probe.isActive() && !trace_probe.retryStarted()) {
    trace_routes.observeDiscovered(route, now, packet->_snr);
  }
  return true;
}

void SensorBotService::startDiscovered(const TraceRoute& route, uint32_t now) {
  uint32_t tag, auth; traceIdentifiers(tag, auth);
  if (trace_probe.startRoute(route, tag, auth, now) != RepeaterTraceProbe::STARTED) {
    discoveryFailure("Trace: discovered path unsupported");
    return;
  }
  trace_routes.beginDiscovered(route, now, trace_scope);
  _settling = _settle_enabled;
  _settle_at = _next_queue_at = _probe_not_before = now;
  _local_failures = 0;
  uint32_t airtime = _transport.estimateAirtime(route.maxPacketLength());
  _settle_delay = TraceTimingPolicy::settleDelay(airtime, _transport.rng().nextInt(0, 251));
  _quiet = TraceTimingPolicy::quietInterval(airtime);
  if (!_settling) admitTrace(now);
}
