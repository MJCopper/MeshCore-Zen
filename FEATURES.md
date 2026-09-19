# Zen features

Zen v1.32.107 extends MeshCore v1.17.1 companion firmware for the Wio Tracker L1.
The features below are provided by Zen in addition to the standard MeshCore
companion protocol and radio operation.

| Area | Behaviour |
| ---- | --------- |
| Standalone UI | Clock-first home carousel and on-device Messages, Favourites, GPS, Advert, Bluetooth, Radio, Sensors, Tools and Settings pages |
| Messaging | Direct, channel and room lists with full transcripts, sender replies, unread tracking and latest-unread shortcut |
| Delivery | Route-aware direct-message retries, path-to-flood fallback, live delivery markers, channel echo counts and manual resend |
| Quick Replies | Ten built-in and five editable replies, with time, location and sensor placeholders |
| Text entry | EN-US ABC and predictive T9 with a 4,000-word Australianised dictionary, compact previous-word ranking and command-prioritised console completion |
| Emoji | Monochrome common emoji, diamond fallback and an insertable chat emoji picker |
| CardKB | Startup detection, direct input, automatic compact editor and screen-aware polling through Grove I2C |
| Clock and time | Permanent first page, unread count, selectable city daylight-saving rules or fixed UTC offset, 12/24-hour display and background GPS/companion time synchronisation ([guide](./docs/zen_features/timezone/timezone.md)) |
| GPS | Session-only power toggle, cached-fix age, Continuous, Adaptive or timed polling, temporary clock synchronisation and a 32-position scrolling course-over-ground tape ([guide](./docs/zen_features/gps/gps.md)) |
| Sensors | Sensor-node carousel with authenticated, on-demand multi-channel telemetry and line scrolling |
| Favourites | Four editable contact shortcuts with unread badges, plus starred contact, room and channel ordering |
| Child Mode | Parent PIN, favourite-only direct messages, optional favourite rooms/private channels and disabled companion access while locked |
| Notifications | On/Off/Auto presentation, optional five-second message screen wake and independent sound choices; unread messages are retained |
| Quiet Time | Daily local-time sound suppression while permitted visual alerts, message reception and unread state continue |
| Notifications | Per-contact/channel tones, eight built-in melodies, two editable chromatic tones and five-second notification wake |
| Advert | Manual and Off/1 h/3 h/6 h automatic adverts with shared GPS privacy control and timed status icon |
| Repeater Mode | Companion repeating with current radio settings, MeshCore repeater timing, Yield x2 and duplicate suppression |
| Node tools | Repeater discovery, filtered Node List, inspection, ping, read-only path details and saved-node management |
| Node administration | ACL/password login, route retries, remote settings, routing, radio, console and verified changes for repeaters, rooms and sensors |
| Battery | Li-ion percentage curve, learned remaining-runtime estimate, hourly low-battery warning and 3.3 V shutdown protection |
| Low Power | Automatic 5% radio/GPS/Bluetooth suspension with a ten-minute emergency communications override |
| Diagnostics | Live radio/mesh counters, battery, storage and flood-scope status, system details, font viewer and a 16-entry RAM-only warning/error log |
| Signal indicator | Three-bar SF-relative repeater-link estimate from passive routed traffic, with authoritative 30-minute user-wake discovery ([guide](./docs/zen_features/signal_indicator/signal_indicator.md)) |
| Configuration | Save-on-exit for changed values, versioned Zen sidecar preferences and schema-based migrations |

Detailed guides are linked from [README.md](./README.md).
