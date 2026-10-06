#pragma once

#include "PetTrainingGames.h"
#include "PetBoxTrainingView.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <stdio.h>

namespace zen { namespace pet {

// Original geometric graphics, sized to the available content region/font.
struct PetTrainingView {
  static void glyph(ZenDisplayDriver& d, int x, int y, uint8_t id, int scale, bool arrow=false) {
    static const uint8_t SYMBOLS[4][7] = {
      {0x7f,0x41,0x41,0x41,0x41,0x41,0x7f},
      {0x1c,0x22,0x41,0x41,0x41,0x22,0x1c},
      {0x08,0x14,0x14,0x22,0x22,0x41,0x7f},
      {0x08,0x08,0x08,0x7f,0x08,0x08,0x08}
    };
    static const uint8_t ARROWS[4][7] = {
      {0x08,0x1c,0x2a,0x49,0x08,0x08,0x08},
      {0x08,0x10,0x20,0x7f,0x20,0x10,0x08},
      {0x08,0x08,0x08,0x49,0x2a,0x1c,0x08},
      {0x08,0x04,0x02,0x7f,0x02,0x04,0x08}
    };
    const uint8_t* rows=arrow?ARROWS[id%4]:SYMBOLS[id%4];
    for(int r=0;r<7;++r) for(int c=0;c<7;++c)
      if(rows[r] & (1<<c)) d.fillRect(x+c*scale,y+r*scale,scale,scale);
  }
  static int render(ZenDisplayDriver& d, int y, int bottom,
                    const PetTrainingGames& game, uint32_t now) {
    using Games=PetTrainingGames;
    int lh=d.getLineHeight(), step=d.lineStep(), scale=lh/8;
    if(scale<1) scale=1;
    char text[32];
    d.setColor(ZenDisplayDriver::LIGHT);
    d.drawTextEllipsized(0,y,d.width(),Games::name(game.game()));
    if(game.phase()==Games::INSTRUCTIONS || game.phase()==Games::FAILED) {
      static const char* const RULES[] = {
        "Remember 3 arrows","Catch 3 of 5 drops","Hit the zone 3 times",
        "Track 5 swaps","Find changed shape"
      };
      static const char* const CONTROLS[] = {
        "Use joystick arrows","Left/Right to move","Enter in the zone",
        "Left/Right, Enter","Left/Right, Enter"
      };
      const bool failed=game.phase()==Games::FAILED;
      const char* lines[] = {
        failed?(game.retryUsed()?"Training failed":"Try again"):RULES[game.game()],
        failed?"No reward or cost":CONTROLS[game.game()],
        failed?(game.retryUsed()?"Enter: Close":"Enter: Retry"):"Enter: Start"
      };
      for(int i=0;i<3 && y+(i+1)*step+lh<=bottom;++i)
        d.drawTextEllipsized(0,y+(i+1)*step,d.width(),lines[i]);
      return 5000;
    }
    if(game.game()==Games::BOXES)
      return PetBoxTrainingView::render(d,y,bottom,game,now);
    int top=y+lh+2, end=bottom-lh-2;
    int middle=top+(end-top-7*scale)/2;
    const char* hint="Back: Cancel";
    if(game.game()==Games::TIMING) {
      snprintf(text,sizeof(text),"Turn %u/3",game.progress()<3?game.progress()+1:3); hint=text;
      int x=6*scale, width=d.width()-2*x, stride=width/9, bar_y=top+(end-top-8*scale)/2;
      d.drawRect(x,bar_y,stride*9,8*scale);
      d.fillRect(x+game.zoneStart()*stride,bar_y,game.zoneWidth()*stride,8*scale);
      uint8_t pos=game.marker();
      d.setColor(game.inZone()?ZenDisplayDriver::DARK:ZenDisplayDriver::LIGHT);
      d.fillRect(x+pos*stride+stride/2-scale,bar_y+2*scale,2*scale,4*scale);
      d.setColor(ZenDisplayDriver::LIGHT);
    } else if(game.game()==Games::FOOD) {
      snprintf(text,sizeof(text),"Caught %u/3",game.progress()); hint=text;
      int x=(game.target()*2+1)*d.width()/(2*Games::FOOD_LANES);
      int drop_y=top+game.stage()*(end-top-6*scale)/3;
      d.fillRect(x-scale,drop_y,2*scale,2*scale);
      int pet_x=(game.selection()*2+1)*d.width()/(2*Games::FOOD_LANES);
      d.drawRect(pet_x-4*scale,end-5*scale,8*scale,5*scale);
      d.fillRect(pet_x-2*scale,end-3*scale,scale,scale);
      d.fillRect(pet_x+scale,end-3*scale,scale,scale);
    } else {
      if(game.game()==Games::ARROWS && game.phase()==Games::PLAY) {
        snprintf(text,sizeof(text),"Repeat %u/3",game.progress()); hint=text;
      }
      for(int i=0;i<3;++i) {
        int cx=(i*2+1)*d.width()/6;
        if(game.game()==Games::ARROWS) {
          if(game.phase()==Games::SHOW ||
              ((game.phase()==Games::PLAY || game.phase()==Games::WON) && i<game.progress()))
            glyph(d,cx-3*scale,middle,game.arrow(i),scale,true);
          else d.drawRect(cx-3*scale,middle,6*scale,6*scale);
        } else if(game.game()==Games::CHANGED_SHAPE) {
          if(game.phase()==Games::COVERED)
            d.fillRect(cx-3*scale,middle,7*scale,7*scale);
          else glyph(d,cx-3*scale,middle,game.symbol(i),scale);
        }
        if(game.phase()==Games::PLAY && game.game()!=Games::ARROWS && i==game.selection())
          d.fillRect(cx-6*scale,end-scale,12*scale,scale);
      }
    }
    d.drawTextEllipsized(0,bottom-lh,d.width(),hint);
    return game.refreshMs(now);
  }
};

} } // namespace zen::pet
