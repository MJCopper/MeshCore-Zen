# Virtual Pet

Enable **Settings → System → Zen Pet** to show the pet page after Favourites.
The default is Off. Only this switch is saved: reboot, reset or power loss
ends the pet's life and creates a new starter next time it is enabled.
Turning Off and On during the same boot preserves the pet.

Press Enter for Feed, Train, Evolve and Details. Hold Enter opens Details.
Up/Down selects an action; Back closes the menu, then returns to Clock.
Left/Right changes carousel pages. CardKB Enter and Escape work normally.

Sprout has twelve levels and 189 named forms, ending in 64 possible final forms.
Entering an even level offers two evolution choices; entering an odd level
offers one. Every evolution is explicitly confirmed, with a name and preview.
Leafy and winged lineages gain ears, shoulders, feet and markings as they grow
from 16 to 32 pixels. Details shows the requirements for the next evolution.

| Current level | XP needed for next level | Bond needed |
|---|---:|---:|
| 1 | 160 | 20 |
| 2 | 280 | 30 |
| 3 | 640 | 40 |
| 4 | 1,190 | 50 |
| 5 | 2,210 | 60 |
| 6 | 4,170 | 70 |
| 7 | 7,530 | 80 |
| 8 | 12,570 | 90 |
| 9 | 19,570 | 95 |
| 10 | 28,810 | 98 |
| 11 | 41,050 | 100 |

XP is cumulative, not spent on evolution. Level 12 is the final form.
The target is about 60 days without mesh bonuses: the host simulation sleeps
22:00–06:00, feeds regularly, wins training while retaining at least 25 fullness,
and evolves promptly. It reaches level 12 after 431 wins. Approximate cumulative
days for levels 2–12 are 0.25, 0.9, 1.9, 3, 5.9, 9.9, 15.9, 23.9, 33.9, 45.9
and 60. This is a care-based estimate, not a minimum-age requirement; bonuses,
training habits, missed care and Low Power change it. Reboot still starts over.

Level 10 Heart forms mature into Wardens; Spirit forms become Sages at level 11.
Level 12 offers Titan/Crown or Oracle/Astral respectively, keeping the inherited
Oak, Ash, Pine, Elm, Leaf, Bloom, Rush, Lily, Flame, Coal, Light, Star, Rain, Snow,
Wind or Sky prefix. For example: Oakheart → Oakwarden → Oaktitan or Oakcrown.
The 96 new forms have individual 16×16 portraits and compact 8×8 choice previews,
scaled within the existing page. Earlier forms retain their graphics.
Feeding consumes one food, restores 25 fullness and adds 5 bond.
Training consumes 20 energy/10 fullness and adds 2 bond, at most once every
five minutes. Attempting training during rest shows the remaining time rounded
up to whole minutes, such as **Rest 2 minutes** or **Rest 1 minute**. It is
calculated when attempted, not continuously refreshed. Pet Off and Low Power
pause rest time. Its XP reward starts at 20 and increases by 10 per level;
progressively larger evolution thresholds keep later levels more demanding.
Costs, XP, Bond and cooldown apply only after winning a training minigame.
Food replenishes every four enabled hours, up to five.
Energy recovers 10 per hour, or 20 while sleeping. Fullness falls 5 per awake hour.

Low battery increases passive hunger: 50–100% loses 5 fullness per awake hour,
25–49% loses 6, 10–24% loses 7, and below 10% loses 8. Recovery requires two
percentage points above a band boundary to prevent fluctuating readings changing
the rate repeatedly. Unknown readings use 5. Charging reduces the rate as battery
recovers, but does not feed the pet. In Details, Down shows **Charge to reduce
hunger** when the rate is increased; Up returns to the first rows.
Sleep and Low Power still pause hunger, including emergency operation. Training
costs and food replenishment are unchanged. Existing hunger alerts apply normally;
there are no extra battery alerts, hardware samples, timers or flash writes.
The 60-day estimate assumes the normal rate; prolonged low battery slows progress.
With the same reference care routine and a fixed rate throughout, host simulations
reach level 12 in about 60, 78, 105 and 106 days at rates 5, 6, 7 and 8 respectively.
Real charging cycles vary the rate, so these are comparisons rather than forecasts.
`PetBatteryPolicy` translates the existing cached battery percentage; the engine
accounts elapsed time at its previous rate before applying a new rate.

The pet sleeps during the configured Quiet Time hours, even when notification
Quiet Time is Off. Before time synchronization it stays awake; identical start
and end times disable scheduled sleep. Local timezone and DST apply.
While sleeping, select **Wake Up** in the care menu to wake it for 30 minutes.
It resumes sleep if still within the schedule. This RAM-only override clears on
reboot, Pet Off or leaving the sleep period; it does not change notifications or
override Low Power. Feeding, training and deferred rewards work normally while awake.
Low Power pauses it; display sleep stops animation.
The pet never sends mesh packets or saves gameplay. Its alerts use Zen's normal
sound and screen-wake policy.
Care timers use runtime elapsed time, independent of GPS, timezone and clock sync.

## Training games

Select **Train** for one randomly chosen game. Enter starts after the instructions.
Each Train selection randomly chooses from all games except the previous one.
Cancelling and reopening selects a different game. Each session allows one
explicit retry with the same game and puzzle; selection history is RAM-only.

| Game | Goal | Controls |
|---|---|---|
| Follow the Arrows | Remember and repeat three arrows | Joystick / arrow keys |
| Catch the Food | Catch three of five drops | Left/Right |
| Perfect Timing | Hit the highlighted zone three times | Enter |
| Find Your Pet | Track the pet through five box swaps | Left/Right, Enter |
| Which One Changed? | Select the changed geometric symbol | Left/Right, Enter |

Back cancels without cost or reward. Failed games also cost nothing.
Find Your Pet reveals the pet for three seconds, closes the boxes for one second,
then highlights each of five swap pairs without arrows. OLED uses short
sliding swaps; E-INK uses discrete frames, with pauses between swaps. Left/Right
stops at the edges and highlights the chosen box. Enter opens it and reveals the
pet's actual location before Won/Lost. Retry repeats the original hiding place
and swaps. Leaving the game or pressing Back cancels without cost, including
during the answer reveal; rewards apply only after the reveal completes.
Follow the Arrows replaces each empty box with the correctly entered arrow.
Completed arrows stay visible; unanswered boxes stay empty. A wrong input fails
the attempt. Retry clears the entered arrows and repeats the same sequence.
Which One Changed? shows the original shapes, covers each with a solid white
square for three seconds, then reveals one changed shape. Left/Right and Enter
are ignored while covered; Back still cancels. Both displays use the same
three-second cover, and retries retain the original shapes and change.
Catch the Food has five evenly spaced lanes and starts in the centre. Left/Right
moves one lane and stops at the edges. Catch three of five random drops to win;
retries retain the drop sequence. Drop speed remains slower on E-INK.
Perfect Timing uses target widths of 5, 3 and 2 positions over its three turns.
Each target has a different random placement. Targets remain fixed during a
turn and repeat on retry. Marker speed stays unchanged; each turn starts outside
its target to prevent repeated Enter presses from winning.
Training requirements are checked before starting and again before rewards.
Scheduled pet sleep (unless temporarily woken), Low Power, Pet Off, screen sleep,
leaving the page or an interrupting
popup cancels the game. The normal display timeout still applies; games do not
keep the display awake. CardKB uses the existing mapped arrow/Enter/Escape keys.

OLED movement is discrete and modest-rate. E-INK uses slower steps and longer
memory previews. Memory/puzzle state advances only for input or phase changes;
animated games update only while visible. Normal carousel status refreshes remain
in place. Games use local pseudorandom state,
not MeshCore's radio RNG, and add no packets, flash writes or background timers.

Short game effects follow Notification mode, Silent/DND, Quiet Time and volume.
Arrows have direction tones during preview and correct-input ticks; Food has
catch/miss tones; Timing has hit pings; Find Your Pet has reveal/swap cues; Changed
Shape has reveal/cover cues. Won! and Lost! retain their result melodies without
duplicate final-step sounds. Gameplay remains usable muted. `PetGameAudio` maps
model cues to sound-only events: no popups, screen wakes, or background timers.
Busy audio or a notification drops an effect instead of queuing stale playback.

## Personality

On OLED, an awake pet wanders in one-pixel steps every randomly chosen
0.5–1.5 seconds, within four pixels horizontally and two vertically of its
resting position. Available space reduces these limits for larger forms;
portrait size takes priority. Its face and attached expressions move with it;
speech bubbles and statistics stay still. Movement never drifts outside its bounds.
Menus, games, notifications, screen sleep and Low Power stop the shuffle and
return it to centre. E-ink stays static. Movement is RAM-only and never wakes
the screen or extends its timeout.

Each new pet is randomly Playful, Calm, Curious or Stubborn. Its nature appears
in Details (Up/Down scrolls) and persists through evolution and same-boot Off/On.
Personality is cosmetic: care costs, cooldown, XP, Bond, game selection and
evolution requirements do not change. Reboot creates a new pet and personality.

Feeding produces a happy expression, winning a proud one and losing a brief sulk.
The pet looks sleepy in the 15 minutes before its configured bedtime and excited
when ready to evolve. Bedtime follows the pet's existing local-time schedule,
including DST, even if notification Quiet Time is Off. Unsynchronized time,
identical sleep endpoints and a Wake Up override disable bedtime anticipation.
Actual sleep and Low Power take precedence over cosmetic reactions.

Short, temperament-specific phrases appear in a speech bubble only on the
ordinary pet page, never in care menus, games or notifications. Bubbles last four
seconds, are spaced at least 30 seconds apart and avoid immediate repeats.
Idle remarks require two minutes of visible page time. A greeting requires five
minutes away; navigating care menus does not count. Action reactions expire on
their original deadline even if a menu or notification obscures them—there is
no delayed queue. Existing Won!/Lost! popups and sounds remain unchanged.

Battery recovery gives a relaxed or delighted expression. It requires cached
external power and a rise of at least three percentage points sustained across
distinct battery samples for two minutes. Further reactions require another
ten-point rise and at least ten minutes. Unknown readings or disconnection reset
the observer; voltage rebound without external power does not qualify. A powered
connection is not proof of active charging. Recovery gives no fullness, energy,
XP or Bond and does not change the battery hunger policy.

While visible, the pet occasionally blinks, looks around, tilts, hops or stretches.
Temperament weights these quirks. OLED uses a few bounded frames, no faster than
the existing 500 ms animation cadence; E-INK uses static poses, with personality
redraws at least five seconds apart. OLED shuffle redraws follow their random
deadlines; when movement is suppressed, ordinary redraws slow to five seconds.
Speech and movement stay inside the portrait area. Opaque one- or two-line
bubbles overlay its top for four seconds without shrinking the pet or changing
movement bounds. They may cover part of the pet; the full portrait returns when
they expire. Text that cannot fit is omitted rather than covering statistics.

Menus, training and notifications suppress quirks and bubbles. Display sleep,
Pet Off, leaving the page, scheduled sleep and Low Power clear transient activity.
There is no catch-up animation, screen wake, extra battery sampling, flash write,
mesh traffic or background timer. `PetPersonality`, `PetPersonalityAssets`,
`PetPortraitLayout` and `PetPersonalityView` separate state, constant
phrases/expressions, safe geometry and rendering.
Their local PRNG is independent of both training and MeshCore randomness.

## Daily mesh rewards

Open **Care → Daily Rewards** to see progress. Enter opens details; Back returns.
The five care actions scroll when they do not all fit on screen.

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

Child Mode applies its existing contact, room and channel permissions.
Quiet Time reserves rewards until the pet wakes, including across midnight.
Low Power ignores new interactions but retains rewards already earned.
Turning the pet Off discards pending rewards and partial conversations without
renewing the daily allowance. Gameplay and reward tracking remain RAM-only.

Once time is synchronized, allowances follow the configured local date.
Before synchronization they use a rolling 24-hour uptime window.
Synchronization and clock/timezone corrections do not directly renew allowances;
after a correction, a 20-hour guard prevents repeated resets. This can delay
one renewal. Details shows whether local time or uptime is in use.

Tracking uses fixed-size tables: eight conversation contacts, sixteen outgoing
attempts and eight recent incoming fingerprints. Heavy traffic can evict old
evidence and miss a reward, but never changes message delivery or retries.
Reward tracking adds no radio transmissions, flash writes or timers.

## Pet alerts

Alerts are active whenever Zen Pet is On, with no additional saved preference.
Care-menu labels stay fixed. Feeding, Wake Up, unavailable actions and manual
training cancellation use soundless popups, retaining the menu and selection.
While a notification covers the care menu or its detail views, Enter/Back only
dismisses it. Other navigation is ignored until dismissal or automatic expiry.
Queued keys from the dismissal burst are discarded; a fresh Enter activates the
same selected option. Training keeps its normal controls and cancellation rules.
Sleeping and Low Power refusals remain visible. Training wins/losses and confirmed
evolutions use their existing dedicated alerts without duplicate action popups.
They use the existing notification mode, Silent/DND, screen-wake and volume
settings. Notifications Off mutes sound; popups still appear normally.

| Event | Notification | Sound |
|---|---|---|
| Fullness reaches 25 or below | Pet: Hungry | Two descending notes |
| Fullness reaches 10 or below | Pet: Very hungry | Three descending notes |
| Mesh bonuses are applied | Pet: +20 XP, +3 Bond | Three rising notes |
| Evolution becomes available | Pet: Ready to evolve | Four ascending notes |
| Evolution is confirmed | Pet: Evolved to Bramble | Five celebratory notes |
| Training reward is applied | Won! | Three rising victory notes |
| Training attempt fails, including gameplay timeout | Lost! | Three descending notes |

XP and Bond bonuses combine into one alert and show only points actually added.
When a bonus unlocks evolution, the combined alert uses the ready melody.
Feeding keeps its visual feedback without a reward alert. Each training attempt
reports Won or Lost once; cancellation is silent. A Lost popup preserves the retry
screen. Training results use the shared notification policies and RAM-only queue.
If manually woken during Quiet Time, training results still display, but their
sound remains muted. Background pet alerts remain deferred.

Hunger alerts occur once per threshold until feeding raises fullness above 40.
Very hungry replaces an unshown Hungry alert. Stale hunger/readiness alerts
are cancelled before presentation. Readiness is announced once per level.
Confirmed evolutions are queued, so quick successive evolutions are not lost.

Quiet Time defers alerts until the pet wakes. Low Power pauses presentation.
Turning Pet Off clears pending alerts. Pet popups wait for existing alerts to
clear, are lower priority than messages/warnings, and are spaced five seconds
apart. Notification-only screen wakes still last five seconds.
Alert tracking remains RAM-only and uses the existing pet update, not a new timer.

The optional implementation lives in `zen-overlay/app/zen/pet/` and is excluded
with `ZEN_FEATURE_PET=0`. OLED uses idle/reaction poses; E-ink uses static poses.
Evolution data, graphics, care logic and presentation are separate modules.
`PetMatureAssets` contains only constant portraits and previews; the care
simulation is host-test-only and adds no firmware timers or saved state.
`PetTrainingGames` owns only game state; `PetTrainingView` draws it. `PetEngine`
remains responsible for training eligibility, costs, rewards and cooldown.
`PetMeshRewards`, `PetRewardDay` and `PetRewardsView` isolate rewards, day handling
and presentation from MeshCore and the ordinary Zen messaging UI.
`PetNotifications` collects pet events and holds its short immutable melodies;
Zen's shared notification coordinator and sound dispatcher present them.
There are no pet-to-pet battles or pet protocol packets.
