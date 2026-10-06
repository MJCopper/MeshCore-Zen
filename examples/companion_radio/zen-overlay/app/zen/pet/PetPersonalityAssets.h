#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// Shared expressions and short ASCII phrases, never per-form animation sheets.
struct PetPersonalityAssets {
  static const char* temperament(uint8_t id) {
    static const char* const NAMES[]={"Playful","Calm","Curious","Stubborn"};
    return NAMES[id%4];
  }
  static const char* phrase(uint8_t event, uint8_t temperament, uint8_t choice) {
    static const char* const WORDS[][4][3]={
      {{"Yum!","More?","Thanks!"},{"Lovely.","Thank you.","Nice."},{"Tasty!","More?","Yum!"},{"Fine!","Not bad.","More?"}},
      {{"Again!","I did it!","Woohoo!"},{"Well done.","Lovely.","Nice."},{"What's next?","Again!","I did it!"},{"Easy!","Again!","Of course!"}},
      {{"So close!","Another go.","Again!"},{"Next time.","So close!","Oh well."},{"Try again?","That was close!","Another go."},{"Another go.","Not fair!","Next time."}},
      {{"Much better!","Thanks!","Ahh!"},{"Relaxing.","Much better!","Thanks!"},{"More energy?","Thanks!","Much better!"},{"Better.","Finally!","Thanks!"}},
      {{"You're back!","Hello!","Hi!"},{"Hello!","Welcome back","Hi."},{"You're back!","Hello!","What's new?"},{"Oh, hello.","You're back!","Hi."}},
      {{"Snack?","Food?","Hungry!"},{"Snack?","Food?","A bite?"},{"Food?","Snack?","Any snacks?"},{"Snack?","Feed me?","Food?"}},
      {{"Again!","Let's play!","Hi!"},{"Lovely day.","Ahh!","Nice."},{"What's next?","What's that?","Hello!"},{"Fine!","Hmm.","Hello."}},
      {{"Ready!","Watch me!","Woohoo!"},{"Ready.","Growing up.","Watch me!"},{"What's next?","Ready!","Watch me!"},{"Watch me!","Ready!","Finally!"}},
      {{"Sleepy...","Bed soon.","Yawn!"},{"Bed soon.","Sleepy...","Yawn."},{"Sleepy...","Bed soon.","Yawn!"},{"Not sleepy!","Bed soon.","Yawn."}}
    };
    return WORDS[event<9?event:6][temperament%4][choice%3];
  }
  static const uint8_t* face(uint8_t pose) {
    static const uint8_t FACES[][8]={
      {0,0,0,0x24,0,0x18,0,0}, // neutral
      {0,0,0,0x24,0,0x24,0x18,0}, // happy
      {0,0,0x24,0x24,0,0x24,0x18,0}, // proud
      {0,0,0,0x24,0,0x18,0x24,0}, // sulking
      {0,0,0,0x24,0,0x18,0,0}, // sleepy
      {0,0,0x24,0x24,0x18,0x24,0x18,0}, // excited, open mouth
      {0,0,0,0x66,0,0x24,0x18,0}, // relaxed
      {0,0,0,0x66,0,0x18,0,0}, // sleeping
      {0,0,0,0x66,0,0x18,0,0} // resting
    };
    return FACES[pose<9?pose:0];
  }
};

} }
