# Zen UI developer guide

[Back to README](../../README.md)

Zen UI screens live in `examples/companion_radio/zen-overlay/app/ui-new/` and are included into
`UITask.cpp` as one translation unit. Include order matters; cross-translation
unit helpers belong in normal headers.

## Screen contract

Every screen implements `UIScreen`:

```cpp
int render(DisplayDriver& display);
bool handleInput(char key);
void poll();
void onShow();
void onHide();
```

`render()` draws one frame and returns the delay before another render.
`handleInput()` consumes keys, `poll()` performs bounded background work,
`onShow()` resets per-visit state and `onHide()` commits staged state or cancels
operations. `UITask` owns frame start/end and guarantees symmetric transitions.

To add a screen: declare its pointer in `UITask.h`, construct it in `begin()`,
add a navigator, then add its menu or carousel entry.

## Layout and lists

Use `DisplayDriver` metrics rather than fixed pixel values:

- `lineStep()`, `headerH()` and `listStart()` for vertical layout;
- `getTextWidth()` and `drawTextEllipsized()` for user text;
- `drawCenteredHeader()` or `drawInvertedHeader()` for headers;
- `drawList()` for selection, scrolling and scrollbar reservation.
- `drawListAt()` for lists embedded below a carousel top bar.
- `drawLabelValueRow()` for collision-safe label/value rows.

Normal screen titles use title case. All capitals are reserved for urgent
warnings or status text where emphasis is intentional.

Use `MessageTranscriptView` for DM, room and channel history. Shared components
also include `PopupMenu`, `KeyboardWidget`, `DigitEditor`, `FullscreenMsgView`
and `AccordionList`.

## Input

Handle `KEY_UP`, `KEY_DOWN`, `KEY_LEFT`, `KEY_RIGHT`, `KEY_ENTER`,
`KEY_CANCEL` and `KEY_CONTEXT_MENU`. Use `keyIsPrev()` and `keyIsNext()` for
editable values so rotary and directional inputs agree.

Use the same control contract throughout Zen:

- Up/Down moves a selection or scrolls content.
- Left/Right changes a value, tab, filter or carousel page.
- Enter opens, activates or confirms the primary action. It toggles Boolean
  rows, opens editors and previews melodies; it does not advance multi-choice
  rows.
- Hold Enter (`KEY_CONTEXT_MENU`, or Tab on CardKB) opens secondary actions or
  the related settings screen. It must never act as Back.
- Back cancels the active editor or dialog, or moves up one level. From a home
  page it returns to Clock; on Clock it turns the display off.

`UIFramework.h` contains the allocation-free presentation models shared by
screens. Use `MenuItem`/`MenuModel` for action lists, `SettingRow` for setting
metadata, `renderMenu()` or `renderSettings()` for standard rows, and
`moveWrapped()` for menu movement. Refresh delays should use the named
`Refresh` intents rather than new anonymous millisecond values.

Navigation into a screen that can be reached from more than one place must use
the `UITask` return stack. A screen should not infer its caller or add another
origin flag merely to decide where Back goes.

## Terminology

- **Favourite** is the contact, room or channel flag used for ordering and
  Child Mode permission.
- **Shortcut** is one of the four contacts pinned to the Favourites home card.
- **Muted** is a per-source notification choice; **Silent** is the temporary
  global DND state; **Quiet Time** is scheduled sound suppression.
- **Messages** is the home category; **Quick Replies** are prepared message
  texts. Avoid the older Quick Message name in user-facing text.

Menus and short pickers wrap using `wrapSelection()`. Long content, transcripts,
telemetry and editor cursors clamp at their ends. Read-only full-screen views
close with Back and ignore Enter and Hold Enter.

Back is the only physical screen-wake key. E-ink builds capture button edges
during panel refresh and replay queued input before one redraw.

## Text and emoji

`KeyboardWidget` provides UTF-8-safe ABC, predictive T9, completion,
placeholders and the message emoji picker. Use `expandMsg()` at send time and
`kbAddSensorPlaceholders()` for available sensor tokens. Do not implement a
second text editor or wrapper.

Display text accepts UTF-8 directly. Unsupported emoji use the shared diamond
fallback; glyph data and overrides are centralised under `src/helpers/ui/`.

## Persistence and policy

MeshCore settings remain in the upstream persistence path. Zen preferences use
the independent Zen preference codec in `zen/ZenPrefsCodec.h`
for stored-data compatibility.

Multi-field screens stage values and call `savePrefsIfDirty()` on exit. Compare
the final value with the entry snapshot so changing a value and changing it back
does not write flash. Immediate one-shot actions may save directly when their
contract requires it.

Rendering must not write preferences or start transport operations. Detect
maintenance changes before rendering and queue them with `requestPrefsSave()`.

Use the shared Child Mode, Quiet Time, notification, room-login, advert privacy,
GPS scheduling and message-delivery helpers. UI screens should present policy,
not duplicate it.

## Remote node operations

On-device Admin, room login and sensor telemetry reserve the single
`zen/RemoteNodeCoordinator.h` instance owned by `UITask`. Its
`RemoteNodeOperation` owns the allocation-free route state, attempt count,
wrap-safe deadline and terminal result. `RemoteLoginAdapter`,
`RemoteTelemetryAdapter` and `RemoteAdminAdapter` isolate protocol-specific
matching and retry safety while packet encoding remains in `MyMesh`.

A known path receives its initial transmission and one path retry before Zen
clears it and makes three flood attempts. A request without a known path makes
three flood attempts in total. Replies must match the full node identity and,
where the protocol provides one, the telemetry or Admin request tag. Late and
unrelated replies cannot complete a newer operation.

Only idempotent operations may use automatic retries. Login, telemetry, Admin
reads and setting read-back verification are retry-safe. Admin writes, console
commands and actions are sent once; a timeout is reported as an unknown result
because the remote node may have applied the command even though its reply was
lost. `MyMesh` does not own a second UI telemetry deadline. Do not introduce a
screen-local retry loop, pending transport or deadline for these flows.

Visible waits show the attempt number and whether the active transmission uses
the saved path or flood. Every attempt and terminal result is added to the
RAM-only diagnostic log with the four-byte target prefix. Routine attempts do
not produce warning popups; terminal handlers retain the existing user-facing
failure notification.

## Messaging and delivery

Messaging is divided by responsibility rather than by screen:

- `ConversationStore.h` owns the bounded channel and direct
  message rings. It contains no navigation or rendering state.
- `MessageDeliveryCoordinator.h` owns direct-message ACK state, retry
  transitions and channel relay-echo state.
- `MessageTransportAdapter.h` is the single boundary used by the standalone UI
  for direct and channel radio sends. Phone and Bluetooth flows remain in
  the MeshCore transport path.
- `MessageSendCoordinator.h` constructs initial on-device sends and their ACK,
  deadline and route metadata. Screens select and authorize recipients but do
  not assemble transport state.
- `MessageUnreadCoordinator.h` owns RAM-only direct and room unread identities
  and counters. Counts are clamped and reconciled against retained history.
- `MessageSendState.h` carries one compose/send result between queueing and
  transcript insertion; `MessageDraftStore.h` owns RAM-only per-conversation
  drafts; `MessageTranscriptView.h` owns transcript layout and scrolling.

The direct-message policy is one initial send plus one retry on a known path,
then path clearing and three flood attempts. With no known path it is one
initial flood plus two retries. ACK tags from every attempt remain valid so a
late ACK completes the message. Channel delivery is observational: a sent row
is pending until one or more relay echoes are heard, or failed when the relay
window expires without an echo.

Unread state changes only when a permitted message is received while its
transcript is not visibly open. Direct and room counters are keyed by the
four-byte conversation prefix; channel counters remain beside the channel ring
because eviction must adjust them atomically. All messaging state is RAM-only
and must not introduce flash writes.

## Notifications

`NotificationProfiles.h` defines the outputs associated with every event class;
callers attach text and identity rather than rebuilding those flags.
`NotificationCoordinator.h` receives one allocation-free `NotificationEvent`
and a snapshot of runtime `NotificationContext`. The event carries its own
contact prefix or channel index, so sound selection never depends on mutable
state left behind by an earlier event. The coordinator returns independent
decisions for unread recording, popup presentation, screen wake, sound and
vibration.

`NotificationEligibility.h` owns the pure Child Mode contact, channel and
advert rules; `UITask` only resolves the corresponding MeshCore record. An ineligible event
produces no unread count or local output. Silent and Quiet Time suppress sound
and vibration without suppressing eligible popups, unread state or an allowed
screen wake. Emergency Mode restores communication availability but does not
override notification preferences. Explicit ringtone previews remain outside
this policy and may play while notifications are silent.

`NotificationWakeController` owns the five-second timeout only when a
notification turned on a sleeping display. An already-on display keeps its
normal timeout, and the first physical interaction transfers ownership back to
the user. `NotificationPopupState` prevents routine alerts from replacing an
active warning or error. Diagnostic warnings and errors are always logged; their event profile
controls popup priority and configured wake behaviour. Low Battery uses the
same path once, rather than separately invoking diagnostics, popup, wake and
sound handlers.

`SoundNotifier` is an event-based output adapter only. It chooses the configured melody
after the coordinator permits playback. Do not mirror Notification mode in the
buzzer driver's quiet flag or store sender/channel context in `UITask`.

GPS acquisition acceptance and failure backoff belong in
`src/helpers/sensors/GpsPollingPolicy.h`. Live and between-poll direction state
belongs in `zen/GpsCourse.h`. Both models are RAM-only; display code reads their
state and must not create a second course or polling policy.

UTC remains the stored time source. Local-time consumers must use
`zen/TimezonePolicy.h` rather than applying their own offsets or seasonal
rules. `TimezoneEditor.h` owns the City/Fixed UTC presentation.

## Power

`PeripheralPowerCoordinator.h` is the allocation-free policy owner for display,
CardKB polling, GPS, Bluetooth, radio and low-power brightness. It keeps saved
requests, RAM-only session requests, restrictions, effective state and last
applied state distinct. `UITask::reconcilePower()` is the only runtime hardware
transition adapter; feature code changes requests or restrictions and asks it
to reconcile.

Child Mode always inhibits Bluetooth. Low Power inhibits GPS, Bluetooth and
radio, while its ten-minute Emergency window restores the radio and permits
explicit RAM-only GPS and Bluetooth requests. Boot-time synchronization is a
temporary GPS hardware claim and cannot override Low Power. Display sleep
suspends CardKB polling before blanking the panel; CardKB remains available in
Low Power whenever the display is awake. These transitions never persist
preferences.

Zen display builds do not activate GPS from the generic startup path. They
configure cadence and activate the receiver through the coordinator before
starting the temporary time-sync claim. Low Power blocks new GPS retries but
does not block observation of an authoritative time update, so a completed
sync cannot leave a stale claim to run when normal power returns.

The GPS polling and adaptive-backoff algorithms remain in
`EnvironmentSensorManager`; the coordinator gates their configured policy but
does not duplicate their scheduling. `MyMesh::applyPowerState()` atomically
applies the radio's low-power/emergency context while retaining MeshCore's
normal radio configuration and receive-power behaviour.

Return long render intervals for static screens. Keep background `poll()` work
bounded and avoid flash writes when values have not changed. E-ink code should
coalesce input and redraws wherever possible.

Use `UI_REFRESH_STATIC_MS` for event-driven screens and
`UI_REFRESH_ACTIVE_MS` only while visible time-dependent progress is changing.
The shared marquee scheduler can request an earlier frame when required.
