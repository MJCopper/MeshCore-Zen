# Signal Indicator

[Back to README](../../../README.md)

The home-page toolbar shows the best recently observed identified repeater
reception. It measures incoming radio quality, not distance, Bluetooth, internet
access, end-to-end delivery, or whether the repeater can hear your transmissions.

| Display | Meaning |
| ------- | ------- |
| One to four bars | Recent reception, from low to high signal quality |
| Hollow triangle with a small X | No measurement yet, or measurements are 15 minutes old or older |
| Standalone X | The latest completed discovery found no repeaters, or the radio is unavailable |

## Understanding the bars

Zen averages recent repeater receptions. If no identified repeater has a fresh
reading, it uses recent routed traffic as an estimate. Direct companion traffic
is excluded. Nearby repeaters can still show fewer bars: the meter reflects
signal-to-noise ratio (SNR), not distance.

The scale adjusts for spreading factor. At **SF7**, initial readings mean:

| SNR | Bars |
| --- | ---: |
| Any valid reading up to 5 dB | 1 |
| More than 5 dB, up to 7 dB | 2 |
| More than 7 dB, below 10 dB | 3 |
| 10 dB or higher | 4 |

Averaging and confirmation of changes reduce flicker, so a single strong or
weak packet may not immediately change the bars. Changing radio settings or
restarting clears the measurements.

## Discovery

While the display is on, Zen can check for repeaters when there has been no
fresh measurement for 15 minutes. Automatic checks are at least 15 minutes
apart. Fresh passive reception avoids unnecessary checks; a sleeping display
stays asleep.

For an immediate check, open **Tools › Discover Repeaters**. A scan lasts eight
seconds. Responses update the meter; no responses produce X unless fresh
passive reception has already supplied a reading. Manual discovery restarts
the automatic cooldown.

## Check a reading

Open **Tools › Diagnostics**:

- **Live** shows the selected signal, its age, RSSI/SNR and discovery status.
- **Signal** lists recent samples. Use Up/Down and Enter to inspect one.

Compare readings over several receptions rather than expecting a nearby
repeater to immediately show four bars. Manual discovery shows a single
response; the toolbar shows an average.
