# Average bidirectional trace SNR validation

Date: 24 October 2026 (Australia/Sydney).
Working tree based on `99dc1448`, following the
[scope implementation](sensor-trace-scope.md).

## Behaviour

Each outward repeater is listed once, in order from the sensor towards the
requester. Its value averages the readings for both directions of its link to
its preceding node. The first link uses the sensor's final receive reading as
its return sample. The requester link is not traversed by the trace.

For `N` repeaters and zero-based outward index `i`, the pair is raw sample `i`
and raw sample `2*N - 1 - i`. Both quarter-dB bytes are converted to signed
integers before summing; division by eight produces dB without truncating the
possible eighth-dB average. Formatting rounds once, to one decimal place.

The header now reads `Trace: <elapsed> ms RTT (avg SNR)`. Raw directional samples
remain intact in diagnostics. Retry success uses its own route and samples;
late first-attempt fallback uses the original route and both original
readings. RTT, packet format, scope, channel, timeout, retry and bounded
multipart delivery policies are unchanged. Existing report size/truncation
limits still apply; the label does not add any radio probes.

Example checked through real forwarding, for both hash widths:

| Listed repeater | Outward SNR | Return SNR | Displayed SNR |
| --- | --- | --- | --- |
| First from sensor | -6 dB | -2 dB | -4.0 dB |
| Second | +2 dB | 0 dB | +1.0 dB |
| Third | -10 dB | +2 dB | -4.0 dB |

This is the arithmetic mean of two reported dB values, not a time average or
weakest-direction score. It can conceal directional asymmetry.

## Automated checks

| Check | Result |
| --- | --- |
| `pio test -e native -e native_sensor -e native_kiss_modem` | 274/274 passed |
| Sensor AddressSanitizer environment | 168/168 passed; no findings |
| `bash bin/check-sensor-boundaries.sh` | Passed |
| `git diff --check` | Passed |
| `mkdocs build --strict` | Passed |
| `Xiao_nrf52_bme680_sensor`: compile/DFU and UF2 conversion | Passed |
| `Xiao_nrf52_sensor_baseline`: compile/DFU and UF2 conversion | Passed |

The new SNR suite adds 26 cases: every length from one through ten repeaters
at both hash widths, explicit three-link pairing/order, one-link sensor RX,
signed extrema and overflow, mixed signs and eighth-dB precision, retry
precedence and different-route fallback. The packet-medium suite adds one case
covering actual outward and return forwarding with independently configured
directional SNRs, both hash widths and exclusion of the requester link. Existing
scope/multipart and ten-repeater report tests continue to pass.

ASan uses the temporary matching runtime and isolated build configuration
[documented for the scope validation](sensor-trace-scope.md#automated-validation):

```sh
ASAN_OPTIONS=detect_leaks=0 pio test \
  -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan
```

No repository sanitizer flags were changed. The existing FAQ anchor notice from
MkDocs remains INFO-level; strict validation succeeds. Firmware builds retain
the project's existing warning flags. The change is confined to custom trace
reporting and its tests; the other firmware roles were already built by the
preceding scope validation and are unaffected by this rendering change.

## Resource measurements and sensor artifacts

Measured with Nordic nRF52 10.12.0 and ARM GCC 14.2.1, compared with the immediately
preceding scope build on the same toolchain:

| Resource | Before averaging | With averaging | Change |
| --- | --- | --- | --- |
| Static RAM | 36,852 bytes | 36,852 bytes | 0 |
| Flash | 343,572 bytes | 343,588 bytes | +16 bytes |

New artifacts supersede the binaries at the same paths in the scope report.
SHA-256 checksums:

- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.uf2` (687,616 bytes):
  `7ba44c9c87f467feee81bd9375f535fbd3b51e4df22ffdcd4118199babc73b16`
- `.pio/build/Xiao_nrf52_bme680_sensor/Xiao_nrf52_bme680_sensor.zip` (344,391 bytes):
  `63275fba9f04c036763279e6a4a313661b2a6d88aef377b7486bc36d20523f8d`
- `.pio/build/Xiao_nrf52_bme680_sensor/firmware.hex` (966,487 bytes):
  `49c2e4760d8c8dfe90598443ec83ff25e3d495f1d98d322307e396cb7f14f52e`

## Physical acceptance

Not run; no device flashed. Automated tests validate sample geometry and
arithmetic through real MeshCore code, not actual RF measurements. On hardware,
record both directions on controlled asymmetric links and compare each displayed
average with the paired raw samples. Include sensor RX, both hash widths, retry
and late fallback, node order, and a multipart report. RTT/SNR measurements and
UF2/OTA installation remain separate physical acceptance checks.
