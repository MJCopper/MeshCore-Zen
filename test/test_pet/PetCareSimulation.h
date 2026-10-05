#pragma once

#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetEngine.h"
#include <stdint.h>

// Host-only minute-step caretaker, using the production engine rather than a
// duplicate discharge/care model. Successful training outcomes are injected;
// minigame input and animation are covered by the separate training/UI tests.
struct PetCareSimulation {
  struct Policy {
    bool sleep = true, bonuses = false, low_power = false;
    unsigned daily_training_limit = 0;
    uint8_t hunger_rate = 5;
  };
  struct Result {
    uint64_t reached[zen::pet::Evolution::LEVELS] = {};
    unsigned wins = 0;
    uint8_t level = 1;
  };
  static Result run(const Policy& policy, unsigned days=365) {
    zen::pet::Engine engine;
    Result result;
    unsigned day=0, trained=0; bool rewarded=false;
    for(uint64_t minute=0;minute<=uint64_t(days)*1440;++minute) {
      unsigned current_day=minute/1440;
      if(current_day!=day) { day=current_day; trained=0; rewarded=false; }
      unsigned local_minute=(8*60+minute)%1440; // Born at 08:00 local time.
      bool sleeping=policy.sleep && (local_minute>=22*60 || local_minute<6*60);
      bool paused=policy.low_power && local_minute>=8*60 && local_minute<12*60;
      engine.update(uint32_t(minute*60000),true,sleeping,paused,policy.hunger_rate);
      if(sleeping || paused) continue;
      if(policy.bonuses && !rewarded) { engine.bonus(35,5); rewarded=true; }
      if(engine.state().fullness<=75 && engine.state().food) engine.feed();
      // Good care retains a 25-fullness reserve after training. This is a
      // simulation policy, not an additional restriction on the actual game.
      if(engine.state().fullness>=35 &&
          (!policy.daily_training_limit || trained<policy.daily_training_limit) &&
          engine.train()==zen::pet::Engine::OK) { ++result.wins; ++trained; }
      while(engine.ready()) {
        engine.evolve(0); result.level=engine.level();
        result.reached[result.level-1]=minute;
      }
      if(result.level==zen::pet::Evolution::LEVELS) break;
    }
    return result;
  }
};
