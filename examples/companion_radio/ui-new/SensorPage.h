#pragma once

#include "../solo/SensorAccessCoordinator.h"

// Remote sensors share the home card's header/navigation. Only opening a node
// or explicitly refreshing sends a request; browsing does not poll the radio.
class SensorPage {
  UITask* _task;
  int _selected = 0, _scroll = 0, _row = 0, _lines = 0;
  bool _open = false;
  bool _error_logged = false;
  solo::SensorAccessCoordinator _access;
  uint8_t _key[PUB_KEY_SIZE] = {};
  char _name[33] = {};

  int count() const {
    int n = 0;
    ContactInfo contact;
    for (int i = 0; the_mesh.getContactByIdx(i, contact); i++)
      if (contact.type == ADV_TYPE_SENSOR) n++;
    return n;
  }
  bool node(int index, ContactInfo& contact) const {
    for (int i = 0; the_mesh.getContactByIdx(i, contact); i++)
      if (contact.type == ADV_TYPE_SENSOR && index-- == 0) return true;
    return false;
  }
  bool requestTelemetry(bool new_sequence = true) {
    _row = 0;
    if (new_sequence) _error_logged = false;
    ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
    if (new_sequence)
      _access.beginTelemetry(contact && contact->out_path_len != OUT_PATH_UNKNOWN);
    else
      _access.telemetryStarted();
    bool sent = the_mesh.requestSensorTelemetry(_key);
    if (!sent) {
      _access.telemetrySendFailed();
      _task->logFailure("Telemetry", contact ? "Send failed" : "Node missing");
      _error_logged = true;
    }
    return sent;
  }
  bool startLogin(const char* password, bool entered_password) {
    ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
    bool sent = contact && _task->startNodeLogin(
        solo::NodeLoginCoordinator::SENSOR, *contact, password, false);
    if (sent) _access.loginStarted(entered_password);
    else {
      _access.loginStartFailed(entered_password);
      _task->logFailure("Sensor login",
                        _task->nodeLoginBusy() ? "Login busy" : "Send failed");
    }
    return sent;
  }

public:
  explicit SensorPage(UITask* task) : _task(task) {}
  bool passwordEditing() const { return _open && _access.passwordEditing(); }
  int renderPassword(DisplayDriver& display) { return _task->keyboard().render(display); }
  void tick() {
    if (!_open) return;
    if (the_mesh.sensorReplyState() == MyMesh::SENSOR_FAILED) {
      ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
      solo::SensorAccessCoordinator::Action action = _access.telemetryTimedOut(
          contact && contact->out_path_len != OUT_PATH_UNKNOWN);
      if (action == solo::SensorAccessCoordinator::START_BLANK_LOGIN) {
        startLogin("", false);
      } else if (action == solo::SensorAccessCoordinator::SEND_TELEMETRY) {
        requestTelemetry(false);
      } else if (action == solo::SensorAccessCoordinator::CLEAR_PATH_AND_SEND_TELEMETRY) {
        the_mesh.clearContactPath(_key, PUB_KEY_SIZE);
        requestTelemetry(false);
      }
    }
    if (_access.tick(millis()) == solo::SensorAccessCoordinator::SEND_TELEMETRY)
      requestTelemetry();
    if (_access.phase() == solo::SensorAccessCoordinator::ERROR && !_error_logged) {
      _task->logFailure("Telemetry", "No reply after retries");
      _error_logged = true;
    }
  }
  void onLoginResult(const uint8_t* key, bool success) {
    if (!_open || memcmp(_key, key, PUB_KEY_SIZE) != 0 ||
        (_access.phase() != solo::SensorAccessCoordinator::ACL_WAIT &&
         _access.phase() != solo::SensorAccessCoordinator::PASSWORD_WAIT)) return;
    bool password_entered = _access.phase() == solo::SensorAccessCoordinator::PASSWORD_WAIT;
    if (success) {
      _access.loginSucceeded(millis());
    } else {
      _access.loginFailed(password_entered);
      if (password_entered) {
        _task->logFailure("Sensor login", "Login rejected");
        _task->keyboard().begin("", 15);
      }
    }
  }
  void onLoginTimeout(const uint8_t* key) {
    if (!_open || memcmp(_key, key, PUB_KEY_SIZE) != 0) return;
    if (_access.phase() == solo::SensorAccessCoordinator::ACL_WAIT) {
      _access.loginFailed(false);
      _task->logFailure("Sensor login", "No ACL reply");
    } else if (_access.phase() == solo::SensorAccessCoordinator::PASSWORD_WAIT) {
      _access.loginFailed(true);
      _task->logFailure("Sensor login", "No reply");
      _task->keyboard().begin("", 15);
    }
  }
  void close() {
    if (_open) _task->cancelNodeLogin(solo::NodeLoginCoordinator::SENSOR, _key);
    _open = false;
    _error_logged = false;
    _access.reset();
    _row = 0;
    the_mesh.cancelSensorTelemetry();
  }
  bool handleInput(char c) {
    if (_open) {
      if (c == KEY_CANCEL) { close(); return true; }
      if (_access.phase() == solo::SensorAccessCoordinator::PASSWORD) {
        auto result = _task->keyboard().handleInput(c);
        if (result == KeyboardWidget::DONE) {
          char password[16];
          snprintf(password, sizeof(password), "%s", _task->keyboard().buf);
          startLogin(password, true);
          _task->keyboard().begin("", 15);  // do not retain credentials in the shared editor
        }
        else if (result == KeyboardWidget::CANCELLED) _access.cancelPassword();
        return true;
      }
      if (_access.phase() == solo::SensorAccessCoordinator::LOGIN_OFFER && c == KEY_ENTER) {
        _task->keyboard().begin("", 15);
        _task->keyboard().clearPlaceholders();
        _access.offerPassword();
        return true;
      }
      if (c == KEY_ENTER) {
        requestTelemetry();
        return true;
      }
      if (c == KEY_UP) { if (_row > 0) _row--; return true; }
      if (c == KEY_DOWN) {
        if (_row + 1 < _lines) _row++;
        return true;
      }
      return false;
    }
    int n = count();
    if (_selected >= n) _selected = n ? n - 1 : 0;
    if (c == KEY_UP || c == KEY_DOWN) {
      if (n) _selected = (_selected + (c == KEY_UP ? n - 1 : 1)) % n;
      return true;
    }
    if (c == KEY_ENTER) {
      ContactInfo contact;
      if (node(_selected, contact)) {
        memcpy(_key, contact.id.pub_key, PUB_KEY_SIZE);
        memcpy(_name, contact.name, 32); _name[32] = 0;
        _open = true; _row = 0;
        _access.reset();
        requestTelemetry();
      }
      return true;
    }
    return false;
  }
  void render(DisplayDriver& display, int y) {
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    int step = display.lineStep();
    if (_open) {
      display.drawTextEllipsized(0, y, display.width(), _name);
      y += step;
    }
    int visible = (display.height() - y) / step;
    if (visible < 1) visible = 1;
    if (_open) {
      auto state = the_mesh.sensorReplyState();
      const char* status = nullptr;
      if (_access.phase() == solo::SensorAccessCoordinator::ACL_WAIT) status = "Checking ACL...";
      else if (_access.phase() == solo::SensorAccessCoordinator::PASSWORD_WAIT) status = "Logging in...";
      else if (_access.phase() == solo::SensorAccessCoordinator::RETRY_DELAY) status = "Access confirmed...";
      else if (_access.phase() == solo::SensorAccessCoordinator::LOGIN_OFFER) status = "Enter: Login";
      else if (_access.phase() == solo::SensorAccessCoordinator::ERROR ||
               (_access.phase() == solo::SensorAccessCoordinator::AUTH_TELEMETRY_WAIT && state == MyMesh::SENSOR_FAILED))
        status = "No reply/access denied";
      else if (state == MyMesh::SENSOR_WAITING) status = "Requesting...";
      else if (state == MyMesh::SENSOR_FAILED) status = "No reply/access denied";
      else if (!the_mesh.sensorTelemetry.rows())
        status = the_mesh.sensorTelemetry.invalid() ? "Unsupported data" : "No telemetry";
      if (status) { display.drawTextEllipsized(0, y, display.width(), status); return; }
      // Wrap long values instead of dropping their last digits. Scroll uses
      // physical lines, including wrapped values, on either display size.
      int rows = the_mesh.sensorTelemetry.rows();
      auto telemetryLines = [&](int max_width, bool draw) {
        int line = 0;
        char text[64];
        for (int i = 0; i < rows + (the_mesh.sensorTelemetry.invalid() ? 1 : 0); i++) {
          if (i == rows) snprintf(text, sizeof(text), "Unsupported data follows");
          else the_mesh.sensorTelemetry.format(i, text, sizeof(text));
          char* start = text;
          while (*start) {
            int n = 1;
            while (start[n]) {
              char saved = start[n + 1]; start[n + 1] = 0;
              bool fits = display.getTextWidth(start) <= max_width;
              start[n + 1] = saved;
              if (!fits) break;
              n++;
            }
            char saved = start[n]; start[n] = 0;
            if (draw && line >= _row && line < _row + visible)
              display.drawTextLeftAlign(0, y + (line - _row) * step, start);
            start[n] = saved;
            start += n;
            line++;
          }
        }
        return line;
      };
      _lines = telemetryLines(display.width(), false);
      int reserve = scrollIndicatorReserve(display, _lines, visible);
      if (reserve) _lines = telemetryLines(display.width() - reserve, false);
      int max_row = _lines > visible ? _lines - visible : 0;
      if (_row > max_row) _row = max_row;
      telemetryLines(display.width() - reserve, true);
      drawScrollIndicator(display, display.width(), y, visible * step,
                          _lines, visible, _row);
    } else {
      int n = count();
      if (!n) { display.drawTextLeftAlign(0, y, "No sensor nodes"); return; }
      if (_selected >= n) _selected = n - 1;
      drawListAt(display, y, n, _selected, _scroll,
          [&](int index, int row_y, bool selected, int reserve) {
        ContactInfo contact;
        if (!node(index, contact)) return;
        char name[33]; memcpy(name, contact.name, 32); name[32] = 0;
        drawRowSelection(display, row_y, selected, reserve);
        display.drawTextEllipsized(2, row_y, display.width() - 4 - reserve,
                                   name, selected);
      });
    }
  }
};
