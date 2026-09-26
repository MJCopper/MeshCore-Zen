#pragma once
// A USB-CDC serial monitor or companion app can leave the port open (DTR
// asserted) without ever draining it. Stream::write() then blocks inside the
// platform's "TX FIFO full" wait loop for as long as that holds true — which
// is forever in practice, since the main loop()'s hardware watchdog is only
// re-armed once per iteration. streamWriteUntil() caps that wait instead of
// trusting the platform to give up.
//
// Under normal conditions (host actually reading) this is a no-op compared to
// a plain write(): writes are never asked for more than availableForWrite()
// already reports free, so they return immediately without entering the
// platform's internal wait/yield loop at all. Only a genuinely stalled host
// (zero throughput) hits the deadline; a slow-but-alive one just takes longer,
// same as a plain write() would.
#include <Arduino.h>

// Returns the number of bytes actually written; less than `len` only when the
// host stopped draining the link before `deadline_ms` (an absolute millis()
// value — share one deadline across multiple calls that make up one logical
// message, so a stall costs a fixed total budget, not one per call).
static inline size_t streamWriteUntil(Stream& s, const uint8_t* buf, size_t len, uint32_t deadline_ms) {
  size_t sent = 0;
  while (sent < len) {
    int avail = s.availableForWrite();
    if (avail <= 0) {
      if ((int32_t)(millis() - deadline_ms) >= 0) break;   // host stalled — give up
      yield();   // let the USB/BLE background task run so it can drain the FIFO
      continue;
    }
    size_t chunk = (size_t)avail < (len - sent) ? (size_t)avail : (len - sent);
    size_t w = s.write(buf + sent, chunk);
    if (w == 0) {
      if ((int32_t)(millis() - deadline_ms) >= 0) break;
      yield();
      continue;
    }
    sent += w;
  }
  return sent;
}
