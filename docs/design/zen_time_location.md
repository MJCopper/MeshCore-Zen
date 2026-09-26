# Time and location orchestration

[Back to README](../../README.md)

Zen keeps the GPS algorithms separate from their orchestration.
`EnvironmentSensorManager` owns receiver hardware and the existing continuous,
timed and Adaptive schedules. `PeripheralPowerCoordinator` resolves saved,
manual, time-sync, Low Power and Emergency claims. `TimeLocationCoordinator`
observes their explicit runtime state and coordinates clock synchronization,
course samples and UI status without writing preferences.

The sensor manager publishes bounded RAM-only lifecycle events for receiver
start, completed acquisition, timeout, stop and Adaptive phase changes. The
coordinator drains these events from the UI loop, while the sensor manager
remains the sole owner of receiver timing and hardware transitions. User wakes
are routed back through the coordinator to the sensor policy; notification
wakes do not alter the acquisition schedule.

GPS configuration is applied as one value containing enabled state, polling
interval and Adaptive state. Runtime GPS sessions identify their purpose as
continuous, scheduled, Adaptive, time synchronization, manual or Emergency.
Time-sync-only fixes are excluded from course and Travel calculations.

Clock synchronization records a RAM-only source such as GPS, App or Mesh.
Bluetooth companion updates use the App source label. A
restored timestamp is not considered a live synchronization, so `SYNC TIME`
remains until an authoritative source updates the RTC. The existing five-minute
initial GPS attempt, hourly 90-second retries and 48-hour retry window remain
unchanged.

`LocalTimeService` is the shared UTC-to-local boundary for the Clock, Quiet
Time, message timestamps, Diagnostics and timezone editor. UTC remains the
stored system time and timezone changes never rewrite it.

**Tools › Diagnostics › Location** exposes the current purpose, receiver and
fix state, fix age, satellite count, HDOP, Adaptive phase, next acquisition,
course source, last lifecycle event, time-sync source and sync age. These
values and counters are RAM-only.
