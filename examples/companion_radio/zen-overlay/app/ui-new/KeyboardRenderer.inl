inline int KeyboardWidget::render(ZenDisplayDriver& display) {
    _visible = true;

    // The keyboard renders directly in the shared UI font.
    display.setTextSize(1);
    display.setColor(ZenDisplayDriver::LIGHT);

    const int rows = gridRows();
    const int cols = gridCols();
    bool compact_ui = isCompact();
    // The normal compact editor needs only the Tab hint row, placing the
    // separator and hint as low as the display allows.
    // Cursor mode temporarily restores the old four-line region so its
    // multi-line controls remain fully visible.
    const KeyboardLayout layout = KeyboardLayout::calculate(
        display, rows, cols, KB_SPECIAL, compact_ui, cursor_mode);
    const int lh = layout.line_height;
    const int preview_lines = layout.preview_lines;
    // Only a failed lookup needs an instruction row. Normal predictive entry
    // uses the extra line for message text; Back/alternative-word behaviour
    // remains available without permanently advertising it on-screen.
    const bool t9_no_match_hint = (_t9_capacity_limited || (predictiveT9Active() && _t9_no_match)) &&
                                  !compact_ui && preview_lines > 1;
    const int prev_lines = t9_no_match_hint ? preview_lines - 1 : preview_lines;
    const int chars_y = layout.characters_y;

    bool has_completion = refreshCompletionPreview();
    const auto& completion = _completion.preview();
    const char* preview_suffix = predictiveT9Active() ? t9GhostSuffix()
                                                      : completion.suffix;

    renderTextPreview(display, layout, compact_ui, t9_no_match_hint,
                      prev_lines, preview_suffix);

    // Cursor-positioning mode: LEFT/RIGHT/UP/DOWN now drive the text cursor
    // instead of the grid (see handleInput), so the grid would otherwise just
    // sit there frozen with no sign anything's different. Replace it with an
    // explicit hint instead.
    if (cursor_mode) {
      const int hh = lh + 2;
      display.setColor(ZenDisplayDriver::LIGHT);
      display.fillRect(0, chars_y, display.width(), hh);
      display.setColor(ZenDisplayDriver::DARK);
      display.drawTextCentered(display.width() / 2, chars_y + 1, "CURSOR MODE");
      display.setColor(ZenDisplayDriver::LIGHT);
      display.drawTextCentered(display.width() / 2, chars_y + hh + 2, "L/R move");
      display.drawTextCentered(display.width() / 2, chars_y + hh + 2 + lh, "Up/Down: Start/End");
      return 50;
    }

    // A connected external keyboard automatically selects the compact view: its
    // typist never looks at the letter grid or special-row icons, so skip
    // drawing them entirely -- no status line either, since nothing it could
    // show (script/page, T9-vs-ABC, caps) is actually actionable from CardKB:
    // typing is always plain ASCII regardless of keyboard_type
    // (direct-typing passthrough, see UITask::pollCardKB()), and caps-lock has no
    // CardKB gesture to toggle it at all. Just the two shortcuts that still do
    // something here (arrows/Enter are self-explanatory -- cursor movement and
    // submit -- so they get no hint of their own).
    // Physical buttons (if used
    // instead of/alongside CardKB) still drive row/col/page as normal; it
    // just won't be visible on this screen which cell is selected.
    if (compact_ui)
      renderCompactEditor(display, layout, has_completion);
    else
      renderKeyboardGrid(display, layout, rows, cols);

    // placeholder picker overlay (drawn on top of keyboard)
    if (_ph_menu.active) _ph_menu.render(display);
    _emoji_picker.render(display);
    return 50;
  }
