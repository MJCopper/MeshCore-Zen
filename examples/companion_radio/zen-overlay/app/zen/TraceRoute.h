#pragma once
#include <Packet.h>
#include <string.h>
namespace zen {
struct TraceRoute {
  static const uint8_t MAX_REPEATERS=10;
  static const uint8_t MAX_PATH_BYTES=(2*MAX_REPEATERS-1)*2;
  uint8_t path[MAX_PATH_BYTES]{};
  uint8_t bytes=0,repeaters=0,flags=0;
  enum Validation { VALID,INVALID,EMPTY,TOO_LONG };
  static Validation fromOutbound(const uint8_t* via,uint8_t encoded,const uint8_t* target,TraceRoute& out) {
    if (!target || !mesh::Packet::isValidPathLen(encoded)) return INVALID;
    unsigned width=(encoded>>6)+1,count=encoded&63;
    if (width>2 || (count && !via)) return INVALID;
    if (count+1>MAX_REPEATERS) return TOO_LONG;
    out=TraceRoute();out.repeaters=count+1;out.flags=width-1;
    out.bytes=(2*out.repeaters-1)*width;
    for(unsigned i=0;i<count;++i) memcpy(out.path+i*width,via+i*width,width);
    memcpy(out.path+count*width,target,width);
    for(unsigned i=0;i<count;++i) memcpy(out.path+(count+1+i)*width,via+(count-1-i)*width,width);
    return VALID;
  }
  bool equals(const TraceRoute& other) const {
    return flags==other.flags && bytes==other.bytes && !memcmp(path,other.path,bytes);
  }
  uint8_t maxPacketLength() const { return 2+9+bytes+2*repeaters-1; }
};
// Ported sensor candidate ranking: directed edges, with self as an endpoint.
inline bool traceSameVertex(const TraceRoute& a,int i,const TraceRoute& b,int j) {
  int an=2*a.repeaters-1,bn=2*b.repeaters-1;
  bool as=i<0||i==an,bs=j<0||j==bn;
  if(as||bs)return as==bs;
  unsigned width=a.flags<b.flags?1<<a.flags:1<<b.flags;
  return !memcmp(a.path+i*(1<<a.flags),b.path+j*(1<<b.flags),width);
}
inline unsigned traceNovelEdges(const TraceRoute& route,const TraceRoute& original) {
  unsigned novel=0;
  for(int i=-1;i<2*route.repeaters-1;++i) {
    bool found=false;
    for(int j=-1;j<2*original.repeaters-1;++j)
      if(traceSameVertex(route,i,original,j)&&traceSameVertex(route,i+1,original,j+1)){found=true;break;}
    if(!found)++novel;
  }
  return novel;
}
} // namespace zen
