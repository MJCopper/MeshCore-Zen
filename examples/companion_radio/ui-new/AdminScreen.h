#pragma once

// Node List > Hold Enter > Admin. UI only: command definitions and transport
// ownership live in solo/AdminCommands and solo/AdminSession respectively.
#include "FullscreenMsgView.h"
#include "RadioParamsEditor.h"
#include "../solo/AdminCommands.h"
#include "ConsoleEditorSupport.h"
#include <helpers/ClientACL.h>

class AdminScreen : public UIScreen {
  UITask* _task;
  enum Phase { LOGIN, ROOT, LIST, WAIT, EDIT, REPLY };
  Phase _phase = LOGIN;
  ContactInfo _target;
  bool _login_waiting = false;
  bool _login_used_password = false;
  bool _authenticated = false;
  enum Pending { PENDING_NONE, FETCH_SETTING, APPLY_SETTING, VERIFY_SETTING, RUN_COMMAND };
  Pending _pending = PENDING_NONE;
  bool _console = false;
  bool _text_edit = false;
  solo::admin::Group _group = solo::admin::STATUS;
  const solo::admin::Field* _field = nullptr;
  int _sel = 0, _scroll = 0;
  uint8_t _items[solo::admin::FIELD_COUNT];
  int _count = 0;
  uint32_t _deadline = 0;
  char _original[161]{};
  char _value[161]{};
  char _reply[201]{};
  const char* _reply_title = "Reply";
  bool _neighbours_reply = false;
  int _neighbour_count = 0;
  PopupMenu _confirm;
  FullscreenMsgView _view;
  DigitEditor _frequency;
  float _number = 0, _freq = 0, _bw = 0;
  uint8_t _sf = 0, _cr = 0;

  KeyboardWidget& kb() { return _task->keyboard(); }
  bool allowed() const { return !_task->isChildModeLocked(); }
  solo::admin::TargetKind targetKind() const {
    if (_target.type == ADV_TYPE_ROOM) return solo::admin::TARGET_ROOM;
    if (_target.type == ADV_TYPE_SENSOR) return solo::admin::TARGET_SENSOR;
    return solo::admin::TARGET_REPEATER;
  }

  void root() { _phase = ROOT; _sel = _scroll = 0; _field = nullptr; }
  void leave() {
    closeSession();
    _task->returnFromAdmin();
  }
  void showReply(const char* title, const char* text) {
    _neighbours_reply = false;
    _neighbour_count = 0;
    _reply_title = title;
    StrHelper::strncpy(_reply, text, sizeof(_reply));
    _view.begin(false);
    _phase = REPLY;
  }
  static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  }
  // Preserve the raw rows so the selected name can marquee independently of
  // the fixed age and signal columns, matching the Discover list.
  void showNeighbours(const char* text) {
    StrHelper::strncpy(_reply, text, sizeof(_reply));
    _neighbour_count = _reply[0] ? 1 : 0;
    for (const char* p = _reply; *p; p++) if (*p == '\n' && p[1]) _neighbour_count++;
    _reply_title = "Neighbours";
    _neighbours_reply = true;
    _sel = _scroll = 0;
    _phase = REPLY;
  }
  bool neighbourRow(int index, char* name, size_t name_size,
                    char* metrics, size_t metrics_size) {
    const char* line = _reply;
    while (index-- > 0 && line) {
      line = strchr(line, '\n');
      if (line) line++;
    }
    if (!line || !*line || !name_size || !metrics_size) return false;
    const char* end = strchr(line, '\n');
    size_t len = end ? (size_t)(end - line) : strlen(line);
    uint8_t prefix[4];
    bool valid = len >= 9 && line[8] == ':';
    for (int i = 0; i < 4 && valid; i++) {
      int hi = hexNibble(line[i * 2]), lo = hexNibble(line[i * 2 + 1]);
      if (hi < 0 || lo < 0) valid = false;
      else prefix[i] = (uint8_t)((hi << 4) | lo);
    }
    unsigned long age = 0;
    int snr = 0, parsed = 0;
    valid = valid && sscanf(line + 9, "%lu:%d%n", &age, &snr, &parsed) == 2 &&
        parsed == (int)len - 9 && age <= UINT32_MAX &&
        snr >= INT8_MIN && snr <= INT8_MAX;
    if (!valid) {
      size_t copy = len < name_size - 1 ? len : name_size - 1;
      memcpy(name, line, copy); name[copy] = 0;
      metrics[0] = 0;
      return true;
    }
    ContactInfo* known = the_mesh.lookupContactByPubKey(prefix, sizeof(prefix));
    if (known && known->name[0]) StrHelper::strncpy(name, known->name, name_size);
    else {
      size_t copy = name_size > 9 ? 8 : name_size - 1;
      memcpy(name, line, copy); name[copy] = 0;
    }
    return solo::admin::formatNeighbourMetrics(metrics, metrics_size,
                                                (uint32_t)age, snr);
  }
  int renderNeighbours(DisplayDriver& d) {
    d.drawCenteredHeader("Neighbours");
    drawList(d, _neighbour_count, _sel, _scroll,
        [&](int i, int y, bool selected, int reserve) {
      char name[64], metrics[20];
      if (!neighbourRow(i, name, sizeof(name), metrics, sizeof(metrics))) return;
      drawRowSelection(d, y, selected, reserve);
      int right = d.width() - reserve - 2;
      int metric_width = d.getTextWidth(metrics);
      int name_right = metrics[0] ? right - metric_width - 4 : right;
      int name_width = name_right - 2;
      if (name_width < d.getCharWidth()) name_width = d.getCharWidth();
      d.drawTextEllipsized(2, y, name_width, name, selected);
      if (metrics[0]) d.drawTextRightAlign(right, y, metrics);
    });
    return UI_REFRESH_STATIC_MS;
  }
  void login() {
    ContactInfo* current = the_mesh.lookupContactByPubKey(_target.id.pub_key, PUB_KEY_SIZE);
    bool password_supplied = kb().buf[0] != 0;
    bool busy = _task->nodeLoginBusy();
    bool sent = current && allowed() && !busy && _task->startNodeLogin(
        solo::NodeLoginCoordinator::ADMIN, *current, kb().buf, false);
    kb().begin("", 15);  // no credential remains in the shared editor
    kb().clearPlaceholders();
    _login_waiting = sent;
    if (sent) {
      _login_used_password = password_supplied;
    } else if (!current) {
      _task->logFailure("Admin login", "Node unavailable");
    } else if (busy) {
      _task->logFailure("Admin login", "Login busy");
    } else {
      _task->logFailure("Admin login", "Send failed");
    }
  }
  void send(const char* command, Pending pending) {
    uint32_t timeout;
    if (!allowed() || !_authenticated ||
        !the_mesh.sendAdminCommand(_target, command, timeout)) {
      _task->logFailure("Admin", "Command not sent");
      showReply(pending == VERIFY_SETTING ? "Not verified" : "Not sent",
                "Busy or send failed");
      return;
    }
    _pending = pending;
    _deadline = millis() + timeout + 4000;
    _phase = WAIT;
  }
  void buildList() {
    _count = 0;
    for (int i = 0; i < solo::admin::FIELD_COUNT; i++)
      if (solo::admin::FIELDS[i].group == _group) _items[_count++] = i;
    _phase = LIST;
    _sel = _scroll = 0;
  }
  void openText(const char* value, int max_len) {
    kb().begin(value, max_len);
    kb().clearPlaceholders();  // CLI values are literal, not chat templates
    if (_console) consoleeditor::enable(kb());
    _text_edit = true;
    _phase = EDIT;
  }
  bool beginEdit(const char* value) {
    using namespace solo::admin;
    StrHelper::strncpy(_value, value, sizeof(_value));
    // Strip line endings, but preserve meaningful spaces in text values.
    size_t n = strlen(_value);
    while (n && (_value[n - 1] == '\r' || _value[n - 1] == '\n')) _value[--n] = 0;
    _text_edit = false;
    if (_field->kind == TEXT) {
      if (n > (size_t)_field->max) return false;
      openText(_value, (int)_field->max);
    } else if (radioTuple(_field->kind)) {
      if (!parseRadio(_value, _freq, _bw, _sf, _cr)) return false;
      formatRadio(_value, sizeof(_value), _freq, _bw, _sf, _cr);
      _frequency.begin(_freq, 150, 2500, 4, 3);
    } else if (_field->kind == TOGGLE) {
      if (strcmp(_value, "on") && strcmp(_value, "off")) return false;
      _number = strcmp(_value, "on") == 0 ? 1 : 0;
    } else {
      if (!number(_value, _field->min, _field->max, _number)) return false;
      const char* format = _field->step < 0.01f ? "%.3f" :
                           (_field->step < 1 ? "%.1f" : "%.0f");
      snprintf(_value, sizeof(_value), format, _number);
    }
    strcpy(_original, _value);
    _phase = EDIT;
    return true;
  }
  void formatValue() {
    using namespace solo::admin;
    if (_text_edit) StrHelper::strncpy(_value, kb().buf, sizeof(_value));
    else if (radioTuple(_field->kind)) {
      if (_field->kind == FREQUENCY) _freq = _frequency.value;
      formatRadio(_value, sizeof(_value), _freq, _bw, _sf, _cr);
    } else if (_field->kind == TOGGLE) strcpy(_value, _number ? "on" : "off");
    else {
      const char* format = _field->step < 0.01f ? "%.3f" :
                           (_field->step < 1 ? "%.1f" : "%.0f");
      snprintf(_value, sizeof(_value), format, _number);
    }
  }
  void reviewEdit() {
    formatValue();
    if (!_console && !strcmp(_original, _value)) { _phase = LIST; return; }
    if (_console && !_value[0]) { root(); return; }
    _confirm.begin(_console ? "Send command?" : "Save change?");
    if (_console) {
      _confirm.addItem("Cancel");
      _confirm.addItem("Send");
      _confirm.addItem("Discard");
    } else {
      _confirm.addItem("No");
      _confirm.addItem("Yes");
    }
  }
  void activate() {
    _field = &solo::admin::FIELDS[_items[_sel]];
    _console = false;
    if (_field->kind == solo::admin::ACTION) {
      _confirm.begin(_field->label);
      _confirm.addItem("Cancel");
      _confirm.addItem(!strcmp(_field->get, "start ota") ? "Start" : "Confirm");
    } else if (_field->kind == solo::admin::WRITE_TEXT) {
      _original[0] = 0;
      openText("", (int)_field->max);
    } else send(_field->get, FETCH_SETTING);
  }

public:
  explicit AdminScreen(UITask* task) : _task(task) {}
  void onShow() override {
    _phase = LOGIN;
    _pending = PENDING_NONE;
    _authenticated = _login_waiting = _login_used_password = false;
    _confirm.active = false;
    _neighbours_reply = false;
    _neighbour_count = 0;
    _field = nullptr;
    _original[0] = _value[0] = _reply[0] = 0;
  }
  void startFor(const ContactInfo& contact) {
    _target = contact;
    kb().begin("", 15);
    kb().clearPlaceholders();
    login();  // empty password asks the remote node to check its ACL first
  }
  void closeSession() {
    _task->cancelNodeLogin(solo::NodeLoginCoordinator::ADMIN, _target.id.pub_key);
    the_mesh.closeAdminSession();
    _authenticated = _login_waiting = _login_used_password = false;
    _pending = PENDING_NONE;
    _confirm.active = false;
    memset(_original, 0, sizeof(_original));
    memset(_value, 0, sizeof(_value));
    memset(_reply, 0, sizeof(_reply));
    _neighbours_reply = false;
    _neighbour_count = 0;
    kb().begin("", 15);
  }
  void onHide() override { closeSession(); }
  void onNodeLoginResult(const uint8_t* key, bool success, uint8_t permissions) {
    if (!_login_waiting || _phase != LOGIN || memcmp(key, _target.id.pub_key, 4)) return;
    _login_waiting = false;
    if (!allowed()) { leave(); return; }
    if (solo::NodeLoginResponse::grantsAdmin(
          success, permissions, _login_used_password)) {
      _authenticated = true;
      the_mesh.authorizeAdmin(_target.id.pub_key);
      root();
    } else {
      _task->logFailure("Admin login", success ? "Not Admin" : "Rejected");
      kb().begin("", 15);
    }
  }
  void onNodeLoginTimeout(const uint8_t* key) {
    if (_phase != LOGIN || !_login_waiting || memcmp(key, _target.id.pub_key, 4)) return;
    _login_waiting = false;
    _task->logFailure("Admin login",
                      _login_used_password ? "No password reply" : "No ACL reply");
    kb().begin("", 15);
    kb().clearPlaceholders();
  }
  void onAdminReply(const uint8_t* key, const char* text) {
    if (_phase != WAIT || !allowed() || memcmp(key, _target.id.pub_key, PUB_KEY_SIZE)) return;
    Pending completed = _pending;
    _pending = PENDING_NONE;
    if (completed == FETCH_SETTING) {
      const char* value = solo::admin::value(text);
      if (_field && _field->kind == solo::admin::READ) {
        const char* read_value = solo::admin::readValue(text);
        if (_field->get && !strcmp(_field->get, "neighbors")) showNeighbours(read_value);
        else showReply(_field->label, read_value);
        return;
      }
      if (value && beginEdit(value)) return;
      _task->logFailure("Admin", "Invalid setting reply");
      showReply("Cannot edit", text);
    } else if (completed == APPLY_SETTING) {
      if (!solo::admin::confirmed(text)) {
        _task->logFailure("Admin", "Change rejected");
        showReply(!strncmp(text, "Error", 5) ? "Rejected" : "Not saved", text);
      } else if (_field && _field->get) {
        send(_field->get, VERIFY_SETTING);
      } else {
        // Password is deliberately write-only in MeshCore. Its OK response is
        // emitted after savePrefs(), so this is the strongest available check.
        showReply("Setting saved", "Confirmed by node");
      }
    } else if (completed == VERIFY_SETTING) {
      const char* actual = solo::admin::value(text);
      if (_field && actual && solo::admin::valuesEqual(*_field, _value, actual))
        showReply("Setting saved", "Verified on node");
      else {
        _task->logFailure("Admin", "Verify mismatch");
        showReply("Not verified", actual ? actual : text);
      }
    } else {
      if (!solo::admin::confirmed(text))
        _task->logFailure("Admin", !strncmp(text, "Error", 5) ? "Command rejected" : "Unexpected reply");
      showReply(solo::admin::confirmed(text) ? "Confirmed" :
          (!strncmp(text, "Error", 5) ? "Rejected" : "Reply"), text);
    }
  }
  void poll() override {
    if (_phase == WAIT && (int32_t)(millis() - _deadline) >= 0) {
      the_mesh.cancelAdminCommand();
      if (_pending == VERIFY_SETTING) {
        _task->logFailure("Admin", "Verify timeout");
        showReply("Not verified", "Read-back timed out");
      } else {
        _task->logFailure("Admin", "No command reply");
        showReply("No reply", "Result unknown.\nNot retried.\nWait before retrying.");
      }
      _pending = PENDING_NONE;
    }
  }
  int render(DisplayDriver& d) override {
    d.setTextSize(1);
    d.setColor(DisplayDriver::LIGHT);
    if (_phase == REPLY) {
      if (_neighbours_reply) return renderNeighbours(d);
      return _view.render(d, _reply_title, _reply, false, false);
    }
    if ((_phase == LOGIN && !_login_waiting) || (_phase == EDIT && _text_edit)) {
      int delay = kb().render(d);
      if (_confirm.active) _confirm.render(d);
      return delay;
    }
    d.drawCenteredHeader(_target.name);
    if (_phase == LOGIN || _phase == WAIT) {
      d.setCursor(2, d.listStart());
      d.print(_phase == LOGIN ? "Logging in..." : "Waiting for reply");
    } else if (_phase == EDIT) {
      d.setCursor(2, d.listStart());
      d.print(_field->label);
      int y = d.listStart() + d.lineStep();
      if (_field->kind == solo::admin::FREQUENCY) {
        drawRowSelection(d, y, true, 0);
        _frequency.render(d, 2, y);
      } else {
        char val[24];
        float n = _number;
        if (_field->kind == solo::admin::BANDWIDTH) n = _bw;
        if (_field->kind == solo::admin::SF) n = _sf;
        if (_field->kind == solo::admin::CR) n = _cr;
        if (_field->kind == solo::admin::TOGGLE) strcpy(val, n ? "On" : "Off");
        else snprintf(val, sizeof(val), "%g", n);
        d.setCursor(2, y); d.print(val);
      }
    } else {
      int count = _phase == ROOT ? solo::admin::groupCount(targetKind()) : _count;
      drawList(d, count, _sel, _scroll, [&](int i, int y, bool selected, int reserve) {
        drawRowSelection(d, y, selected, reserve);
        int group = _phase == ROOT ? solo::admin::groupAt(targetKind(), i) : 0;
        const char* label = _phase == ROOT ? solo::admin::GROUP_LABELS[group] :
            solo::admin::FIELDS[_items[i]].label;
        d.drawTextEllipsized(2, y, d.width() - reserve - 4, label, selected);
      });
    }
    if (_confirm.active) _confirm.render(d);
    return (_phase == LOGIN || _phase == WAIT) ? UI_REFRESH_ACTIVE_MS
                                                : UI_REFRESH_STATIC_MS;
  }
  bool handleInput(char c) override {
    if (!allowed()) { leave(); return true; }
    if (_confirm.active) {
      auto result = _confirm.handleInput(c);
      if (result != PopupMenu::NONE && _phase == EDIT && !_text_edit &&
          _field && _field->kind == solo::admin::FREQUENCY) _frequency.active = true;
      if (result == PopupMenu::SELECTED) {
        if (_confirm._sel == 1) {
          char command[161];
          if (_phase == LIST) send(_field->get, RUN_COMMAND);
          else if (_console) send(_value, RUN_COMMAND);
          else if (solo::admin::formatCommand(command, sizeof(command), _field->set, _value))
            send(command, APPLY_SETTING);
          else showReply("Not sent", "Value too long");
        } else if (_confirm._sel == 2) {
          if (_console) root(); else _phase = LIST;
        } else if (!_console && _phase == EDIT) {
          _phase = LIST;
        }
      }
      return true;
    }
    if (_phase == LOGIN) {
      if (_login_waiting) { if (c == KEY_CANCEL) leave(); }
      else {
        auto result = kb().handleInput(c);
        if (result == KeyboardWidget::DONE) login();
        else if (result == KeyboardWidget::CANCELLED) leave();
      }
      return true;
    }
    if (_phase == WAIT) {
      if (c == KEY_CANCEL) {
        the_mesh.cancelAdminCommand();
        showReply("Cancelled wait", "Command may have run.\nWait before retrying.");
      }
      return true;
    }
    if (_phase == REPLY) {
      if (_neighbours_reply) {
        if ((c == KEY_UP || keyIsPrev(c)) && _neighbour_count)
          _sel = (_sel + _neighbour_count - 1) % _neighbour_count;
        else if ((c == KEY_DOWN || keyIsNext(c)) && _neighbour_count)
          _sel = (_sel + 1) % _neighbour_count;
        else if (c == KEY_ENTER || c == KEY_CANCEL) {
          _neighbours_reply = false;
          _phase = LIST;
        }
        return true;
      }
      if (_view.handleInput(c) != FullscreenMsgView::NONE) {
        if (_console) root(); else _phase = LIST;
      }
      return true;
    }
    if (_phase == EDIT) {
      if (_text_edit) {
        auto result = kb().handleInput(c);
        if (result == KeyboardWidget::DONE || result == KeyboardWidget::CANCELLED) reviewEdit();
      } else if (_field->kind == solo::admin::FREQUENCY) {
        if (_frequency.handleInput(c) != DigitEditor::NONE) reviewEdit();
      } else if (c == KEY_ENTER || c == KEY_CANCEL) reviewEdit();
      else if (keyIsPrev(c) || keyIsNext(c)) {
        int dir = keyIsNext(c) ? 1 : -1;
        if (_field->kind == solo::admin::TOGGLE) _number = !_number;
        else if (_field->kind == solo::admin::BANDWIDTH) RadioParamsEditor::stepBW(_bw, dir);
        else if (_field->kind == solo::admin::SF) RadioParamsEditor::stepSF(_sf, dir);
        else if (_field->kind == solo::admin::CR) RadioParamsEditor::stepCR(_cr, dir);
        else {
          _number = solo::admin::stepNumber(*_field, _number, dir);
        }
      }
      return true;
    }
    int count = _phase == ROOT ? solo::admin::groupCount(targetKind()) : _count;
    if (c == KEY_CANCEL) { if (_phase == ROOT) leave(); else root(); }
    else if (c == KEY_UP && count) _sel = (_sel + count - 1) % count;
    else if (c == KEY_DOWN && count) _sel = (_sel + 1) % count;
    else if (c == KEY_ENTER && count) {
      if (_phase == LIST) activate();
      else {
        _group = solo::admin::groupAt(targetKind(), _sel);
        _console = _group == solo::admin::CONSOLE;
        if (_console) openText("", solo::AdminSession::TEXT_LIMIT); else buildList();
      }
    }
    return true;
  }
};
