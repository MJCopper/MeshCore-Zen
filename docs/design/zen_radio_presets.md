# Official radio preset snapshot

Zen's on-device preset catalogue is generated from a committed snapshot of the
[MeshCore app API](https://api.meshcore.nz/api/v1/config), the source identified
by baseline `docs/radio_presets.md`. No MeshCore core, transport, serializer or
baseline firmware upgrade is involved. The firmware never downloads or parses
JSON. Existing saved settings are not migrated when the catalogue changes.

## Files and release workflow

- `data/zen_radio_presets.json`: upstream `suggested_radio_settings` payload,
  source URL, UTC retrieval time and canonical payload SHA-256.
- `data/zen_radio_preset_overrides.json`: currently empty. Additions require a
  source and reason and cannot replace an official name. Generated entries
  identify additions separately from upstream entries.
- `tools/zen_radio_presets.py`: standard-library refresh, generation and check.
- `RadioPresets.generated.h`: committed generated flash-resident entries.
- `RadioPresets.h`: types, bandwidth capabilities and matching helpers.

During a release, explicitly refresh and review:

```sh
python3 tools/zen_radio_presets.py --refresh
git diff -- data/zen_radio_presets.json examples/companion_radio/zen-overlay/app/RadioPresets.generated.h
python3 -B -m unittest discover -s test -p test_zen_radio_presets.py
python3 tools/zen_radio_presets.py --check
```

For a reviewed local snapshot/override edit, run `--generate` instead of
`--refresh`. Generation and checking are offline. Refresh uses a project-identifying
User-Agent, a 20-second timeout and a 1-MiB response limit. Invalid API data or
network failure cannot replace the previous snapshot/header. Unchanged upstream
content does not create retrieval-date-only diffs. Writes use atomic replacement
per file; if an interrupted local update leaves the pair inconsistent, the
build check fails until `--generate` repairs it from the snapshot.

Both Zen PlatformIO preflight hooks and CI run `--check`; they never refresh.
No build-time timestamp appears in generated content. After PlatformIO's normal
toolchain/dependencies are installed, preset handling adds no network dependency.
CI runs the generator tests with Python and a host C++ compiler, plus native
firmware tests and both ARM target builds.

Review frequency, bandwidth, SF, CR, names, additions/removals and optional network
settings in every snapshot diff. Do not update an existing snapshot just to
silence a check failure. Review any upstream format change first.

## Inclusion and validation

The current snapshot contains 26 entries. Zen includes 21: the existing
868–928 MHz policy excludes three 433-MHz entries and two deprecated entries.
Each exclusion and reason appears in generation/check output and header comments.
Upstream ordering and names are preserved; stale hand-maintained entries and
aliases are removed from the built-in list. User-defined presets remain intact.

RF values must be finite, use supported bandwidths, SF5–12 and CR5–8. Titles
must be nonempty, bounded UTF-8 without control characters; generated C++ strings
are escaped as UTF-8 bytes. Duplicate/conflicting names are rejected, including
case-only conflicts. Missing optional fields are supported. Unsupported network
options fail refresh for review rather than being silently omitted.

Australia (Narrow) is 916.575 MHz / BW62.5 / SF7 / CR7. This corrects Zen's
previous CR8 entry and matches both the official API and pinned baseline example.

## Picker and compatibility

The existing 24-row shared popup is unchanged. Built-ins use pages of at most
16 entries, leaving room for Save Current, four user presets, Delete, Previous
and Next. Opening selects the page containing the matching built-in. An explicit matching
hash recommendation takes precedence over a generic RF-only match; otherwise
identical aliases retain upstream-first matching. Every included entry is
reachable. Paging, opening and cancellation do not change settings.

Explicitly selecting a built-in applies RF values and, when provided, its
`network_settings.path_hash_size` through MeshCore's existing radio preferences
setter. Sizes 1/2/3 map to modes 0/1/2. Missing metadata leaves the current mode
unchanged. Matching checks an explicitly recommended hash size as well as RF.

Saved user presets retain their existing four-RF-field format; selecting one
leaves hash mode unchanged. Existing active CR8 configurations remain CR8 until
an updated preset is selected. A no-longer-matching configuration shows its
matching user-preset name or Custom. Presets do not change power, scope or any
other setting not included in their supported data.

Some official entries recommend three-byte hashes. MeshCore supports their
configuration and Zen applies it without coercion. Zen repeater trace still
supports one/two-byte paths: trace startup rejects three-byte mode without
sending probes, displaying the existing unsupported-width guidance. Extending
trace's hash-width support is outside this catalogue change.

## Verification

Generator tests cover reproducibility, provenance, RF/schema validation,
filtering, all exclusions, duplicate names, optional/unsupported metadata,
UTF-8/quote/backslash escaping, offline checking, stale output, failed refresh
and unchanged refresh. Picker tests cover all entries, pages with zero through
four users, sparse slots, selection/cancellation/save/delete confirmation,
current-name/hash matching, CR8 persistence, three-byte trace rejection and
render bounds on OLED/E-ink/portrait geometries. Heap-allocation tracking covers
picker open/navigation/matching/application. The table and names must be in
read-only flash, with no increased static RAM or shared popup capacity.

Release acceptance also requires native regressions, baseline checks, both ARM
builds and versioned UF2 structure/family/version verification. Physical display,
radio interoperability and power-cycle acceptance still require device tests;
no attached serial device is available in this environment.

### v2.2.60 software validation

647 native cases, 11 generator cases and 6 baseline-extension checks passed.
Both Zen targets built successfully and their versioned UF2 files passed block,
family, count/order and embedded-version verification. All 21 generated entries
were decoded directly from each ELF and compared with the snapshot, including
network hash recommendations; the table and every name reside in readonly flash.
Static RAM remains 197,928 bytes (OLED) and 202,568 bytes (E-ink), unchanged from
v2.2.59. Baseline/shadow checks and whitespace checks passed. Physical acceptance
remains pending as described above.
