#pragma once

#include <helpers/ui/UIScreen.h>
#include "ZenDisplayDriver.h"

#define KEY_LEFT           0xB4
#define KEY_UP             0xB5
#define KEY_DOWN           0xB6
#define KEY_RIGHT          0xB7
#define KEY_SELECT           10
#define KEY_ENTER            13
#define KEY_CANCEL           27   // Esc
#define KEY_HOME           0xF0
#define KEY_NEXT           0xF1
#define KEY_PREV           0xF2
#define KEY_CONTEXT_MENU   0xF3
#define KEY_DOUBLE_CANCEL  0xF4
// A literal-ASCII input source's (e.g. an I2C CardKB) Enter, translated only
// when the on-screen keyboard's plain grid state is active (see
// UITask::pollCardKB()) -- means "submit the field", not KEY_ENTER's usual
// "commit whatever grid cell (row,col) is currently selected", which would
// otherwise insert a stray character since direct typing never touches the
// grid.
#define KEY_KB_ENTER       0x05

// "Previous"/"Next" navigation keys: the directional key and its rotary-encoder
// twin. Decrement / increment a value, or step a selection. (Cancel and the
// context-menu key stay screen-specific — they mean different things per screen.)
static inline bool keyIsPrev(char c) { return c == KEY_LEFT  || c == KEY_PREV; }
static inline bool keyIsNext(char c) { return c == KEY_RIGHT || c == KEY_NEXT; }

// Menus wrap; content views and editor cursors clamp. Keeping menu movement in
// one helper prevents subtly different Up/Down behaviour on short lists.
static inline int wrapSelection(int current, int count, int delta) {
  if (count <= 0) return 0;
  int next = (current + delta) % count;
  return next < 0 ? next + count : next;
}

// Input and asynchronous UI callbacks invalidate the frame immediately. Static
// screens therefore need only a slow safety refresh; active progress views use
// the shorter cadence while waiting on time-dependent state.
static const int UI_REFRESH_STATIC_MS = 30000;
static const int UI_REFRESH_ACTIVE_MS = 500;

#ifndef JOYSTICK_ROTATION
  #define JOYSTICK_ROTATION 0
#endif

// Rotate directional key by rot steps clockwise (0–3).
// Pass NodePrefs::joystick_rotation at runtime; JOYSTICK_ROTATION macro is the compile-time default.
static inline char rotateJoystickKey(char key, uint8_t rot) {
  switch (rot & 3) {
    case 1:
      if ((uint8_t)key == KEY_UP)    return KEY_RIGHT;
      if ((uint8_t)key == KEY_RIGHT) return KEY_DOWN;
      if ((uint8_t)key == KEY_DOWN)  return KEY_LEFT;
      if ((uint8_t)key == KEY_LEFT)  return KEY_UP;
      break;
    case 2:
      if ((uint8_t)key == KEY_UP)    return KEY_DOWN;
      if ((uint8_t)key == KEY_DOWN)  return KEY_UP;
      if ((uint8_t)key == KEY_LEFT)  return KEY_RIGHT;
      if ((uint8_t)key == KEY_RIGHT) return KEY_LEFT;
      break;
    case 3:
      if ((uint8_t)key == KEY_UP)    return KEY_LEFT;
      if ((uint8_t)key == KEY_LEFT)  return KEY_DOWN;
      if ((uint8_t)key == KEY_DOWN)  return KEY_RIGHT;
      if ((uint8_t)key == KEY_RIGHT) return KEY_UP;
      break;
  }
  return key;
}

// Zen screens add lifecycle callbacks and render against Zen's extended
// display API while remaining valid MeshCore screens.
class ZenUIScreen : public UIScreen {
protected:
  ZenUIScreen() { }
public:
  virtual int render(ZenDisplayDriver& display) =0;   // return value is number of millis until next render
  int render(DisplayDriver& display) final {
    return render(static_cast<ZenDisplayDriver&>(display));
  }
  virtual bool handleInput(char c) { return false; }
  virtual void poll() { }
  // Called by UITask::setCurrScreen() each time this screen becomes current —
  // the place to reset per-visit state (selection, dirty flag, sub-views).
  // Default no-op for screens that keep state across visits. Because it's
  // invoked centrally, a new screen can't "forget" to be reset on show.
  virtual void onShow() { }
  // Called before another screen becomes current. Use this for staged writes,
  // sensitive-editor cleanup and cancellation; render() remains side-effect free.
  virtual void onHide() { }
};
