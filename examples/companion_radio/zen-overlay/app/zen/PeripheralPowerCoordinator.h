#pragma once

#include <stdint.h>

namespace zen {

// Cross-peripheral power policy. Persistent preferences are inputs only: all
// temporary requests and safety restrictions remain in RAM and can therefore
// never turn a runtime transition into a flash write.
class PeripheralPowerCoordinator {
public:
  static const int8_t NO_OVERRIDE = -1;

  struct RequestedState {
    bool display_on = false;
    bool cardkb_present = false;
    bool gps_saved_on = false;
    int8_t gps_session = NO_OVERRIDE;
    bool gps_time_sync = false;
    bool emergency_gps = false;
    bool bluetooth_saved_on = false;
    int8_t bluetooth_session = NO_OVERRIDE;
    bool radio_on = true;
    uint8_t brightness = 0;
  };

  struct Restrictions {
    bool child_locked = false;
    bool low_power = false;
    bool emergency = false;
    bool shutting_down = false;
  };

  struct EffectiveState {
    bool display_on = false;
    bool cardkb_polling = false;
    bool gps_policy_on = false;
    bool gps_force_on = false;
    bool bluetooth_on = false;
    bool radio_on = false;
    bool low_power = false;
    bool emergency = false;
    uint8_t brightness = 0;

    bool operator==(const EffectiveState& rhs) const {
      return display_on == rhs.display_on &&
             cardkb_polling == rhs.cardkb_polling &&
             gps_policy_on == rhs.gps_policy_on &&
             gps_force_on == rhs.gps_force_on &&
             bluetooth_on == rhs.bluetooth_on &&
             radio_on == rhs.radio_on &&
             low_power == rhs.low_power && emergency == rhs.emergency &&
             brightness == rhs.brightness;
    }
    bool operator!=(const EffectiveState& rhs) const { return !(*this == rhs); }
  };

  enum Change : uint16_t {
    CHANGE_DISPLAY = 1u << 0,
    CHANGE_CARDKB = 1u << 1,
    CHANGE_GPS_POLICY = 1u << 2,
    CHANGE_GPS_FORCE = 1u << 3,
    CHANGE_BLUETOOTH = 1u << 4,
    CHANGE_RADIO = 1u << 5,
    CHANGE_BRIGHTNESS = 1u << 6,
    CHANGE_POWER_CONTEXT = 1u << 7,
    CHANGE_ALL = CHANGE_DISPLAY | CHANGE_CARDKB | CHANGE_GPS_POLICY |
                 CHANGE_GPS_FORCE | CHANGE_BLUETOOTH | CHANGE_RADIO |
                 CHANGE_BRIGHTNESS | CHANGE_POWER_CONTEXT
  };

  struct Transition {
    EffectiveState target;
    uint16_t changes = 0;
  };

private:
  RequestedState _requested;
  Restrictions _restrictions;
  EffectiveState _applied;
  bool _applied_valid = false;

  static bool requested(bool saved, int8_t session) {
    return session == NO_OVERRIDE ? saved : session != 0;
  }

public:
  RequestedState& requests() { return _requested; }
  const RequestedState& requests() const { return _requested; }
  Restrictions& restrictions() { return _restrictions; }
  const Restrictions& restrictions() const { return _restrictions; }

  EffectiveState resolve() const {
    EffectiveState out;
    out.low_power = _restrictions.low_power;
    out.emergency = _restrictions.low_power && _restrictions.emergency;

    if (_restrictions.shutting_down) return out;

    out.display_on = _requested.display_on;
    out.cardkb_polling = out.display_on && _requested.cardkb_present;
    out.brightness = _restrictions.low_power ? 0 : _requested.brightness;

    const bool gps_requested = requested(_requested.gps_saved_on,
                                         _requested.gps_session);
    if (_restrictions.low_power) {
      out.gps_force_on = out.emergency && _requested.emergency_gps;
    } else {
      out.gps_policy_on = gps_requested;
      out.gps_force_on = _requested.gps_time_sync;
    }

    const bool bluetooth_requested = requested(
        _requested.bluetooth_saved_on, _requested.bluetooth_session);
    if (!_restrictions.child_locked) {
      out.bluetooth_on = _restrictions.low_power
          ? out.emergency && _requested.bluetooth_session == 1
          : bluetooth_requested;
    }

    out.radio_on = _requested.radio_on &&
                   (!_restrictions.low_power || out.emergency);
    return out;
  }

  Transition transition() const {
    Transition out;
    out.target = resolve();
    if (!_applied_valid) {
      out.changes = CHANGE_ALL;
      return out;
    }
    if (out.target.display_on != _applied.display_on) out.changes |= CHANGE_DISPLAY;
    if (out.target.cardkb_polling != _applied.cardkb_polling) out.changes |= CHANGE_CARDKB;
    if (out.target.gps_policy_on != _applied.gps_policy_on) out.changes |= CHANGE_GPS_POLICY;
    if (out.target.gps_force_on != _applied.gps_force_on) out.changes |= CHANGE_GPS_FORCE;
    if (out.target.bluetooth_on != _applied.bluetooth_on) out.changes |= CHANGE_BLUETOOTH;
    if (out.target.radio_on != _applied.radio_on) out.changes |= CHANGE_RADIO;
    if (out.target.brightness != _applied.brightness) out.changes |= CHANGE_BRIGHTNESS;
    if (out.target.low_power != _applied.low_power ||
        out.target.emergency != _applied.emergency)
      out.changes |= CHANGE_POWER_CONTEXT;
    return out;
  }

  void markApplied(const EffectiveState& state) {
    _applied = state;
    _applied_valid = true;
  }
  void invalidateApplied() { _applied_valid = false; }
  void noteBluetoothApplied(bool on) {
    if (_applied_valid) _applied.bluetooth_on = on;
  }
  bool hasAppliedState() const { return _applied_valid; }
  const EffectiveState& applied() const { return _applied; }
  EffectiveState effective() const { return resolve(); }
  uint16_t pendingChanges() const { return transition().changes; }
};

} // namespace zen
