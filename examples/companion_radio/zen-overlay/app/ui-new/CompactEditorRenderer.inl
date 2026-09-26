inline void KeyboardWidget::renderCompactEditor(
    ZenDisplayDriver& display, const KeyboardLayout& layout, bool has_completion) {
  const int cw = layout.char_width;
  const int chars_y = layout.characters_y;
  const auto& completion = _completion.preview();
  display.setColor(ZenDisplayDriver::LIGHT);
  if (has_completion) {
    char hint[sizeof(completion.candidates) + 6];
    snprintf(hint, sizeof(hint), "Tab: %s", completion.candidates);
    // The compact hint owns one physical row. Clip at the display's
    // character capacity even when that cuts through the final word: this
    // exposes more useful candidates than dropping the whole last item.
    int hint_chars = display.width() / cw;
    if (hint_chars < 0) hint_chars = 0;
    if (hint_chars < (int)sizeof(hint)) hint[hint_chars] = '\0';
    display.drawTextCentered(display.width() / 2, chars_y, hint);
  } else {
    display.drawTextCentered(display.width() / 2, chars_y, "Tab: ----");
  }
}
