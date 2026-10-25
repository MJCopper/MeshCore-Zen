# Named destination trace acknowledgement validation

Date: 2026-10-25. Current working tree on `meshcore/sensor`, based on `adc6cab4` (`trace`), which includes the existing recovery,
acknowledgement and fixed 60-second cap changes.
No commit, flash or physical radio acceptance is implied.

## Implemented behaviour

A uniquely known `7188` named `Hilltop Repeater` produces:

```text
BME680 Sensor: Trace requested to Hilltop Repeater, please wait up to 60 seconds.
```

The same cache lookup returns the full identity and copied destination label.
Full display words and case are preserved; the separate compact hop formatter
still removes `Repeater` as before. ASCII controls/whitespace are normalized,
valid UTF-8 is preserved, and a known identity with no usable name uses lowercase
`7188`. Unknown and ambiguous prefixes retain their errors and clear lookup
outputs. Adverts without a name field retain existing cache admission behaviour.
No cache schema, authentication or ACL changes were made.

The acknowledgement formatter snapshots sensor prefix and destination before
queueing. A rename or cache eviction cannot change queued/retried text or the
full key used for discovery. It fits one 160-byte public message, truncating only
the destination at a valid UTF-8 boundary when necessary. If no meaningful label
fits, the prefix fallback is tried; if that cannot fit either, formatting fails
before acceptance. Fixed text and the timeout suffix cannot be truncated.

Bare acknowledgement, final trace formatting, node ordering, averaged SNR,
channels/scopes, command controls and the 55/60-second cutoffs are preserved.
The earlier sensor-versus-Zen discovery failure remains a separate investigation.

## Test evidence

New tests cover:

- Full display name with `Repeater`, normalized whitespace/control characters,
  unnamed/invalid UTF-8 fallback, maximum stored UTF-8 names and cache replacement.
- Clearing full-key and label outputs on unknown or ambiguous resolution;
  unchanged compact hop labels and existing cache persistence behaviour.
- Exact 160-byte acknowledgement, one-byte overflow, UTF-8-safe truncation,
  fallback when no valid destination character fits, and impossible prefixes.
- Frozen mutable input across definitely-unsent and uncertain failures.
- Real Dispatcher named acknowledgement before discovery, uppercase prefix,
  same-name/different-key identities, and decryption with the selected full key.
  Decryption with the other identity fails, ensuring names do not select keys.
- Queue rejection followed by cache/sensor rename, and eviction of the selected
  cache entry before retry; emitted text and discovery identity remain frozen.
- Scoped real-network delivery with exact named acknowledgement on both channels
  and both path-hash widths, alongside unchanged final results.

Existing acknowledgement ownership/recovery, unknown/ambiguous rejection,
rate-limit/control, timing, fallback, SNR, scope and packet-pool regressions also
run. Legacy exact-phase tests explicitly isolate acknowledgement through their
transport seam; dedicated acknowledgement tests and network tests use the real
Dispatcher. The seam's message recognition now includes both acknowledgement
forms.

## Validation gates

All software gates passed:

| Gate | Result |
| --- | --- |
| Full native/sensor/KISS regression suites | 426/426 passed |
| Sensor AddressSanitizer suite | 319/319 passed |
| Extended subsequent-request rename assertions | 77/77 targeted tests passed, both native and ASan |
| Five firmware environments, buildprog and UF2 | Passed |
| Five DFU archives, CRC and root manifest/bin/dat | Passed |
| Sensor boundaries and whitespace checks | Passed |
| Strict MkDocs build | Passed; existing FAQ anchor INFO notice remains |

After the full runs, the rename test was strengthened to check a subsequent
accepted request. Both targeted suites were rerun successfully against that
assertion. Production code was unchanged between those runs.

The final sensor uses 37,364 bytes of static RAM and 360,532 bytes of flash:
no static RAM increase and 1,360 additional flash bytes versus `adc6cab4`'s
60-second cap build. The five environments were `Xiao_nrf52_bme680_sensor`,
`Xiao_nrf52_sensor_baseline`, `Xiao_nrf52_companion_radio_usb`,
`Xiao_nrf52_repeater`, and `Xiao_nrf52_room_server`.

[Check summary and artifact SHA256 values](sensor-trace-destination-ack-checks.txt)
record the generated firmware. The named sensor OTA is
`.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Named_Trace_OTA_2026-10-25.zip`.
It is copied byte-for-byte from the verified `firmware.zip`, with a DFU
application manifest at the archive root. Its SHA256 is
`8ec8cedfcb3b4cf6113b608d84efb674160188d6d1327935effb28428d44ac7a`.

```sh
pio test -e native -e native_sensor -e native_kiss_modem
ASAN_OPTIONS=detect_leaks=0 pio test -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan
pio run -e Xiao_nrf52_bme680_sensor -e Xiao_nrf52_sensor_baseline \
  -e Xiao_nrf52_companion_radio_usb -e Xiao_nrf52_repeater \
  -e Xiao_nrf52_room_server -t buildprog -t create_uf2
bash bin/check-sensor-boundaries.sh
git diff --check
mkdocs build --strict
```

PlatformIO executable: `/home/matt/.platformio/penv/bin/pio`. ASan uses the
existing isolated host configuration and extracted runtime documented in earlier
validation records, with leak detection disabled. Firmware contains no sanitizer
runtime. The production implementation uses bounded stack/static buffers and
adds no per-request heap object or cache persistence writes.

## Physical acceptance pending

No device has been flashed. Verify the named and fallback acknowledgements on
hardware, a rename during queue retry and on a later request, unknown/ambiguous
errors, both channels, usual final output and the unchanged 60-second cap.
Physical discovery/receipt performance remains unverified by native simulation.
