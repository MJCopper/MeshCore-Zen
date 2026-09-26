#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <ZenWioTrackerL1Board.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <zen/TrackingRTCClock.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#ifdef DISPLAY_CLASS
  #if defined(WIO_TRACKER_L1_EINK)
    #include <helpers/ui/ZenGxEPDDisplay.h>
  #else
    #include <helpers/ui/ZenSH1106Display.h>
  #endif
  #include <helpers/ui/ZenMomentaryButton.h>
#endif
#include <helpers/sensors/EnvironmentSensorManager.h>

extern ZenWioTrackerL1Board board;
extern WRAPPER_CLASS radio_driver;
extern zen::TrackingRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;
#ifdef DISPLAY_CLASS
  extern DISPLAY_CLASS display;
  extern ZenMomentaryButton user_btn;
  extern ZenMomentaryButton joystick_left;
  extern ZenMomentaryButton joystick_right;
  extern ZenMomentaryButton back_btn;
  #ifdef UI_HAS_JOYSTICK_UPDOWN
    extern ZenMomentaryButton joystick_up;
    extern ZenMomentaryButton joystick_down;
  #endif
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();
