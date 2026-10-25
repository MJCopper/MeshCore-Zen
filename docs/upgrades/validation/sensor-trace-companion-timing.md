# Companion-derived trace timing validation

Date: 25 October 2026. Implements the approved
[companion timing plan](../sensor-trace-companion-timing-plan.md). No hardware
was flashed; interoperability with repeater 7188 remains unconfirmed.

## Timing behaviour

The request acknowledgement freezes a calculated maximum at acceptance. The
maximum is rounded up to whole seconds, is not a required wait, and is capped at
120 seconds. Successful traces and definite failures finish earlier. Acknowledged
requests keep their original deadline through retries, status requests, late
responses and route changes.

The timing helpers use the companion formulas plus an explicit 1,000 ms sensor
margin:

| Exchange | Bound in milliseconds |
| --- | --- |
| Flood discovery/login | `1,500 + 16 * request airtime` |
| Round-trip TRACE | `1,500 + (6 * maximum TRACE packet airtime + 250) * (2 * repeater count)` |
| Local TX watchdog | `1,000 + 1.5 * packet airtime` |

Packet airtime is obtained from the active radio configuration and actual packet
length, including hash width, transport-scope bytes, cipher padding, and accumulated
TRACE SNR bytes. The SX126x reference matrix is generated from the pinned
RadioLib calculator implementation, not a separately approximated LoRa formula.
The table covers Australia Narrow SF7/BW62.5/CR7, slower spreading factors,
different coding rates, preambles, headers and CRC, both path-hash widths, and
one through ten repeaters.

For Australia Narrow SF7/BW62.5/CR7, the table gives a 6.62-second guest-login
reply window and a 6.724-second TRACE window for a two-repeater, one-byte-hash
route. Its calculated maximum is 81 seconds when all permitted retries and
reporting are reserved. Without a compatible route hint, the conservative
ten-repeater sizing advertises 109 seconds for one-byte hashes and 117 seconds
for two-byte hashes. These are failure-path bounds; an ordinary successful
login-return-path trace should finish much sooner. Slower PHYs may reach the
120-second ceiling, reduce optional probes, or reject a route that cannot fit a
complete reply window and report reserve.

Queueing shares eight cumulative seconds between acknowledgement, control
traffic and probes. Reporting has a separate two-second cumulative queue
allowance. The report reserve is calculated from actual report airtime and
includes both possible report-part watchdogs, initial scheduling delay,
inter-part gap and report queueing. Optional retries are admitted only when
their full calculated windows fit; a started receive window is never shortened
to squeeze another attempt into the deadline.

## Login-return route

A valid, authenticated flooded guest-login PATH response now supplies a route
candidate directly. It must match the selected full identity, expected login
response fields, an accepted role, active transaction/profile, supported route
encoding and path bounds. The embedded outbound path is converted to the usual
sensor-to-target-to-sensor loop. The selected repeater remains the final outward
node. A fresh TRACE is still required for success, RTT and averaged SNR.

This avoids the redundant second ordinary discovery request when login returns
a usable path. A successful login without a usable path may use the one remaining
ordinary discovery request, then the verified advert-route fallback. Two login
retries remain allowed, each with a fresh durable timestamp. Login, discovery,
TRACE, cancellation, and late-response ownership remain bounded by the same
calculated deadline.

Successful TRACE paths are still kept in RAM history; this change does not add
new persistent TRACE-route storage. Persistent advert hints retain their
existing cache policy.

For bare `!hillvue trace`, the acknowledgement now names the requester using the
sender label in the public message, for example `Trace requested to Alice, please
wait up to X seconds.` This label is copied and normalized when the command is
accepted, so retries cannot change it. It is message-supplied display text, not
an authenticated companion identity. The bare-trace budget uses its incoming
route and allowed probe/retry branch, and does not include targeted discovery or
guest login. Targeted trace acknowledgements continue to name the selected
repeater. Both planned bare probes are sized at acceptance from the incoming
route because passive alternates are not yet known. A longer alternate is used
only if its full timeout, scheduled delay and remaining shared queue allowance
fit; otherwise the original route is retried if it fits. If neither fits, no
second TRACE is sent and the existing `Trace: retry not sent` terminal report is
used. The promise/deadline is never extended.

## Validation

| Gate | Result |
| --- | --- |
| `native`, `native_sensor`, `native_kiss_modem` | 677/677 passed |
| `native_sensor_asan` | 569/569 passed; no AddressSanitizer findings |
| Sensor firmware build and UF2 | Passed |
| Strict MkDocs | Passed after adding this report |
| Pinned upstream-file diff and custom-symbol boundary searches | Passed manually; the script could not run because its `rg` dependency is unavailable |
| `git diff --check` | Passed |

The companion timing test matrix covers the pinned PHY airtime vectors,
routes/hash widths, and the exact 37-second two-repeater bare-trace budget at the
100 ms test airtime. Integration tests cover same-route retry, a longer passive
alternate falling back to the original route, and retry omission when neither
route's complete window fits. The omission emits no second TRACE, preserves the
frozen deadline, and reports `Trace: retry not sent`. Dispatcher and network
tests also cover the authenticated login-return path, retry tags, no redundant
discovery, scoped relays and existing trace output. These are software/network-
model results, not measurements from physical radio hardware.

The timing implementation was also built in the five firmware environments:
`Xiao_nrf52_bme680_sensor`, `Xiao_nrf52_sensor_baseline`,
`Xiao_nrf52_companion_radio_usb`, `Xiao_nrf52_repeater`, and
`Xiao_nrf52_room_server`. After the requester-label-only update, the sensor
firmware and UF2 were rebuilt; no other role's source is affected. No device was
flashed.

The final sensor image uses 46,620 bytes of static RAM and 385,636 bytes of
flash (19.8% and 47.6% of the configured budgets). The direct OTA archive is
`.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Companion_Timing_Bare_Trace_Budget_OTA_2026-10-25.zip`.
Its SHA-256 is
`d6dda5fffc7c9adadd4faeaff5d079ee77636894f542977e258ed9b8e1d7974b`.
The archive is byte-for-byte identical to the verified `firmware.zip`; its root
manifest references non-empty application BIN and DAT files. The matching UF2
is `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2`.

## Field acceptance pending

Compare one companion and one sensor exchange with repeater 7188 under the same
Australia Narrow settings. Record the actual sensor TX, repeater response,
accepted login path, first TRACE, final result, queue delays and calculated
deadline. Confirm login-return-path discovery avoids the redundant request and
that late/lost replies, retry, cancel/restart and advert fallback behave as
expected. The native tests do not establish the cause of the prior field failure
or prove physical interoperability.