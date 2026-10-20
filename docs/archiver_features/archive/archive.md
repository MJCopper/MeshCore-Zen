# Archive Behaviour

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/archiver/README.md)

## Message capture

The firmware listens to MeshCore's built-in Public channel and accepts valid
Public text payloads. Each payload contains a timestamp and the channel's
claimed `name: message` text. Public packets do not provide an authenticated
sender identity, so the room envelope identifies the archive server while the
original claimed name remains in the message body.

Binary group data, unsupported text types, empty messages and malformed
payloads are ignored.

## RAM history

The newest 256 messages are stored in a circular RAM buffer. Once full, each new
message replaces the oldest entry. No archived message is written to flash, and
the entire history is cleared by reboot, reset or power loss.

## Synchronization

A successful room login captures a fixed snapshot boundary. Only messages
already stored when that login begins are eligible for that session. Messages
received afterward remain silent until another login.

The companion's `sync_since` timestamp is respected. Messages already reported
by the companion are not sent again.

| Detected connection | Maximum eligible history |
| ------------------- | ------------------------ |
| Direct | 256 messages |
| One hop | 256 messages |
| Two or more hops | Newest 20 messages |

Flood logins contain an exact path-hash count. A direct-routed login has
consumed its forward path before reaching the server, so the archive uses its
stored reverse path when one is available. If no reverse path is known, the
protocol exposes no previous-hop count and the session is treated as direct.

Each archived message waits for its companion acknowledgement before the next
message is sent. A failed synchronization stops after three consecutive
delivery failures.

## Mesh traffic policy

The archive starts as a leaf node with forwarding disabled. It does not forward
Public packets or other traffic in this state, and receiving a Public message
does not generate an acknowledgement. A reboot restores the disabled-forwarding
archive policy if a remote administrator changes the setting during a session.
Archived messages are sent only in response to a companion-initiated room login.
