# Node Administration

[Back to README](../../../README.md)

Open **Tools › Node List**, select a saved repeater, room or sensor, hold Enter
and choose **Admin**. Zen first attempts empty-password ACL login, then offers
password entry. Credentials remain in RAM and are cleared on exit.

Sections depend on node type and firmware support. They include Status,
Settings, Radio, Routing, Room, Console and Actions. Opening an editable setting
reads its value. Finish with Enter or Back and confirm the change. Unchanged
values are not sent; writable settings report **Setting saved** only after a
matching read-back.

The Neighbours view resolves known key prefixes to contact names. The selected
name scrolls while age and one-decimal SNR remain fixed at the right.

For a saved node, **Path details** in the Node List's Hold Enter menu shows
its learned route and latest local send result without probing or saving it.
Hop names appear only when their hash matches one saved contact uniquely.

Radio changes affect the remote node and may make it unreachable. Actions and
console commands require confirmation and are not automatically retried.
**Result unknown** means the command may have run even though its reply was
lost. Login, status reads and setting verification use the shared known-path to
flood retry lifecycle. Writes and actions are sent once to prevent duplicate
side effects. Admin is unavailable while Child Mode is locked.
