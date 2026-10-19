# Public-channel Commands

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

The node listens on MeshCore's built-in Public channel and `#hillvue` for
messages beginning with the exact command word `!hillvue`. Matching is
case-insensitive. Each response stays on the requesting channel, including
delayed message parts, trace results, fallback results and error notices.
Both channels share the same four-command-per-minute limit and active trace.

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
| `!hillvue trace` | Route round-trip time and each repeater's SNR |

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

`!hillvue trace` is standalone. The sensor sends one direct MeshCore trace out
through the repeaters in the received request path and back to itself, then
reports the elapsed milliseconds as a **round-trip** measurement and the receive
SNR at each repeater on the outward leg. For example:

```text
Trace: 1540 ms RTT
1 Ridge +3.5 dB
2 Valley -8.0 dB
```

MeshCore TRACE does not record timestamps at each repeater, so individual
per-node milliseconds cannot be reported from this single probe. The RTT excludes
the final repeater-to-companion link, but includes the sensor-to-nearest-repeater
link, radio airtime and forwarding delays. Time waiting in the sensor's transmit
queue is excluded. It is not a
one-way latency or a measurement to the companion. Only one trace runs at
a time, including bounded report delivery; paths over ten repeaters are rejected.
Before the first probe, a short airtime-aware settling window lets the command
flood finish. Its earliest start is twice the command airtime, clamped to
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
accumulated during the loop. The minimum is ten seconds for two repeaters,
13 seconds for three, and 30 seconds for ten. Slower radio settings can increase
that allowance; remote repeater congestion cannot be predicted exactly.

After the first timeout, the sensor makes one silent retry following a random
one-to-two-second pause. It passively watches additional copies of the exact
same command for up to three alternate flood paths. The original route is always
tested first. Retry candidates are ranked using recent exact-route outcomes,
directed-edge diversity per estimated airtime, freshness and last-hop SNR.
Last-hop SNR is not an end-to-end score. Four RAM-only history records expire
after fifteen minutes and are scoped to the channel and radio configuration;
temporary radio configuration changes and reversion invalidate old context too.
History only ranks routes observed for the current command: it never invents a
route or supplies an old, unobserved path. This sends no discovery packets and
does not treat repeater advert names as network topology.
An observed path is a routing hint, not proof that its links work in reverse.

Each attempt has fresh identifiers and retains its own path, hash width and
transmission-start time. Only a final result or failure is posted to the
requesting channel. A first result received before retry transmission cancels
the queued retry. During retry it is retained in RAM as fallback; a successful
retry takes precedence. Fallback names and SNRs always describe the first route,
even if the retry used another route.

When both probes expire without a result, a three-second final grace period
allows either result to arrive. Valid late results can also replace an unsent
failure report. Once failure transmission starts, the outcome is committed and
late results are ignored, preventing contradictory reports. Failure-report
queueing is bounded to ten seconds, including the normal random send delay.
The node reports `Trace: no result after retry`, not that no route exists.
Radio-busy, transmit-failure and unsent-retry conditions have separate messages.
Local recovery never increases the two-probe budget.

Completed results are retained while their reports are prepared and transmitted.
Both report parts, including the node-name prefix and cleaned repeater labels,
are frozen together. Temporary packet allocation or confirmed queue rejection
does not discard a result. Each part has a ten-second admission allowance, and
the overall success-report allowance is at least thirty seconds, extended for
slow radio airtime. The second part remains eligible only three seconds after
the first finishes transmitting. An active transmission is left to MeshCore's
existing completion/watchdog handling, never freed by the application.

An uncertain report transmission is not repeated: it may already have reached
other users. RadioLib start errors are conservatively uncertain because its
Boolean result cannot prove that no frame was emitted. A report problem never
turns a successful trace into a public trace-failure message. Local transmission
completion is not confirmation that the companion received the channel reply.

Local diagnostics retain outward and return SNRs, the final received-link SNR,
and the furthest matching trace progress overheard for each attempt. Partial
progress never completes a trace, extends its timeout or identifies a failed
repeater. These snapshots are available to tests/debugger inspection only;
the public RTT/SNR format is unchanged and no extra diagnostic traffic is sent.

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
