#pragma once

#include <stdint.h>
#include <string.h>
#include "NotificationPolicy.h"

namespace zen {

enum class NotificationType : uint8_t {
  NONE, DIRECT_MESSAGE, CHANNEL_MESSAGE, ROOM_MESSAGE,
  ADVERT_FLOOD, ADVERT_LOCAL, NEW_CONTACT, UI_FEEDBACK,
  LOW_BATTERY, WARNING, ERROR
};

// Complete, short-lived description of one local notification. Pointers are
// consumed synchronously by UITask and are never retained.
struct NotificationEvent {
  NotificationType type = NotificationType::NONE;
  bool eligible = true;
  bool record_unread = false;
  bool visual = false;
  bool audible = true;
  bool quiet_affected = true;
  bool urgent_wake = false;
  NotificationPolicy::Source source = NotificationPolicy::DEFAULT;
  const char* popup = nullptr;
  uint16_t popup_ms = 0;
  uint8_t prefix[4] = {0, 0, 0, 0};
  bool prefix_valid = false;
  int8_t channel = -1;
};

struct NotificationContext {
  NotificationPolicy::Mode mode = NotificationPolicy::ON;
  NotificationPolicy::ScreenWake screen_wake = NotificationPolicy::ScreenWake::ON;
  bool client_connected = false;
  bool silent = false;
  bool quiet_time = false;
  bool low_power = false;
  bool emergency = false;
};

class NotificationCoordinator {
public:
  static uint8_t popupPriority(NotificationType type) {
    if (type == NotificationType::ERROR) return 3;
    if (type == NotificationType::WARNING || type == NotificationType::LOW_BATTERY)
      return 2;
    return type == NotificationType::NONE ? 0 : 1;
  }

  static NotificationDecision decide(const NotificationEvent& event,
                                     const NotificationContext& context) {
    NotificationDecision result = NotificationPolicy::decide(
        event.eligible, context.silent || context.quiet_time,
        event.quiet_affected, context.mode, context.client_connected,
        context.screen_wake, event.visual, event.source);
    result.record_unread = event.eligible && event.record_unread;
    if (!event.audible) {
      result.play_sound = false;
      result.vibrate = false;
    }
    if (event.urgent_wake)
      result.wake_screen = result.show_visual &&
          context.screen_wake != NotificationPolicy::ScreenWake::OFF;
    return result;
  }
};

// Owns the special five-second lifetime of a display woken only to present a
// notification. It deliberately knows nothing about display hardware.
class NotificationWakeController {
  bool _active = false;

public:
  struct Action {
    bool turn_on;
    uint32_t deadline;
    bool refresh;
  };

  Action present(bool display_on, bool allow_wake, uint32_t now,
                 uint32_t normal_timeout_ms) {
    bool turn_on = !display_on && allow_wake;
    if (turn_on) _active = true;
    bool visible = display_on || turn_on;
    uint32_t deadline = 0;
    if (visible)
      deadline = now + (_active ? 5000UL : normal_timeout_ms);
    return {turn_on, deadline, visible};
  }

  bool active() const { return _active; }
  void interaction() { _active = false; }
  void displayOff() { _active = false; }
};

} // namespace zen
