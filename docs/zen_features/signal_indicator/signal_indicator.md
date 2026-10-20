# Signal Indicator

[Back to README](https://github.com/MJCopper/MeshCore-Zen/blob/zen/main/README.md)

Zen displays a phone-style signal indicator at the right edge of the home-page
toolbar, immediately after the battery status. It estimates the quality of the
most recently observed repeater link; it does not indicate Bluetooth, GPS,
internet access or end-to-end delivery to a particular contact.

| Display | Meaning |
| ------- | ------- |
| `×` | No current repeater-link measurement, or radio unavailable |
| One bar | Low link margin |
| Two bars | Medium link margin |
| Three bars | High link margin |

The indicator is advisory. LoRa conditions, interference, collisions, routing
and repeater availability can change between packets, so bars do not guarantee
that a message will be delivered.

## Passive measurements

Zen reads the local SNR already supplied by the radio for received packets. It
accepts measurements when the packet proves that a repeater was involved:

- flood traffic containing at least one routing hop;
- direct traffic containing an intermediate routing hop;
- adverts received directly from a node identified as a repeater;
- repeater responses to manual or silent discovery.

Zero-hop companion traffic is excluded. Traffic received through Bluetooth or
USB is also unrelated to the measurement. Zen does not need to identify or save
the repeater responsible for a passive sample.

The four most recent passive samples form a rolling average. This prevents the
icon changing rapidly because of one unusually strong or weak packet. A passive
sample refreshes the measurement age immediately.

## Strength calculation

Bars are based on SNR link margin rather than one fixed SNR threshold. Zen uses
the approximate receiver limit for the configured spreading factor, from
−7.5 dB at SF7 to −20 dB at SF12.

| Margin above receiver limit | Indicator |
| --------------------------- | --------- |
| Up to 5 dB | One bar |
| More than 5 dB, up to 10 dB | Two bars |
| More than 10 dB | Three bars |

This makes the display comparable across radio presets: an SNR usable at SF12
may be below the demodulation limit at SF7.

## Discovery refresh

There is no periodic background scan. Zen considers the measurement due for a
refresh after 30 minutes. A silent repeater-only discovery starts only when all
of these conditions are met:

- the user wakes the sleeping display with Back;
- the last signal measurement is at least 30 minutes old, or none exists;
- no silent scan has been attempted during the previous 30 minutes;
- the radio is available and Low Power mode is not active;
- another interactive discovery scan is not active.

Notification and alarm wakes never start the scan. Pressing a button after a
notification has already woken the display does not count as a new user wake.
Emergency Mode does not perform automatic signal discovery.

The silent scan lasts eight seconds and produces no popup, sound or additional
display wake. It does not alter the interactive Discover Repeaters list or saved
contact timestamps. A manual **Tools › Discover Repeaters** scan can replace an
active silent scan immediately.

Discovery is authoritative when its eight-second window completes:

- if repeaters answer, the strongest received SNR replaces the passive average;
- if no repeater answers, the existing reading is cleared and the cross appears.

A failed silent attempt is not repeated until another eligible user wake at
least 30 minutes later.

## Age, restart and power

A passive or successful discovery value expires after two hours. The cross also
appears immediately whenever the radio is unavailable. Signal history and scan
times are held only in RAM, cause no flash writes and reset at reboot.

Passive collection adds no radio transmissions. A silent discovery adds one
zero-hop request and the resulting repeater replies, but only on an eligible
user wake. Low Power and Emergency modes suppress this automatic transmission.

## Troubleshooting

- **Cross immediately after restart:** no qualifying traffic or successful
  discovery has been received yet. Wake the screen or run Discover Repeaters.
- **Cross despite receiving messages:** the traffic may be arriving directly
  from another companion rather than through a repeater.
- **Cross after a wake:** the authoritative discovery window completed without
  a repeater response.
- **Bars do not change for 30 minutes:** this is expected if no qualifying
  packet arrives; the next eligible user wake requests a refresh.
- **Bars differ from Discover Repeaters:** passive display uses a rolling
  average, whereas completed discovery uses its strongest response.

