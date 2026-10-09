#pragma once

#include "PetAssets.h"
#include "PetPersonalityAssets.h"
#include <helpers/ui/ZenDisplayDriver.h>

namespace zen { namespace pet {

// Shared square portrait primitive. Previews use the exact same neutral face
// and silhouette, without animation or a separate thumbnail asset.
struct PetPortraitDrawing {
  static void rect(ZenDisplayDriver& d,int x,int y,int w,int h,
                   int left,int top,int right,int bottom) {
    int end_x=x+w,end_y=y+h;
    if(x<left)x=left; if(y<top)y=top;
    if(end_x>right)end_x=right; if(end_y>bottom)end_y=bottom;
    if(end_x>x && end_y>y)d.fillRect(x,y,end_x-x,end_y-y);
  }
  static void draw(ZenDisplayDriver& d,const Form& asset,int x,int y,int size,
                   int left,int top,int right,int bottom,uint8_t pose=0,
                   int tilt=0,int look=0,bool blink=false,bool sleepy=false,
                   bool stretch=false,bool inverted=false) {
    if(size<16 || size%16)return;
    int scale=size/16,face_scale=size/8;
    auto ink=inverted?ZenDisplayDriver::DARK:ZenDisplayDriver::LIGHT;
    auto paper=inverted?ZenDisplayDriver::LIGHT:ZenDisplayDriver::DARK;
    d.setColor(ink);
    for(int row=0;row<16;++row)for(int col=0;col<16;++col)
      if(asset.pixel(col,row)) {
        int dx=row<8?tilt:0,dy=stretch && row<14?-1:0;
        rect(d,x+col*scale+dx,y+row*scale+dy,scale,scale,left,top,right,bottom);
      }
    int face_y=y-(stretch?1:0);
    // Match the body tilt; all paired face cells have the same dimensions.
    int face_x=x+tilt;
    // The silhouette already supplies the face background. Filling a face
    // rectangle here would square off the cheeks and hide branch features.
    d.setColor(paper);
    const uint8_t* face=PetPersonalityAssets::face(pose);
    for(int row=2;row<7;++row)for(int col=1;col<7;++col) {
      uint8_t bits=blink && row==2?0:blink && row==3?0x66:face[row];
      if(bits&(1u<<col))rect(d,face_x+col*face_scale+look,face_y+row*face_scale,
          face_scale,row==3 && (blink || sleepy)?1:face_scale,left,top,right,bottom);
    }
    d.setColor(ZenDisplayDriver::LIGHT);
  }
};

} }
