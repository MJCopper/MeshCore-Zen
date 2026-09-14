# Clock

[Back to README](../../../README.md)

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
Companion time is authoritative and can correct a fast clock backwards. Normal
small connection-time corrections do not cause an immediate flash write.
The temporary GPS schedule is independent of the user's normal polling cadence;
see [GPS and Course](../gps/gps.md).

Time Mode can automatically apply the selected city's daylight-saving rules or
use a fixed manual UTC offset. The same local-time policy is used by the clock,
Quiet Time, diagnostics and message time placeholders. OLED supports 12/24-hour
format and optional seconds; e-ink omits seconds to limit refreshes.
City selection is manual rather than GPS-derived. See the
[Time Zone guide](../timezone/timezone.md) for supported cities, seasonal rules
and fixed-offset behaviour.
