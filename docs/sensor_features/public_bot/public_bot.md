# Public-channel Commands

[Back to README](../../../README.md)

The node listens on MeshCore's built-in Public channel for messages beginning
with the exact command word `!hillvue`. Matching is case-insensitive.

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
unchanged adverts do not cause writes. The reply appears on the Public channel,
not as a private message to the requester.

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
a time; paths over ten repeaters are rejected. A probe that cannot begin
transmitting within ten seconds is cancelled. Once transmission begins, the
timeout is 20 seconds for up to four repeaters and 30 seconds for five to ten
repeaters. A timed-out
trace is not retried automatically; send a new `!hillvue trace` command to
try again. It counts toward the unchanged four-command-per-minute limit.
Requests received while a trace is running do not use a rate-limit slot; the
node sends at most one `Trace: already running` notice for that trace.
Incoming 3-byte path hashes cannot be mirrored by MeshCore TRACE and are
reported as unavailable.

After a trace result arrives, its Public-channel reply is scheduled with a
two-second minimum plus the usual random 0.5–2-second transmit delay. This
spacing does not guarantee a collision-free transmission. The RTT is measured
when the trace returns; the reply delay is not included. Repeater labels retain
their existing 12-byte UTF-8-safe limit. With the default `BME680 Sensor` name,
all ten repeater lines fit in at most two replies. A longer sensor name may
leave too little room; the second reply then ends with `...truncated`.

Each selected value appears on its own line in a Public-channel reply;
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
