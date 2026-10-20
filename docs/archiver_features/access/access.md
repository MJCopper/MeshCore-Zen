# Access and Management

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/meshcore/archiver/README.md)

The archive uses MeshCore's encrypted room-server login and ACL protocol. Room
access and Public-channel reception are independent.

## Login roles

| Login | Companion interface | Server behaviour |
| ----- | ------------------- | ---------------- |
| Administrator password | Administrator and remote management | Management allowed; room posts rejected |
| Guest password | Limited Guest | Room posts rejected |
| Blank or incorrect password | Limited Guest | Room posts rejected |

The clean-build administrator password is `password`. Change it during initial
setup. Saved credentials take precedence over this compile-time default.

The archive forces read-only access to remain available. A previously stored
administrator who later logs in without the administrator password is treated
as a Guest for that session.

## Administration

An authenticated administrator can use a compatible MeshCore companion's
remote-management interface. USB serial commands remain available for initial
configuration and recovery.

Archive policy is fixed independently of the ACL:

- Room posts are rejected for every user, including administrators.
- Packet forwarding is forced Off at startup.
- Captured Public messages cannot be edited through the room interface.

## Persistent state

The following data persists across reboot:

- Node identity and name.
- Administrator password and ACL.
- Radio, region and advert configuration.

Message history, active login snapshots and delivery state exist only in RAM.
