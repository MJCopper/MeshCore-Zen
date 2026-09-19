#include "SensorMesh.h"

#ifdef PUBLIC_CHANNEL_SENSOR_BOT
  #include "PublicChannelSensorBot.h"
  #include "PublicResponseQueue.h"
  #include "RepeaterNameCache.h"
  #include "RepeaterTraceProbe.h"
#endif

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  static UITask ui_task(display);
#endif

#ifdef PUBLIC_CHANNEL_SENSOR_BOT
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
#endif

class MyMesh : public SensorMesh {
public:
  MyMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables)
     : SensorMesh(board, radio, ms, rng, rtc, tables), 
       battery_data(12*24, 5*60)    // 24 hours worth of battery data, every 5 minutes
  {
  }

#ifdef PUBLIC_CHANNEL_SENSOR_BOT
  void begin(FILESYSTEM* fs) {
    SensorMesh::begin(fs);
    repeater_names.begin(fs);
  }

  void loop() {
    SensorMesh::loop();
    repeater_names.loop(millis());
    if (trace_probe.isTimedOut(millis())) sendPublicResponse("Trace: route timed out");
    char part[PublicResponseQueue::MAX_PART_LENGTH + 1];
    if (public_replies.takeDue(millis(), part)) sendPublicPart(part, 0);
  }
#endif

protected:
  /* ========================== custom logic here ========================== */
  Trigger low_batt, critical_batt;
  TimeSeriesData  battery_data;
#ifdef PUBLIC_CHANNEL_SENSOR_BOT
  PublicChannelSensorBot public_bot;
  PublicResponseQueue public_replies;
  RepeaterNameCache repeater_names;
  RepeaterTraceProbe trace_probe;
  float bme_temperature = NAN;
  float bme_humidity = NAN;
  float bme_pressure = NAN;
  float bme_air_quality = NAN;
  float cached_voltage = NAN;
  bool bme_data_ready = false;
  uint8_t bme_sample_count = 0;
#endif

  void onSensorDataRead() override {
    float batt_voltage = getVoltage(TELEM_CHANNEL_SELF);

    battery_data.recordData(getRTCClock(), batt_voltage);   // record battery
    alertIf(batt_voltage < 3.4f, critical_batt, HIGH_PRI_ALERT, "Battery is critical!");
    alertIf(batt_voltage < 3.6f, low_batt, LOW_PRI_ALERT, "Battery is low");
#ifdef PUBLIC_CHANNEL_SENSOR_BOT
    cached_voltage = batt_voltage;
    // This build enables only the BME680, so it occupies telemetry channel 2.
    bme_temperature = getTemperature(2);
    bme_humidity = getRelativeHumidity(2);
    bme_pressure = getBarometricPressure(2);
    bme_air_quality = getTelemValue(2, LPP_GENERIC_SENSOR);
    // getTelemValue() returns zero for a missing field. Pressure is strictly
    // positive for a successful BME680 reading, while AQ zero is valid.
    bme_data_ready = bme_pressure > 0.0f;
    if (bme_data_ready && bme_sample_count < 10) bme_sample_count++;
#endif
  }

  int querySeriesData(uint32_t start_secs_ago, uint32_t end_secs_ago, MinMaxAvg dest[], int max_num) override {
    battery_data.calcMinMaxAvg(getRTCClock(), start_secs_ago, end_secs_ago, &dest[0], TELEM_CHANNEL_SELF, LPP_VOLTAGE);
    return 1;
  }

  bool handleCustomCommand(uint32_t sender_timestamp, char* command, char* reply) override {
#ifdef PUBLIC_CHANNEL_SENSOR_BOT
    if (strcmp(command, "get repeat") == 0) {
      strcpy(reply, "> off");
      return true;
    }
    if (strncmp(command, "set repeat ", 11) == 0) {
      strcpy(reply, "OK - repeat is fixed OFF");
      return true;
    }
#endif
    if (strcmp(command, "magic") == 0) {    // example 'custom' command handling
      strcpy(reply, "**Magic now done**");
      return true;   // handled
    }
    return false;  // not handled
  }

#ifdef PUBLIC_CHANNEL_SENSOR_BOT
  bool allowPacketForward(const mesh::Packet*) override { return false; }

  bool sendPublicPart(const char* response, uint32_t delay_millis) {
    uint8_t payload[MAX_PACKET_PAYLOAD];
    uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
    memcpy(payload, &timestamp, sizeof(timestamp));
    payload[4] = 0;
    int prefix_len = snprintf(reinterpret_cast<char*>(&payload[5]), sizeof(payload) - 5,
                              "%s: ", getNodeName());
    if (prefix_len < 0 || prefix_len >= (int)(sizeof(payload) - 5)) return false;
    size_t response_len = strlen(response);
    if (prefix_len + response_len > PublicResponseQueue::MAX_PART_LENGTH) return false;
    memcpy(&payload[5 + prefix_len], response, response_len);

    auto reply_packet = createGroupDatagram(PAYLOAD_TYPE_GRP_TXT, public_bot.channel(), payload,
                                            5 + prefix_len + response_len);
    if (!reply_packet) return false;
    sendFlood(reply_packet, delay_millis);
    return true;
  }

  void sendPublicResponse(const char* response) {
    size_t prefix_len = strlen(getNodeName()) + 2;
    if (prefix_len >= PublicResponseQueue::MAX_PART_LENGTH) return;
    char first[PublicResponseQueue::MAX_PART_LENGTH + 1];
    char second[PublicResponseQueue::MAX_PART_LENGTH + 1];
    bool multipart = PublicResponseQueue::split(response,
                                                 PublicResponseQueue::MAX_PART_LENGTH - prefix_len,
                                                 first, second);
    uint32_t first_delay = getRNG()->nextInt(500, 2001);
    if (!sendPublicPart(first, first_delay)) return;
    if (multipart) public_replies.schedule(second, millis() + first_delay + 3000);
  }

  void onAdvertRecv(mesh::Packet*, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len) override {
    repeater_names.onAdvert(id, timestamp, app_data, app_data_len, millis());
  }

  void onTraceRecv(mesh::Packet* packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t* path_snrs, const uint8_t* path_hashes, uint8_t path_len) override {
    uint32_t elapsed_millis;
    uint8_t repeater_count;
    if (!trace_probe.complete(packet, tag, auth_code, flags, path_hashes, path_len,
                              millis(), elapsed_millis, repeater_count)) return;

    char response[2 * PublicResponseQueue::MAX_PART_LENGTH + 1];
    snprintf(response, sizeof(response), "Trace: %lu ms RTT", (unsigned long)elapsed_millis);
    uint8_t hash_size = 1 << (flags & 0x03);
    for (uint8_t i = 0; i < repeater_count; i++) {
      char label[13];
      char line[40];
      repeater_names.formatLabel(&path_hashes[i * hash_size], hash_size,
                                 label, sizeof(label));
      snprintf(line, sizeof(line), "%u %s %+.1f dB", i + 1, label,
               (float)(int8_t)path_snrs[i] / 4.0f);
      appendResponseLine(response, sizeof(response), line);
    }
    sendPublicResponse(response);
  }

  int searchChannelsByHash(const uint8_t* hash, mesh::GroupChannel channels[], int max_matches) override {
    return public_bot.findChannel(hash, channels, max_matches);
  }

  void onGroupDataRecv(mesh::Packet* packet, uint8_t type, const mesh::GroupChannel&,
                       uint8_t* data, size_t len) override {
    uint8_t metric_mask;
    if (!public_bot.accept(type, data, len, millis(), metric_mask)) return;

    if (metric_mask == PublicChannelSensorBot::REQUEST_TRACE) {
      uint32_t tag, auth_code;
      getRNG()->random(reinterpret_cast<uint8_t*>(&tag), sizeof(tag));
      getRNG()->random(reinterpret_cast<uint8_t*>(&auth_code), sizeof(auth_code));
      switch (trace_probe.start(packet, tag, auth_code, millis())) {
        case RepeaterTraceProbe::STARTED: {
          auto trace = createTrace(tag, auth_code, trace_probe.flags());
          if (trace) {
            sendDirect(trace, trace_probe.path(), trace_probe.pathBytes());
            return;  // Reply when the trace returns or times out.
          }
          trace_probe.cancel();
          sendPublicResponse("Trace: unable to start");
          return;
        }
        case RepeaterTraceProbe::BUSY:
          sendPublicResponse("Trace: already running");
          return;
        case RepeaterTraceProbe::NO_REPEATERS:
          sendPublicResponse("Trace: no repeaters in request path");
          return;
        case RepeaterTraceProbe::TOO_LONG:
          sendPublicResponse("Trace: path too long (max 4 repeaters)");
          return;
        case RepeaterTraceProbe::INVALID_PATH:
          sendPublicResponse("Trace: request path unavailable");
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
    } else if (metric_mask & PublicChannelSensorBot::METRIC_VOLTAGE && cached_voltage > 0.0f) {
      snprintf(line, sizeof(line), "Voltage: %.2f V", cached_voltage);
      appendResponseLine(response, sizeof(response), line);
    }

    const uint8_t bme_metrics = PublicChannelSensorBot::METRIC_ALL &
                                ~PublicChannelSensorBot::METRIC_VOLTAGE;
    if ((metric_mask & bme_metrics) && !bme_data_ready) {
      appendResponseLine(response, sizeof(response), "Sensor data is not ready");
    } else if (metric_mask != PublicChannelSensorBot::REQUEST_PING &&
               metric_mask != PublicChannelSensorBot::REQUEST_PATH && bme_data_ready) {
      formatAirQuality(air_quality, sizeof(air_quality), bme_air_quality, bme_sample_count);
      if (metric_mask & PublicChannelSensorBot::METRIC_TEMPERATURE) {
        snprintf(line, sizeof(line), "Temperature: %.1f C", bme_temperature);
        appendResponseLine(response, sizeof(response), line);
      }
      if (metric_mask & PublicChannelSensorBot::METRIC_HUMIDITY) {
        snprintf(line, sizeof(line), "Humidity: %.1f%%", bme_humidity);
        appendResponseLine(response, sizeof(response), line);
      }
      if (metric_mask & PublicChannelSensorBot::METRIC_PRESSURE) {
        snprintf(line, sizeof(line), "Pressure: %.1f hPa", bme_pressure);
        appendResponseLine(response, sizeof(response), line);
      }
      if (metric_mask & PublicChannelSensorBot::METRIC_AIR_QUALITY)
        appendResponseLine(response, sizeof(response), air_quality);
    }
    if (response[0] == 0)
      StrHelper::strncpy(response, "Sensor data is not ready", sizeof(response));

    sendPublicResponse(response);
  }
#endif
  /* ======================================================================= */
};

StdRNG fast_rng;
SimpleMeshTables tables;

MyMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static char command[160];

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#ifdef DISPLAY_CLASS
  if (display.begin()) {
    display.startFrame();
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) { halt(); }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  InternalFS.begin();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  SPIFFS.begin(true);
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Sensor ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();

  command[0] = 0;

  sensors.begin();

  the_mesh.begin(fs);

#ifdef DISPLAY_CLASS
  ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif
}

void loop() {
  int len = strlen(command);
  while (Serial.available() && len < sizeof(command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      command[len++] = c;
      command[len] = 0;
    }
    Serial.print(c);
  }
  if (len == sizeof(command)-1) {  // command buffer full
    command[sizeof(command)-1] = '\r';
  }

  if (len > 0 && command[len - 1] == '\r') {  // received complete line
    command[len - 1] = 0;  // replace newline with C string null terminator
    char reply[160];
    the_mesh.handleCommand(0, command, reply);  // NOTE: there is no sender_timestamp via serial!
    if (reply[0]) {
      Serial.print("  -> "); Serial.println(reply);
    }

    command[0] = 0;  // reset command buffer
  }

  the_mesh.loop();
  sensors.loop();
#ifdef DISPLAY_CLASS
  ui_task.loop();
#endif
  rtc_clock.tick();
#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif
}
