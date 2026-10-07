#pragma once

#include "PetEngine.h"
#include "PetAssets.h"
#include "PetRenderer.h"
#include "PetRewardsView.h"
#include "PetNotifications.h"
#include "PetTrainingView.h"
#include "PetSleep.h"
#include "PetBatteryPolicy.h"
#include "PetGameAudio.h"
#include "PetGameDisplay.h"
#include "PetSnapshot.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <stdio.h>

namespace zen { namespace pet {

// Optional presentation/controller adapter. Ownership stays on the home screen;
// the engine and assets never call Zen, MeshCore, notification or save APIs.
class Page {
  Engine _engine;
  PetMeshRewards _rewards;
  PetNotifications _notifications;
  PetTrainingGames _training;
  PetSleep _sleep;
  PetBatteryPolicy _battery;
  PetPersonality _personality;
  PetGameDisplay _game_display;
  int8_t _portrait_range_x=0,_portrait_range_y=0;
  uint8_t _view = 0, _selection = 0, _details_last = 3;
  const char* _feedback = nullptr;
  char _rest_feedback[24] = {};
  bool _enabled = false;
  bool _loss_reported = false;
  bool _save_requested=false;
  bool completeTraining(uint32_t now) {
    if (_training.phase() == PetTrainingGames::FAILED) {
      if (!_loss_reported) {
        _game_display.result(now);
        _notifications.training(false); _personality.event(PetPersonality::LOST,now);
        _loss_reported = true;
      }
    } else _loss_reported = false;
    if (_training.phase() != PetTrainingGames::WON) return false;
    // Revalidate at completion: gameplay never owns costs or rewards.
    auto r = _engine.train();
    if (r == Engine::OK) { _notifications.training(true); _game_display.result(now); }
    _training.finish();
    if (r != Engine::OK) _feedback = result(r);
    _view = r == Engine::OK ? 0 : 1; _selection = 1;
    if (r == Engine::OK) _personality.event(PetPersonality::WON,now);
    _notifications.observe(_engine.state(),_engine.level(),_engine.ready(),_enabled,
                           _engine.sleeping(),_engine.paused());
    return true;
  }
  const char* result(Engine::Result r) {
    if (r == Engine::COOLDOWN) {
      uint32_t minutes = (_engine.trainingRestMillis()+59999UL)/60000UL;
      snprintf(_rest_feedback,sizeof(_rest_feedback),"Rest %lu minute%s",
               (unsigned long)minutes,minutes == 1 ? "" : "s");
      return _rest_feedback;
    }
    static const char* TEXT[] = {"Done", "Sleeping", "Low Power", "Already full",
      "No food", "Needs food/rest", "Resting", "Not ready"};
    return TEXT[r];
  }
public:
  PetSnapshot checkpoint() const {
    PetSnapshot s; s.engine=_engine.checkpoint(); s.rewards=_rewards.checkpoint();
    s.temperament=_personality.temperament(); return s;
  }
  void restore(const PetSnapshot& s,uint32_t now) {
    _engine.restore(s.engine,now); _rewards.restore(s.rewards,now);
    _personality.restoreTemperament(s.temperament,now);
    _training.cancel(); _game_display.cancel(); _sleep=PetSleep();
    _view=_selection=0; _feedback=nullptr; _save_requested=false;
  }
  bool takeSaveRequest() { bool requested=_save_requested; _save_requested=false; return requested; }
  void saveFeedback(const char* text) { _feedback=text; }
  void update(uint32_t now, bool enabled, bool sleeping, bool paused,
              bool synced = false, int64_t local = 0, bool visible = true,
              int battery_percent = -1, bool external_power = false,
              uint32_t battery_sample = 0, bool bedtime = false,
              bool page_visible = true, bool unobscured = true, bool slow = false,
              uint32_t personality_seed = 0) {
    sleeping = _sleep.sleeping(now,enabled,sleeping);
    if (_view == 1 && _selection == 6 && !sleeping) _selection = 0;
    _engine.update(now, enabled, sleeping, paused, _battery.update(battery_percent));
    _enabled = enabled;
    PetPersonality::Context context;
    context.enabled=enabled; context.sleeping=sleeping; context.paused=paused;
    context.page_visible=page_visible; context.ordinary=_view==0 && !_training.active();
    context.unobscured=unobscured; context.slow=slow;
    context.ready=_engine.ready(); context.bedtime=bedtime && !_sleep.wakeActive();
    context.fullness=_engine.state().fullness; context.battery_percent=battery_percent;
    context.external_power=external_power; context.battery_sample=battery_sample;
    context.range_x=_portrait_range_x; context.range_y=_portrait_range_y;
    _personality.update(now,context,personality_seed);
    if (!enabled) _feedback = nullptr;
    if (_training.active()) {
      if (!enabled || sleeping || paused || (!visible && !trainingFailed())) cancelTraining();
      else { _training.tick(now); completeTraining(now); }
    }
    _rewards.update(now,enabled,sleeping,paused,synced,local);
    _notifications.observe(_engine.state(),_engine.level(),_engine.ready(),enabled,sleeping,paused);
    const uint16_t old_xp = _engine.state().xp;
    const uint8_t old_bond = _engine.state().bond;
    if (_rewards.apply(_engine)) {
      _notifications.reward(_engine.state().xp-old_xp,_engine.state().bond-old_bond);
    }
    _notifications.observe(_engine.state(),_engine.level(),_engine.ready(),enabled,sleeping,paused);
    _enabled = enabled;
    context.ready=_engine.ready(); context.fullness=_engine.state().fullness;
    context.ordinary=_view==0 && !_training.active();
    _personality.update(now,context);
  }
  PetMeshRewards& rewards() { return _rewards; }
  bool takeAlert(uint32_t now, PetNotifications::Alert& alert, bool quiet = false) {
    if (_training.active() && !trainingFailed()) return false;
    // Explicit action feedback is visual-only, including Sleeping/Low Power
    // refusals. It must not replace menu labels or inherit ambient pet gating.
    if (_enabled && _feedback) {
      alert.sound = PetNotifications::COUNT; alert.action = true;
      snprintf(alert.text,sizeof(alert.text),"%s",_feedback); _feedback = nullptr;
      return true;
    }
    return _notifications.take(now,alert,_training.active() || quiet);
  }
  bool trainingFailed() const { return _training.phase() == PetTrainingGames::FAILED; }
  bool enabled() const { return _enabled; }
  bool menuOpen() const { return _view != 0; }
  const PetPersonality& personality() const { return _personality; }
  bool takePersonalityRedraw(uint32_t now,bool slow) { return _personality.takeRedraw(now,slow); }
  void hidePresentation(uint32_t now) { _personality.hide(now); }
  bool trainingActive() const { return _training.active(); }
  PetGameDisplay::Decision gameDisplay(uint32_t now,bool visible) {
    return _game_display.update(now,_training.active() && !trainingFailed(),
        _training.previewing() || _training.phase()==PetTrainingGames::ANSWER,
        visible && _enabled && !_engine.sleeping() && !_engine.paused());
  }
  const char* takeGameSound() {
    auto cue=_training.takeCue();
    if (!_enabled || _engine.sleeping() || _engine.paused() ||
        !_training.active() || _training.phase()==PetTrainingGames::FAILED ||
        _training.phase()==PetTrainingGames::WON) return nullptr;
    return PetGameAudio::melody(cue);
  }
  void cancelTraining() {
    _game_display.cancel();
    if (!_training.active()) return;
    _training.cancel(); _view = 0;
  }
  void close() { _game_display.cancel(); _training.cancel(); _view = _selection = 0; _feedback = nullptr; }
  bool input(char c, char enter, char back, char hold, char up, char down,
             char left = 'l', char right = 'r', char prev = '[', char next = ']',
             uint32_t seed = 0, bool slow = false) {
    if (_training.active()) {
      using Games = PetTrainingGames;
      Games::Action action = c == back ? Games::BACK : c == enter ? Games::ENTER :
          c == up ? Games::UP : c == down ? Games::DOWN :
          c == left || c == prev ? Games::LEFT : c == right || c == next ? Games::RIGHT : Games::NONE;
      if(action!=Games::NONE) _game_display.input(millis());
      if(action==Games::BACK) _game_display.cancel();
      // Capture timer expiry before input can accept the retry or dismiss it.
      if (action != Games::BACK) {
        _training.tick(millis());
        if (completeTraining(millis())) return true;
      }
      _training.input(action,millis());
      if (completeTraining(millis())) return true;
      if (!_training.active()) {
        _view = 1; _selection = 1;
        if (c == back) _feedback = "Training cancelled";
      }
      return true;
    }
    if (c == back && _view) {
      if (_view == 5) _view = 4;
      else if (_view == 4) { _view = 1; _selection = 4; }
      else close();
      return true;
    }
    if (!_view) {
      if (c == enter) { _view = 1; _selection = 0; return true; }
      if (c == hold) { _view = 2; _selection = 0; return true; }
      return false;
    }
    if (_view == 2 && (c == up || c == down)) {
      if (c==down && _selection<_details_last) ++_selection;
      if (c==up && _selection>0) --_selection;
      return true;
    }
    if ((c == up || c == down) && (_view == 1 || _view == 3)) {
      uint8_t count = _view == 3 ? _engine.choices() : _engine.sleeping() ? 7 : 6;
      _selection = (_selection + count + (c == down ? 1 : -1)) % count;
      return true;
    }
    if (c != enter) return false;
    // A new action supersedes feedback not yet presented from the last one.
    _feedback = nullptr;
    if (_view == 4 || _view == 5) { _view = _view == 4 ? 5 : 4; return true; }
    if (_view == 2) { close(); return true; }
    if (_view == 1 && _selection == 5) { _save_requested=true; return true; }
    if (_view == 1 && _selection == 6) {
      if (_engine.sleeping() && !_engine.paused()) {
        _sleep.wake(millis());
        _engine.update(millis(),_enabled,false,false,_battery.rate());
        _view = 1; _selection = 0; _feedback = "Awake 30 minutes";
      } else { _feedback = "Low Power"; }
      return true;
    }
    if (_view == 3) {
      auto r = _engine.evolve(_selection);
      if (r == Engine::OK) _notifications.evolved(_engine.state().form);
      _notifications.observe(_engine.state(),_engine.level(),_engine.ready(),_enabled,
                             _engine.sleeping(),_engine.paused());
      if (r != Engine::OK) _feedback = result(r);
      _view = 1; _selection = 2;
      return true;
    }
    if (_selection == 3) { _view = 2; _selection = 0; return true; }
    if (_selection == 4) { _view = 4; return true; }
    if (_selection == 1) {
      auto r = _engine.trainingAvailable();
      if (r == Engine::OK) { _loss_reported = false; _training.begin(millis(),seed,slow); }
      else _feedback = result(r);
      return true;
    }
    if (_selection == 2 && _engine.ready() && _engine.available() == Engine::OK) {
      _view = 3; _selection = 0; return true;
    }
    auto r = _selection == 0 ? _engine.feed() : Engine::NOT_READY;
    _feedback = r == Engine::OK ? "Fed" : result(r);
    if (r == Engine::OK) {
      _notifications.observe(_engine.state(),_engine.level(),_engine.ready(),_enabled,
                             _engine.sleeping(),_engine.paused());
      _personality.event(PetPersonality::FED,millis());
    }
    return true;
  }
  int render(ZenDisplayDriver& d, int y, int bottom) {
    auto layout=PetPortraitLayout::calculate(d.width(),d.getLineHeight(),y,bottom,
        form(_engine.state().form).size);
    _portrait_range_x=layout.range_x; _portrait_range_y=layout.range_y;
    _personality.setMovementBounds(_portrait_range_x,_portrait_range_y);
    if (_training.active()) {
      _training.tick(millis());
      if (!completeTraining(millis()))
        return PetTrainingView::render(d,y,bottom,_training,millis());
    }
    if (_view == 4 || _view == 5) {
      PetRewardsView::render(d,y,bottom,_rewards,_view == 5); return 5000;
    }
    int rows=(bottom-y-d.getLineHeight())/d.lineStep()+1;
    if(rows<1) rows=1;
    int count=_engine.hungerRate()>5?7:5;
    _details_last=count>rows?count-rows:0;
    if(_view==2 && _selection>_details_last) _selection=_details_last;
    return Renderer::render(d,y,bottom,_engine,_view,_selection,
        _personality.presentation(millis()),_personality.name());
  }
};

} }
