#pragma once

#include <Arduino.h>
#include <Wire.h>

// Complete transport/lifecycle layer for an explicitly enabled M5Stack
// CardKB. The UI receives semantic events and does not need to know the
// keyboard's I2C protocol, Fn scan codes, debounce rules or sleep boundary.
class CardKBController {
public:
  enum EventType : uint8_t { NONE, KEY, SUBMIT, HOLD, EMOJI };

  struct Event {
    EventType type = NONE;
    char key = 0;
  };

private:
  // About 33 Hz remains responsive for human typing while cutting active-screen
  // I2C transactions by one third compared with the former 50 Hz poll.
  static const uint32_t POLL_INTERVAL_MS = 30;
  static const uint8_t MAX_READ_FAILURES = 3;

  TwoWire* _wire = nullptr;
  uint8_t _address = 0x5F;
  uint8_t _read_failures = 0;
  uint8_t _last_raw = 0;
  uint32_t _next_poll_ms = 0;
  bool _present = false;
  bool _active = false;
  bool _await_release = false;

  static bool due(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
  }

  bool readRaw(uint8_t& raw) {
    if (!_wire || !_present || !_active) return false;
    uint32_t now = millis();
    if (!due(now, _next_poll_ms)) return false;
    _next_poll_ms = now + POLL_INTERVAL_MS;
    if (_wire->requestFrom(_address, (uint8_t)1) != 1 || !_wire->available()) {
      if (++_read_failures >= MAX_READ_FAILURES) _present = false;
      return false;
    }
    _read_failures = 0;
    raw = (uint8_t)_wire->read();
    return true;
  }

public:
  void begin(TwoWire& wire, uint8_t address = 0x5F) {
    _wire = &wire;
    _address = address;
    _read_failures = 0;
    _last_raw = 0;
    _next_poll_ms = 0;
    _wire->beginTransmission(_address);
    _present = (_wire->endTransmission() == 0);
    _active = _present;
    _await_release = false;
  }

  bool isPresent() const { return _present; }
  bool isActive() const { return _present && _active; }

  // Suspend before the display is powered down. Resume deliberately ignores
  // input until it observes a release, so a key held across wake cannot leak
  // into the newly visible screen.
  void suspend() {
    _active = false;
    _last_raw = 0;
    _await_release = true;
  }

  void resume() {
    if (!_present) return;
    _active = true;
    _last_raw = 0;
    _next_poll_ms = 0;
    _await_release = true;
  }

  bool poll(Event& event) {
    event = Event();
    uint8_t raw;
    if (!readRaw(raw)) return false;
    if (_await_release) {
      if (raw == 0) _await_release = false;
      return false;
    }
    if (raw == _last_raw) return false;
    _last_raw = raw;
    if (raw == 0) return false;

    if (raw == 0xA3) event.type = SUBMIT;       // Fn+Enter
    else if (raw == 0x09) event.type = HOLD;    // Tab
    else if (raw == 0xAC) event.type = EMOJI; // Fn+M
    else if (raw < 0x80 || raw > 0xAF) { event.type = KEY; event.key = (char)raw; }
    return event.type != NONE;
  }
};
