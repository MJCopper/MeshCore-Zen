# Sensor trace scope implementation validation

Date: 24 October 2026 (Australia/Sydney).
Revision: working tree based on `99dc1448` on `meshcore/sensor`.

## Implemented behaviour

- Replies capture the selected transport key, channel and path-hash width when
  accepted. Ordinary multipart replies, trace success/failure reports, delayed
  fallback, busy notices and allocation/queue retries retain their own context.
- Recognised scope is mirrored; explicitly unscoped floods remain unscoped;
  unresolved/direct requests use the usable default, otherwise unscoped.
  Pending replies retain their captured selection across default changes or
  region removal. Transport codes are computed from each new reply payload.
- Candidate matching and the four-entry, fifteen-minute RAM history are scoped
  by the complete resolved transport key. Unresolved scopes do not reuse
  history; within a transaction their copies must match both transport codes
  and route class as well as command ciphertext. Short-code ambiguity remains
  possible, so these are conservative hints rather than proven node identities.
- Region resolution uses the packet being observed and performs no persistent
  writes. Multiple configured keys matching one short code are treated as
  unresolved. Names/region IDs are not history identities.
- The production and native runtimes instantiate the same `SensorScopeBinding`.
  Generic capture/selection/send helpers are shared with the upstream sensor;
  bot-specific context stays in the BME680 application. The trace packet format,
  two-attempt limit, timeout policy and conservative failure wording are retained.

## Automated validation

| Check | Result |
| --- | --- |
| `native`, `native_sensor`, `native_kiss_modem` | 247/247 passed |
| `native_sensor_asan` | 141/141 passed; no address-sanitizer findings |
| Final report-direction negative controls and pool recovery assertions | 30/30 passed in both regular and ASan sensor environments |
| Sensor boundary check | Passed |
| `git diff --check` | Passed |
| `mkdocs build --strict` | Passed |

The new suites are selected by the existing `test_sensor_*` CI filter and its
sanitizer inheritance. These are local runs of the CI checks; no remote CI run
or publication was initiated. Native clean compilation retains existing Crypto
library endianness macro/no-hardware-RNG warnings. Firmware retains the project's
existing `-w` build flag. MkDocs reports an existing FAQ anchor issue at INFO
level, without failing strict validation.


The [recorded check summaries](sensor-trace-scope-checks.txt) contain the test
suite and firmware results.

The regular checks use:

```sh
pio test -e native -e native_sensor -e native_kiss_modem
bash bin/check-sensor-boundaries.sh
```

The sensor sanitizer checks use the existing `native_sensor_asan` source and
flags with `ASAN_OPTIONS=detect_leaks=0`, as documented by the project. The host
compiler's ASan linker script pointed at a missing system library, so the first
attempt could not link. Validation downloaded and extracted the matching Fedora
`libasan-16.2.1-2.fc44.x86_64` RPM under `/tmp`, without installing a system
package. A temporary copy of the project configuration adds only the local
library search path/rpath and an isolated build directory:

```sh
ASAN_OPTIONS=detect_leaks=0 pio test \
  -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan
```

The temporary config uses `-L/tmp/meshcore-scope-asan-runtime/usr/lib64`,
`-Wl,-rpath,/tmp/meshcore-scope-asan-runtime/usr/lib64`, and
`build_dir = /tmp/meshcore-scope-asan-build`. No sanitizer was disabled beyond
existing leak detection policy. The temporary sanitizer build directory is isolated so local configuration
changes do not invalidate firmware outputs.

New regression coverage includes five focused Dispatcher lifecycle tests,
eighteen scope integration/binding tests, four scope/history candidate tests
and four scoped network-medium cases. Existing coverage continues to exercise
pool reuse, cancellation, expiry, uncertain sends, temporary radio changes,
late-result fallback, ambiguity display and physical-path geometry.

## Firmware and artifacts

All five environments from `sensor-firmware.yml` compiled and converted to
UF2 successfully. Their normal build also generated DFU ZIP files.

| Environment | Compile/DFU | UF2 | RAM bytes | Flash bytes |
| --- | --- | --- | --- | --- |
| `Xiao_nrf52_bme680_sensor` | Passed | Passed | 36,852 | 343,572 |
| `Xiao_nrf52_sensor_baseline` | Passed | Passed | 31,116 | 315,684 |
| `Xiao_nrf52_companion_radio_usb` | Passed | Passed | 143,408 | 372,448 |
| `Xiao_nrf52_repeater` | Passed | Passed | 34,596 | 360,864 |
| `Xiao_nrf52_room_server` | Passed | Passed | 38,284 | 353,296 |

Sensor static RAM: **36,852 bytes**, up **216 bytes** from the baseline.
Sensor flash: **343,572 bytes**, up **5,376 bytes** from the baseline.
The board budgets are 235,520 bytes RAM and 811,008 bytes flash: 15.6% and 42.4%
used respectively. Builds use Nordic nRF52 platform 10.12.0 and ARM GCC 14.2.1.

Sensor artifact paths and SHA-256 checksums:

- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2` (687,616 bytes):
  `a34d124871f6db3bccd3dcc523b4a5c74a887872991186782183ea8d27fb7bd7`
- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.zip` (344,375 bytes):
  `aa642721bb8bd001dcb07f171b238409b685aeafec5fd9fcd9d81192c0b90a55`
- `.pio/build/Xiao_nrf52_bme680_sensor/firmware.hex` (966,442 bytes):
  `ef1623af19c27b3fd73b09ac0e34d5330e01b0f9074c60d5bb442c941362fc50`


The before measurement was taken from the same working tree baseline and
installed Nordic nRF52 toolchain before implementation: static RAM 36,636 bytes;
flash 338,196 bytes. Resource deltas compare the sensor target, not another role.

## Physical acceptance

**Not run. No device was flashed.** Automated packet-medium tests cannot verify
physical delivery, actual RF contention, RTT timing or directional SNR order.

The [implementation plan](../sensor-trace-scope-plan.md#7-physical-acceptance-gate)
remains the physical checklist: scoped delivery through relays rejecting
unscoped floods; both channels and hash widths; delayed multipart and default
scope changes; slower radio and contention timing; retry/fallback without
contradictory reports; outward-hop SNR order; telemetry/admin persistence and
UF2/OTA update smoke tests. Record physical results independently before marking
hardware acceptance complete.
