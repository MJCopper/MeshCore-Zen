# Sensors

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

The Sensors home page lists saved sensor nodes. Select one to request telemetry.
Values appear by newest channel first and in their original order within each
channel; only the telemetry label and value are shown.

- **Up/Down** scrolls one value at a time.
- **Enter** refreshes the selected node.
- **Back** returns to the sensor list.

Requests first use the contact's current route and permissions. If no reply
arrives, Zen tries blank-password ACL login. If access is not confirmed, Enter
offers administrator-password login and one further request. There is no
background telemetry polling.

Sensors is enabled for new configurations and can be reordered under
**Settings › Home Pages**. It is hidden while Child Mode is locked.

