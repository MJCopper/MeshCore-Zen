# Trace discovery repair and stationary-sensor advert fallback

Status: software implemented and tested; physical acceptance pending.
See the [implementation validation](validation/sensor-trace-discovery-repair.md).
Date: 2026-10-25. Sensor baseline: `5add24af`; Zen comparison: `6ba09b28`.

This plan implements the findings in the
[sensor/Zen discovery review](validation/sensor-zen-discovery-review.md), together
with login-free advert-route fallback. The sensor is stationary, so retaining
observed routes is useful. Repeaters and radio conditions can still change;
every candidate must be tested by a new TRACE before reporting a result.

## 1. Required user-visible behaviour

Keep the existing commands and output:

```text
!hillvue trace 7188
Trace requested to Hilltop Repeater, please wait up to 60 seconds.
```

Preserve full-key prefix resolution, frozen destination name or lowercase prefix
fallback, unknown/ambiguous errors, bare trace acknowledgement, final sensor-to-
target node ordering, averaged bidirectional SNR and usual result formatting.

For targeted traces, obtain a route through authenticated discovery where
possible. Establish first contact when needed and publicly permitted. If no
authenticated route is obtained, try a stored route inferred from that target's
verified flooded adverts. A usable advert route must allow tracing without any
successful login. Never require an administrator password for tracing.

Use one immutable deadline from command acceptance: stop discovery/probing on
the first running poll at 55 seconds, reserve the final five seconds for report
delivery, and release trace ownership by the first running poll at 60 seconds.
No login, cache hit, retry, late response or new advert extends these deadlines.
Already transmitting packets stay Dispatcher-owned until completion/recovery.
RF receipt and application scheduling cannot be guaranteed by the deadline.

The normal final trace text stays unchanged. Status/debug evidence identifies
whether the candidate came from authenticated discovery or an advert hint.
Neither a login response nor a cached route is itself a successful trace result.

## 2. Concrete routing and first-contact policy

Recommended default sequence after acknowledgement:

1. Send one ordinary authenticated path-discovery request. Existing clients get
   the fast path and avoid an unnecessary login.
2. On no usable response, if public guest login is enabled, send exactly one
   anonymous login using the empty public guest credential. It includes the
   sensor's full public key, allowing an unknown requester to establish contact.
3. After an authenticated protocol-valid login response, issue the second and
   final discovery request with a fresh durable timestamp. Do not substitute the
   login response's route for freshly matched discovery in this first repair.
4. If an authenticated route is found, probe it first. Its second probe may use
   another current authenticated route or a compatible advert alternative.
5. If authentication/discovery cannot produce a route, select an advert hint and
   probe it. Use a second distinct advert candidate when available; otherwise a
   bounded retry of the same route is allowed while time remains.
6. If no candidate exists, remain passively eligible for a new verified advert
   during the owned request, until insufficient work time remains for a probe.
   No additional discovery flood or background scan is generated. End with the
   appropriate bounded failure.

A configured `off` policy skips guest login. Following the first unanswered
request it can make one remaining ordinary discovery attempt if that can fit;
otherwise transition directly to fallback. It must not use all remaining time
waiting on authentication while an eligible fallback is available. A login that
is not acknowledged transitions to fallback rather than another identical
unauthenticated first-contact cycle.

Default public mode sends one empty credential, never a password list. Do not
reuse the sensor's incoming management passwords or Zen's saved credentials.
Protected guest passwords are outside the initial implementation: tracing such
a target can use adverts, or an already established client entry. Add sensor-only
admin/local configuration `get trace.login` and `set trace.login public|off`,
with a small versioned policy record separate from upstream NodePrefs. A failed
policy save must report failure and retain the previous effective setting.

The target determines the role granted by its login policy. An empty credential
could unexpectedly match a misconfigured empty admin password. Do not claim the
wire protocol guarantees a guest role: reject an unexpected admin grant for the
first-contact workflow, record it, and use advert fallback. Never perform admin
commands, copy credentials, or attempt to undo the target's ACL automatically.
A login necessarily changes remote client state if accepted; local management
ACLs and saved management routes must remain untouched.

## 3. Time allocation and traffic bounds

Keep the current acknowledgement admission/watchdog policy, constrained by the
overall deadline. Add an absolute **control cutoff at 25 seconds from acceptance**
so primary discovery/login cannot consume the whole work window. It includes
any time already spent acknowledging; it is not another 25-second timer after
acknowledgement.

| Control stage | Maximum stage lifetime, including queue/TX/reply wait |
| --- | --- |
| Initial authenticated discovery | 6 seconds |
| One public guest login | 8 seconds |
| Final authenticated discovery | 6 seconds |

Each stage deadline is the earlier of its entry time plus this limit and the
25-second control cutoff. Reply wait begins at actual TX start but cannot
extend that stage deadline. Queue/CAD/duty delays consume the same allowance.
A quick valid response immediately advances the workflow. If the estimated
transmission cannot fit, skip it and use fallback. On slow profiles these stages
can end before a valid but slow response; the fixed user-requested cap takes
precedence. No timer callback queries the transmitting radio.

At the control cutoff, cancel owned queued control packets and select an
available route. Do not free an in-flight packet. Do not queue probes that cannot
fit their estimate and the remaining work time. If an unusually slow in-flight
acknowledgement/control packet occupies the radio past this point, wait only
within the existing work deadline; no phase is entitled to a fresh allowance.

Probe windows retain airtime awareness, divide remaining work among the
remaining allowed probe attempts, and share the 55-second cutoff with retry
pause/settling/grace. The **two-probe total is global**: authenticated-route and
advert-route probing do not each get two attempts. If the first authenticated
route fails, prefer a distinct fresh/previously validated compatible advert
route for the remaining attempt when no better authenticated alternative exists.

Maximum potentially emitting sensor packets per targeted operation:

- One public acknowledgement.
- Two ordinary discovery requests total.
- One anonymous public guest login.
- Two TRACE probes total across all route sources.
- At most two final public report parts.

Thus at most eight application-originated emitting packets, excluding unrelated
commands and other nodes' forwarding. Only definitely-unsent local rejections
may retry within a stage lease. Never repeat an uncertain emission beyond that
stage's count. Late replies do not create extra login/discovery/probe submissions.

## 4. Correct login protocol and transaction ownership

Add a separate bounded `TraceGuestLogin` owner and a `TraceFirstContact`
controller. Reuse the existing discovery/probe owners rather than making login
look like an ordinary telemetry response. Keep the selected full target key,
control context, current generation and retired-response ownership independent
of the mutable name cache.

Stock 1.18 login replies begin with the **target's** RTC timestamp followed by
`RESP_SERVER_LOGIN_OK` and role/version fields. They do not echo the request's
discovery tag. Validate decrypted packet ownership, full target identity,
expected response shape/code, current login state, actual login TX start,
profile and remaining time. Account for encryption padding. Explicitly test the
13-byte stock response and its padded representation.

The login wire format has no echoed request nonce. An unseen delayed old login
reply cannot always be distinguished perfectly from a new one. Treat login
success as advisory readiness only: do not publish a route, update a clock or
change management state from it. Require the subsequent fresh-tag authenticated
PATH response before accepting a primary route. Track last accepted server login
timestamp where available and absorb retired responses without reopening work.

Extend the scope binding to dispatch owned login responses delivered inside
PATH and, where applicable, ordinary RESPONSE packets. Transient peer data is
currently ignored, so that dispatch requires explicit implementation. Intercept
only packets owned by the current/retired trace operation; unrelated management
traffic retains its existing owner. No reciprocal PATH or local management-route
installation should be triggered by trace-owned replies.

A late authenticated discovery reply from an attempt sent in the **current**
transaction may supply a primary candidate while control is still active.
Cancel any superseded queued control packet. A control packet already on air
remains radio-owned. Later valid replies may add candidates only while another
probe can still be selected; they cannot restart completed probing or reporting.

## 5. Scope policy: make the Zen difference explicit

Separate three values: frozen public reply context, outgoing discovery/login
context, and received route-evidence context. A public channel is not a transport
region.

Recommended outgoing policy:

1. Use the sensor's explicitly configured default outbound transport scope.
2. If none is configured, use a resolved scope from the accepted command.
3. Otherwise use unscoped flood. Never fan out across guessed regions.

Use the sensor's configured supported discovery hash width (one or two bytes),
matching Zen's local-setting approach. Capture it once. Public acknowledgement
and final reporting retain the original command's channel/scope/hash-width
context. Advert-route TRACE uses the width actually observed for that route.

For current-transaction authenticated discovery, match Zen's response eligibility:
accept a valid full-target MAC/decrypted PATH with an actually sent matching tag,
valid route and live owner/profile even if its return scope differs. Record that
difference; do not silently discard the response. Preserve normal runtime packet
validation. Do not copy a returned scope into outgoing or public reply context.
This exception is limited to an owned authenticated control response and does
not authorize background foreign-scope route harvesting.

Advert fallback eligibility remains conservative: require the observation to
match the captured outgoing control scope, or both to be unscoped. Unknown or
ambiguous scope observations are retained only as diagnostics, not reusable
fallbacks. History and candidates are keyed by full target, stable radio profile,
control scope and route width; preserve the request-channel dimension of existing
trace outcome history. Advert observations themselves are channel-independent.
Temporary radio changes invalidate active history/candidates immediately, even
when settings later revert. Persistent hints never restore active validated
history automatically.

## 6. Durable request timestamps and clock diagnostics

Add a sensor-only `TraceRequestClock` for outgoing login and discovery timestamps.
Keep them separate from random TRACE identifiers and wall-clock-dependent cache
ages. Use `max(current RTC unique value, next durable request value)` and reserve
an exclusive upper bound before issuing any value from a new block. Proposed
reservation: **256 timestamp values**.

Persist a version, local full identity, upper bound, sequence and checksum using
two independently committed replicated records. A reservation becomes usable
only after both replicas durably contain its upper bound. On reboot, take the
highest valid bound, start beyond that reserved range, and commit the next range
to both replicas before use. This ordering keeps one surviving record sufficient
when its partner is torn/corrupt: no value from a newer range may have emitted
before both replicas covered it. If neither record is trustworthy, do not issue
authenticated timestamps. Missing durability or retry must never reuse a value
that could already have reached a peer. If a current RTC jump exceeds the reservation,
reserve a new range before transmission. Saturation/overflow is an error.

This is a forward-only request-time floor; do not rewrite the real wall clock
from arbitrary peer data. Reserving ahead trades a bounded forward logical-time
jump after restart for reduced flash writes. Document that jump and measure write
frequency. All new journal/cache I/O is deferred to service-loop work, not RX or TX callbacks.
Reserve a timestamp before queueing its packet. The shared allocator covers
both trace login and trace discovery, so login cannot consume a timestamp later
reused by discovery.

First installation has no record: seed from the local RTC and commit before use,
marking clock quality in diagnostics. Migration cannot infer a higher timestamp
previously recorded by a remote client from silence. A corrupt existing journal
or failed reservation prevents further authenticated requests until safe recovery;
use advert fallback where possible. Record the condition precisely instead of
claiming the radio or repeater is unreachable. A successful login does not prove
that a subsequent sender timestamp exceeds the remote replay floor.

Record RTC source/quality, journal health, request logical time and clock-regression
flags. Add a documented local/admin recovery procedure for damaged state or a
legacy remote replay floor ahead of trusted time. Never repair by random request
tags, resetting the remote ACL, importing Zen's identity or trusting an unmatched
login timestamp. Scope this allocator to sensor code; review any other outgoing
authenticated operation under the same identity before sharing the mechanism.

## 7. Verified advert-route collection and construction

Introduce `TraceAdvertRouteCache`, separate from existing name persistence and
validated outcome history. Populate it only from the **signature-verified**
`onAdvertRecv` callback with the actual received packet. The earlier generic RX
observer is not an authenticated advert hook.

Accept the repeater identity from the signed payload, valid flood packet metadata,
one/two-byte hash width, compatible resolved scope and stable radio profile.
A zero-intermediate flooded advert is a valid direct-target hint. Do not treat
a direct/shared contact advert as proof of a zero-hop RF neighbour. Reject
malformed paths, unsupported widths and paths requiring more than ten total
repeaters, including the target.

If a received target advert has intermediate path `[R1, R2, ... Rn]` from target
towards sensor, infer outward intermediates `[Rn, ... R2, R1]`, append the target's
hash, then reverse the outward intermediates for the return leg:

```text
advert: target -> R1 -> R2 -> sensor
TRACE:  sensor -> R2 -> R1 -> target -> R1 -> R2 -> sensor
```

Build through an explicit advert-to-`TraceRoute` converter, not the incoming
public-command path helper. Test byte order and hop count against actual wire
packets for both widths and every supported length.

The advert signature authenticates origin identity/data, **not the mutable flood
path**. Reversing an advert supplies a route hint, not proof of symmetric links
or cryptographic proof that a short-hash TRACE passed through that full identity.
Existing ambiguous hop labels must remain ambiguous. A new round-trip probe is
mandatory; no success, RTT or SNR may be copied from stored observations.

Core first-packet deduplication means duplicate routes from the same advert
normally never reach the verified callback. Collect alternatives from different
verified advert emissions over time; do not change core deduplication merely to
collect duplicate-path copies.

## 8. Stationary-node cache, persistence and freshness

Recommended limits: **48 full target identities, two distinct advert candidates
per identity**, with a static route-cache budget of **10 KiB**. Store compact
intermediate paths (up to nine hashes of two bytes), not duplicate expanded loops.
Measure actual struct sizes and enforce bounds. This matches the known-device
capacity without relying on mutable name-cache slot indices.

Keep full key, scope, width, stable PHY fingerprint, local observation time,
origin advert timestamp/fingerprint, last-hop SNR, provenance and validation/
failure metadata. Stable PHY fingerprint uses actual relevant configuration;
a reboot-local generation counter alone cannot identify persisted radio settings.
Temporary-radio observations are RAM-only. Clear live candidates/history on any
radio change; restored disk hints are candidates requiring new validation.

Use a **24-hour observation-age limit** when age can be established reliably.
This suits a stationary sensor and infrequent repeater adverts better than a
short discovery window. A stored prior TRACE success may rank the route but
cannot refresh its advert observation time. Last-hop SNR is a tie-breaker,
not an end-to-end score. Prefer previously validated, recent compatible distinct
routes; use shorter airtime/hops as further ranking evidence.

Repeated copies of a remembered advert must not refresh observation freshness.
Record timestamp/fingerprint regression rather than presenting a replay or a
repeater-clock reset as a fresh advertisement. A new clock-regressed observation
may be kept as a probationary hint and tested by the bounded user-owned TRACE;
it must not become authenticated fresh route evidence. A bounded fingerprint
cache cannot prove freshness of every historic signed replay; do not claim it can.
No advert receipt itself triggers radio traffic.

Persist a new versioned sidecar file, leaving the existing name-cache schema
unchanged. Include local sensor identity, record count/size, stable radio metadata,
CRC and observation provenance. Use atomic replacement/backup recovery, a
30-second dirty coalescing delay and at most one routine save per ten minutes. Do not rewrite
for unchanged duplicate adverts or per trace request. Save failures retain RAM
candidates and last valid disk state; all I/O is outside radio callbacks.

Persisted hints have three eligibility states:

- Known age, within 24 hours and matching identity/profile/scope: normal fallback.
- Known expired/incompatible/corrupt: ineligible.
- Age unknown after reboot without trusted wall time: probationary fallback,
  tested only by a new bounded probe. Do not reset or invent an advert observation
  time, or call it freshly discovered. A successful TRACE records new validation
  time separately. This permits stationary-node recovery without pretending a
  volatile RTC supplies reliable calendar age.

On repeated route failure, lower rank and prefer another candidate; do not
permanently erase a potentially useful stationary route based on one collision.
Never return an old selected result without a new probe. Replacing/evicting a
name-cache entry cannot retarget a route record: full keys are authoritative.

## 9. Diagnostics and messages

Add bounded RAM/debug evidence for first-contact stage and route provenance:

- Full selected identity, attempt/generation, queue/TX/completion timing and
  packets actually submitted versus started.
- Guest mode, response protocol validation, returned role and advisory login state.
- RTC source, durable timestamp/journal health and clock-regression risk.
- Outgoing scope/width, received scope mode/fingerprint and mismatch accepted.
- MAC/decryption/tag/owner/profile/scope/path rejection checkpoints where visible.
- Advert candidate availability, age confidence, width, source and selected route.

Core decryption failures may require a narrow generic observer or target-side
packet capture; do not assert visibility that the service does not have. Keep
sensor policy out of core Mesh/Dispatcher. Expose safe status/debug summaries,
not shared secrets, passwords or raw private keys. Public silence must never be
presented as a proven permission denial or replay attack.

Suggested status phases: acknowledging, authenticated discovery, public guest
login, discovery after login, waiting for advert route, testing advert route,
probe retry and reporting; include remaining overall seconds. Final failures
should distinguish no discovered/advert route, no acknowledged guest login,
clock/storage issue and attempted-route timeout where evidence supports them.
Keep one terminal report; do not emit contradictory preliminary failures while
fallback can still succeed. Unknown/ambiguous target errors remain unchanged.

## 10. Implementation boundaries and sequence

| Area | Planned responsibility |
| --- | --- |
| `SensorBotService` | Frozen public/control contexts, fixed deadlines, fallback transitions, one final outcome |
| `TraceFirstContact`, `TraceGuestLogin` | Bounded initial discovery/login/fresh discovery orchestration and ownership |
| `TraceDiscovery` | Fresh-tag authenticated matching, retired tags and up to two ordinary submissions |
| `SensorScopeBinding`, `Bme680SensorMesh` | Correct anonymous request creation, owned PATH/RESPONSE dispatch, verified advert packet/context capture |
| `TraceRequestClock` | Atomic timestamp reservation and diagnostics, separate from wall-clock trust |
| `TraceAdvertRouteCache` and converter | Full-key route evidence, compact paths, persistence/aging/ranking |
| Sensor-only configuration hook | Public guest/off setting; no upstream NodePrefs or credential reuse |
| `RepeaterTraceProbe`, timing helpers | Shared two-probe count, source-aware candidate selection and remaining-time limits |
| Diagnostics/status and documentation | First-contact/scope/clock/provenance visibility and exact bounded behaviour |

Implement in reviewable steps: reproduce missing first contact with a production-
faithful target; add diagnostic owners and clock journal; implement guest protocol;
separate scope contexts; collect/cache advert hints; integrate source selection
and global attempt budget; add persistence/reboot recovery; finish regressions,
builds and hardware acceptance. Preserve existing working-tree changes and avoid
unrelated upstream or Zen modifications.

## 11. Test matrix

### First contact and real 1.18 semantics

- Empty target ACL, public guest enabled: initial discovery silence, one valid
  anonymous login, fresh-tag discovery/PATH, successful TRACE. No test may
  pre-register the sensor to make this case pass.
- Already known client: first ordinary discovery succeeds, no login is sent.
- Public guest unavailable, protected password, wrong/no login reply, public mode
  off and unexpected admin grant: bounded transitions to advert fallback; no
  password guessing or admin operation.
- Guest success but lost reply, delayed old login, padded/truncated/malformed
  login, wrong peer/key/MAC/code/role and stale server timestamp: no false route
  commit. Require fresh ordinary discovery ownership.
- Remote reboot loses guest state; next operation re-establishes it. Sensor's
  management ACL/routes remain unchanged; trace-owned PATH causes no reciprocal
  traffic. Unrelated management traffic keeps its normal owner.

The existing simulated target is insufficient by itself. Add a native harness
that executes the production stock 1.18 login/request reply-selection logic or
refactor only the needed production policy into shared testable helpers with
unchanged behaviour. Retain full encryption/Dispatcher integration. Also test
against actual unmodified 1.18 hardware before declaring field success.

### Advert conversion and login-free fallback

- Verified flooded adverts for one through ten total repeaters and both widths:
  exact reversed intermediates, appended full-target hash prefix, return loop
  bytes and normal outward-order/averaged-SNR output.
- Fresh and restored direct-target zero-intermediate flood hints; direct/shared
  contact adverts do not create a false direct RF route.
- Empty remote ACL and unavailable guest access: advert fallback succeeds with
  no successful login and no seeded remote client entry.
- No guest credentials, authenticated discovery unavailable and two candidate
  paths: first broken/asymmetric path fails, second valid route succeeds within
  the global two-probe and 60-second budgets.
- Authenticated route first, alternate advert used for the one remaining probe;
  no third probe or restarted timeout.
- Invalid advert signature, wrong device type, unsupported width, overlong/
  malformed hint and foreign/unresolved scope do not become eligible. Bound
  looped candidates without mistaking repeated ambiguous short hashes alone for
  proof of a loop; no route bypasses the round-trip validation requirement.
- First-packet duplicate handling, later independent adverts, duplicate timestamp/
  fingerprint, timestamp regression and replay never manufacture fresh validated
  outcomes. Last-hop SNR alone cannot establish end-to-end success.
- A fresh advert during passive wait can supply a route only for the live
  transaction; cancellation/completion/expiry cannot trigger new traffic.

### Scope, identity and history

- Same-scope, target-default fallback scope, unscoped fallback and differing
  reply scope: current full-target/tag authenticated replies follow the explicit
  acceptance policy; mismatches are recorded, outgoing/public context stays frozen.
- Wrong MAC/full key/tag/owner/profile rejected even if short hashes match.
- Same four-character prefix on two identities rejected; same name on different
  identities cannot mix discovery, persisted hints or history.
- Both public channels, both configured discovery widths, advert width different
  from command width, and configured-default versus request-scope selection.
- Radio change and reversion invalidate active candidates/history; persisted
  incompatible/temporary hints cannot revive as fresh evidence.

### Time, persistence and failures

- Sensor reboot with remote replay state retained: reserved timestamps always
  exceed previously possibly emitted values; login and discovery share allocation.
- RTC rollback/forward jump, journal first install/migration, wrong local identity,
  checksum failure, torn replicated-record write, failed reserve, exhaustion and
  overflow. No uncommitted value may emit; fallback remains possible.
- Cache save/load, corrupt/truncated/version/count/CRC records, failed save/rename,
  reboot without trusted UTC, 24-hour boundary, eviction and restored probationary
  hints. No fabricated observation age or stale result replay.
- Verify write coalescing/ten-minute route-save limit and timestamp reservation
  write count. No filesystem or radio estimation in TX callbacks; bounded memory.
- Every phase at its deadline minus one, exact deadline and plus one; control
  cutoff 25 seconds, work cutoff 55, final deadline 60, rollover and skipped polls.
- Packet allocation/queue rejection, CAD/duty contention, definite/uncertain TX
  failure, missing start/completion and late callbacks after packet reuse.
- Cancel/status/busy/dedup/rate-limit throughout all new states; a new trace starts
  after every terminal path without reboot. Never free Dispatcher-owned TX memory.
- One frozen named acknowledgement, at most one login, two ordinary discoveries,
  two probes and two terminal report parts. Verify real emitted packet counts,
  not just state transitions.

### Regression, firmware and documentation gates

Run focused first-contact, advert, clock, scope and lifecycle tests; then all
native/native_sensor/native_kiss_modem suites and the working sensor ASan setup.
Build sensor, baseline sensor, USB companion, repeater and room-server XIAO
nRF52 environments; generate applicable DFU/UF2 artifacts. Run sensor-boundary,
whitespace and strict documentation checks. Compare RAM/flash, enforce the route
cache budget and validate all generated DFU manifests/CRCs. Record exact revision,
results, sizes and SHA256 values in a new validation report. Package an updated
uniquely named OTA when requested; do not flash or publish automatically.

## 12. Hardware acceptance and completion

Capture a working Zen and a sensor transaction against 7188 (stock 1.18) and the
additional repeaters Zen can trace without known prior access. Record identities,
PHY, packet types/order, scopes, tags and timings without assuming their ACL state.

Required field cases: fresh sensor identity with no remote ACL entry; known client;
public guest disabled/protected; successful advert-only fallback; sensor reboot
without external RTC; remote reboot; radio change/reversion; one broken advert
route and valid alternate; cancellation; and repeated requests after timeout.
Preload the stationary sensor with genuine received adverts, reboot it, and verify
fallback still works through persisted hints without manufacturing a fresh advert.

Completion requires a successful fresh first-contact case **without fixture ACL
seeding**, a successful login-free advert fallback, preserved result formatting,
correct scope/clock diagnostics, all bounded ownership/traffic/timing regressions,
and recorded hardware evidence. Software-only validation must be labelled as
such until the physical cases are actually performed. No firmware code or tests
are changed by this planning document.

## Approved guest-login timing amendment — 25 October 2026

The original eight-second combined guest-login lease is superseded. Use an
independent eight-second queue allowance, an airtime-based TX watchdog starting
at actual TX start, then an eight-second receive window starting at successful
local TX completion. All phases remain bounded by the original 25-second control
cutoff, 55-second work cutoff and 60-second overall deadline. Do not extend
ordinary discovery stage leases or increase login emissions. An expired queue,
failed/missing completion, cancellation, or late/duplicate completion cannot
start or reset a receive window. Status distinguishes queued, transmitting and
awaiting reply. Tests cover busy-channel queueing, actual completion, exact
boundaries, late replies, stale callbacks, cancellation and millisecond wrap.

## Approved guest-login retry amendment — 25 October 2026

Allow two retries of guest login (three transmissions total), with distinct
forward-only request timestamps and bounded backoff after an unanswered reply
or failed transmission. Stop on accepted login response, unexpected role,
cancellation, queue expiry, clock failure or exhausted control time. Retain
separate queue, TX and post-completion receive phases. To accommodate three
eight-second reply windows plus initial/final discovery, replace the 25-second
control cutoff with 40 seconds from acceptance. The 55-second work and 60-second
overall cutoffs remain unchanged. This is an upper bound on retries, not a
promise all three fit after arbitrary queue delays. The maximum targeted-trace
application emissions become ten. Preserve diagnostics slots 0/1/2 and add
slots 3/4 for the login retries. Successful-route persistence is not changed.
