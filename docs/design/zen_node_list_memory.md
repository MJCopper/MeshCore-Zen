# Node List memory consolidation — v2.2.61

This records the v2.2.61 implementation. The subsequent
[infrastructure-only Node List](zen_infrastructure_node_list.md) removes
companions and untyped advert rows from the view while retaining its compact
reference storage and shared data APIs.

Zen's Node List is a projection over MeshCore-owned contacts, recent adverts
and discovery results. It does not own another contact database. This change
removes only the Node List's duplicated buffers; the trace fallback cache,
radio/discovery policy, phone protocol and flash formats are unchanged.

## Storage and identity

`zen/NodeListModel.h` holds 32 references of 16 bytes each, a single selected
record and small bookkeeping fields (616 bytes on ARM). Each reference contains
its source, index, stale-reference signature and sorting value. Names, full keys,
GPS coordinates, paths and signal fields are not duplicated for every row.

`zen/NodeListAccess.h` implements bounded, host-testable reads used by MyMesh.
Contacts start after MeshCore's eight anonymous slots. Previously the screen
used a count excluding those slots with indices including them, omitting the
last contacts and potentially displaying anonymous slots. Path Details now uses
the same correct physical-slot bounds.

Advert summaries include name, seven-byte identity prefix and receive time,
without copying the 64-byte path. Recent ordering is calculated without sorting
the shared cache. The existing bulk getter and phone advert-path command remain
available and unchanged. Only the eight most recent adverts are considered,
matching the previous display admission policy.

Discovery reads one existing result at a time, preserving local RSSI, local SNR
and remote SNR. Current contact membership is resolved when read rather than
using the result's original known/new flag. A small generation counter increments
when a non-silent discovery request resets the shared results; silent signal
monitoring leaves it unchanged.

The selected record holds its complete identity across sorting, menus, details,
Admin return and confirmations. Actions revalidate that identity against its
source. A new discovery generation invalidates a scan selection. Contact
compaction, cache sorting or eviction invalidates affected row references and
causes a rebuild before screen rendering/input. Removing a selected node closes
its menu/detail/confirmation; a replacement cannot inherit its pending action.
Admin resolves by full public key rather than using a retained contact index.
Row signatures only detect stale references and are never action identities.
Advert-only entries retain their informational restrictions.

No heap allocation is introduced. Sorting rearranges references, using cached
ordering values so it does not repeatedly calculate geographic distances.
The 32-row cap, contact-first admission, filters, favourites, distance/time
ordering, discovery RSSI ordering and refresh intervals remain unchanged.
Sorting still applies to the admitted rows, not all 350 contacts.

## Measured ARM RAM

Compared with the v2.2.60 ELF files:

| Allocation | Before | After |
| --- | ---: | ---: |
| NearbyScreen instance | 3,684 | 1,356 |
| Permanent discovery scratch | 1,216 | 0 |
| Permanent advert scratch | 864 | 0 |
| Discovery generation including MyMesh alignment | 0 | 8 |
| Net saving | | **4,400 bytes per target** |

| Target | Static RAM before | Static RAM after | Heap capacity after |
| --- | ---: | ---: | ---: |
| OLED | 197,928 | 193,528 | 41,992 |
| E-INK | 202,568 | 198,168 | 37,352 |

Heap capacity is the linker region before runtime allocations, not live free
heap. Runtime FreeRTOS/BLE allocations still consume part of it.

MeshCore contacts remain 65,872 bytes, the general advert cache 1,728 bytes,
discovery results 1,216 bytes and discovery deduplication 256 bytes. The separate
Zen trace fallback cache remains 672 bytes, RAM-only, with its identity, scope,
radio profile, duplicate and expiry safeguards intact.

GCC `-fstack-usage` builds of MyMesh.cpp and UITask.cpp show bounded static
frames: model rebuild 248 bytes, model validation 120 bytes, row read 32 bytes,
contact adapter 208 bytes, advert adapter 128 bytes, screen render 264 bytes and
screen input 24 bytes. These are individual frames, not maximum cumulative
stack usage. No removed array was moved onto the stack. On-device stack
high-water verification remains part of hardware acceptance.

## Automated verification

The native Node List suite exercises the production screen, model and storage
adapters with fake storage/radio/UI task boundaries. It covers:

- Empty and full contact tables, reserved slots and the last real contact.
- The 32-row admission limit, all filters, favourites, distance/time sorting,
  unknown GPS and invalid timestamps.
- Recent-advert ordering, ties, deduplication, eviction, bounded summaries,
  terminated names and unchanged shared path/cache data.
- Discovery signals, live contact membership, stronger arrivals and scan reset.
- Selection after compaction/reordering; same-prefix/full-key replacement.
- Ping, Trace, Path Details, Admin, Favourite, Pin and Add targets.
- Delete confirmation default/cancel, reorder and target replacement/removal.
- OLED 128x64 at 6x9 and landscape E-INK 250x122 at 12x18: list, detail,
  menu, empty and scrolling bounds.
- No allocations by model/storage reads or screen refresh/navigation.

Native builds use `-funsigned-char` to match ARM's key-code semantics. Codes
above 0x7f otherwise fail comparisons on the host, preventing meaningful screen
input tests.

Verification completed: **666 native cases** (including 19 Node List cases),
**17 Python cases** (baseline extensions and preset generator), baseline/shadow
checks, both target builds and versioned UF2 structure/version checks passed.
ELF symbols confirm both removed scratch allocations are absent. ARM debug
layouts confirm the model and four shared/trace cache sizes listed above.

Commands:

```sh
pio test -e native
python -m unittest test/test_zen_baseline_extensions.py test/test_zen_radio_presets.py
python tools/check_zen_baseline.py
pio run -e WioTrackerL1_Zen_OLED -e WioTrackerL1_Zen_E-INK
```

Inspect the target ELF symbol table to confirm `nearby_instance` is 1,356 bytes
and the former `NearbyScreen::refreshScan()::dr` and
`NearbyScreen::mergeRecentlyHeard()::heard` allocations are absent. Compare
`__HeapBase - __data_start__` against the table above. Static assertions bound
the model/reference sizes, and allocation-counter tests protect the read paths.

## Hardware acceptance — pending

No OLED or E-INK board was connected for this implementation. Host rendering
and successful target compilation do not establish physical RF/BLE/display
behaviour. On both boards:

1. Boot, browse every filter, sort, scroll and open details; return from Admin.
2. Discover/rescan, add a node and exercise every applicable node action.
3. Receive adverts/results and modify/delete contacts through the phone while
   browsing, including with a menu, detail or delete confirmation open.
4. Verify a removed/replaced node never transfers a pending action to another.
5. Complete Ping and Trace, inspect Path Details and confirm phone advert-path
   requests continue working.
6. Record free heap and stack high-water before/after repeated browsing/scans;
   verify no progressive loss and sufficient runtime margin.
7. Check screen timeout/activity behaviour and both display orientations visually.

An exploratory fixed-font portrait header check found existing tab-label
clipping at 122px width. Shared TabBar layout changes are outside this buffer
consolidation; portrait visual acceptance should include that existing issue.
