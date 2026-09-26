#pragma once

#include <stdint.h>
#include <limits.h>

namespace zen {

// Zen's location-policy seam. The MeshCore SensorManager remains unchanged;
// the Wio environment manager implements this alongside the baseline sensor
// interface so UI scheduling never leaks into MeshCore's generic contract.
class GpsService {
public:
  enum Purpose : uint8_t {
    GPS_NONE, GPS_CONTINUOUS, GPS_SCHEDULED, GPS_ADAPTIVE,
    GPS_TIME_SYNC, GPS_MANUAL, GPS_EMERGENCY
  };
  enum EventType : uint8_t {
    GPS_EVENT_NONE, GPS_EVENT_STARTED, GPS_EVENT_COMPLETED,
    GPS_EVENT_TIMEOUT, GPS_EVENT_STOPPED, GPS_EVENT_ADAPTIVE_PHASE
  };
  struct Event {
    EventType type = GPS_EVENT_NONE;
    Purpose purpose = GPS_NONE;
    uint8_t adaptive_phase = 0;
    uint32_t session_id = 0;
  };
  struct Configuration {
    bool enabled = false;
    bool adaptive = false;
    uint32_t interval_seconds = 0;
    Purpose purpose = GPS_NONE;
  };
  struct Status {
    bool available = false;
    bool configured = false;
    bool receiver_active = false;
    bool fix_valid = false;
    bool quality_good = false;
    Purpose purpose = GPS_NONE;
    uint8_t adaptive_phase = 0;
    uint8_t consecutive_failures = 0;
    uint32_t session_id = 0;
    uint32_t sample_id = 0;
    uint32_t next_acquire_ms = 0;
    int32_t latitude = 0;
    int32_t longitude = 0;
    long altitude = 0;
    long satellites = 0;
    long hdop = -1;
    long course = LONG_MIN;
    long speed = LONG_MIN;
  };

  virtual ~GpsService() = default;
  virtual void onUserDisplayWake() = 0;
  virtual bool getAdaptiveRetry(uint32_t& remaining_ms) const = 0;
  virtual bool applyConfiguration(const Configuration& config) = 0;
  virtual bool setPowerClaim(bool active, Purpose purpose) = 0;
  virtual Status runtimeStatus() const = 0;
  virtual bool takeEvent(Event& event) = 0;
};

}  // namespace zen
