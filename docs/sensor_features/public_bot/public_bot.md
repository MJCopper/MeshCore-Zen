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

For a selected destination, the sensor sends a standard authenticated, flooded
base-telemetry request and uses the returned PATH to build a loop through the
selected repeater and back to the sensor. The selected repeater is included as
the final outward entry, even for a direct neighbour. The output format and
average-SNR rules are the same as bare trace. The ten-repeater limit includes
the target; one- and two-byte path hashes are supported. Unsupported request
hash widths are rejected before discovery; unsupported returned paths are not
probed. Settings are never silently changed.

The target must recognise and permit the **sensor's identity**, not merely the
phone or node that sent the channel command. Being present in the advert cache
does not establish remote permissions. The bot does not log in, add ACL entries
or change permissions automatically. `Trace: discovery timed out` can mean
missing permissions, a replay-clock mismatch, congestion or RF loss; it does not
prove the repeater is unreachable. Local failures instead report discovery radio
busy or discovery transmit failed.

A busy reply after thirty seconds can be normal: discovery has not necessarily
finished its first attempt. At the shipped SF8/BW62.5/CR5 settings with a
32-symbol preamble, the current conservative maximum-packet allowance is about
187 seconds per discovery attempt. Two unanswered attempts can therefore take
about six minutes fourteen seconds, plus queue and report time. Saved radio
settings can change this estimate. `!hillvue trace status` shows the current
attempt and its remaining time; a busy discovery reply includes that information
too. These countdowns are captured when the command is processed, before reply
queueing. They do not promise successful delivery.

Use `!hillvue trace cancel` to stop waiting without rebooting. Cancellation acts
on the sensor's shared trace from either watched channel. It removes queued
discovery, probe and report packets and prevents further retries/parts. A packet
already on air cannot be recalled and remains owned by Dispatcher; its late
callbacks/results cannot restart the cancelled operation. The cancel reply uses
the operator's requesting channel and scope. The normal four-command-per-minute
limit still applies to starts and status requests. Cancelling an active trace
is allowed even when those slots are exhausted, with one acknowledgement for
the cancelled operation; idle cancel requests use the normal limit.

Every accepted bare or destination trace first schedules one message with no
application jitter:

```text
BME680 Sensor: Trace requested, please wait up to 60 seconds.
```

For `!hillvue trace 7188`, if the known repeater is named `Hilltop Repeater`,
the acknowledgement is instead:

```text
BME680 Sensor: Trace requested to Hilltop Repeater, please wait up to 60 seconds.
```

The four-character prefix must resolve to exactly one known full public key.
The full key is used for authenticated discovery; the name is only a display
label. The acknowledgement preserves the full stored display name, including
words such as `Repeater`, independently of compact hop labels. Whitespace is
normalized and UTF-8 is kept intact. A known identity without a usable name uses
`7188` as its label. An unknown or ambiguous identity retains the existing error
and starts neither acknowledgement nor discovery.

Destination, sensor prefix and reply context are frozen when the request is
accepted, including during allocation/queue retries. A later rename or cache
eviction cannot change that pending request. The acknowledgement stays within
one 160-byte public message; if necessary, only the destination label is shortened
at a UTF-8 boundary. Bare trace acknowledgement and final result formatting are
unchanged.

The entire operation has a fixed **60-second maximum**, starting when the command
is accepted. It includes acknowledgement, discovery, settling, probes, retries
and final reporting. A successful trace finishes early. The final five seconds
are reserved for reporting: discovery/probing stops at 55 seconds, and unfinished
work becomes `Trace: discovery timed out` or `Trace: no result before timeout`.
At 60 seconds, remaining queued work is cancelled and the trace slot is released.

Discovery and probe receive windows share the remaining work budget. At actual
TX start, the first discovery can use at most one quarter of the remaining work
time, the second one third; the first probe can use at most half, and its retry
the remaining work time. Each window is also limited by the ordinary airtime/hop
policy below. Queue waits, acknowledgement and retries consume the same fixed
budget. A retry is skipped if its estimated transmission cannot fit. These
limits apply to both bare and destination traces, all supported route lengths,
and all radio profiles. Slow/congested links can time out despite being reachable.

The acknowledgement has its own bounded stage. If it cannot be sent, tracing
still proceeds after that stage expires. Discovery and probe queue clocks start
after the acknowledgement stage. At most one acknowledgement can emit; it does
not use another command-rate slot. Invalid requests, duplicates, busy requests,
status and cancellation do not create this message.

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

At most two discovery submissions precede the existing maximum of two trace
submissions. Discovery tags use the unique RTC request timestamp; random trace
identifiers are separate. Discovery queue admission is bounded to ten seconds
per attempt with the existing local backoff. A transmitted discovery's timeout
uses the ten-repeater airtime allowance described below, clamped to its share
of the remaining work time starting at actual TX start. Discovery uses the captured reply scope and hash
width; it does not silently retry in another scope. A short settling window
uses the discovered loop's estimated maximum packet airtime. Discovery, settling
and queue wait are all excluded from reported RTT.

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

Temporary allocation and queue rejections are retried with 250-millisecond,
500-millisecond, then one-second backoff and bounded jitter. Admission is limited
to ten seconds per attempt, excluding the initial settling window. A rejected
radio start may use the remaining probe attempt, but there are never more than
two potentially emitting probe submissions. Local failures do not count as
evidence against a route. Once transmission begins, the
timeout is the larger of the existing hop allowance and an airtime estimate,
rounded up to whole seconds:

```text
Hop allowance:     5000 + 2500 × repeater count
Airtime allowance: 500 + (6 × maximum packet airtime + 250) × (2 × repeater count)
```

All values are milliseconds. Maximum packet airtime includes the SNR bytes
accumulated during the loop. The base minimum is ten seconds for two repeaters,
13 seconds for three, and 30 seconds for ten. Slower radio settings can increase
that base allowance; the actual wait is always clamped to the remaining shared
work time. Remote repeater congestion cannot be predicted exactly.

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
allows either result to arrive, within the 55-second work cutoff. Valid late
results before that cutoff can also replace an unsent failure report. Once failure transmission starts, the outcome is committed and
late results are ignored, preventing contradictory reports. Failure-report
queueing is bounded to ten seconds, including the normal random send delay,
and always ends by the original 60-second deadline.
The node reports `Trace: no result after retry`, not that no route exists.
Radio-busy, transmit-failure and unsent-retry conditions have separate messages.
Local recovery never increases the two-probe budget.

Completed results are retained while their reports are prepared and transmitted.
Both report parts, including the node-name prefix and cleaned repeater labels,
are frozen together. Temporary packet allocation or confirmed queue rejection
does not discard a result. Each part has a ten-second admission allowance, and
the base success-report allowance is thirty seconds or longer for slow radio
airtime, always shortened by the original 60-second transaction deadline.
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
