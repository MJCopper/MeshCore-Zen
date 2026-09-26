#pragma once
// Populates a KeyboardWidget's placeholder list based on live sensor data.
// Only adds placeholders for types that actually returned a value right now.
// Always adds {loc} and {time} via begin(); this helper appends the rest.

#include "KeyboardWidget.h"
#include <helpers/SensorManager.h>
#include <helpers/sensors/LPPDataHelpers.h>

inline void kbAddSensorPlaceholders(KeyboardWidget& kb, SensorManager* sm) {
  if (!sm) return;

  CayenneLPP lpp(100);
  sm->querySensors(0xFF, lpp);

  // collect unique LPP types that have actual data
  uint8_t present[16];
  int pc = 0;
  {
    LPPReader r(lpp.getBuffer(), lpp.getSize());
    uint8_t ch, type;
    while (r.readHeader(ch, type)) {
      bool already = false;
      for (int i = 0; i < pc; i++) if (present[i] == type) { already = true; break; }
      if (!already && pc < 16) present[pc++] = type;
      r.skipData(type);
    }
  }

  static const struct { uint8_t t; const char* ph; } MAP[] = {
    { LPP_TEMPERATURE,         "{temp}" },
    { LPP_RELATIVE_HUMIDITY,   "{hum}"  },
    { LPP_BAROMETRIC_PRESSURE, "{pres}" },
    { LPP_VOLTAGE,             "{batt}" },
    { LPP_ALTITUDE,            "{alt}"  },
    { LPP_LUMINOSITY,          "{lux}"  },
    { LPP_DISTANCE,            "{dist}" },
    { LPP_CONCENTRATION,       "{co2}"  },
  };

  for (const auto& m : MAP) {
    for (int i = 0; i < pc; i++) {
      if (present[i] == m.t) { kb.addPlaceholder(m.ph); break; }
    }
  }
}
