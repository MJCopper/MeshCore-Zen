#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
inline char* ltoa(long value, char* dest, int radix) {
  sprintf(dest, radix == 16 ? "%lx" : "%ld", value); return dest;
}
template<class T, class L, class H> inline T constrain(T v, L lo, H hi) {
  return v < lo ? (T)lo : v > hi ? (T)hi : v;
}
// Hardware boundary for host tests of the real radio preference commands.
struct NativeRadioDriver {
  bool rx_boosted = false;
  int tx_power = 0;
  bool setRxBoostedGainMode(bool enabled) { rx_boosted = enabled; return true; }
  void setTxPower(int power) { tx_power = power; }
};
inline NativeRadioDriver radio_driver;
