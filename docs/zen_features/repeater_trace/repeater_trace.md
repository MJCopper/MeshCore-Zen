# Repeater Trace

[Back to README](../../../README.md)

Open **Tools › Node List**, select a repeater, hold Enter and choose **Trace**.
Zen finds a route, then measures the trip to that repeater and back. The
repeater needs normal MeshCore support; the Hillvue Bot is not required.

The progress screen shows discovery/trace phases and attempt numbers. Use
Up/Down to scroll the result. Back closes the view and cancels queued work and
future retries. Leaving Nodes or entering Child Mode also cancels an operation.
Already-transmitted packets cannot be recalled; late results cannot reopen a
cancelled view. A completed result stays available in that view until dismissed
or replaced. Hold Enter on the trace screen to open its action menu and choose
**Re-Trace** for a fresh discovery and trace to the same repeater. This also
works during an active trace: queued work and retries are cancelled before the
new operation starts. Back from the action menu returns to the current trace.

## Read the result

A result shows total round-trip time (**RTT**) and signal-to-noise ratio
(**SNR**) for each link in both directions:

```text
You -> Ridge -> Target -> Ridge -> You
RTT: 1540 ms

Link                 Out       Back
You <> Ridge         +5.25     +0.00 dB
Ridge <> Target      -2.00     +4.50 dB
```

SNR is measured by the receiver at each end. Zero and negative readings are
valid. RTT covers the loop itself, excluding route discovery and time spent
waiting to transmit. Individual hop times are not available.

## Progress and retries

The progress view shows the current stage, attempts and remaining time. An
operation can take up to **120 seconds**, depending on radio settings and route
length. Zen retries discovery and the loop trace when time permits, and can
use a recently heard repeater advert to find an alternative route. Every
successful result comes from a fresh trace, not a cached measurement.

Settings → Radio → **Trace guest** controls public guest login (On by default).
Off permits ordinary discovery and advert fallback without guest login. Stored
administrator passwords are never used automatically. Silent guest login may
mean protected access, congestion or RF loss; it is not proof of denial.
Trace never installs its temporary peer/path as a saved contact route.

A failed trace reports whether discovery timed out, the trace timed out, or the
radio could not send. Try **Re-Trace** after checking that the repeater is
reachable and radio settings match.

## Limits

- Maximum ten repeater entries, **including the target**. Direct neighbours are
  supported as `You -> Target -> You` with two SNR measurements.
- One- and two-byte path hashes are supported. Longer hashes/paths are rejected
  with a specific message; settings are never silently changed.
- Names are used only for unique known prefixes. Unknown hashes remain hex;
  ambiguous prefixes have `?`. Short hashes are routing hints, not proof of a
  repeater's identity.
- Run one local remote operation at a time. An active phone operation may cancel
  a local trace; close it before retrying.
- Radio or scope changes, Low Power, target removal and locked Child Mode can
  stop a trace. Partial progress cannot identify a particular failed repeater.
