# Development and Testing

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

## Baseline

The integration baseline is MeshCore v1.18.0, official release commit
`81dbae592ffad40b4212bc0b37e82b38da08f4bc`. Use the official
`companion-v1.18.0`, `repeater-v1.18.0` and `room-server-v1.18.0` tags;
all three resolve to this commit. Do not infer the baseline from unrelated
fork-local version tags.

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
| `TraceRouteCandidates` | Three bounded passive alternatives and deterministic retry ranking |
| `TraceRouteHistory` | Four RAM-only, context-scoped recent route outcomes with lazy expiry |
| `TraceReportDelivery` | Frozen two-part report, bounded local retry and uncertain-outcome handling |
| `TraceDiagnostics` | Full directional SNR, passive progress and local outcome counters |
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
The legacy exact-time packet cases explicitly suppress settling; production
defaults and separate settling tests enable it. Multi-node tests use the
production default.

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
| `native` | 88 test cases passed |
| `native_sensor` | 108 test cases passed, including 49 packet integration and 21 network-medium cases |
| `native_kiss_modem` | 8 test cases passed |
| `native_sensor_asan` | The same 108 sensor cases passed with AddressSanitizer |
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

The software extraction and earlier remediation are complete. Physical RF, phone-app presentation and OTA
installation remain pending acceptance on an attached node; simulation is not
a substitute for those checks.

The software extraction, automated checks and build integration are complete.
Full hardware acceptance is not claimed until the following checklist passes.

### Trace reliability validation

The pre-change baseline passed 162 regular cases. The reliability change adds
retained report delivery, bounded allocation/queue recovery, conservative radio
start handling, a first-probe settling window, recent route evidence and passive
diagnostics. All buffers are fixed-capacity; history and progress are RAM-only.
Idle operation performs no route scans or periodic radio/configuration queries.

| Reliability plan item | Software validation |
| --------------------- | ------------------- |
| Retain successful results through report delivery | Allocation/queue recovery, frozen text, multipart gap, expiry and uncertain-outcome cases pass |
| Recover from local probe failures | Admission backoff, rejected starts, remaining-attempt budget and late-first recovery pass |
| Airtime-aware first-probe settling | Production defaults, quiet extension, hard limit, unrelated traffic and RTT exclusion pass |
| Rank alternatives and retain recent evidence | Three-candidate/four-history bounds, channel/radio context, expiry and deterministic ranking pass |
| Directional SNR and passive progress | Both hash widths and one through ten repeaters pass; malformed/stale progress is rejected |
| Collision/contention/hash-collision tests | 21 deterministic network cases pass, including eight seeded-loss runs |
| Physical RF measurements and OTA install | Pending: no attached XIAO; no physical performance claim |

Final automated validation comprises 204 regular cases and 108 repeated sensor
cases under AddressSanitizer: 312 executions passed. The existing sanitizer
leak-detection qualification above still applies; packet-pool return is checked
explicitly by integration/network tests.

The final custom sensor linker reports 32,408 bytes of static RAM and 362,956
bytes of flash. Static RAM increased by 968 bytes from the preceding 31,440-byte
build, below the two-KiB review budget. These figures exclude the linker's
reserved heap area and do not claim a measured runtime stack/heap peak. The
additional modules introduce no per-request application heap allocations or
new persistent formats/writes.

The radio medium now has optional half-duplex, overlapping-frame collision,
hidden-terminal, six-decibel capture-threshold, busy-channel and seeded-loss
models. It includes standard direct-forwarding delay estimates and competing
repeaters sharing a one-byte prefix. These are reproducible approximations,
not measured RF capture behaviour. Packet ownership callbacks are synchronous
and precede dispatcher release; reports retain no packet handle after callback
completion. No asynchronous callback may refer to a recycled allocation.
The adapter runs queued-work expiry before the dispatcher, preventing an old
queued report from starting after its deadline when the loop resumes late.

Production conservatively treats rejected report starts as uncertain: the
RadioLib Boolean interface does not expose whether an error occurred before or
after the TX command. Tests explicitly mark a simulated rejection as definitely
unsent only when their fake radio emits no frame. No generic MeshCore callback
or forwarding changes were needed for this reliability change.

Hardware acceptance remains a separate gate. Use an isolated/controlled RF
setup: `#hillvue` alone does not isolate interference on the shared frequency.
For each important condition, compare baseline and updated firmware, preferably
interleaving at least thirty runs each where practical. Observe the existing
command limit and allow each operation to finish. Record firmware, radio
settings, topology, trial count, confirmed starts, potentially emitting probe
submissions, local failures, timeouts, selected route, passive progress, RTT,
total response delay, local report outcome, companion receipt, duplicates and
airtime. Include clean two-/three-relay routes, slow settings, contention,
asymmetry, alternate paths, delayed results and multipart reception. Report
sample sizes and uncertainty rather than claiming a universal success rate.

## Storage and firmware updates

The `/repeaters`, `/s_contacts`, identity and preference formats are unchanged.
Module relocation requires no erase or settings migration. Administrator state
and repeater names retain their existing write policies. Service buffers are
bounded; normal request processing does not allocate new application objects.

The custom target keeps the established `Xiao_nrf52_bme680_sensor.uf2` and
`Xiao_nrf52_bme680_sensor.zip` artifact names.

## Trace scope regression coverage

The scope adapter is shared between the production sensor and native test
runtime. Native tests use real RegionMap resolution, transport-key HMACs,
MeshCore encryption, Dispatcher and packet pools. Each delayed reply owns its
channel, scope key and path-hash width. Scope selection helpers also serve the
upstream synchronous sensor reply path; the baseline firmware build checks that
this generic API remains compatible.

`test_sensor_dispatcher` isolates actual TX-start timing, scheduled and busy-channel
waiting, failed-start ownership, selective cancellation and ordinary consumer
completion. `test_sensor_scope` covers both watched channels, interleaved
multipart messages, busy notices, trace retry/fallback, allocation and queue
recovery, scope removal and key rotation. Route tests isolate history by the
complete scope key and prevent foreign-scope command copies extending settling.
Network tests validate transport codes on relays that reject unscoped floods,
including wrong-scope controls, multipart delivery and trace failure reports.

These tests run in the regular and sanitizer sensor environments. Firmware
resource measurements, validation commands and separate physical acceptance
status are recorded in the [scope implementation validation report](../../upgrades/validation/sensor-trace-scope.md).

## Average bidirectional trace SNR

Trace reports display one node per outward link, beginning at the sensor. For
zero-based repeater index `i` and `N` repeaters, the paired samples are `i` and
`2*N - 1 - i`. Raw signed quarter-dB readings remain available in diagnostics;
their promoted signed sum divided by eight yields the average in dB. The final
sample is the sensor's receive SNR. Rounding happens only when rendering one
decimal place. The header explicitly says `avg SNR`.

`test_sensor_trace_snr` covers every route length from one to ten and both hash
widths, signed extrema, opposite signs, eighth-dB precision, node order,
multipart output, retry precedence and original-route fallback. A packet-medium
case assigns different SNR values to each directed link and exercises actual
MeshCore forwarding for both widths, including sensor RX and exclusion of the
requester link. Existing scoped delivery tests continue to cover channel/scope
retention. See the [average SNR validation report](../../upgrades/validation/sensor-trace-average-snr.md).

Physical acceptance must compare the paired raw readings and displayed averages
on a controlled asymmetric link. Packet-medium tests validate protocol ordering
and arithmetic but cannot establish real radio measurements.

## Destination trace discovery

`test_sensor_target_trace` tests the public-key-prefix command through real
MeshCore encryption, authenticated PATH decoding, the shared production peer
binding and Dispatcher. It covers unknown/ambiguous/malformed prefixes, all
one-to-ten-repeater routes at both hash widths, RTC tags, bounded discovery
retries, radio-start/queue/allocation failure, cancellation before radio send,
scoped replies, late old-target replies, retry/fallback SNRs and full-target
history isolation. Transient discovery peers are not added to the sensor ACL.
Management PATH ownership and other authenticated admin traffic are exercised
against the native ACL runtime.

The packet-medium tests include a known-client/replay-protected repeater
responder: targeted discovery and the complete loop cross real forwarding
nodes, both channels, both hash widths and relays rejecting unscoped floods.
Unknown remote clients time out without automatic login or ACL changes. The
sensor's peer binding consumes RESPONSE/PATH packets without applying them to
management routes; ordinary management PATHs retain their existing callback.
A generic Mesh PATH decoder guard rejects a path that does not fit in the
authenticated decrypted payload before reading its extra fields; an encrypted
truncated-path regression covers this under ASan.

See the [destination trace validation record](../../upgrades/validation/sensor-target-trace.md)
for the commands, measured resource changes and separate physical acceptance.

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
