#pragma once

#include <stdint.h>
#include "RemoteNodeOperation.h"

namespace zen {

// Single owner of every on-device remote request. BLE requests deliberately
// remain in the upstream MyMesh pending fields and are not routed through this
// coordinator.
class RemoteNodeCoordinator {
public:
  enum Owner : uint8_t { NO_OWNER, LOGIN_OWNER, ADMIN_OWNER, SENSOR_OWNER };

private:
  Owner _owner = NO_OWNER;
  RemoteNodeOperation _operation;

public:
  bool active() const { return _owner != NO_OWNER; }
  bool ownedBy(Owner owner) const { return _owner == owner && _operation.active(); }
  Owner owner() const { return _owner; }
  const RemoteNodeOperation& operation() const { return _operation; }

  bool begin(Owner owner, RemoteNodeOperation::Kind kind, const uint8_t* key,
             bool known_path, RemoteNodeOperation::RetryMode retry_mode,
             uint32_t deadline) {
    if (active() || owner == NO_OWNER || !key) return false;
    _owner = owner;
    _operation.begin(kind, key, known_path, retry_mode, deadline);
    return true;
  }

  bool expired(Owner owner, uint32_t now) const {
    return ownedBy(owner) && _operation.expired(now);
  }

  RemoteNodeOperation::Action timeout(Owner owner, uint32_t now,
                                      bool path_available) {
    if (!ownedBy(owner)) return RemoteNodeOperation::WAIT;
    RemoteNodeOperation::Action action = _operation.onTimeout(now, path_available);
    if (action == RemoteNodeOperation::FINISH_TIMEOUT ||
        action == RemoteNodeOperation::FINISH_UNKNOWN) _owner = NO_OWNER;
    return action;
  }

  bool restart(Owner owner, uint32_t deadline) {
    if (_owner != owner || _operation.active()) return false;
    return _operation.restart(_operation.route(), deadline);
  }

  bool complete(Owner owner, const uint8_t* key,
                RemoteNodeOperation::Result result) {
    if (!ownedBy(owner) || !_operation.matches(key)) return false;
    bool completed = _operation.finish(result);
    if (completed) _owner = NO_OWNER;
    return completed;
  }

  bool cancel(Owner owner, const uint8_t* key = nullptr) {
    if (_owner != owner || (key && !_operation.matches(key))) return false;
    _operation.cancel();
    _owner = NO_OWNER;
    return true;
  }

  bool fail(Owner owner, RemoteNodeOperation::Result result) {
    if (_owner != owner) return false;
    _operation.settle(result);
    _owner = NO_OWNER;
    return true;
  }
};

} // namespace zen
