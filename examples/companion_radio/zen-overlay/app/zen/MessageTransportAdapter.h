#pragma once

#include "MessageDeliveryCoordinator.h"

namespace zen {

// Thin boundary around MyMesh used by the standalone messaging UI. It keeps
// radio calls out of conversation storage and leaves companion-app transport paths
// unchanged.
struct MessageTransportAdapter {
  static bool contactByPrefix(const uint8_t* prefix, ContactInfo& out) {
    int total = the_mesh.getNumContacts();
    for (int i = 0; i < total; i++) {
      ContactInfo contact;
      if (the_mesh.getContactByIdx(i, contact) && !memcmp(contact.id.pub_key, prefix, 4)) {
        out = contact;
        return true;
      }
    }
    return false;
  }
  static bool sendDirect(ContactInfo& contact, uint32_t timestamp, uint8_t attempt,
                         const char* text, uint32_t& ack, uint32_t& timeout,
                         bool force_flood = false) {
    ContactInfo route = contact;
    if (force_flood) route.out_path_len = OUT_PATH_UNKNOWN;
    return the_mesh.sendUIMessage(route, timestamp, attempt, text, ack, timeout) > 0;
  }
  static bool sendChannel(ChannelDetails& channel, const char* text) {
    return the_mesh.sendGroupMessage(rtc_clock.getCurrentTime(), channel.channel,
                                     the_mesh.getNodeName(), text, strlen(text));
  }
};

} // namespace zen
