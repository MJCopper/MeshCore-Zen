# Tools

[Back to README](../../../README.md)

| Tool | Use it to |
| ---- | --------- |
| Discover Repeaters | Find nearby repeaters and add them as contacts |
| Node List | Inspect and manage repeaters, room servers and sensors |
| Repeater Mode | Forward mesh traffic using this device |
| Ringtone Editor | Create and preview two custom notification melodies |
| Diagnostics | Check signal, battery, storage and recent errors |

## Node List

Use Left/Right to cycle **All → Fav → Rpt → Room → Snsr**. Fav shows starred
infrastructure nodes. Companion contacts remain in Messages and Favourites.
Select a node for details; hold Enter for available actions such as Ping,
Trace, Path details, Favourite, Add, Delete or Admin.

**Discover Repeaters** starts an eight-second scan. Results show SNR beside
each name; open details for RSSI and both signal directions. Hold Enter to
add a contact or rescan.

Starred nodes sort first. Favourite stars are separate from contact speed-dial
pins. Use the Sort action to choose distance or recent activity.

## Node administration

Select a saved repeater, room or sensor, hold Enter and choose **Admin**.
Zen tries permitted access first, then offers password entry. Open a setting,
edit it and confirm the change; **Setting saved** means it was verified.

Radio changes warn that the node may become unreachable; Zen's radio stays unchanged.

Actions and console commands require confirmation. **Confirmed** means the
remote node returned an OK response. **No reply** means the result is unknown
(including reboot); changes are never automatically retried. Login, read-only
requests and setting verification retry once over a known path, then clear a
stale path and try by flood. The console shows other replies verbatim. **Start
OTA** is a dedicated action and its confirmation opens on Cancel.

See [Node Administration](../node_admin/node_admin.md) for login, menus,
console use and troubleshooting. Admin is unavailable in locked Child Mode.

## Other tools

- [Repeater Mode](../repeater_mode/repeater_mode.md) continues after leaving
  Tools and uses your current radio settings.
- **Ringtone Editor** edits Custom1 and Custom2. Choose them under
  **Settings › Sound**; each supports up to 16 notes or pauses.
- [Diagnostics](../diagnostics/diagnostics.md) includes a recent error log and
  [battery estimates](../power/power.md).
