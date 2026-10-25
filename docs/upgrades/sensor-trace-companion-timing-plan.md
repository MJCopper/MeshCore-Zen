# Plan: companion-derived trace timing and login-return routes

Status: implemented and software-tested; physical acceptance pending. See the
[validation record](validation/sensor-trace-companion-timing.md).
Date: 25 October 2026.

## Bare-trace budget amendment

Bare `!hillvue trace` uses the same airtime budget engine, but excludes targeted
discovery/login phases and sizes both planned probes from the incoming route.
The sender label from the public message prefix appears in the acknowledgement
as a bounded display name, not as an authenticated identity. A longer passive
alternate discovered after acceptance is selected only when its complete
airtime timeout, scheduled retry delay and remaining shared queue allowance fit
the frozen deadline. If not, retry the original route when its full attempt fits;
otherwise omit the retry and use the existing `RETRY_NOT_SENT` report. Never
extend the advertised X or truncate a started receive window.

Validation adds exact calculated bare-budget coverage, same-route retry,
longer-alternate fallback and retry omission, while retaining targeted naming,
120-second ceiling, existing wire format and final result behavior.

## 1. Intended result and agreed scope

Replace the fixed discovery/login waits and larger TRACE timeout minimum with
companion-derived airtime calculations. Accept the path returned with a valid
flooded guest login, avoiding a redundant discovery request. Preserve two guest
login retries (three attempts total), the existing targeted/bare trace formats,
sensor-outward node order, averaged bidirectional SNR, cancellation, durable
request timestamps, advert fallback and transaction ownership protections.

Sixty seconds becomes a reference, not a mandatory duration or minimum. Freeze
a calculated maximum in the initial acknowledgement. Permit a calculated
maximum above 60 seconds when justified by airtime, route length and retries.
Recommended initial absolute safety limit: 120 seconds. This is an explicit
product policy, not a number copied from the phone app. Do not increase that
limit automatically. Complete immediately when a usable result or definitive
failure exists; never wait merely because the advertised allowance remains.

Do not add persistence of newly discovered successful paths in this change.
Current successful-route history is RAM-only for 15 minutes; persistent advert
hints remain a separate mechanism. That storage enhancement requires its own
format/eviction/freshness plan.

## 2. Verified references and limits of emulation

Local companion firmware (also checked against companion-v1.18.0):

- `examples/companion_radio/MyMesh.cpp`: base 500 ms, flood multiplier 16,
  direct per-entry multiplier 6 and extra 250 ms.
- `src/helpers/BaseChatMesh.cpp::sendLogin`: sends anonymously with the full
  public key; chooses flood for an unknown route; returns an estimated timeout.
- `examples/companion_radio/MyMesh.cpp::CMD_SEND_LOGIN`: sends RESP_CODE_SENT
  immediately after queue admission, including the estimate. This is not actual
  RF TX completion and is not an acknowledgement from the repeater.
- `BaseChatMesh::onContactPathRecv`: saves the returned outbound path before
  passing the embedded login response to the pending-login handler.
- Companion TRACE commands use the direct-timeout calculation.

Public client reference: meshcore-dev/meshcore.js at commit
`26193c059fcd1d0524f1ad5b19fd88287ecc5c61`, `src/connection/connection.js`.
`login()` waits for RESP_CODE_SENT, then uses firmware estimate plus a default
1000 ms allowance. Its TRACE helper also consumes the firmware estimate, with
default extra allowance zero. The native mobile application is closed source;
its exact private timer margins or retry orchestration are not established.
Do not describe public JavaScript behaviour as verified native-app behaviour.

The sensor will emulate the verified formulas and path handling, while keeping
explicit RF queue/TX ownership and post-completion response timing. This is a
deliberate adaptation of the client timer, not an identical start event.

## 3. Pure timing policy

Inputs are milliseconds, final wire lengths, actual active PHY settings and
route-entry counts. Use checked 64-bit intermediates and round advertised seconds
up. No randomness in the calculated bound; include maximum possible scheduled
jitter/backoff in that bound.

For request airtime A:

- Companion flood estimate: F(A) = 500 + 16*A.
- Sensor discovery/login receive allowance: F(A) + 1000 ms.
- Companion direct estimate: D(A,N) = 500 + (6*A + 250)*(N+1), where N is the
  number of path entries in the transmitted packet, not path bytes.
- Sensor TRACE timeout: D(A,N) + 1000 ms. The extra second is an explicit sensor
  margin; do not imply the public TRACE client uses this by default.
- TX watchdog remains 1.5*A + 1000 ms, with integer rounding defined centrally.

Remove the sensor's separate `5000 + 2500*repeaters` TRACE minimum. For an
out-and-back trace through R repeaters, N = 2*R-1, so N+1 = 2*R. Hash width
changes wire airtime but must not multiply the hop count. Use the maximum wire
length reached as TRACE accumulates SNR samples; do not estimate only its empty
outbound payload. Scope bytes, header, cipher padding and path bytes must be
included where applicable.

Use live SF/BW/CR plus actual preamble, CRC and header settings. Do not special
case the string "Australia Narrow" or only SF7. Cache estimates before TX;
TX-start/completion and receive observers must not query the radio for airtime,
write flash or allocate/send further control packets directly.

Use eight seconds as the maximum individual queue lease and ten seconds as the
shared cumulative queue-wait allowance for a transaction. This retains bounded
contention tolerance without budgeting ten separate full queue leases. Track
actual waiting once across overlapping pending operations. Reserve two of those
ten queue seconds for terminal reporting; acknowledgement/control/probes share
the other eight. The calculated total adds the eight work-queue seconds plus
a reporting reserve that already contains its two queue seconds, not ten plus
another two. Backoff and settling
are separate scheduled delays and count against the absolute deadline. Queue
allowances are explicit policy constants, not companion RF response estimates.

Discovery and login each get separate queue, TX and response phases. Their
response clock starts at successful local TX completion, so queueing and own TX
airtime do not consume it. TRACE keeps its existing actual-TX-start clock, with
the companion-derived direct estimate covering the whole trace exchange.
Completion must not reset or extend a TRACE timer. Never arm a response window
from failed, expired, missing, duplicate, wrong-owner or cancelled TX completion.

## 4. Calculate and freeze the request maximum

Introduce an immutable per-request budget rather than global fixed 60/55/40
second arithmetic. It contains accepted time, final deadline, reporting reserve,
queue allowance, PHY/scope identity, candidate route sizing, per-phase windows,
and permitted attempt counts. Build it before acknowledging the request.

Compute the maximum of reachable state-machine branches, not their sum:

1. Initial discovery succeeds, then at most two TRACE probes.
2. Initial discovery times out; up to three logins; a valid login-return path
   goes directly to at most two TRACE probes.
3. Login succeeds without a usable returned route; one remaining ordinary
   discovery request, then at most two TRACE probes.
4. Control attempts exhaust; an eligible advert candidate receives the remaining
   shared TRACE probe allowance, or the request ends promptly with a failure.

Include acknowledgement and up to two terminal report parts, exact TX watchdog
allowances where sequential, response waits, retry backoffs, settling, report
part gaps and the final late-result grace. Do not double-count a TRACE TX
watchdog as an additional serial phase when it overlaps its response timer.
Account for the shared queue allowance once. Budget early/alternate successes
as alternative branches, not extra operations after the failure branch.

Reporting reserve is airtime-derived: enough for the largest permitted terminal
report (two parts), maximum scheduled inter-part delay, applicable TX watchdogs
and a bounded reporting queue allowance. Reserve it once, before admission of
control/probes. Do not retain a universal five-second report reserve if actual
radio airtime makes it insufficient. A delayed application loop cannot guarantee
an on-air report by the deadline; ownership must still close at its next poll.

Route sizing before discovery:

- Bare trace: use the accepted command's actual route.
- Targeted trace with compatible hints: use the largest supported candidate
  scheduled by this plan. Hints are sizing evidence only, never fresh results.
- Targeted trace without a usable route: reserve one supported ten-repeater
  TRACE using the configured supported hash width; make a second probe
  conditional on the time available after discovery. Do not sum two maximum
  ten-repeater probes automatically and revive the old several-minute waits.
- If the discovered route is larger than a preflight hint, recalculate required
  phase admission against the frozen deadline. Never extend the acknowledgement
  promise. Prefer an eligible fitting alternate or report an insufficient-budget
  reason rather than sending a probe with an artificially short receive window.

Calculate the full branch maximum. If it is at most 120 seconds, advertise its
rounded-up value, even when below or above 60. Above 120, use a bounded plan:
first remove the optional second maximum-length probe reservation, then make
remaining optional retries conditional on full-window admission. Three logins
and two probes remain upper bounds, not guaranteed emissions under arbitrary
contention. A known route for which even one probe plus reporting cannot fit
must produce an explicit budget error; an unknown route may still discover a
short fitting path within the capped plan. Do not manufacture an RF timeout by
shrinking a started receive window to milliseconds to force admission.

Freeze X, name and final deadline when accepting the request:
`Trace requested to Y, please wait up to X seconds.` Bare trace keeps its wording
with calculated X. Size the acknowledgement conservatively before formatting
its duration, then verify final encoded length/airtime. Retries, renames, status,
late replies or new adverts cannot change X or renew the deadline.

Replace the fixed 40-second control cutoff with live admission: before each
control attempt, reserve one eligible TRACE probe plus terminal reporting. If
there is not enough time for another full control exchange, use an available
advert route or end with the stage-specific reason. Do not spend the probe
reserve on repeated authentication. At the final deadline, cancel owned queued
packets and release trace ownership; Dispatcher retains in-flight ownership.

## 5. Accept the login-return path

Extend the scope adapter's embedded PATH/RESPONSE handling to pass both the
validated login body and outbound path to the current trace owner. Do not
interpret an embedded login's server timestamp as an echoed discovery tag.

Required checks: authenticated decryption against the full selected identity;
owned active transaction; accepted role/body; current radio profile; valid hash
encoding/width/count; sufficient bytes; supported route length; and live deadline.
Use the embedded outbound path, not the outer reply packet's accumulated path.
`TraceRoute::fromOutbound` appends the selected repeater and constructs the
sensor-to-target-to-sensor loop. A zero-intermediate path is valid for a direct
target. Preserve names, ambiguity handling, outward order and paired SNR output.

For a successful login with a valid path, stop login retries and queue a new
TRACE from the main service loop; skip the second ordinary discovery request.
The login proves readiness and supplies a route candidate, not bidirectional
link quality. Only the subsequent TRACE can produce a successful trace report.

For a valid successful login with missing/malformed/unsupported path, record the
specific route rejection and try the one remaining ordinary discovery exchange
if its full allowance fits; otherwise use an eligible advert route. Do not retry
a successful login merely because its attached path was unusable.

A recognizable login response in an ordinary RESPONSE frame can establish
readiness but supplies no path; it follows the same discovery fallback.
Unexpected admin role is not used. Scope drift after full authentication remains
diagnostic as in the current policy; the frozen outgoing scope and public reply
scope remain separate. Never silently install a local management ACL route or
send a reciprocal management PATH as a side effect.

A late valid reply from an earlier login attempt may satisfy readiness while
another attempt is queued, if still inside the owned login operation: cancel
that queued retry before Dispatcher can send it. Do not cancel/free an already
in-flight retry; absorb its completion and avoid another login. Replies after
cancellation/completion or from a retired operation cannot restart work, mutate
saved routes, or create traffic. Because stock login responses do not echo a
request nonce, same-peer delayed replies cannot be perfectly assigned to an
attempt; document that limitation, retain peer-clock evidence, and require a
new TRACE result for success.

## 6. Service integration and diagnostics

Keep all policy inside the BME680 sensor implementation. Prefer a pure timing
helper accepting airtime/count inputs, used by `TraceTimingPolicy`,
`TraceTransactionBudget`, `TraceDiscovery` and `RepeaterTraceProbe`. Avoid changing
companion/repeater wire formats or shared NodePrefs. Pass current budget values
through acknowledgement, phase admission, report delivery and status APIs.

Retain existing `get trace.diag`, `get trace.peer`, and control slots 0–4. Add
bounded private readouts for: PHY settings, request/TRACE airtime, calculated
flood/direct estimates, queue use, advertised total, remaining/report reserve,
login attempt, route source, optional retry omitted and reason, accepted/ignored
login replies, and login-path rejection. Split replies into explicit commands
where necessary to obey the existing 160-byte CLI limit. Status shows active
phase, attempt, phase time and overall time. No unsolicited diagnostics floods.

## 7. Tests required before packaging

Pure policy/budget tests:

- Exact companion formula reference vectors, including zero/invalid airtime,
  fractional rounding, overflow and millisecond wrap.
- Australia Narrow SF7/BW62.5/CR7; SF8 and slower SF9/SF11/SF12; alternative BW,
  CR, preamble and scope settings. Validate estimates against the pinned RadioLib
  calculator, not a constant-airtime radio mock alone.
- One through ten repeaters, both supported hash widths, accumulating TRACE SNR
  bytes, and separation of bytes from entry counts.
- Branch maximum rather than branch sum; no queue/watchdog/report double counts;
  below-60, above-60 and over-120 plans; no lower floor forcing 60 seconds.
- Unknown-route conservative single-probe sizing and shorter discovered routes;
  misleading short hints and a larger actual route; optional attempts omitted
  without truncating an admitted receive window.
- UTF-8 named/unnamed acknowledgement length, rounding X up, immutable text and
  deadline across queue retries, renames, default changes and delayed polling.

Controller/real-Dispatcher tests:

- Queued/busy channel and actual TX completion; no reply allowance lost to
  queueing/own airtime; missing/failed/late/duplicate/wrong TX callbacks.
- Exactly three possible login emissions with fresh durable timestamps; success
  on each attempt stops retries; failure/queue expiry/role/cancel/deadline cases.
- Successful flooded login PATH immediately leads to TRACE with no extra REQ;
  assert packet types, counts, exact loop bytes and timers through real Mesh
  encryption/decryption and Dispatcher callbacks.
- Late earlier-attempt login cancels queued retry; in-flight retry ownership;
  response plus retry/completion in one poll; late same-peer/retired replies.
- Wrong full identity, hash collision, malformed body, wrong path field,
  short/truncated path, invalid encoding, three-byte/oversized path and zero-hop
  path. Role error must not be downgraded into successful route acceptance.
- Login without path uses at most one final ordinary discovery; failure/timeout
  still permits advert fallback. No management-route mutation or reciprocal PATH.
- New TRACE identifiers per probe; original/alternate and late-first-result
  selection, report uniqueness, SNR averaging and node order unchanged.
- Newly calculated deadline, report reserve, expiry/cancel/restart and radio
  changes return every pool slot; no infinite busy and no deferred extra traffic.

Network regression scenarios:

- Fresh empty remote ACL with public guest enabled, known client, remote reboot,
  protected/disabled guest, source clock rollback, persisted advert fallback,
  broken first route and a working alternate, asymmetric links, loss and
  contention, and scoped/unscoped return drift.
- Realistic variable-length airtime and scheduled contention on both sensor and
  repeaters. Existing 100 ms constant-airtime tests remain useful but cannot
  alone validate Australia Narrow budgets.
- Sweep a documented loss/delay matrix and record completion time, retry count,
  declared upper bound and cleanup outcome. Do not interpret deterministic
  network-model timings as physical RF validation.

Build/quality checks: full native/native_sensor/native_kiss_modem suite; existing
native_sensor_asan configuration; all five sensor/baseline/companion/repeater/
room-server buildprog and UF2 roles; sensor-boundary contract; strict MkDocs;
diff checks; RAM/flash comparison. Check that report and acknowledgement budgets
include all encoded parts. Keep ordinary private bot/telemetry behaviours intact.

## 8. Timing evidence, hardware acceptance and delivery

Before declaring the timeout policy complete, publish a generated table from the
actual timing helpers and pinned RadioLib estimates. Include Australia Narrow
with known one/two/four/ten-repeater routes, both hash widths, and an unknown
route; show airtime, one login wait, one TRACE wait, retry branches, queue/report
allowances, final advertised X and any conditional attempts. Include slower
settings showing why a result above 60 seconds is justified or why the 120-second
cap reduces attempts. Do not invent a promised two-hop total from SF alone.

Physical test with 7188: record actual full target/sensor keys, active PHY and
scope; compare one working companion flooded login and one sensor exchange.
Capture TX start/completion, login PATH receipt and accepted route, first TRACE,
final result and queue delays. A warm successful path should proceed promptly
without waiting out any maximum. Confirm initial empty-ACL public login,
subsequent trace, cancel/restart, delayed/lost acknowledgement, and advert-only
fallback. Preserve packet evidence for remaining field failures rather than
assuming credentials or RF reachability from silence.

Deliver a new distinctly named sensor OTA ZIP, unchanged older packages,
validated root manifest/application BIN/DAT and archive CRC, SHA256, matching
HEX/UF2, validation report and timing table. No automatic flashing or publishing.
Acceptance requires all software checks, truthful immutable X, bounded cleanup,
correct trace output and the hardware result clearly recorded as passed or
pending. Software test success alone does not resolve the 7188 field defect.
