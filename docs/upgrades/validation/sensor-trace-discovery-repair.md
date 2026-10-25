# Trace first-contact repair and persistent advert fallback

Date: 2026-10-25. Implementation in the current working tree on `meshcore/sensor`
above `5add24af`. No device has been flashed. Physical acceptance is pending.

## Implemented

- Separate initial discovery, public guest login, fresh final discovery and
  passive advert wait phases in the existing `TraceDiscovery` owner. This is
  the planned first-contact/login separation implemented with a single current
  control-packet owner rather than overlapping controllers.
- One empty public guest attempt by default, configurable locally/privately by
  `get trace.login` / `set trace.login public|off`. No stored/admin password use.
  Protected guest access, absent replies or unexpected admin grants proceed to
  advert fallback. No silent denial is presented as proven permission failure.
- Stock 1.18 login uses the target timestamp and protocol fields. Its route is
  not committed as discovery. Fresh authenticated request/tag matching is still
  required. Old login-shaped replies are absorbed separately from discovery.
- Six/eight/six-second control leases and a 25-second absolute control cutoff;
  fixed 55-second work cutoff and 60-second transaction deadline; shared total
  two probes, one login and two ordinary discovery requests. Normal report and
  averaged-SNR output are preserved.
- Durable identity-bound timestamp reservations, 256 values per block, two
  replicated CRC records and read-back validation before use. Reboot skips
  beyond reserved values; rollback, corrupt storage and write failure cannot
  reuse uncommitted values. No peer response sets the local wall clock.
- Verified flooded advert route collection, exact reverse/target/return loop
  construction for both widths, compact 48-by-two candidates bounded to 10 KiB,
  scope/radio context and 24-hour known-age expiry. Restored unknown-age hints
  are probationary and must pass a new TRACE. Alternative routes share the
  existing probe budget.
- Separate route-cache sidecar with CRC/identity/version checks, backup recovery,
  30-second coalescing and ten-minute save throttling. Only populated entries
  are serialized; checkpoint replacement keeps at most two route files, with
  both full caches below 16,000 bytes combined. Existing name persistence
  and management ACL schemas are unchanged. Temporary-radio observations are
  not persisted and live candidates are invalidated on configuration changes.
- Explicit outgoing control scope/default/hash-width selection independent of
  frozen public reply context. Authenticated full-target/fresh-tag PATH replies
  may return on a different scope; differences are recorded.
- Bounded diagnostics: control queue/start/completion evidence and tags, login
  role, clock health/regression, path rejection and advert/probation provenance.
  Core pre-decryption failures still require core debug or target-side capture;
  no visibility is claimed where no service callback exists.

The production repeater login and replay-acceptance logic is shared with the
native target through generic `RepeaterLogin.h`. The stock response encoder is
also shared. Production's platform-specific sensor/voltage acquisition remains
outside the native target; actual interoperability still needs hardware evidence.
The baseline sensor runtime exposes only a protected default-scope value getter;
RTC helpers expose hardware detection. No bot policy was added to core Mesh or
Dispatcher.

## Behavioural evidence

New tests exercise:

- Fresh remote ACL with no sensor entry: initial silence, public guest login,
  fresh-tag discovery and actual multi-hop TRACE success. The remote guest state
  is then cleared to model a repeater reboot; the next operation re-establishes
  it successfully.
- Protected guest login and unavailable authenticated discovery with a genuine
  signature-verified advert: login-free fallback still succeeds.
- Retained filesystem/identity and service recreation without a new advert:
  persisted hints are loaded as probationary and verified with a new TRACE.
- Both widths and every route length through ten repeaters, exact outward and
  return bytes; direct-target hint, invalid signature rejection, alternative
  route retry, snapshot context and no old result replay.
- Production-faithful return-scope fallback, including an unknown incoming
  transport scope processed locally by a target and a default-scope reply. It
  is accepted while final public context remains frozen.
- Clock reboot/rollback/forward jump, both-replica commit, missing/corrupt/foreign
  journal, failed second replica, exhaustion, policy save failure, cache save
  failure, corrupt-primary backup recovery and full 48-entry repeated checkpoints
  without a third temporary cache file.
- Login response padding, missing/wrong identity/data, unexpected role, remote
  clock regression and fresh ordinary tag ownership; queued-login cancellation
  and the existing expiry, Dispatcher ownership and packet-pool regressions.

Existing exact-phase fixtures isolate acknowledgement and explicitly select
public-login off where that test concerns only ordinary discovery. Their extra
radio poll is restricted to initial isolated acknowledgement acceptance, so it
cannot alter unrelated multipart queue-boundary timing. Filesystem assertions
that previously prohibited every write now specifically prohibit management
contact writes: durable timestamp reservations are intentional new trace writes.

Hardware-clock presence supplies the RTC quality flag; this is not an independent
attestation of absolute calendar correctness. Previously deployed remote replay
floors ahead of locally recoverable time cannot be inferred from silence. Such
migration cases may still require trusted time recovery; advert fallback does
not require authenticated request timestamps.

## Validation gates

All final software gates passed:

| Gate | Result |
| --- | --- |
| Native, sensor and KISS suites | 467/467 passed |
| Sensor AddressSanitizer suite | 360/360 passed |
| Five firmware environments, buildprog and UF2 | All passed |
| Five DFU archives, CRC and root application manifest/bin/dat | Passed |
| Sensor boundaries and whitespace checks | Passed |
| Strict MkDocs build | Passed; existing FAQ anchor INFO notice remains |

The final sensor uses 45,308 bytes of static RAM and 372,388 bytes of flash.
Against the preceding named-trace build (37,364 RAM / 360,532 flash), this adds
7,944 RAM and 11,856 flash bytes. Compact route data uses 64-byte hints and
160-byte target entries: a one-target checkpoint is 204 bytes; two full
48-target checkpoints total 15,448 bytes before filesystem overhead. The XIAO
framework reserves 28 KiB for InternalFS, so writing populated entries and
avoiding a third route checkpoint are material safeguards. Space exhaustion
still keeps RAM candidates and the last valid checkpoint rather than discarding
management files. A failed timestamp reservation falls back without reusing tags.

The environments were `Xiao_nrf52_bme680_sensor`, `Xiao_nrf52_sensor_baseline`,
`Xiao_nrf52_companion_radio_usb`, `Xiao_nrf52_repeater`, and
`Xiao_nrf52_room_server`. [Check summary and artifact hashes](sensor-trace-discovery-repair-checks.txt)
record the final outputs. The sensor OTA is
`.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Discovery_Repair_OTA_2026-10-25.zip`.
It is a direct DFU archive, copied from verified `firmware.zip`, with SHA256
`67218cad1e9773a323722d53bd5cad8565c2588fc313fc65f9198b5f97d7e59f`.

After installation, `get trace.login` in the private/local CLI should return
`> public` unless the operator previously selected off. Old named-device records
contain no routes: allow verified flooded adverts to warm the new cache before
expecting advert-only fallback. The package has not been installed on hardware
as part of this work.

```sh
pio test -e native -e native_sensor -e native_kiss_modem
ASAN_OPTIONS=detect_leaks=0 pio test -c /tmp/meshcore-scope-asan.ini -e native_sensor_asan
pio run -e Xiao_nrf52_bme680_sensor -e Xiao_nrf52_sensor_baseline \
  -e Xiao_nrf52_companion_radio_usb -e Xiao_nrf52_repeater \
  -e Xiao_nrf52_room_server -t buildprog -t create_uf2
bash bin/check-sensor-boundaries.sh
git diff --check
mkdocs build --strict
```

The existing isolated ASan configuration uses the extracted host runtime and
leak checking disabled, as documented in prior validation. No sanitizer runtime
is included in firmware.

## Physical acceptance still required

No over-air exchange with 7188 was captured and no device was flashed. Before
calling the original field issue resolved, test fresh sensor identity against
stock 1.18, known registration, protected guest, advert-only fallback, persistence
without an external RTC, remote reboot, scope fallback, a broken first advert
route, and cancellation/restart around the 25/55/60-second boundaries. Compare
one working Zen exchange and one sensor exchange with full identities, packet
order, scopes and tags. Record actual failures instead of inferring permissions
or replay rejection from an absent response.
