#pragma once

#include <Arduino.h>

#define BUTTON_EVENT_NONE        0
#define BUTTON_EVENT_CLICK       1
#define BUTTON_EVENT_LONG_PRESS  2
#define BUTTON_EVENT_DOUBLE_CLICK 3
#define BUTTON_EVENT_TRIPLE_CLICK 4

class ZenMomentaryButton {
  int8_t _pin;
  int8_t prev, cancel;
  bool _reverse, _pull;
  int _long_millis;
  int _threshold;  // analog mode
  unsigned long down_at;
  uint8_t _click_count;
  unsigned long _last_click_time;
  int _multi_click_window;
  bool _pending_click;

  bool isPressed(int level) const;
  void applyTransition(int btn, unsigned long at);

#ifdef BUTTON_USE_INTERRUPTS
  // GPIO-IRQ edge capture: boards whose display blocks the main loop for a
  // long time per refresh (e-ink) can miss an entire press+release cycle if
  // check() only ever samples the live pin. The ISR latches edges with their
  // own timestamp into a small ring buffer; check() replays them so no tap
  // gets lost while the loop is stuck waiting on the panel.
  int8_t _isr_slot = -1;
  static const uint8_t EDGE_BUF_SIZE = 16;
  volatile uint8_t _edge_level[EDGE_BUF_SIZE];
  volatile uint32_t _edge_time[EDGE_BUF_SIZE];
  volatile uint8_t _edge_head = 0, _edge_tail = 0;
  volatile uint32_t _last_isr_edge = 0;
  // Self-heal debounce: only reconcile against the live pin once a divergence
  // from `prev` has persisted, so a single raw read of a bouncing contact can't
  // inject a phantom press/release pair (a double-click).
  bool _healing = false;
  uint8_t _heal_level = 0;
  uint32_t _heal_since = 0;

  void pushEdge(uint8_t level, uint32_t at);
  bool popEdge(uint8_t &level, uint32_t &at);
  void isrHandler();
  static ZenMomentaryButton* _isr_table[8];

public:
  // MAX_ISR_BUTTONS/isrTrampolineN must be public: attachInterrupt() needs a
  // plain free-function pointer, taken from a file-scope table outside the
  // class (see ZenMomentaryButton.cpp), which needs both to size and fill itself.
  static const uint8_t MAX_ISR_BUTTONS = 8;
  static void isrTrampoline0();
  static void isrTrampoline1();
  static void isrTrampoline2();
  static void isrTrampoline3();
  static void isrTrampoline4();
  static void isrTrampoline5();
  static void isrTrampoline6();
  static void isrTrampoline7();
private:
#endif

public:
  ZenMomentaryButton(int8_t pin, int long_press_mills=0, bool reverse=false, bool pulldownup=false, bool multiclick=true);
  ZenMomentaryButton(int8_t pin, int long_press_mills, int analog_threshold);
  void begin();
  int check(bool repeat_click=false);  // returns one of BUTTON_EVENT_*
  void cancelClick();  // suppress next BUTTON_EVENT_CLICK (if already in DOWN state)
  uint8_t getPin() { return _pin; }
  bool isPressed() const;
};
