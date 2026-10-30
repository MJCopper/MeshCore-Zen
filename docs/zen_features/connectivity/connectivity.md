# Radio, GPS, Bluetooth and Adverts

[Back to README](../../../README.md)

## Radio

The Radio home page shows frequency, spreading factor, bandwidth, coding rate,
TX power and noise floor. Hold Enter to open **Settings › Radio**. A preset
changes frequency, bandwidth, spreading factor and coding rate together.
All communicating nodes must use compatible radio settings.

The toolbar shows up to four bars for recent reception quality. A hollow
triangle with X means no current measurement; standalone X means no repeaters
answered discovery or the radio is unavailable. See
[Signal Indicator](../signal_indicator/signal_indicator.md).

## GPS

The GPS home page shows position, receiver state and direction of movement.

- **Enter:** turn GPS on or off for this session.
- **Hold Enter:** open **Settings › System › GPS Polling**.
- **Settings › System › GPS:** save the startup choice.

Choose Continuous, Adaptive or timed polling to balance updates and power use.
**Sleep** includes the age of the last position; **Search** means a fix is not
ready. Direction comes from movement, not a magnetic compass.

Zen may briefly run GPS to set the clock even when saved GPS power is Off.
See [GPS and Course](../gps/gps.md) for modes and troubleshooting.

## Bluetooth

Press Enter on the Bluetooth home page to enable or disable BLE for the current
session. A successful change plays the standard acknowledgement sound when the
buzzer is enabled. Hold Enter to open **Settings › Bluetooth**, where the saved
startup state and pairing PIN can be configured. PIN Mode can generate a random
PIN at each boot or retain a fixed six-digit PIN. PIN changes take effect after
reboot. Zen does not expose the companion protocol over USB; USB remains
available for charging and bootloader DFU flashing.

Child Mode and Low Power can temporarily disable Bluetooth without changing
its saved setting. Check [Battery and Low Power Mode](../power/power.md) if
Bluetooth remains unavailable.

## Adverts and location privacy

Press Enter on the Advert home page to send a manual advert, or hold Enter to
open **Settings › Advert**. Auto Advert offers Off, 1 hour, 3 hours and 6 hours;
GPS Details offers Hide or Share. GPS Details applies to manual, automatic and
companion-triggered self adverts. The advert toolbar icon remains visible for
five seconds after an advert is queued.
