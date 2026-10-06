#pragma once

#include "PetTrainingGames.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <stdio.h>

namespace zen { namespace pet {

// Box-game presentation only. The model owns swaps, timing and results.
struct PetBoxTrainingView {
  static void face(ZenDisplayDriver& d,int x,int y,int scale) {
    static const uint8_t FACE[7]={0x41,0x7f,0x55,0x41,0x49,0x36,0x1c};
    for(int r=0;r<7;++r) for(int c=0;c<7;++c)
      if(FACE[r] & (1<<c)) d.fillRect(x+c*scale,y+r*scale,scale,scale);
  }
  static int render(ZenDisplayDriver& d,int y,int bottom,
                    const PetTrainingGames& game,uint32_t now) {
    using Games=PetTrainingGames;
    int lh=d.getLineHeight(), scale=lh/8;
    if(scale<1) scale=1;
    int top=y+lh+2, end=bottom-lh-2;
    int middle=top+(end-top-7*scale)/2;
    bool swap=game.phase()==Games::BOX_HINT || game.phase()==Games::BOX_MOVE;
    uint8_t a=swap?game.swap(game.stage()*2):0;
    uint8_t b=swap?game.swap(game.stage()*2+1):0;
    int centres[3]={d.width()/6,d.width()/2,5*d.width()/6};
    for(uint8_t i=0;i<3;++i) {
      int cx=centres[i], box_y=middle-2*scale;
      if(game.phase()==Games::BOX_MOVE && (i==a || i==b)) {
        uint8_t other=i==a?b:a;
        cx+=(centres[other]-centres[i])*game.boxFrame()/4;
        box_y+=i==a?-2*scale:2*scale;
      }
      bool selected=(game.phase()==Games::PLAY || game.phase()==Games::ANSWER) && i==game.selection();
      if(selected || (swap && (i==a || i==b)))
        d.drawRect(cx-9*scale,box_y-scale,18*scale,13*scale);
      d.drawRect(cx-7*scale,box_y,14*scale,11*scale);
      bool open=(game.phase()==Games::SHOW && i==game.target()) ||
          (game.phase()==Games::ANSWER && (i==game.selection() || i==game.target()));
      if(open) {
        if(i==game.target()) face(d,cx-3*scale,box_y+2*scale,scale);
        else {
          // The chosen empty box is open; the pet is revealed in the correct box.
          d.fillRect(cx-3*scale,box_y+5*scale,6*scale,scale);
        }
      } else d.fillRect(cx-7*scale,box_y+2*scale,14*scale,scale);
    }
    char text[32]; const char* hint="Boxes closed";
    if(game.phase()==Games::SHOW) hint="Remember";
    else if(swap) {
      snprintf(text,sizeof(text),"Watch swap %u/%u",game.stage()+1,Games::BOX_SWAPS); hint=text;
    } else if(game.phase()==Games::BOX_PAUSE) hint="Pause";
    else if(game.phase()==Games::PLAY) hint="Where is your pet?";
    else if(game.phase()==Games::ANSWER)
      hint=game.selection()==game.target()?"Found your pet!":"Your pet was here";
    d.drawTextEllipsized(0,bottom-lh,d.width(),hint);
    return game.refreshMs(now);
  }
};

} }
