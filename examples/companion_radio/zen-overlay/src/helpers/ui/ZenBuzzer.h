#pragma once

#include <Arduino.h>

// NRF52 uses a custom non-blocking RTTTL player (see buzzer.cpp); only the
// other platforms pull in the NonBlockingRtttl library here.
#if !defined(NRF52_PLATFORM)
  #include <NonBlockingRtttl.h>
#endif

/* class abstracts underlying RTTTL library

    Just a simple implementation to start.  At the moment use same
    melody for message and discovery
    Suggest enum type for different sounds
    - on message
    - on discovery

    TODO
    - make message ring tone configurable

*/

class ZenBuzzer
{
    public:
        void begin();
        void play(const char *melody);
        void playForced(const char *melody);
        void loop();
        void startup();
        void shutdown();
        bool isPlaying();
        void quiet(bool buzzer_state);
        bool isQuiet();
        void setVolume(uint8_t level);
        uint8_t getVolume() const { return _volume_level; }
        void stop();

    private:
        uint8_t _volume_level = 4;
        void applyVolume();
        const char *startup_song  = "Startup:d=4,o=5,b=160:16c6,16e6,8g6";
        const char *shutdown_song = "Shutdown:d=4,o=5,b=100:8g5,16e5,16c5";
        bool _is_quiet = true;

#if defined(NRF52_PLATFORM)
        // Own RTTTL player — bypasses tone() to allow volume control from note start.
        // tone() pre-computes seq_refresh so the DMA repeats 50% duty for ~30ms before
        // re-reading its buffer; we cannot beat that timing.  By owning NRF_PWM2 directly
        // and setting REFRESH=0, DMA re-reads _duty_buf every period so duty takes effect
        // immediately at SEQSTART.
        volatile uint16_t _duty_buf = 0;   // DMA source — must stay in RAM
        const char*   _rtttl_pos   = nullptr;
        bool          _rtttl_done  = true;
        bool          _pwm_on      = false;
        uint8_t       _def_dur     = 4;
        uint8_t       _def_oct     = 5;
        uint16_t      _def_bpm     = 120;

        void    _nrfBegin(const char* melody);
        void    _nrfAdvance();
        void    _nrfStartPwm(uint16_t freq);
        void    _nrfStopPwm();
        uint8_t _dutyPct() const;
        static uint16_t _noteFreq(char letter, bool sharp, uint8_t octave);
        static bool     _parseNext(const char*& pos, uint8_t def_dur, uint8_t def_oct,
                                   uint16_t bpm, uint16_t& freq_hz, uint32_t& dur_ms);
        static void     _parseHeader(const char* melody, uint8_t& def_dur, uint8_t& def_oct,
                                     uint16_t& bpm, const char*& notes_start);

        // TIMER1-driven note advance: a hardware compare interrupt calls
        // _nrfAdvance() at the exact moment the current note's duration ends,
        // so playback timing survives a blocking display refresh (e-ink)
        // instead of waiting for the next loop() poll. Same reasoning as
        // ZenMomentaryButton's GPIO-IRQ edge capture, just timer- rather than
        // pin-driven. TIMER0 is reserved by the SoftDevice; TIMER1 is free.
        void _armNoteTimer(uint32_t dur_ms);
        void _disarmNoteTimer();
        static ZenBuzzer* _isr_instance;

    public:
        // Must be public: the extern "C" TIMER1_IRQHandler (see buzzer.cpp) is
        // a free function — the vector table requires that exact symbol — so
        // it needs access from outside the class to dispatch into it.
        static void _timer1ISR();
#endif
};
