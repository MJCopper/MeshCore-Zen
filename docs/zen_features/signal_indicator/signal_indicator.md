# Signal Indicator

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

The home-page toolbar shows the best recently observed identified repeater
reception. It measures incoming radio quality, not distance, Bluetooth, internet
access, end-to-end delivery, or whether the repeater can hear your transmissions.

| Display | Meaning |
| ------- | ------- |
| One to four bars | Fresh qualifying reception, increasingly good SNR margin |
| Small hollow clock | Previous evidence has expired (stale) |
| Cross | No evidence since boot/configuration change, or radio unavailable |

Only active bars are drawn. Status/bar changes refresh a visible home page
without waking a sleeping display. No packets are transmitted by rendering or
by opening Diagnostics.

## Evidence selection

Each sample expires individually after **five minutes**. Up to five identified
repeaters each retain their four most recent measurements. The meter selects
the fresh repeater with the highest averaged SNR, breaking ties in favour of
more recent reception. When the table is full, the least recently heard entry
is replaced. Full 32-byte keys, not short path hashes, distinguish identities.

An identified sample comes from:

- a directly received zero-hop flood advert whose signature was verified by
  MeshCore and whose application data identifies a repeater; or
- a zero-hop repeater discovery response matching Zen's active request tag.
  Discovery replies are identified by their full advertised key, but are not
  signed identity proofs.

Routed flood traffic with at least one hop, and direct traffic with more than
one path entry, provide **unattributed fallback** evidence. Their routing data
does not reliably identify the immediate transmitter. These samples share a
separate four-sample buffer and cannot dilute an identified repeater's history.
Fallback is used only when no identified repeater has fresh evidence. It is an
estimate of recent routed reception, not a claim that all samples came from the
same repeater.

Zero-hop companions are excluded. Qualification occurs after successful packet
envelope parsing and minimum payload checks; that does not authenticate routed
fallback traffic. Measurements use SNR and RSSI from the same radio reception.
A bounded capture retains advert RSSI through MeshCore's delayed receive queue;
if that capture is unavailable, the measurement is skipped rather than guessed.

Duplicate packet hashes within one link's retained samples are suppressed for
30 seconds: they do not refresh age or confirm a bar transition. The same
payload received from different identified repeaters counts independently.
Unattributed duplicates cannot reliably be separated by transmitter, so they
are conservatively suppressed together. The hash cache is bounded to the
four retained samples per link; it is not an unlimited duplicate database.

## SNR scale and smoothing

RSSI is retained for diagnostics; bars remain an SF-adjusted **SNR quality**
scale. No combined RSSI/SNR score has been introduced without field data.
A nearby repeater can still have modest reported SNR. The scale does not promise
four bars merely because a repeater is physically close.

The reference is −2.5 dB at SF5, decreasing by 2.5 dB per SF to −20 dB at SF12
([Semtech SX1261/SX1262 datasheet, Table 6-1](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)).

| Averaged margin above reference | Initial bars | Equivalent SNR at SF7 |
| ------------------------------- | ------------ | -------------------- |
| Up to 12.5 dB, with a valid sample | One | Up to 5 dB |
| More than 12.5 dB, up to 14.5 dB | Two | More than 5 dB, up to 7 dB |
| More than 14.5 dB, below 17.5 dB | Three | More than 7 dB, below 10 dB |
| At least 17.5 dB | Four | At least 10 dB |

Initial evidence uses these exact boundaries. Subsequent transitions require
two accepted receptions on the same link with its averaged SNR at least 1 dB
beyond the relevant boundary (retaining the strict/inclusive comparisons above).
At SF7, rising to four therefore requires an average of at least 11 dB twice;
falling from four requires an average below 9 dB twice. Rendering and duplicate
packets cannot advance confirmations. Changing the selected repeater does not
transfer confirmations between links. When evidence expires, classification
uses only the remaining fresh samples and bypasses the old transition latch.

SF5–SF12 are supported, with no extra bandwidth/coding-rate correction.
Changing frequency, bandwidth, SF or coding rate clears evidence, diagnostic
history and pending discovery. Reapplying unchanged settings retains evidence.

## Discovery and airtime

There is **no periodic background scan**. A silent eight-second repeater-only
discovery may start only when the user wakes a sleeping display with Back,
there is no fresh qualifying evidence, and at least **30 minutes** have passed
since the previous successfully queued discovery request. Manual requests also
start this automatic-probe cooldown; changing presets cannot bypass it.

Fresh passive traffic suppresses automatic probing. An expired five-minute
reading can remain stale while the 30-minute cooldown is still active. Repeated
wakes do not shorten that cooldown. Notification/alarm wakes do not probe, and
Low Power/Emergency suppress automatic discovery. Manual discovery remains
available and can replace a silent scan under the existing rules.

Discovery responses update the same per-repeater histories as passive adverts;
they do not replace all existing evidence with one strongest response. No
response records a failed scan but preserves other fresh evidence. Cancelled,
late or nonmatching responses cannot add discovery samples. There is no special
penalty sample for silence: evidence ages out normally.

All history is RAM-only and resets at reboot. Passive collection, expiry,
selection and diagnostics add no mesh traffic or flash writes. An actual scan
adds one zero-hop request and responder traffic, so the cooldown still matters.

## Diagnostics and field checks

In **Tools › Diagnostics › Live**, Signal shows status/bars; Evidence shows
identified (`ID`) or fallback (`rt`), newest sample age and sample count;
Link RX shows RSSI/SNR for the selected average, rather than an unrelated last packet.

The **Signal** diagnostics tab lists the 16 most recent accepted samples, newest
first. Each row shows source, packet SNR/RSSI and current age. Select a row and
press Enter for a scrollable detail containing the packet measurements, source
key, selected estimate/key prefix, sample count and selected evidence age at
that moment. The detail freezes the row last displayed so incoming traffic
cannot silently replace the sample while it is being inspected.

Local USB CLI Rescue also accepts `signal` for current selection and failure
count, or `signal 0` through `signal 15` for history. `snr4`, `rx4` and `avg4`
are quarter-dB units (40 means 10 dB). These commands are local diagnostics;
they do not initiate discovery. The on-device Signal detail exposes the full
source key where a CLI line uses its short prefix.

For field calibration, record passive samples beside a repeater, while moving
away, and at a quiet location. Compare packet RSSI/SNR with selected averages,
identity and age. A comparison against Discover should allow for its raw single
response versus the toolbar's per-link smoothing. Keep the initial SNR scale
until measurements justify a separate combined-strength proposal.
