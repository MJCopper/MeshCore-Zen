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

## Discovery, public guest login and fallback

Trace first sends a standard targeted flooded telemetry request. If it receives
no usable path, it can introduce Zen's identity with empty-password public guest
login. A usable authenticated login PATH supplies the route directly, avoiding a
redundant discovery flood. Login success without a usable path permits the
remaining ordinary discovery request. Failed control discovery can fall back to
a route hint learned from a signature-verified repeater advert. Every hint still
needs a new successful loop TRACE; cached routes never supply cached results.

Settings → Radio → **Trace guest** controls public guest login (On by default).
Off permits ordinary discovery and advert fallback without guest login. Stored
administrator passwords are never used automatically. Silent guest login may
mean protected access, congestion or RF loss; it is not proof of denial.
Trace never installs its temporary peer/path as a saved contact route.

One operation permits at most **two ordinary discovery submissions, three public
guest-login attempts and two loop TRACE submissions**, subject to the overall
time budget. Forwarding and replies add RF traffic. No public-channel
acknowledgement or result report is sent. Automatic signal discovery pauses
while Trace is active. A conflicting local operation or active app login is busy.

The progress view shows stages, attempts, route provenance, queue submissions
and remaining/maximum seconds. Its calculated maximum is frozen at acceptance
and capped at **120 seconds**; successful traces finish sooner. Queueing shares
**eight cumulative seconds** across control and loop work. An optional retry is
sent only when its full calculated window fits. Slow/long routes that cannot fit
are reported explicitly rather than receiving a shortened reply window.

Discovery/login reply time starts after actual TX completion. TRACE RTT starts
at actual loop TX start. Both use active-radio airtime, including hash width,
scope/encryption overhead and accumulated TRACE samples. The loop retry delay is
randomised one to two seconds; final grace is three seconds. A first result
arriving before retry transmission cancels that queued retry. During retry it
is retained as fallback, and a successful retry takes precedence. Route, RTT and
both directional SNR values always belong to the selected attempt together.

Live alternatives use exact-route history, directed-edge diversity, airtime,
freshness and last-hop SNR. The separate advert fallback cache keeps one route
for up to eight repeaters in RAM only, with full identity, scope/radio isolation,
duplicate suppression and a 24-hour monotonic age limit. It starts empty after
reboot and fills passively from new verified adverts. No advert hints are read
from or written to flash; old cache files are ignored. Without a cached hint,
normal fresh discovery and trace retries still operate. An advertised path is
only a mutable hint and must pass a fresh TRACE.
All MeshCore unique request timestamps share one identity-bound durable clock,
including app login, telemetry and CLI paths. This preserves replay sequencing
across reboot without changing displayed wall time. Journal faults fail trace
control safely; a compatible advert hint can still permit an advert-only trace.
Plain repeater-message retries preserve their logical wire timestamp.

Local/private diagnostic commands are `get trace.diag`, `get trace.login` and
`set trace.login public|off`. Diagnostics include submissions, queue use, frozen
time budget, scope differences, clock health and route provenance. Return-scope
differences are recorded for authenticated owned replies, not treated as proof
that a valid full-identity reply belongs to another operation.

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

The reference is local `meshcore/sensor` commit `6f5506a3`. Zen follows its
targeted discovery/login, timing, budget-aware retry and advert fallback process,
with local presentation and a smaller cache. Both outward and return SNR values
remain visible instead of the Bot's averaged-SNR summary.
