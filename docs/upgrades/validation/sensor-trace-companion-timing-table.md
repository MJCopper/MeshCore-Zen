# Companion-derived timing table

Generated on 25 October 2026 from the pinned RadioLib SX126x calculator and
current TraceTransactionBudget. All rows use an unscoped target, public guest
login enabled, CRC enabled and explicit headers. SF7/8 preamble is 32 symbols;
higher SF uses 16. These are upper bounds including unused retries, shared
queueing and reporting, not predicted successful completion times. An unknown
route reserves one supported ten-repeater probe. A zero maximum means even
the minimum probe/report allowance does not fit the safety policy.

| SF | BW | CR | Width | Route | Login RX ms | TRACE ms | Maximum s | Reserved probes | Capped |
|---:|---:|---:|---:|:---|---:|---:|---:|---:|:---|
| 7 | 62.5 | 7 | 1 | 1 | 6620 | 3944 | 75 | 2 | no |
| 7 | 62.5 | 7 | 1 | 2 | 6620 | 6724 | 81 | 2 | no |
| 7 | 62.5 | 7 | 1 | 4 | 6620 | 13340 | 94 | 2 | no |
| 7 | 62.5 | 7 | 1 | 10 | 6620 | 43100 | 109 | 1 | no |
| 7 | 62.5 | 7 | 1 | unknown | 6620 | 43100 | 109 | 1 | no |
| 7 | 62.5 | 7 | 2 | 1 | 6620 | 3944 | 75 | 2 | no |
| 7 | 62.5 | 7 | 2 | 2 | 6620 | 7060 | 82 | 2 | no |
| 7 | 62.5 | 7 | 2 | 4 | 6620 | 14684 | 97 | 2 | no |
| 7 | 62.5 | 7 | 2 | 10 | 6620 | 51740 | 117 | 1 | no |
| 7 | 62.5 | 7 | 2 | unknown | 6620 | 51740 | 117 | 1 | no |
| 8 | 62.5 | 7 | 1 | 1 | 10812 | 5540 | 101 | 2 | no |
| 8 | 62.5 | 7 | 1 | 2 | 10812 | 10276 | 111 | 2 | no |
| 8 | 62.5 | 7 | 1 | 4 | 10812 | 21788 | 110 | 1 | no |
| 8 | 62.5 | 7 | 1 | 10 | 10812 | 72860 | 120 | 1 | yes |
| 8 | 62.5 | 7 | 1 | unknown | 10812 | 72860 | 120 | 1 | yes |
| 8 | 62.5 | 7 | 2 | 1 | 10812 | 5540 | 101 | 2 | no |
| 8 | 62.5 | 7 | 2 | 2 | 10812 | 10972 | 112 | 2 | no |
| 8 | 62.5 | 7 | 2 | 4 | 10812 | 24572 | 113 | 1 | no |
| 8 | 62.5 | 7 | 2 | 10 | 10812 | 90140 | 120 | 1 | yes |
| 8 | 62.5 | 7 | 2 | unknown | 10812 | 90140 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 1 | 1 | 16204 | 7520 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 1 | 2 | 16204 | 13540 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 1 | 4 | 16204 | 31100 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 1 | 10 | 16204 | 116780 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 1 | unknown | 16204 | 116780 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 2 | 1 | 16204 | 7520 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 2 | 2 | 16204 | 14932 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 2 | 4 | 16204 | 36620 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 2 | 10 | 16204 | 144260 | 120 | 1 | yes |
| 9 | 62.5 | 7 | 2 | unknown | 16204 | 144260 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 1 | 1 | 60348 | 21356 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 1 | 2 | 60348 | 46732 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 1 | 4 | 60348 | 113948 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 1 | 10 | 60348 | 420260 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 1 | unknown | 60348 | 420260 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 2 | 1 | 60348 | 24116 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 2 | 2 | 60348 | 52228 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 2 | 4 | 60348 | 135980 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 2 | 10 | 60348 | 557900 | 120 | 1 | yes |
| 11 | 62.5 | 7 | 2 | unknown | 60348 | 557900 | 120 | 1 | yes |
| 12 | 62.5 | 7 | 1 | 1 | 111852 | 40724 | 120 | 1 | yes |
| 12 | 62.5 | 7 | 1 | 2 | 111852 | 90964 | 120 | 1 | yes |
| 12 | 62.5 | 7 | 1 | 4 | 111852 | 202460 | 120 | 1 | yes |
| 12 | 62.5 | 7 | 1 | 10 | 111852 | 779060 | 0 | 1 | yes |
| 12 | 62.5 | 7 | 1 | unknown | 111852 | 779060 | 0 | 1 | yes |
| 12 | 62.5 | 7 | 2 | 1 | 111852 | 40724 | 120 | 1 | yes |
| 12 | 62.5 | 7 | 2 | 2 | 111852 | 90964 | 120 | 1 | yes |
| 12 | 62.5 | 7 | 2 | 4 | 111852 | 246476 | 0 | 1 | yes |
| 12 | 62.5 | 7 | 2 | 10 | 111852 | 999260 | 0 | 1 | yes |
| 12 | 62.5 | 7 | 2 | unknown | 111852 | 999260 | 0 | 1 | yes |

Reproduce with `python3 test/timing_reference/generate_companion_table.py`.
The generator extracts the dependency calculator body verbatim; it does not
substitute a separately maintained LoRa formula. `--vectors` generates the
native-test reference matrix including other bandwidth, coding rate, preamble,
CRC and header settings. Scope bytes increase request airtime; the live
firmware includes them when selecting its timeout.
