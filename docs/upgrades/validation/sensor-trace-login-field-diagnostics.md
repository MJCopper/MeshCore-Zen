# Trace guest-login field investigation — 25 October 2026

The later [guest-login timing update](sensor-trace-login-timing.md) includes
these diagnostics and supersedes this diagnostic-only OTA for testing.

The operator confirmed that 7188 runs MeshCore 1.18, permits blank guest login,
and accepts guest login from a companion beside the sensor. The original
Discovery Repair OTA still fails: public guest login at 53 seconds remaining,
waiting for an advert route at 33 seconds remaining, followed by
`Trace: guest login not acknowledged`. The field problem remains unresolved.

Compared the sensor's anonymous login against BaseChatMesh::sendLogin and the
repeater-v1.18.0 tag. The blank credential payload is the same four-byte timestamp
with cipher zero padding. Stock 1.18 accepts an unknown full identity with the
configured blank guest password. The stock reply uses the server timestamp,
zero response code and keepalive byte, admin flag, ACL permissions, nonce and
firmware level, matching the sensor parser. This does not prove over-air receipt.

Transport differences remain: the companion can use a saved direct path and
selected/default send scope; the sensor floods and can inherit the public
command scope when it has no explicit default scope. The sensor also applies
fixed stage leases encompassing queue, transmission and response. Neither
observation proves the cause on this particular deployment. No credential,
scope, retry or timing changes were made in this diagnostic follow-up.

Added private/local `get trace.diag`, `get trace.peer` and
`get trace.control 0|1|2`. The readouts retain the last transaction until a new
trace, perform no I/O writes or sends, and fit the existing 160-byte CLI reply
contract. Candidate packet counts use short hashes and are not authentication.
Authenticated handler counts can include multiple calls for a frame. Accepted
login replies include unexpected-role responses; inspect role and final error.
A successful local TX is not proof the repeater received it. No physical radio
capture, flashing, or hardware acceptance was performed here.

Validation: all 362 native_sensor tests passed, including two new diagnostics
regressions. Xiao_nrf52_bme680_sensor firmware/DFU/UF2 build, strict MkDocs,
sensor-boundary check and git diff --check passed. ZIP CRC, manifest application
BIN/DAT presence validated. Prior named repair archive retained unchanged.

Diagnostic OTA: `.pio/build/Xiao_nrf52_bme680_sensor/MeshCore_BME680_Sensor_Trace_Login_Diagnostics_OTA_2026-10-25.zip`.
SHA256: `5514bc846258643f44816786792871bf91a461cbce87af21bce558090b4fa4cb`.

After reproducing once, collect `get trace.diag`, `get trace.control 1`,
`get trace.peer` before starting another trace. If the login exchange is locally
complete but no candidate reply is seen, a capture of the sensor and companion
exchanges or repeater-side reception is still required to separate forward-path
failure, repeater processing and return-path failure.
