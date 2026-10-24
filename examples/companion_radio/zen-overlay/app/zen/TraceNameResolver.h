#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <helpers/UTF8Helpers.h>
namespace zen {
class TraceNameResolver {
  unsigned _matches=0;char _name[32]{};
public:
  void observe(const uint8_t* prefix,unsigned width,const uint8_t* key,const char* name) {
    if(!prefix||!key||!name||width<1||width>2||memcmp(prefix,key,width)) return;
    ++_matches;
    size_t n=mesh::validUtf8PrefixLength(name,sizeof(_name)-1);
    memcpy(_name,name,n);_name[n]=0;
  }
  void format(const uint8_t* prefix,unsigned width,char* out,size_t size) const {
    if(!size)return;
    if(!prefix||width<1||width>2){snprintf(out,size,"?");return;}
    if(_matches==1&&_name[0]) {
      size_t n=mesh::validUtf8PrefixLength(_name,size-1);memcpy(out,_name,n);out[n]=0;return;
    }
    snprintf(out,size,_matches>1?"?%02X":"%02X",prefix[0]);
    if(width==2){size_t n=strlen(out);snprintf(out+n,size-n,"%02X",prefix[1]);}
  }
};
} // namespace zen
