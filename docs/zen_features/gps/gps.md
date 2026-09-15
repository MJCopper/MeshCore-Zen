# GPS and Course

[Back to README](../../../README.md)

## GPS home page

The GPS home page shows the configured polling cadence, current receiver state,
position, altitude, satellite count and direction of travel.

The receiver state is separate from the saved GPS setting:

| State | Meaning |
| ----- | ------- |
| Off | GPS is disabled |
| Sleep | Timed polling is enabled and the receiver is between acquisitions |
| Search | The receiver is powered but does not yet have a usable fix |
| Fix | The receiver is powered and has a valid fix |
| HW Off | The hardware GPS switch is off |

While sleeping, the state includes the age of the most recent fix, for example
`Sleep <1m`, `Sleep 12m` or `Sleep 2h`. The fix and its age are held in RAM and
do not create flash writes.

- Press **Enter** to turn GPS on or off for the current session. This does not
  alter the saved startup setting.
- Hold **Enter** to open **Settings › System › GPS Polling**.
- Use **Settings › System › GPS** to save whether GPS is enabled after startup.

## Polling and power

GPS polling can be Continuous, Adaptive, 2 min, 5 min, 15 min, 30 min or 1 h.
The factory default is GPS power Off with Adaptive selected, so enabling GPS
uses Adaptive without requiring another settings change.
Continuous keeps the receiver powered. A timed mode powers it only for an
acquisition and then returns it to standby for the selected interval. The next
interval begins after the receiver returns to standby.

Adaptive starts with a five-minute acquisition allowance. Once it has a good
fix, it remains continuous while that fix is useful. If fix quality remains poor
for five minutes, it changes to 90-second searches separated by two-minute
standby periods. After 15 minutes in backoff, standby increases to five minutes.
A good fix returns immediately to continuous tracking. The backoff state is held
only in RAM and does not write preferences.

A timed acquisition ends when one of these conditions is met:

- a stationary, acceptable-quality fix has remained stable for four seconds;
- movement is detected and a 25-second movement capture has completed; or
- the 90-second acquisition limit is reached.

The movement window starts when movement is first detected and is not extended
by continued movement. Constant movement therefore does not turn a timed mode
into Continuous mode.

A completed fix requires HDOP 4.0 or better. If HDOP is unavailable, eight or
more satellites are accepted as the fallback quality test. After consecutive
90-second failures, Zen delays the second and subsequent retries to 2× the
chosen interval. Backoff never exceeds 2× and the absolute delay remains capped
at two hours. A successful fix immediately clears the backoff. This is runtime
behaviour only; the saved polling selection never changes.

If the display is off, pressing Back to wake it clears a pending failure backoff
and starts an acquisition immediately. Timed modes receive one normal retry;
Adaptive receives a fresh five-minute search allowance. If Adaptive is already
searching, the receiver is not restarted and only its deadline is extended.
Notification and alarm wakes do not reset GPS backoff.

During Low Power, GPS remains disabled. GPS enabled manually for the ten-minute
Emergency window stays continuous and bypasses Adaptive backoff until that
window ends. The GPS page shows `Retry Xm` while Adaptive is in standby.

Timed modes usually consume much less power than Continuous because the GPS
hardware is in standby between acquisitions. The receiver uses its normal
measurement rate whenever it is awake. Poor reception increases consumption
because an unsuccessful acquisition can remain active for the full 90 seconds.

## Direction display

The bottom line is a course-over-ground display. It calculates direction from
GPS movement and is not a magnetic compass, so it cannot determine which way the
device is pointing while stationary.

While moving, a 32-position tape scrolls around the current direction. The eight
named points are separated by three dots, with every dot representing another
11.25° course increment. The inverted centre label or dot is the direction of
travel. Zen accepts live course after
approximately 10 m of plausible movement at 0.8 knots or faster. Moderate HDOP
raises the required movement to reduce GPS jitter. It smooths the five most
recent course samples and applies boundary hysteresis so the tape does not
continually scroll between adjacent increments. The finer display is an
approximate course indication rather than a claim of 11.25° GPS accuracy.

Brief loss of fix or a short speed drop is tolerated. When movement stops, the
last live direction is shown as `Last: NE` for up to 15 minutes. It then changes
to `No course` unless a newer direction source is available.

## Direction with timed polling

Timed polling may not remain awake long enough for a continuous course, so Zen
also compares completed polling fixes. Movement of at least 20 m between good
fixes produces a result such as `Travel: SW`; moderate HDOP raises this threshold
to 30 m. Several shorter moves can accumulate from the previous accepted anchor.

Travel is the straight-line bearing between the fixes, not the route travelled.
Its lifetime is twice the selected polling interval, with a minimum of 15 minutes
and a maximum of two hours with the available polling choices. A failed
acquisition retains the previous Travel
result. A successful acquisition without enough movement clears the displayed
Travel result while retaining the anchor for later accumulated movement.

The display chooses the freshest useful source in this order:

1. live course while GPS movement is being tracked;
2. Travel calculated between completed timed acquisitions;
3. the last live course retained after stopping;
4. `No course`.

## Time, privacy and Low Power Mode

At boot, Zen can temporarily power GPS to set the clock even when saved GPS is
Off. The first attempt lasts up to five minutes, followed by 90-second hourly
attempts for 48 hours. `SYNC TIME` remains on the Clock page until time is set,
even after automatic GPS attempts finish. A companion connection can also set
the time.

Location is included in self adverts only when **Settings › Advert › GPS
Details** is set to Share. The same privacy setting applies to manual, automatic
and companion-triggered adverts.

Low Power Mode turns GPS off. During its ten-minute Emergency Mode, GPS remains
off initially but may be enabled manually from the emergency or GPS home page.

## Troubleshooting

- Move outdoors with a clear view of the sky if Search persists or acquisitions
  repeatedly reach 90 seconds.
- If polling is waiting in failure backoff, turn the screen off and wake it with
  Back to request an immediate new acquisition.
- Check the Wio Tracker hardware GPS switch if `HW Off` is displayed.
- A valid position without a direction is normal when stationary or before the
  minimum movement distance has been reached.
- Long polling intervals provide lower power consumption but make Travel updates
  less frequent.
