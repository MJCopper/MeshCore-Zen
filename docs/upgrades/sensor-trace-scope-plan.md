# Sensor trace scope and Dispatcher regression plan

Status: implemented; see [validation and physical acceptance status](validation/sensor-trace-scope.md).
Date: 24 October 2026.

## Objective and evidence

Preserve the scope of bot replies through delayed delivery, isolate trace route
evidence by scope, and add focused Dispatcher lifecycle tests. Keep the existing
bounded two-attempt trace algorithm, RTT definition, radio ownership, report
deadlines, channel behaviour and rate limits.

Review found that `Bme680SensorMesh::sendFlood()` calls plain
`Mesh::sendFlood()`, bypassing upstream request/default reply-scope selection.
History currently distinguishes route, channel and radio profile, but not
transport scope. Candidate matching uses the accepted command ciphertext without
scope discrimination. These are implementation gaps; their RF consequences have
not been demonstrated on hardware.

The review ran 76 passing cases across `test_sensor_integration`,
`test_sensor_storage`, `test_sensor_routes` and
`test_sensor_trace_reliability`. This is a baseline, not the complete release
validation suite.

Authenticated discovery is absent from the sensor trace flow. Do not introduce
discovery, RTC replay-protection changes, telemetry-permission diagnostics or
requester identity as part of this work. Traces follow observed repeater hashes
and return to the sensor. Full identities cannot be inferred from those hashes.
Ambiguous cached names already fall back to hex and have regression coverage.

## 1. Establish regression coverage first

Add `test/test_sensor_dispatcher/test_sensor_dispatcher.cpp`, adapted from the
five focused tests on local `zen/main:test/test_trace_dispatcher/`.
Use real Dispatcher and StaticPoolPacketManager with controllable radio/clock
fixtures, under `native_sensor` and its sanitizer environment. Avoid adding
Dispatcher sources to the unrelated native environment just for these tests.

Required assertions:

- Scheduled delay produces no TX-start callback until the radio accepts a send;
  callback time equals the actual start, not enqueue time.
- A busy receiver/channel defers sending without starting the RTT clock.
- A rejected radio start calls failure once, never calls TX-start, and returns
  exactly one allocation to the pool. A later allocation remains usable.
- Cancelling one queued packet leaves another queued packet untouched; repeated
  cancellation is harmless. An in-flight packet cannot be cancelled through the
  queued-packet API and remains owned by Dispatcher until completion/watchdog.
- A normal consumer with no TX-start override retains its baseline lifecycle.

Keep existing service integration tests for queue expiry, pool reuse, late
fallback and committed failure. Add service-level busy-channel coverage so that
the Dispatcher callback and probe RTT are tested together, rather than relying
only on separate unit tests.

Acceptance: these tests pass before scope changes; any pre-existing failure is
triaged separately instead of silently changing core timing behaviour.

## 2. Introduce a value-owned reply context

Add a small application reply-context value, carried explicitly through
`SensorBotTransport`, `SensorBotService`, `PublicResponseQueue` and
`TraceReportDelivery`. It contains the channel, selected reply scope key (or
explicit unscoped state), and reply path-hash width. Keep request scope identity
separate from the selected reply scope: an unresolved request may use the default
for its reply without proving that its observed path belongs to that default.

Capture context synchronously when an accepted command is handled. Store values,
not Packet/RegionEntry pointers, mutable globals, region names, or packet-specific
16-bit transport codes. Transport codes must be recalculated for each newly
encrypted reply packet using its captured key. Do not persist or log keys.

The selected reply scope follows existing `chooseReplyScope()` semantics:

| Accepted request | Reply selection |
| --- | --- |
| Resolved transport scope A | A, even if default is B |
| Explicit unscoped flood | Unscoped, even if a default exists |
| Unresolved transport scope | Current usable default, otherwise unscoped |
| Direct request, if supported by a bot command | Current usable default, otherwise unscoped |

Snapshot the selection at acceptance. A later default-region change or unrelated
received packet must not redirect already pending replies. A new command uses
the new configuration. Radio configuration cancellation/deadline behaviour stays
as currently implemented; scope changes do not restart report budgets.

The necessary region map and scope fields are private in `SensorMesh`. Add a
minimal generic protected API to capture a value-owned reply scope and emit a
packet with an explicit captured scope. Reuse existing resolution and
`sendFloodScoped()` logic. Keep bot-specific types and policy out of upstream
sensor and core code; do not expose mutable region-map internals. Prefer a
shared generic scope-selection helper used by both the existing synchronous
`sendFloodReply()` and the new capture API, preserving current behaviour.

Acceptance: baseline consumers retain identical scope selection and wire
format. Application code can capture and send without holding receive-context
pointers. The boundary script continues to reject bot-policy leakage.

## 3. Propagate context through every reply path

Update the service and queue APIs so context is required at scheduling time,
including single-part replies, ordinary multipart replies, rescheduling after
allocation failure, trace success, timeout/error reports and late fallback.

Keep an immutable active-trace context independent of the new request's context.
A busy notice uses the new request context; the active trace's eventual result
uses the original context. Before closing/resetting a trace, preserve the context
needed to construct its final report. Each queued multipart entry owns its own
context. Trace probes remain direct MeshCore TRACE packets; do not add flood
scope fields to direct trace frames.

Regression matrix:

- Scope A request/default B replies in A; unscoped request/default B remains
  unscoped; unresolved scope uses B or unscoped when no default exists.
- Interleave A and B on both Public and `#hillvue`, including multipart replies.
  Assert every emitted part has the original channel, selected scope and width.
- While trace A runs, receive request B and a busy notice. Verify A's success,
  retry failure and late fallback remain in A, while the notice uses B.
- Change the default and process unrelated receive traffic before delayed send,
  allocation retry and second-part send. Context remains unchanged.
- Decrypt each reply and independently verify its transport code using the
  expected key. Ensure multipart packets get their own calculated codes.
- Repeat cancellation, expiry, uncertain TX failure and pool-exhaustion tests;
  check bounded traffic and complete pool recovery after terminal work drains.

## 4. Isolate request candidates and route history by scope

Introduce an explicit request-scope identity: unscoped, resolved key, or
unresolved. For resolved history compare the complete key bytes, or an adequately
sized full-key fingerprint; do not use a region ID/name or 16-bit transport code
as a durable identity. Keep existing channel, radio profile, exact route and TTL
checks. Full-key changes naturally separate records without changing the
fixed four-entry bound.

For unresolved transport scopes, do not read/write cross-transaction history.
Within a transaction, accept alternate copies only with the same ciphertext,
transport route class and conservative matching transport-code tuple. This may
discard valid alternatives but avoids asserting a scope identity that cannot be
resolved. Document residual short-code ambiguity. Resolved alternatives must
resolve to the original key; unscoped and scoped copies never mix.

Candidate observation occurs through `logRx`, before normal receive dispatch.
Resolve scope from the observed packet itself using a side-effect-free adapter
operation. Never use `recv_pkt_region` left by the previous packet. Keep this
operation bounded and avoid persistent writes. Do not change ciphertext-match,
path validation, three-candidate capacity or settling hard limits.

Required tests:

- Identical routes/channel/radio in scopes A and B have independent outcomes.
- Unscoped, resolved and unresolved requests cannot borrow each other's history.
- Same scope key with a renamed region retains identity; changed key under the
  same region ID does not reuse evidence.
- Different-scope copies of identical ciphertext do not enter candidates or
  extend quiet time. Same-scope copies still qualify.
- History cannot supply a candidate not observed for the current command.
- TTL, capacity eviction, radio changes, temporary apply/revert and millis wrap
  retain their existing behaviour.
- Completion/cancellation clears candidate state. Late old results or command
  copies cannot update history, queue probes or alter a newer transaction.
- Preserve intentional late-result acceptance while a transaction remains open,
  including replacement of an unsent failure and original-attempt fallback.

Acceptance: only fresh observations in the current request scope can be retried;
history changes ranking only and never supplies freshness or full-node identity.

## 5. Test the production binding, not only the service mock

Extend the native fixtures with real transport-code generation and region/key
resolution. Add tests for the generic SensorMesh scope-capture API or a shared
production helper that it directly uses, plus a thin-adapter test proving the
Bme680 binding calls that path. Resolve embedded build dependencies with small
existing-style board/storage stubs if necessary. A mock returning a chosen scope
alone is insufficient evidence for the integration gap being fixed.

Add deterministic network cases in `test_sensor_network`: a relay rejecting
unscoped floods must forward a correctly scoped report; a wrong-scope report
must fail the same check. Cover both channels, delayed multipart and retry
failure. The filter must validate transport codes rather than a fixture label.
Keep the model explicitly separate from RF acceptance.

## 6. Validation and documentation

Run focused tests after each change, then the complete release checks once the
implementation is stable. Use the installed `pio` executable; on the reviewed
machine it is `/home/matt/.platformio/penv/bin/pio`.

```sh
pio test -e native -e native_sensor -e native_kiss_modem
ASAN_OPTIONS=detect_leaks=0 pio test -e native_sensor_asan
bash bin/check-sensor-boundaries.sh
pio run -e Xiao_nrf52_bme680_sensor
pio run -e Xiao_nrf52_bme680_sensor -t create_uf2
pio run -e Xiao_nrf52_sensor_baseline
mkdocs build --strict
```

Also run the remaining firmware targets required by
`.github/workflows/sensor-firmware.yml`, particularly consumers affected by any
generic SensorMesh header/API change. Ensure the new tests are selected by CI
and the sanitizer environment; adjust source filters only when required.

Update the public-bot and development documentation with scope-selection rules,
snapshot behaviour, unresolved-scope limitations and new automated coverage.
Preserve conservative failure wording and ambiguity fallback. Record test
counts, compiler warnings, firmware flash/static RAM deltas against the same
revision/toolchain baseline, and UF2/DFU artifact paths and checksums. Confirm no
key material or bot policy leaks into logs/core code. Do not copy old resource
figures as new measurements.

## 7. Physical acceptance gate

Use a controlled sensor/companion/two- or three-repeater setup. Record firmware
revision, radio settings, scope keys by non-secret labels, topology, hash width
and repeat counts. Capture timestamps and decoded packets with temporary test
instrumentation; avoid logging key material.

1. Set repeaters to reject unscoped flooding and confirm scoped command and
   final report delivery. Verify unscoped and wrong-scope negative controls.
2. Repeat on Public and `#hillvue`, one- and two-byte path hashes, including a
   long multipart report and interleaved requests from different scopes.
3. Change the default scope while a report is pending. The old report keeps
   its accepted context; the next command uses the new selection rules.
4. Exercise contention and slower radio settings. Measure enqueue and actual
   TX-start separately; RTT excludes local waiting and follows the existing
   airtime-aware timeout policy.
5. Exercise retry success, late first-attempt fallback and a lost result.
   Confirm at most two probe submissions and no contradictory final reports.
6. Correlate distinct receive SNRs at each outward hop with report order using
   repeater logs or controlled link differences. Do not infer correctness from
   simulation alone.
7. Check post-test telemetry, admin access, node identity, one-minute sampling
   and normal channel commands. Smoke-test UF2 flashing and DFU update.

Record physical checks as passed, failed or not run. Automated completion does
not imply physical acceptance.

## Delivery sequence and completion criteria

Use reviewable commits: (1) Dispatcher tests; (2) generic scope capture and its
baseline tests; (3) bot context propagation; (4) candidate/history isolation;
(5) production-binding/network coverage and documentation. Put each feature's
regressions in the same commit as its implementation where practical.

Software completion requires all relevant CI suites, sanitizer checks, boundary
checks, firmware builds and documentation validation to pass; bounded memory and
packet ownership must remain intact. Record physical acceptance separately and
leave deployment/installation as a subsequent action. If regression tests expose
an unrelated defect, document it and address it separately unless it blocks this
scope fix.
