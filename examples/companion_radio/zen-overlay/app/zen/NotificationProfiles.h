#pragma once

#include "NotificationCoordinator.h"

namespace zen {

// Single source of truth for the default behaviour of every notification
// class. Callers attach identity and text; they do not reconstruct policy
// flags independently.
struct NotificationProfiles {
  static NotificationEvent event(NotificationType type) {
    NotificationEvent result;
    result.type = type;
    switch (type) {
      case NotificationType::DIRECT_MESSAGE:
      case NotificationType::CHANNEL_MESSAGE:
      case NotificationType::ROOM_MESSAGE:
        result.record_unread = true;
        result.visual = true;
        break;
      case NotificationType::NEW_CONTACT:
        result.visual = true;
        break;
      case NotificationType::LOW_BATTERY:
        result.visual = true;
        break;
      case NotificationType::WARNING:
      case NotificationType::ERROR:
        result.visual = true;
        result.audible = false;
        result.quiet_affected = false;
        result.urgent_wake = true;
        break;
      case NotificationType::ADVERT_FLOOD:
      case NotificationType::ADVERT_LOCAL:
      case NotificationType::UI_FEEDBACK:
      case NotificationType::NONE:
      default:
        break;
    }
    return result;
  }
};

} // namespace zen
