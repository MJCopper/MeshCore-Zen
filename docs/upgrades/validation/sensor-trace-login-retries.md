# Two guest-login retries — 25 October 2026

Guest login now allows an initial attempt and two retries: at most three login
transmissions per targeted trace. An unanswered reply window or failed TX can
advance to the next attempt after bounded backoff (250 then 500 ms). Every
submitted login uses a fresh timestamp from the existing durable clock. Local
allocation/admission retries remain within the current queue lease.

An accepted reply stops further logins and enables the second ordinary
path-discovery request. Invalid/unexpected roles, clock failure, queue expiry,
cancellation, exhausted attempts and the control cutoff do not generate more
logins. Protected guest access is tested through all three attempts and still
permits successful advert fallback. Cancellation and stale callbacks retain
existing ownership protections.

Each attempt retains a separate eight-second queue allowance, airtime-based TX
watchdog and up to eight seconds for the acknowledgement after successful local
TX completion. The control cutoff moves from 25 to 40 seconds from acceptance
to accommodate three reply windows plus ordinary discovery. All phases are
clipped to that cutoff; arbitrary queue delays can prevent all attempts fitting.
The original 55-second work and 60-second overall deadlines remain. This leaves
15 seconds after the latest control cutoff for probe work and five for final
reporting; long routes are not guaranteed to complete within the hard cap.

At most ten application-originated packets can emit: two discovery requests,
three logins, two TRACE probes, one acknowledgement and two report parts.
Diagnostics preserve slots 0/1/2 (initial discovery, first login, final discovery)
and add slots 3/4 for the two login retries. Status includes login attempt N/3.

Path persistence is unchanged: successful trace results update RAM-only route
history (15-minute TTL). A newly discovered path is not inserted into the
persistent advert-route cache. An already matching advert hint is marked as
validated in RAM; that outcome alone does not schedule a flash checkpoint.
Advert-derived hints persist via the existing deferred advert-cache saves.

Four new regressions cover exactly three attempts with independent receive
windows, stopping on second-attempt success, absolute cutoff/cancellation, and
three real Dispatcher transmissions using distinct durable timestamps. Existing
single-login timeout tests now expect the bounded retry state. Protected-target
integration expects three login requests. Busy/status tests explicitly advance
polls or the updated control deadline rather than relying on the old cutoff.

Validation passed: **479/479 regression tests**, **372/372 sanitizer tests**,
sensor firmware/UF2/DFU build, strict MkDocs, sensor-boundary check and diff checks.

Logs:

- `/tmp/trace-login-retries-all-tests.log`: native, native_sensor, native_kiss_modem.
- `/tmp/trace-login-retries-asan.log`: existing isolated native_sensor_asan config,
  ASAN_OPTIONS=detect_leaks=0.
- `/tmp/trace-login-retries-build.log`: Xiao_nrf52_bme680_sensor buildprog + create_uf2.
- Strict MkDocs, sensor-boundary integration check and git diff --check.

OTA: `.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Trace_Login_Retries_OTA_2026-10-25.zip`.
SHA256: `fce20672fec24d85e510de37f018d624bfedf7ebcc06051296c71319037157e8`.
Archive CRC/root manifest/application BIN/DAT validated. Named archive equals the
current firmware.zip; earlier named OTA packages remain untouched. No hardware
was flashed. Successful discovery/login with repeater 7188 remains unconfirmed.
