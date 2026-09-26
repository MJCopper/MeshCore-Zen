#pragma once

#include <stdint.h>

namespace zen {

// Zen decides whether Bluetooth is requested; MeshCore remains the sole owner
// of the BLE stack, pairing, bonds, framing and companion protocol. This small
// adapter keeps policy transitions and passive observations out of that
// baseline implementation.
class BluetoothPolicyAdapter {
public:
  enum Action : uint8_t { NO_ACTION, ENABLE, DISABLE };
  enum Event : uint8_t {
    NO_EVENT, ENABLED, DISABLED, CONNECTED, DISCONNECTED
  };

private:
  bool _observed = false;
  bool _enabled = false;
  bool _connected = false;

public:
  Action action(bool requested, bool enabled) const {
    if (requested == enabled) return NO_ACTION;
    return requested ? ENABLE : DISABLE;
  }

  Event observe(bool enabled, bool connected) {
    connected = enabled && connected;
    if (!_observed) {
      _observed = true;
      _enabled = enabled;
      _connected = connected;
      return NO_EVENT;
    }

    Event event = NO_EVENT;
    if (_connected != connected)
      event = connected ? CONNECTED : DISCONNECTED;
    else if (_enabled != enabled)
      event = enabled ? ENABLED : DISABLED;
    _enabled = enabled;
    _connected = connected;
    return event;
  }
};

} // namespace zen
