#pragma once

#include "Mesh.h"
#include <limits.h>


class LocationProvider {
protected:
    bool _time_sync_needed = true;

public:
    virtual void syncTime() { _time_sync_needed = true; }
    virtual bool waitingTimeSync() { return _time_sync_needed; }
    virtual long getLatitude() = 0;
    virtual long getLongitude() = 0;
    virtual long getAltitude() = 0;
    virtual long satellitesCount() = 0;
    // Course in thousandths of a degree and speed in thousandths of a knot.
    // LONG_MIN means the provider does not expose a current RMC value.
    virtual long getCourse() { return LONG_MIN; }
    virtual long getSpeed() { return LONG_MIN; }
    // Horizontal Dilution of Precision, in tenths (11 == HDOP 1.1) -- lower is
    // better, and a much more direct read on fix quality than satellite count
    // alone (few satellites in good geometry can beat many in poor geometry).
    // -1 means this provider doesn't expose it; callers fall back to
    // satellitesCount() in that case.
    virtual long getHDOP() { return -1; }
    virtual bool isValid() = 0;
    virtual long getTimestamp() = 0;
    virtual void sendSentence(const char * sentence);
    virtual void reset() = 0;
    virtual void begin() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    virtual bool isEnabled() = 0;
};
