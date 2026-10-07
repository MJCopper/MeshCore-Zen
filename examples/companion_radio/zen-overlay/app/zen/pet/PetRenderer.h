#pragma once

#include "PetEngine.h"
#include "PetAssets.h"
#include "PetPersonalityView.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <stdio.h>

namespace zen { namespace pet {

// Rendering has no input, persistence or gameplay ownership.
class Renderer {
public:
  static int render(ZenDisplayDriver& d, int y, int bottom,
                    const Engine& _engine, uint8_t _view, uint8_t _selection,
                    const PetPersonality::Presentation& look,
                    const char* temperament = "Playful") {
    char text[40]; const State& s = _engine.state();
    int step = d.lineStep();
    if (_view) {
      if (_view == 2) {
        char rows[5][40];
        snprintf(rows[0],sizeof(rows[0]),"%s L%u",form(s.form).name,_engine.level());
        if (_engine.level() == Evolution::LEVELS)
          snprintf(rows[1],sizeof(rows[1]),"Final form XP %u",s.xp);
        else snprintf(rows[1],sizeof(rows[1]),"XP %u/%u",s.xp,Evolution::xp(_engine.level()));
        snprintf(rows[2],sizeof(rows[2]),"Energy %u Food %u",s.energy,s.food);
        snprintf(rows[3],sizeof(rows[3]),"Full %u Bond %u/%u",s.fullness,s.bond,
                 Evolution::bond(_engine.level()));
        snprintf(rows[4],sizeof(rows[4]),"Nature %s",temperament);
        bool hungry = _engine.hungerRate() > 5;
        int count = hungry ? 7 : 5;
        int first = _selection<count?_selection:0;
        for (int i=first; i<count && y+d.getLineHeight()<=bottom; ++i,y+=step)
          d.drawTextLeftAlign(0,y,i<5 ? rows[i] : i==5 ? "Charge to reduce" : "hunger");
      } else {
        static const char* ITEMS[] = {"Feed","Train","Evolve","Details","Daily Rewards","Save Pet","Wake Up"};
        int count = _view==3?_engine.choices():_engine.sleeping()?7:6;
        int visible = (bottom-y)/step;
        if (visible < 1) visible = 1;
        int first = _selection >= visible ? _selection-visible+1 : 0;
        for (int i=first;i<count && i<first+visible;++i) {
          int row_y = y+(i-first)*step;
          const char* label = _view==3 ? form(_engine.child(i)).name : ITEMS[i];
          d.drawSelectionRow(0,row_y-1,d.width(),d.getLineHeight()+1,i==_selection);
          d.drawTextLeftAlign(0,row_y,label);
          if (_view==3) {
            const uint8_t* bits=form(_engine.child(i)).preview;
            for(int row=0;row<8;++row) for(int col=0;col<8;++col)
              if(bits[row] & (1<<col)) d.fillRect(d.width()-18+col*2,row_y+row,2,1);
          }
        }
      }
      return 5000;
    }
    snprintf(text,sizeof(text),"%s L%u",form(s.form).name,_engine.level());
    if (_engine.sleeping() || _engine.paused())
      snprintf(text,sizeof(text),"%s %s",form(s.form).name,_engine.paused()?"Rest":"Sleep");
    d.drawTextLeftAlign(0,y,text);
    auto presentation=look;
    if (_engine.sleeping() || _engine.paused()) {
      presentation.pose=_engine.paused()?PetPersonality::REST:PetPersonality::SLEEP;
      presentation.quirk=PetPersonality::NONE; presentation.phrase=nullptr;
    }
    PetPersonalityView::render(d,y,bottom,form(s.form),presentation);
    snprintf(text,sizeof(text),"Energy %u",s.energy);
    d.drawTextLeftAlign(0,y+step,text);
    snprintf(text,sizeof(text),"Full %u",s.fullness);
    d.drawTextLeftAlign(0,y+2*step,text);
    snprintf(text,sizeof(text),"XP %u",s.xp);
    d.drawTextLeftAlign(0,y+3*step,text);
    if (y+4*step+d.getLineHeight()<=bottom) d.drawTextLeftAlign(0,y+4*step,_engine.paused()?"Low Power":_engine.sleeping()?"Sleeping":
        _engine.ready()?"Evolve":"Care");
    return d.isEink() ? 30000 : look.active && look.quirk!=PetPersonality::NONE ? 500 : 5000;
  }
};

} }
