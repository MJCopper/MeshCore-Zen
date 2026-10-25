# Zen repeater trace: sensor adaptation and verification

Zen v2.2.49 follows `meshcore/sensor` commit
`6f5506a3` for targeted discovery, first-contact guest login, response timing,
transaction budgeting, retries and advert-route fallback. The immutable source
commit and source hashes are recorded in `tools/zen_trace_reference.json`.
`RepeaterTraceSession` owns the local transaction; `MyMesh::TraceAdapter` encodes
packets through existing MeshCore routing. `RepeaterTraceView` remains local
presentation, including separate signed outward/return quarter-dB SNR.

## Discovery and timing

The control owner ports sensor `TraceDiscovery`: initial authenticated REQ,
optional public guest login (up to three attempts), direct use of an authenticated
login-return PATH, remaining ordinary REQ when required, and advert fallback.
There are at most two ordinary discovery requests and two loop probes. Empty
password guest introduction is explicit Settings/Radio policy, On by default;
existing administrator credentials are not automatically used. Unexpected admin
roles do not establish guest readiness. Silence is not diagnosed as proven denial.

Queue waiting, transmission watchdog and response waiting have separate clocks.
REQ/login reply windows begin at TX completion. Loop RTT begins at actual TRACE
TX start. Formula inputs come from the active radio's packet-airtime estimator:

- Flood reply: `1500 + 16 * request_airtime` ms.
- Loop reply: `1500 + (6 * maximum_trace_airtime + 250) * 2 * repeaters` ms.
- Local transmission watchdog: `1000 + 1.5 * airtime` ms.

Sizes account for scope overhead, cipher padding, one-/two-byte hashes and
accumulated TRACE samples. The test PHY calculator is extracted verbatim from
the installed pinned RadioLib SX126x source by
`tools/generate_zen_trace_airtime.py`, with a body digest in its generated header.
The matrix includes Australia Narrow SF7/BW62.5/CR8, CR7, SF5 through SF12,
other bandwidths/coding rates, preambles, CRC/header choices, every supported
route length, both widths and scoped/unscoped packet sizes (440 combinations).

The local budget omits the Bot's public acknowledgement/report airtime. It is
calculated once at acceptance, rounded to seconds, and capped at 120 seconds.
Eligible queue waiting shares eight cumulative seconds. An optional retry needs
its complete receive window, scheduled delay, remaining queue allowance and
final grace. The original route is used if a longer alternate cannot fit; no
second probe is sent if neither fits. A slow/long unknown route can be rejected
before transmission if its conservative budget cannot support an attempt.

Settling/quiet timing, random 1–2-second retry pause, history/diversity ranking,
late-first-result retention and three-second final grace follow the sensor
process. Expired queued work is removed before polling Dispatcher. In-flight
packets remain owned exclusively by Dispatcher. Deadline/status updates never
extend the original transaction. The shared screen hold spans all active stages,
then releases into the normal configured timeout. Manual blanking still wins.

## Ownership and scope

Current and two retired discovery peers allow authenticated late PATH drainage
without overwriting saved contact routes or transmitting reciprocal PATHs.
Full target identity, actual emitted request tag and active transaction state
control acceptance. Login fields/role and TX-completion readiness are checked
separately because login replies carry server time rather than the request tag.
Current app/UI login owners take precedence over late-login drainage. Starting
Trace while an app login is still active is refused; incoming app operations
retain their existing cancellation/ownership path.

Effective outgoing scope, default/unscoped overrides, radio settings, hash width
and local identity contribute to context. Control packets retain the captured
scope. Authenticated owned responses may return on a different scope; a bounded
counter records the difference instead of discarding the valid full-identity
reply. Context changes cancel active work and invalidate local route history.
`get trace.diag` exposes bounded queue/submission/time/scope/clock/provenance
values; `get/set trace.login` exposes the same public/off policy as Settings.

## RAM-only advert fallback hints

Only the upstream signature-verified advert callback feeds the cache. The route
itself remains mutable and is only a hint, requiring a fresh TRACE result.
Malformed lengths, unsupported paths and unknown/foreign scopes are excluded.
Temporary scope overrides do not feed cached observations.

Zen retains one hint for each of eight full-identity repeaters in 672 bytes.
Each entry contains only the public key, directed path, hash width/hop count,
radio/profile and scope identity, monotonic observation time, and advert origin
and fingerprint for duplicate suppression. The last two prevent repeated copies
of the same advert from refreshing expiry or replacing its first retained path.
SNR, success/failure ranking, probation, saved UTC and storage bookkeeping are
removed. New adverts replace the previous hint; an identical excluded hint
cannot provide an alternative retry. Fresh-discovery/trace retries are unchanged.
The oldest-observed repeater is evicted when full, and hints expire after 24
hours using rollover-safe millis arithmetic.

The cache starts empty after reboot and clears on local identity changes. There
is no cache filesystem dependency, startup load, periodic save, backup or flash
write. Legacy /trace_adverts files are ignored; this path does not open or
remove them. Fallback becomes available as new verified repeater adverts arrive
passively. Until then, normal fresh path discovery and tracing remain available.
No completed measurement is restored as a fresh result. The separate durable
request clock remains persisted to preserve MeshCore replay protection.

The device boot freeze was reported resolved with the reduced cache in
v2.2.54. v2.2.55 removes the temporary setup/UI/pet progress markers, serial
breadcrumbs and BLE diagnostic wrapper. The normal loading/version splash
returns; the existing reset-fault reporting and on-demand diagnostics remain.
The smaller cache and the MeshCore baseline boundary are retained. Runtime
heap/stack peaks still require device measurement before memory safety sign-off.

## Shared durable request clock

`RequestRTC` delegates ordinary wall time to the physical RTC and supplies one
durable unique-time sequence to MeshCore. Login, telemetry, binary request,
admin CLI and companion-originated unique-time producers share it. A preflight
reserves a value before management producers start. Replicated identity-bound
CRC records reserve 256 values, with both writes and read-back required before
emission. Reboot skips the entire possibly emitted range. Rollback, exhaustion
and failed/corrupt reservations fail safely; advert-only trace remains possible.

Each identity has separate journal names, with the full key checked inside each
record; prefix collisions fail closed. Explicit identity import rebinds the clock,
clears message timestamp mappings and reinitializes trace/cache ownership.
Restoring a previous identity preserves its own earlier replay floor. Matching
legacy sensor journals are included conservatively when establishing that floor.
Plain repeater-message sends use the sequence too; their bounded retry mapping
preserves logical wire timestamps and refuses an evicted retry rather than
re-executing an old command as a new one. Ordinary chat dedup timestamps retain
their established behaviour. Peer responses never set wall time.

The only new shared-core change is the virtual qualifier on
`RTCClock::getCurrentTimeUnique()`. Its default body remains unchanged. The
baseline checker reconstructs exactly this one-token patch from the pinned
baseline and checks its digest. Existing Dispatcher extensions stay pinned to
the earlier reviewed sensor source; forbidden core shadows remain forbidden.

Public-login policy uses a new tagged preference record and reuses a retired
Zen byte without changing the existing tail shape. Legacy files default to On,
regardless of the retired byte's old content. On/Off round-trips through the
normal Zen store and settings lifecycle.

## Validation

```sh
python3 -B tools/check_zen_baseline.py
python3 -B -m unittest discover -s test -p 'test_zen_baseline_extensions.py'
pio test -e native -e native_kiss_modem -e native_trace_transport
pio run -e WioTrackerL1_companion_radio_ble -e WioTrackerL1_Zen_OLED -e WioTrackerL1_Zen_E-INK
graphify update .
```

Native coverage drives the actual control/session/probe, timestamp journal,
RAM-only advert cache, message sequence and preference policies. The dedicated transport
environment uses real Dispatcher and StaticPoolPacketManager, including busy
queue → TX completion → full reply window and cancellation before expired work
can start. Existing suites cover display holds, both display layouts, remote
owner exclusions, cancellation, late replies, two directional SNR results and
Re-Trace.

Final v2.2.49 validation passed all **626 native cases** across the three native
environments, all **six baseline-extension checker tests**, the baseline/shadow
checks and `git diff --check`. Final OLED and E-ink builds both passed. The
standard Wio BLE companion also built successfully with the default RTC hook.
Both versioned `.2.2.49.uf2` files passed block magic, sequence/count, payload,
nRF52840-family and embedded-version checks. The knowledge graph was updated.
No serial test devices were connected; physical acceptance remains pending.

## Physical acceptance still required

Software/network models do not establish over-air interoperability or actual
flash-write behaviour. No device is flashed by these validation commands. On
both OLED and E-ink, verify:

1. Fresh repeater client state: guest introduction and login-return-path trace.
2. Protected guest and Guest Off: ordinary discovery and signed-advert fallback.
3. Direct and multi-hop routes, directional SNR ordering and TX-start RTT.
4. Congestion, queue delays, exhausted retries, full receive windows and RF counts.
5. Different return scope, radio changes, Back, Re-Trace and simultaneous app use.
6. Device/repeater reboot, RTC rollback, identity import/restore and storage faults.
7. Display readability and holding through all stages, including manual blanking.
