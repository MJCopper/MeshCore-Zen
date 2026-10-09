#pragma once

#include "PetAssets.h"
#include "PetPersonality.h"
#include "PetPortraitLayout.h"
#include "PetPortraitDrawing.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <string.h>

namespace zen { namespace pet {

// Portrait-area rendering only. All motion and decorations stay clipped away
// from the left-hand statistics and header; E-INK uses one static pose.
struct PetPersonalityView {
  static void bubble(ZenDisplayDriver& d,const PetPortraitLayout& layout,const char* phrase) {
    int left=layout.left,right=layout.right,top=layout.top,bottom=layout.bottom;
    if(phrase) {
      char lines[2][24]={{0},{0}}; unsigned length=strlen(phrase);
      int columns=(right-left-4)/d.getCharWidth();
      if(columns>23) columns=23;
      if(columns>0 && length<=(unsigned)(columns*2)) {
        unsigned split=length;
        if(length>(unsigned)columns) {
          split=columns;
          while(split>0 && phrase[split]!=' ') --split;
        }
        unsigned second=split<length?split+1:length;
        if(split>0 && length-second<=(unsigned)columns) {
          memcpy(lines[0],phrase,split);
          memcpy(lines[1],phrase+second,length-second);
          int rows=second<length?2:1;
          int h=layout.bubbleHeight(d.getLineHeight(),rows);
          if(h) {
            d.setColor(ZenDisplayDriver::DARK);
            d.fillRect(left,top,right-left,h+1);
            d.setColor(ZenDisplayDriver::LIGHT);
            d.drawRect(left,top,right-left,h);
            for(int row=0;row<rows;++row) d.drawTextLeftAlign(left+2,top+2+row*d.getLineHeight(),lines[row]);
            d.fillRect(right-8,top+h,2,1);
          }
        }
      }
    }
  }
  static void render(ZenDisplayDriver& d,int y,int bottom,const Form& asset,
                     const PetPersonality::Presentation& p) {
    using P=PetPersonality;
    auto layout=PetPortraitLayout::calculate(d.width(),d.getLineHeight(),y,bottom,asset.size);
    int left=layout.left,right=layout.right,top=layout.top;
    int scale=d.getLineHeight()/8; if(scale<1)scale=1;
    bool moving=p.active && !d.isEink();
    int size=layout.size;
    if(size<4) return;
    int x=layout.x,sy=layout.y;
    int dx=moving?p.offset_x:0,dy=moving?p.offset_y:0;
    if(dx<-layout.range_x)dx=-layout.range_x; if(dx>layout.range_x)dx=layout.range_x;
    if(dy<-layout.range_y)dy=-layout.range_y; if(dy>layout.range_y)dy=layout.range_y;
    x+=dx; sy+=dy;
    int frame=d.isEink()?1:p.frame;
    if(p.quirk==P::HOP && frame%2) sy-=2;
    bool tilt=(p.quirk==P::TILT && (frame==1 || frame==2)) || p.pose==P::SULKING;
    int look=p.quirk==P::LOOK_LEFT?-1:p.quirk==P::LOOK_RIGHT?1:0;
    bool blink=p.quirk==P::BLINK && frame==1;
    PetPortraitDrawing::draw(d,asset,x,sy,size,left,top,right,bottom,p.pose,
        tilt?(p.pose==P::SULKING?-1:1):0,look,blink,p.pose==P::SLEEPY,
        p.quirk==P::STRETCH && frame%2);
    // Small decorations distinguish the shared expressions without new sprites.
    if(p.pose==P::PROUD || p.pose==P::EXCITED) {
      PetPortraitDrawing::rect(d,x+size+1,sy+3,3*scale,scale,left,top,right,bottom);
      PetPortraitDrawing::rect(d,x+size+1+scale,sy+3-scale,scale,3*scale,left,top,right,bottom);
    }
    if((p.pose==P::SLEEP || p.pose==P::SLEEPY) && sy>=top && sy+d.getLineHeight()<=bottom)
      d.drawTextLeftAlign(x+size+1,sy,"z");
    bubble(d,layout,p.phrase);
  }
};

} }
