#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// Final-stage milestone, independent of lifetime evolution XP.
struct PetRetirement {
  static constexpr uint8_t XP=100,BOND=10,TRAINING_XP=20;
  static uint8_t add(uint8_t value,uint16_t reward) {
    return reward>=XP-value?XP:value+reward;
  }
};

} }
