# Sensor multi.flood validation

Date: 31 October 2026 (Australia/Sydney).
Scope: `meshcore/sensor`, sensor release **v1.18.0-sensor.1**.
Base: `6f5506a38f65666260e40673cb5a341e66640b18` with working-tree changes.
Port reference: `meshcore/doubler` at `7f0cb629cdb853421cd85acf43c38243050a274a`.
Zen was not modified, built or versioned. No hardware was flashed or published.

## Result

Implemented local-origin modes 0/1/2 (default off), exact-byte optional copies,
500–800 ms ready delay and fixed 3,000 ms expiry. Four pending records retain
existing pool packets; four free slots are required. Normal traffic keeps
priority. Copies never force a busy-channel TX, never create a third TX and
retain normal radio airtime/duty accounting.

Generic inactive Dispatcher hooks are ported alongside the existing sensor
TX-start hook. SensorMesh has generic applied-radio and packet-context
notifications. `SensorFloodBinding<Runtime>` provides the BME680-only policy,
32-packet origin and trace-operation tracking, storage and diagnostics.
The companion adapter is retained as a source-reference regression helper; it
is not connected to the stock companion role in this change.

`/doubler.json` uses the reference schema and atomic temporary-write/readback/
rename semantics. A generalized write-mode template supports native fault FS
and production LittleFS. No wire fields, NodePrefs layouts or baseline serializer
changes. Mode changes and applied context changes invalidate copies; waiting
copies cannot revive after A → B → A transitions.

Trace packet tags are local epoch/deadline bookkeeping. Original logical
callbacks run once; optional callbacks update physical radio activity without
restarting discovery/login timers, changing logical retry counts or advancing
reports. Closing work invalidates its existing tagged copies; new terminal
report parts may retain copies within their original deadline. Final copy
admission rechecks time after radio preparation and reserves its TX watchdog.
The acknowledgement's calculated maximum and 120-second ceiling are unchanged;
optional traffic consumes existing channel/airtime/queue allowances and can be
skipped instead of extending those limits.

## Tests

- **792/792 regression tests passed** across `native`, `native_sensor` and
  `native_kiss_modem`.
- **595/595 sensor AddressSanitizer tests passed**.
- **89/89 imported policy/settings AddressSanitizer tests passed**.
- Imported policy/settings native suite rerun after the test-only allocator shim
  correction: **89/89 passed**.
- All **five** sensor/baseline regression firmware builds and UF2 tasks passed.

The 89 source-policy tests cover scheduling, exact bytes, expiry, priorities,
budget contention, in-flight ownership, contexts, settings and atomic-storage
faults. The sensor-specific suite adds 25 cases; the network suite adds mode 2
on a variable-length-airtime Australia Narrow SF7/BW62.5/CR7 medium. Tests
exercise the production binding with real Mesh crypto/parsing/Dispatcher:

- Scoped/unscoped floods and 1/2/3-byte outer path widths; direct/zero-hop
  exclusion; local text versus other payload modes.
- Explicit zero-path forwarded exclusion, low pool headroom, normal priority,
  cancelled/expired pending packets and safe in-flight cancellation.
- Shared mode persistence, corruption/future schemas, invalid commands, failed
  saves, public bot permission boundaries and read-only counters.
- Radio/identity/hash/default-scope A → B → A invalidation without flash writes.
- Up to six physical login frames still representing three logical attempts
  with distinct durable timestamps; no extra direct TRACE or report lifecycle.
- Failure of an optional terminal copy preserves delivered logical outcome.
- An old terminal copy cannot complete/reset a newer trace operation.
- Copy deadline and full-watchdog admission, including time advancing during
  radio preparation. Every completed/recovered path returns pool ownership.

ASan uses the existing extracted matching runtime and an isolated temporary
configuration, with leak checking disabled as in prior project validation.
Allocation/deallocation mismatch detection was not disabled. The ported test's
allocation-counting global new/delete shim initially lacked nothrow overloads,
causing libstdc++/GTest setup to mix ASan allocation families. Matching nothrow
new/delete overloads corrected the test harness; the clean sanitizer rerun passed.
An initial temporary environment inherited no explicit platform; the successful
policy sanitizer configuration specifies `platform = native`.

## Commands and logs

```sh
pio test -e native -e native_sensor -e native_kiss_modem
ASAN_OPTIONS=detect_leaks=0 pio test -c /tmp/meshcore-multiflood-asan.ini -e native_sensor_asan
ASAN_OPTIONS=detect_leaks=0 pio test -c /tmp/meshcore-multiflood-asan.ini -e native_flood_asan
pio run -e Xiao_nrf52_bme680_sensor -e Xiao_nrf52_sensor_baseline \
  -e Xiao_nrf52_companion_radio_usb -e Xiao_nrf52_repeater \
  -e Xiao_nrf52_room_server -t buildprog -t create_uf2
pio run -e Xiao_nrf52_bme680_sensor
bash bin/check-sensor-boundaries.sh
git diff --check
mkdocs build --strict --site-dir /tmp/meshcore-multiflood-docs
graphify update .
```

Clean logs: `/tmp/sensor-multi-final-tests.log`,
`/tmp/sensor-multi-sensor-asan.log`, `/tmp/sensor-multi-policy-asan-fixed.log`,
`/tmp/sensor-multi-policy-final-native.log`, `/tmp/sensor-multi-final-builds.log`,
`/tmp/sensor-multi-check-size.log` and `/tmp/sensor-multi-graphify-final.log`.
Graph update is AST-only; its non-code extraction/viz-size notices do not affect
the generated code graph. Source-boundary checks passed.

## Resources and artifacts

Sensor static RAM: **47,452 bytes** of 235,520 (20.1%).
Sensor flash: **410,796 bytes** of 811,008 (50.7%).
Compared with the prior companion-timing validation (46,620 RAM / 385,636 flash),
the change adds **832 bytes static RAM** and **25,160 bytes flash**.

Sensor-only version is configured in the BME680 PlatformIO environment; the
baseline explicitly undefines it. All four baseline roles were verified not to
contain the custom sensor version. README matches `v1.18.0-sensor.1`.

Files in `.pio/build/Xiao_nrf52_bme680_sensor/`:

- `MeshCore_BME680_Sensor_MultiFlood_OTA_2026-10-31.zip`
  SHA256: `5b416888c9530e93613ceb1071d41a66781809730f925b4d40b61ec5f126f898`.
- `Xiao_nrf52_bme680_sensor.1.18.0-sensor.1.uf2`
  SHA256: `691ac87438144919ccf743796e7c1468df758c1a40943014b4caa3d25eeb7fd5`.
- `firmware.hex` and `sensor-multi-flood-manifest.json`.

The named OTA is byte-for-byte the generated `firmware.zip`; it was not nested
or repacked. ZIP CRC, root application-only DFU manifest, embedded sensor
version, binary CRC16, init packet and Xiao SoftDevice requirement `0x0123`
were verified. Every UF2 block/family/sequence was checked. The same checks
passed for all five role artifacts; see
[artifact checksums](sensor-multi-flood-artifacts.json).
Previous named OTA packages were preserved.

## Physical acceptance remaining

Confirm mode 0 baseline and mode 1/2 exact-byte repeats by RF capture; measure
the post-completion gap, busy-channel suppression and actual airtime. Exercise
trace cancellation and report copies while a second trace starts, long traffic
bursts, mode/context changes and a low filesystem-space/save failure. Check
power-loss behaviour of LittleFS settings updates. Simulated Australia Narrow
network success is not proof of reception, duplicate-cache retention or storage
atomicity on physical hardware.
