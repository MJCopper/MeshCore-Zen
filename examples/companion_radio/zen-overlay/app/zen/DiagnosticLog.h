#pragma once

#include <stdint.h>
#include <string.h>
#include "OperationResult.h"

namespace zen {

// Small, RAM-only record of operational failures. Consecutive identical
// failures are folded into one entry so a noisy background task cannot evict
// every other useful diagnostic. Nothing is persisted to flash.
class DiagnosticLog {
public:
  static const uint8_t CAPACITY = 16;
  enum Severity : uint8_t { INFO, WARNING, ERROR };
  struct Entry {
    uint32_t timestamp;
    uint32_t occurred_ms;
    OperationResult result;
    uint8_t count;
  };

private:
  Entry _entries[CAPACITY]{};
  uint8_t _head = 0;
  uint8_t _size = 0;

public:
  uint8_t size() const { return _size; }
  void clear() { memset(_entries, 0, sizeof(_entries)); _head = _size = 0; }

  static Severity severity(const OperationResult& result) {
    return OperationResultCatalog::isError(result.outcome) ? ERROR :
           (OperationResultCatalog::isWarning(result.outcome) ? WARNING : INFO);
  }

  bool add(uint32_t timestamp, uint32_t occurred_ms,
           const OperationResult& result) {
    if (!OperationResultCatalog::shouldLog(result)) return false;
    if (_size) {
      Entry& last = _entries[(_head + CAPACITY - 1) % CAPACITY];
      if (last.result.operation == result.operation &&
          last.result.outcome == result.outcome &&
          last.result.reason == result.reason &&
          !memcmp(&last.result.context, &result.context, sizeof(result.context))) {
        last.timestamp = timestamp;
        last.occurred_ms = occurred_ms;
        if (last.count < 255) last.count++;
        return false;
      }
    }
    Entry& entry = _entries[_head];
    entry.timestamp = timestamp;
    entry.occurred_ms = occurred_ms;
    entry.result = result;
    entry.count = 1;
    _head = (_head + 1) % CAPACITY;
    if (_size < CAPACITY) _size++;
    return true;
  }

  // index 0 is the newest event.
  const Entry* newest(uint8_t index) const {
    if (index >= _size) return nullptr;
    return &_entries[(_head + CAPACITY - 1 - index) % CAPACITY];
  }
};

} // namespace zen
