#pragma once

#include <limits.h>

namespace zen {

// Optional motion/quality data implemented by GPS providers that expose RMC
// course/speed and GGA dilution. Baseline LocationProvider stays unchanged.
class GpsMetrics {
public:
  virtual ~GpsMetrics() = default;
  virtual long course() = 0;
  virtual long speed() = 0;
  virtual long hdop() = 0;
};

}  // namespace zen
