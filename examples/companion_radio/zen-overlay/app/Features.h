#pragma once

// Build-time feature flags derived from board defines. Two flavours:
//
//   FEAT_* preprocessor macros — for conditional compilation of struct
//   members, enum entries, and class fields where the preprocessor must
//   run. Use `#if FEAT_X` instead of `#ifdef EINK_DISPLAY_MODEL`; the
//   name documents *what* the flag toggles rather than *why* it's bound
//   to e-ink.
//
//   Features::* constexpr values — for runtime-shape decisions (booleans,
//   timings, defaults). Compiler dead-branch-eliminates on the constexpr,
//   so cost is identical to a preprocessor branch but the code stays
//   reachable to tooling.

#if defined(EINK_DISPLAY_MODEL)
  // E-ink build: slow refresh, rotatable panel, no per-second redraws
  #define FEAT_BRIGHTNESS_SETTING         0
  #define FEAT_CLOCK_SECONDS_SETTING      0
  #define FEAT_DISPLAY_ROTATION_SETTING   1
  #define FEAT_JOYSTICK_ROTATION_SETTING  1
  #define FEAT_FULL_REFRESH_SETTING       1
#else
  // OLED build: fast refresh, fixed orientation
  #define FEAT_BRIGHTNESS_SETTING         1
  #define FEAT_CLOCK_SECONDS_SETTING      1
  #define FEAT_DISPLAY_ROTATION_SETTING   0
  #define FEAT_JOYSTICK_ROTATION_SETTING  0
  #define FEAT_FULL_REFRESH_SETTING       0
#endif

namespace Features {

#if defined(EINK_DISPLAY_MODEL)
  static constexpr bool     IS_EINK              = true;
  static constexpr bool     BLINK_INDICATORS     = false;   // e-ink avoids high-rate redraws
  static constexpr bool     CLOCK_HIDE_SECONDS_DEFAULT = true;  // pref starting value
  static constexpr unsigned HOME_REFRESH_MS      = 30000;   // slow display polls less
#else
  static constexpr bool     IS_EINK              = false;
  static constexpr bool     BLINK_INDICATORS     = true;
  static constexpr bool     CLOCK_HIDE_SECONDS_DEFAULT = false;
  static constexpr unsigned HOME_REFRESH_MS      = 1000;
#endif

} // namespace Features
