inline KeyboardWidget::Result KeyboardWidget::handleInputImpl(char c) {
    if (_emoji_picker.active()) {
      const char* emoji = _emoji_picker.handleInput(c);
      if (emoji) insertUtf8(emoji);
      return NONE;
    }
    // placeholder overlay consumes all input
    if (_ph_menu.active) {
      bool selecting_t9_prediction = predictiveT9Active();
      auto res = _ph_menu.handleInput(c);
      if (res == PopupMenu::SELECTED) {
        int idx = _ph_menu.selectedIndex();
        if (_ph_t9_actions) {
          // Both actions discard the unmatched digit sequence. Spell keeps the
          // editor on the literal abc page so the user can enter the missing
          // word without changing their saved T9 preference.
          resetT9Prediction();
          if (idx == 0) {
            page = 0;
            _t9_literal_mode = true;
          }
          _ph_t9_actions = false;
          return NONE;
        }
        const char* ph = _suggestions.items[idx];
        char sentence_ph[KB_PH_LEN];
        // Contextual (refresh-hook) fields complete the in-progress word --
        // the text since the last space, up to the cursor -- instead of
        // appending after it, so picking a match doesn't duplicate what's
        // already been typed. Anything after the cursor (if it's not at the
        // end) shifts along with the insertion, same as a normal keystroke.
        int replace_start = cursor_pos;
        int replace_end = cursor_pos;
        if (_completion.hasRefresh()) {
          replace_start = _suggestions.range_set ? _suggestions.replace_start : cursor_pos;
          replace_end = _suggestions.range_set ? _suggestions.replace_end : cursor_pos;
          if (!_suggestions.range_set)
            while (replace_start > 0 && buf[replace_start - 1] != ' ') replace_start--;
        }
        if (_sentence_case_enabled && ph[0] >= 'a' && ph[0] <= 'z' &&
            zen::SentenceCase::shouldCapitalize(buf, (size_t)replace_start)) {
          snprintf(sentence_ph, sizeof(sentence_ph), "%s", ph);
          sentence_ph[0] = zen::SentenceCase::apply(
              sentence_ph[0], buf, (size_t)replace_start);
          ph = sentence_ph;
        }
        int ph_len = strlen(ph);
        int tail_len = len - replace_end;
        // Message word completions leave the cursor ready for the next word.
        // Do not duplicate a separator already present after the replaced word,
        // and do not append spaces to dynamic placeholders such as {loc}.
        bool append_space = _suggestions.append_space && ph_len > 0 && ph[0] != '{' &&
                            (replace_end == len || buf[replace_end] != ' ');
        if (append_space && replace_start + ph_len + 1 + tail_len > max_len)
          append_space = false;  // accept a final full-length word without its separator
        char replacement[KB_PH_LEN + 1];
        snprintf(replacement, sizeof(replacement), "%s%s", ph,
                 append_space ? " " : "");
        replace(replace_start, replace_end, replacement);
        if (selecting_t9_prediction) {
          if (!_t9_suggest) zen::T9Predictor::remember(ph);
          resetT9Prediction();
        }
      }
      return NONE;
    }

    // Cursor-positioning sub-mode (see the KEY_UP block below): the grid
    // selection is parked while LEFT/RIGHT walk cursor_pos one codepoint at a
    // time. UP/DOWN jump to the very start/end -- Home/End, in effect -- and,
    // once already at that boundary, continue the wrap the entry trigger
    // interrupted: UP again lands on the special row, DOWN again back on the
    // letter grid's row 0, same destinations the plain grid wrap used to reach
    // directly (see the entry/exit comment below). Enter/Cancel just leave the
    // mode from anywhere; the actual edit (insert/backspace) happens back in
    // normal typing, now targeting the repositioned cursor.
    if (cursor_mode) {
      if (c == KEY_LEFT)  { movePrevious(); return NONE; }
      if (c == KEY_RIGHT) { moveNext(); return NONE; }
      if (c == KEY_UP) {
        if (cursor_pos > 0) { cursor_pos = 0; return NONE; }
        cursor_mode = false;
        row = gridRows();
        col = col * KB_SPECIAL / gridCols();
        return NONE;
      }
      if (c == KEY_DOWN) {
        if (cursor_pos < len) { cursor_pos = len; return NONE; }
        cursor_mode = false;
        row = 0;
        return NONE;
      }
      // KEY_KB_ENTER (external keyboard's Fn+Enter) leaves the mode too rather
      // than being silently eaten -- it's the one key an external-keyboard
      // typist would reach for here, and cursor mode is only ever entered from
      // a physical button, so without this it looks like a dead key.
      if (c == KEY_ENTER || c == KEY_CANCEL || c == KEY_KB_ENTER) { cursor_mode = false; return NONE; }
      return NONE;
    }

    if (c == KEY_DOUBLE_CANCEL) {
#if ZEN_FEATURE_AUTOCOMPLETE
      // Double Back only exits when there is no active predictive word. A
      // recognised word is completed with sentence punctuation; an unmatched
      // digit sequence opens the same recovery list as single Back instead of
      // unexpectedly cancelling the editor.
      if (predictiveT9Active()) {
        if (!acceptT9WordAndSentence() && _t9_no_match) openPlaceholders();
        return NONE;
      }
      if (acceptPreviewCompletion(". ")) return NONE;
#endif
      c = KEY_CANCEL;  // outside active predictive input, retain normal Back
    }

    if (c == KEY_CANCEL) {
#if ZEN_FEATURE_AUTOCOMPLETE
      // In predictive T9, Back is the missing dedicated phone-keypad 0 key:
      // accept the current word and insert its separator without driving the
      // grid down to Space and back. Outside an active word it remains Cancel.
      if (acceptT9WordAndSpace()) return NONE;
      if (acceptPreviewCompletion(" ")) return NONE;
      if (predictiveT9Active() && _t9_no_match) {
        openPlaceholders();
        return NONE;
      }
#endif
      return CANCELLED;
    }

    // Direct-typing passthrough (CardKB or similar literal-ASCII input
    // source, see UITask::pollCardKB()). Printable characters insert
    // straight at the cursor, bypassing the on-screen grid entirely -- no
    // caps re-application, the source already sends the correct case.
    // Backspace deletes the previous character. KEY_KB_ENTER submits the
    // field directly -- pollCardKB() emits it for Fn+Enter always, and for
    // plain Enter too when Compact mode's plain grid state applies (there's
    // no grid cell to commit there). Every other plain Enter (Full mode, or
    // Compact but mid popup/cursor-move) arrives here as ordinary KEY_ENTER
    // and falls through to the grid dispatch below instead, so there's still
    // no ambiguity to resolve at this layer -- pollCardKB() already decided.
    if (c == KEY_KB_ENTER) return DONE;
    if (c == 0x08) {
      t9_cell = -1;   // invalidate any pending T9 cycle -- see the grid paths below
      if (backspaceT9Prediction()) return NONE;
      commitT9Prediction();
      erasePrevious();
      return NONE;
    }
    if (c >= 0x20 && c <= 0x7E) {
      commitT9Prediction();
      insertTyped(c);
      return NONE;
    }

    const int rows = gridRows();
    const int cols = gridCols();

    // In a completion-enabled message field Hold-Enter is a direct shortcut to the
    // same completion/placeholder dialogue as the {} cell. It is deliberately
    // independent of the highlighted grid cell: the held physical button is a
    // text action here, not a long-press action on that cell.
    //
    // Elsewhere Hold-Enter is normally "cancel", with cell-specific
    // exceptions for Shift, Backspace and the emoji picker.
    if (c == KEY_CONTEXT_MENU) {
#if ZEN_FEATURE_AUTOCOMPLETE
      if (_predictive_t9_enabled && openPlaceholders()) return NONE;
#endif
      if (row == rows && col == 0) {          // Shift
        caps_lock = !caps_lock;
        caps = caps_lock;
        return NONE;
      }
      if (row == rows && col == 2) {          // Backspace
        clear();
        t9_cell = -1;
        commitT9Prediction();
        return NONE;
      }
      if (row == rows && col == 3 && openEmojiPicker()) return NONE;
      if (row < rows) return NONE;
      return CANCELLED;
    }

    if (c == KEY_UP) {
      if (row > 0) {
        row--;
        if (row == rows - 1)  // leaving special row upward
          col = col * cols / KB_SPECIAL;
      } else {
        // row 0: enter cursor mode instead of wrapping to the special row --
        // row/col are deliberately left as-is so the cursor-mode UP/DOWN
        // continuation above can still reach the special row proportionally.
        cursor_mode = true;
        t9_cell = -1;
        commitT9Prediction();
        return NONE;
      }
      t9_cell = -1;   // navigating away finalizes any pending multi-tap cycle
      return NONE;
    }
    if (c == KEY_DOWN) {
      if (row < rows) {
        row++;
        if (row == rows)  // entering special row
          col = col * KB_SPECIAL / cols;
      } else {
        row = 0;                        // wrap down onto the first char row
        col = col * cols / KB_SPECIAL;
      }
      t9_cell = -1;
      return NONE;
    }
    if (c == KEY_LEFT) {
      int max_col = (row == rows) ? KB_SPECIAL - 1 : cols - 1;
      col = (col > 0) ? col - 1 : max_col;
      t9_cell = -1;
      return NONE;
    }
    if (c == KEY_RIGHT) {
      int max_col = (row == rows) ? KB_SPECIAL - 1 : cols - 1;
      col = (col < max_col) ? col + 1 : 0;
      t9_cell = -1;
      return NONE;
    }
    if (c == KEY_ENTER) {
      // By the time a plain KEY_ENTER reaches here, it's a real grid
      // interaction -- repeat a letter, cycle T9, switch page/script, reach
      // the special row's DONE cell -- same as a physical button. CardKB's
      // own Enter is only ever KEY_ENTER when that's true (see pollCardKB()'s
      // KEY_KB_ENTER cases just above), so there's no ambiguity here.
      if (row < rows && isT9()) {
        int cell = row * cols + col;
        // Predictive T9 uses the classic letter keys 2-9. Key 1 remains a
        // punctuation/digit multi-tap key, as do every key on the symbols page
        // and the explicit `abc` fallback page.
        if (predictiveT9Ready() && cell > 0) {
          appendT9Digit((char)('1' + cell));
          return NONE;
        }
        commitT9Prediction();
        const char* group = t9GroupStr(cell);
        int glen = kbUtf8Len(group);       // codepoint count, not byte length
        int total = glen + 1;   // + the cell's own digit, at the end of the cycle
        bool cycling = (t9_cell == cell) && (millis() - t9_last_ms < KB_T9_TIMEOUT_MS);
        if (cycling) {
          t9_cycle = (t9_cycle + 1) % total;
          if (cursor_pos > 0) {
            char one[5];
            if (t9_cycle < glen) kbUtf8CharAt(group, t9_cycle, one);
            else { one[0] = (char)('1' + cell); one[1] = '\0'; }
            char shown[5];
            kbApplyCapsUtf8(one, t9_caps, shown, sizeof(shown));
            // Replace the codepoint just before the cursor (what the previous
            // tap inserted), preserving anything after the cursor too.
            int old_n = kbUtf8LastCharBytes(buf, cursor_pos);
            int new_pos = cursor_pos - old_n;
            int tail_len = len - cursor_pos;
            int n = (int)strlen(shown);
            if (new_pos + n + tail_len <= max_len) {
              memmove(buf + new_pos + n, buf + cursor_pos, tail_len);
              memcpy(buf + new_pos, shown, n);
              len = new_pos + n + tail_len;
              cursor_pos = new_pos + n;
              buf[len] = '\0';
            }
          }
        } else if (len < max_len) {
          char one[5]; kbUtf8CharAt(group, 0, one);
          char shown[5]; kbApplyCapsUtf8(one, caps, shown, sizeof(shown));
          if (insert(shown)) {
            t9_cell = cell;
            t9_cycle = 0;
            t9_caps = caps;   // remember it for every later cycling tap on this cell
            if (caps && !caps_lock) caps = false;   // one-shot: only this first tap gets capitalised
          }
        }
        t9_last_ms = millis();
      } else if (row < rows) {
        insertGlyph(cellStr(row, col), caps);
        if (caps && !caps_lock) caps = false;   // one-shot: revert after the letter it capitalised
      } else {
        t9_cell = -1;   // any special-row action finalizes a pending multi-tap cycle
        switch (col) {
          // Tap toggles one-shot caps on/off; while caps_lock is held (Hold-Enter
          // on this key, see handleInput's top), a tap cancels the lock instead.
          case 0: if (caps_lock) { caps = false; caps_lock = false; } else { caps = !caps; } break;
          case 1:
            commitT9Prediction();
            insert(" ");
            break;
          case 2:
            if (backspaceT9Prediction()) break;
            commitT9Prediction();
            erasePrevious();
            break;
          case 3:
            openEmojiPicker();
            break;
          case 4:
            commitT9Prediction();
#if ZEN_FEATURE_AUTOCOMPLETE
            if (_predictive_t9_enabled && isT9()) {
              // Predictive letters -> symbols -> literal multi-tap ->
              // predictive letters. This gives out-of-dictionary words a
              // local escape hatch without changing the saved keyboard mode.
              if (page == 0 && !_t9_literal_mode) {
                page = 1;
              } else if (pageIsSymbols(page)) {
                page = 0;
                _t9_literal_mode = true;
              } else {
                page = 0;
                _t9_literal_mode = false;
              }
            } else
#endif
            {
              page = (page + 1) % totalPages();
            }
            break;
          case 5:
            commitT9Prediction();
            return DONE;
        }
      }
    }
    return NONE;
  }
