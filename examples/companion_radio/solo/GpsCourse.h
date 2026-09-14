#pragma once

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

namespace solo {

// RAM-only course model separating live course-over-ground from straight-line
// travel between completed timed GPS acquisitions.
class GpsCourse {
  static constexpr uint8_t COURSE_SAMPLES = 5;
  bool _candidate = false, _live = false, _have_segment_fix = false;
  int32_t _segment_lat = 0, _segment_lon = 0;
  uint32_t _segment_ms = 0, _last_motion_ms = 0;
  float _movement_metres = 0;
  long _samples[COURSE_SAMPLES] = {};
  uint8_t _sample_count = 0, _sample_head = 0;
  int8_t _stable_direction = -1;
  long _course_millideg = LONG_MIN;
  uint32_t _last_course_ms = 0;

  bool _poll_active = false, _poll_fix_valid = false;
  bool _previous_poll_valid = false, _travel_valid = false;
  int32_t _poll_lat = 0, _poll_lon = 0;
  int32_t _previous_poll_lat = 0, _previous_poll_lon = 0;
  long _poll_hdop = -1, _previous_poll_hdop = -1;
  long _travel_course_millideg = LONG_MIN;
  uint32_t _travel_expiry_ms = 0;

  static int32_t longitudeDelta(int32_t from, int32_t to) {
    int32_t delta = to - from;
    if (delta > 180000000L) delta -= 360000000L;
    else if (delta < -180000000L) delta += 360000000L;
    return delta;
  }
  static float distanceMetres(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    const float DEG_TO_M = 111194.9f / 1000000.0f;
    float mean_lat = ((lat1 + lat2) * 0.5f) / 1000000.0f;
    float dy = (lat2 - lat1) * DEG_TO_M;
    float dx = longitudeDelta(lon1, lon2) * DEG_TO_M * cosf(mean_lat * 0.01745329252f);
    return sqrtf(dx * dx + dy * dy);
  }
  static long bearingMilliDegrees(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    const float TO_RAD = 0.01745329252f / 1000000.0f;
    float p1 = lat1 * TO_RAD, p2 = lat2 * TO_RAD;
    float dl = longitudeDelta(lon1, lon2) * TO_RAD;
    float degrees = atan2f(sinf(dl) * cosf(p2),
        cosf(p1) * sinf(p2) - sinf(p1) * cosf(p2) * cosf(dl)) * 57.2957795f;
    if (degrees < 0) degrees += 360.0f;
    return (long)(degrees * 1000.0f + 0.5f);
  }
  static bool qualityUsable(long hdop) { return hdop < 0 || hdop <= 40; }
  static float qualityThreshold(float base, long a, long b = -1) {
    long worst = a > b ? a : b;
    return worst > 20 ? base * 1.5f : base;
  }
  void clearLiveCandidate() {
    _candidate = _live = _have_segment_fix = false;
    _movement_metres = 0;
    _sample_count = _sample_head = 0;
    _stable_direction = -1;
  }
  long smoothedCourse(long course) {
    _samples[_sample_head] = course;
    _sample_head = (_sample_head + 1) % COURSE_SAMPLES;
    if (_sample_count < COURSE_SAMPLES) _sample_count++;
    float sx = 0, sy = 0;
    for (uint8_t i = 0; i < _sample_count; i++) {
      float rad = _samples[i] * 0.00001745329252f;
      sx += cosf(rad); sy += sinf(rad);
    }
    long mean = (long)(atan2f(sy, sx) * 57295.7795f);
    if (mean < 0) mean += 360000L;
    uint8_t proposed = direction(mean);
    if (_stable_direction < 0) _stable_direction = proposed;
    else if (proposed != (uint8_t)_stable_direction) {
      long delta = mean - (long)_stable_direction * 45000L;
      while (delta > 180000L) delta -= 360000L;
      while (delta < -180000L) delta += 360000L;
      if (labs(delta) >= 27500L) _stable_direction = proposed;
    }
    return (long)_stable_direction * 45000L;
  }

public:
  static constexpr long MIN_SPEED_MILLI_KNOTS = 800;
  static constexpr long HOLD_SPEED_MILLI_KNOTS = 500;
  static constexpr uint32_t MOTION_GRACE_MS = 5000;
  static constexpr uint8_t DIRECTION_COUNT = 8;
  static constexpr float MIN_MOVEMENT_METRES = 10.0f;
  static constexpr float MIN_POLL_MOVEMENT_METRES = 20.0f;
  static constexpr uint32_t RETAIN_MS = 15UL * 60UL * 1000UL;
  static constexpr uint32_t MAX_TRAVEL_RETAIN_MS = 12UL * 60UL * 60UL * 1000UL;
  enum Source : uint8_t { NONE, LIVE, TRAVEL, LAST };

  static bool available(bool fix_valid, long course, long speed) {
    return fix_valid && course != LONG_MIN && speed != LONG_MIN && course >= 0 &&
           course < 360000 && speed >= MIN_SPEED_MILLI_KNOTS;
  }
  static uint8_t direction(long course) {
    return (uint8_t)(((course + 22500L) / 45000L) % DIRECTION_COUNT);
  }
  static uint8_t offset(uint8_t index, int8_t steps) {
    int value = (int)index + steps;
    while (value < 0) value += DIRECTION_COUNT;
    return (uint8_t)(value % DIRECTION_COUNT);
  }
  static const char* label(uint8_t index) {
    static const char* const LABELS[DIRECTION_COUNT] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    return LABELS[index < DIRECTION_COUNT ? index : 0];
  }

  void update(uint32_t now, bool receiver_active, bool periodic, uint32_t poll_interval_sec,
              bool fix_valid, int32_t lat, int32_t lon, long course, long speed, long hdop) {
    if (!periodic) {
      _poll_active = _poll_fix_valid = _previous_poll_valid = _travel_valid = false;
    } else if (receiver_active) {
      _poll_active = true;
      if (fix_valid && qualityUsable(hdop)) {
        _poll_lat = lat; _poll_lon = lon; _poll_hdop = hdop; _poll_fix_valid = true;
      }
    } else if (_poll_active) {
      if (_poll_fix_valid) {
        float threshold = qualityThreshold(MIN_POLL_MOVEMENT_METRES, _previous_poll_hdop, _poll_hdop);
        if (_previous_poll_valid && distanceMetres(_previous_poll_lat, _previous_poll_lon,
                                                   _poll_lat, _poll_lon) >= threshold) {
          _travel_course_millideg = bearingMilliDegrees(
              _previous_poll_lat, _previous_poll_lon, _poll_lat, _poll_lon);
          uint32_t retain = poll_interval_sec > MAX_TRAVEL_RETAIN_MS / 2000UL
                                ? MAX_TRAVEL_RETAIN_MS : poll_interval_sec * 2000UL;
          if (retain < RETAIN_MS) retain = RETAIN_MS;
          _travel_expiry_ms = now + retain;
          _travel_valid = true;
          _previous_poll_lat = _poll_lat; _previous_poll_lon = _poll_lon;
          _previous_poll_hdop = _poll_hdop;
        } else if (!_previous_poll_valid) {
          _previous_poll_lat = _poll_lat; _previous_poll_lon = _poll_lon;
          _previous_poll_hdop = _poll_hdop; _previous_poll_valid = true;
        } else {
          // Clear stale presentation but retain the anchor so short moves can accumulate.
          _travel_valid = false;
        }
      } // A failed acquisition does not disprove the previous travel result.
      _poll_active = _poll_fix_valid = false;
    }

    bool valid_course = receiver_active && fix_valid && qualityUsable(hdop) &&
                        course >= 0 && course < 360000;
    bool enter_motion = valid_course && speed >= MIN_SPEED_MILLI_KNOTS;
    bool hold_motion = valid_course && speed >= HOLD_SPEED_MILLI_KNOTS;
    if (!enter_motion && !_candidate) return;
    if (!hold_motion) {
      if ((uint32_t)(now - _last_motion_ms) <= MOTION_GRACE_MS) return;
      clearLiveCandidate();
      return;
    }
    _last_motion_ms = now;
    if (!_candidate) { _candidate = true; _movement_metres = 0; }
    if (!_have_segment_fix) {
      _segment_lat = lat; _segment_lon = lon; _segment_ms = now; _have_segment_fix = true;
      return;
    }
    float segment = distanceMetres(_segment_lat, _segment_lon, lat, lon);
    uint32_t dt = now - _segment_ms;
    if (segment >= 1.0f && dt > 0 && segment / (dt / 1000.0f) <= 50.0f) {
      _movement_metres += segment;
      _segment_lat = lat; _segment_lon = lon; _segment_ms = now;
    }
    if (!_live && _movement_metres < qualityThreshold(MIN_MOVEMENT_METRES, hdop)) return;
    _live = true;
    _course_millideg = smoothedCourse(course);
    _last_course_ms = now;
  }

  Source read(uint32_t now, long& course) const {
    if (_live && _course_millideg != LONG_MIN) { course = _course_millideg; return LIVE; }
    if (_travel_valid && (int32_t)(now - _travel_expiry_ms) <= 0) {
      course = _travel_course_millideg; return TRAVEL;
    }
    if (_course_millideg != LONG_MIN && (uint32_t)(now - _last_course_ms) <= RETAIN_MS) {
      course = _course_millideg; return LAST;
    }
    return NONE;
  }
};

} // namespace solo
