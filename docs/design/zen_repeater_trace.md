# Zen repeater trace implementation and verification

The user transaction is `zen::RepeaterTraceSession`, with the ported
`RepeaterTraceProbe` and `TraceTimingPolicy`. `TraceRoute` constructs
`via...,target,reverse(via)...`; the final self reception completes the loop.
`MyMesh::TraceAdapter` owns encoding/admission and calls existing MeshCore
routing APIs. `RepeaterTraceView` is presentation only.

`tools/zen_baseline_extensions.json` pins exactly `src/Dispatcher.h/.cpp` to the
reviewed sensor-branch commit. These add a default no-op TX-start observer and
safe queue-only lookup/cancellation. All other protected files remain checked
against MeshCore 1.18.0. `tools/check_zen_baseline.py` compares extension bytes
against both their pinned commit and SHA-256; the shadow check stays enabled.
There is no Dispatcher shadow or replacement forwarding implementation.

The trace owner participates in `RemoteNodeCoordinator`. A transient peer is
appended only when it is absent from saved contacts and the bounded search has
space; it supplies the shared secret and receives authenticated PATH data
without mutating contacts. Trace tags never occupy upstream pending fields.
Discovery tags use RTC uniqueness, not random trace identifiers, to preserve the
remote replay-protection clock. Matching late PATH replies are consumed without
installing a saved route or sending a reciprocal PATH.

Route candidates are current-operation observations only. Target changes clear
history to avoid cross-target prefix collisions. Configuration changes invalidate
history and active state. `TraceNameResolver` is separately tested for collisions
and UTF-8-safe truncation. Software RTT excludes queue wait through the actual
Dispatcher callback; physical timing still requires hardware measurement.

## Verification commands

```sh
python3 tools/check_zen_baseline.py
python3 -B -m unittest discover -s test -p 'test_zen_baseline_extensions.py'
pio test -e native -e native_kiss_modem -e native_trace_transport
pio run -e WioTrackerL1_Zen_OLED -e WioTrackerL1_Zen_E-INK
graphify update .
```

The native transaction suite covers discovery -> full loop -> signed outward
and reverse SNR/RTT result, direct neighbours, two-byte hashes, bounded discovery
and trace retries, alternate candidates, exact-route history, late results,
final grace, partial/malformed replies, queue/start failure, cancellation,
configuration and target changes, RTC tag separation and timer rollover.
Rendering tests scroll progress/results on OLED and portrait/landscape E-ink.
The transport-specific environment compiles the actual Dispatcher and packet
pool to verify TX-start timing, busy-channel delay, failure ownership and
queue-only cancellation. Baseline-checker tests verify exact extension pinning
and reject drift. Other native suites cover existing remote owner exclusion,
Ping-adjacent UI and storage/protocol policies.

The completed automated run passed all 559 native test cases across the three
environments and all five baseline-extension checker tests. The standard BLE
companion build also passed, checking the shared Dispatcher hooks with the
non-Zen consumer. Both Zen display targets produce versioned v2.2.46 UF2 files.

## Physical acceptance still required

Software tests use deterministic radios and packet sequences. They are not an
over-the-air acceptance claim. On a designated test mesh, verify:

1. Direct-neighbour trace and a multi-hop repeater loop.
2. Directed SNR ordering against per-node logs/captures and signed zero values.
3. TX-start/receive timing against a capture, including display refresh latency.
4. Unreachable/permission-denied and congested targets, with no more than two
   discovery and two trace originating submissions.
5. Back cancellation, configuration/power changes and simultaneous phone app use.
6. Legibility and scrolling on both physical display types.
