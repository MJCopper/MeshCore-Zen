#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "Stream.h"
inline uint32_t g_mock_millis = 0;
inline uint32_t millis() { return g_mock_millis; }
inline uint32_t micros() { return g_mock_millis * 1000; }
inline void delay(uint32_t ms) { g_mock_millis += ms; }
template<typename T> inline T constrain(T x, T lo, T hi) {
  return x < lo ? lo : x > hi ? hi : x;
}
#define PROGMEM
#define F(x) x
using std::isnan;
inline char* ltoa(long value, char* dest, int radix) {
  sprintf(dest, radix == 16 ? "%lx" : "%ld", value);
  return dest;
}
