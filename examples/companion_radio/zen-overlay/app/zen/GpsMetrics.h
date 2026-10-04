#pragma once

#include <limits.h>
#include <stdint.h>

namespace zen {

// Optional motion/quality data implemented by GPS providers that expose RMC
// course/speed and GGA dilution. Baseline LocationProvider stays unchanged.
class GpsMetrics {
public:
  virtual ~GpsMetrics() = default;
  virtual long course() = 0;
  virtual long speed() = 0;
  virtual long hdop() = 0;
  virtual uint32_t fixSequence() const { return 0; }
  virtual uint32_t fixReceivedMs() const { return 0; }
};

}  // namespace zen
