#pragma once

#include "RemoteNodeOperation.h"

namespace zen {

struct RemoteAdminAdapter {
  static bool retrySafe(bool fetch_or_verify) { return fetch_or_verify; }
  static RemoteNodeOperation::Kind kind(bool fetch_or_verify, bool write) {
    return fetch_or_verify ? RemoteNodeOperation::ADMIN_READ :
           (write ? RemoteNodeOperation::ADMIN_WRITE
                  : RemoteNodeOperation::ADMIN_ACTION);
  }
  static RemoteNodeOperation::RetryMode retryMode(bool fetch_or_verify) {
    return fetch_or_verify ? RemoteNodeOperation::RETRY_SAFE
                           : RemoteNodeOperation::DO_NOT_RETRY;
  }
};

} // namespace zen
