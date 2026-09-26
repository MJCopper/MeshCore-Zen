#pragma once

#include "NodeLoginResponse.h"

namespace zen {

struct RemoteLoginAdapter {
  static bool appBusy(uint32_t pending, uint32_t deadline, uint32_t now) {
    return NodeLoginResponse::busy(pending, deadline, now);
  }
  static bool valid(const uint8_t* data, uint8_t len) {
    return NodeLoginResponse::valid(data, len);
  }
  static uint8_t permissions(uint8_t role, uint8_t acl) {
    return NodeLoginResponse::effectivePermissions(role, acl);
  }
  static bool grantsAdmin(bool success, uint8_t permissions,
                          bool password_supplied) {
    return NodeLoginResponse::grantsAdmin(success, permissions,
                                          password_supplied);
  }
};

} // namespace zen
