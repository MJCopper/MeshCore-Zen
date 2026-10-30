#pragma once
#include "ZenSettingsFields.h"
#include <new>
#include <string.h>
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Zen settings wire scalars require a little-endian adapter on this target"
#endif

namespace zen {
struct SettingsAllocator {
  static ZenSettings* create(const ZenSettings& initial) { return new(std::nothrow) ZenSettings(initial); }
  static void destroy(ZenSettings* p) { delete p; }
};

// One canonical encoder drives flash writes, exact verification and hashing.
// No complete encoded record is allocated by any production entry point.
class ZenPrefsCodec {
  static constexpr uint32_t MAGIC=0x46504E5AUL;
  static constexpr uint8_t VERSION=3;
  enum Record : uint8_t { REC_CHILD=1,REC_QUIET,REC_SYSTEM,REC_UI,REC_NOTIFICATIONS,
    REC_QUICK_REPLIES,REC_RADIO_PRESETS,REC_ADVERT,REC_ZEN_TAIL,REC_PET,REC_TRACE };
public:
  static constexpr size_t MAX_ENCODED_SIZE=4096; // Input bound, not buffer size.
  enum Status : uint8_t { OK,INVALID,IO_ERROR,NEWER_VERSION,NO_MEMORY };
  struct MemorySource {
    const uint8_t* data;size_t length,at=0;
    MemorySource(const uint8_t* p,size_t n):data(p),length(p?n:0){}
    size_t size() const{return length;}
    int read(uint8_t* p,size_t n){if(n>length-at)n=length-at;memcpy(p,data+at,n);at+=n;return int(n);}
  };
private:
  struct MemorySink {
    uint8_t* data;size_t capacity,at=0;
    MemorySink(uint8_t* p,size_t n):data(p),capacity(p?n:0){}
    size_t write(const uint8_t* p,size_t n){if(n>capacity-at)return 0;memcpy(data+at,p,n);at+=n;return n;}
  };
  struct HashSink {
    uint64_t hash=14695981039346656037ULL;
    size_t write(const uint8_t* p,size_t n){for(size_t i=0;i<n;++i)hash=(hash^p[i])*1099511628211ULL;return n;}
  };
  template<class Sink> struct Writer {
    Sink& sink;bool good=true;uint32_t hash=2166136261UL;
    explicit Writer(Sink& s):sink(s){}
    void put(const void* data,size_t n,bool digest=true) {
      auto p=static_cast<const uint8_t*>(data);
      while(n && good){size_t k=n>128?128:n;if(sink.write(p,k)!=k){good=false;return;}
        if(digest)for(size_t i=0;i<k;++i)hash=(hash^p[i])*16777619UL;
        p+=k;n-=k;}
    }
    void u8(uint8_t v){put(&v,1);}
    void u16(uint16_t v){uint8_t b[2]={uint8_t(v),uint8_t(v>>8)};put(b,2);}
    void u32(uint32_t v,bool digest=true){uint8_t b[4]={uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)};put(b,4,digest);}
  };
  template<class Source> struct Reader {
    Source& source;bool good=true,io=false;uint32_t hash=2166136261UL;
    explicit Reader(Source& s):source(s){}
    bool get(void* data,size_t n,bool digest=true){auto p=static_cast<uint8_t*>(data);
      while(n && good){size_t k=n>128?128:n;int got=source.read(p,k);
        if(got<=0 || size_t(got)>k){io=true;good=false;break;}
        if(digest)for(int i=0;i<got;++i)hash=(hash^p[i])*16777619UL;
        p+=got;n-=got;}return good;}
    uint8_t u8(){uint8_t v=0;get(&v,1);return v;}
    uint16_t u16(){uint8_t b[2]={};get(b,2);return b[0]|uint16_t(b[1])<<8;}
    uint32_t u32(bool digest=true){uint8_t b[4]={};get(b,4,digest);return b[0]|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;}
  };
  template<class R> struct RecordReader {
    R& reader;size_t left;bool good=true;
    RecordReader(R& r,size_t n):reader(r),left(n){}
    bool get(void* p,size_t n){if(n>left){good=false;return false;}left-=n;return good=reader.get(p,n)&&good;}
    uint8_t u8(){uint8_t v=0;get(&v,1);return v;}
    uint16_t u16(){uint8_t b[2]={};get(b,2);return b[0]|uint16_t(b[1])<<8;}
    uint32_t u32(){uint8_t b[4]={};get(b,4);return b[0]|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;}
    bool skip(){uint8_t scratch[16];while(left && good)get(scratch,left>sizeof(scratch)?sizeof(scratch):left);return good;}
  };
  template<class R> static void legacyRecord(R& r,uint8_t id,uint16_t len,ZenSettings& p,bool& trace_seen,bool& trace_login) {
      if(id==REC_TRACE && len==1) { trace_seen=true;trace_login=r.u8()==1; }
      else if (id == REC_PET && len == 1) {
        p.pet_enabled = r.u8() == 1;
      } else if (id == REC_CHILD && len == 9) {
        p.child_mode_enabled = r.u8(); p.child_mode_pin_hash = r.u32();
        p.child_visible_pages = r.u16(); p.child_channels_enabled = r.u8(); p.child_rooms_enabled = r.u8();
      } else if (id == REC_QUIET && len == 5) {
        p.quiet_time_enabled = r.u8(); p.quiet_time_start_min = r.u16(); p.quiet_time_end_min = r.u16();
      } else if (id == REC_SYSTEM && len == 9) {
        p.bluetooth_enabled = r.u8(); p.gps_adaptive = r.u8(); p.timezone_mode = r.u8();
        p.timezone_city = r.u8(); r.get(&p.timezone_manual_min, sizeof(p.timezone_manual_min));
        p.notification_screen_wake = r.u8(); p.zen_config_schema = r.u16();
      } else if (id == REC_UI) {
        const uint16_t expected = 2 + 1 + ZenSettings::PAGE_ORDER_LEN + 1 + 2 + 11 + 8 + 1 +
                                  sizeof(p.favourite_contacts);
        const uint16_t legacy_expected = expected - 2;
        if (len == expected || len == legacy_expected) {
          p.home_pages_mask = r.u16(); p.page_order_set = r.u8(); r.get(p.page_order, sizeof(p.page_order));
          p.display_brightness = r.u8(); r.get(&p.auto_off_secs, sizeof(p.auto_off_secs));
          p.clock_12h = r.u8();
          if (len == expected) p.clock_hide_seconds = r.u8();
          p.use_lemon_font = r.u8();
          p.display_rotation = r.u8();
          p.joystick_rotation = r.u8(); p.eink_full_refresh_every = r.u8(); p.keyboard_type = r.u8();
          p.batt_display_mode = r.u8();
          if (len == expected) p.units_imperial = r.u8();
          p.dm_show_all = r.u8(); p.room_fav_only = r.u8();
          r.get(&p.ch_fav_bitmask, sizeof(p.ch_fav_bitmask)); p.ch_fav_only = r.u8();
          r.get(p.favourite_contacts, sizeof(p.favourite_contacts));
        }
      } else if (id == REC_NOTIFICATIONS) {
        const uint16_t expected = 8 + 2 + sizeof(p.ringtone_notes) + 2 + sizeof(p.ringtone2_notes) +
                                  sizeof(p.ch_notif_override) + sizeof(p.ch_notif_muted) +
                                  sizeof(p.dm_notif) + sizeof(p.dm_melody) + sizeof(p.channel_melody_overrides);
        const uint16_t legacy_expected = expected - 4 - sizeof(p.ringtone_notes) - sizeof(p.ringtone2_notes);
        if (len == expected || len == legacy_expected) {
          (void)r.u8(); // legacy duplicate; MeshCore /prefs.json is authoritative
          p.buzzer_volume = r.u8(); p.buzzer_auto = r.u8();
          p.notif_melody_dm = r.u8(); p.notif_melody_ch = r.u8(); p.notif_melody_ad = r.u8();
          p.notif_melody_new_contact = r.u8(); p.advert_sound_scope = r.u8();
          if (len == expected) {
            p.ringtone_bpm_idx = r.u8(); p.ringtone_len = r.u8();
            r.get(p.ringtone_notes, sizeof(p.ringtone_notes));
            p.ringtone2_bpm_idx = r.u8(); p.ringtone2_len = r.u8();
            r.get(p.ringtone2_notes, sizeof(p.ringtone2_notes));
          }
          r.get(&p.ch_notif_override, sizeof(p.ch_notif_override));
          r.get(&p.ch_notif_muted, sizeof(p.ch_notif_muted));
          r.get(p.dm_notif, sizeof(p.dm_notif)); r.get(p.dm_melody, sizeof(p.dm_melody));
          r.get(p.channel_melody_overrides, sizeof(p.channel_melody_overrides));
        }
      } else if (id == REC_QUICK_REPLIES && len == 5 * sizeof(p.custom_msgs[0])) {
        r.get(p.custom_msgs, 5 * sizeof(p.custom_msgs[0]));
      } else if (id == REC_RADIO_PRESETS &&
                 len == ZenSettings::USER_RADIO_PRESET_MAX * (16 + 4 + 4 + 1 + 1)) {
        for (uint8_t i = 0; i < ZenSettings::USER_RADIO_PRESET_MAX; i++) {
          r.get(p.user_radio_presets[i].name, sizeof(p.user_radio_presets[i].name));
          r.get(&p.user_radio_presets[i].freq, sizeof(float));
          r.get(&p.user_radio_presets[i].bw, sizeof(float));
          p.user_radio_presets[i].sf = r.u8(); p.user_radio_presets[i].cr = r.u8();
        }
      } else if (id == REC_ADVERT && len == 4) {
        r.get(&p.advert_auto_interval_sec, sizeof(p.advert_auto_interval_sec));
      } else if (id == REC_ZEN_TAIL && len==LEGACY_SETTINGS_SIZE) {
        for(size_t at=0;at<len;++at) {
          uint8_t byte=r.u8();
          for(const auto& field:SETTINGS_FIELDS)
            if(at>=field.legacy && at<size_t(field.legacy)+field.size)
              reinterpret_cast<uint8_t*>(&p)[field.offset+at-field.legacy]=byte;
          if(at>=2192 && at<2304)
            reinterpret_cast<uint8_t*>(p.user_radio_presets)[at-2192]=byte;
        }
      }
  }
public:
  static size_t encodedSize(){size_t n=17+3+104+4;for(const auto& f:SETTINGS_FIELDS)n+=3+f.size;return n;}
  template<class Sink> static bool encodeStream(const ZenSettings& p,uint32_t generation,Sink& sink) {
    static_assert(sizeof(float)==4,"Radio preset format requires IEEE binary32");
    const size_t size=encodedSize();if(size>MAX_ENCODED_SIZE)return false;
    Writer<Sink> w(sink);w.u32(MAGIC);w.u8(VERSION);w.u16(size-17);w.u32(generation);w.u16(p.zen_config_schema);
    for(const auto& f:SETTINGS_FIELDS){w.u8(f.tag);w.u16(f.size);w.put(reinterpret_cast<const uint8_t*>(&p)+f.offset,f.size);}
    w.u8(REC_RADIO_PRESETS);w.u16(104);
    for(const auto& v:p.user_radio_presets){w.put(v.name,16);w.put(&v.freq,4);w.put(&v.bw,4);w.u8(v.sf);w.u8(v.cr);}
    w.u8(REC_TRACE);w.u16(1);w.u8(p.tracePublicLogin()?1:0);
    uint32_t hash=w.hash;w.u32(hash,false);return w.good;
  }
  static uint64_t fingerprint(const ZenSettings& p){HashSink sink;return encodeStream(p,0,sink)?sink.hash:0;}
  static size_t encode(const ZenSettings& p,uint32_t gen,uint8_t* out,size_t capacity){
    MemorySink sink(out,capacity);return encodeStream(p,gen,sink)?sink.at:0;
  }
  static bool hasNewerVersion(const uint8_t* p,size_t n){return p && n>=5 && p[0]=='Z' && p[1]=='N' && p[2]=='P' && p[3]=='F' && p[4]>VERSION;}
  template<class Source,class Allocator=SettingsAllocator>
  static bool decodeStream(Source& source,ZenSettings& destination,uint32_t* generation=nullptr,
                           Status* status=nullptr,uint8_t* format=nullptr) {
    Status result=INVALID;
    struct ResultGuard {Status* out;Status& value;~ResultGuard(){if(out)*out=value;}} result_guard{status,result};
    Reader<Source> reader(source);
    if(source.size()<5)return false;
    uint32_t magic=reader.u32();uint8_t version=reader.u8();
    if(!reader.good){result=IO_ERROR;return false;}
    if(magic!=MAGIC || version<1)return false;
    if(version>VERSION){result=NEWER_VERSION;return false;}
    if(source.size()<17 || source.size()>MAX_ENCODED_SIZE)return false;
    uint16_t payload=reader.u16();uint32_t gen=reader.u32();(void)reader.u16();
    if(!reader.good){result=IO_ERROR;return false;}
    if(size_t(payload)+17!=source.size())return false;
    ZenSettings* staged=Allocator::create(destination);
    if(!staged){result=NO_MEMORY;return false;}
    struct StageGuard {ZenSettings* p;~StageGuard(){Allocator::destroy(p);}} stage_guard{staged};
    staged->multi_flood_mode=0; // Missing field in every older record means Off.
    if(version<3)staged->pet_enabled=0;
    bool trace_seen=false,trace_login=true;size_t remaining=payload;
    while(remaining && reader.good){
      if(remaining<3)return false;
      uint8_t id=reader.u8();uint16_t length=reader.u16();remaining-=3;
      if(length>remaining)return false;
      RecordReader<Reader<Source>> record(reader,length);
      if(version<3)legacyRecord(record,id,length,*staged,trace_seen,trace_login);
      else if(id==REC_TRACE){if(length!=1)return false;trace_seen=true;trace_login=record.u8()==1;}
      else if(id==REC_RADIO_PRESETS){
        if(length!=104)return false;
        for(auto& v:staged->user_radio_presets){record.get(v.name,16);record.get(&v.freq,4);record.get(&v.bw,4);v.sf=record.u8();v.cr=record.u8();}
      } else {
        for(const auto& f:SETTINGS_FIELDS)if(id==f.tag){
          if(length!=f.size)return false;
          record.get(reinterpret_cast<uint8_t*>(staged)+f.offset,f.size);break;
        }
      }
      if(!record.skip()){result=reader.io?IO_ERROR:INVALID;return false;}
      remaining-=length;
    }
    uint32_t computed=reader.hash;uint32_t supplied=reader.u32(false);
    if(!reader.good){result=IO_ERROR;return false;}
    if(remaining || supplied!=computed || staged->multi_flood_mode>2)return false;
    staged->setTracePublicLogin(trace_seen?trace_login:true);
    destination=*staged;if(generation)*generation=gen;if(format)*format=version;result=OK;return true;
  }
  static bool decode(ZenSettings& p,const uint8_t* data,size_t n,uint32_t* gen=nullptr){MemorySource source(data,n);return decodeStream(source,p,gen);}
  static bool inspect(const uint8_t* data,size_t n,uint32_t& generation){
    if(!data || n<17 || n>MAX_ENCODED_SIZE)return false;
    MemorySource src(data,n);Reader<MemorySource> r(src);
    if(r.u32()!=MAGIC)return false;uint8_t version=r.u8();if(version<1 || version>VERSION)return false;
    uint16_t bytes=r.u16();uint32_t gen=r.u32();(void)r.u16();if(size_t(bytes)+17!=n)return false;
    size_t left=bytes;while(left){if(left<3)return false;(void)r.u8();uint16_t k=r.u16();left-=3;if(k>left)return false;
      RecordReader<Reader<MemorySource>> record(r,k);if(!record.skip())return false;left-=k;}
    uint32_t hash=r.hash;if(hash!=r.u32(false)||!r.good)return false;generation=gen;return true;
  }
};
} // namespace zen
