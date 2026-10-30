# Clock

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

Clock is always the first home page. It shows the status bar, time, date and the
total unread **Messages** count.

- **Enter** opens the newest unread conversation, or the most recent
  conversation when nothing is unread.
- **Back** from another home page returns to Clock.
- **Back** on Clock turns the display off.
- Another home page returns to Clock after five minutes without input.

At boot, `SYNC TIME` replaces the time and date until GPS, a companion app or
another network source supplies valid time. GPS may run for five minutes on the
first attempt, then for up to 90 seconds each hour for 48 hours. GPS is powered
down between attempts when its configured mode is Off. `SYNC TIME` remains
until time is eventually set, even after automatic attempts stop.
A companion connection can also correct the clock. See
[GPS and Course](../gps/gps.md) for normal GPS polling.

Choose city daylight-saving rules or a fixed UTC offset under
**Settings › System › Time Zone**. City selection is manual, not GPS-derived.
OLED offers 12/24-hour time and optional seconds; E-ink omits seconds.
See [Time Zone](../timezone/timezone.md).
