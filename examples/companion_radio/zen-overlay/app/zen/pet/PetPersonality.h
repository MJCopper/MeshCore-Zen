#pragma once

#include <stdint.h>
#include <string.h>
#include "PetPersonalityAssets.h"

namespace zen { namespace pet {

// Cosmetic state only: no engine, platform, mesh, notification or save ownership.
class PetPersonality {
public:
  void setMovementBounds(int8_t x,int8_t y) {
    x=x<0?0:x>4?4:x; y=y<0?0:y>2?2:y;
    _context.range_x=x; _context.range_y=y;
    int nx=_offset_x,ny=_offset_y;
    if(nx<-x)nx=-x; if(nx>x)nx=x;
    if(ny<-y)ny=-y; if(ny>y)ny=y;
    if(nx!=_offset_x || ny!=_offset_y) { _offset_x=nx; _offset_y=ny; _dirty=true; }
    if(!x && !y) stopMovement();
  }
  uint32_t updateDelay(uint32_t now) const {
    if (!_moving) return 1000;
    uint32_t elapsed=now-_move_at;
    if (elapsed>=_move_ms) return 1;
    uint32_t remaining=_move_ms-elapsed;
    return remaining<1000?remaining:1000;
  }
  enum Temperament : uint8_t { PLAYFUL, CALM, CURIOUS, STUBBORN };
  enum Pose : uint8_t { NEUTRAL, HAPPY, PROUD, SULKING, SLEEPY, EXCITED, RELAXED, SLEEP, REST };
  enum Quirk : uint8_t { NONE, BLINK, LOOK_LEFT, LOOK_RIGHT, TILT, HOP, STRETCH };
  enum Event : uint8_t { FED, WON, LOST, RECOVERY, GREETING, HUNGER, IDLE, READY, BEDTIME };
  struct Context {
    bool enabled=true, sleeping=false, paused=false;
    bool page_visible=true, ordinary=true, unobscured=true, slow=false;
    bool ready=false, bedtime=false, external_power=false;
    uint8_t fullness=70;
    int battery_percent=-1;
    uint32_t battery_sample=0;
    int8_t range_x=4,range_y=2;
  };
  struct Presentation {
    Pose pose=NEUTRAL; Quirk quirk=NONE;
    const char* phrase=nullptr;
    uint8_t frame=0;
    bool active=false;
    int8_t offset_x=0, offset_y=0;
  };
private:
  bool _born=false, _eligible=false, _page=false, _away=false;
  bool _reaction=false, _bubble=false, _bubble_used=false, _dirty=false, _drawn=false;
  bool _ready=false, _bedtime=false, _sample_seen=false, _power=false;
  bool _candidate=false, _recovered=false;
  Temperament _temperament=PLAYFUL;
  Pose _pose=NEUTRAL;
  Event _event=IDLE;
  Quirk _quirk=NONE;
  Context _context;
  uint32_t _rng=1, _last=0, _away_at=0, _reaction_at=0, _reaction_ms=0;
  uint32_t _bubble_at=0, _bubble_last=0, _quirk_at=0, _idle_credit=0, _speech_credit=0;
  uint32_t _idle_due=15000, _last_draw=0, _sample=0, _candidate_at=0, _recovery_at=0;
  int _anchor=-1, _recovery_percent=-1;
  const char* _phrase=nullptr;
  const char* _previous_phrase=nullptr;
  bool _moving=false;
  int8_t _offset_x=0, _offset_y=0;
  uint32_t _move_at=0, _move_ms=0;
  void stopMovement() {
    if (_offset_x || _offset_y) _dirty=true;
    _moving=false; _offset_x=_offset_y=0;
  }
  uint32_t random() {
    _rng^=_rng<<13; _rng^=_rng>>17; _rng^=_rng<<5; return _rng;
  }
  static uint8_t priority(Event event) {
    return event==FED || event==WON || event==LOST?3:event==READY?2:event==RECOVERY?1:0;
  }
  void clearTransient() {
    if (_reaction || _bubble || _quirk!=NONE) _dirty=true;
    _reaction=_bubble=false; _quirk=NONE; _phrase=nullptr;
  }
  void bubble(Event event, uint32_t now) {
    if (!_page || !_context.enabled || _context.sleeping || _context.paused ||
        (_bubble_used && uint32_t(now-_bubble_last)<30000)) return;
    uint8_t choice=random()%3;
    const char* text=PetPersonalityAssets::phrase(event,_temperament,choice);
    if (_previous_phrase && strcmp(text,_previous_phrase)==0)
      text=PetPersonalityAssets::phrase(event,_temperament,(choice+1)%3);
    _previous_phrase=_phrase=text; _bubble=_bubble_used=true;
    _bubble_at=_bubble_last=now; _dirty=true;
  }
  bool recovery(uint32_t now, const Context& c) {
    if (!_sample_seen || c.battery_sample!=_sample) {
      _sample_seen=true; _sample=c.battery_sample;
      if (!c.external_power || c.battery_percent<0 || c.battery_percent>100) {
        _power=_candidate=_recovered=false; _anchor=-1; return false;
      }
      if (!_power) {
        _power=true; _anchor=c.battery_percent; _candidate=_recovered=false;
        return false;
      }
      int threshold=_recovered?_recovery_percent+10:_anchor+3;
      if (c.battery_percent<threshold) { _candidate=false; return false; }
      if (!_candidate) { _candidate=true; _candidate_at=now; return false; }
      if (uint32_t(now-_candidate_at)<120000 ||
          (_recovered && uint32_t(now-_recovery_at)<600000)) return false;
      _recovered=true; _candidate=false; _recovery_at=now;
      _recovery_percent=c.battery_percent;
      return true;
    }
    return false;
  }
public:
  Temperament temperament() const { return _temperament; }
  const char* name() const { return PetPersonalityAssets::temperament(_temperament); }
  void hide(uint32_t now) {
    if (_page) { _away=true; _away_at=now; }
    _page=_eligible=false; _idle_credit=_speech_credit=0;
    _context.page_visible=false;
    stopMovement();
    clearTransient();
  }
  void event(Event event, uint32_t now) {
    if (!_born || !_context.enabled || _context.sleeping || _context.paused || !_page) return;
    Pose pose=event==FED?HAPPY:event==WON?PROUD:event==LOST?SULKING:
        event==RECOVERY?(_temperament==PLAYFUL || _temperament==CURIOUS?HAPPY:RELAXED):
        event==READY?EXCITED:event==BEDTIME?SLEEPY:NEUTRAL;
    // A minor ambient event must not replace a successful care/game reaction.
    if (_reaction && uint32_t(now-_reaction_at)<_reaction_ms && priority(event)<priority(_event)) return;
    _pose=pose; _event=event; _reaction=true; _reaction_at=now;
    _reaction_ms=event==WON || event==RECOVERY?5000:4000;
    _quirk=NONE; _bubble=false; _phrase=nullptr;
    _dirty=true; bubble(event,now);
  }
  void update(uint32_t now, const Context& c, uint32_t seed=0) {
    if (!_born && c.enabled) {
      _born=true; _rng=seed?seed:now^0x6d2b79f5UL; if (!_rng) _rng=1;
      _temperament=(Temperament)(random()%4); _last=now; _dirty=true;
      _idle_due=15000+random()%15001;
    }
    uint32_t elapsed=now-_last; _last=now;
    bool eligible=c.enabled && !c.sleeping && !c.paused && c.page_visible && c.ordinary && c.unobscured;
    bool greeting=c.enabled && c.page_visible && !_page && _away && uint32_t(now-_away_at)>=300000;
    if (_page && !c.page_visible) { _away=true; _away_at=now; }
    if (!c.enabled) _away=false;
    if (_eligible && eligible) {
      // Never replay a long unseen interval as catch-up animation.
      if (elapsed>5000) elapsed=5000;
      _idle_credit+=elapsed; _speech_credit+=elapsed;
    }
    if (_eligible!=eligible || _context.sleeping!=c.sleeping || _context.paused!=c.paused ||
        _context.ready!=c.ready || _context.bedtime!=c.bedtime) _dirty=true;
    _context=c; _page=c.enabled && c.page_visible; _eligible=eligible;
    int rx=c.range_x<0?0:c.range_x>4?4:c.range_x;
    int ry=c.range_y<0?0:c.range_y>2?2:c.range_y;
    setMovementBounds(rx,ry);
    if (!eligible || c.slow || (!rx && !ry)) stopMovement();
    else if (!_moving) {
      _moving=true; _move_at=now; _move_ms=500+random()%1001;
    } else if (uint32_t(now-_move_at)>=_move_ms) {
      // Uniform choice among valid neighbouring positions, never a blocked step.
      int8_t xs[8],ys[8]; unsigned count=0;
      for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        int nx=_offset_x+dx,ny=_offset_y+dy;
        if((dx || dy) && nx>=-rx && nx<=rx && ny>=-ry && ny<=ry) {
          xs[count]=nx; ys[count++]=ny;
        }
      }
      unsigned next=random()%count; _offset_x=xs[next]; _offset_y=ys[next];
      _move_at=now; _move_ms=500+random()%1001; _dirty=true;
    }
    if (!c.enabled || c.sleeping || c.paused || !c.page_visible) {
      clearTransient(); _idle_credit=_speech_credit=0;
      if (!c.enabled) { _sample_seen=_power=_candidate=_recovered=false; _anchor=-1; }
    } else if (!c.ordinary || !c.unobscured) {
      // No speech backlog behind menus or notification overlays.
      if (_quirk!=NONE) _dirty=true;
      _quirk=NONE; _idle_credit=0;
    }
    if (_reaction && uint32_t(now-_reaction_at)>=_reaction_ms) { _reaction=false; _dirty=true; }
    if (_bubble && uint32_t(now-_bubble_at)>=4000) { _bubble=false; _dirty=true; }
    if (_quirk!=NONE && uint32_t(now-_quirk_at)>=(c.slow?5000u:2000u)) { _quirk=NONE; _dirty=true; }
    bool recovered=c.enabled && recovery(now,c);
    if (greeting && eligible) event(GREETING,now);
    if (c.ready && !_ready && eligible) event(READY,now);
    if (c.bedtime && !_bedtime && eligible) event(BEDTIME,now);
    _ready=c.ready; _bedtime=c.bedtime;
    if (recovered && eligible) event(RECOVERY,now);
    if (eligible && !_reaction && _quirk==NONE && _idle_credit>=_idle_due) {
      static const Quirk CHOICES[][8]={
        {HOP,HOP,STRETCH,BLINK,TILT,LOOK_LEFT,LOOK_RIGHT,HOP},
        {BLINK,BLINK,STRETCH,LOOK_LEFT,LOOK_RIGHT,TILT,BLINK,STRETCH},
        {TILT,TILT,LOOK_LEFT,LOOK_RIGHT,LOOK_LEFT,LOOK_RIGHT,BLINK,HOP},
        {LOOK_LEFT,LOOK_RIGHT,TILT,BLINK,STRETCH,BLINK,TILT,HOP}
      };
      _quirk=CHOICES[_temperament][random()%8]; _quirk_at=now;
      _idle_credit=0; _idle_due=15000+random()%15001; _dirty=true;
    }
    if (eligible && !_reaction && _speech_credit>=120000) {
      _speech_credit=0; bubble(c.fullness<=25?HUNGER:c.ready?READY:c.bedtime?BEDTIME:IDLE,now);
    }
  }
  Presentation presentation(uint32_t now) const {
    Presentation p;
    p.pose=_context.paused?REST:_context.sleeping?SLEEP:
        _reaction && uint32_t(now-_reaction_at)<_reaction_ms?_pose:
        _context.ready?EXCITED:_context.bedtime?SLEEPY:NEUTRAL;
    p.active=_eligible;
    if (_eligible) {
      p.offset_x=_offset_x; p.offset_y=_offset_y;
      if (_bubble && uint32_t(now-_bubble_at)<4000) p.phrase=_phrase;
      if (_quirk!=NONE && uint32_t(now-_quirk_at)<(_context.slow?5000u:2000u)) p.quirk=_quirk;
      p.frame=_context.slow?1:(now-_quirk_at)/500%4;
    }
    return p;
  }
  bool takeRedraw(uint32_t now, bool slow) {
    if (!_context.enabled || !_context.page_visible || !_context.ordinary ||
        !_context.unobscured || !_dirty ||
        (slow && _drawn && uint32_t(now-_last_draw)<5000)) return false;
    _dirty=false; _drawn=true; _last_draw=now; return true;
  }
};

} }
