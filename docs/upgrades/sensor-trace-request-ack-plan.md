# Trace request acknowledgement: implementation and test plan

Status: historical plan; the calculated maximum is superseded by the fixed
[60-second cap](validation/sensor-trace-60-second-cap.md). Date: 2026-10-25.

See the [implementation validation record](validation/sensor-trace-request-ack.md).
The following sections retain the approved implementation and test requirements.

## Intended behaviour

When the sensor accepts a new trace, immediately schedule this single message:

> Trace requested, please wait up to X seconds.

`X` is the rounded-up maximum duration of the whole accepted operation, including acknowledgement, discovery where needed, both possible probe attempts, settling, retry delay, final grace, and final report delivery. This applies to both `!hillvue trace` and `!hillvue trace 4df5`.

Keep the normal sensor-name prefix and send through the accepted request's reply channel and scope. Keep the existing final trace output, sensor-to-selected-repeater ordering, and bidirectional averaged SNR unchanged.

“Immediately” means queued without application jitter after validation and acceptance. Actual transmission must still respect the radio, duty limits, and busy-channel checks. The advertised duration bounds application work and releases trace ownership on the first running service poll at or after its deadline; it cannot guarantee that a receiver hears a packet or that a stalled event loop runs on time. An already transmitting packet remains owned by the Dispatcher until completion or its existing recovery mechanism.

Invalid, ambiguous, unknown-target, rate-limited, duplicate, and busy requests keep their existing responses and do not receive this acknowledgement. Neither `trace status` nor `trace cancel` starts another acknowledgement or resets a deadline.

## Acceptance and lifecycle

1. Parse the request and apply existing deduplication, rate limits, and busy handling.
2. Validate the route for a bare trace, or resolve a targeted prefix to one full repeater identity. Validate supported path widths and other start prerequisites.
3. Capture the full target or original route, reply context, radio profile, and monotonic acceptance time. Compute the maximum budget before marking the request accepted. Reject unsupported or unrepresentable timing configurations with a clear error.
4. Freeze `X`, the acknowledgement text, and an absolute transaction deadline. Charge the accepted command once under the existing rate policy.
5. Enter a separate, bounded acknowledgement stage. Attempt transmission with zero application delay. Once the acknowledgement completes, fails, or exhausts its allowance, proceed with tracing regardless of acknowledgement success. Do not wait for a remote receipt acknowledgement.
6. Start discovery for a targeted trace, or the existing settling/probe flow for a bare trace. Discovery and probe queue timers start after the acknowledgement stage; acknowledgement airtime must not consume their admission budgets.
7. Run the existing trace attempts and final report within the frozen overall deadline. Finish early whenever possible.
8. At expiry, stop further trace admissions, cancel only owned queued packets, retire transaction callbacks, and release busy state. Do not begin another report budget after expiry.

Use a small independent acknowledgement owner, proposed as `TraceRequestAcknowledgement`. It must not reuse the final `TraceReportDelivery` state or an ordinary reply queue with an unbounded retry lifetime. States should distinguish waiting, queued, transmitting, and terminal outcomes. Permit local retries only while emission is definitely known not to have occurred and within the admission deadline. Once transmission starts or emission is uncertain, do not create another acknowledgement copy.

Cancellation, radio-profile invalidation, or an early terminal outcome must remove any acknowledgement still queued. Late callbacks must be matched to their owning packet/transaction and cannot mutate a newer trace. Preserve Dispatcher ownership of packets already on air.

## Budget calculation

Add a pure budget composer alongside the existing timing policy. Use 64-bit intermediates and enforce the existing wrap-safe maximum interval of `0x7FFFF000` milliseconds. Never silently clamp a combined budget below the time its phases can consume.

Initial conservative policy:

| Symbol | Allowance |
| --- | --- |
| `Q` | Existing discovery/probe queue admission: 10,000 ms each |
| `QA` | New acknowledgement queue admission: 10,000 ms |
| `aMax` | Airtime estimate for `MAX_TRANS_UNIT` under the captured radio profile |
| `W` | Existing `transmitWatchdog(aMax)` |
| `A` | Acknowledgement: `QA + W` |
| `D` | Discovery receive allowance: `attemptTimeout(10, aMax)` |
| `Pmax` | Probe receive allowance using the maximum supported ten-repeater loop packet, including two-byte hop hashes |
| `P0` | Bare trace: allowance for its captured original route; targeted trace: `Pmax` |
| `P1` | `Pmax`, since a fresh alternate route may be longer than the first |
| `S` | Existing hard settling cap: 2,500 ms |
| `J` | Maximum retry pause: 2,000 ms |
| `G` | Existing final grace: 3,000 ms |
| `R` | Existing `reportBudget(aMax)` |
| `F` | Existing failure report allowance: 10,000 ms |
| `L` | Conservative terminal delivery allowance: `max(R + 2*W, F + W)` |
| `M` | Explicit processing reserve: 1,000 ms |

Calculate:

```text
discovery = targeted ? 2 * (Q + D) : 0
total_ms = A + discovery + S + (Q + P0) + J + (Q + P1) + G + L + M
X = ceil(total_ms / 1000)
deadline = accepted_at + total_ms
```

The acknowledgement's maximum-frame allowance avoids a circular dependency between the number of digits in `X` and acknowledgement airtime. Cache the actual packet's airtime/watchdog before enqueueing it; verify it fits the planned allowance. Do not estimate airtime or access radio configuration from a TX-start callback.

During implementation, verify the maximum loop packet length against the actual packet serializer and supported hop count. Receive allowances start at actual TX start and already cover transmission time; do not add that airtime again. Verify they cover the corresponding TX watchdog. The existing absolute report budget covers admission retries and the inter-part gap; do not add those twice. `L` deliberately reserves conservative TX completion allowances for the final report parts.

Use the captured radio profile for every component. Discovery must remain bounded to two attempts and probes to two attempts. Fresh route observations can shorten actual work, but must not increase the promised maximum. A radio-profile change terminates the transaction rather than recalculating a longer deadline.

The reserve is a scheduling allowance, not permission to extend the deadline. Every admission and retry must also obey the remaining overall budget. Define whether a phase can complete with the remaining time before admitting it; otherwise transition to an appropriate terminal outcome while report time remains. Deadline cleanup itself must work even when no report time remains.

## Code changes

| Area | Planned change |
| --- | --- |
| `TraceTimingPolicy.h` and, if useful, a small transaction-budget header | Pure composition, checked arithmetic, rounded seconds, and wrap-safe remaining-time helpers |
| New `TraceRequestAcknowledgement.h` | Bounded state, immutable text/context, packet ownership, and terminal outcomes |
| `SensorBotService.h/.cpp` | Acceptance snapshot, acknowledgement prelude, fixed deadline, callbacks, cancellation, profile-change handling, and busy ownership |
| Discovery/probe/report helpers | Only the interfaces needed to constrain work to the transaction deadline; preserve existing phase timing and recovery semantics |
| `PublicChannelSensorBot` | Preserve parser, deduplication, and rate behaviour; change only if acceptance feedback requires it |
| Native test fixtures | Realistic packet airtime, controllable clocks, and acknowledgement/failure observations |
| Public bot and development documentation | Explain acknowledgement, conservative maximum, early completion, status, and cancellation |

Keep acknowledgement memory bounded and avoid per-request heap allocation. Do not add wire protocols, authentication exchanges, persistent storage, or repeater ACL changes. Authenticated discovery continues to use its existing monotonic RTC timestamp, independently of transaction identifiers and millisecond deadlines.

Extend `trace status` to identify the acknowledgement stage and show remaining overall seconds alongside useful existing phase detail. Keep `trace cancel` usable throughout acknowledgement, discovery, probing, grace, and final reporting, including when normal start-command rate slots are exhausted.

## Test plan

### Budget and timer tests

- Independently calculate expected totals for bare and targeted requests. Cover one through ten repeaters, both supported hash widths, short and maximum-sized packets, and representative fast/slow radio profiles.
- Confirm targeted budgets include two discovery admissions and receive windows. Confirm bare budgets exclude discovery and allow a longer second route.
- Confirm acknowledgement admission, report splitting, maximum retry delay, settling, grace, and conservative TX completion allowances are included exactly as specified.
- Test millisecond-to-second rounding, digit boundaries in `X`, zero or invalid airtime estimates, checked addition, maximum safe intervals, and rejection rather than unsafe saturation.
- Exercise acceptance near millisecond rollover. Check deadline minus one millisecond, the exact deadline, deadline plus one, and service polls that jump across several phase boundaries.
- Verify queue, discovery reply, status, and retry events never move the original deadline.

### Acceptance and acknowledgement tests

- Accepted bare and targeted commands produce the exact message body once, using the expected node prefix, channel, and scope.
- Invalid syntax, missing bare routes, unknown/ambiguous target prefixes, busy responses, duplicates, rate limits, status, and cancellation produce no request acknowledgement.
- No added application jitter; ordinary Dispatcher channel access rules still apply.
- The acknowledgement does not overwrite final report text, target identity, route state, SNR values, or reply context and does not consume another command-rate slot.
- Discovery/probe admission clocks begin after the acknowledgement stage, including when the acknowledgement occupies the radio for longer than a probe's normal queue allowance.
- Allocation failures, queue rejection, definitely-not-sent failures, uncertain emission, missing TX-start, and missing completion all leave the acknowledgement stage within its bound. No uncertain or started transmission creates another emitting copy.
- Oversized prefixed messages and formatting failures have a defined bounded outcome; tracing does not stay busy because acknowledgement formatting failed.

### Lifecycle and recovery tests

- Cancel at each acknowledgement state and every subsequent phase; confirm owned queued packets are removed and a new trace can start.
- Change the radio profile during each acknowledgement state and later phases; no recalculated deadline or stale acknowledgement survives.
- Deliver delayed discovery replies, TX-start callbacks, and TX-complete/failure callbacks after cancellation, expiry, completion, and a subsequent accepted request. They must not change the newer transaction.
- Hold the channel busy or deny transmission until admission expiry. Exercise both discovery failures, first-probe timeout, retry, final grace, and two-part final delivery.
- Cross the overall deadline with queued and in-flight packets. Queued packets are cancelled; Dispatcher-owned transmitting packets are not freed or reused by the service. Busy state and stale-callback protection remain correct.
- Ensure expiry never opens another full failure-report window or produces a contradictory failure after a possibly emitted success report.
- Preserve trace RTT measurement from actual probe TX start, averaged link SNR, alternate-route selection, history scoping, late-result fallback, and existing final formatting.

### Real Dispatcher and network tests

Use the real Dispatcher and existing sensor network fixtures, beyond state-only mocks, to verify packet-pool ownership, actual TX-start timing, CAD/duty delays, cancellation of queued packets, failed transmission ownership, and callback order. Model realistic slow airtimes so an unrealistically short test TX duration cannot hide contention between acknowledgement and tracing.

Cover both public channels, scoped and unscoped replies, direct and multi-hop paths, and final reports of one and two parts. Assert bounded packet counts and balanced packet-pool ownership after completion and recovery. Keep regressions behavioural rather than duplicating implementation formulas in the test oracle.

### Build and regression gates

Run these after implementation, with results recorded against the implemented revision:

1. Focused acknowledgement/budget tests plus existing public-bot, target-trace, trace-reliability, network, and Dispatcher tests.
2. Full native suites: `pio test -e native -e native_sensor -e native_kiss_modem`.
3. Sensor AddressSanitizer suite using the documented working host configuration; distinguish infrastructure failures from test failures.
4. Firmware builds and applicable DFU/UF2 generation for `Xiao_nrf52_bme680_sensor`, `Xiao_nrf52_sensor_baseline`, `Xiao_nrf52_companion_radio_usb`, `Xiao_nrf52_repeater`, and `Xiao_nrf52_room_server`.
5. `bash bin/check-sensor-boundaries.sh`, whitespace checks, and the strict documentation build.
6. Compare sensor RAM/flash with the pre-change build; confirm bounded memory use and no unintended cross-role dependency.

Use the installed PlatformIO executable or its virtual environment where needed. Preserve existing unrelated working-tree changes. Do not publish or install firmware as part of planning. Produce a fresh OTA package only after implementation/build validation if requested, with revision and checksum recorded.

### Physical radio acceptance

On test hardware, record firmware revision, radio settings, channel/scope, topology, and observed timings. Test bare and targeted traces, a known target such as `4df5`, fast and slow profiles, reachable and silent targets, and a busy channel. Capture acceptance, acknowledgement TX, each probe TX, terminal reply, and busy release times.

Check that the acknowledgement has no added application delay, its `X` matches the calculated frozen maximum, and normal success completes early. Repeat the original scenario: start `trace 4df5`, request another trace after 30 seconds, inspect status, then cancel or wait for the advertised deadline. A busy reply before that deadline can be correct; a busy transaction persisting beyond expiry on a running service loop is a failure. Confirm a subsequent trace starts after terminal cleanup.

Verify cancellation during acknowledgement and discovery, a lost final reply, and recovery after channel contention. Physical receipt and SNR accuracy require this hardware evidence; native tests alone do not establish them.

## Implementation order and completion criteria

1. Add checked budget composition and independent timing tests.
2. Add the bounded acknowledgement owner and ownership/failure tests.
3. Integrate acceptance, acknowledgement prelude, fixed deadline, status, cancellation, and profile-change cleanup.
4. Add real Dispatcher/network regressions, including late callbacks and slow-radio contention.
5. Update user/developer documentation, run all regression/build gates, and record hardware results separately.

Complete when every accepted start schedules at most one emitting acknowledgement, the displayed maximum covers every permitted phase, all exits release trace ownership within the defined software deadline contract, and existing trace output and recovery behaviour pass regression tests. Mark hardware validation pending until actually performed. This document itself changes no firmware.
