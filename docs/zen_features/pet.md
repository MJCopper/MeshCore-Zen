# Virtual Pet

[Back to README](../../README.md)

Enable **Settings → System → Zen Pet** to show the pet page after Favourites.
The default is Off. The enable switch is saved in Zen settings; pet progress uses
a separate checkpoint. Reboot restores the last valid save, without offline ageing.
Unsaved progress is lost. If no checkpoint exists, enabling creates a new starter.
Turning Off and On during the same boot preserves the pet.

Press Enter for **Feed, Train, Evolve, Details, Daily Rewards** and **Save Pet**.
Hold Enter opens Details. Up/Down selects an action; Back closes the menu, then
returns to Clock. Left/Right changes home pages.

## Saving

Save Pet makes a one-off verified checkpoint. It reports Pet saved or Already
saved, or a specific storage/battery failure. Repeated requests are coalesced.
Saving a sleeping pet does not wake it. Training must finish or be cancelled first.

A new starter is saved automatically. Later automatic saves happen at a safe
local midnight, at least 24 hours after the last save. Before the clock is set,
Zen uses a 24-hour interval. Save Pet lets you keep progress sooner.

No automatic writes occur while disabled, on shutdown, or for individual care
actions. Automatic failures are logged silently with hourly retry backoff.
Unknown/critical battery readings block writes; external power permits saving.
Internal-flash fallback writes wait until Bluetooth is switched off. External flash
uses the existing Zen storage path. Actual flash operations include filesystem
metadata and replacement, not just the small 96-byte snapshot payload.

The versioned, checksummed `/zen_pet` record uses verified staging and recovery
through `/zen_pet.tmp` and `/zen_pet.bak`, independently of MeshCore baseline files.
Corrupt candidates are not automatically overwritten that boot; Save Pet can replace
them. A newer unsupported schema is preserved and write-locked. Filesystem operations
run from the main loop, never from rendering, input or mesh callbacks.

## Care and evolution

Sprout has twelve levels and 189 forms. Open **Details** for current care values
and the requirements for the next evolution. **Evolve** shows the available
names and portraits and asks for confirmation.

- **Feed:** uses one food, restores 25 fullness and adds 1 Bond.
- **Train:** a win uses 20 energy and 10 fullness, adds 1 Bond and awards XP.
  The reward starts at 20 XP and increases by 10 per level. Rest five minutes
  between wins; the menu shows the remaining time.
- Food replenishes every four enabled hours, up to five.
- Energy recovers 10 per hour, or 20 while sleeping.
- Fullness falls by 5 per awake hour at normal battery levels.

Low battery increases hunger: 6 per hour at 25–49%, 7 at 10–24%, and 8 below
10%. Charging reduces this as the battery recovers, but does not feed the pet.
Details can show **Charge to reduce hunger**. Sleep and Low Power pause hunger.

| Current level | XP needed for next level | Bond needed |
|---|---:|---:|
| 1 | 160 | 10 |
| 2 | 280 | 12 |
| 3 | 640 | 16 |
| 4 | 1,190 | 20 |
| 5 | 2,210 | 25 |
| 6 | 4,170 | 30 |
| 7 | 7,530 | 40 |
| 8 | 12,570 | 50 |
| 9 | 19,570 | 65 |
| 10 | 28,810 | 80 |
| 11 | 41,050 | 100 |


XP is cumulative. Evolution spends the required Bond and keeps any surplus;
Bond caps at 100 and does not decay or decrease after a lost game. Even levels
offer two evolution choices; odd levels offer one. Regular care without mesh
bonuses takes roughly 60 days to reach level 12; missed care and low battery
can make it slower.

At level 12, **Retire** replaces Evolve. Earn 100 retirement XP and 10 Bond:
training wins add 20 retirement XP, and eligible mesh XP also counts. Details
shows your progress.

When ready, confirm **Retire pet? Start a new pet**; No is selected initially.
Sleeping, Low Power and Pet Off prevent retirement. A verified checkpoint must
be saved before the old pet is replaced. Unsafe storage defers the request;
Back cancels it. Save failures keep the active pet and report the error.
The new Sprout has normal starting resources and a newly selected temperament.
Old games, reactions and pending pet alerts/rewards are cleared, but the day's
mesh allowances remain consumed. Retirement adds one explicit checkpoint write.
Snapshot schema 2 preserves retirement progress and reads schema 1 with zero
retirement XP; neither baseline storage nor settings schema changes.

## Sleep

The pet sleeps during the configured Quiet Time hours even when notification
Quiet Time is Off. Equal start/end times disable scheduled sleep; an unset
clock leaves the pet awake.

Choose **Wake Up** to keep it awake for 30 minutes. This does not change
notification settings or override Low Power. Turning Pet Off, restarting or
leaving the sleep period clears the temporary wake choice. Low Power pauses
care; display sleep stops animation.

## Training games

Choose **Train**, read the instructions and press Enter to start. Zen selects
a game different from the previous one. One retry repeats the same puzzle.

| Game | Goal | Controls |
|---|---|---|
| Follow the Arrows | Remember and repeat three arrows | Joystick / arrow keys |
| Catch the Food | Catch three of five drops | Left/Right |
| Perfect Timing | Hit the highlighted zone three times | Enter |
| Find Your Pet | Track the pet through five box swaps | Left/Right, Enter |
| Which One Changed? | Select the changed geometric symbol | Left/Right, Enter |


Back cancels without cost or reward. Failed games also cost nothing.
Training costs, XP, Bond and rest time apply only after a win.

- **Follow the Arrows:** repeat the previewed sequence; a wrong direction ends
  the attempt.
- **Catch the Food:** move one lane at a time and catch three of five drops.
- **Perfect Timing:** press Enter as the marker reaches the target, three times.
- **Find Your Pet:** watch the box swaps, then choose and open a box.
- **Which One Changed?:** remember the shapes before they are covered, then
  select the changed one.

Training keeps the screen on. A 60-second inactivity limit starts when you
have control, not while watching a preview. Back, leaving the page, screen-off,
Pet Off, scheduled sleep or Low Power cancels training. Manual screen-off and
safety restrictions always take precedence.

Training games run locally and send no mesh traffic.
E-ink games use slower movement and longer previews. Games remain usable
without sound; effects follow notification mode, volume, Quiet Time and DND.

## Personality

Each pet is Playful, Calm, Curious or Stubborn; check Details to see its nature.
Personality changes expressions and remarks, not care costs, rewards or
progression. OLED pets wander and animate; E-ink pets use static poses.
Menus, games and notifications pause idle reactions.

A pet can look sleepy before bedtime, excited when ready to evolve, happy
after feeding or proud after a win. Charging recovery can also prompt a reaction;
it gives no fullness, energy, XP or Bond.

## Daily mesh rewards

Open **Care → Daily Rewards** to see progress; Enter opens details.

| Activity | Daily reward |
|---|---:|
| First DM acknowledged by its recipient | 20 XP |
| First channel message with a heard relay, or acknowledged room message | 15 XP shared |
| First two-way DM conversation with the same contact | 5 Bond |
| First interaction with a favourited contact or room | 3 Bond |


Bond milestones overlap: a favourite interaction earns 3, then a two-way
conversation adds 2. If the conversation happens first, it earns all 5.
The daily maximum is **35 XP and 5 Bond**. Neither receiving broadcasts nor
queuing a send counts. Public and permitted private channels qualify.
Phone and Zen messages count equally; retries and repeated confirmations do not.
Favourite interaction means an acknowledged send or a new incoming message.
Room delivery means server acknowledgement, not confirmation from every guest.

Phone and Zen messages count equally. Sending must be acknowledged or a channel
relay heard; simply queuing a message or receiving a broadcast does not count.
Room acknowledgement confirms delivery to the server, not every guest.

Rewards follow Child Mode permissions. Quiet Time defers them until the pet
wakes. Low Power ignores new interactions but retains earned rewards. Turning
Pet Off discards pending rewards without renewing the day's allowance.

Allowances follow the local date once the clock is set, or a 24-hour uptime
window beforehand. Correcting the clock can delay the next renewal. Saved
pets may need to reach the next safe reward-day rollover before earning new
rewards; saved pending rewards can still apply.

## Pet alerts

| Event | Notification | Sound |
|---|---|---|
| Fullness reaches 25 or below | Pet: Hungry | Two descending notes |
| Fullness reaches 10 or below | Pet: Very hungry | Three descending notes |
| Mesh bonuses are applied | Pet: +20 XP, +3 Bond | Three rising notes |
| Evolution becomes available | Pet: Ready to evolve | Four ascending notes |
| Evolution is confirmed | Pet: Evolved to Bramble | Five celebratory notes |
| Training reward is applied | Won! | Three rising victory notes |
| Training attempt fails, including gameplay timeout | Lost! | Three descending notes |


Hunger alerts occur once per threshold until feeding raises fullness above 40.
Readiness is announced once per level. Bonuses that arrive together share one
alert. Cancelled training is silent; a Lost result keeps the retry available.

Popups use normal notification sound and screen-wake settings. Quiet Time
defers background alerts until the pet wakes; Low Power pauses them, and Pet
Off clears them. Messages and warnings take priority.

While a popup covers the care menu, Enter or Back dismisses it. Press Enter
again to use the selected action. Deliberately waking a pet during Quiet Time
allows games, but their results remain muted.
