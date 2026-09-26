#pragma once
// Fork-specific buzzer/melody dispatch — isolated here so upstream changes to
// UITask::notify() don't create merge conflicts.
#ifdef PIN_BUZZER
#include <helpers/ui/ZenBuzzer.h>
#include "../ZenPrefs.h"
#include "../zen/NotificationPreferences.h"
#include "../zen/BuiltinMelodies.h"
#include "../zen/RingtoneModel.h"
#include "../zen/NotificationCoordinator.h"

class SoundNotifier {
  ZenBuzzer&   _buz;
  const ZenPrefs* _prefs;
  char*            _mel_buf;
  int              _mel_buf_sz;

  static void buildMelody(const ZenPrefs* p, int slot, char* buf, int size) {
    const uint8_t* notes = (slot == 2) ? p->ringtone2_notes   : p->ringtone_notes;
    uint8_t        len   = (slot == 2) ? p->ringtone2_len      : p->ringtone_len;
    uint8_t        bpm_i = (slot == 2) ? p->ringtone2_bpm_idx  : p->ringtone_bpm_idx;
    zen::RingtoneModel::buildRTTTL(notes, len, bpm_i, buf, size);
  }

  void playSelection(uint8_t selection, uint8_t empty_fallback) {
    selection = zen::BuiltinMelodies::validate(selection, empty_fallback);
    if (selection == zen::BuiltinMelodies::NONE) return;
    if ((selection == zen::BuiltinMelodies::CUSTOM1 ||
         selection == zen::BuiltinMelodies::CUSTOM2) && _prefs) {
      if (_buz.isPlaying()) _buz.stop();
      buildMelody(_prefs, selection == zen::BuiltinMelodies::CUSTOM2 ? 2 : 1,
                  _mel_buf, _mel_buf_sz);
      if (_mel_buf[0]) {
        _buz.playForced(_mel_buf);
        return;
      }
    }
    const char* melody = zen::BuiltinMelodies::melody(selection);
    if (!melody) melody = zen::BuiltinMelodies::melody(empty_fallback);
    if (!melody) return; // an empty custom slot may intentionally fall back to None
    _buz.playForced(melody);
  }

public:
  SoundNotifier(ZenBuzzer& buz, const ZenPrefs* prefs, char* buf, int sz)
    : _buz(buz), _prefs(prefs), _mel_buf(buf), _mel_buf_sz(sz) {}

  void preview(uint8_t selection, uint8_t empty_fallback) {
    // An explicit user preview is not an incoming notification. Its selected
    // "None" value is still silent, but global/temporary mute is ignored.
    if (_buz.isPlaying()) _buz.stop();
    playSelection(selection, empty_fallback);
  }

  void playDM(bool dm_valid, const uint8_t* dm_prefix) {
    // NotificationPolicy has already decided whether this event may sound.
    uint8_t slot = _prefs ? _prefs->notif_melody_dm : zen::BuiltinMelodies::MESSAGE;
    if (dm_valid && _prefs) {
      uint8_t override_slot = zen::NotificationPreferences::dmMelody(_prefs, dm_prefix);
      if (override_slot) slot = override_slot - 1;
    }
    playSelection(slot, zen::BuiltinMelodies::MESSAGE);
  }

  void playLowBattery() {
    // Policy controls muting; the buzzer still applies the selected volume.
    _buz.playForced("LowBat:d=32,o=6,b=120:c");
  }

  void playCH(int ch_idx) {
    uint8_t slot = _prefs ? _prefs->notif_melody_ch : zen::BuiltinMelodies::KERPLOP;
    if (ch_idx >= 0 && ch_idx < 64 && _prefs) {
      uint8_t override_slot = zen::NotificationPreferences::channelMelody(_prefs, ch_idx);
      if (override_slot) slot = override_slot - 1;
    }
    playSelection(slot, zen::BuiltinMelodies::KERPLOP);
  }

  void playAD(bool is_flood) {
    if (_prefs && _prefs->advert_sound_scope == ADVERT_SOUND_SCOPE_ZERO_HOP && is_flood) return;
    uint8_t slot = _prefs ? _prefs->notif_melody_ad : zen::BuiltinMelodies::MESSAGE;
    playSelection(slot, zen::BuiltinMelodies::MESSAGE);
  }

  void playNewContact() {
    uint8_t slot = _prefs ? _prefs->notif_melody_new_contact : zen::BuiltinMelodies::NONE;
    playSelection(slot, zen::BuiltinMelodies::NONE);
  }

  void play(const zen::NotificationEvent& event) {
    switch (event.type) {
      case zen::NotificationType::DIRECT_MESSAGE:
        playDM(event.prefix_valid, event.prefix); break;
      case zen::NotificationType::CHANNEL_MESSAGE:
        playCH(event.channel); break;
      case zen::NotificationType::ROOM_MESSAGE:
        playDM(false, nullptr); break;
      case zen::NotificationType::ADVERT_FLOOD:
      case zen::NotificationType::ADVERT_LOCAL:
        playAD(event.type == zen::NotificationType::ADVERT_FLOOD); break;
      case zen::NotificationType::NEW_CONTACT:
        playNewContact(); break;
      case zen::NotificationType::UI_FEEDBACK:
        _buz.playForced("ack:d=32,o=8,b=120:c"); break;
      case zen::NotificationType::LOW_BATTERY:
        playLowBattery(); break;
      case zen::NotificationType::WARNING:
      case zen::NotificationType::ERROR:
      case zen::NotificationType::NONE:
      default:
        break;
    }
  }
};
#endif
