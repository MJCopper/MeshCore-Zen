#pragma once

#include <stdint.h>
#include <string.h>

namespace zen {

// Login replies carry server time, not the request tag. Give tagged app
// responses priority and accept the legacy shapes plus extensible current
// replies, matching the BLE parser in MyMesh.
struct NodeLoginResponse {
  static bool valid(const uint8_t* data, uint8_t len) {
    if (!data || len < 6) return false;
    if (len == 6 && memcmp(data + 4, "OK", 2) == 0) return true;
    return (len == 8 || len == 12 || len >= 13) && data[4] == 0;
  }

  static bool busy(uint32_t pending, uint32_t deadline, uint32_t now) {
    return pending && (int32_t)(now - deadline) < 0;
  }

  static uint8_t effectivePermissions(uint8_t login_role,
                                      uint8_t acl_permissions) {
    // The protocol carries both the legacy login role and the newer complete
    // ACL byte. Some repeater versions reliably report Admin only through the
    // legacy role field, so retain the ACL flags while honouring that explicit
    // result. A room role of 2 is not Admin and must not be promoted.
    if (login_role == 1) {
      acl_permissions &= ~0x03;
      acl_permissions |= 0x03;
    }
    return acl_permissions;
  }

  static bool grantsAdmin(bool success, uint8_t permissions,
                          bool password_supplied) {
    if (!success) return false;
    if ((permissions & 0x03) == 0x03) return true;
    // Legacy repeaters reply with only "OK" and no permissions byte. A
    // password-authenticated success from that response means the admin
    // password was accepted. Never make this inference for blank ACL login.
    return password_supplied && permissions == 0;
  }
};

} // namespace zen
