#pragma once

#include <stdint.h>
#include <string.h>

namespace zen {

struct RemoteTelemetryAdapter {
  static bool matches(const uint8_t* expected_key, uint32_t expected_tag,
                      const uint8_t* key, uint32_t tag) {
    return expected_key && key && expected_tag == tag &&
           memcmp(expected_key, key, 32) == 0;
  }
};

} // namespace zen
