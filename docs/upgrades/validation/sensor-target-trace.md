# Destination trace implementation and validation

Date: 25 October 2026 (Australia/Sydney).
Working tree based on `99dc1448`, extending the
[average SNR implementation](sensor-trace-average-snr.md).

## Behaviour

`!hillvue trace 4df5` resolves exactly four case-insensitive hexadecimal
characters against full public keys in the verified, named repeater-advert
cache. One match selects that identity; no match or multiple matches produce
an unknown/ambiguous repeater error without transmitting discovery. Invalid
syntax is ignored like other invalid bot commands. Bare trace is preserved.

The sensor originates a standard authenticated flooded base-telemetry request.
It discovers a fresh outbound path, appends the selected target and reverses the
intermediate path to construct the loop back to itself. Direct neighbours and
one through ten repeaters including the target are supported, with one- or
two-byte path hashes. Unsupported request widths fail before discovery;
unsupported/oversized returned paths are never probed.

The public report remains `Trace: <elapsed> ms RTT (avg SNR)`, with each node
listed once from sensor to target, paired directional averages, and no separate
return-node list. RTT begins at actual loop TX start; discovery, settling and
queue time are excluded. Retry and original-route fallback reuse the existing
probe and immutable report-delivery policies.

The target must already recognise/permit the sensor identity. The bot does not
log in, add ACL entries or grant permissions. Missing responses report discovery
timeout, not that the target is unreachable. Unknown remote identity, permissions,
clock replay checks, congestion and RF conditions may all prevent discovery.

## State and ownership

- One transaction remains busy through discovery, tracing and bounded reporting.
  There are at most two discovery submissions and two trace submissions.
  Discovery admission is bounded to ten seconds per attempt, with the existing
  250/500/1000 ms backoff and bounded jitter. Its receive timeout uses the
  ten-repeater airtime allowance, starting at TX start (at least 30 seconds).
- Discovery tags come from `getCurrentTimeUnique()`, independently of random
  trace identifiers. Authenticated PATH data must match the selected full key,
  a transmitted discovery tag and the captured discovery scope.
- Radio profile changes cancel queued targeted work before Dispatcher can start
  it. In-flight ownership remains with Dispatcher. A late first discovery can
  cancel an unsent discovery retry. Matching late PATHs cannot restart a closed
  transaction or create reciprocal traffic.
- The production scope/peer adapter is instantiated by native tests too. It
  adds at most three transient full-key peer contexts to the baseline bounded
  ACL search: current transaction and two retired contexts, all RAM-only. It
  does not change the sensor ACL or management route arrays.
- The sensor has no other outbound RESPONSE/PATH request owner. Its custom
  binding consumes unowned RESPONSE/PATH messages rather than installing them
  as management routes, including responses older than the transient peer ring.
  Normal non-response management PATH and authenticated request callbacks keep
  their existing runtime owner. This policy is confined to the custom adapter.
- The generic Mesh PATH decoder now checks that the encoded path plus its
  extra-type byte fits inside the authenticated decrypted payload before
  reading either extra fields or invoking a callback. A valid encoding with
  truncated content is rejected; no bot policy was added to core.
- Reports/discovery keep their original channel, scope key and hash width.
  Intervening traffic or default-scope changes do not redirect the transaction.
  Candidate routes must come from fresh authenticated responses; recent
  four-entry history ranks them but cannot invent a route. Targeted history
  includes the full target key, full scope key, channel and radio profile, and
  cannot borrow bare-trace evidence. History expires after fifteen minutes.

Short routing hashes remain hints rather than proof of per-hop identity.
Cached ambiguity still displays a hash rather than a confident repeater name.

## Automated validation

| Check | Result |
| --- | --- |
| `pio test -e native -e native_sensor -e native_kiss_modem` | 329/329 passed |
| `native_sensor_asan` | 223/223 passed, no address-sanitizer findings |
| Targeted suite after refining the truncated-content regression | 49/49 passed in both native and ASan environments |
| `bash bin/check-sensor-boundaries.sh` | Passed |
| `git diff --check` | Passed |
| `mkdocs build --strict` | Passed |

The [recorded summaries](sensor-target-trace-checks.txt) retain the full-suite
and firmware results.

The new suite has 49 cases covering parser rejection, case folding, unknown and
ambiguous prefixes, actual request/response cryptography, colliding peer hashes
and full ACL search capacity, RTC tags, all route lengths/hash widths, bounded
queue/start/allocation failures, wrong tag/MAC/scope, configuration cancellation,
late replies and new-target ownership, alternate retries, paired-SNR fallback,
management ACL preservation and ordinary admin request callbacks. It also checks
wrap-safe discovery clocks and separation of full-target history.

Six additional packet-medium cases exercise actual forwarding plus a
known-client/replay-protected repeater responder: selected-target discovery,
loop and report on both channels and both widths through scoped-only relays;
unknown-client timeout without login; and correct RTC replay-counter handling.
The existing bare trace, scope, averaged SNR, storage and policy suites pass.

The existing CI `test_sensor_*` filter and sanitizer inheritance select the new
suite automatically. ASan uses the same temporary local Fedora runtime and
isolated configuration described in the
[scope validation](sensor-trace-scope.md#automated-validation):

```sh
ASAN_OPTIONS=detect_leaks=0 pio test \
  -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan
```

No repository sanitizer flags were changed. These are local CI-equivalent runs,
not a claim of remote CI execution. Firmware keeps its existing `-w` flag;
MkDocs retains the existing FAQ missing-anchor INFO notice without failing
strict validation.

## Firmware and artifacts

All five firmware regression environments compiled, generated DFU ZIPs, and
converted to UF2 successfully:

| Environment | Compile / UF2 | Static RAM bytes | Flash bytes |
| --- | --- | --- | --- |
| `Xiao_nrf52_bme680_sensor` | Passed / Passed | 37,156 | 353,876 |
| `Xiao_nrf52_sensor_baseline` | Passed / Passed | 31,116 | 315,700 |
| `Xiao_nrf52_companion_radio_usb` | Passed / Passed | 143,408 | 372,464 |
| `Xiao_nrf52_repeater` | Passed / Passed | 34,596 | 360,880 |
| `Xiao_nrf52_room_server` | Passed / Passed | 38,284 | 353,312 |

Compared with the immediately preceding average-SNR sensor build on the same
Nordic nRF52 10.12.0 / ARM GCC 14.2.1 toolchain, static RAM increased from
36,852 to **37,156 bytes** (+304); flash increased from 343,588 to **353,876 bytes**
(+10,288). Usage is 15.8% of the RAM budget and 43.6% of the flash budget.

The newly rebuilt artifacts supersede the binaries at the same paths in the
previous records. SHA-256 checksums:

- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2` (708,096 bytes):
  `591daf20573c8015ab001696616691eb15a04a5a7a7fcd11738e02adc274f0d7`
- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.zip` (354,679 bytes):
  `188248efca00b5f8b88789242cd20efba10073b80c0d598b5c2351d918383c89`
- `.pio/build/Xiao_nrf52_bme680_sensor/firmware.hex` (995,422 bytes):
  `905abd60c84b6418bd8f3a0b550a742aa15a5c10ed8eaa02fe0b215cbca51b76`

## Physical acceptance

Not run. No device was flashed. Verify on a controlled mesh with the sensor
registered as a permitted client on the selected repeater: direct neighbour,
multi-hop target, both hash widths/channels, scoped relay delivery, congestion,
retry and fallback, RTC replay handling after reboot, average-SNR pairing and
actual TX-start RTT. Include unknown-client and ambiguous-prefix controls, and
confirm existing administrator routes/access survive tracing and UF2/OTA
updates. Deterministic forwarding tests do not establish physical RF behaviour.
