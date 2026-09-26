inline void KeyboardWidget::renderKeyboardGrid(
    ZenDisplayDriver& display, const KeyboardLayout& layout, int rows, int cols) {
  const int lh = layout.line_height;
  const int cell_w = layout.cell_width;
  const int chars_y = layout.characters_y;
  const int cell_h = layout.cell_height;
  const int spec_y = layout.special_y;
  const int spec_w = layout.special_width;
  // character grid
  if (isT9()) {
    for (int r = 0; r < rows; r++) {
      int y = chars_y + r * cell_h;
      for (int c = 0; c < cols; c++) {
        bool sel = (row == r && col == c);
        int cell = r * cols + c;
        // Label the cell "<digit><group>" so it reads like a phone keypad.
        // The digit is what the multi-tap cycle lands on after the letters.
        char group_shown[12];
        kbApplyCapsUtf8(t9GroupStr(cell), caps, group_shown, sizeof(group_shown));
        char label[14];
        snprintf(label, sizeof(label), "%c%s", (char)('1' + cell), group_shown);
        int cx = c * cell_w;
        display.drawSelectionRow(cx, y - 1, cell_w - 1, cell_h, sel);
        int tw = display.getTextWidth(label);
        display.setCursor(cx + (cell_w - tw) / 2, y);
        display.print(label);
      }
    }
  } else {
    for (int r = 0; r < rows; r++) {
      int y = chars_y + r * cell_h;
      for (int c = 0; c < cols; c++) {
        bool sel = (row == r && col == c);
        char ch_buf[3];
        kbApplyCapsUtf8(cellStr(r, c), caps, ch_buf, sizeof(ch_buf));
        if (ch_buf[0] == ' ' && ch_buf[1] == '\0') ch_buf[0] = '_';
        int cx = c * cell_w;
        display.drawSelectionRow(cx, y - 1, cell_w - 1, cell_h, sel);
        int tw = display.getTextWidth(ch_buf);
        display.setCursor(cx + (cell_w - tw) / 2, y);
        display.print(ch_buf);
      }
    }
  }

  // special row: caps ⇧ · space ⎵ · delete ⌫ · emoji 🙂 · page · OK ✓
  const int s   = miniIconScale(display);
  const int icy = spec_y + (cell_h - lh) / 2;   // centre icons within the cell
  for (int i = 0; i < KB_SPECIAL; i++) {
    bool sel    = (row == rows && col == i);
    bool active = (i == 0 && caps);
    int sx = i * spec_w;
    display.drawSelectionRow(sx, spec_y - 1, spec_w - 1, cell_h, sel || active);
    if (i == 3 || i == 4) {               // text keys: emoji picker, page toggle
      // Shows what pressing it lands on next, same "reads as the
      // destination" convention as the original 2-page abc<->#@ toggle,
      // generalized to however many pages are in the cycle right now.
      const char* lbl;
      if (i == 3) {
        lbl = "\xF0\x9F\x99\x82";       // U+1F642 slightly smiling face
      } else {
#if ZEN_FEATURE_AUTOCOMPLETE
        if (_predictive_t9_enabled && isT9()) {
          if (page == 0 && !_t9_literal_mode) lbl = "#@";
          else if (pageIsSymbols(page))      lbl = "abc";
          else                               lbl = "T9";
        } else
#endif
        {
          int next = (page + 1) % totalPages();
          lbl = pageIsSymbols(next) ? "#@" : "abc";
        }
      }
      int tw = display.getTextWidth(lbl);
      display.setCursor(sx + (spec_w - tw) / 2, spec_y);
      display.print(lbl);
    } else if (i == 1) {                  // space ⎵ — two halves side by side
      int icw = (ICON_SPACE_L.w + ICON_SPACE_R.w) * s;
      int ix  = sx + (spec_w - icw) / 2;
      miniIconDraw(display, ix, icy, ICON_SPACE_L);
      miniIconDraw(display, ix + ICON_SPACE_L.w * s, icy, ICON_SPACE_R);
    } else {
      const MiniIcon& ic = (i == 0) ? ICON_SHIFT
                         : (i == 2) ? ICON_BACKSPACE
                                    : ICON_CHECK;   // i == 5 → OK
      int ix = sx + (spec_w - ic.w * s) / 2;
      miniIconDraw(display, ix, icy, ic);
      // Underline the ⇧ icon while caps_lock is held. Without it the two
      // Shift states are indistinguishable -- caps_lock sets caps too, so
      // the highlight above is identical -- even though they behave
      // completely differently (one letter vs. every following letter).
      // caps_lock implies the cell is filled, so the current (inverted)
      // ink colour is the one that shows against it.
      if (i == 0 && caps_lock) display.fillRect(sx + 2, spec_y + cell_h - 3, spec_w - 5, 1);
    }
    display.setColor(ZenDisplayDriver::LIGHT);
  }

}
