#pragma once

#include "PetEngine.h"
#include "PetAssets.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <stdio.h>

namespace zen { namespace pet {

// Rendering has no input, persistence or gameplay ownership.
class Renderer {
public:
  static int render(ZenDisplayDriver& d, int y, int bottom,
                    const Engine& _engine, uint8_t _view, uint8_t _selection,
                    uint8_t _pose, uint32_t _reaction_until) {
    char text[40]; const State& s = _engine.state();
    int step = d.lineStep();
    if (_view) {
      if (_view == 2) {
        char rows[4][40];
        snprintf(rows[0],sizeof(rows[0]),"%s L%u",form(s.form).name,_engine.level());
        if (_engine.level() == Evolution::LEVELS)
          snprintf(rows[1],sizeof(rows[1]),"Final form XP %u",s.xp);
        else snprintf(rows[1],sizeof(rows[1]),"XP %u/%u",s.xp,Evolution::xp(_engine.level()));
        snprintf(rows[2],sizeof(rows[2]),"Energy %u Food %u",s.energy,s.food);
        snprintf(rows[3],sizeof(rows[3]),"Full %u Bond %u/%u",s.fullness,s.bond,
                 Evolution::bond(_engine.level()));
        bool hungry = _engine.hungerRate() > 5;
        int count = hungry ? 6 : 4;
        int first = hungry && _selection ? 2 : 0;
        for (int i=first; i<count && y+d.getLineHeight()<=bottom; ++i,y+=step)
          d.drawTextLeftAlign(0,y,i<4 ? rows[i] : i==4 ? "Charge to reduce" : "hunger");
      } else {
        static const char* ITEMS[] = {"Feed","Train","Evolve","Details","Daily Rewards","Wake Up"};
        int count = _view==3?_engine.choices():_engine.sleeping()?6:5;
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
    bool reaction = (int32_t)(_reaction_until-millis())>0;
    uint8_t pose = _engine.sleeping() || _engine.paused() ? 1 : reaction ? _pose : 0;
    int space = bottom - y;
    int size = form(s.form).size;
    int available = space-d.getLineHeight()-1;
    if (available < size) size = available;
    if (size < 8) size = 8;
    int x = d.width()-size-8, sy=y+space-size;
    if (!d.isEink() && pose==0 && !_engine.paused()) sy -= (millis()/500)&1;
    const Form& asset = form(s.form);
    int source=asset.source_size;
    for(int row=0;row<source;++row) for(int col=0;col<source;++col)
      if(asset.pixel(col,row))
        d.fillRect(x+col*size/source,sy+row*size/source,
                   (col+1)*size/source-col*size/source,(row+1)*size/source-row*size/source);
    d.setColor(ZenDisplayDriver::DARK);
    d.fillRect(x+2*size/8,sy+3*size/8,size/8,pose==1?1:size/8);
    d.fillRect(x+5*size/8,sy+3*size/8,size/8,pose==1?1:size/8);
    d.fillRect(x+3*size/8,sy+5*size/8,size/4,pose==2?size/8:1);
    d.setColor(ZenDisplayDriver::LIGHT);
    if(pose==3) d.drawTextLeftAlign(x+size,sy,"!");
    if(pose==1) d.drawTextLeftAlign(x+size,sy,"z");
    snprintf(text,sizeof(text),"Energy %u",s.energy);
    d.drawTextLeftAlign(0,y+step,text);
    snprintf(text,sizeof(text),"Full %u",s.fullness);
    d.drawTextLeftAlign(0,y+2*step,text);
    snprintf(text,sizeof(text),"XP %u",s.xp);
    d.drawTextLeftAlign(0,y+3*step,text);
    if (y+4*step+d.getLineHeight()<=bottom) d.drawTextLeftAlign(0,y+4*step,_engine.paused()?"Low Power":_engine.sleeping()?"Sleeping":
        _engine.ready()?"Evolve":"Care");
    return d.isEink() || pose==1 ? 30000 : 500;
  }
};

} }
