#pragma once

#include <stdint.h>
#include <helpers/SensorManager.h>
#include "../../src/helpers/sensors/GpsAdaptivePolicy.h"
#include "BootTimeSync.h"
#include "GpsCourse.h"
#include "TimeSyncSource.h"
#include "GpsService.h"

namespace zen {

// Coordinates existing GPS, course and clock policies without owning hardware
// or persistence. Its outputs are claims consumed by the power coordinator.
class TimeLocationCoordinator {
public:
  struct Actions {
    bool request_gps_time = false;
    bool release_gps_time = false;
    bool retry_window_ended = false;
    bool refresh_ui = false;
  };
  struct Status {
    bool sync_pending = true;
    bool retry_window_open = false;
    TimeSyncSource sync_source = TimeSyncSource::RESTORED;
    uint32_t last_sync_ms = 0;
    GpsService::Status gps;
    GpsService::Event last_gps_event;
    uint32_t gps_event_count = 0;
    GpsCourse::Source course_source = GpsCourse::NONE;
    long course_millideg = LONG_MIN;
    bool fix_age_available = false;
    uint32_t fix_age_ms = 0;
  };

private:
  BootTimeSync _sync;
  GpsCourse _course;
  Status _status;
  uint32_t _rtc_generation = 0;
  uint32_t _next_sample_ms = 0;
  uint32_t _last_session_id = 0;
  bool _last_receiver_active = false;
  uint32_t _last_fix_ms = 0;

  void updateCourse(uint32_t now, const GpsService::Status& gps,
                    uint32_t configured_interval, bool lifecycle_event = false) {
    bool transition = gps.session_id != _last_session_id ||
                      gps.receiver_active != _last_receiver_active;
    if (!transition && !lifecycle_event &&
        (int32_t)(now - _next_sample_ms) < 0) return;
    _next_sample_ms = now + 1000UL;
    _last_session_id = gps.session_id;
    _last_receiver_active = gps.receiver_active;

    bool sync_only = gps.purpose == GpsService::GPS_TIME_SYNC;
    bool periodic = gps.purpose == GpsService::GPS_SCHEDULED ||
                    gps.purpose == GpsService::GPS_ADAPTIVE;
    _course.update(now, sync_only ? false : gps.receiver_active,
                   periodic, configured_interval, gps.fix_valid,
                   gps.latitude, gps.longitude, gps.course, gps.speed, gps.hdop);
    if (gps.fix_valid) {
      _last_fix_ms = now;
      _status.fix_age_available = true;
    }
  }

public:
  static const char* sourceName(TimeSyncSource source) {
    switch (source) {
      case TimeSyncSource::GPS: return "GPS";
      case TimeSyncSource::COMPANION: return "App";
      case TimeSyncSource::USB_CLI: return "USB/CLI";
      case TimeSyncSource::MESH: return "Mesh/Other";
      case TimeSyncSource::RESTORED: return "Restored";
      default: return "None";
    }
  }
  static const char* purposeName(GpsService::Purpose purpose) {
    switch (purpose) {
      case GpsService::GPS_CONTINUOUS: return "Continuous";
      case GpsService::GPS_SCHEDULED: return "Scheduled";
      case GpsService::GPS_ADAPTIVE: return "Adaptive";
      case GpsService::GPS_TIME_SYNC: return "Time sync";
      case GpsService::GPS_MANUAL: return "Manual";
      case GpsService::GPS_EMERGENCY: return "Emergency";
      default: return "Off";
    }
  }
  static const char* eventName(GpsService::EventType type) {
    switch (type) {
      case GpsService::GPS_EVENT_STARTED: return "Started";
      case GpsService::GPS_EVENT_COMPLETED: return "Completed";
      case GpsService::GPS_EVENT_TIMEOUT: return "Timeout";
      case GpsService::GPS_EVENT_STOPPED: return "Stopped";
      case GpsService::GPS_EVENT_ADAPTIVE_PHASE: return "Phase change";
      default: return "None";
    }
  }
  static const char* adaptiveName(uint8_t phase) {
    switch ((GpsAdaptivePolicy::Phase)phase) {
      case GpsAdaptivePolicy::INITIAL_SEARCH: return "Initial";
      case GpsAdaptivePolicy::TRACKING: return "Tracking";
      case GpsAdaptivePolicy::SHORT_STANDBY: return "Short sleep";
      case GpsAdaptivePolicy::SHORT_SEARCH: return "Short search";
      case GpsAdaptivePolicy::LONG_STANDBY: return "Long sleep";
      case GpsAdaptivePolicy::LONG_SEARCH: return "Long search";
      default: return "Disabled";
    }
  }
  static const char* courseSourceName(GpsCourse::Source source) {
    switch (source) {
      case GpsCourse::LIVE: return "Live";
      case GpsCourse::TRAVEL: return "Travel";
      case GpsCourse::LAST: return "Last";
      default: return "None";
    }
  }
  Actions begin(uint32_t generation, bool has_gps, bool gps_configured_on,
                uint32_t now) {
    _rtc_generation = generation;
    _sync.begin(generation, has_gps, gps_configured_on, now);
    _status.sync_pending = true;
    _status.retry_window_open = true;
    _status.sync_source = TimeSyncSource::RESTORED;
    Actions actions;
    actions.request_gps_time = _sync.shouldStartGps();
    actions.refresh_ui = true;
    return actions;
  }

  void noteSynchronized(TimeSyncSource source, uint32_t generation,
                        uint32_t now) {
    _status.sync_source = source;
    _status.last_sync_ms = now;
    _rtc_generation = generation;
  }

  Actions tick(uint32_t generation, bool gps_configured_on, bool gps_enabled,
               bool allow_gps_start, uint32_t configured_interval,
               const GpsService::Status& gps,
               bool provider_waiting_time, uint32_t now,
               bool lifecycle_event = false) {
    Actions actions;
    bool was_pending = _sync.pending();
    bool retries_open = _sync.retryWindowOpen();
    if (generation != _rtc_generation) {
      TimeSyncSource source = (!provider_waiting_time && gps.receiver_active)
          ? TimeSyncSource::GPS : TimeSyncSource::MESH;
      noteSynchronized(source, generation, now);
    }
    BootTimeSync::Action action = _sync.tick(
        generation, gps_configured_on, gps_enabled, now, allow_gps_start);
    actions.request_gps_time = action == BootTimeSync::Action::START_TEMP_GPS;
    actions.release_gps_time = action == BootTimeSync::Action::STOP_TEMP_GPS;
    actions.retry_window_ended = retries_open && !_sync.retryWindowOpen() &&
                                 _sync.pending();
    actions.refresh_ui = was_pending != _sync.pending();
    _status.sync_pending = _sync.pending();
    _status.retry_window_open = _sync.retryWindowOpen();
    _status.gps = gps;
    updateCourse(now, gps, configured_interval, lifecycle_event);
    _status.course_source = _course.read(now, _status.course_millideg);
    if (_status.fix_age_available) _status.fix_age_ms = now - _last_fix_ms;
    return actions;
  }

  Actions tick(uint32_t generation, bool gps_configured_on, bool gps_enabled,
               bool allow_gps_start, uint32_t configured_interval,
               GpsService* sensors, bool provider_waiting_time,
               uint32_t now) {
    GpsService::Status gps = sensors
        ? sensors->runtimeStatus() : GpsService::Status();
    bool lifecycle_event = false;
    if (sensors) {
      GpsService::Event event;
      while (sensors->takeEvent(event)) {
        bool preserve_terminal = event.type == GpsService::GPS_EVENT_STOPPED &&
            event.session_id == _status.last_gps_event.session_id &&
            (_status.last_gps_event.type == GpsService::GPS_EVENT_COMPLETED ||
             _status.last_gps_event.type == GpsService::GPS_EVENT_TIMEOUT);
        if (!preserve_terminal) _status.last_gps_event = event;
        _status.gps_event_count++;
        lifecycle_event = true;
      }
    }
    return tick(generation, gps_configured_on, gps_enabled,
        allow_gps_start, configured_interval, gps, provider_waiting_time, now,
        lifecycle_event);
  }

  bool syncPending() const { return _sync.pending(); }
  void onUserWake(GpsService* sensors) {
    if (sensors) sensors->onUserDisplayWake();
  }
  const Status& status() const { return _status; }
  GpsCourse::Source course(long& value) const {
    value = _status.course_millideg;
    return _status.course_source;
  }
};

} // namespace zen
