#include <Arduino.h>
#include "ZenWioTrackerL1Board.h"

void ZenWioTrackerL1Board::begin() {
  WioTrackerL1Board::begin();
  pinMode(VBAT_ENABLE, OUTPUT);
  digitalWrite(VBAT_ENABLE, HIGH); // voltage divider always on: gating it per-read left
                                   // the divider node unsettled at sample time (10ms was
                                   // too short), reading high & jittery. ~2uA standby cost.
}
