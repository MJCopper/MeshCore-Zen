#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mesh {

enum TracePathPosition { TRACE_PATH_INVALID, TRACE_PATH_NEXT, TRACE_PATH_END };

// TRACE stores one SNR byte per completed hop in packet.path. Its remaining
// payload is a sequence of equal-sized repeater hashes.
inline TracePathPosition getTracePathPosition(uint8_t snr_count, uint8_t flags,
                                              size_t path_bytes) {
  size_t hash_size = 1u << (flags & 0x03);
  if (path_bytes % hash_size != 0) return TRACE_PATH_INVALID;
  size_t offset = (size_t)snr_count * hash_size;
  if (offset == path_bytes) return TRACE_PATH_END;
  return offset + hash_size <= path_bytes ? TRACE_PATH_NEXT : TRACE_PATH_INVALID;
}

}  // namespace mesh
