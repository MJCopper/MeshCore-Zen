# Tools

[Back to README](../../../README.md)

| Tool | Purpose |
| ---- | ------- |
| Discover Repeaters | Scan for zero-hop repeaters and add results as contacts |
| Node List | Browse, filter, inspect, ping and manage known nodes |
| Repeater Mode | Toggle the companion repeater backend |
| Ringtone Editor | Edit and preview two 16-note chromatic notification melodies |
| Diagnostics | View device, radio, mesh and runtime information |

Node List opens on **All**. Use Left/Right for the Fav, All, Comp, Rpt, Room and
Snsr filters; hold Enter for actions available to the selected node.

Diagnostics includes a RAM-only Events tab containing the 16 most recent
operational warnings and errors. Newest events appear first, `W` and `E` mark
severity, and consecutive duplicates are combined. Use Up/Down to select an
event and Enter to open its complete details. Hold Enter on Events to clear the
log; it is also cleared by reboot. No event is written to flash.
Warnings and errors use the normal popup notifier. Repeated background failures
are rate-limited, and wake for five seconds when the display was off.

The Battery tab shows filtered voltage, curve-based percentage
and estimated time to 3.3 V. Remaining starts from a five-day full-charge model,
ignores the first two hours after charging, then gradually learns from six to
24 hours of normal discharge. Samples remain RAM-only. Charging and Paused
replace the time when appropriate.

See the dedicated [Diagnostics](../diagnostics/diagnostics.md) and
[Battery and Low Power Mode](../power/power.md) guides.

## Node administration

Select a saved repeater, room or sensor in Node List, hold Enter and choose
**Admin**.
Zen first tries ACL login with an empty password, then offers password entry.
The remote node must grant admin rights. Credentials stay in RAM; Back returns
to the same Node List filter and position.
Silent login failures identify whether ACL/route or password/time is the
unresolved cause. MeshCore servers intentionally send no rejection detail, so
Zen cannot distinguish an incorrect password from server replay protection.

Menus provide Status, Settings, Radio, Routing, type-specific options, Console
and Actions. Only room servers show Room options. Open a setting to fetch its
current value. Finish editing with Enter or Back, then choose Yes or No at the
save confirmation. Unchanged values send nothing. After a successful write, Zen
reads the setting back and only reports **Setting saved** when the value matches.
The write-only administrator password is confirmed by the node's save response.
The Neighbours status view replaces known public-key prefixes with names from
the local contact list and leaves unknown prefixes in hexadecimal. Each entry
uses one line in `name age signal` form. The selected name scrolls horizontally
when required, while age and signal remain fixed at the right.
Signal is decoded from MeshCore's quarter-dB wire value; positive values omit
the sign, negative values retain it, and both use one decimal place. SNR is
right-aligned while the name uses all space remaining before the measurements.
Radio changes warn that the node may become unreachable; Zen's radio stays unchanged.
Routing includes forwarding, hop limits, path hashing, RX delay, flood and direct
TX delays, duty cycle, channel detection, Multi ACKs, interference threshold and
AGC reset interval.

Console uses predictive T9 with commands and keywords from the
[MeshCore CLI reference](https://docs.meshcore.io/cli_commands/) ranked ahead of
chat words. Dotted names such as `flood.advert.interval` are single candidates;
T9 supplies their dots automatically. Hold Enter (CardKB: Tab) opens alternatives.
ABC and CardKB use the same command-first completion list. Completing a word
adds a trailing space. Password and structured setting fields remain literal.

Actions and console commands require confirmation. **Confirmed** means the
remote node returned an OK response. **No reply** means the result is unknown
(including reboot); changes are never automatically retried. Login, read-only
requests and setting verification retry once over a known path, then clear a
stale path and try by flood. The console shows other replies verbatim. **Start
OTA** is a dedicated action and its confirmation opens on Cancel.

Admin is unavailable while Child Mode is locked. Leaving Admin clears its
session. Commands use tagged replies and current contact paths, without background
polling. Local cancellation and timeout do not block the next command because
reply tags reject late responses. Phone commands retain priority; after app
overlap, wait for its response timeout plus one minute. Nodes must support MeshCore's echoed
CLI prefix; untagged replies cannot populate an editor.

See [Node Administration](../node_admin/node_admin.md) for the concise operating
guide.

Discover Repeaters starts a repeater-only scan immediately. Its list shows the
right-aligned, one-decimal locally received SNR beside each name; result details retain RSSI, local SNR and
remote SNR. Hold Enter on a result to manage it or rescan.

Stored Node List entries can be starred independently of Favourites Dial pins.
Starred nodes sort first within the selected filter.

Repeater Mode uses the current **Settings › Radio** configuration. Its fixed
backend values are RX delay `10`, flood/direct airtime factors `0.5`/`0.3`,
Yield `x2`, and duplicate suppression On. It continues in the background after
leaving Tools.

See the [Repeater Mode guide](../repeater_mode/repeater_mode.md).
