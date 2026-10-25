# Guest-login reply timing — 25 October 2026

Superseded by the [two-retry update](sensor-trace-login-retries.md), which retains
the post-completion reply window and increases the control cutoff to 40 seconds.

The guest-login stage previously had one eight-second lease covering queueing,
transmission and the reply. A valid acknowledgement could therefore arrive after
that lease despite responding promptly to the actual transmission. This is a
confirmed timing limitation, not proof of the cause of 7188's field failure.

Guest login now uses three distinct phases:

1. Queue admission/dispatch: at most eight seconds.
2. Transmission: cached airtime watchdog (`airtime * 1.5 + 1000 ms`) starting
   at actual radio TX start.
3. Receive: up to eight seconds starting at successful local TX completion.

Each phase is clipped to the original 25-second control cutoff from command
acceptance. A completion close to that cutoff has less than eight seconds for
its reply. The 55-second work cutoff and 60-second overall promise remain.
Ordinary discovery leases, login count, credentials and scope selection are
unchanged. The private diagnostic readouts remain included.

TX completion must match the owned in-flight packet. Missing or failed TX,
queue expiry, cancellation and expired TX/control leases never arm receive.
Duplicate or late completion cannot renew a receive window. `trace status`
distinguishes guest login queued, transmitting and awaiting reply with the
current phase's remaining time. A TX watchdog expiry is a transmission failure,
not a claim that the repeater ignored the credential.

Six new regressions cover queue/airtime separation, the exclusive receive
boundary, missing/late/wrong/duplicate completion, cancellation and queue expiry,
25-second clipping across millisecond wrap, and a real Dispatcher with a busy
channel. In that packet-level test login starts at 13.2 seconds, completes at
13.3 seconds and receives its authenticated reply at 19 seconds; the old
combined stage lease would have ended at 14.2 seconds. The new implementation
accepts the reply and queues fresh discovery without another login.

Validation: **475/475 regression tests and 368/368 sanitizer tests passed**.

Commands and logs:

- `pio test -e native -e native_sensor -e native_kiss_modem`:
  `/tmp/trace-login-timing-all-tests.log`.
- `ASAN_OPTIONS=detect_leaks=0 pio test -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan`:
  `/tmp/trace-login-timing-asan.log`. Existing isolated local ASan runtime/config.
- `pio run -e Xiao_nrf52_bme680_sensor -t buildprog -t create_uf2`:
  `/tmp/trace-login-timing-build.log`; passed.
- Strict MkDocs, sensor-boundary check and `git diff --check`: passed.
- OTA ZIP CRC, root manifest and application BIN/DAT presence validated; built
  HEX/UF2 nonempty. The new archive is byte-for-byte equal to `firmware.zip`.
  Earlier named archives were preserved.

OTA: `.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Trace_Login_Timing_OTA_2026-10-25.zip`.
SHA256: `a34a99560b5a2f53a03d6d184b0f161136bc8049904b67d55000633451f8747a`.

No hardware was flashed and 7188 interoperability is not yet confirmed. If this
still fails, use the private/local diagnostics after one failed trace, before
starting another: `get trace.diag`, `get trace.control 1`, `get trace.peer`.
