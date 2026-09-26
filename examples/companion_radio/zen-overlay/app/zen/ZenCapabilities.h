#pragma once

// Board capabilities are derived only from hardware definitions. They do not
// enable Zen product features by themselves.
#if defined(EINK_DISPLAY_MODEL)
  #define ZEN_CAP_EINK 1
#else
  #define ZEN_CAP_EINK 0
#endif

#if defined(CARDKB_ADDRESS)
  #define ZEN_CAP_CARDKB 1
#else
  #define ZEN_CAP_CARDKB 0
#endif

#if ENV_INCLUDE_GPS
  #define ZEN_CAP_GPS 1
#else
  #define ZEN_CAP_GPS 0
#endif

#if defined(BLE_PIN_CODE)
  #define ZEN_CAP_BLUETOOTH 1
#else
  #define ZEN_CAP_BLUETOOTH 0
#endif

#if defined(PIN_BUZZER)
  #define ZEN_CAP_BUZZER 1
#else
  #define ZEN_CAP_BUZZER 0
#endif

namespace zen {

struct Capabilities {
  static constexpr bool EINK = ZEN_CAP_EINK;
  static constexpr bool CARDKB = ZEN_CAP_CARDKB;
  static constexpr bool BUZZER = ZEN_CAP_BUZZER;
  static constexpr bool GPS = ZEN_CAP_GPS;
  static constexpr bool BLUETOOTH = ZEN_CAP_BLUETOOTH;
};

} // namespace zen
