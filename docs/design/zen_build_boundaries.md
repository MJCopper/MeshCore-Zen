# Zen feature and build boundaries

[Back to documentation](../index.md)

Zen extends the MeshCore companion firmware without making Zen the default for
every Wio Tracker build. Only `WioTrackerL1_Zen_OLED` and
`WioTrackerL1_Zen_E-INK` define `FIRMWARE_ZEN_BUILD`.

`zen/ZenFeatures.h` owns product feature selection. `zen/ZenCapabilities.h`
derives hardware availability from the board configuration. A hardware macro
can expose a capability, but it cannot enable a Zen feature in a baseline
MeshCore target.

All reusable Zen policy, persistence, messaging, notification, power,
text-entry, remote-operation and time/location modules live under
`examples/companion_radio/zen-overlay/app/zen` and use the `zen` namespace. Hardware-specific
pin, display, bus and peripheral integration remains in the Wio variant and UI
integration layers.

The Zen companion entry point and UI live under
`examples/companion_radio/zen-overlay/app`. Reviewed Wio integration code lives
beside it under `zen-overlay/src` and `zen-overlay/variants`. Zen build include
paths select explicitly named `Zen*` extensions; there are no global forwarding
headers and every other target resolves the unmodified MeshCore implementation.

`ZenUIScreen` derives from MeshCore `UIScreen` and adds Zen's screen lifecycle.
`ZenDisplayDriver` derives from MeshCore `DisplayDriver`; `ZenSH1106Display` and
`ZenGxEPDDisplay` provide the two product renderers. Input and ringtone services
are likewise named `ZenMomentaryButton` and `ZenBuzzer`, so these features do not
masquerade as replacements for baseline classes. `ZenWioTrackerL1Board` derives
from the baseline Wio board and owns only the stable battery-divider behaviour.

Packet dispatch, mesh forwarding, radio drivers, BLE transport, USB transport,
MeshCore `DataStore`, MeshCore `NodePrefs`, the generic sensor contract, RTC discovery and the Wio Arduino variant are
compiled directly from the MeshCore baseline. Zen observes connection state
through the registered baseline interfaces, tracks RTC writes with a decorator,
and applies GPS policy through `zen::GpsService`. None of these services replace
the baseline RF or companion-protocol lifecycle.

The remaining same-path integration files are intentionally limited and listed in
`zen-overlay/build/shadow_manifest.json`, together with the hash of the upstream
file reviewed for that integration. They comprise the companion application
entry points, the GPS provider/manager and Wio target composition. MeshCore does
not currently expose narrower extension seams for these responsibilities.
Adding an unlisted same-path file, restoring a forbidden core shadow, or changing
an upstream counterpart without review fails the boundary check.

`tools/check_zen_baseline.py` verifies the complete shared core, public include
tree, official Wio variants and complete official companion application against
the pinned MeshCore 1.17.1 commit. It also enforces the reviewed integration
manifest and requires Zen to compile baseline `DataStore` directly. Run it
whenever the overlay boundary changes. Update the pinned commit and reviewed
hashes only as part of an intentional upstream baseline update.

Navigation, Remote Bot, Clock Tools, general-purpose GPIO and the removed
location-sharing tools have no feature switches or runtime entry points. Their
old preference fields remain reserved only where required to preserve the
established binary preference layout.

Earlier Zen/Solo preference layouts are intentionally not imported. MeshCore
backup/restore remains responsible for baseline data, while `ZenPrefsCodec`
stores a new independent Zen-only record on external flash.

Changes to these boundaries must verify the native suite, the standard Wio
companion boundary where dependencies are available, and both Zen display
targets. Release artifacts remain scoped to the two Zen environments.
