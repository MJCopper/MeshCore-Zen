# Time Zone

[Back to README](../../../README.md)

Zen keeps the system clock in UTC and applies a local offset only when displaying
or evaluating local time. Changing the time zone does not rewrite the clock.

Open **Settings › System › Time Zone**. Use Up/Down to select the mode or value,
Left/Right to change it, and Back to apply the selection. Zen saves only when the
result differs from the value that was opened.

## City mode

City mode uses the selected city's embedded standard-time and daylight-saving
rules. It is automatic with respect to daylight saving; it does not use GPS to
choose a city. Select the representative city manually.

Fresh Zen installations default to **City: Sydney**. The editor shows the
effective UTC offset and adds `DST` while daylight saving is active. Before the
clock has synchronised, it shows `Offset --:--` because the firmware cannot yet
determine whether a seasonal rule applies.

Supported cities are:

| Region | Cities |
| ------ | ------ |
| Australia | Sydney, Melbourne, Brisbane, Adelaide, Perth, Hobart, Darwin, Canberra |
| New Zealand | Auckland |
| Asia | Tokyo, Singapore, Hong Kong, Beijing, Delhi |
| Europe | London, Paris |
| United States | New York, Chicago, Denver, Los Angeles |

Sydney, Melbourne, Hobart and Canberra use Australian eastern daylight-saving
rules. Adelaide uses the corresponding central rule. Brisbane, Perth and Darwin
do not apply daylight saving. Auckland, London, Paris and the listed US cities
use their respective embedded seasonal rules.

The rules are intentionally compact and do not contain a complete worldwide
time-zone database. If a government changes its daylight-saving law, a firmware
update may be required.

## Fixed UTC mode

Fixed UTC applies one unchanging offset throughout the year. It is useful for a
location not represented by the city list or when automatic daylight saving is
not wanted.

The offset ranges from UTC−12:00 through UTC+14:00 in 15-minute steps, including
half-hour and quarter-hour zones. For example, use UTC+10:00 for New South Wales
standard time only. Fixed UTC will remain at +10:00 during summer; use City:
Sydney if automatic AEDT adjustment is wanted.

## Where local time is used

The selected mode is applied consistently to:

- the Clock home page;
- Quiet Time start and end times;
- diagnostic event timestamps;
- local-time message and Quick Reply placeholders.

GPS and companion time synchronisation still provide UTC. They do not overwrite
the selected city or fixed offset.

Existing installations retain their previous fixed UTC offset during migration.
Selecting City opts into the embedded daylight-saving rules.

