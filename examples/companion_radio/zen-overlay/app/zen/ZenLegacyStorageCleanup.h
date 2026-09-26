#pragma once

#include <helpers/IdentityStore.h>

namespace zen {

// Earlier Zen/Solo builds placed large extension records in MeshCore's small
// internal filesystem. Current Wio builds keep Zen data on external flash, so
// these ignored files only consume the working space needed by /prefs.json and
// Bluefruit bond records. Removal is idempotent and deliberately names no live
// MeshCore or Bluefruit path.
class LegacyStorageCleanup {
public:
  static void run(FILESYSTEM& primary) {
    static const char* const obsolete[] = {
      "/solo_prefs",
      "/solo_prefs.tmp",
      "/new_prefs.tmp",
      "/room_pw",
      "/room_pw.tmp",
      "/zen_prefs",
      "/zen_prefs.tmp",
    };
    for (const char* path : obsolete) {
      if (primary.exists(path)) primary.remove(path);
    }
  }
};

} // namespace zen
