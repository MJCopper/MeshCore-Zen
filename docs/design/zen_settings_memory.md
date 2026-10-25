# Streaming Zen settings and transactional saves

Zen v2.2.58 keeps active settings in a plain `ZenSettings` value and retains
MeshCore's `NodePrefs` as an independent base of `ZenPrefs`. MeshCore continues
to own baseline serialization, filesystems, BLE and radio startup. Only the
existing reviewed Zen application integration files change at that boundary.

## Memory budget

Both ARM targets have the same settings object sizes:

| Allocation | Before | After |
|---|---:|---:|
| `ZenStore` | 4,128 B | 160 B |
| `ZenPrefs`, including 384 B baseline preferences | 3,056 B | 1,600 B |
| Combined permanent RAM | 7,184 B | 1,760 B |

This recovers **5,424 bytes on each target**. The store's only working buffer
is 128 bytes. Full records up to the unchanged 4,096-byte input limit can be
processed through it. A 1,216-byte Zen-only staging allocation exists only
while decoding a candidate. It is checked, committed only after validation,
and released before MyMesh startup returns, before Bluetooth/UI initialization.
No 4-KiB stack or heap copy replaces the removed permanent buffer. Compile-time
bounds enforce store <=192 B and staging <=1,536 B.

The current record is 1,383 bytes. Tags have frozen widths and explicit field
selection; preset fields are emitted individually without C++ padding. Unknown
records are bounded and skipped. Checksums and canonical fingerprints are
incremental. Fingerprints ignore padding and the MeshCore base. Settings-screen
change detection combines separate canonical Zen and baseline fingerprints.

## Compatibility

Envelope version 3 stops emitting the raw whole-tail record. Five quick replies
remain. Unused quick-reply slots, retired bot data, Trail/Locator/Clock Tools,
GPIO and other obsolete storage fields are removed from the live object.
Existing numeric page IDs and active preferences retain their meaning.

Versions 1 and 2 remain readable, including the 2,672-byte nRF52 raw tail.
`ZenSettingsFields.h` records frozen legacy byte offsets; decoding never
allocates an old preferences object or uses host-layout offsets for old files.
Active values are staged field by field; retired fields are discarded. Existing
semantic migrations/validation still follow file decoding. A legacy or recovered
record requests a verified migration save. MeshCore base bytes are never copied
into or out of the Zen record.

Every candidate header (primary, temporary, backup) is checked before migration.
Newer formats lock writes, even when a readable older primary also exists.
Read or allocation failure protects saved files instead of triggering a defaults
write. The application reports a load fault separately from incompatible-format
status. Unsupported newer data is preserved; older firmware cannot read version
3 and its existing newer-version lock prevents overwriting it on downgrade.

## Save transaction

Settings mutation and storage are loop-owned. Reentrant store operations are
rejected. Canonical fingerprints before writing, before promotion and after
verification detect changes made by yielding callbacks; unsaved edits remain
dirty rather than reporting a mixed-settings save as successful.

1. If the only verified record was temporary/backup, move it into the primary
   position before truncating the next staging file. Failed recovery leaves
   that verified candidate available.
2. Stream a complete new generation into `/zen_prefs.tmp`, then close it.
3. Reopen and compare its complete bytes against the canonical expected encoder,
   including header, checksum, generation and payload.
4. Preserve the previous primary in `/zen_prefs.bak`, then promote the staging
   file. This sequence works whether rename can overwrite a destination or not.
5. Reopen and verify the promoted file before deleting the backup and reporting
   success. Failed verification restores the previous primary where possible,
   otherwise preserves its backup for boot recovery.

After interruption, load selects a complete valid primary, then staging, then
backup. Failed save operations do not clear the caller's dirty flag. These are
Zen-file transactions; baseline MeshCore preference writes retain their existing
independent lifecycle.

Raw room-password and pet extension files use the same bounded comparisons and
promotion protection. Their sizes are not limited to the scratch-buffer size,
and saving an extension does not clear Zen preference recovery state.

## Validation

Native coverage includes every active tagged field, byte-for-byte frozen
legacy fixtures, baseline preservation, canonical hashing, reset behavior,
maximum strings, unknown records, corruption/truncation, full input limits,
short/error reads, failed opens/writes, allocation failure, newer versions in
all candidate positions, edits/reentry during save, and power cuts after every
modeled write/promotion mutation. First save, normal replacement, legacy
migration, sole temporary/backup recovery and raw extension transactions are
covered. Both rename policies are exercised. Pet save/recovery regressions remain.

Both ARM builds must verify the memory deltas and readonly descriptor/pet tables,
UF2 structure/family/version, baseline manifest and compiler stack reports.
The measured largest individual settings frame is 280 bytes, not a guarantee
of complete call-chain or interrupt-stack headroom.

Physical acceptance still requires OLED and E-ink power-cycle/save tests,
settings edit/cancel/reset/reboot checks, Bluetooth/messages/trace/pet exercise,
and heap/stack readings under load. No board is connected in this environment.
Simulated filesystem power loss does not establish hardware flash behavior.
