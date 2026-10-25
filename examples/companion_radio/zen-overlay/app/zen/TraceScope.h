#pragma once
#include <stdint.h>
#include <string.h>

namespace zen {
// Value-owned scope identity. Wire codes are only useful within one ciphertext.
struct TraceScope {
  enum Kind : uint8_t { UNSCOPED, RESOLVED, UNRESOLVED };
  uint8_t key[16]{};
  uint16_t codes[2]{};
  Kind kind = UNSCOPED;
  uint8_t route_type = 0;
  bool reusable() const { return kind != UNRESOLVED; }
  bool equals(const TraceScope& other) const {
    if (kind != other.kind) return false;
    if (kind == RESOLVED) return memcmp(key, other.key, sizeof(key)) == 0;
    return kind == UNSCOPED || (route_type == other.route_type &&
        memcmp(codes, other.codes, sizeof(codes)) == 0);
  }
};
}
