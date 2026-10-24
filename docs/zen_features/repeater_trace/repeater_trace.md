# Repeater Trace

[Back to README](../../../README.md)

Open a repeater's node menu and select **Trace**. It discovers a fresh route,
then sends one full loop trace to the selected repeater and back. The repeater
needs normal MeshCore support; it does not need the Hillvue Bot installed.

The progress screen shows discovery/trace phases and attempt numbers. Use
Up/Down to scroll the result. Back closes the view and cancels queued work and
future retries. Leaving Nodes or entering Child Mode also cancels an operation.
Already-transmitted packets cannot be recalled; late results cannot reopen a
cancelled view. A completed result stays available in that view until dismissed
or replaced. Hold Enter on the trace screen to open its action menu and choose
**Re-Trace** for a fresh discovery and trace to the same repeater. This also
works during an active trace: queued work and retries are cancelled before the
new operation starts. Back from the action menu returns to the current trace.

A result includes software-measured total loop RTT and each link's SNR in both
directions, for example:

```text
You -> Ridge -> Valley -> Target -> Valley -> Ridge -> You
RTT: 1540 ms

Link                 Out       Back
You <> Ridge         +5.25     +0.00 dB
Ridge <> Valley      -2.00     +4.50 dB
Valley <> Target     +7.00     -3.25 dB
```

SNR is the receiving end's measurement for that directed link. Zero and negative
values are valid. Return values include the final reception at your device.
RTT starts at the actual local radio-start callback and includes loop airtime,
forwarding and software receive processing; it excludes route discovery,
settling and local TX queue waiting. TRACE supplies no per-hop timestamps, so
individual hop latency is not shown.

## Discovery and retries

Trace uses MeshCore's standard targeted flooded base-telemetry path-discovery
request. It does not reset or replace your saved contact route. An unsaved
repeater uses a temporary RAM peer context and is not added to contacts.
The repeater still has to recognise/permit this client's request: local presence
in a discovery list does not grant remote permissions. Missing responses may
reflect permissions, congestion or RF conditions; they do not prove no route
exists. Trace does not silently log in or change remote permissions.

The operation permits at most **two discovery submissions and two loop-trace
submissions**. Forwarded packets and replies add network airtime beyond these
originating requests. No channel result report or automatic repeating trace is
sent. Background automatic signal discovery is suppressed during this local
operation. Starting Trace while a node-discovery scan or another local remote
operation is active is refused as busy.

Discovery matches the target full key and each attempt's monotonic RTC tag,
using the same request clock as normal MeshCore telemetry. It retries once on
timeout. Loop trace retries once after a random one-to-two-second pause.
Allocation failures use 250/500/1000 ms backoff with bounded jitter and a
10-second admission window; they cannot create an unlimited stream of probes.
Queue counts displayed in progress are submissions, not confirmations of RF
emission or remote reception.

The loop timeout and settling policy are ported from the sensor branch's
working `!hillvue trace` engine. Timeouts scale with hop count and maximum
accumulated-packet airtime, rather than one fixed delay for all radio presets.
A three-second final grace accepts valid late results before failure commits.
A first result arriving before retry transmission cancels that queued retry.
During retry it is held as fallback; a successful retry takes precedence. Names,
route, RTT and SNR always come from the selected attempt together.

Only routes actually returned during this operation are candidates. Up to three
are retained; exact-route history, directed-edge diversity per estimated airtime,
freshness and last-hop SNR provide bounded ranking. Last-hop SNR is only a tie
breaker, not an end-to-end quality estimate. RAM history expires after 15 minutes
and is cleared across target/radio/scope context changes. This finds a usable
observed route, not a guaranteed globally best route.

## Limits and ownership

- Maximum ten repeater entries, **including the target**. Direct neighbours are
  supported as `You -> Target -> You` with two SNR measurements.
- One- and two-byte path hashes are supported. Longer hashes/paths are rejected
  with a specific message; settings are never silently changed.
- Names are used only for unique known prefixes. Unknown hashes remain hex;
  ambiguous prefixes have `?`. Short hashes are routing hints, not proof of a
  repeater's identity.
- Only one local remote operation runs at a time. Ping, admin, login and sensor
  operations retain their existing owners. Trace uses independent pending state
  and does not clear the companion app's request IDs.
- Incoming app login/telemetry/path/trace requests cancel a local trace before
  normal app handling. Radio/scope/hash configuration changes, power restriction
  and target removal cancel/invalidate it. Trace is unavailable in Child Mode.
- Failure distinguishes discovery timeout, trace timeout after retry, radio
  busy, transmission failure and an unsent retry. Partial overheard progress
  cannot identify a particular failed repeater or complete a trace.

The reference implementation is local `meshcore/sensor` commit
`99dc1448` (`RepeaterTraceProbe`, `TraceTimingPolicy`, candidate/history ranking
and `SensorBotService`). Zen adapts its route builder for a discovered outbound
path; the Bot instead starts from a received channel-command route. Results and
report ownership are local UI state rather than public-channel messages.
