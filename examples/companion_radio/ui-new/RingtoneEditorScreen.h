#pragma once
// Custom screen — not part of upstream UITask.cpp
// Included by UITask.cpp after the global KB_* constants are defined.

#include "../solo/RingtoneModel.h"

class RingtoneEditorScreen : public UIScreen {
  UITask* _task;
  NodePrefs* _prefs;
  static const int MAX_NOTES = solo::RingtoneModel::MAX_NOTES;
  enum MenuIdx { MI_PLAY, MI_SWITCH, MI_DURATION, MI_BPM, MI_INSERT, MI_DELETE, MI_SAVE, MI_DISCARD };
  enum ConfirmAction { CA_NONE, CA_EXIT, CA_SWITCH };

  int _visible_notes = 7;
  uint8_t _notes[MAX_NOTES], _initial_notes[MAX_NOTES];
  uint8_t _len, _initial_len, _bpm_idx, _initial_bpm_idx;
  int _slot, _cursor, _scroll;
  bool _legacy_truncated;
  PopupMenu _menu, _confirm;
  ConfirmAction _confirm_action;
  char _play_buf[220], _menu_play_label[8], _menu_slot_label[24];
  char _menu_dur_label[18], _menu_bpm_label[12];

  bool dirty() const {
    return _legacy_truncated || _len != _initial_len || _bpm_idx != _initial_bpm_idx ||
           memcmp(_notes, _initial_notes, sizeof(_notes)) != 0;
  }
  void snapshot() {
    memcpy(_initial_notes, _notes, sizeof(_notes));
    _initial_len = _len;
    _initial_bpm_idx = _bpm_idx;
    _legacy_truncated = false;
  }
  void clampScroll() {
    if (_cursor < _scroll) _scroll = _cursor;
    if (_cursor >= _scroll + _visible_notes) _scroll = _cursor - _visible_notes + 1;
    if (_scroll < 0) _scroll = 0;
  }
  void previewNote(uint8_t note) {
    if (!solo::RingtoneModel::pitchIndex(note)) { _task->stopMelody(); return; }
    solo::RingtoneModel::buildRTTTL(&note, 1, 4, _play_buf, sizeof(_play_buf));
    _task->playMelody(_play_buf);
  }
  void saveCurrent() {
    if (!_prefs || !dirty()) return;
    uint8_t* stored = _slot ? _prefs->ringtone2_notes : _prefs->ringtone_notes;
    memset(stored, 0, solo::RingtoneModel::STORAGE_NOTES);
    memcpy(stored, _notes, _len);
    if (_slot) {
      _prefs->ringtone2_bpm_idx = _bpm_idx;
      _prefs->ringtone2_len = _len;
    } else {
      _prefs->ringtone_bpm_idx = _bpm_idx;
      _prefs->ringtone_len = _len;
    }
    the_mesh.savePrefs();
    snapshot();
  }
  void leaveEditor() { _task->stopMelody(); _task->gotoToolsScreen(); }
  void confirmExit() {
    if (!dirty()) { leaveEditor(); return; }
    _confirm_action = CA_EXIT;
    _confirm.beginConfirm("Discard changes?", "Discard");
  }
  void confirmSwitch() {
    if (!dirty()) { selectSlot(1 - _slot); return; }
    _confirm_action = CA_SWITCH;
    _confirm.begin("Save changes?", 3);
    _confirm.addItem("Save & Switch");
    _confirm.addItem("Discard");
    _confirm.addItem("Cancel");
    _confirm.setSelected(2);
  }

public:
  RingtoneEditorScreen(UITask* task, NodePrefs* prefs)
    : _task(task), _prefs(prefs), _len(0), _initial_len(0), _bpm_idx(2),
      _initial_bpm_idx(2), _slot(0), _cursor(0), _scroll(0),
      _legacy_truncated(false), _confirm_action(CA_NONE) {
    memset(_notes, 0, sizeof(_notes));
    memset(_initial_notes, 0, sizeof(_initial_notes));
  }
  void onHide() override { _task->stopMelody(); }

  // Preference slots retain their historical 32-byte layout. Editing exposes
  // the first 16 notes; saving clears the inactive legacy tail.
  void selectSlot(int slot = 0) {
    _slot = slot == 1 ? 1 : 0;
    uint8_t bpm = _prefs ? (_slot ? _prefs->ringtone2_bpm_idx : _prefs->ringtone_bpm_idx) : 2;
    _bpm_idx = bpm < solo::RingtoneModel::BPM_COUNT ? bpm : 2;
    uint8_t stored_len = _prefs ? (_slot ? _prefs->ringtone2_len : _prefs->ringtone_len) : 0;
    _len = stored_len > MAX_NOTES ? MAX_NOTES : stored_len;
    memset(_notes, 0, sizeof(_notes));
    if (_prefs && _len) memcpy(_notes, _slot ? _prefs->ringtone2_notes : _prefs->ringtone_notes, _len);
    _cursor = _scroll = 0;
    _menu.active = _confirm.active = false;
    _confirm_action = CA_NONE;
    snapshot();
    _legacy_truncated = stored_len > MAX_NOTES;
  }

  void openMenu() {
    snprintf(_menu_play_label, sizeof(_menu_play_label), "%s", _task->isMelodyPlaying() ? "Stop" : "Play");
    int other = 1 - _slot;
    uint8_t other_len = _prefs ? (other ? _prefs->ringtone2_len : _prefs->ringtone_len) : 0;
    snprintf(_menu_slot_label, sizeof(_menu_slot_label), "Custom%d%s", other + 1, other_len ? "" : " (empty)");
    uint8_t duration = _cursor < _len ? solo::RingtoneModel::duration(_notes[_cursor]) : 0;
    snprintf(_menu_dur_label, sizeof(_menu_dur_label), "Duration: %s", solo::RingtoneModel::durationLabel(duration));
    snprintf(_menu_bpm_label, sizeof(_menu_bpm_label), "BPM: %u", solo::RingtoneModel::bpm(_bpm_idx));
    _menu.begin("Options", 5);
    _menu.addItem(_menu_play_label);
    _menu.addItem(_menu_slot_label);
    _menu.addValueItem(_menu_dur_label);
    _menu.addValueItem(_menu_bpm_label);
    _menu.addItem("Insert");
    _menu.addItem("Delete");
    _menu.addItem("Save & Exit");
    _menu.addItem("Discard");
  }

  int render(DisplayDriver& display) override {
    display.setTextSize(1);
    display.setColor(DisplayDriver::LIGHT);
    int lh = display.getLineHeight(), cw = display.getCharWidth();
    int cell_w = cw * 3 + 5, notes_y = display.listStart(), cell_h = lh + 6;
    _visible_notes = display.width() / cell_w;
    if (_visible_notes < 1) _visible_notes = 1;
    clampScroll();
    char header[32];
    snprintf(header, sizeof(header), "Custom%d%s %u %d/%d", _slot + 1, dirty() ? "*" : "",
             solo::RingtoneModel::bpm(_bpm_idx), _len, MAX_NOTES);
    display.drawCenteredHeader(header, true, _menu.active);
    for (int i = 0; i < _visible_notes; i++) {
      int ni = _scroll + i, x = i * cell_w;
      bool selected = ni == _cursor;
      if (ni < _len) {
        char label[5];
        solo::RingtoneModel::label(_notes[ni], label, sizeof(label));
        display.drawSelectionRow(x, notes_y, cell_w - 1, cell_h, selected);
        display.setCursor(x + 2, notes_y + 3); display.print(label);
        display.setColor(DisplayDriver::LIGHT);
      } else if (ni == _len && _len < MAX_NOTES) {
        display.drawSelectionRow(x, notes_y, cell_w - 1, cell_h, selected);
        display.setCursor(x + (cell_w - cw) / 2, notes_y + 3); display.print("+");
        display.setColor(DisplayDriver::LIGHT);
      }
    }
    int info_y = notes_y + cell_h + 2;
    if (_scroll > 0) { display.setCursor(0, info_y); display.print("<"); }
    if (_scroll + _visible_notes <= _len) { display.setCursor(display.width() - cw, info_y); display.print(">"); }
    display.setCursor(cw + 2, info_y);
    if (_cursor < _len) {
      char info[24];
      snprintf(info, sizeof(info), "oct:%u dur:%s", solo::RingtoneModel::octave(_notes[_cursor]),
               solo::RingtoneModel::durationLabel(solo::RingtoneModel::duration(_notes[_cursor])));
      display.drawTextEllipsized(cw + 2, info_y, display.width() - cw * 2 - 4, info);
    } else {
      display.drawTextEllipsized(cw + 2, info_y, display.width() - cw * 2 - 4,
                                 _len ? "Up/Down: Add note" : "Empty: Up/Down adds");
    }
    display.drawTextEllipsized(0, display.height() - display.lineStep(), display.width(),
                               "Enter: Oct Hold: Menu");
    if (_menu.active) _menu.render(display);
    if (_confirm.active) _confirm.render(display);
    return UI_REFRESH_STATIC_MS;
  }

  bool handleInput(char c) override {
    if (_confirm.active) {
      PopupMenu::Result result = _confirm.handleInput(c);
      if (result == PopupMenu::SELECTED) {
        int choice = _confirm.selectedIndex();
        ConfirmAction action = _confirm_action;
        _confirm_action = CA_NONE;
        if (action == CA_EXIT && choice == 0) leaveEditor();
        else if (action == CA_SWITCH && choice == 0) { saveCurrent(); selectSlot(1 - _slot); }
        else if (action == CA_SWITCH && choice == 1) selectSlot(1 - _slot);
      } else if (result == PopupMenu::CANCELLED) _confirm_action = CA_NONE;
      return true;
    }
    bool up = c == KEY_UP, down = c == KEY_DOWN;
    bool left = keyIsPrev(c), right = keyIsNext(c), enter = c == KEY_ENTER;
    if (_menu.active) {
      int selected = _menu.selectedIndex();
      if (left || right) {
        if (selected == MI_DURATION && _cursor < _len) {
          uint8_t value = solo::RingtoneModel::duration(_notes[_cursor]);
          value = right ? (value + 1) % solo::RingtoneModel::DURATION_COUNT
                        : (value + solo::RingtoneModel::DURATION_COUNT - 1) % solo::RingtoneModel::DURATION_COUNT;
          _notes[_cursor] = solo::RingtoneModel::withDuration(_notes[_cursor], value);
          snprintf(_menu_dur_label, sizeof(_menu_dur_label), "Duration: %s", solo::RingtoneModel::durationLabel(value));
        } else if (selected == MI_BPM) {
          if (right && _bpm_idx + 1 < solo::RingtoneModel::BPM_COUNT) _bpm_idx++;
          else if (left && _bpm_idx) _bpm_idx--;
          snprintf(_menu_bpm_label, sizeof(_menu_bpm_label), "BPM: %u", solo::RingtoneModel::bpm(_bpm_idx));
        }
        return true;
      }
      PopupMenu::Result result = _menu.handleInput(c);
      if (result == PopupMenu::SELECTED) {
        switch ((MenuIdx)_menu.selectedIndex()) {
          case MI_PLAY:
            if (_task->isMelodyPlaying()) _task->stopMelody();
            else if (_len) { solo::RingtoneModel::buildRTTTL(_notes, _len, _bpm_idx, _play_buf, sizeof(_play_buf)); _task->playMelody(_play_buf); }
            break;
          case MI_SWITCH: confirmSwitch(); break;
          case MI_DURATION: case MI_BPM: break;
          case MI_INSERT:
            if (_len < MAX_NOTES) {
              int at = _cursor < _len ? _cursor + 1 : _cursor;
              for (int i = _len; i > at; i--) _notes[i] = _notes[i - 1];
              _notes[at] = solo::RingtoneModel::pack(1, 5, 1);
              _len++; _cursor = at; clampScroll();
            }
            break;
          case MI_DELETE:
            if (_cursor < _len) {
              for (int i = _cursor; i + 1 < _len; i++) _notes[i] = _notes[i + 1];
              _notes[--_len] = 0;
              if (_cursor >= _len && _len) _cursor = _len - 1;
              else if (!_len) _cursor = 0;
              clampScroll();
            }
            break;
          case MI_SAVE: saveCurrent(); leaveEditor(); break;
          case MI_DISCARD: confirmExit(); break;
        }
      }
      return true;
    }
    if (c == KEY_CANCEL) { confirmExit(); return true; }
    if (c == KEY_CONTEXT_MENU) { openMenu(); return true; }
    if (left && _cursor > 0) { _cursor--; clampScroll(); return true; }
    if (right) {
      int maximum = _len < MAX_NOTES ? _len : _len - 1;
      if (_cursor < maximum) { _cursor++; clampScroll(); return true; }
    }
    if ((up || down || enter) && _cursor == _len && _len < MAX_NOTES) {
      _notes[_len++] = solo::RingtoneModel::pack(1, 5, 1);
      clampScroll(); previewNote(_notes[_cursor]); return true;
    }
    if ((up || down) && _cursor < _len) {
      uint8_t pitch = solo::RingtoneModel::pitchIndex(_notes[_cursor]);
      pitch = up ? (pitch + 1) % solo::RingtoneModel::PITCH_COUNT
                 : (pitch + solo::RingtoneModel::PITCH_COUNT - 1) % solo::RingtoneModel::PITCH_COUNT;
      _notes[_cursor] = solo::RingtoneModel::withPitch(_notes[_cursor], pitch);
      previewNote(_notes[_cursor]); return true;
    }
    if (enter && _cursor < _len) {
      uint8_t octave = solo::RingtoneModel::octave(_notes[_cursor]);
      if (solo::RingtoneModel::pitchIndex(_notes[_cursor])) octave = octave < 7 ? octave + 1 : 4;
      _notes[_cursor] = solo::RingtoneModel::withOctave(_notes[_cursor], octave);
      previewNote(_notes[_cursor]); return true;
    }
    return false;
  }
};
