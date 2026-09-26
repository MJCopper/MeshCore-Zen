#pragma once

#include "ZenPolicy.h"

namespace zen {

// Pure Child Mode eligibility rules. UITask resolves contacts/channels from
// MyMesh, then supplies those values here; this module performs no transport
// or global-state lookup.
struct NotificationEligibility {
  static bool advert(bool child_locked) {
    return Policy::advertNotificationAllowed(child_locked);
  }

  static bool contact(const ZenPrefs* prefs, bool child_locked,
                      const ContactInfo* contact, uint8_t reported_type,
                      uint8_t expected_type) {
    if (!child_locked) return true;
    return contact &&
        Policy::contactIdentityMatches(contact->type, reported_type, expected_type) &&
        Policy::contactAllowed(prefs, true, contact, expected_type);
  }

  static bool channel(const ZenPrefs* prefs, bool child_locked, uint8_t index,
                      const char* name, const uint8_t* secret) {
    return !child_locked ||
        Policy::channelAllowed(prefs, true, index, name, secret);
  }
};

} // namespace zen
