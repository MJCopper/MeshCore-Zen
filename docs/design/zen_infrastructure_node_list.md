# Infrastructure Node List — v2.2.62

The Node List manages repeaters, room servers and sensors. Companions remain in
MeshCore's normal contact/messaging interfaces with their existing favourite
flags and speed-dial pins. No contact migration, deletion or settings conversion
is performed by this change.

## Behaviour

The shared `NodeListModel::Filter` definition and label table provide exactly
five filters: **All → Fav → Rpt → Room → Snsr**, wrapping in both directions.
All includes only infrastructure, and Fav includes only starred infrastructure.
The screen aliases the model's values directly instead of maintaining a second
numerically matched enum.

Eligibility requires a full identity and an advert type of repeater, room or
sensor. It is checked for both stored and discovery records before admission
into the 32-reference table. A large number of companions cannot consume its
slots. Unknown/unsupported types and untyped recent-advert summaries are
excluded. The baseline advert cache and its access API remain intact, including
the phone's advert-path functionality and the separate Zen trace fallback cache.
The Node List no longer enumerates or merges recent adverts at all.

Infrastructure actions retain their existing rules: Ping, Path Details,
Favourite, Admin, Add and Delete as applicable, plus Trace for repeaters.
Companion speed-dial pin/unpin code and its label buffer have been removed from
this screen. Pin management elsewhere and saved pins are unchanged.

The selected full identity is revalidated before input or rendering. A change
to companion/unknown type invalidates both the selected record and its row.
Any node type change closes its action menu, confirmation or detail so an old
Trace action cannot run after a repeater becomes a room. Full-key target checks
continue to protect against removal, compaction and replacement.

Returning from Admin preserves the originating infrastructure filter and
selection. Cancelling Tools discovery restores the previous filter and leaves
scan mode before returning to Tools. Existing discovery requests, masks,
queueing, timing, retries and signal-monitoring policy are unchanged; the Node
List adds no probes. Tools discovery continues to request repeaters.

The 32-row cap, favourite ordering, distance/time sorting, strongest-first scan
sorting, periodic refresh and compact-reference memory model remain in place.
Sorting applies to admitted infrastructure rows rather than the whole contact
table. There are no new buffers or heap allocations.

## Automated verification

The production screen, model and storage adapters run in the native Node List
suite with fake storage/radio/UI-task boundaries. The suite covers:

- Every eligible type, companion/unknown/unsupported exclusions in stored and
  scan views, and full-identity requirements.
- More than 32 companions preceding infrastructure, including starred
  companions and an infrastructure favourite beyond the All view's cap.
- Exactly five shared filter labels, cycling/wraparound, discovery filter
  restoration and Admin return.
- Typed discovery acceptance, local/remote signal fields, live membership,
  rescan generation and selection when stronger results arrive.
- Untyped-advert exclusion with unchanged cache/path contents and access API.
- Full-key safety after reordering, deletion and same-prefix replacement.
- Type changes while a detail, menu or delete confirmation is open, including
  repeater-to-room changes that invalidate the old action set.
- Repeater, room and sensor management actions and absence of dial pin actions.
- Companion contact records, flags and speed-dial settings unchanged by browsing.
- OLED 128x64 and E-INK 250x122 rendering, empty/companion-only states, scrolling,
  and allocation-counter/static-size checks.

Run:

```sh
pio test -e native
PYTHONDONTWRITEBYTECODE=1 python -m unittest test/test_zen_baseline_extensions.py test/test_zen_radio_presets.py
python tools/check_zen_baseline.py
pio run -e WioTrackerL1_Zen_OLED -e WioTrackerL1_Zen_E-INK
```

## Verified results

- **675 native cases passed**, including 28 Node List cases.
- **17 Python cases passed** for baseline extensions and preset generation.
- Baseline/shadow checks and whitespace checks passed.
- OLED and E-INK target builds passed; both v2.2.62 UF2 files passed block,
  family, sequence and embedded-version verification.
- ELF measurements confirm no RAM increase: **48 bytes saved per target**.

| Allocation / target | v2.2.61 | v2.2.62 |
| --- | ---: | ---: |
| NearbyScreen | 1,356 | 1,332 |
| OLED static RAM | 193,528 | 193,480 |
| E-INK static RAM | 198,168 | 198,120 |
| OLED linker heap capacity | 41,992 | 42,040 |
| E-INK linker heap capacity | 37,352 | 37,400 |

The removed pin label accounts for 24 bytes; the former mutable filter-pointer
array accounts for another 24 bytes. The shared five-label table is 20 bytes in
read-only flash. The model remains 616 bytes and MyMesh remains 128,384 bytes;
its shared stores and the 672-byte trace cache have no layout changes. Linker
heap capacity precedes runtime allocations and is not live free heap.

## Hardware acceptance — pending

No OLED or E-INK board is connected. Automated and target-build verification
does not establish physical RF, BLE or display acceptance. On both boards:

1. Browse mixed companion/repeater/room/sensor contacts using all five filters.
   Confirm companion contacts, messages, favourite flags and pins remain usable
   in their existing interfaces.
2. Exercise all applicable infrastructure management actions, discovery/rescan,
   Add, Delete and Cancel, including returning from Admin.
3. Modify/remove/replace contacts through the phone with a detail, action menu
   or delete confirmation open; change a selected node's type as well.
4. Confirm no obsolete action is executed against an ineligible/replaced node.
5. Verify phone advert-path requests and repeater trace still operate normally.
6. Check both display orientations and compare runtime free heap/stack margins
   after repeated browsing and discovery.
