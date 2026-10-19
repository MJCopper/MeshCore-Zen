#include "SensorBotService.h"

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
}

void SensorBotService::loop() {
  uint32_t now = _transport.now();
  repeater_names.loop(now);
  if (trace_probe.isQueueTimedOut(now)) {
    _transport.cancelQueuedPacket(trace_probe.queuedPacket());
    trace_probe.queueFailed(now);
  }
  if (trace_probe.isTimedOut(now) && trace_probe.retryPending()) {
    uint32_t tag, auth;
    traceIdentifiers(tag, auth);
    if (!trace_probe.beginRetry(tag, auth, now, trace_routes.alternate()) ||
        !queueTrace(_transport.rng().nextInt(1000, 2001)))
      trace_probe.queueFailed(now);
  }
  if (trace_probe.isFinalizing()) {
    RepeaterTraceProbe::Result result;
    if (trace_probe.takeFallback(result)) finishTrace(result);
    else trace_probe.advanceFinal(now);
  }
  if (trace_probe.failurePending()) {
    if (trace_probe.reportExpired(now)) closeTrace();
    else if (!trace_failure_packet) queueTraceFailure();
  }
  char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
  uint8_t channel_index;
  if (public_replies.takeDue(now, part, &channel_index) &&
      !sendPublicPart(part, channel_index, 0))
    public_replies.schedule(part, now + 1000, channel_index);
}

void SensorBotService::traceIdentifiers(uint32_t& tag, uint32_t& auth) {
  // Monotonic tags prevent a stale attempt from matching a later transaction.
  tag = ++next_trace_tag;
  _transport.rng().random(reinterpret_cast<uint8_t*>(&auth), sizeof(auth));
}

bool SensorBotService::queueTrace(uint32_t delay_millis) {
  trace_probe.setAirtimeEstimate(_transport.estimateAirtime(trace_probe.route().maxPacketLength()));
  auto trace = _transport.createTrace(trace_probe.tag(), trace_probe.authCode(), trace_probe.flags());
  if (!trace) return false;
  trace_probe.setQueuedPacket(trace);
  _transport.sendDirect(trace, trace_probe.path(), trace_probe.pathBytes(), delay_millis);
  return _transport.isQueuedPacket(trace);
}

void SensorBotService::observeRoute(const mesh::Packet* packet) {
  if (trace_probe.isActive() && !trace_probe.retryStarted()) trace_routes.observe(packet);
}

void SensorBotService::onTxStarted(mesh::Packet* packet, uint32_t now_millis) {
  if (packet == trace_failure_packet) {
    // Failure is committed once it goes on air; never send a contradictory result.
    trace_failure_packet = NULL;
    trace_probe.cancel();
    trace_routes.clear();
  }
  trace_probe.onTxStarted(packet, now_millis);
}

void SensorBotService::logTx(mesh::Packet* packet, int) {
  trace_probe.onTxComplete(packet);
  public_replies.onFirstSent(packet, _transport.now());
}

void SensorBotService::logTxFail(mesh::Packet* packet, int) {
  if (packet == trace_failure_packet) trace_failure_packet = NULL;
  trace_probe.onTxFailed(packet, _transport.now());
  public_replies.onFirstFailed(packet);
}

void SensorBotService::closeTrace() {
  auto pending = trace_probe.queuedPacket();
  auto failure = trace_failure_packet;
  trace_failure_packet = NULL;
  trace_probe.cancel();
  trace_routes.clear();
  if (pending) _transport.cancelQueuedPacket(pending);
  if (failure) _transport.cancelQueuedPacket(failure);
}

void SensorBotService::finishTrace(const RepeaterTraceProbe::Result& result) {
  closeTrace();
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
  auto packet = createPublicPart(text, trace_channel);
  if (!packet) return;
  trace_failure_packet = packet;
  _transport.sendFlood(packet, _transport.rng().nextInt(500, 2001));
  if (!_transport.isQueuedPacket(packet)) trace_failure_packet = NULL;
}

mesh::Packet* SensorBotService::createPublicPart(const char* response, uint8_t channel_index) {
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

  return _transport.createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, public_bot.channel(channel_index), payload,
                             5 + prefix_len + response_len);
}

bool SensorBotService::sendPublicPart(const char* response, uint8_t channel_index, uint32_t delay_millis) {
  auto reply_packet = createPublicPart(response, channel_index);
  if (!reply_packet) return false;
  _transport.sendFlood(reply_packet, delay_millis);
  return _transport.isQueuedPacket(reply_packet);
}

void SensorBotService::sendPublicResponse(const char* response, uint8_t channel_index,
                                          uint32_t extra_delay_millis) {
  size_t prefix_len = strlen(_transport.nodeName()) + 2;
  if (prefix_len >= PublicResponseQueue::MAX_PART_LENGTH) return;
  char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
  char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
  bool multipart = PublicResponseQueue::split(response,
                                               PublicResponseQueue::MAX_PART_LENGTH - prefix_len,
                                               first, second);
  uint32_t first_delay = extra_delay_millis + _transport.rng().nextInt(500, 2001);
  auto first_packet = createPublicPart(first, channel_index);
  if (!first_packet) return;
  if (multipart && !public_replies.scheduleAfterFirst(second, first_packet, channel_index)) {
    _transport.releasePacket(first_packet);
    sendPublicPart("Reply queue busy", channel_index, first_delay);
    return;
  }
  _transport.sendFlood(first_packet, first_delay);
  if (!_transport.isQueuedPacket(first_packet)) public_replies.onFirstFailed(first_packet);
}

void SensorBotService::onAdvertRecv(mesh::Packet*, const mesh::Identity& id, uint32_t timestamp,
                  const uint8_t* app_data, size_t app_data_len) {
  repeater_names.onAdvert(id, timestamp, app_data, app_data_len, _transport.now());
}

void SensorBotService::onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                                  const uint8_t* path_snrs, const uint8_t* path_hashes, uint8_t path_len) {
  RepeaterTraceProbe::Result result;
  if (trace_probe.receive(packet, tag, auth_code, flags, path_snrs, path_hashes, path_len,
                           _transport.now(), result) == RepeaterTraceProbe::COMPLETE)
    finishTrace(result);
}

void SensorBotService::sendTraceResult(const RepeaterTraceProbe::Result& result) {
  char response[2 * PublicResponseQueue::MAX_PART_LENGTH + 1];
  snprintf(response, sizeof(response), "Trace: %lu ms RTT", (unsigned long)result.elapsed_millis);
  uint8_t hash_size = 1 << result.route.flags;
  for (uint8_t i = 0; i < result.route.repeaters; i++) {
    char label[13];
    char line[40];
    repeater_names.formatLabel(&result.route.path[i * hash_size], hash_size, label, sizeof(label));
    snprintf(line, sizeof(line), "%u %s %+.1f dB", i + 1, label, (float)(int8_t)result.snrs[i] / 4.0f);
    appendResponseLine(response, sizeof(response), line);
  }
  sendPublicResponse(response, trace_channel, TRACE_REPLY_DELAY_MILLIS);
}

int SensorBotService::searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) {
  return public_bot.findChannel(hash, channels, max_matches);
}

void SensorBotService::onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel,
                     uint8_t* data, size_t len) {
  uint8_t metric_mask;
  bool busy_trace;
  int channel_index = public_bot.channelIndex(channel);
  if (channel_index < 0) return;
  if (!public_bot.accept(type, data, len, _transport.now(), trace_probe.isActive(),
                         metric_mask, busy_trace, channel_index)) return;

  if (busy_trace) {
    if (!busy_trace_reported) {
      busy_trace_reported = true;
      sendPublicResponse("Trace: already running", channel_index);
    }
    return;
  }

  if (metric_mask == PublicChannelSensorBot::REQUEST_TRACE) {
    uint32_t tag, auth_code;
    traceIdentifiers(tag, auth_code);
    switch (trace_probe.start(packet, tag, auth_code, _transport.now())) {
      case RepeaterTraceProbe::STARTED: {
        busy_trace_reported = false;
        trace_channel = channel_index;
        trace_routes.begin(packet);
        if (!queueTrace()) trace_probe.queueFailed(_transport.now());
        return;
      }
      case RepeaterTraceProbe::BUSY:
        sendPublicResponse("Trace: already running", channel_index);
        return;
      case RepeaterTraceProbe::NO_REPEATERS:
        sendPublicResponse("Trace: no repeaters in request path", channel_index);
        return;
      case RepeaterTraceProbe::TOO_LONG:
        sendPublicResponse("Trace: path too long (max 10 repeaters)", channel_index);
        return;
      case RepeaterTraceProbe::INVALID_PATH:
        sendPublicResponse("Trace: request path unavailable", channel_index);
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
    // Leave room for the node name and group-message framing.
    repeater_names.formatPath(packet, response, 120);
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

  sendPublicResponse(response, channel_index);
}

