# Quiet Time

[Back to README](../../../README.md)

Quiet Time suppresses incoming notification sounds during a daily local-time
period. Messages are still received, stored and counted. Eligible message and
new-contact popups appear when the display is on, regardless of sound mode.
Screen Wake On follows Notifications Mode (On, Off or Auto); Always wakes the
display even when sound is suppressed by Mode, Quiet Time or DND. Off never
wakes it for these alerts. A contact or channel marked Local can still sound
and wake in connected Auto when Screen Wake is On. Source Off and Child Mode
restrictions continue to block local alerts.

Configure **Quiet Time**, **Quiet from** and **Quiet until** under
**Settings › Notifications**. Defaults are Off, 21:00 and 07:00.

The schedule uses the timezone in **Settings › System** and may cross midnight.
City mode applies daylight-saving changes automatically; see the
[Time Zone guide](../timezone/timezone.md).
Equal start and end times disable the interval. Quiet Time remains inactive
until the clock has valid time.

Delivery acknowledgements, other key feedback, routing and companion queues
are not affected. The Advert, Bluetooth and GPS homepage action tones follow
Silent, Quiet Time and Notifications Mode, so they are quiet when notification
audio is muted.

User-requested ringtone previews remain audible during Quiet Time and DND.

Triple-press Back to toggle Do Not Disturb (DND). It suppresses the same
notification sounds as Quiet Time while permitted screen wakes and unread
counts continue. DND is held only in RAM, resets to Off at reboot and does not
change the Quiet Time schedule or saved Notifications mode. The crossed-speaker
icon appears whenever default notification audio is muted by DND, Quiet Time,
Notifications Off or Auto with a connected client. Local overrides can still
alert in connected Auto mode; deliberate previews can still play in any mode.
The popup says Silent On or Silent Off. Turning Silent Off plays the same short
feedback as the Advert, Bluetooth and GPS homepage actions, only when
notification audio is otherwise enabled.
