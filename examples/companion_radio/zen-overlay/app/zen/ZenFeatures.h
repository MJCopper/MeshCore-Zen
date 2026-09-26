#pragma once

#include "ZenCapabilities.h"

// Zen feature selection is compile-time only. Product intent stays separate
// from board capability so baseline MeshCore companion builds do not inherit
// Zen behaviour merely because they use the same Wio hardware and UI sources.
#if defined(FIRMWARE_ZEN_BUILD)
  #define ZEN_FEATURE_DEFAULT 1
#else
  #define ZEN_FEATURE_DEFAULT 0
#endif

// Each flag remains independently overrideable from a board/environment. This
// lets an upstream integration take only the modules it wants without editing
// shared headers or maintaining another family of near-identical manifests.
#ifndef ZEN_FEATURE_CHILD_MODE
  #define ZEN_FEATURE_CHILD_MODE ZEN_FEATURE_DEFAULT
#endif
#ifndef ZEN_FEATURE_QUIET_TIME
  #define ZEN_FEATURE_QUIET_TIME ZEN_FEATURE_DEFAULT
#endif
#ifndef ZEN_FEATURE_CARDKB
  #define ZEN_FEATURE_CARDKB ZEN_FEATURE_DEFAULT
#endif
#ifndef ZEN_FEATURE_REPEATER
  #define ZEN_FEATURE_REPEATER ZEN_FEATURE_DEFAULT
#endif
#ifndef ZEN_FEATURE_AUTOCOMPLETE
  #define ZEN_FEATURE_AUTOCOMPLETE ZEN_FEATURE_DEFAULT
#endif
#ifndef ZEN_FEATURE_ADMIN
  #define ZEN_FEATURE_ADMIN ZEN_FEATURE_DEFAULT
#endif

namespace zen {

struct Features {
  static constexpr bool CHILD_MODE = ZEN_FEATURE_CHILD_MODE;
  static constexpr bool QUIET_TIME = ZEN_FEATURE_QUIET_TIME;
  static constexpr bool CARDKB = ZEN_FEATURE_CARDKB && ZEN_CAP_CARDKB;
  static constexpr bool ADMIN = ZEN_FEATURE_ADMIN;
  static constexpr bool REPEATER = ZEN_FEATURE_REPEATER;
  static constexpr bool AUTOCOMPLETE = ZEN_FEATURE_AUTOCOMPLETE;
};

} // namespace zen
