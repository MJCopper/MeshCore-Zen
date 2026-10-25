#pragma once
#include <Arduino.h>
#include "ZenPrefsCodec.h"

namespace zen {
// Loop-owned transaction layer for Zen files. MeshCore retains filesystem and
// baseline-data ownership. One scratch buffer serves streaming reads/writes.
class ZenStore {
public:
  enum class Failure : uint8_t { NONE,FILESYSTEM_UNAVAILABLE,WRITE_LOCKED,ENCODE_FAILED,
    TEMP_WRITE_FAILED,TEMP_VERIFY_FAILED,RENAME_FAILED,BACKUP_FAILED,PROMOTION_FAILED,
    PROMOTION_VERIFY_FAILED,READ_FAILED,NO_MEMORY,BUSY,SETTINGS_CHANGED };
private:
  FILESYSTEM* _fs=nullptr;
  uint64_t _fingerprint=0;
  uint32_t _generation=0;
  bool _external=false,_fingerprint_valid=false,_write_locked=false,_recovery_needed=false;
  bool _busy=false,_newer=false;
  uint8_t _loaded=0; // 1=primary, 2=staging, 3=backup; verified at load.
  Failure _failure=Failure::NONE;
  uint8_t _buffer[128]{};
  struct BusyGuard {bool& busy;explicit BusyGuard(bool& b):busy(b){busy=true;}~BusyGuard(){busy=false;}};
  static File openRead(FILESYSTEM* fs,const char* path){
#if defined(RP2040_PLATFORM)
    return fs->open(path,"r");
#else
    return fs->open(path);
#endif
  }
  static File openWrite(FILESYSTEM* fs,const char* path){
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
    fs->remove(path);return fs->open(path,FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
    return fs->open(path,"w");
#else
    return fs->open(path,"w",true);
#endif
  }
  class Source {
    File& _file;uint8_t* _scratch;size_t _size,_at=0,_count=0;bool _failed=false;
    bool fill(){if(_failed)return false;if(_at<_count)return true;int n=_file.read(_scratch,128);if(n<0)_failed=true;_at=0;_count=n>0?size_t(n):0;return _count>0;}
  public:
    Source(File& file,uint8_t* scratch):_file(file),_scratch(scratch),_size(file.size()){}
    size_t size()const{return _size;}
    int read(uint8_t* out,size_t n){size_t total=0;while(n){if(!fill())break;size_t k=n<_count-_at?n:_count-_at;
      memcpy(out,_scratch+_at,k);_at+=k;out+=k;n-=k;total+=k;}return int(total);}
    bool matches(const uint8_t* data,size_t n){while(n){if(!fill())return false;size_t k=n<_count-_at?n:_count-_at;
      if(memcmp(_scratch+_at,data,k))return false;_at+=k;data+=k;n-=k;}return true;}
  };
  class Sink {
    File& _file;uint8_t* _scratch;size_t _count=0;bool _good=true;
  public:
    Sink(File& file,uint8_t* scratch):_file(file),_scratch(scratch){}
    bool finish(){if(_good && _count)_good=_file.write(_scratch,_count)==_count;_count=0;return _good;}
    size_t write(const uint8_t* data,size_t n){const size_t total=n;while(n && _good){size_t k=n<128-_count?n:128-_count;
      memcpy(_scratch+_count,data,k);_count+=k;data+=k;n-=k;if(_count==128)finish();}return _good?total:0;}
  };
  struct CompareSink {Source& source;size_t write(const uint8_t* data,size_t n){return source.matches(data,n)?n:0;}};
  template<class Expected> bool matches(const char* path,size_t size,Expected expected){
    File file=openRead(_fs,path);if(!file)return false;Source source(file,_buffer);
    bool ok=source.size()==size && expected(source);file.close();return ok;
  }
  bool prepareRecovery(){
    if(_loaded!=2 && _loaded!=3)return true;
    const char* path=_loaded==2?PREFS_TMP_PATH:PREFS_BAK_PATH;
    // Do not truncate the only verified record while starting another save.
    _fs->remove(PREFS_PATH);
    if(!_fs->rename(path,PREFS_PATH)){_failure=Failure::PROMOTION_FAILED;return false;}
    _loaded=1;return true;
  }
  template<class Verify> bool promote(const char* temporary,const char* final_path,const char* backup,Verify verify,bool settings){
    File existing=openRead(_fs,final_path);bool had=bool(existing);if(existing)existing.close();
    if(had){_fs->remove(backup);if(!_fs->rename(final_path,backup)){_failure=Failure::BACKUP_FAILED;return false;}}
    if(!_fs->rename(temporary,final_path)){
      if(had)_fs->rename(backup,final_path);
      _failure=Failure::PROMOTION_FAILED;return false;
    }
    if(!verify(final_path)){
      _failure=Failure::PROMOTION_VERIFY_FAILED;
      bool restored=false;
      if(had){_fs->remove(final_path);restored=_fs->rename(backup,final_path);}
      if(settings){_recovery_needed=true;_loaded=had?(restored?1:3):1;}
      return false; // The previous verified record is primary or backup.
    }
    _fs->remove(backup);if(settings)_loaded=1;return true;
  }
  bool protectCandidates(){
    // Check all headers before migration can replace any recovery file.
    const char* candidates[]={PREFS_PATH,PREFS_TMP_PATH,PREFS_BAK_PATH};
    for(const char* path:candidates){
      File file=openRead(_fs,path);
      if(!file){if(_fs->exists(path)){_write_locked=true;_failure=Failure::READ_FAILED;return false;}continue;}
      Source source(file,_buffer);uint8_t header[5]{};size_t size=source.size();
      int count=size>=sizeof(header)?source.read(header,sizeof(header)):0;file.close();
      if(size>=sizeof(header) && count!=sizeof(header)){_write_locked=true;_failure=Failure::READ_FAILED;return false;}
      if(ZenPrefsCodec::hasNewerVersion(header,count)){
        _write_locked=_newer=true;_failure=Failure::WRITE_LOCKED;return false;
      }
    }
    return true;
  }
  template<class Allocator> bool loadCandidate(const char* path,ZenSettings& prefs){
    File file=openRead(_fs,path);if(!file)return false;
    Source source(file,_buffer);ZenPrefsCodec::Status status;uint32_t gen=0;uint8_t version=0;
    bool ok=ZenPrefsCodec::decodeStream<Source,Allocator>(source,prefs,&gen,&status,&version);file.close();
    if(!ok){
      if(status==ZenPrefsCodec::NEWER_VERSION){_write_locked=_newer=true;_failure=Failure::WRITE_LOCKED;}
      else if(status==ZenPrefsCodec::NO_MEMORY){_write_locked=true;_failure=Failure::NO_MEMORY;}
      else if(status==ZenPrefsCodec::IO_ERROR){_write_locked=true;_failure=Failure::READ_FAILED;}
      return false;
    }
    _generation=gen;_fingerprint=ZenPrefsCodec::fingerprint(prefs);_fingerprint_valid=true;
    _recovery_needed=version<3;return true;
  }
public:
  static constexpr const char* PREFS_PATH="/zen_prefs";
  static constexpr const char* PREFS_TMP_PATH="/zen_prefs.tmp";
  static constexpr const char* PREFS_BAK_PATH="/zen_prefs.bak";
  void begin(FILESYSTEM* external,FILESYSTEM* fallback){_fs=external?external:fallback;_external=external!=nullptr;}
  template<class Allocator=SettingsAllocator> bool load(ZenSettings& prefs){
    if(_busy){_failure=Failure::BUSY;return false;}BusyGuard guard(_busy);
    if(!_fs){_failure=Failure::FILESYSTEM_UNAVAILABLE;return false;}
    _failure=Failure::NONE;_newer=_write_locked=_recovery_needed=false;_loaded=0;
    if(!protectCandidates())return false;
    if(loadCandidate<Allocator>(PREFS_PATH,prefs)){_loaded=1;return true;}
    if(_write_locked)return false;
    if(loadCandidate<Allocator>(PREFS_TMP_PATH,prefs)){_loaded=2;_fingerprint_valid=false;_recovery_needed=true;return true;}
    if(_write_locked)return false;
    if(loadCandidate<Allocator>(PREFS_BAK_PATH,prefs)){_loaded=3;_fingerprint_valid=false;_recovery_needed=true;return true;}
    return false;
  }
  bool save(const ZenSettings& prefs){
    if(_busy){_failure=Failure::BUSY;return false;}BusyGuard guard(_busy);_failure=Failure::NONE;
    if(!_fs){_failure=Failure::FILESYSTEM_UNAVAILABLE;return false;}
    if(_write_locked){_failure=Failure::WRITE_LOCKED;return false;}
    if(_generation==UINT32_MAX){_failure=Failure::ENCODE_FAILED;return false;}
    // All mutation is loop-owned. The hash additionally detects edits made by
    // a yielding filesystem callback, preventing a mixed settings transaction.
    if(!prepareRecovery())return false;
    const uint64_t before=ZenPrefsCodec::fingerprint(prefs);uint32_t generation=_generation+1;
    File file=openWrite(_fs,PREFS_TMP_PATH);if(!file){_failure=Failure::TEMP_WRITE_FAILED;return false;}
    Sink sink(file,_buffer);bool encoded=ZenPrefsCodec::encodeStream(prefs,generation,sink);
    bool written=encoded && sink.finish();file.close();
    if(!written){_failure=Failure::TEMP_WRITE_FAILED;_fs->remove(PREFS_TMP_PATH);return false;}
    if(before!=ZenPrefsCodec::fingerprint(prefs)){_failure=Failure::SETTINGS_CHANGED;_fs->remove(PREFS_TMP_PATH);return false;}
    auto verify=[&](const char* path){return matches(path,ZenPrefsCodec::encodedSize(),[&](Source& source){
      CompareSink compare{source};return ZenPrefsCodec::encodeStream(prefs,generation,compare);});};
    if(!verify(PREFS_TMP_PATH)){_failure=Failure::TEMP_VERIFY_FAILED;_fs->remove(PREFS_TMP_PATH);return false;}
    if(before!=ZenPrefsCodec::fingerprint(prefs)){_failure=Failure::SETTINGS_CHANGED;_fs->remove(PREFS_TMP_PATH);return false;}
    if(!promote(PREFS_TMP_PATH,PREFS_PATH,PREFS_BAK_PATH,verify,true))return false;
    if(before!=ZenPrefsCodec::fingerprint(prefs)){_failure=Failure::SETTINGS_CHANGED;return false;}
    _generation=generation;_fingerprint=before;_fingerprint_valid=true;_recovery_needed=false;_failure=Failure::NONE;return true;
  }
  bool available()const{return _fs!=nullptr;}bool external()const{return _external;}
  File openRead(const char* path){return openRead(_fs,path);}File openWrite(const char* path){return openWrite(_fs,path);}
  bool remove(const char* path){return _fs && _fs->remove(path);}
  bool writeLocked()const{return _write_locked;}bool newerVersion()const{return _newer;}
  bool recoveryNeeded()const{return _recovery_needed;}Failure failure()const{return _failure;}
  uint32_t generation()const{return _generation;}
  bool replaceExtension(const char* temporary,const char* final_path,const char* backup,const uint8_t* data,size_t size){
    if(_busy){_failure=Failure::BUSY;return false;}BusyGuard guard(_busy);_failure=Failure::NONE;
    if(!_fs){_failure=Failure::FILESYSTEM_UNAVAILABLE;return false;}
    if(!data || !size || size>ZenPrefsCodec::MAX_ENCODED_SIZE){_failure=Failure::ENCODE_FAILED;return false;}
    File file=openWrite(_fs,temporary);if(!file){_failure=Failure::TEMP_WRITE_FAILED;return false;}
    Sink sink(file,_buffer);bool ok=sink.write(data,size)==size && sink.finish();file.close();
    if(!ok){_failure=Failure::TEMP_WRITE_FAILED;_fs->remove(temporary);return false;}
    auto verify=[&](const char* path){return matches(path,size,[&](Source& source){return source.matches(data,size);});};
    if(!verify(temporary)){_failure=Failure::TEMP_VERIFY_FAILED;_fs->remove(temporary);return false;}
    return promote(temporary,final_path,backup,verify,false);
  }
};
static_assert(sizeof(ZenStore)<=192,"Streaming settings store exceeds its RAM budget");
} // namespace zen
