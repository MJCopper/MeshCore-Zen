#pragma once

#include "PetEngine.h"
#include "PetPortraitDrawing.h"

namespace zen { namespace pet {

// Evolution choices have their own roomy rows, independent of the care menu.
// Use native square portraits; small surfaces scroll one choice at a time.
struct PetEvolutionView {
  struct Layout {
    int row_height,visible,preview_size,preview_x,label_width;
  };
  static Layout layout(int width,int line,int start,int bottom,int count) {
    Layout p;
    int available=bottom-start;
    p.preview_size=width>=40 && available>=20?16:0;
    p.row_height=line+4;
    if(p.preview_size && p.row_height<20)p.row_height=20;
    p.visible=available/p.row_height;
    if(p.visible<1)p.visible=1;
    if(p.visible>count)p.visible=count;
    p.preview_x=width-p.preview_size-2;
    p.label_width=p.preview_size?p.preview_x-6:width-4;
    return p;
  }
  static void render(ZenDisplayDriver& d,int y,int bottom,const Engine& engine,
                     uint8_t selection) {
    int count=engine.choices(); if(!count)return;
    auto p=layout(d.width(),d.getLineHeight(),y,bottom,count);
    int first=selection>=p.visible?selection-p.visible+1:0;
    for(int i=first;i<count && i<first+p.visible;++i) {
      int row_y=y+(i-first)*p.row_height;
      if(row_y+p.row_height>bottom)return;
      const auto& asset=form(engine.child(i)); bool selected=i==selection;
      d.drawSelectionRow(0,row_y,d.width(),p.row_height-1,selected);
      d.drawTextEllipsized(2,row_y+(p.row_height-d.getLineHeight())/2,
                          p.label_width,asset.name);
      if(p.preview_size)PetPortraitDrawing::draw(d,asset,p.preview_x,
          row_y+(p.row_height-p.preview_size)/2,p.preview_size,
          p.preview_x,row_y,d.width()-2,row_y+p.row_height,0,0,0,false,false,false,selected);
    }
    d.setColor(ZenDisplayDriver::LIGHT);
  }
};

} }
