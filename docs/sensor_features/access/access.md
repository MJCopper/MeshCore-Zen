# Access and Management

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/sensor/README.md)

The sensor uses MeshCore's encrypted remote-management protocol and ACL. Live
telemetry and management requests are handled separately from the unencrypted
Public-channel `!hillvue` interface.

## Administrator access

The build default administrator password is `password`. Change it during first
setup. A successful administrator login can configure the node, inspect status,
request telemetry and manage ACL entries.

The first successful administrator-password login adds the companion identity
to the ACL as an administrator. A known identity using a blank password retains
its assigned ACL role. An unknown identity using a blank password, or a session
using an incorrect password, receives read-only access with the app's limited
guest interface.

Remote text/CLI commands require administrator permission. Telemetry requests
are available to administrator and read-only guest sessions. Guest sessions
cannot change settings or issue management commands.

## Persistent and temporary state

The following data persists across reboot:

- Node identity and name.
- Administrator password and administrator ACL entries.
- Radio and advert configuration.

Sensor readings, air-quality calibration, command hashes, response-rate state
and any pending second reply parts remain in RAM and reset at boot. Read-only
guest sessions also remain in RAM; guest telemetry access does not write flash.

## Leaf-node policy

Packet forwarding is hard-coded Off for this target. Remote attempts to enable
repeat mode leave forwarding disabled, and status reports repeat mode as Off.
This prevents the sensor from becoming a mesh repeater.
