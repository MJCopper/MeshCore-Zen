#pragma once

#include <stdint.h>
#include <stddef.h>

namespace zen {

// A presentation-only view of the existing MeshCore flood-scope precedence.
class FloodScopeView {
public:
  enum State : uint8_t { UNSCOPED, DEFAULT, APP_OVERRIDE, APP_UNSCOPED };

  static State state(bool forced_unscoped, bool temporary_key, bool default_key) {
    if (forced_unscoped) return APP_UNSCOPED;
    if (temporary_key) return APP_OVERRIDE;
    return default_key ? DEFAULT : UNSCOPED;
  }

  // The on-device editor accepts a region name, not the leading '#' used
  // when deriving its transport key. Empty text clears the saved default.
  static bool normaliseName(const char* input, char* output, size_t size) {
    if (!input || !output || size < 2) return false;
    if (*input == '#') input++;
    size_t len = 0;
    while (input[len]) {
      unsigned char c = (unsigned char)input[len];
      if (c < 32 || c > 126 || c == '#' || c == '*') return false;
      if (len + 1 >= size) return false;
      output[len] = (char)c;
      len++;
    }
    if (len && (output[0] == ' ' || output[len - 1] == ' ')) return false;
    output[len] = '\0';
    return true;
  }
};

} // namespace zen
