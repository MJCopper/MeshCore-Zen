#pragma once

#include <cstdint>
#include <cmath>
#include "Stream.h"

inline uint32_t g_mock_millis = 0;

using std::isnan;
using std::isfinite;

template<typename T> inline T constrain(T value, T low, T high) {
  return value < low ? low : value > high ? high : value;
}

inline uint32_t millis() {
  return g_mock_millis;
}

inline void delay(uint32_t ms) {
  g_mock_millis += ms;
}
