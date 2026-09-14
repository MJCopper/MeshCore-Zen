# Zen UI developer guide

[Back to README](../../README.md)

Zen UI screens live in `examples/companion_radio/ui-new/` and are included into
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
the versioned sidecar in `solo/SoloPrefsCodec.h`; its internal name is retained
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

GPS acquisition acceptance and failure backoff belong in
`src/helpers/sensors/GpsPollingPolicy.h`. Live and between-poll direction state
belongs in `solo/GpsCourse.h`. Both models are RAM-only; display code reads their
state and must not create a second course or polling policy.

UTC remains the stored time source. Local-time consumers must use
`solo/TimezonePolicy.h` rather than applying their own offsets or seasonal
rules. `TimezoneEditor.h` owns the City/Fixed UTC presentation.

## Power

Return long render intervals for static screens. Keep background `poll()` work
bounded, suspend peripheral polling while the display sleeps, and avoid flash
writes when values have not changed. E-ink code should coalesce input and
redraws wherever possible.

Use `UI_REFRESH_STATIC_MS` for event-driven screens and
`UI_REFRESH_ACTIVE_MS` only while visible time-dependent progress is changing.
The shared marquee scheduler can request an earlier frame when required.
