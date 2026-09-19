# Messages

[Back to README](../../../README.md)

The Messages home page opens directly to **Direct Message**, **Channel** and
**Room Servers**, with unread badges. Hold Enter on a category to mark it read.

## Conversations

Select a recipient to open its full transcript. Sender names are inverted,
messages are separated by a rule, and Up/Down scroll one wrapped line at a time.
Messages sent from a connected companion app are added to the same direct,
channel and room histories and use the same delivery markers.

- **Enter** composes a message.
- **Hold Enter** opens quick replies and transcript actions.
- In channels and rooms, **Reply to…** selects one of the six latest senders and
  inserts `@[name] `.
- A failed latest message offers **Resend failed**; channels offer
  **Resend anyway** when no echo was heard.

Unread messages are cleared only when their transcript is visible while the
display is awake. When Screen Wake permits a message to wake
the display, it turns off again after five seconds unless a Tracker button is
pressed.

## Delivery markers

Direct and room messages show `D`, `P` or `F` for direct, stored path or flood.
The number is the transmission count; a tick confirms delivery and `✗` marks an
exhausted send. Channels show the number of matching repeater echoes, such as
`2✓`, or `✗` when none are heard during the response window.

With a known path, Zen tries it twice, clears it, then makes up to three flood
attempts. Without a path it makes up to three flood attempts. An ACK stops the
sequence immediately.

Hold Enter on a saved direct-message contact and choose **Path details** to
inspect its current learned route, hash width and latest send result. The
route and attempt summary are RAM-only; a path that received no ACK remains
summarised after Zen falls back to flooding. Unknown or ambiguous hop hashes
remain in hex, while unique matches to saved contacts show their names. This
is not a live reachability test and sends no radio request.

## Text entry

The default on-screen layout is predictive T9; ABC is selectable under
**Settings › Keyboard**. Message fields provide:

- a 4,000-word Australianised completion dictionary;
- cursor-safe UTF-8 editing and encoded-length enforcement;
- up to twelve completion choices, accepted with a trailing space;
- previous-word ranking covering 2,000 common words and eight likely followers;
- high-confidence chat phrase prediction, contraction handling and ranked
  sentence openings;
- placeholders for time, GPS and active sensors;
- automatic sentence capitalisation, including after a reply mention;
- an emoji picker for 👍, 👎, 🙂 and 🙁.

Predictive T9 includes the common single-letter words `a` and `I` as the first
choices for keys 2 and 4 respectively.

Hold Enter opens word alternatives. In predictive T9, Back accepts the current
word and inserts a space; double-pressing Back accepts it with a full stop and
one space. Double Back exits only when no predictive word is active; if the
digits have no match, it opens the alternatives instead. A prediction shown
after backspacing into an existing word follows the same Back behaviour.
CardKB uses direct input, Tab for completion and Fn+M for emoji.

Leaving a partly written custom message keeps it as a RAM-only draft for that
contact, room or channel. Reopening the same transcript restores the draft.
Drafts are cleared when submitted and are lost when the device restarts.

## Rooms

Opening a room starts its login when required. Enter a password, or submit an
empty field for an open or ACL-controlled room. Successful passwords are saved
and reused after restart. A rejection discards the rejected password; a timeout
keeps it because the server may simply be unreachable.

For an ACL-only blank login with no reply, Zen opens the transcript after the
normal response window. The room server remains authoritative when a message is
sent. Hold Enter on a room to log in again or log out.

## Lists and context actions

Contact and channel menus provide read state, notification and melody controls.
Notification choices are Default, Off and Local. Off suppresses that source's
local alert without discarding messages or unread counts. Local permits an
alert even when Auto has a connected phone or USB client. Global Notifications
Off suppresses the alert; Quiet Time and DND suppress its sound but retain any
permitted visual alert and screen wake. Melody previews play on request even
while notifications are silent.
Contacts, rooms and channels can be starred and are sorted first in their lists.
Contacts can separately be pinned to the four-slot Favourites Dial. Channels can
also be edited or deleted, and the Channels list can add Public, hashtag or
private channels.

Child Mode limits these lists to permitted favourites and blocks editing.
