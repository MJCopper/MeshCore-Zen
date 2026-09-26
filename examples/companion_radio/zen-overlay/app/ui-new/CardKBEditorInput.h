#pragma once

#include <helpers/ui/CardKBController.h>
#include <helpers/ui/ZenUIScreen.h>

// Stateless translation from CardKB semantic events into editor/UI actions.
// Transport, polling and power stay in CardKBController; UITask only applies
// the returned action to the active UI context.
namespace cardkbinput {

enum Kind : uint8_t {
  IGNORE,
  FORWARD_KEY,
  MOVE_CURSOR,
  OPEN_SUGGESTIONS,
  OPEN_EMOJI
};

struct Translation {
  Kind kind;
  char key;
  Translation(Kind action = IGNORE, char value = 0) : kind(action), key(value) {}
};

inline Translation translate(const CardKBController::Event& event,
                             bool compact_plain_editor) {
  if (event.type == CardKBController::SUBMIT) return { FORWARD_KEY, KEY_KB_ENTER };
  if (event.type == CardKBController::HOLD) {
    return compact_plain_editor ? Translation{ OPEN_SUGGESTIONS, 0 }
                                : Translation{ FORWARD_KEY, KEY_CONTEXT_MENU };
  }
  if (event.type == CardKBController::EMOJI) return { OPEN_EMOJI, 0 };
  if (event.type != CardKBController::KEY) return {};

  char key = event.key;
  if (compact_plain_editor &&
      (key == KEY_LEFT || key == KEY_UP || key == KEY_DOWN || key == KEY_RIGHT))
    return { MOVE_CURSOR, key };
  if (compact_plain_editor && key == KEY_ENTER) key = KEY_KB_ENTER;
  return { FORWARD_KEY, key };
}

} // namespace cardkbinput
