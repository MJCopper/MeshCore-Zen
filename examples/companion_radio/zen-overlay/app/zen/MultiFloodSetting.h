#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace zen {
// The synchronous transactional saver cannot yield to the radio loop. Restore
// the previous value on failure, and never save queries/unchanged selections.
struct MultiFloodSetting {
  template<class Prefs,class Save>
  static bool set(Prefs& prefs,uint8_t mode,Save save) {
    if(mode>2)return false;
    const uint8_t before=prefs.multi_flood_mode;
    if(before==mode)return true;
    prefs.multi_flood_mode=mode;
    if(save())return true;
    prefs.multi_flood_mode=before;return false;
  }
  static bool whitespace(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
  static const char* skipSpace(const char* p) { while(whitespace(*p))++p;return p; }
  template<class Set>
  static bool handleCommand(const char* command,char* reply,uint8_t current,Set set) {
    constexpr size_t length=sizeof("get multi.flood")-1;
    bool get=!strncmp(command,"get multi.flood",length);
    bool put=!strncmp(command,"set multi.flood",length);
    if((!get && !put) || (command[length] && !whitespace(command[length])))return false;
    const char* value=skipSpace(command+length);
    if(get) {
      if(*value)strcpy(reply,"Error, invalid argument");
      else snprintf(reply,16,"> %u",unsigned(current));
      return true;
    }
    if(*value<'0' || *value>'2') { strcpy(reply,"Error, invalid value");return true; }
    uint8_t mode=*value++-'0';
    if(*skipSpace(value)) { strcpy(reply,"Error, invalid value");return true; }
    strcpy(reply,set(mode)?"OK":"Error, save failed");return true;
  }
};
} // namespace zen
