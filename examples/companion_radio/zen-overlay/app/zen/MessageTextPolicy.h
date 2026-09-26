#pragma once

#include <stddef.h>
#include <string.h>
#include <helpers/UTF8Helpers.h>

namespace zen {

// UI text budgets mirror MeshCore's wire format without changing app sends.
struct MessageTextPolicy {
  static size_t limit(size_t wire_limit, const char* channel_sender = nullptr) {
    size_t overhead = channel_sender ? strlen(channel_sender) + 2 : 2;
    return overhead < wire_limit ? wire_limit - overhead : 0;
  }

  static void trim(char* text, size_t max_bytes) {
    text[mesh::validUtf8PrefixLength(text, max_bytes)] = '\0';
  }
};

} // namespace zen
