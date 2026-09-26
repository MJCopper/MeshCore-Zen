#pragma once

#include <helpers/ui/ZenDisplayDriver.h>

// Geometry shared by full, compact and cursor-mode text editor renderers.
// Keeping it data-only makes OLED/E-ink bounds independently testable.
struct KeyboardLayout {
  int line_height;
  int char_width;
  int cell_width;
  int preview_lines;
  int separator_y;
  int characters_y;
  int cell_height;
  int special_y;
  int special_width;

  static KeyboardLayout calculate(int width, int height, int separator_height,
                                  int line_height, int char_width, int rows,
                                  int columns, int special_count, bool compact,
                                  bool cursor_mode) {
    KeyboardLayout layout;
    layout.line_height = line_height;
    layout.char_width = char_width;
    layout.cell_width = width / columns;
    int keyboard_height = compact
        ? (cursor_mode ? 4 * layout.line_height : layout.line_height)
        : (rows + 1) * layout.line_height;
    int preview_height = height - keyboard_height - separator_height;
    layout.preview_lines = preview_height / layout.line_height;
    if (layout.preview_lines < 1) layout.preview_lines = 1;
    layout.separator_y = layout.preview_lines * layout.line_height;
    layout.characters_y = layout.separator_y + separator_height;
    layout.cell_height = (height - layout.characters_y) / (rows + 1);
    layout.special_y = layout.characters_y + rows * layout.cell_height;
    layout.special_width = width / special_count;
    return layout;
  }

  static KeyboardLayout calculate(const ZenDisplayDriver& display, int rows,
                                  int columns, int special_count,
                                  bool compact, bool cursor_mode) {
    return calculate(display.width(), display.height(), display.sepH(),
                     display.getLineHeight(), display.getCharWidth(), rows,
                     columns, special_count, compact, cursor_mode);
  }
};
