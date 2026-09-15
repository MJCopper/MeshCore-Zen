#pragma once

#include <Mesh.h>
#include <helpers/SensorManager.h>
#include <helpers/sensors/LocationProvider.h>
#include <helpers/sensors/GpsAdaptivePolicy.h>

class EnvironmentSensorManager : public SensorManager {
protected:
  static const int MAX_ACTIVE_SENSORS = 16;

  // Query function pointer + sub-channel index (for multi-channel sensors like INA3221).
  // Sub-channel is 0 for all single-output sensors.
  struct ActiveSensor {
    void    (*query)(uint8_t channel, uint8_t sub_channel, CayenneLPP& telemetry);
    uint8_t   sub_channel;
  };

  ActiveSensor _active_sensors[MAX_ACTIVE_SENSORS];
  int          _active_sensor_count = 0;
  uint8_t      next_available_channel = TELEM_CHANNEL_SELF + 1;

  bool     gps_detected = false;
  bool     gps_configured = false;
  bool     gps_active = false;
  bool     gps_adaptive = false;
  bool     gps_force_active = false;
  uint32_t gps_update_interval_sec = 0;  // 0 = continuous; non-zero = fix cadence
  uint32_t gps_next_acquire_ms = 0;
  uint32_t gps_acquire_deadline_ms = 0;
  uint32_t gps_fix_stable_since_ms = 0;
  bool     gps_movement_seen = false;
  uint32_t gps_movement_since_ms = 0;
  uint8_t  gps_consecutive_failures = 0;
  uint32_t gps_next_cache_ms = 0;
  GpsAdaptivePolicy gps_adaptive_policy;

  #if ENV_INCLUDE_GPS
  LocationProvider* _location;
  void start_gps();
  void stop_gps();
  void start_periodic_gps();
  void initBasicGPS();
  #ifdef RAK_BOARD
  void rakGPSInit();
  bool gpsIsAwake(uint8_t ioPin);
  #endif
  #endif

public:
  #if ENV_INCLUDE_GPS
  EnvironmentSensorManager(LocationProvider &location): _location(&location){};
  LocationProvider* getLocationProvider() { return _location; }
  #else
  EnvironmentSensorManager(){};
  #endif
  bool begin() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
  #if ENV_INCLUDE_GPS || defined(ENV_INCLUDE_BME680_BSEC)
  void loop() override;
  #endif
  int getNumSettings() const override;
  const char* getSettingName(int i) const override;
  const char* getSettingValue(int i) const override;
  bool setSettingValue(const char* name, const char* value) override;
  void onUserDisplayWake() override;
  bool getGpsAdaptiveRetry(uint32_t& remaining_ms) const override;
};
