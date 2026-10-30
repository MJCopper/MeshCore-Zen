# Battery and Low Power Mode

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

Zen treats 4.12 V as full and 3.3 V as empty using a non-linear single-cell
Li-ion/LiPo curve. The device shuts down at or below 3.3 V unless externally
powered.

At 20% or below, Zen gives one short **Low Battery** alert and repeats it hourly
while on battery. Quiet Time suppresses the sound. The alert does not create an
unread message. Its popup, five-second wake, sound and vibration use the same
notification policy as message alerts.

## Low Power mode

After three consecutive samples at 5% or below, Zen suspends the radio, GPS and
Bluetooth, stops automatic retries, and uses minimum OLED brightness with a
five-second display timeout. CardKB remains usable while the display is awake.
External power leaves Low Power mode and restores the saved settings.

**Tools › Diagnostics › Power** shows the applied display, CardKB, GPS,
Bluetooth and radio states. **Applied** means they match current policy;
**Pending** identifies a transition that could not be confirmed and will be
retried by the next relevant state change.

## Emergency communications

Low Power adds **Low Power Emergency** at the end of the carousel. Press Enter
and confirm to enable radio and messaging for ten minutes. GPS and Bluetooth
remain off initially but can be enabled temporarily from their home pages. The
Emergency page shows remaining time and all three statuses. Press Enter there
again to end the window early.

Ending Emergency Mode clears its temporary GPS and Bluetooth choices. If the
device remains in Low Power mode, those peripherals and the radio return to
their restricted state. Leaving Low Power restores the saved GPS, Bluetooth,
brightness and radio behaviour.

## Remaining-time estimate

**Tools › Diagnostics › Battery** shows filtered voltage, percentage and time
remaining to 3.3 V. It starts with a five-day full-charge model and gradually
learns from normal discharge, so the result is an estimate rather than a
guaranteed runtime.

## Top-bar charging status

The home-page battery display shows a flashing lightning bolt when USB power is
connected and a tick when the filtered battery voltage reaches **4.15 V**.
Charging is inferred from USB power; completion is inferred from voltage, not
reported by the charger. The tick remains until voltage falls below **4.12 V**
or USB is disconnected. Reconnecting below 4.15 V shows the bolt again.

The battery-level bar remains visible while the lightning bolt flashes once
per second. Percentage and voltage modes keep their values beside the charging
symbol. E-ink updates can be slower because of the display's refresh time.

A plug symbol means external power is present but the charge reading is
unavailable. Disconnecting USB restores the normal battery display.

State is checked with the existing battery sample every eight seconds and uses
the smoothed voltage, so the indication may lag a connection or voltage change.
A changed status refreshes the visible home-page top bar without waking a
sleeping display. The percentage estimate
reaches 100% at 4.12 V; that estimate is independent of the 4.15 V completion
indicator.
