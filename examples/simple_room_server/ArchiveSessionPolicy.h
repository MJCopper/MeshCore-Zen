#pragma once
#include <stdint.h>
#include <stddef.h>

struct ArchiveSessionPolicy {
  static uint8_t hops(bool flood, uint8_t incoming_count, uint8_t reverse_path) {
    return flood ? incoming_count : (reverse_path == 0xff ? 0 : reverse_path & 63);
  }
  template<class Post, size_t N>
  static uint32_t historyFloor(const Post (&posts)[N], uint32_t cutoff, uint8_t limit) {
    uint32_t cursor = cutoff, oldest = 0;
    bool include_cursor = true;
    for (uint8_t n = 0; n < limit; n++) {
      uint32_t next = 0;
      for (size_t i = 0; i < N; i++) {
        uint32_t timestamp = posts[i].post_timestamp;
        if (timestamp > next && (timestamp < cursor || (include_cursor && timestamp == cursor)))
          next = timestamp;
      }
      if (next == 0) return 0;
      oldest = next; cursor = next; include_cursor = false;
    }
    return oldest > 0 ? oldest - 1 : 0;
  }
  static bool inSnapshot(uint32_t timestamp, uint32_t since, uint32_t floor, uint32_t cutoff) {
    return timestamp > since && timestamp > floor && timestamp <= cutoff;
  }
};
