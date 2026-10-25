# Sensor versus Zen trace discovery: read-only review

Historical pre-implementation findings. See the subsequent
[repair implementation and validation](sensor-trace-discovery-repair.md).

Date: 2026-10-25. Sensor: `meshcore/sensor` at `5add24af`.
Zen comparison: local `zen/main` at `6ba09b28`.
No firmware, test source, configuration, identity, ACL or radio settings changed.
Only this review document was added. No network request/login was performed.

## Conclusion and confidence

The sensor sends the same authenticated telemetry-based path discovery as Zen,
but it assumes the destination already knows the sensor's full identity. The
sensor does not establish that prerequisite. Stock MeshCore 1.18 cannot decrypt
this request from an unknown client because the packet carries only a short
sender hash. Learning a repeater's advert/name/public key on the sensor does not
register the sensor on that repeater.

User evidence for 7188: it runs MeshCore 1.18, Zen has previously logged into it,
and the sensor has not knowingly been registered or logged in. This strongly
supports missing remote client registration as the cause for 7188. It is a
confirmed protocol/design gap under an unregistered-sensor setup, not a remotely
observed ACL inspection or packet-level proof of this specific failure.

The user also reports Zen success with repeaters they have never had access to.
That observation remains valid and unresolved: administrator access is not
required for this telemetry request, and guest/client identity knowledge differs
from admin access. However, the current Zen trace entry point itself performs
no automatic login either. Static code cannot establish those other repeaters'
client tables, earlier traffic, exact firmware, or the installed Zen build. Do
not claim that a prior guest login definitely occurred. Capture the successful
Zen transaction and compare it to sensor traffic to settle this discrepancy.

Additional confirmed differences are outgoing scope/width selection and strict
sensor reply-scope filtering. A clock-reset replay failure is another concrete
conditional risk. No evidence establishes either as the deployed cause yet.

## Protocol comparison

| Stage | Sensor | Zen |
| --- | --- | --- |
| Destination | Unique four-hex prefix resolves a full cached 32-byte key | Selected repeater's full 32-byte key/contact |
| Discovery type | `PAYLOAD_TYPE_REQ` | `PAYLOAD_TYPE_REQ` |
| Plaintext | RTC tag, `03 FE 00 00 00`, four random bytes | Same fields and sizes |
| Request size | 13 plaintext bytes before encryption/padding | Same |
| Shared secret | `self_id.calcSharedSecret(secret, target_key)` | `ContactInfo::getSharedSecret`, which calls the same operation |
| Source on wire | Sender identity hash, not its full public key | Same |
| Outgoing login inside trace | None | None |
| Send scope | Captured bot request/reply scope, default fallback when unresolved | Current `send_unscoped` / `send_scope` / configured default scope |
| Flood hash width | Incoming bot command's width | Zen's configured `path_hash_mode + 1` |
| Expected route reply | Authenticated `PATH` with embedded `RESPONSE` and matching tag | Same |
| Reply eligibility | Full target, sent tag, current owner/profile/time and exact captured scope | Full target, sent tag and active session; no equivalent reply-scope comparison in `pathResponse` |
| Timing | Existing hop/airtime policy clamped into 55-second work / 60-second total | Flood timeout from actual request airtime plus grace |
| Initial public acknowledgement | Yes, separate radio transmission | No corresponding public-channel ACK |

The payload fields are equivalent in the compared source. No random trace-ID /
RTC-tag mixup was found. The added public acknowledgement can affect airtime and
contention, but normal discovery response matching begins only after actual
request TX start, and the current budget leaves far more than the reported
one-second successful Zen discovery under ordinary settings.

## Finding 1: unknown requesters cannot use this authenticated discovery

Evidence:

- `examples/bme680_sensor/SensorScopeBinding.h:50-59` creates a normal encrypted
  request; there is no anonymous login or full sender key in its application data.
- `src/Mesh.cpp:492-509` serializes destination hash, source hash and ciphertext.
- `src/Mesh.cpp:151-160` asks `searchPeersByHash` for candidate shared secrets
  before decrypting any ordinary request.
- `examples/simple_repeater/MyMesh.cpp:623-637` searches the repeater ACL only.
  With no matching client, there is no decryption candidate and no request reply.
- `examples/simple_repeater/MyMesh.cpp:663-687` handles a known client's request,
  checks replay freshness and creates the PATH response for a flooded request.
- `examples/simple_repeater/MyMesh.cpp:240-260` permits guest base telemetry;
  this is not an administrator-only operation.
- `examples/simple_repeater/MyMesh.cpp:650-659` observes repeater neighbours but
  does not turn incoming sensor adverts into registered requester clients.

The normal first-contact mechanism is an encrypted `PAYLOAD_TYPE_ANON_REQ`
login. Its packet includes the full sender public key, allowing the recipient
to derive a shared secret before the requester is in its ACL. A valid configured
guest login can establish a transient client entry; it does not require admin
privileges. Bad/disabled guest credentials can be silently ignored, so an absent
login reply alone must not be labelled proof of permission denial.

Zen has a general login path (`BaseChatMesh::sendLogin`) and the user confirms
using it on 7188. Its trace path is `UITask::startRepeaterTrace` ->
`MyMesh::startRepeaterTrace` -> `RepeaterTraceSession::start` -> authenticated
telemetry flood. That sequence does not itself call login. Accordingly, simply
copying more of Zen's request builder will not fill the sensor's first-contact gap.

## Finding 2: scope is neither selected nor accepted identically

Outgoing policy differs:

- Sensor `SensorScopeBinding.h:64-75` captures context from the bot packet;
  `src/helpers/RegionScope.h` preserves a resolved request scope, keeps an
  unscoped flood unscoped, or uses the configured default for unresolved scope.
- Zen `MyMesh.cpp:746-770` on the compared branch uses its local flood-scope
  override/default and local path-hash-width preference.

The public `#hillvue` channel and a transport region are separate concepts.
Receiving the command on the same channel does not prove the subsequent sensor
and Zen discovery frames use the same transport key or scope mode.

The sensor then silently returns from `SensorBotService.cpp:660-662` when a
matching authenticated PATH response uses a different scope/mode. Zen's
`RepeaterTraceSession::pathResponse` has no equivalent scope comparison.

A concrete conditional divergence: a target directly hears a request whose
transport scope it does not resolve, processes it locally, and emits a reply
using its own default scope or no scope. The stock repeater's `onRecvPacket`
sets `recv_pkt_region` but still calls core Mesh processing; the region gate is
on forwarding. `sendFloodReply` selects a default when request scope is
unresolved. Sensor may decrypt and silently reject that response; Zen may accept
an equivalently authenticated response if it reaches the device.

For a correctly resolved matching scope, stock 1.18 normally mirrors it; this
is not proof of a defect on every scoped trace. Record actual outgoing and
return modes/keys before changing policy. Do not indiscriminately accept all
foreign-scope replies to conceal the discrepancy. Explicitly decide whether
trace discovery should retain strict captured-scope semantics or match Zen's
less restrictive response policy.

## Finding 3: RTC tags are correct in-process, not guaranteed across reboot

Both implementations use `getCurrentTimeUnique` for discovery. The sensor's
`uniqueTime` adapter calls the real RTC helper, so the old random-ID integration
mistake is not present here.

However, `src/MeshCore.h:119-124` only guarantees increasing values within the
current clock object's lifetime. `variants/xiao_nrf52/target.cpp:16-22` uses an
auto-discovered RTC with `VolatileRTCClock` fallback. Without a supported external
RTC or later clock synchronization, `ArduinoHelpers.h:11` starts that fallback
at 2024-05-15 on each reset. A registered sender that previously sent higher
timestamps can then be rejected by `timestamp > client->last_timestamp` on a
still-running repeater. Wrong absolute time alone is insufficient to prove a
replay failure: it must be at or below that same client's recorded timestamp.

Tests use a valid advancing synthetic RTC and do not cover this complete
production reboot/remote-retained-client scenario. Compare both actual sensor
time and transmitted tag to the remote per-client value. Do not solve replay
rejection with random tags or unconditional trust in a peer's reported clock.

## Finding 4: prior success tests assumed registration

`TargetedSensorNetwork::SetUp` at `test/test_sensor_network.cpp:571-573` explicitly
adds `sensor_mesh->self_id` as a guest on the remote before every success case.
It supplies the correct shared secret too. The mock target implements the
request/replay/PATH response subset, not the complete production repeater login
and telemetry handler.

`UnknownRemoteClientTimesOutWithoutAutomaticLogin` deliberately removes that
remote ACL entry, then expects discovery timeout and no trace. This confirms the
behaviour but does not satisfy a user expectation that a newly deployed sensor
can discover any known repeater. Earlier passing test totals therefore did not
validate first-contact operation. This is a gap in the implementation's intended
behaviour and its acceptance coverage.

Wrong-scope tests likewise deliberately assert sensor rejection, while normal
network successes use one shared scope throughout. They do not establish that
real 1.18 reply-scope fallback matches the sensor's assumption.

## Paths checked without a matching defect found

- Full destination keys are resolved and passed to encryption, not four hex
  characters treated as a complete key. Ambiguous prefixes are rejected.
- Base telemetry mask is `~1`, matching Zen's `~TELEM_PERM_BASE`.
- Reserved bytes and random nonce positions match; packet padding is handled by
  the shared core encryption/decryption routines.
- Discovery ownership is installed after queueing and before the asynchronous
  Dispatcher can start TX. Production callbacks forward TX start/completion/
  failure into the service. No missing binding was found.
- Transient target peers supplement the sensor's management ACL lookup and are
  used to decrypt returning PATH replies. An existing matching ACL peer can
  still dispatch to discovery without saving a management route.
- Both implementations construct the loop from the target's embedded outbound
  path, append the target hash, and reverse intermediate hops for the return leg.
- Late response absorption and retired target/tag ownership are deliberate and
  do not substitute an old cached route for a fresh discovery.
- Differences in discovery timeout formulas cannot explain a normal one-second
  reply never being accepted under the ordinary current sensor time windows.
- Core branch differences reviewed include PATH bounds checks and safer TRACE
  path validation; neither shows a normal valid 1.18 PATH reply being rejected
  solely because those safety checks exist.

## Read-only validation performed

Ran the existing suites without editing test source:

```sh
/home/matt/.platformio/penv/bin/pio test -e native_sensor \
  --filter test_sensor_target_trace --filter test_sensor_network
```

109/109 tests passed. This includes known-client success, unknown-client expected
failure, replay protection, full-key matching, wrong-scope rejection and real
Dispatcher/network timing. No new reproduction tests were implemented, and no
over-air evidence or target ACL contents were collected during this review.
Build products/logs may be refreshed by that test command; firmware source is
unchanged.

## Recommended repair and verification sequence

1. Confirm the comparison uses the same full target key, actual deployed build
   versions and PHY settings. Read the relevant target client ACL locally or
   capture target-side request handling. Never infer ACL state solely from the
   sensor knowing the target's name or from prior admin access on another device.
2. Capture one working Zen attempt and one failing sensor attempt. Record packet
   types in order (including any anonymous login before discovery), sender and
   target identities, actual TX times, scope mode/fingerprint, hash width,
   request timestamp and whether a PATH response was emitted. Do not log shared
   secrets or passwords.
3. Track the sensor receive checkpoints: matching source/destination, MAC/decrypt
   success, tag ownership, active phase/profile, scope acceptance, route decode
   and first probe admission. Existing timeout text does not distinguish these
   silent exits; add bounded RAM/debug evidence in a separately authorized fix.
4. Implement a bounded first-contact strategy for targets that do not know the
   sensor, using only the target's configured guest-access policy. Prefer an
   explicitly owned guest login followed by a fresh ordinary discovery request.
   Do not elevate to admin or silently guess passwords. Both stages must share
   the existing 60-second total and preserve cancellation and packet ownership.
5. Handle login as its own protocol: stock login replies begin with the target's
   timestamp and `RESP_SERVER_LOGIN_OK`, not an echo of the discovery request
   tag (`MyMesh.cpp:135-144`). Feeding that response through current tag matching
   would still fail. Correlate it using the correct selected identity/state and
   protocol semantics, then send a new, fresh-tag discovery. Avoid introducing
   stale login replies or new management-route side effects.
6. Resolve any measured scope difference deliberately, with tests against real
   stock request/reply selection. Keep public reply context independent if
   outbound discovery scope becomes a separate policy; preserve captured final
   report channel/scope semantics.
7. Add an explicit registered-client reboot/replay test and clock validity
   diagnostics before choosing a persistent monotonic-time strategy. Do not
   assume a successful login repairs a stale subsequent request timestamp.

Essential new tests for an eventual fix:

- Fresh sensor, empty remote ACL, allowed guest login, then authenticated PATH
  discovery and successful trace, with no test pre-registration.
- Guest access unavailable/restricted: bounded stage-specific outcome without
  claiming RF failure or explicit permission denial from silence alone.
- Remote restart loses transient guest state: next sensor operation re-establishes
  it within the same deadline and emits bounded traffic.
- Sensor reboot rolls back fallback RTC while the target retains higher replay
  state; detect/report this accurately and exercise the chosen clock policy.
- Valid reply on same scope, target default fallback scope, unscoped fallback,
  and deliberately foreign scope; compare sensor/Zen acceptance under the
  selected explicit policy.
- Real 1.18 login and request-handler semantics in the test target, plus duplicate
  hashes/full keys, late PATH/login replies, cancellation and expiry in each
  stage, failed sends and no automatic alteration of management routes.
- Preserve the 55-second work cutoff, 60-second total, acknowledgement text and
  at most the intended emitting submissions per stage.

Hardware acceptance must compare captured packets and confirm the original
7188 case succeeds from an initially unregistered sensor. The additional
successful Zen targets should be included without assuming how they learned
Zen's identity. No diagnosis of those targets is complete until their actual
packet exchange or client state is observed.

## Zen source references for reproducibility

The following refer to `zen/main@6ba09b28`, not paths checked out on sensor:

- `examples/companion_radio/zen-overlay/app/MyMesh.cpp:1375-1391`: RTC tag,
  shared-secret request construction and flood scope dispatch.
- Same file `:746-770`: active/default/unscoped scope and local hash width.
- Same file `:1412-1460`: start, transient peer matching and PATH dispatch.
- Same file `:1271-1283`: existing-contact PATH dispatch into trace matching.
- `examples/companion_radio/zen-overlay/app/zen/RepeaterTraceSession.h:150-185`:
  authenticated identity/tag route acceptance and TX-start ownership.
- `examples/companion_radio/zen-overlay/app/ui-new/UITask.cpp:1833-1846`:
  UI trace start has no login stage.
- `src/helpers/BaseChatMesh.cpp:603-633`: separate anonymous login mechanism.
