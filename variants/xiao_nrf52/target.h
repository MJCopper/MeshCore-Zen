#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <XiaoNrf52Board.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/sensors/EnvironmentSensorManager.h>

#ifdef SENSOR_MANAGER_HEADER
  #include SENSOR_MANAGER_HEADER
#endif
#ifndef SENSOR_MANAGER_CLASS
  #define SENSOR_MANAGER_CLASS EnvironmentSensorManager
#endif

#ifdef DISPLAY_CLASS
  #include <helpers/ui/NullDisplayDriver.h>
  #include <helpers/ui/MomentaryButton.h>
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
#endif

extern XiaoNrf52Board board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern SENSOR_MANAGER_CLASS sensors;

bool radio_init();
mesh::LocalIdentity radio_new_identity();

