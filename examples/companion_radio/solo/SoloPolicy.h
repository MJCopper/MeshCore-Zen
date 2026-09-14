#pragma once

#include "../NodePrefs.h"
#include "SoloFeatures.h"
#include <Utils.h>
#include <helpers/ContactInfo.h>
#include <helpers/AdvertDataHelpers.h>
#include <string.h>

// One fail-closed policy boundary for every feature that can expose a child to
// a contact/channel or initiate traffic on their behalf. UI visibility,
// notifications, bots, location sharing and recovery entry points all call the
// same predicates so future features do not need to reinterpret Child Mode.
namespace solo {

class Policy {
public:
  static bool childLocked(const NodePrefs* prefs, bool parent_unlocked) {
    return Features::CHILD_MODE && prefs && prefs->child_mode_enabled && !parent_unlocked;
  }

  static bool favouriteContact(const ContactInfo& contact) {
    return (contact.flags & 0x01) != 0;
  }

  static bool contactIdentityMatches(uint8_t stored_type, uint8_t reported_type,
                                     uint8_t expected_type) {
    return stored_type == expected_type && reported_type == expected_type;
  }

  static bool channelsVisible(const NodePrefs* prefs, bool locked) {
    return !locked || (prefs && prefs->child_channels_enabled);
  }

  static bool contactAllowed(const NodePrefs* prefs, bool locked,
                             const ContactInfo* contact, uint8_t expected_type = 0) {
    if (!locked) return true;
    if (!prefs || !contact || !favouriteContact(*contact)) return false;
    if (expected_type != 0 && contact->type != expected_type) return false;
    if (contact->type == ADV_TYPE_CHAT) return true;
    if (contact->type == ADV_TYPE_ROOM) return prefs->child_rooms_enabled != 0;
    return false;
  }

  static bool privateChannel(const char* name, const uint8_t* secret) {
    static const uint8_t PUBLIC_SECRET[16] = {
      0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
      0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72
    };
    if (!name || !secret || memcmp(secret, PUBLIC_SECRET, sizeof(PUBLIC_SECRET)) == 0)
      return false;

    bool all_zero = true;
    for (int i = 0; i < 16 && all_zero; i++) all_zero = secret[i] == 0;
    if (all_zero) return false;

    // A leading '#' is only public when its key is the conventional hash of
    // the name. Separately shared private channels may legitimately use '#'.
    if (name[0] == '#') {
      uint8_t digest[32];
      mesh::Utils::sha256(digest, sizeof(digest),
                          reinterpret_cast<const uint8_t*>(name), strlen(name));
      if (memcmp(secret, digest, 16) == 0) return false;
    }
    return true;
  }

  static bool channelAllowed(const NodePrefs* prefs, bool locked, uint8_t index,
                             const char* name, const uint8_t* secret) {
    if (!locked) return true;
    return prefs && prefs->child_channels_enabled && index < 64 &&
           (prefs->ch_fav_bitmask & (1ULL << index)) != 0 &&
           privateChannel(name, secret);
  }

  static bool advertNotificationAllowed(bool locked) { return !locked; }
  static bool recoveryAllowed(bool locked) { return !locked; }
};

} // namespace solo
