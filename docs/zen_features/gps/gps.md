# GPS and Course

[Back to README](../../../README.md)

The GPS home page shows position, altitude, satellites, receiver state and
direction of travel. The left side shows the selected polling mode; the right
side shows what the receiver is doing.

| State | Meaning |
| ----- | ------- |
| Off | GPS is disabled |
| Sleep | Waiting between position checks; the age of the last fix is shown |
| Search | Looking for a usable fix |
| Fix | A current position is available |
| HW Off | The hardware GPS switch is off |

- **Enter:** turn GPS on or off for this session.
- **Hold Enter:** open **Settings › System › GPS Polling**.
- **Settings › System › GPS:** choose whether GPS starts enabled.

The last position remains visible while sleeping or searching. Dashes mean no
position has been obtained yet.

## Choose a polling mode

| Mode | Best suited to |
| ---- | -------------- |
| Continuous | Frequent position and direction updates; higher power use |
| Adaptive | Continuous tracking with a good fix, less frequent searches when reception is poor |
| 2 / 5 / 15 / 30 min / 1 h | Periodic position checks with standby between checks |

Factory settings are GPS Off with Adaptive selected. Adaptive initially allows
five minutes to find a good fix. After sustained poor reception it uses shorter
searches with two-minute, then five-minute rests. A good fix restores continuous
tracking.

Timed modes search for up to 90 seconds. A stable stationary fix can finish
sooner; movement allows a short direction-capture period. Repeated failures
increase the wait between searches, up to twice the chosen interval. A good
fix restores the normal interval.

Wake the screen with Back to request a fresh search during a failure backoff.
Notification wakes do not restart GPS searches. The page shows **Retry Xm**
when Adaptive is resting.

## Direction display

The bottom line is a course-over-ground display. It calculates direction from
GPS movement and is not a magnetic compass, so it cannot determine which way the
device is pointing while stationary.

The centred, inverted direction on the scrolling tape shows your live course.
It normally needs about 10 m of movement at 0.8 knots or faster; poor-quality
fixes need more movement. After stopping, **Last: direction** can remain for
15 minutes, then changes to **No course**.

With timed polling, **Travel: direction** compares successive good fixes after
at least 20 m of movement. Shorter moves can accumulate before a direction is
shown. Travel is the straight-line direction between fixes, not the route
followed. It expires after twice the polling interval, with a 15-minute minimum
and a two-hour maximum for the available intervals.

## Time and location privacy

Zen may briefly use GPS to set the clock even when saved GPS power is Off.
See [Clock](../clock_screen/clock_screen.md) for time-sync behaviour.

Location is included in self adverts only when **Settings › Advert › GPS
Details** is set to Share. This applies to manual, automatic and
companion-triggered adverts.

During Low Power, GPS remains disabled. GPS enabled manually for the ten-minute
Emergency window stays continuous and bypasses Adaptive backoff until that
window ends. The GPS page shows `Retry Xm` while Adaptive is in standby.

## Troubleshooting

- Move outdoors with a clear view of the sky if Search persists.
- Check the hardware GPS switch when **HW Off** appears.
- No direction is normal while stationary or before enough movement.
- Longer polling intervals save power but update position and Travel less often.
- **Tools › Diagnostics › Location** shows fix age, satellite count, fix quality
  and the next search time.
