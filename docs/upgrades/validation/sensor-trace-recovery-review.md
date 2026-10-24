# Trace recovery review and fixes

Date: 25 October 2026 (Australia/Sydney).
Reviewed baseline: `69891ef8` (`More trace functionality`). No device was flashed.

## Reported symptom and conclusion

The user ran `!hillvue trace 4df5` again after thirty seconds and received a busy
reply. That observation matches the configured discovery wait; by itself it does
not demonstrate a deadlock. The target must already recognise and permit the
sensor identity, independently of whether the requesting phone can contact it.
No device logs, active radio settings or over-air capture were supplied, so a
physical failure of repeater 4df5 has not been established.

The discovery receive allowance uses the estimated airtime of a maximum
255-byte packet and the ten-repeater trace policy. Under shipped SF8/BW62.5/CR5,
CRC/explicit-header and 32-symbol-preamble assumptions, the repository's RadioLib
formula gives about 1,512 ms maximum-frame airtime. The policy then gives
187,000 ms per discovery attempt: about six minutes fourteen seconds for two
unanswered attempts, plus admission and reporting delays. Saved device radio
preferences can change that. The former constant-100-ms radio fixtures exercised
only the 30-second minimum and did not make the real-world wait obvious.

The conservative receive allowance has not been shortened without physical
latency evidence. The firmware now exposes the actual current-phase allowance
and lets the operator stop waiting without rebooting.

## Findings and changes

| Priority | Finding | Resolution |
| --- | --- | --- |
| High, demonstrated by fault injection | `TraceReportDelivery::expired()` unconditionally excluded active transmissions. Losing the owner completion notification could keep `trace_report.active()` and the shared busy flag set forever, even after Dispatcher released the packet. | Cache a TX observer budget before transmission: estimated frame airtime × 1.5 plus 1,000 ms grace. Evaluate the fallback only after Dispatcher gets its normal completion/watchdog poll. Close application state as uncertain, with no re-send or contradictory failure. Never release an in-flight packet from the application. |
| Medium | Terminal discovery `FAILED` was excluded from the busy flag. A new bare command arriving exactly at second queue expiry could be admitted, then cancelled by the previous discovery failure handler using the new command's context. | Keep discovery failure owned until its error report is scheduled. Advance discovery deadlines before processing the next received command. |
| Medium | Unsigned elapsed subtraction could expire a newly created report if its begin timestamp was newer than a cached loop timestamp. This can drop the error/result that explains an operation's outcome. | Check signed wrap-safe report age, refresh loop time after work, and use a fresh timestamp for report expiry. Future inter-part due times remain valid. |
| Medium | Bare queued probes were invalidated after the runtime radio loop, allowing a send on changed radio settings. Pending reports could also carry a cached TX allowance into another profile. | Validate all active trace forms before radio dispatch. Drop remaining result parts on profile change without issuing a conflicting second terminal message. Advance the captured profile when scheduling a configuration-change failure so it cannot recreate itself forever. |
| Hardening | A null TX callback could advance an unsubmitted discovery because both pointers were null. | Ignore null start/failure notifications; keep the start observer free of radio/SPI estimates by using the previously cached budget. |
| Operator visibility | A single generic busy notice did not distinguish discovery from probing/report delivery, and there was no channel cancellation command. | Add phase/attempt/remaining-time information, explicit status and cancellation controls. |

The first three reproductions were added before their fixes and failed on the
reviewed code. The missing-notification reproduction suppresses the service's
completion callback while allowing real Dispatcher/pool completion, then tries
a second trace. This is a software fault-injection result, not evidence that the
same notification was lost on the user's hardware.

## Operator commands

```text
!hillvue trace status
!hillvue trace cancel
!hillvue trace 4df5
```

Status shows idle, discovery, settling, probe or report delivery. Discovery and
probe status include the attempt and current queue/receive time remaining.
The first busy discovery notice now includes those details as well. Status is a
snapshot taken when the command is processed; reply queue time can make it older.

Cancellation removes owned queued discovery/probe/report work, stops retries
and future report parts, and releases the shared trace slot. Its acknowledgement
uses the cancelling command's channel and scope. Already-emitted packets cannot
be recalled, and Dispatcher retains exclusive ownership of any in-flight packet.
Late replies cannot restart the cancelled transaction or modify management ACL
routes. Starting another trace still obeys the normal four-command-per-minute
limit. Status and idle cancel requests use that limit; active cancellation is
permitted even if its normal command slots are exhausted, with at most one
cancel acknowledgement per rate-limited operation.

## Scope of review and tests

Reviewed command grammar/rate limiting, target resolution, authenticated peer
ownership, discovery queue/wait/retired states, passive/discovered candidates,
full-target/scope history, probe retries/fallback/grace, signed directional SNR
pairing, report queue/TX/multipart lifecycle, callback routing, pool ownership,
radio generation changes, and production loop ordering.

New regressions cover:

- The reported thirty-second busy observation, plus a realistic 1,512 ms
  max-frame airtime fixture showing discovery 1/2 with 158 seconds remaining
  when queried at thirty seconds.
- New commands arriving at discovery expiry and a missing report completion
  notification, followed by successful admission of another trace.
- Restart and full pool recovery after success, discovery timeout, queue
  exhaustion, rejected start, probe timeout, and unsent report expiry.
- Status/idle controls, cancellation of queued discovery, waiting discovery,
  a bare probe and queued report, and a new transaction despite old late replies.
- Cancel acknowledgement channel/scope and cancellation under exhausted normal
  rate slots, without allowing unlimited idle cancellation replies.
- Stale timestamps, millis wrap, explicit TX callback budgets and no radio
  estimate inside the TX-start observer.
- Delayed-but-successful completion polling preserving the second report part;
  queued/in-flight results and bare probes across radio profile changes.

## Validation

| Check | Result |
| --- | --- |
| `pio test -e native -e native_sensor -e native_kiss_modem` | 356/356 passed |
| Sensor AddressSanitizer environment | 249/249 passed; no address-sanitizer findings |
| Sensor boundary script | Passed |
| `git diff --check` | Passed |
| `mkdocs build --strict` | Passed |

The existing CI `test_sensor_*` selection includes these sensor cases; the
parser policy regression runs in `native`. ASan uses the temporary local runtime
and isolated configuration already described in the
[scope validation](sensor-trace-scope.md#automated-validation), with existing
`ASAN_OPTIONS=detect_leaks=0`. No repository sanitizer flags were changed. These
are local CI-equivalent runs. Existing FAQ anchor diagnostics remain INFO-level
and do not fail strict documentation validation.

All firmware regression environments compiled and generated DFU/UF2 artifacts:

| Environment | Compile / UF2 | Static RAM bytes | Flash bytes |
| --- | --- | --- | --- |
| `Xiao_nrf52_bme680_sensor` | Passed / Passed | 37,164 | 355,172 |
| `Xiao_nrf52_sensor_baseline` | Passed / Passed | 31,116 | 315,700 |
| `Xiao_nrf52_companion_radio_usb` | Passed / Passed | 143,408 | 372,464 |
| `Xiao_nrf52_repeater` | Passed / Passed | 34,596 | 360,880 |
| `Xiao_nrf52_room_server` | Passed / Passed | 38,284 | 353,312 |

The sensor grows from 37,156 to **37,164 bytes static RAM** (+8), and from
353,876 to **355,172 bytes flash** (+1,296), using the same Nordic nRF52 10.12.0 /
ARM GCC 14.2.1 toolchain as the reviewed build. No discovery/probe submission
limits, trace format, averaged SNR rule or existing radio duty policy changed.

[Recorded check summaries](sensor-trace-recovery-checks.txt).

## Corrected artifacts

The recovery OTA file is a direct nRF52 DFU ZIP, with `manifest.json`,
`firmware.dat` and `firmware.bin` at its root. It is not an outer distribution
archive. ZIP CRCs and manifest references were checked. SHA-256 checksums:

- `.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Trace_Recovery_OTA_2026-10-25.zip` (355,974 bytes):
  `ea34c6464bba315ee853ad1c3ddd259a1c52f7789f2edc5bb9d608ab4c300a02`
- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2` (710,656 bytes):
  `7c83b9ed938f7dc7441fee19bf19287ae6996b6e41e4bf215e6f5521f3cb7c24`

The newly rebuilt artifacts supersede earlier packages at the standard build
paths. The separately named recovery OTA identifies this correction explicitly.

## Physical acceptance and remaining uncertainty

Not run. No device was flashed. Install the corrected firmware and confirm
4df5's ACL/telemetry permission for the sensor's full identity and the actual
radio parameters. Verify status countdown, cancel-and-restart after thirty
seconds, eventual timeout with an intentionally non-responding target, successful
routing, scope retention, TX completion/multipart delivery and late replies on a
controlled mesh. Compare RTT and directional average SNR with captures.

This review establishes the software recovery faults and fixes; it does not
establish that the user's thirty-second busy reply was caused by an indefinite
lock, a remote permission failure or a physical radio problem. Hardware/OTA
installation remains a separate acceptance gate.
