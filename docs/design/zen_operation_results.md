# Operation results

[Back to README](../../README.md)

Zen represents operational outcomes as structured values rather than screen-
specific error strings. Each result contains an operation, outcome, reason and
optional context such as the node-key prefix, route and attempt number.

`OperationResultCoordinator` is the single publication point. It updates an
optional screen status, records warnings and failures in the RAM-only
diagnostic log, and requests a pop-up through the notification framework.
Background duplicates are combined in the log and suppress repeated pop-ups
for five minutes. No diagnostic result is written to flash.

Remote admin, room login and sensor telemetry translate their shared request
lifecycle through `RemoteOperationResultAdapter`. Persistence, messaging,
radio, power and UI actions publish the same result model directly. This keeps
the result shown on a feature screen consistent with the Events tab and any
warning or error pop-up.
