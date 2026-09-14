# Quiet Time

[Back to README](../../../README.md)

Quiet Time suppresses incoming notification sounds during a daily local-time
period. Messages are still received, stored and counted. Eligible notifications
still appear, vibrate where supported, and wake the display for five seconds.

Configure **Quiet Time**, **Quiet from** and **Quiet until** under
**Settings › Sound**. Defaults are Off, 21:00 and 07:00.

The schedule uses the timezone in **Settings › System** and may cross midnight.
City mode applies daylight-saving changes automatically; see the
[Time Zone guide](../timezone/timezone.md).
Equal start and end times disable the interval. Quiet Time remains inactive
until the clock has valid time.

Delivery acknowledgements, key feedback, routing and companion queues are not
affected.
