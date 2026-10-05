#pragma once

namespace zen { namespace pet {

// A toast covering the care menu owns input until dismissed or expired.
// Training retains its own input/cancellation rules, including the retry view.
struct PetPopupInput {
  enum Action { PASS, BLOCK, DISMISS };
  static Action action(bool menu, bool training, bool popup, char key,
                       char enter, char back) {
    if (!menu || training || !popup) return PASS;
    return key == enter || key == back ? DISMISS : BLOCK;
  }
};

} }
