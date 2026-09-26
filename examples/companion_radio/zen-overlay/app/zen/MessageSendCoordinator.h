#pragma once

#include "MessageSendState.h"
#include "MessageTransportAdapter.h"

namespace zen {

// Owns construction of one on-device send attempt. Recipient selection and
// Child Mode permission remain UI policy; radio calls and delivery metadata do
// not. Phone and Bluetooth sends continue through MyMesh unchanged.
struct MessageSendCoordinator {
  static bool sendChannel(ChannelDetails& channel, const char* text) {
    return MessageTransportAdapter::sendChannel(channel, text);
  }

  static bool sendDirect(ContactInfo& contact, const char* text,
                         MessageSendState& state) {
    state.reset();
    state.timestamp = rtc_clock.getCurrentTime();
    state.route = MessageDeliveryCoordinator::routeForPath(contact.out_path_len);

    uint32_t expected_ack = 0;
    uint32_t estimated_timeout = 0;
    if (!MessageTransportAdapter::sendDirect(contact, state.timestamp, 0, text,
                                             expected_ack, estimated_timeout))
      return false;

    state.ack_tag = expected_ack;
    if (expected_ack)
      state.ack_deadline_ms = MessageDeliveryCoordinator::deadline(
          millis(), estimated_timeout);
    return true;
  }
};

} // namespace zen
