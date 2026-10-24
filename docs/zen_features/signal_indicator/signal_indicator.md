# Signal Indicator

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

The home-page toolbar shows the best recently observed identified repeater
reception. It measures incoming radio quality, not distance, Bluetooth, internet
access, end-to-end delivery, or whether the repeater can hear your transmissions.

| Display | Meaning |
| ------- | ------- |
| One to four bars | Fresh qualifying reception, increasingly good SNR margin |
| Hollow right triangle with a small X inside | No measurement yet, or all measurements have expired |
| Standalone X | Latest completed discovery found no repeaters, or radio unavailable |

Only active bars are drawn. Status/bar changes refresh a visible home page
without waking a sleeping display. The hollow triangle has a flat bottom,
a vertical right edge, and a diagonal rising from left to right; the small X
sits inside it. It occupies the existing signal-meter width. Rendering and
passive collection do not transmit; the automatic scheduler can queue a scan
while any screen is on.

## Evidence selection

Each sample expires individually after **15 minutes**. Up to five identified
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

Automatic discovery is checked continuously through the existing application
loop **while the display is on**, on any screen. It no longer requires a new
Back-button wake. It may queue a silent eight-second repeater-only scan when:

- no qualifying measurement is less than **15 minutes** old;
- at least **15 minutes** have passed since the previous queued discovery, or
  none has been queued since boot;
- no discovery is active;
- the radio is available and Low Power/Emergency restrictions permit it.

Fresh passive traffic suppresses automatic probing. A continuously lit display
becomes eligible when evidence and the cooldown expire. Sleeping displays stay
asleep and do not probe. Notification/alarm wakes follow the same screen-on
policy; there is no separate wake-only trigger or catch-up burst.

Manual discovery remains available during the cooldown and can replace an
active silent scan. Every queued manual or automatic discovery restarts the
15-minute automatic cooldown. Changing presets cannot evade it. If a packet
cannot be allocated, no scan is recorded as failed: a five-second local retry
backoff prevents the application loop from repeatedly attempting allocation.

Starting a scan preserves the current symbol until evidence or completion
changes it. Successful responses update the same per-repeater histories as
passive adverts. A completed scan with **no repeater responses** clears old
strength evidence and shows the standalone X. That result persists until fresh
qualifying reception or successful discovery restores bars. The next automatic
attempt still waits for the cooldown and an on-screen display.

If fresh qualifying passive traffic arrives during an unanswered scan, it is
preserved rather than erased by scan completion. Cancelled scans, allocation
failures, and late/nonmatching responses do not report "none found". Turning
the radio off cancels an active scan without recording a false failure. Radio
unavailability always shows X; when it becomes available, the stored evidence
or outcome is shown again. Preset changes clear incompatible evidence and the
failed-discovery outcome, returning to the triangle while retaining cooldown.

Each sample expires at exactly 15 minutes; other unexpired samples can continue
to provide bars. An expired measurement shows the triangle unless a failed scan
has established the persistent standalone X state. No measurement since boot
or a configuration change also shows the triangle.

All history is RAM-only and resets at reboot. Passive collection, expiry,
selection and diagnostics add no mesh traffic or flash writes. An actual scan
adds one zero-hop request and responder traffic, so the cooldown still matters.

## Diagnostics and field checks

In **Tools › Diagnostics › Live**, Signal shows status/bars; Evidence shows
identified (`ID`) or fallback (`rt`), newest sample age and sample count;
Link RX shows RSSI/SNR for the selected average, rather than an unrelated last
packet. Discovery shows scanning/idle; Auto wait shows seconds remaining until
freshness, cooldown and allocation backoff allow an automatic attempt (or
"disabled" for power restrictions). The screen must still be on to queue it.

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
