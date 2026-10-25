#pragma once
#include <MeshCore.h>
#include <string.h>
namespace zen {
// Repeater plaintext commands share the same remote replay floor as REQ/login.
// Preserve a logical message's wire timestamp across retries; an evicted retry
// fails locally rather than executing an old command as a fresh request.
class RequestMessageClock {
  struct Entry {uint8_t key[32]{};uint32_t original=0,fingerprint=0,wire=0;} _entries[4];
  unsigned _next=0;
public:
  uint32_t timestamp(const uint8_t* key,uint32_t original,uint8_t attempt,const char* text,mesh::RTCClock& clock) {
    uint32_t hash=2166136261u;for(auto p=(const uint8_t*)text;*p;++p)hash=(hash^*p)*16777619u;
    if(attempt) {
      for(const auto& e:_entries)if(e.wire && e.original==original && e.fingerprint==hash && !memcmp(e.key,key,32))return e.wire;
      return 0;
    }
    uint32_t wire=clock.getCurrentTimeUnique();if(!wire)return 0;
    auto& e=_entries[_next++%4];memcpy(e.key,key,32);e.original=original;e.fingerprint=hash;e.wire=wire;return wire;
  }
};
}
