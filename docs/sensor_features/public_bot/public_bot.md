# Public-channel Commands

[Back to README](../../../README.md)

The node listens on MeshCore's built-in Public channel for messages beginning
with the exact command word `!hillvue`. Matching is case-insensitive.

## Commands

| Message | Response |
| ------- | -------- |
| `!hillvue` | All measurements |
| `!hillvue all` | All measurements |
| `!hillvue t` | Temperature |
| `!hillvue h` | Humidity |
| `!hillvue p` | Pressure |
| `!hillvue a` | Air quality |
| `!hillvue v` | XIAO battery voltage |
| `!hillvue ping` | `Pong` reachability reply |
| `!hillvue path` | Repeater path taken by this request |

An option is selected by its first letter, so forms such as `temp`,
`Temperature` and `T` all select temperature. Likewise, `v`, `volt` and
`voltage` select voltage. Multiple space-separated options
can be combined:

```text
!hillvue t air
!hillvue Temperature Humidity
!hillvue p h a
!hillvue voltage t
```

`!hillvue ping` is standalone and cannot be combined with measurements. It
uses the same traffic limits as other commands and does not require sensor data.
`Pong` confirms that the node received the command, but does not measure mesh
round-trip time in milliseconds.

`!hillvue path` is also standalone. It reports the repeaters traversed by the
request on its way to the sensor, using names from verified repeater adverts
where available. Unknown or ambiguous names are shown as path hashes. This is
the first received route, not necessarily the best route or the return path.
The sensor retains up to 48 repeater names in its internal flash filesystem;
unchanged adverts do not cause writes. The reply appears on the Public channel,
not as a private message to the requester.

Each selected value appears on its own line in a single Public-channel reply;
`!hillvue` and `!hillvue all` include voltage. A voltage-only request works
even when the BME680 is not ready. Before the first voltage sample, or for
requested BME680 readings that are not ready, the reply says
`Sensor data is not ready`.
During air-quality calibration it reports, for example:

```text
Air Quality: Warming Up (4/10)
```

## Mesh traffic controls

- Commands use the latest cached reading and never trigger an extra sample.
- The node sends at most four bot responses in any rolling 60-second period,
  shared across all users and commands.
- Hashes for the eight most recent accepted commands are retained in RAM to
  suppress duplicate packet delivery.
- The bot does not publish on a schedule and does not respond to its own output.
- A response is sent once as a normal MeshCore flood; the node does not repeat
  other mesh traffic.

Unknown command options are ignored without a response.
