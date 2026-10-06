#pragma once

#include "PetAssets.h"
#include "PetPersonality.h"
#include <helpers/ui/ZenDisplayDriver.h>
#include <string.h>

namespace zen { namespace pet {

// Portrait-area rendering only. All motion and decorations stay clipped away
// from the left-hand statistics and header; E-INK uses one static pose.
struct PetPersonalityView {
  static void rect(ZenDisplayDriver& d,int x,int y,int w,int h,
                   int left,int top,int right,int bottom) {
    int end_x=x+w,end_y=y+h;
    if(x<left) x=left; if(y<top) y=top;
    if(end_x>right) end_x=right; if(end_y>bottom) end_y=bottom;
    if(end_x>x && end_y>y) d.fillRect(x,y,end_x-x,end_y-y);
  }
  static void render(ZenDisplayDriver& d,int y,int bottom,const Form& asset,
                     const PetPersonality::Presentation& p) {
    using P=PetPersonality;
    int left=d.width()/2+2,right=d.width()-2;
    int top=y+d.getLineHeight()+1,available=bottom-top;
    int scale=d.getLineHeight()/8; if(scale<1) scale=1;
    if(p.phrase && available>=2*d.getLineHeight()+10) {
      char lines[2][24]={{0},{0}}; unsigned length=strlen(p.phrase);
      int columns=(right-left-4)/d.getCharWidth();
      if(columns>23) columns=23;
      if(columns>0 && length<=(unsigned)(columns*2)) {
        unsigned split=length;
        if(length>(unsigned)columns) {
          split=columns;
          while(split>0 && p.phrase[split]!=' ') --split;
        }
        unsigned second=split<length?split+1:length;
        if(split>0 && length-second<=(unsigned)columns) {
          memcpy(lines[0],p.phrase,split);
          memcpy(lines[1],p.phrase+second,length-second);
          int rows=second<length?2:1;
          int h=rows*d.getLineHeight()+4;
          if(bottom-(top+h+1)>=8) {
            d.drawRect(left,top,right-left,h);
            for(int row=0;row<rows;++row) d.drawTextLeftAlign(left+2,top+2+row*d.getLineHeight(),lines[row]);
            d.fillRect(right-8,top+h,2,1); top+=h+1;
          }
        }
      }
    }
    int size=asset.size;
    if(size>bottom-top-1) size=bottom-top-1;
    if(size>right-left-4) size=right-left-4;
    if(size<4) return;
    int x=left+(right-left-size)/2,sy=bottom-size-1;
    int frame=d.isEink()?1:p.frame;
    if(p.quirk==P::HOP && frame%2) sy-=2;
    if(p.quirk==P::STRETCH && frame%2) { ++size; --sy; }
    bool tilt=(p.quirk==P::TILT && (frame==1 || frame==2)) || p.pose==P::SULKING;
    for(int row=0;row<asset.source_size;++row) for(int col=0;col<asset.source_size;++col)
      if(asset.pixel(col,row)) {
        int offset=tilt && row<asset.source_size/2?(p.pose==P::SULKING?-1:1):0;
        rect(d,x+col*size/asset.source_size+offset,sy+row*size/asset.source_size,
             (col+1)*size/asset.source_size-col*size/asset.source_size,
             (row+1)*size/asset.source_size-row*size/asset.source_size,left,top,right,bottom);
      }
    int look=p.quirk==P::LOOK_LEFT?-1:p.quirk==P::LOOK_RIGHT?1:0;
    const uint8_t* face=PetPersonalityAssets::face(p.pose);
    d.setColor(ZenDisplayDriver::LIGHT);
    rect(d,x+size/8,sy+2*size/8,6*size/8,5*size/8,left,top,right,bottom);
    d.setColor(ZenDisplayDriver::DARK);
    bool blink=p.quirk==P::BLINK && frame==1;
    for(int row=2;row<7;++row) for(int col=1;col<7;++col) {
      uint8_t bits=blink && row==2?0:blink && row==3?0x66:face[row];
      if(bits&(1<<col))
        rect(d,x+col*size/8+look,sy+row*size/8,(col+1)*size/8-col*size/8,
             row==3 && (blink || p.pose==P::SLEEPY)?1:(row+1)*size/8-row*size/8,
             left,top,right,bottom);
    }
    d.setColor(ZenDisplayDriver::LIGHT);
    // Small decorations distinguish the shared expressions without new sprites.
    if(p.pose==P::PROUD || p.pose==P::EXCITED) {
      rect(d,x+size+1,sy+3,3*scale,scale,left,top,right,bottom);
      rect(d,x+size+1+scale,sy+3-scale,scale,3*scale,left,top,right,bottom);
    }
    if((p.pose==P::SLEEP || p.pose==P::SLEEPY) && sy>=top && sy+d.getLineHeight()<=bottom)
      d.drawTextLeftAlign(right-d.getCharWidth(),sy,"z");
  }
};

} }
