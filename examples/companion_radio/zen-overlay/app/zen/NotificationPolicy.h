#pragma once

#include <stdint.h>

namespace zen {

// Separate delivery, visual presentation, screen wake and sound. Quiet Time
// suppresses sound only; Child Mode can reject a local alert entirely.
struct NotificationDecision {
  bool record_unread;
  bool show_visual;
  bool wake_screen;
  bool play_sound;
  bool vibrate;

  bool present() const { return show_visual || play_sound || vibrate; }
};

class NotificationPolicy {
public:
  enum Mode : uint8_t { ON, OFF, AUTO };
  enum class ScreenWake : uint8_t { OFF, ON, ALWAYS };
  // Source overrides affect routine message sound and mode-dependent wake,
  // never an explicit global Off or scheduled/manual quiet. MUTED suppresses
  // local presentation but not unread state; LOCAL permits sound in Auto with
  // a connected client.
  enum Source : uint8_t { DEFAULT, MUTED, LOCAL };

  static Mode mode(bool quiet, bool automatic) {
    return automatic ? AUTO : (quiet ? OFF : ON);
  }

  static bool active(Mode mode, bool client_connected) {
    return mode == ON || (mode == AUTO && !client_connected);
  }

  static bool audioMuted(Mode mode, bool client_connected, bool quiet_active) {
    return !active(mode, client_connected) || quiet_active;
  }

  static ScreenWake screenWake(uint8_t value) {
    return value <= (uint8_t)ScreenWake::ALWAYS ? (ScreenWake)value : ScreenWake::ON;
  }

  static NotificationDecision decide(bool eligible, bool quiet_active,
                                     bool quiet_affected, Mode mode,
                                     bool client_connected, ScreenWake screen_wake,
                                     bool visual_event, Source source = DEFAULT) {
    bool allowed = eligible && source != MUTED;
    bool audible = active(mode, client_connected) || (mode == AUTO && source == LOCAL);
    bool visual = allowed && visual_event;
    bool sound = allowed && audible && !(quiet_active && quiet_affected);
    NotificationDecision result = {
      eligible, visual,
      visual && (screen_wake == ScreenWake::ALWAYS ||
                 (screen_wake == ScreenWake::ON && audible)),
      sound, sound
    };
    return result;
  }
};

} // namespace zen
