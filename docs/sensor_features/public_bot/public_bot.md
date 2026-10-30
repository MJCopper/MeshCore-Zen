# Public-channel Commands

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

The node listens on MeshCore's built-in Public channel and `#hillvue` for
messages beginning with the exact command word `!hillvue`. Matching is
case-insensitive. Each response stays on the requesting channel, including
delayed message parts, trace results, fallback results and error notices.
Both channels share the same four-command-per-minute limit and active trace.

Replies retain the request's transport scope through delayed and multipart
messages, trace results, fallback results and error notices. A recognised scope
is mirrored; an explicitly unscoped request receives an unscoped reply. If a
transport scope cannot be resolved, the sensor uses its configured default
scope, or replies unscoped if no usable default exists. The selection is captured
when the command is accepted, so later traffic or default-scope changes do not
redirect a pending reply. A busy notice follows the new request's scope while
the running trace keeps its original scope.


Hashtag channels are not private: anyone who knows the name can derive the key.

## Commands

| Message | Response |
| ------- | -------- |
| `!hillvue` | All measurements |
| `!hillvue all` | All measurements |
| `!hillvue t` | Temperature |
| `!hillvue h` | Humidity |
| `!hillvue p` | Pressure |
| `!hillvue a` | Air quality |
| `!hillvue v` | XIAO battery voltage |
| `!hillvue ping` | `Pong` reachability reply |
| `!hillvue path` | Repeater path taken by this request |
| `!hillvue trace` | Incoming command route RTT and average bidirectional link SNR |
| `!hillvue trace 4df5` | Discover and trace to the known repeater with public-key prefix `4df5` |
| `!hillvue trace status` | Current discovery/probe/report phase, attempt and receive/queue time remaining |
| `!hillvue trace cancel` | Stop the active trace and cancel its queued work |

An option is selected by its first letter, so forms such as `temp`,
`Temperature` and `T` all select temperature. Likewise, `v`, `volt` and
`voltage` select voltage. Multiple space-separated options
can be combined:

```text
!hillvue t air
!hillvue Temperature Humidity
!hillvue p h a
!hillvue voltage t
```

`!hillvue ping` is standalone and cannot be combined with measurements. It
uses the same traffic limits as other commands and does not require sensor data.
`Pong` confirms that the node received the command, but does not measure mesh
round-trip time in milliseconds.

`!hillvue path` is also standalone. It reports the repeaters traversed by the
request on its way to the sensor, using names from verified repeater adverts
where available. Unknown or ambiguous names are shown as path hashes. This is
the first received route, not necessarily the best route or the return path.
The sensor retains up to 48 repeater names in its internal flash filesystem;
unchanged adverts do not cause writes. The reply appears on the requesting channel,
not as a private message to the requester.
Long paths use at most two messages, labelled `1/2` and `2/2`, with a three-second
gap after the first transmission completes. Replies split between repeaters,
preserving their order and using the same short names as trace replies. Paths
that exceed both messages end with `...truncated`. Short paths remain one message;
the shared four-requests-per-minute limit is unchanged.

Path and trace display names omit the whole words `repeater` and `rptr`, ignoring
case, before applying the existing 12-byte UTF-8-safe name limit. For example,
`Hillvue Repeater` becomes `Hillvue`, and `Tamworth rptr` becomes `Tamworth`.
Whitespace is tidied; stored advert names are unchanged. If removal leaves an
empty name, the repeater's path hash is displayed instead.

`!hillvue trace` without a destination sends one direct MeshCore trace out
through the repeaters in the received request path and back to itself, then
reports the elapsed milliseconds as a **round-trip** measurement and the average
of both directional SNR readings for each link. Nodes are listed once, starting
with the nearest repeater to the sensor and proceeding towards the requester.
Each node's displayed SNR describes its link to the preceding node (the sensor
for the first entry). For example:

```text
Trace: 1540 ms RTT (avg SNR)
1 Ridge -4.0 dB
2 Valley +1.0 dB
```

For the first entry, sensor-to-repeater -6 dB and repeater-to-sensor -2 dB
produce -4.0 dB. This is the arithmetic mean of the two dB readings from the
selected trace attempt, not an average over time. Values are averaged before
rounding to one decimal place. Retry and late fallback reports use their own
route's samples; the first link includes the sensor's final received SNR.
An average can hide a weaker direction, so it is not a weakest-link score.
The final repeater-to-requester link is outside the trace measurement.

To choose a destination instead of using the command's incoming route, send
`!hillvue trace 4df5`. The four hexadecimal characters select the first two bytes
of a full public key in the sensor's verified repeater-advert cache (up to 48
named repeaters, including persisted adverts). Uppercase hexadecimal is accepted.
The sensor must already know the full identity; four characters alone cannot
identify an unknown node. An absent prefix reports `Trace: unknown repeater
4df5`; multiple cached matches report `Trace: ambiguous repeater 4df5`. Neither
case starts discovery. Malformed prefixes or combined trace/measurement commands
are ignored. Both trace forms are standalone commands.

For a selected destination, the sensor first tries standard authenticated path
discovery. If the repeater does not yet recognise the sensor, the default
`public` first-contact policy makes an anonymous login with the empty public
guest credential, allowing two retries (three transmissions total). A valid
authenticated login-return path leads straight to TRACE. A successful login
without a usable path may use the one remaining ordinary discovery request. No administrator
password is needed or guessed. A target with protected guest access may ignore
that login; lack of a reply does not establish that the repeater is unreachable.
Private/local administration can select `get trace.login` or
`set trace.login public|off`. A successful login may create a transient client
entry on the target. An unexpected administrator grant is not used by this
workflow; tracing falls back instead.

If discovery cannot obtain a path, the stationary sensor can use a reversed
route from that repeater's previously verified flooded adverts. It retains up
to two candidate routes for each of 48 identities in a separate persisted cache.
Known-age hints expire after 24 hours. Restored hints without reliable calendar
age are probationary candidates, never freshly discovered routes. Every attempt
sends a new round-trip TRACE: old RTT/SNR/results are never returned as current.
If one route fails, a distinct alternate may use the one remaining probe.
Advert fallback can succeed with no login and no target ACL entry. On first
installation, the route cache is empty even if old stored names are present:
allow the sensor to receive genuine flooded adverts before relying on fallback.
After collection and a deferred save, those hints can survive reboot.

The selected repeater remains the final outward entry, including a direct
neighbour. Final output and average-SNR rules match bare trace. Ten repeaters
includes the target; one/two-byte hashes are supported. Unsupported paths are
never probed. An advert authenticates the originating identity/data, not its
mutable flood path or reverse reachability, so a candidate can still fail if
links are asymmetric or topology changes. Ambiguous short-hash labels remain
ambiguous.

Discovery/login reply waits use the companion flood estimate: 500 ms plus
16 times request airtime, with an explicit one-second sensor margin. Their
receive clocks start at successful local TX completion. Queueing and own TX
airtime do not consume the receive allowance. Control admission reserves enough
remaining time for a probe and terminal reporting. There is no fixed six/eight
second reply wait or fixed 40-second control cutoff. Status includes login
attempt N/3 and whether it is queued, transmitting or awaiting a reply.

The target must recognise the **sensor's identity** to answer authenticated
discovery; a phone's or Zen's earlier login does not register the sensor.
Authenticated discovery replies are matched by full identity, MAC and fresh
request tag even if a target's return scope differs; the difference is recorded.
Outgoing control uses the configured default transport scope (otherwise the
resolved command scope or unscoped), and the local configured discovery hash
width. Public replies retain the original command's captured context. Cached
advert hints must match the selected control scope and radio configuration.

Durable request timestamps prevent ordinary reboot/clock rollback from reusing
already issued trace login/discovery timestamps. Storage/clock failure disables
further authenticated requests and still permits advert fallback. Failures such
as `Trace: guest login not acknowledged`, `Trace: request clock unavailable`,
radio busy, transmit failed or discovery timed out describe the known stage;
they do not prove RF unreachability or permission denial from silence alone.

Use `!hillvue trace cancel` to stop waiting without rebooting. Cancellation acts
on the sensor's shared trace from either watched channel. It removes queued
discovery, probe and report packets and prevents further retries/parts. A packet
already on air cannot be recalled and remains owned by Dispatcher; its late
callbacks/results cannot restart the cancelled operation. The cancel reply uses
the operator's requesting channel and scope. The normal four-command-per-minute
limit still applies to starts and status requests. Cancelling an active trace
is allowed even when those slots are exhausted, with one acknowledgement for
the cancelled operation; idle cancel requests use the normal limit.

Every accepted trace first schedules one message with no application jitter. For
bare `!hillvue trace`, Y is the sender label parsed from the channel message:

```text
BME680 Sensor: Trace requested to Alice, please wait up to X seconds.
```

That label is copied from the message's `Name: !hillvue trace` prefix, with
whitespace normalized and UTF-8 kept intact. It is display text supplied in the
group message, not a cryptographically verified companion identity; the trace
route still comes from the packet path. If no usable label is present, the
acknowledgement says `requester`.

For `!hillvue trace 7188`, if the known repeater is named `Hilltop Repeater`,
the acknowledgement is instead:

```text
BME680 Sensor: Trace requested to Hilltop Repeater, please wait up to X seconds.
```

The four-character prefix must resolve to exactly one known full public key.
The full key is used for authenticated discovery; the name is only a display
label. The acknowledgement preserves the full stored display name, including
words such as `Repeater`, independently of compact hop labels. Whitespace is
normalized and UTF-8 is kept intact. A known identity without a usable name uses
`7188` as its label. An unknown or ambiguous identity retains the existing error
and starts neither acknowledgement nor discovery.

Destination/requester label, sensor prefix and reply context are frozen when the
request is accepted, including during allocation/queue retries. A later rename
or cache eviction cannot change that pending request. The acknowledgement stays
within one 160-byte public message; if necessary, only its label is shortened at
a UTF-8 boundary. Final result formatting is unchanged.

The acknowledgement's X is the rounded-up calculated maximum for that request,
starting at acceptance. It includes acknowledgement, discovery/login attempts,
settling, TRACE probes, queueing and terminal reporting. Sixty seconds is not a
minimum or expected wait. The calculated maximum may exceed 60 seconds when
justified by radio airtime and route size, with an absolute **120-second safety
ceiling**. A successful trace or definite failure finishes early.

For bare trace, the same budget calculator uses the incoming route and its
permitted probe/retry branch; discovery and login allowances are omitted. Since
a future passive alternate is not known at acceptance, X reserves both probes at
the incoming route's size. At retry time, the sensor uses a longer observed
alternate only when its complete airtime window fits the frozen deadline. If
not, it retries the original route when that full window fits; if neither fits,
it omits the retry and reports `Trace: retry not sent`. X is never extended and
a receive window is never clipped to fit an alternate. For a destination trace,
known routes size the budget from compatible candidates. Without a route, the sensor reserves one
conservative supported ten-repeater probe; a second is conditional on available
time after discovery. An airtime-derived terminal
reporting reserve replaces the old fixed five seconds. Optional attempts are
omitted when a full exchange cannot fit, rather than shortening a started reply
window. High contention or a larger discovered route can still prevent success.

Acknowledgement/control/probes share at most eight seconds of cumulative local
queue wait. Reporting has two cumulative queue seconds of its own, excluding
scheduled send delays and the inter-part gap. The acknowledgement has at most
one possible emission. It does not consume another command-rate slot. If it
cannot be sent, later tracing can proceed only within the shared remaining
queue/work allowance; exhaustion does not create a fresh lease.

The overall deadline is fixed when the request is accepted. Replies, retries,
status requests and newly observed routes cannot extend it. `trace status`
includes remaining overall seconds and identifies acknowledgement when that is
the current stage. `trace cancel` removes an unsent acknowledgement as well as
other queued trace work. On the first running service poll at the overall
deadline, the sensor stops further trace work and releases the trace slot. A
packet already on air remains under radio ownership until completion/recovery.
Channel access and duty limits still apply: neither immediate scheduling nor the
advertised maximum guarantees over-air receipt. A blocked application loop also
cannot perform cleanup until it runs again.

At most two ordinary discovery requests, three public guest login attempts, and two TRACE
probes may emit. The two-probe limit is shared by authenticated and advert
routes. Acknowledgement and up to two terminal report parts give at most ten
application-originated emitting packets per targeted trace. Discovery/login
use durable forward-only request timestamps; random TRACE identifiers remain
separate. A short settling window uses the selected loop's estimated airtime.
Acknowledgement, discovery/login, settling and queue wait are excluded from RTT.

Transient peer contexts exist only in RAM. Discovery responses do not replace
saved management routes or trigger reciprocal PATH traffic. Late and ownerless
RESPONSE/PATH replies are ignored after the transaction closes. Normal management
requests and non-response PATH handling retain their existing owners. Radio
configuration changes cancel queued discovery/probes before further sends.
Remaining result parts are stopped on a radio profile change, without issuing a
conflicting failure after a result that may already have been transmitted.

MeshCore TRACE does not record timestamps at each repeater, so individual
per-node milliseconds cannot be reported from this single probe. The RTT excludes
the final repeater-to-companion link, but includes the sensor-to-nearest-repeater
link, radio airtime and forwarding delays. Time waiting in the sensor's transmit
queue is excluded. It is not a
one-way latency or a measurement to the companion. Only one trace runs at
a time, including bounded report delivery; paths over ten repeaters are rejected.
After acknowledgement, a short airtime-aware settling window before the first
probe lets the command flood finish. For bare trace, its earliest start is twice the command airtime, clamped to
300-2000 milliseconds, plus 0-250 milliseconds of jitter. Matching duplicates
extend a quiet interval of 150-750 milliseconds, but cannot extend settling past
2.5 seconds. Unrelated messages do not extend it. Settling is excluded from RTT.

Temporary allocation and queue rejections use bounded backoff and jitter within
the shared queue/work allowance. A rejected radio start may use the remaining
probe attempt; at most two potentially emitting probes are allowed. Local
failures do not count as evidence against a route. After actual TRACE TX start,
the timeout uses the companion direct estimate plus a one-second sensor margin:

```text
1500 + (6 × maximum packet airtime + 250) × (2 × repeater count)
```

All values are milliseconds. Maximum packet airtime includes the SNR bytes
accumulated during the loop, configured hash width and actual PHY settings.
There is no separate ten-second minimum for a two-repeater route. A new probe
is admitted only if its full calculated window fits the remaining work time;
its receive timer is not shortened at TX start. Remote repeater congestion
cannot be predicted exactly.

After the first trace timeout, if time remains, the sensor makes one silent retry following a
random one-to-two-second pause. Bare trace passively watches additional copies
of the exact same command for up to three alternate flood paths. Destination
trace instead collects additional authenticated PATH responses for the current
target and transmitted discovery tags. The original received/discovered route
is always tested first. Retry candidates are ranked using recent exact-route outcomes,
directed-edge diversity per estimated airtime, freshness and last-hop SNR.
Last-hop SNR is not an end-to-end score. Four RAM-only history records expire
after fifteen minutes and are scoped to the full transport key, channel and radio
configuration. Destination trace also requires the full target public key;
bare and destination trace histories remain separate. Temporary radio changes
and reversion invalidate old context too. Bare traces with unresolved transport
scopes do not reuse history between commands. Destination traces use their
selected discovery scope. Alternate
copies must match the original command ciphertext and scope. For unresolved
scopes, matching also requires the same transport route type and both transport
codes; these short codes remain potentially ambiguous and are only conservative
hints within that command. A transport-code match to multiple configured keys is
treated as unresolved. Region names and IDs are not route-history identities.
History only ranks routes observed for the current command: it never invents a
route or supplies an old, unobserved path. Bare trace sends no discovery packets.
Neither mode treats repeater advert names as network topology.
An observed path is a routing hint, not proof that its links work in reverse.

Each attempt has fresh identifiers and retains its own path, hash width and
transmission-start time. After acknowledgement, one final result or failure is
posted to the requesting channel. A first result received before retry transmission cancels
the queued retry. During retry it is retained in RAM as fallback; a successful
retry takes precedence. Fallback names and SNRs always describe the first route,
even if the retry used another route.

When both probes expire without a result, a three-second final grace period
allows either result to arrive, within the calculated work cutoff. Valid late
results before that cutoff can also replace an unsent failure report. Once failure transmission starts, the outcome is committed and
late results are ignored, preventing contradictory reports. Failure-report
queueing shares the separate two-second report queue allowance, excluding
scheduled send delay, and always ends by the original advertised deadline.
The node reports `Trace: no result after retry` or the applicable timeout stage;
it does not claim that no route exists.
Radio-busy, transmit-failure and unsent-retry conditions have separate messages.
Local recovery never increases the two-probe budget.

Completed results are retained while their reports are prepared and transmitted.
Both report parts, including the node-name prefix and cleaned repeater labels,
are frozen together. Temporary packet allocation or confirmed queue rejection
does not discard a result. Each part has a ten-second admission allowance, and
the base success-report allowance is thirty seconds or longer for slow radio
airtime, always shortened by the original advertised transaction deadline.
Remaining multipart delivery is best effort within that deadline. The second part remains eligible only three seconds after
the first finishes transmitting. An active transmission is left to MeshCore's
existing completion/watchdog handling, never freed by the application.

An uncertain report transmission is not repeated: it may already have reached
other users. RadioLib start errors are conservatively uncertain because its
Boolean result cannot prove that no frame was emitted. A report problem never
turns a successful trace into a public trace-failure message. Local transmission
completion is not confirmation that the companion received the channel reply.
If the owner completion notification is lost, a report observer watchdog releases
the trace slot after estimated TX airtime × 1.5 plus one second. It runs after
Dispatcher has had its completion/watchdog poll and never frees an in-flight
packet. Such delivery is classified as uncertain and is not repeated.

Local diagnostics retain outward and return SNRs, the final received-link SNR,
and the furthest matching trace progress overheard for each attempt. Partial
progress never completes a trace, extends its timeout or identifies a failed
repeater. These snapshots are available to tests/debugger inspection only;
the public report averages paired directional readings, and no extra diagnostic
traffic is sent.

The original command counts once toward the unchanged
four-command-per-minute limit, although a retry sends a second trace packet.
Requests received while a trace is running do not use a rate-limit slot; the
node sends at most one `Trace: already running` notice for that trace.
Incoming 3-byte path hashes cannot be mirrored by MeshCore TRACE and are
reported as unavailable.

After a trace result arrives, its channel reply is scheduled with a
one-second minimum plus the usual random 0.5–2-second transmit delay. This
spacing does not guarantee a collision-free transmission. The RTT is measured
when the trace returns; the reply delay is not included. Repeater labels retain
their existing 12-byte UTF-8-safe limit. With the default `BME680 Sensor` name,
all ten repeater lines fit in at most two replies. A longer sensor name may
leave too little room; the second reply then ends with `...truncated`.

Each selected value appears on its own line in a channel reply;
`!hillvue` and `!hillvue all` include voltage. A voltage-only request works
even when the BME680 is not ready. Before the first voltage sample, or for
requested BME680 readings that are not ready, the reply says
`Sensor data is not ready`.
During air-quality calibration it reports, for example:

```text
Air Quality: Warming Up (4/10)
```

A reply that exceeds one message is divided at a line boundary where possible.
Both parts carry the sensor's name and are labelled so they can be read in order:

```text
BME680 Sensor: 1/2
Temperature: 22.4 C
Humidity: 48.2%

BME680 Sensor: 2/2
Pressure: 1013.2 hPa
Air Quality: 56/500 (Moderate)
```

Only two messages are sent for one command. If the reply still does not fit,
the second ends with `...truncated`; no further parts are sent.

## Mesh traffic controls

- Commands use the latest cached reading and never trigger an extra sample.
- The node accepts at most four commands in any rolling 60-second period,
  shared across all users and commands. This limit is unchanged when a response
  needs two messages.
- Responses that do not fit one message are split into at most two, marked
  `1/2` and `2/2`. The second transmission is scheduled at least three seconds
  after the first one's completed transmission. A busy radio may lengthen the
  actual gap. Each message is a separate flood, so four accepted commands can
  produce up to eight reply messages.
- Hashes for the eight most recent accepted commands are retained in RAM to
  suppress duplicate packet delivery.
- The bot does not publish on a schedule and does not respond to its own output.
- Each response part is sent once as a normal MeshCore flood; the node does not
  repeat other mesh traffic.

Unknown command options are ignored without a response.

### Private trace diagnostics

After a failed targeted trace, private/local CLI commands expose the last
transaction without transmitting packets or writing storage:

- `get trace.diag`: candidate PATH/RESPONSE frames addressed to the sensor with
  the target's short hash (`rx`), authenticated response-handler calls
  (`decrypted`), accepted login replies (`login_reply`), recognizable login
  replies ignored by the state machine, role and route/tag/scope counters.
- `get trace.control 0`, `get trace.control 1`, `get trace.control 2`: initial
  discovery, first guest login and final discovery. `get trace.control 3` and
  `get trace.control 4` show the two guest-login retries. Each exposes timestamps, request tag and
  transmission flags. Millisecond timestamps are local uptime. `sent=1` means
  radio TX started; completion without failure is local completion, not an
  acknowledgement from the repeater.
- `get trace.timing`: calculated maximum/report/probe reserves, reply windows,
  queue use, cap status and whether a login-return path supplied the route.
- `get trace.airtime`: request/probe/report airtimes and remaining overall time.
- `get trace.radio`: the captured configured PHY description and temporary-mode
  flag; airtime estimates always come from the active radio.
- `get trace.peer`: full target public key, outgoing transport scope key and
  hash width. An all-zero scope denotes an unscoped flood.

Short-hash candidate matches do not authenticate the remote node. The decrypted
counter counts handler calls, not necessarily unique packets. Accepted login
replies include unexpected-role replies; consult `role` and the final failure.
A new trace clears these counters. These commands contain no shared secrets,
passwords or private keys. They are not public bot commands.

Guest login stops immediately after an accepted reply. An unanswered login or
failed transmission may retry, using a fresh durable timestamp each time.
Retries are bounded by the frozen calculated work/reporting deadlines, so less than three attempts may fit after long queue delays. Queue
expiry, invalid/unexpected granted role, cancellation and clock failure do not
start another login. Local allocation/queue admission retries do not count as
additional over-air transmissions.

Successful TRACE routes enter volatile route history for 15 minutes. They do
not automatically become persistent advert-route entries. An existing matching
advert hint can be marked validated in RAM; that outcome alone does not schedule
an immediate cache write. Advert-originated paths are persisted through the
normal deferred advert-cache save mechanism.

The generated [companion timing table](../../upgrades/validation/sensor-trace-companion-timing-table.md)
shows worst-case request allowances, not normal completion times. For unscoped
Australia Narrow SF7/BW62.5/CR7 with a known two-repeater one-byte route, the
login reply window is 6.62 seconds, one TRACE window is 6.724 seconds, and the
maximum including the permitted retry branches and reporting is 81 seconds.
An unknown route advertises 109 seconds (117 with two-byte hashes), reserving
one conservative ten-repeater probe. A healthy request completes without waiting
through those unused attempts or maximum-route allowances.

### Optional physical flood copies

The sensor can enable `multi.flood` through the private/local administration CLI.
Mode 1 adds one best-effort physical copy to bot channel text, including trace
acknowledgements and reports. Mode 2 also covers locally originated discovery
and guest-login floods. Direct TRACE probes are excluded. Logical retry counts,
report ownership and frozen deadlines remain unchanged. See
[configuration and behaviour](../multi_flood.md).
