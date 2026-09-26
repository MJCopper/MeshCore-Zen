#pragma once

#include "../zen/SensorAccessCoordinator.h"

// Remote sensors share the home card's header/navigation. Only opening a node
// or explicitly refreshing sends a request; browsing does not poll the radio.
class SensorPage {
  UITask* _task;
  int _selected = 0, _scroll = 0, _row = 0, _lines = 0;
  bool _open = false;
  bool _error_logged = false;
  zen::SensorAccessCoordinator _access;
  uint8_t _key[PUB_KEY_SIZE] = {};
  char _name[33] = {};
  zen::OperationStatus _status;

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
  bool requestTelemetry(bool new_sequence = true, bool force_flood = false) {
    _row = 0;
    if (new_sequence) _error_logged = false;
    ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
    if (new_sequence && _task->remoteNode().active()) {
      if (_task->remoteNode().owner() == zen::RemoteNodeCoordinator::SENSOR_OWNER) {
        _task->remoteNode().cancel(zen::RemoteNodeCoordinator::SENSOR_OWNER, _key);
        the_mesh.cancelSensorTelemetry();
      } else {
        _access.telemetrySendFailed();
        _task->operationError(zen::Operation::TELEMETRY,
            zen::OperationOutcome::BUSY, zen::OperationReason::REQUEST_ACTIVE,
            &_status);
        _error_logged = true;
        return false;
      }
    }
    uint32_t timeout = 0;
    bool sent = the_mesh.requestSensorTelemetry(_key, &timeout, force_flood);
    if (sent && new_sequence) {
      _access.beginTelemetry(contact && contact->out_path_len != OUT_PATH_UNKNOWN,
                             _key, zen::RemoteNodeOperation::responseDeadline(
                                 millis(), timeout));
      sent = _task->remoteNode().begin(zen::RemoteNodeCoordinator::SENSOR_OWNER,
          zen::RemoteNodeOperation::TELEMETRY, _key,
          contact && contact->out_path_len != OUT_PATH_UNKNOWN,
          zen::RemoteNodeOperation::RETRY_SAFE,
          zen::RemoteNodeOperation::responseDeadline(millis(), timeout));
    } else if (sent) {
      _access.telemetryStarted(zen::RemoteNodeOperation::responseDeadline(
          millis(), timeout));
      sent = _task->remoteNode().restart(zen::RemoteNodeCoordinator::SENSOR_OWNER,
          zen::RemoteNodeOperation::responseDeadline(millis(), timeout));
    }
    if (sent) _task->publishRemoteOperation(
        zen::Operation::TELEMETRY, zen::RemoteNodeOperation::PENDING);
    if (!sent) {
      _task->remoteNode().fail(zen::RemoteNodeCoordinator::SENSOR_OWNER,
                               zen::RemoteNodeOperation::SEND_FAILED);
      _access.telemetrySendFailed();
      the_mesh.cancelSensorTelemetry();
      _task->operationError(zen::Operation::TELEMETRY,
          contact ? zen::OperationOutcome::TRANSPORT_FAILURE
                  : zen::OperationOutcome::NOT_FOUND,
          contact ? zen::OperationReason::SEND_FAILED
                  : zen::OperationReason::NODE_UNAVAILABLE, &_status);
      _error_logged = true;
    }
    return sent;
  }
  bool startLogin(const char* password, bool entered_password) {
    ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
    bool sent = contact && _task->startNodeLogin(
        zen::NodeLoginCoordinator::SENSOR, *contact, password, false);
    if (sent) _access.loginStarted(entered_password);
    else {
      _access.loginStartFailed(entered_password);
      _task->operationError(zen::Operation::SENSOR_LOGIN,
          _task->nodeLoginBusy() ? zen::OperationOutcome::BUSY
                                 : zen::OperationOutcome::TRANSPORT_FAILURE,
          _task->nodeLoginBusy() ? zen::OperationReason::LOGIN_BUSY
                                 : zen::OperationReason::SEND_FAILED, &_status);
    }
    return sent;
  }

public:
  explicit SensorPage(UITask* task) : _task(task) {}
  bool passwordEditing() const { return _open && _access.passwordEditing(); }
  int renderPassword(ZenDisplayDriver& display) { return _task->keyboard().render(display); }
  void tick() {
    if (!_open) return;
    if (_task->remoteNode().expired(zen::RemoteNodeCoordinator::SENSOR_OWNER,
                                    millis())) {
      ContactInfo* contact = the_mesh.lookupContactByPubKey(_key, PUB_KEY_SIZE);
      zen::RemoteNodeOperation::Action retry = _task->remoteNode().timeout(
          zen::RemoteNodeCoordinator::SENSOR_OWNER, millis(),
          contact && contact->out_path_len != OUT_PATH_UNKNOWN);
      zen::SensorAccessCoordinator::Action action = _access.telemetryTimedOut(retry);
      if (action == zen::SensorAccessCoordinator::START_BLANK_LOGIN) {
        the_mesh.cancelSensorTelemetry();
        startLogin("", false);
      } else if (action == zen::SensorAccessCoordinator::SEND_TELEMETRY) {
        requestTelemetry(false);
      } else if (action == zen::SensorAccessCoordinator::CLEAR_PATH_AND_SEND_TELEMETRY) {
        requestTelemetry(false, true);
      }
      if (action == zen::SensorAccessCoordinator::NONE &&
          _access.phase() == zen::SensorAccessCoordinator::ERROR) {
        the_mesh.failSensorTelemetry();
        _task->publishRemoteOperation(zen::Operation::TELEMETRY,
            _task->remoteNode().operation().result(), &_status);
      }
    }
    if (_access.tick(millis()) == zen::SensorAccessCoordinator::SEND_TELEMETRY)
      requestTelemetry();
    if (_access.phase() == zen::SensorAccessCoordinator::ERROR && !_error_logged) {
      _task->operationError(zen::Operation::TELEMETRY,
          zen::OperationOutcome::TIMEOUT, zen::OperationReason::RETRIES_EXHAUSTED,
          &_status);
      _error_logged = true;
    }
  }
  void onLoginResult(const uint8_t* key, bool success) {
    if (!_open || memcmp(_key, key, PUB_KEY_SIZE) != 0 ||
        (_access.phase() != zen::SensorAccessCoordinator::ACL_WAIT &&
         _access.phase() != zen::SensorAccessCoordinator::PASSWORD_WAIT)) return;
    bool password_entered = _access.phase() == zen::SensorAccessCoordinator::PASSWORD_WAIT;
    if (success) {
      _access.loginSucceeded(millis());
    } else {
      _access.loginFailed(password_entered);
      if (password_entered) {
        _task->operationError(zen::Operation::SENSOR_LOGIN,
            zen::OperationOutcome::REJECTED, zen::OperationReason::LOGIN_REJECTED,
            &_status);
        _task->keyboard().beginProfile(zen::EditorProfile::CREDENTIAL, "", 15);
      }
    }
  }
  void onLoginTimeout(const uint8_t* key) {
    if (!_open || memcmp(_key, key, PUB_KEY_SIZE) != 0) return;
    if (_access.phase() == zen::SensorAccessCoordinator::ACL_WAIT) {
      _access.loginFailed(false);
      _task->operationError(zen::Operation::SENSOR_LOGIN,
          zen::OperationOutcome::TIMEOUT, zen::OperationReason::NO_ACL_REPLY,
          &_status);
    } else if (_access.phase() == zen::SensorAccessCoordinator::PASSWORD_WAIT) {
      _access.loginFailed(true);
      _task->operationError(zen::Operation::SENSOR_LOGIN,
          zen::OperationOutcome::TIMEOUT, zen::OperationReason::NO_PASSWORD_REPLY,
          &_status);
      _task->keyboard().beginProfile(zen::EditorProfile::CREDENTIAL, "", 15);
    }
  }
  void close() {
    if (_open) _task->cancelNodeLogin(zen::NodeLoginCoordinator::SENSOR, _key);
    _open = false;
    _error_logged = false;
    _access.reset();
    _status.clear();
    _row = 0;
    the_mesh.cancelSensorTelemetry();
    _task->remoteNode().cancel(zen::RemoteNodeCoordinator::SENSOR_OWNER, _key);
  }
  bool handleInput(char c) {
    if (_open) {
      if (c == KEY_CANCEL) { close(); return true; }
      if (_access.phase() == zen::SensorAccessCoordinator::PASSWORD) {
        auto result = _task->keyboard().handleInput(c);
        if (result == KeyboardWidget::DONE) {
          char password[16];
          snprintf(password, sizeof(password), "%s", _task->keyboard().buf);
          startLogin(password, true);
          _task->keyboard().beginProfile(zen::EditorProfile::CREDENTIAL, "", 15);
        }
        else if (result == KeyboardWidget::CANCELLED) _access.cancelPassword();
        return true;
      }
      if (_access.phase() == zen::SensorAccessCoordinator::LOGIN_OFFER && c == KEY_ENTER) {
        _task->keyboard().beginProfile(zen::EditorProfile::CREDENTIAL, "", 15);
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
        _status.clear();
        _access.reset();
        requestTelemetry();
      }
      return true;
    }
    return false;
  }
  void render(ZenDisplayDriver& display, int y) {
    display.setColor(ZenDisplayDriver::LIGHT);
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
      if (_access.phase() == zen::SensorAccessCoordinator::ACL_WAIT ||
          _access.phase() == zen::SensorAccessCoordinator::PASSWORD_WAIT) {
        static char login_status[28];
        if (_task->remoteStatus(zen::RemoteNodeCoordinator::LOGIN_OWNER,
                                login_status, sizeof(login_status)))
          status = login_status;
        else status = _access.phase() == zen::SensorAccessCoordinator::ACL_WAIT
                          ? "Checking ACL..." : "Logging in...";
      }
      else if (_access.phase() == zen::SensorAccessCoordinator::RETRY_DELAY) status = "Access confirmed...";
      else if (_access.phase() == zen::SensorAccessCoordinator::LOGIN_OFFER) status = "Enter: Login";
      else if (_access.phase() == zen::SensorAccessCoordinator::ERROR ||
               (_access.phase() == zen::SensorAccessCoordinator::AUTH_TELEMETRY_WAIT && state == MyMesh::SENSOR_FAILED))
        status = _status.available ? _status.text() : "No reply after retries";
      else if (state == MyMesh::SENSOR_WAITING) {
        static char remote_status[28];
        if (_task->remoteStatus(zen::RemoteNodeCoordinator::SENSOR_OWNER,
                                remote_status, sizeof(remote_status)))
          status = remote_status;
        else status = "Requesting...";
      }
      else if (state == MyMesh::SENSOR_FAILED)
        status = _status.available ? _status.text() : "No reply after retries";
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
