#pragma once

#include "../NodePrefs.h"

// Defaults and validation for preferences introduced by the Solo extension.
// Persistence code only reads/writes bytes; runtime code no longer needs to
// duplicate the valid ranges or migration fallbacks.
namespace solo {

class PrefsDefaults {
public:
  static void apply(NodePrefs& prefs) {
    prefs.child_visible_pages = NodePrefs::HP_FAVOURITES;
    prefs.child_rooms_enabled = 0;
    prefs.quiet_time_start_min = 21 * 60;
    prefs.quiet_time_end_min = 7 * 60;
    prefs.bluetooth_enabled = 1;
    // T9 is the factory on-screen layout. CardKB input is direct QWERTY and
    // ignores this preference while connected, so no hardware-specific value
    // needs to be persisted or rewritten at boot.
    prefs.keyboard_type = 1;
  }

  static void normalize(NodePrefs& prefs) {
    if (prefs.child_mode_enabled > 1) prefs.child_mode_enabled = 0;
    if (prefs.child_channels_enabled > 1) prefs.child_channels_enabled = 0;
    if (prefs.child_rooms_enabled > 1) prefs.child_rooms_enabled = 0;
    prefs.child_visible_pages &= NodePrefs::HP_FAVOURITES |
                                 NodePrefs::HP_MAP | NodePrefs::HP_SENSORS |
                                 NodePrefs::HP_SHUTDOWN;
    if (prefs.quiet_time_enabled > 1) prefs.quiet_time_enabled = 0;
    if (prefs.quiet_time_start_min >= 24 * 60) prefs.quiet_time_start_min = 21 * 60;
    if (prefs.quiet_time_end_min >= 24 * 60) prefs.quiet_time_end_min = 7 * 60;
    if (prefs.bluetooth_enabled > 1) prefs.bluetooth_enabled = 1;
    if (prefs.keyboard_type > 1) prefs.keyboard_type = 1;
  }
};

} // namespace solo
