#pragma once

#include <stdint.h>

namespace zen { namespace pet {

// Pure geometry. Reserve the combined quirk/decorations footprint before
// assigning wandering space; speech overlays never change the portrait size.
struct PetPortraitLayout {
  int left,top,right,bottom,size,x,y;
  int8_t range_x=0,range_y=0;
  int bubbleHeight(int line,int rows) const {
    if(rows<1 || rows>2)return 0;
    int height=rows*line+4;
    return top+height+1<=bottom?height:0;
  }
  static PetPortraitLayout calculate(int width,int line,int start,int end,int desired) {
    PetPortraitLayout p;
    p.left=width/2+2; p.right=width-2;
    p.top=start+line+1; p.bottom=end;
    int scale=line/8; if(scale<1) scale=1;
    int horizontal=6+6*scale,vertical=4;
    p.size=desired;
    if(p.size>p.right-p.left-horizontal) p.size=p.right-p.left-horizontal;
    if(p.size>p.bottom-p.top-vertical) p.size=p.bottom-p.top-vertical;
    if(p.size<0) p.size=0;
    int spare_x=p.right-p.left-horizontal-p.size;
    int spare_y=p.bottom-p.top-vertical-p.size;
    if(spare_x<0)spare_x=0; if(spare_y<0)spare_y=0;
    p.range_x=spare_x/2>4?4:spare_x/2;
    p.range_y=spare_y/2>2?2:spare_y/2;
    p.x=p.left+2+spare_x/2;
    // Keep the face low, away from top-anchored speech, without sacrificing
    // downward wandering room or the upper hop/stretch margin.
    p.y=p.bottom-p.size-1-p.range_y;
    return p;
  }
};

} }
