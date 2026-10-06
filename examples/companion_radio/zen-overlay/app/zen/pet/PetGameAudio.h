#pragma once

#include "PetTrainingGames.h"
#include "../NotificationProfiles.h"

namespace zen { namespace pet {

// Immutable effects only. The model emits cues; Zen owns muting, volume and
// arbitration. Effects never create a popup, wake the screen or retain a queue.
struct PetGameAudio {
  static const char* melody(PetTrainingGames::Cue cue) {
    static const char* const SOUNDS[] = {
      nullptr, "Up:d=32,o=6,b=180:c", "Right:d=32,o=6,b=180:e",
      "Down:d=32,o=6,b=180:g", "Left:d=32,o=6,b=180:b",
      "Correct:d=32,o=6,b=180:e", "Catch:d=32,o=6,b=180:c,g",
      "Miss:d=32,o=5,b=180:c", "Hit:d=32,o=6,b=180:g",
      "Reveal:d=32,o=6,b=180:c,e", "Swap:d=32,o=5,b=180:g",
      "Cover:d=32,o=5,b=180:e,c"
    };
    return cue <= PetTrainingGames::COVER ? SOUNDS[cue] : nullptr;
  }
  static zen::NotificationEvent event(const char* melody) {
    auto result=zen::NotificationProfiles::event(zen::NotificationType::PET_TRAINING);
    result.visual=false; result.melody=melody;
    return result;
  }
};

} }
