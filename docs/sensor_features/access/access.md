# Access and Management

[Back to README](../../../README.md)

The sensor uses MeshCore's encrypted remote-management protocol and ACL. Live
telemetry and management requests are handled separately from the unencrypted
Public-channel `!hillvue` interface.

## Administrator access

The build default administrator password is `password`. Change it during first
setup. A successful administrator login can configure the node, inspect status,
request telemetry and manage ACL entries.

The first successful password login adds the companion identity to the ACL as
an administrator. Later blank-password login is accepted only for an identity
already stored in that ACL, retaining its assigned role. An unknown identity
with a blank or incorrect password is rejected.

Remote text/CLI commands require administrator permission. Telemetry requests
are available to a successfully authenticated ACL session.

## Persistent and temporary state

The following data persists across reboot:

- Node identity and name.
- Administrator password and ACL.
- Radio and advert configuration.

Sensor readings, air-quality calibration, command hashes, response-rate state
and any pending second reply parts remain in RAM and reset at boot.

## Leaf-node policy

Packet forwarding is hard-coded Off for this target. Remote attempts to enable
repeat mode leave forwarding disabled, and status reports repeat mode as Off.
This prevents the sensor from becoming a mesh repeater.
