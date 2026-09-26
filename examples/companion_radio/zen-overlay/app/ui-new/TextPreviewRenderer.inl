inline void KeyboardWidget::renderTextPreview(
    ZenDisplayDriver& display, const KeyboardLayout& layout, bool compact_ui,
    bool t9_no_match_hint, int prev_lines, const char* preview_suffix) {
  const int lh = layout.line_height;
  const int cw = layout.char_width;
  const int sep_y = layout.separator_y;
  // Multi-line text preview: the view follows cursor_pos (normally == len,
  // i.e. the end — so this is identical to the old "always the last line"
  // behaviour until cursor mode moves cursor_pos elsewhere, at which point
  // the preview scrolls to keep the repositioned cursor in view).
  // Line breaks are counted in CODEPOINTS, not bytes: cpl is how many
  // characters physically fit, so dividing byte offsets by it would count a
  // multi-byte character as more than one -- reducing the usable
  // line width and, worse, letting a break land inside a codepoint, which
  // reaches print() as a truncated sequence and draws as garbage (both
  // display drivers decode UTF-8 directly). Everything else in this widget already
  // works in codepoints via the kbUtf8*() helpers; this was the last
  // byte-based holdout.
  int cpl = display.width() / cw;  // chars per preview line
  if (cpl < 1) cpl = 1;
  if (cpl > KB_PREVIEW_CAP) cpl = KB_PREVIEW_CAP;  // never overrun linebuf below
  // Which preview line the cursor sits on = how many whole codepoints precede it.
  int cursor_chars = 0;
  for (int p = 0; p < cursor_pos; ) { p += kbUtf8CharBytesAt(buf, p, len); cursor_chars++; }
  int cursor_line = cursor_chars / cpl;
  int first_line  = (cursor_line >= prev_lines) ? (cursor_line - prev_lines + 1) : 0;
  // ...and the byte offset that line starts at.
  int ps = 0;
  for (int n = first_line * cpl; n > 0 && ps < len; n--) ps += kbUtf8CharBytesAt(buf, ps, len);
  bool cursor_drawn = false;
  for (int pl = 0; pl < prev_lines; pl++) {
    int pe = ps;   // byte offset cpl codepoints further along (or end of text)
    int line_chars = 0;
    while (line_chars < cpl && pe < len) {
      pe += kbUtf8CharBytesAt(buf, pe, len);
      line_chars++;
    }
    // End-of-text belongs on the current row while it still has room. Only
    // move the cursor to the following row when this one is exactly full.
    bool cursor_here = !cursor_drawn && ps <= cursor_pos &&
        (cursor_pos < pe ||
         (cursor_pos == pe && pe == len && line_chars < cpl) ||
         (pl == prev_lines - 1 && cursor_pos >= pe));
    if (cursor_here) cursor_drawn = true;
    int line_end = (len < pe) ? len : pe;
    char linebuf[KB_PREVIEW_BYTES + 2];   // cpl codepoints + cursor '_' + NUL
    if (cursor_here) {
      // Cursor drawn as an inserted '_' between whatever text precedes and
      // follows it on this line -- reduces to the old "text + trailing _"
      // when cursor_pos == len (after_n is always 0 in that case).
      int before_n = cursor_pos - ps;        if (before_n < 0) before_n = 0;
      if (before_n > line_end - ps) before_n = line_end - ps;
      int after_n  = line_end - cursor_pos;  if (after_n < 0) after_n = 0;
      snprintf(linebuf, sizeof(linebuf), "%.*s_%.*s", before_n, buf + ps, after_n, buf + ps + before_n);
    } else if (len > ps) {
      snprintf(linebuf, sizeof(linebuf), "%.*s", line_end - ps, buf + ps);
    } else {
      linebuf[0] = '\0';
    }
    display.setCursor(0, pl * lh);
    display.print(linebuf);
    // The full on-screen keyboard previews the untyped remainder after the
    // cursor. CardKB's compact editor keeps the text area literal and shows
    // its completion only in the dedicated Tab hint below.
    if (!compact_ui && cursor_here && preview_suffix[0]) {
      char before[KB_PREVIEW_BYTES + 1];
      int before_n = cursor_pos - ps;
      if (before_n < 0) before_n = 0;
      if (before_n > line_end - ps) before_n = line_end - ps;
      snprintf(before, sizeof(before), "%.*s", before_n, buf + ps);
      int ghost_x = display.getTextWidth(before) + cw;
      int room = (display.width() - ghost_x) / cw;
      if (room > 0) {
        char ghost[KB_PH_LEN];
        snprintf(ghost, sizeof(ghost), "%.*s", room, preview_suffix);
        display.setCursor(ghost_x, pl * lh);
        display.print(ghost);
      }
    }
    ps = pe;
  }
  if (t9_no_match_hint) {
    display.setCursor(0, prev_lines * lh);
    display.print(_t9_capacity_limited ? "Text full" : "No match - Hold Enter");
  }
  display.fillRect(0, sep_y, display.width(), display.sepH());
}
