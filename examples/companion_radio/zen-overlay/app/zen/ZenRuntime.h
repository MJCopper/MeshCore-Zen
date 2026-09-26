#pragma once

#include "ZenPolicy.h"

// Tiny, allocation-free owner for Zen session state. Persisted configuration
// stays in ZenPrefs; transient parent authorization and lock transitions stay
// here instead of leaking feature-specific flags through the UI task.
namespace zen {

class Runtime {
  bool _parent_unlocked = true;
  bool _was_child_locked = false;

public:
  void begin(const ZenPrefs* prefs) {
    _parent_unlocked = !prefs || !prefs->child_mode_enabled;
    _was_child_locked = false;
  }

  void setParentUnlocked(bool unlocked) { _parent_unlocked = unlocked; }
  bool parentUnlocked() const { return _parent_unlocked; }

  bool childLocked(const ZenPrefs* prefs) const {
    return Policy::childLocked(prefs, _parent_unlocked);
  }

  bool recordChildLockState(const ZenPrefs* prefs) {
    bool locked = childLocked(prefs);
    bool entered = locked && !_was_child_locked;
    _was_child_locked = locked;
    return entered;
  }
};

} // namespace zen
