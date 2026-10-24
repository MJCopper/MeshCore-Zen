# Trace request acknowledgement validation

Historical pre-cap record: its calculated maximum and artifact hashes are
superseded by the [60-second cap validation](sensor-trace-60-second-cap.md).

Date: 2026-10-25. Implementation is in the current working tree on top of
`69891ef8`, including the existing trace recovery changes. No commit, device
flash, or over-air acceptance is implied by this record.

## Implemented behaviour

Accepted bare and targeted trace requests schedule one acknowledgement without
application jitter:

```text
BME680 Sensor: Trace requested, please wait up to X seconds.
```

The text, name prefix, reply channel, scope and path-hash width are captured.
The acknowledgement owns separate state from the final result. Its admission
lease is ten seconds; after TX start, it uses the actual packet's cached TX
watchdog, checked against the planned maximum-frame allowance. Only definitely
unsent failures can retry. Missing callbacks, allocation failure and queue
rejection cannot hold the acknowledgement stage indefinitely. Tracing proceeds
when that stage finishes, even if acknowledgement failed.

The fixed overall deadline is anchored at acceptance. Its checked 64-bit budget
includes acknowledgement, optional two-attempt discovery, settling, two probe
attempts/admissions, retry pause, final grace and terminal delivery. Bare traces
reserve their original route for attempt one and the maximum supported route
for a fresh alternate; destination traces reserve maximum routes before
discovery. Displayed seconds round up. Unrepresentable budgets are rejected.
New probe/discovery phases reserve terminal-report time; expiry does not open a
new reporting window. Status shows remaining overall seconds.

Queue cleanup runs before Dispatcher admission. Transmission observation gets
normal completion/watchdog polling first. Cancellation, profile changes and
expiry cancel only owned queued packets. In-flight allocation stays under
Dispatcher ownership. The software releases trace ownership on the first
running service poll at expiry; receipt under RF loss or a stopped application
loop cannot be guaranteed.

The timing calculation uses the conservative composition in the
[approved plan](../sensor-trace-request-ack-plan.md). For representative shipped
SF8/BW62.5/CR5 estimates, a destination trace advertises 617 seconds; a bare
three-repeater example advertises 164 seconds. Stored settings determine the
actual value. These maxima intentionally include failure paths and are longer
than normal successful runs.

## Coverage

`test_sensor_trace_ack` contains 54 cases covering:

- Independent numeric fast/slow budget examples; checked overflow/invalid input;
  rounding boundaries; all ten supported route lengths and both hash widths.
- Exact deadline and rollover behaviour, skipped polls, and refusing a new full
  phase when too little time remains.
- Real encryption/Dispatcher acknowledgement before probe and discovery;
  unchanged RTT/SNR result output; slow acknowledgement not consuming probe
  admission time.
- Allocation pressure, busy-channel delay, queue rejection, definitely unsent
  failures, uncertain emission, and lost completion callbacks.
- Cancellation queued/on air, radio-profile invalidation, duplicates/busy starts,
  invalid routes/unknown target, status and restart after expiry.
- Frozen prefix despite queue retry; oversized prefix rejection; frozen channel,
  scope and width across both channels, both widths and scoped/unscoped requests.

Older exact-phase integration tests explicitly isolate acknowledgement via a
transport seam to preserve their probe/report boundary characterization. The
new acknowledgement suite and network suites disable that seam and use real
Dispatcher ownership. Existing parser/rate-limit, target discovery, averaged SNR,
late-result fallback, history, two-part reporting and cancellation regressions
also run.

The network simulation delivers acknowledgements separately from terminal
reports, including scoped relays and both channels/widths. Its collision case
shows that an acknowledgement flood can make an otherwise valid two-byte route
need the second allowed attempt. Success remains required, with at most two
probe transmissions. This is an RF contention consideration for hardware
acceptance, not proof of physical radio performance.

## Validation commands and results

All software gates passed on the final implementation:

| Gate | Result |
| --- | --- |
| Native, sensor and KISS regression suites | 410/410 passed |
| Sensor AddressSanitizer suite | 303/303 passed |
| Dedicated acknowledgement suite (included above) | 54/54 passed |
| Five firmware environments, buildprog and UF2 | All passed |
| Five DFU archives, CRC and root manifest/bin/dat validation | All passed |
| Sensor boundary script and whitespace checks | Passed |
| Strict MkDocs build | Passed; existing FAQ anchor INFO notice remains |

Final sensor static RAM is 37,372 bytes and flash is 359,204 bytes. Against the
pre-acknowledgement recovery build (37,164 RAM / 355,172 flash), this adds 208 RAM
and 4,032 flash bytes. The acknowledgement stores bounded text/state and does
not allocate a per-request heap object.

The five environments are `Xiao_nrf52_bme680_sensor`,
`Xiao_nrf52_sensor_baseline`, `Xiao_nrf52_companion_radio_usb`,
`Xiao_nrf52_repeater`, and `Xiao_nrf52_room_server`.
[Check summary and artifact SHA256 values](sensor-trace-request-ack-checks.txt)
record the generated HEX, DFU ZIP and UF2 files. The current sensor DFU archive
is `.pio/build/Xiao_nrf52_bme680_sensor/firmware.zip`; older named OTA archives
remain untouched and do not include this acknowledgement change.

PlatformIO is `/home/matt/.platformio/penv/bin/pio` on this host.

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

The ASan configuration uses an isolated `/tmp/meshcore-scope-asan-build` and the
existing extracted runtime at `/tmp/meshcore-scope-asan-runtime/usr/lib64`, with
explicit linker search path/rpath. Leak checking is disabled for this host setup;
this is address-safety testing. No sanitizer runtime is shipped in firmware.

## Hardware acceptance still pending

No connected sensor/repeater hardware was used and no firmware was installed.
Verify the plan's physical matrix before treating RF behaviour as accepted:
reachable/silent `4df5`, bare trace, both channels and scopes, direct/multi-hop,
fast/slow profiles, contention, cancellation and lost final replies. Record
acceptance/TX/result/cleanup timestamps and compare them to the frozen displayed
maximum. Re-run the original second-request-after-30-seconds scenario: busy
before the advertised deadline can be correct; busy persisting after deadline
cleanup on a running loop is a failure. Confirm the next trace starts without a
reboot. Actual link SNR and receipt require hardware evidence.
