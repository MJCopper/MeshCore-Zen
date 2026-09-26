# Battery and Low Power Mode

[Back to README](../../../README.md)

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

Saved settings express the user's requested state; temporary restrictions do
not overwrite them. Zen resolves Low Power, Child Mode, Emergency Mode,
notification wakes and temporary GPS time-sync requests in RAM, then applies
only hardware states that changed. This avoids extra flash writes and
background polling.

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
