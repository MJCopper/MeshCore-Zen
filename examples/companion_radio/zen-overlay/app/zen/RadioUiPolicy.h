#pragma once

namespace zen {

// UI-only constraints for the Wio Tracker L1 editor. Applying radio settings
// and all packet handling remain owned by the MeshCore baseline.
struct RadioUiPolicy {
  static constexpr float MIN_FREQUENCY_MHZ = 150.0f;
  static constexpr float MAX_FREQUENCY_MHZ = 2500.0f;
};

}  // namespace zen
