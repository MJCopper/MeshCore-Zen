# Frozen pre-compaction Zen settings fixtures

These records were generated with the v2.2.57 codec and preferences layout
before modifying either implementation. v1/v2 fixtures use the previously
accepted envelope versions with the same valid tagged payload and checksum.
They are synthetic compatibility files, not device captures.

The raw Zen tail is 2,672 bytes. Its ARM offsets were frozen from target
compiler layout information. Numeric fields are little-endian; no native host
`sizeof` or current live settings offsets are used to decode old files.

`v1.bin` and `v2.bin` contain representative active values, long quick replies,
Australia Narrow radio preset, and dormant data which must be discarded.
`all_fields_v*.bin` additionally exercise every retained raw-tail field with
nonzero patterns (including deliberately out-of-range values before semantic
validation). `all_fields.records` stores the independent pre-compaction field
values as tag/length/bytes, for byte-exact migration assertions. Tests also
round-trip the current format and test active-value normalization separately.

SHA-256:

- `all_fields.records`: `66c4816ba2e234f2dd32d53ffd40f51e64d2474e83e6dd40169858c7f5a49b5a`
- `all_fields_v1.bin`: `c5011c26fd0ed20e50532915f887860fd8f60a20902d040fc301e2b3ea7b9e1b`
- `all_fields_v2.bin`: `f18e5a318ee2112d8432df73f2e1805101a9127ef1ecce25bfe220367c107c40`
- `v1.bin`: `6d9b0cb8a53557047ca799b3717d4b86467ce0c7f7a2455d563e9b276a5b0ab5`
- `v2.bin`: `f4993159fd3029b2eb4f4173fc845dfa1ef9c33fded4e0f43d8a76c4bbb2dc79`
