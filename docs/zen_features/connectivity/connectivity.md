# Radio, GPS, Bluetooth and Adverts

[Back to README](../../../README.md)

## Radio

The Radio home page shows frequency, spreading factor, bandwidth, coding rate,
TX power and noise floor. Hold Enter to open **Settings › Radio**. A preset
changes frequency, bandwidth, spreading factor and coding rate together.
All communicating nodes must use compatible radio settings.

The toolbar signal indicator estimates recent repeater-link quality from the
SNR of routed traffic and repeater adverts. Direct companion traffic is ignored.
Three bars represent high link margin, two medium and one low; a cross means no
fresh measurement or that the radio is unavailable. The value is averaged in
RAM and expires after two hours.

When the user wakes the display with Back and the last measurement is over 30
minutes old, Zen performs one silent repeater-only discovery. Notification and
alarm wakes do not trigger it, and unsuccessful scans are limited to one attempt
per 30 minutes. Discovery is authoritative: the strongest response becomes the
new reading, or the indicator changes to a cross after the eight-second window
when no repeater answers. Manual Discover Repeaters scans behave the same way.
See the [Signal Indicator guide](../signal_indicator/signal_indicator.md) for
measurement sources, thresholds, timing and troubleshooting.

## GPS

See the dedicated [GPS and Course guide](../gps/gps.md) for the screen layout,
power behaviour, acquisition quality, retry backoff and course-over-ground
operation.

The GPS home page shows polling mode, receiver state, position, altitude,
satellites and an eight-point course-over-ground tape. The first row keeps the
power and polling states separate: the left side always shows the configured
polling cadence, while the right side shows **Off**, **Sleep**, **Search** or
**Fix** from the actual receiver state. Temporary time-sync acquisitions therefore
show Search or Fix even when saved GPS power is Off. While sleeping, the state
also shows the age of the last fix, such as **Sleep 12m** or **Sleep 2h**.
The centred, inverted
direction is the direction of movement. A bearing is accepted after about 10 m
of accumulated, plausible movement at 0.8 knots or faster. Brief fix loss and
speed dips are tolerated, recent course samples are circularly smoothed, and
sector hysteresis prevents flicker near direction boundaries. After stopping,
**Last: direction** remains available for 15 minutes in RAM, then changes to
**No course**.

For timed polling, Zen separately compares completed polling fixes in RAM.
Movement of at least 20 m is shown as **Travel: direction**; moderate HDOP raises
the live and Travel distance thresholds, while poor HDOP is rejected. Short
polling moves accumulate against the last accepted endpoint. A failed poll
retains the last Travel result, while a successful short poll clears its display.
Travel expires after twice the polling interval, bounded to 15 minutes–12 hours.
It is the straight-line direction between fixes, not the route followed.

Timed acquisition still has a 90-second cap. A stationary fix completes after
four seconds of acceptable quality. Once movement is detected, its 25-second
capture window starts at that detection and remains latched. Completion requires
HDOP 4.0 or better, or at least eight satellites when HDOP is unavailable.
After consecutive 90-second failures, only the runtime retry delay backs off to
2× and then 4×, capped at six hours; the configured polling selection is not
changed, and the normal interval resumes immediately after a successful fix.

This is not a magnetic heading. Press Enter to turn GPS on or off for the
current session without writing to flash. Hold Enter to open
**Settings › System › GPS Polling**.

Saved GPS power and polling are separate settings. Polling can be Continuous,
2 min, 5 min, 15 min, 30 min, 1 h, 3 h or 6 h. Timed polling obtains a stable
fix, sleeps the receiver, then repeats after the selected interval. Settings
changes are staged and applied and saved only when leaving Settings.

At boot, Zen can temporarily use GPS to set the clock even when its saved GPS
power setting is Off. See the [Clock guide](../clock_screen/clock_screen.md).

## Bluetooth

Press Enter on the Bluetooth home page to enable or disable BLE for the current
session. A successful change plays the standard acknowledgement sound when the
buzzer is enabled. Hold Enter to open **Settings › Bluetooth**, where the saved
startup state and pairing PIN can be configured. PIN Mode can generate a random
PIN at each boot or retain a fixed six-digit PIN. PIN changes take effect after
reboot. BLE takes priority over USB serial.

## Adverts and location privacy

Press Enter on the Advert home page to send a manual advert, or hold Enter to
open **Settings › Advert**. Auto Advert offers Off, 1 hour, 3 hours and 6 hours;
GPS Details offers Hide or Share. GPS Details applies to manual, automatic and
companion-triggered self adverts. The advert toolbar icon remains visible for
five seconds after an advert is queued.
