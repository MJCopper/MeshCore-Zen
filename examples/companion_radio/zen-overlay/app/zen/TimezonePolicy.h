#pragma once
#include <stdint.h>
#include <time.h>

namespace zen {

// Resolves local UTC offsets without a timezone database or network access.
// The small city table uses contemporary rules suited to the device's limited
// flash/RAM budget. UTC remains the source of truth; no clock value is rewritten.
class TimezonePolicy {
public:
  enum Mode : uint8_t { MANUAL = 0, CITY = 1 };
  static constexpr uint8_t DEFAULT_MODE = CITY;
  static constexpr uint8_t DEFAULT_CITY = 0; // Sydney
  enum Rule : uint8_t { NONE, AU_EAST, AU_CENTRAL, NZ, EU, US };
  struct City {
    const char* name;
    int16_t standard_minutes;
    int16_t daylight_minutes;
    Rule rule;
  };

  static const City* cities() {
    static const City C[] = {
      {"Sydney", 600, 660, AU_EAST}, {"Melb.", 600, 660, AU_EAST},
      {"Brisbane", 600, 600, NONE}, {"Adelaide", 570, 630, AU_CENTRAL},
      {"Perth", 480, 480, NONE}, {"Hobart", 600, 660, AU_EAST},
      {"Darwin", 570, 570, NONE}, {"Canberra", 600, 660, AU_EAST},
      {"Auckland", 720, 780, NZ}, {"Tokyo", 540, 540, NONE},
      {"S'pore", 480, 480, NONE}, {"HongKong", 480, 480, NONE},
      {"Beijing", 480, 480, NONE}, {"Delhi", 330, 330, NONE},
      {"London", 0, 60, EU}, {"Paris", 60, 120, EU},
      {"New York", -300, -240, US}, {"Chicago", -360, -300, US},
      {"Denver", -420, -360, US}, {"Los Ang.", -480, -420, US}
    };
    return C;
  }
  static constexpr uint8_t CITY_COUNT = 20;

  static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe - 719468;
  }
  static uint8_t weekday(int y, int m, int d) {
    int64_t w = (daysFromCivil(y, m, d) + 4) % 7;
    if (w < 0) w += 7;
    return (uint8_t)w;
  }
  static uint8_t nthSunday(int year, int month, int nth) {
    return (uint8_t)(1 + ((7-weekday(year,month,1))%7) + 7*(nth-1));
  }
  static uint8_t lastSunday(int year, int month) {
    static const uint8_t md[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int last=md[month-1]+(month==2 && (year%4==0 && (year%100!=0 || year%400==0)));
    return (uint8_t)(last-weekday(year,month,last));
  }
  static uint32_t utcDate(int year, int month, int day, int hour, int offset_minutes) {
    return (uint32_t)(daysFromCivil(year,month,day)*86400 + hour*3600 - (int64_t)offset_minutes*60);
  }
  static bool inDst(const City& c, uint32_t utc) {
    if (c.rule==NONE || c.standard_minutes==c.daylight_minutes) return false;
    time_t raw = utc;
    struct tm* p = gmtime(&raw);
    if (!p) return false;
    int y = p->tm_year + 1900;
    uint32_t start = 0, end = 0;
    if (c.rule == AU_EAST || c.rule == AU_CENTRAL) {
      start = utcDate(y, 10, nthSunday(y, 10, 1), 2, c.standard_minutes);
      end = utcDate(y, 4, nthSunday(y, 4, 1), 3, c.daylight_minutes);
      return utc >= start || utc < end;
    }
    if (c.rule==NZ) {
      start=utcDate(y,9,lastSunday(y,9),2,c.standard_minutes);
      end=utcDate(y,4,nthSunday(y,4,1),3,c.daylight_minutes);
      return utc>=start || utc<end;
    }
    if (c.rule==EU) {
      start=utcDate(y,3,lastSunday(y,3),1,0);
      end=utcDate(y,10,lastSunday(y,10),1,0);
    } else {
      start=utcDate(y,3,nthSunday(y,3,2),2,c.standard_minutes);
      end=utcDate(y,11,nthSunday(y,11,1),2,c.daylight_minutes);
    }
    return utc>=start && utc<end;
  }
  static int16_t offsetMinutes(uint8_t mode, int16_t manual, uint8_t city, uint32_t utc) {
    if (mode != CITY || city >= CITY_COUNT) return manual;
    const City& c = cities()[city];
    return inDst(c, utc) ? c.daylight_minutes : c.standard_minutes;
  }
  static const char* cityName(uint8_t city) {
    return cities()[city < CITY_COUNT ? city : DEFAULT_CITY].name;
  }
};
}
