# Development and Testing

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

## Baseline

The integration baseline is MeshCore v1.17.1, release commit `d9296435`.
Use this commit for comparisons: the fork's local tag named `v1.17.1` points
to a different, Solo-related commit. The extraction started from sensor commit
`5f1bc689`.

## Application modules

| Module | Responsibility |
| ------ | -------------- |
| `main.cpp` | Hardware startup, serial console and main loop |
| `Bme680SensorMesh` | Binding to the upstream sensor runtime and radio callbacks |
| `SensorBotService` | Public commands, response formatting and asynchronous coordination |
| `PublicChannelSensorBot` | Command decoding, duplicate tracking and rolling rate limit |
| `PublicResponseQueue` | Bounded two-part replies and transmission-completion spacing |
| `RepeaterTraceProbe` | Path validation, timeouts, single retry and late-result fallback |
| `TraceTimingPolicy` | Hop/airtime timeout estimates and bounded queue, grace and report periods |
| `TraceRouteCandidates` | One passive, RAM-only alternate path for the exact accepted command |
| `RepeaterNameCache` | Verified repeater names and bounded persistent cache |
| `Bme680Telemetry` | Cached measurements, channel discovery and telemetry encoding |
| `Bme680SensorManager` | Upstream BME680 driver adapter and installation altitude adjustment |
| `BME680AirQuality` | RAM-only calibration, warm-up count and relative score |
| `SensorManagementPolicy` | Read-only guest admission and administrator persistence |

These modules live in `examples/bme680_sensor/`. They share one measurement
snapshot. Public and management queries read that snapshot without sampling
the BME680 or advancing its calibration. A failed sample retains the previous
successful reading. Sensor readings and pending trace results do not persist.

## Baseline integration ledger

| Baseline file | Retained integration |
| ------------- | -------------------- |
| `examples/simple_sensor/SensorMesh.h/.cpp` | Optional login, protocol-request, preference and persistence hooks; default policies retain the standard sensor behaviour |
| `src/Dispatcher.h/.cpp` | Generic transmit-start callback and queued-packet inspection/cancellation |
| `src/Mesh.cpp` | TRACE header and complete-path bounds checks |
| `src/helpers/TracePathBounds.h` | Small shared TRACE validation helper |
| `src/helpers/ClientACL.h/.cpp` | Optional administrator-preserving client allocation, sharing the standard allocator |
| `variants/xiao_nrf52/target.h/.cpp` | Selectable sensor-manager type, defaulting to the upstream environment manager |

The standard `examples/simple_sensor/main.cpp` and
`src/helpers/sensors/EnvironmentSensorManager.cpp` match the baseline.
Earlier humidity-decoding and request-length corrections, together with
request-scope-aware login/telemetry replies, remain generic sensor corrections.
Later upstream imports are distinct from this application and are retained.

The core does not include custom bot, air-quality or guest-policy classes.
The XIAO custom environment selects the application; there is no bot flag
controlling unrelated authentication or storage behaviour.

## Automated validation

```sh
pio test -e native -e native_sensor -e native_kiss_modem
bash bin/check-sensor-boundaries.sh
pio run -e Xiao_nrf52_bme680_sensor -e Xiao_nrf52_sensor_baseline
pio run -e Xiao_nrf52_repeater -e Xiao_nrf52_room_server -e Xiao_nrf52_companion_radio_usb
```

The original native suite covers command parsing, rate-limit boundaries,
reply splitting, trace state transitions and malformed TRACE paths. The
`native_sensor` suite uses actual MeshCore encryption, signatures, packet
handling, dispatcher queues, telemetry encoding and ACL storage. Its simulated
clock, radio and filesystem make failure timing and write counts repeatable.
Hardware randomness is replaced only by deterministic test input.

Integration cases include silent retry, duplicate command reception, cancelling
a queued retry, late first-result fallback, multipart spacing after actual TX
completion, verified repeater names, and radio recovery after an advert TX
timeout. Storage tests cover capacity, ambiguous hashes, corrupt cache data,
failed file replacement and avoiding writes for unchanged adverts.

Channel tests cover interleaved Public and `#hillvue` requests, multipart reply
ownership, busy notices, trace results and late-result fallback. Pending parts
and active traces retain a bounded channel index rather than a mutable global
reply destination. Rate limiting is shared; duplicate tracking includes the
source channel. Candidate lookup also handles matching one-byte channel hashes.

Trace expiry now schedules the next action rather than invalidating an otherwise
matching result. Both transmitted attempt records remain live through final
grace and unsent failure reporting. The radio adapter commits failure at TX
start; queued failures and retries are cancelled before their pool handles are
discarded. Monotonic, randomly seeded tags distinguish successive attempts.

The multi-repeater suite uses real MeshCore forwarding and encryption with
simulated airtime, directed links, delay and packet loss. It covers two/three
repeaters, two-byte hashes, same-route retry, alternate-route retry, asymmetric
links and useful late results. The medium is not a physical RF or collision model.

An additional AddressSanitizer run checks the real-crypto suites:

```sh
ASAN_OPTIONS=detect_leaks=0 pio test -e native_sensor_asan
```

Leak detection is disabled because the baseline static packet-pool test objects
do not implement destruction. Tests separately require all packet slots to be
returned after each completed scenario. Address/buffer checks remain enabled.

Initial command, air-quality, cache, ACL and real-crypto tests were run against
the implementation before extraction. The extraction also corrects two
behavioural defects: non-finite sensor inputs cannot advance calibration, and
remote telemetry requests now use the same cached readings as Public commands.

The compile-only baseline environment exercises ordinary BME680 telemetry
without any custom sensor-manager or altitude-offset definition. This also
guards against the previous misplaced altitude-offset fallback macro.

## Implementation validation

Recorded on 19 October 2026:

| Check | Result |
| ----- | ------ |
| Pre-extraction characterization tests | Passed before moving application code |
| `native` | 85 test cases passed |
| `native_sensor` | 58 test cases passed, including 31 packet integration and seven multi-repeater cases |
| `native_kiss_modem` | 8 test cases passed |
| `native_sensor_asan` | The same 58 sensor cases passed with AddressSanitizer |
| Custom sensor, baseline sensor, companion USB, repeater and room-server builds | All five passed |
| Baseline boundary check and `git diff --check` | Passed |
| Strict MkDocs site build | Passed |
| Named UF2 generation and DFU ZIP integrity | Passed |
| Physical sensor, phone app, RF and OTA acceptance | Pending: no attached hardware |

The native companion-preferences suite is conditionally excluded by its existing
platform guard; it does not exercise this sensor application. No test case in
the executed suites failed.

The initial extracted sensor build reports 31,200 bytes of static RAM and 353,196
bytes of flash, compared with 31,168 and 354,604 before extraction. These are
linker figures, not measured peak heap or stack usage. The new Cayenne buffers
are allocated once at startup, not for each telemetry request.

The completed trace remediation reports 31,440 bytes of static RAM and 355,724
bytes of flash, compared with 31,232 and 353,692 before this remediation. The
additional trace state therefore costs 208 bytes of static RAM and 2,032 bytes
of flash. It adds no heap buffers or flash writes. Local sanitizer validation
used a signature-verified Fedora runtime extracted under `/tmp`, without
installing system packages.

### Trace plan completion

- The two deadline-boundary regressions failed before the implementation changed.
- Per-attempt result validation, final grace, cancellable failure reports and
  bounded queue ownership are implemented and tested.
- Airtime estimation, a single passive alternate route and original-route
  fallback rendering are implemented and tested.
- Limits, channel ownership, formatting, packet-pool return and zero-write
  behaviour are covered by regression and multi-repeater tests.
- CI runs the regular tests, sanitizer tests and five firmware targets.
- Firmware artifacts and documentation are validated separately from hardware.

The software plan is complete. Physical RF, phone-app presentation and OTA
installation remain pending acceptance on an attached node; simulation is not
a substitute for those checks.

The software extraction, automated checks and build integration are complete.
Full hardware acceptance is not claimed until the following checklist passes.

## Storage and firmware updates

The `/repeaters`, `/s_contacts`, identity and preference formats are unchanged.
Module relocation requires no erase or settings migration. Administrator state
and repeater names retain their existing write policies. Service buffers are
bounded; normal request processing does not allocate new application objects.

The custom target keeps the established `Xiao_nrf52_bme680_sensor.uf2` and
`Xiao_nrf52_bme680_sensor.zip` artifact names.

## Hardware acceptance

Automated tests cannot establish RF delivery, physical sensor accuracy or the
phone app's presentation. Complete these checks on the installed XIAO before
marking hardware acceptance passed:

1. Install the named UF2 and confirm identity, radio settings and administrator
   access survive the update.
2. Observe the first reading, ten-sample warm-up and continued one-minute
   sampling; issue repeated telemetry queries and confirm warm-up does not
   advance between samples.
3. Check blank-password and incorrect-password guest telemetry in the official
   app and Zen companion; confirm management controls remain restricted.
4. Check administrator status, owner info, configuration and ACL management.
5. Exercise `!hillvue`, combined measurements, voltage, ping, path and trace,
   respecting the rolling four-command limit.
6. Test real two- and three-repeater routes on both watched channels, retry
   success, an observed alternate route, late fallback and multipart delivery
   during ordinary network traffic. Repeat with a slower radio configuration;
   compare observed RTT with the airtime-aware deadline. Confirm a valid late
   result replaces an unsent failure, with no contradictory terminal report.
7. Trigger remote Advert and verify whether the advert is heard, its command
   confirmation arrives, and subsequent telemetry/Public commands still work.
   The reported app Advert timeout is not reproduced by the simulated radio
   recovery test and remains a hardware acceptance item.
8. Install the named DFU ZIP through the intended OTA application and repeat
   the basic access and telemetry checks.

No XIAO serial device was attached during this implementation. Hardware and
phone-app acceptance must therefore be recorded separately from automated
test/build results. Zen version and OLED/E-ink build instructions apply to the
Zen application; those sources/environments are absent on this sensor branch.
