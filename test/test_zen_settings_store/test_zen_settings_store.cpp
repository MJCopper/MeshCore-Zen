#include <gtest/gtest.h>
#include <map>
#include <vector>
#include <set>
#include <string>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <cstring>
class SettingsFS;
class File {
  SettingsFS* fs=nullptr;std::string path;size_t at=0;bool writing=false;
public:
  File()=default;File(SettingsFS* f,const std::string& p,bool w):fs(f),path(p),writing(w){}
  explicit operator bool()const{return fs!=nullptr;}
  size_t size();int read(uint8_t*,size_t);size_t write(const uint8_t*,size_t);void close();
};
class SettingsFS {
public:
  std::map<std::string,std::vector<uint8_t>> files;
  int mutations=0,cut=0,reads=0,fail_read=0,renames=0;
  bool full=false,short_write=false,overwrite=false;
  size_t max_read=128,max_write_seen=0,max_read_seen=0;
  std::set<int> fail_rename;
  std::function<void()> on_write,on_read;
  void mutation(){if(cut && ++mutations==cut)throw std::runtime_error("Power loss");if(!cut)++mutations;}
  File open(const char* p){return files.count(p)?File(this,p,false):File();}
  File open(const char* p,const char*,bool=true){if(full)return File();files[p].clear();mutation();return File(this,p,true);}
  bool exists(const char* p)const{return files.count(p);}
  bool remove(const char* p){files.erase(p);mutation();return true;}
  bool rename(const char* from,const char* to){++renames;if(fail_rename.count(renames)||!files.count(from)||(!overwrite && files.count(to)))return false;
    files[to]=files[from];files.erase(from);mutation();return true;}
};
size_t File::size(){return fs->files[path].size();}
int File::read(uint8_t* p,size_t n){fs->max_read_seen=std::max(fs->max_read_seen,n);if(++fs->reads==fs->fail_read)return -1;
  if(fs->on_read)fs->on_read();auto& v=fs->files[path];n=std::min(n,fs->max_read);n=std::min(n,v.size()-at);memcpy(p,v.data()+at,n);at+=n;return int(n);}
size_t File::write(const uint8_t* p,size_t n){fs->max_write_seen=std::max(fs->max_write_seen,n);if(fs->short_write && n)--n;auto& v=fs->files[path];v.resize(std::max(v.size(),at+n));memcpy(v.data()+at,p,n);at+=n;fs->mutation();if(fs->on_write)fs->on_write();return n;}
void File::close(){if(fs && writing)fs->mutation();fs=nullptr;}
#define FILESYSTEM SettingsFS
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenStore.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/BaselinePrefsFingerprint.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenPrefsDefaults.h"
using namespace zen;
static std::vector<uint8_t> encode(const ZenSettings& p,uint32_t gen=1){std::vector<uint8_t> b(ZenPrefsCodec::MAX_ENCODED_SIZE);auto n=ZenPrefsCodec::encode(p,gen,b.data(),b.size());EXPECT_GT(n,0u);b.resize(n);return b;}
static std::vector<uint8_t> fixture(int version){std::ifstream f("test/fixtures/zen_settings/v"+std::to_string(version)+".bin",std::ios::binary);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
static void rehash(std::vector<uint8_t>& b){size_t payload=b.size()-17;b[5]=payload;b[6]=payload>>8;uint32_t h=2166136261u;for(size_t i=0;i<b.size()-4;++i)h=(h^b[i])*16777619u;memcpy(b.data()+b.size()-4,&h,4);}
struct NoMemory {static ZenSettings* create(const ZenSettings&){return nullptr;}static void destroy(ZenSettings*){}};
TEST(ZenSettingsStore, MemoryBudgetsAndEveryTaggedFieldRoundTripWithoutBaselineMutation) {
  EXPECT_LE(sizeof(ZenStore),192u);EXPECT_LE(sizeof(ZenSettings),1536u);EXPECT_LT(sizeof(ZenPrefs),2356u);
  ZenPrefs before,after;before.ble_pin=123456;after.ble_pin=654321;after.freq=916.575;after.gps_interval=123;
  auto baseline=BaselinePrefsFingerprint::calculate(after);
  for(const auto& field:SETTINGS_FIELDS)for(unsigned i=0;i<field.size;++i)reinterpret_cast<uint8_t*>(&before.zenSettings())[field.offset+i]=uint8_t(field.tag+i);
  for(auto& preset:before.user_radio_presets){strcpy(preset.name,"Narrow");preset.freq=916.575;preset.bw=62.5;preset.sf=7;preset.cr=8;}
  before.setTracePublicLogin(false);auto bytes=encode(before);uint32_t gen=0;
  ASSERT_TRUE(ZenPrefsCodec::decode(after,bytes.data(),bytes.size(),&gen));EXPECT_EQ(1u,gen);
  EXPECT_EQ(bytes,encode(after));EXPECT_EQ(baseline,BaselinePrefsFingerprint::calculate(after));
}
TEST(ZenSettingsStore, FrozenLegacyVersionsMigrateAllActiveSettingsAndDiscardRetiredStorage) {
  for(int version=1;version<=2;++version){auto bytes=fixture(version);ASSERT_EQ(3914u,bytes.size());ZenPrefs p;p.ble_pin=765432;auto base=BaselinePrefsFingerprint::calculate(p);
    uint32_t gen=0;ASSERT_TRUE(ZenPrefsCodec::decode(p,bytes.data(),bytes.size(),&gen));EXPECT_EQ(42u,gen);
    EXPECT_EQ(3,p.display_brightness);EXPECT_EQ(60,p.auto_off_secs);EXPECT_EQ(1,p.pet_enabled);EXPECT_EQ(0,p.bluetooth_enabled);EXPECT_EQ(630,p.timezone_manual_min);
    EXPECT_EQ(0x12345678u,p.child_mode_pin_hash);EXPECT_EQ(1260,p.quiet_time_start_min);EXPECT_EQ(2,p.notification_screen_wake);EXPECT_EQ(3,p.notif_melody_new_contact);
    EXPECT_EQ(0xA5,p.favourite_contacts[2][3]);EXPECT_EQ(17,p.ringtone_notes[3]);EXPECT_EQ(18,p.ringtone2_notes[2]);EXPECT_EQ(0x32,p.channel_melody_overrides[0]);
    EXPECT_FALSE(p.tracePublicLogin());EXPECT_EQ(1,p.gps_adaptive);EXPECT_EQ(12,p.page_order[0]);EXPECT_EQ(10800u,p.advert_auto_interval_sec);
    EXPECT_STREQ("Australia",p.user_radio_presets[1].name);EXPECT_FLOAT_EQ(916.575f,p.user_radio_presets[1].freq);EXPECT_EQ(8,p.user_radio_presets[1].cr);
    for(unsigned i=0;i<5;++i){EXPECT_EQ(139u,strlen(p.custom_msgs[i]));EXPECT_EQ('A'+i,p.custom_msgs[i][0]);}
    EXPECT_EQ(base,BaselinePrefsFingerprint::calculate(p));auto compact=encode(p);EXPECT_LT(compact.size(),2000u);
    ZenPrefs restored;ASSERT_TRUE(ZenPrefsCodec::decode(restored,compact.data(),compact.size()));EXPECT_EQ(compact,encode(restored));
  }
}
TEST(ZenSettingsStore, CorruptionTruncationAndInvalidLengthsNeverPartiallyCommit) {
  ZenPrefs p;p.display_brightness=2;auto baseline=encode(p);auto bytes=fixture(2);
  for(size_t size=0;size<bytes.size();++size){EXPECT_FALSE(ZenPrefsCodec::decode(p,bytes.data(),size));EXPECT_EQ(baseline,encode(p));}
  for(size_t i=0;i<bytes.size();++i){bytes[i]^=0x40;EXPECT_FALSE(ZenPrefsCodec::decode(p,bytes.data(),bytes.size()))<<i;bytes[i]^=0x40;}
  auto current=encode(p);current[14]=255;current[15]=127;rehash(current);EXPECT_FALSE(ZenPrefsCodec::decode(p,current.data(),current.size()));EXPECT_EQ(baseline,encode(p));
}
TEST(ZenSettingsStore, UnknownRecordsAndMaximumQuickRepliesAreBoundedAndCompatible) {
  ZenPrefs p;for(unsigned i=0;i<5;++i){memset(p.custom_msgs[i],'x',139);memcpy(p.custom_msgs[i]+135,"\xE7\x95\x8C",3);p.custom_msgs[i][139]=0;}
  auto original=encode(p);auto extra=original;extra.insert(extra.begin()+13,{250,5,0,1,2,3,4,5});rehash(extra);
  ZenPrefs restored;ASSERT_TRUE(ZenPrefsCodec::decode(restored,extra.data(),extra.size()));EXPECT_EQ(original,encode(restored));
}
TEST(ZenSettingsStore, StreamedSaveVerificationAndGenerationWorkWithShortReadsAndBothRenamePolicies) {
  for(bool overwrite:{false,true}){SettingsFS fs;fs.overwrite=overwrite;fs.max_read=7;fs.files["/identity"]={1,2,3};fs.files["/prefs.json"]={4,5,6};ZenStore store;store.begin(&fs,nullptr);ZenPrefs p;p.display_brightness=1;
    ASSERT_TRUE(store.save(p));EXPECT_EQ(1u,store.generation());p.display_brightness=4;ASSERT_TRUE(store.save(p));EXPECT_EQ(2u,store.generation());
    ZenStore reboot;reboot.begin(&fs,nullptr);ZenPrefs restored;ASSERT_TRUE(reboot.load(restored));EXPECT_EQ(4,restored.display_brightness);EXPECT_EQ(2u,reboot.generation());
    EXPECT_LE(fs.max_write_seen,128u);EXPECT_LE(fs.max_read_seen,128u);EXPECT_EQ((std::vector<uint8_t>{1,2,3}),fs.files["/identity"]);EXPECT_EQ((std::vector<uint8_t>{4,5,6}),fs.files["/prefs.json"]);
  }
}
TEST(ZenSettingsStore, AllocationOrReadFailureLocksWritesAndPreservesExistingFilesAndMemory) {
  for(bool oom:{false,true}){SettingsFS fs;ZenPrefs p;p.display_brightness=4;fs.files["/zen_prefs"]=encode(p);ZenStore store;store.begin(&fs,nullptr);p.display_brightness=1;auto prior=fs.files;
    if(oom)EXPECT_FALSE(store.load<NoMemory>(p));else{fs.fail_read=1;EXPECT_FALSE(store.load(p));}
    EXPECT_EQ(1,p.display_brightness);EXPECT_TRUE(store.writeLocked());EXPECT_FALSE(store.newerVersion());EXPECT_FALSE(store.save(p));EXPECT_EQ(prior,fs.files);
  }
}
TEST(ZenSettingsStore, NewerVersionInAnyRecoveryPositionIsNeverOverwritten) {
  for(const char* path:{"/zen_prefs","/zen_prefs.tmp","/zen_prefs.bak"}){SettingsFS fs;ZenPrefs p;auto future=encode(p);++future[4];fs.files[path]=future;if(std::string(path)!="/zen_prefs")fs.files["/zen_prefs"]={1,2,3};auto prior=fs.files;
    ZenStore store;store.begin(&fs,nullptr);EXPECT_FALSE(store.load(p));EXPECT_TRUE(store.newerVersion());EXPECT_FALSE(store.save(p));EXPECT_EQ(prior,fs.files);
  }
}
TEST(ZenSettingsStore, SaveFailuresAndConcurrentEditsNeverCommitSuccessOrDiscardOldSettings) {
  for(int mode=0;mode<6;++mode){SettingsFS fs;ZenStore store;store.begin(&fs,nullptr);ZenPrefs p;p.display_brightness=1;ASSERT_TRUE(store.save(p));p.display_brightness=4;fs.renames=0;
    if(mode==0)fs.full=true;if(mode==1)fs.short_write=true;if(mode==2)fs.fail_read=fs.reads+1;if(mode==3)fs.fail_rename={1};if(mode==4)fs.fail_rename={2};if(mode==5)fs.on_write=[&](){p.clock_12h=1;};
    EXPECT_FALSE(store.save(p))<<mode;EXPECT_EQ(1u,store.generation());fs.full=fs.short_write=false;fs.fail_read=0;fs.fail_rename.clear();fs.on_write={};
    ZenStore reboot;reboot.begin(&fs,nullptr);ZenPrefs restored;ASSERT_TRUE(reboot.load(restored))<<mode;EXPECT_EQ(1,restored.display_brightness)<<mode;
  }
}
TEST(ZenSettingsStore, PowerLossAtEverySaveMutationRecoversCompleteOldOrNewSettings) {
  for(bool overwrite:{false,true}){SettingsFS initial;initial.overwrite=overwrite;ZenStore store;store.begin(&initial,nullptr);ZenPrefs old;old.display_brightness=1;strcpy(old.custom_msgs[0],"Old");ASSERT_TRUE(store.save(old));auto files=initial.files;
    ZenPrefs next;next.display_brightness=4;strcpy(next.custom_msgs[0],"New");SettingsFS measured;measured.overwrite=overwrite;measured.files=files;ZenStore m;m.begin(&measured,nullptr);ZenPrefs scratch;ASSERT_TRUE(m.load(scratch));measured.mutations=0;ASSERT_TRUE(m.save(next));int steps=measured.mutations;
    for(int cut=1;cut<=steps;++cut){SettingsFS fs;fs.overwrite=overwrite;fs.files=files;ZenStore current;current.begin(&fs,nullptr);ASSERT_TRUE(current.load(scratch));fs.mutations=0;fs.cut=cut;
      try{current.save(next);}catch(const std::runtime_error&){}fs.cut=0;ZenStore reboot;reboot.begin(&fs,nullptr);ZenPrefs restored;ASSERT_TRUE(reboot.load(restored))<<cut;
      bool old_ok=restored.display_brightness==1&&!strcmp(restored.custom_msgs[0],"Old");bool new_ok=restored.display_brightness==4&&!strcmp(restored.custom_msgs[0],"New");EXPECT_TRUE(old_ok||new_ok)<<cut;
    }
  }
}
TEST(ZenSettingsStore, LegacyMigrationAndSoleRecoveryRecordSurviveEverySaveMutation) {
  for(const char* path:{"/zen_prefs","/zen_prefs.tmp","/zen_prefs.bak"}){auto bytes=fixture(2);
    SettingsFS probe;probe.files[path]=bytes;ZenStore measured;measured.begin(&probe,nullptr);ZenPrefs initial;
    ASSERT_TRUE(measured.load(initial));probe.mutations=0;ASSERT_TRUE(measured.save(initial));const int steps=probe.mutations;
    for(int cut=1;cut<=steps;++cut){SettingsFS fs;fs.files[path]=bytes;ZenStore store;store.begin(&fs,nullptr);ZenPrefs p;ASSERT_TRUE(store.load(p));ASSERT_TRUE(store.recoveryNeeded());fs.mutations=0;fs.cut=cut;
      try{store.save(p);}catch(const std::runtime_error&){}fs.cut=0;ZenStore reboot;reboot.begin(&fs,nullptr);ZenPrefs restored;ASSERT_TRUE(reboot.load(restored))<<path<<cut;EXPECT_EQ(3,restored.display_brightness);EXPECT_STREQ("Australia",restored.user_radio_presets[1].name);
    }
  }
}
TEST(ZenSettingsStore, VerificationRejectsValidChecksumButUnexpectedPayloadAndRetainsBackup) {
  SettingsFS fs;ZenStore store;store.begin(&fs,nullptr);ZenPrefs p;p.display_brightness=1;ASSERT_TRUE(store.save(p));p.display_brightness=4;
  // The final-file verification must compare expected bytes, not only a valid
  // envelope/generation. Mutate a fully valid replacement after promotion.
  bool changed=false;fs.on_read=[&](){if(!changed && fs.files.count("/zen_prefs.bak") && fs.files.count("/zen_prefs")){changed=true;ZenPrefs wrong;wrong.display_brightness=2;fs.files["/zen_prefs"]=encode(wrong,2);}};
  EXPECT_FALSE(store.save(p));fs.on_read={};
  ZenStore reboot;reboot.begin(&fs,nullptr);ZenPrefs restored;ASSERT_TRUE(reboot.load(restored));EXPECT_EQ(1,restored.display_brightness);
}

struct CountStages {
  static int live,peak,total;
  static ZenSettings* create(const ZenSettings& initial){++live;++total;peak=std::max(peak,live);return new ZenSettings(initial);}
  static void destroy(ZenSettings* p){--live;delete p;}
};
int CountStages::live=0;int CountStages::peak=0;int CountStages::total=0;
TEST(ZenSettingsStore, BootStagingIsSingleCompactAllocationAndAlwaysReleased) {
  CountStages::live=CountStages::peak=CountStages::total=0;
  for(int version=1;version<=2;++version){SettingsFS fs;fs.files["/zen_prefs"]=fixture(version);ZenStore store;store.begin(&fs,nullptr);ZenPrefs p;
    ASSERT_TRUE(store.load<CountStages>(p));EXPECT_EQ(0,CountStages::live);EXPECT_EQ(1,CountStages::peak);
  }
  EXPECT_EQ(2,CountStages::total);EXPECT_LE(sizeof(ZenSettings),1536u);
}
TEST(ZenSettingsStore, RawRoomAndPetRecordsLargerThanScratchAreComparedExactly) {
  for(size_t length:{1u,127u,128u,129u,700u,4096u}){SettingsFS fs;ZenStore store;store.begin(&fs,nullptr);
    std::vector<uint8_t> data(length);for(size_t i=0;i<length;++i)data[i]=uint8_t(i);
    ASSERT_TRUE(store.replaceExtension("/extension.tmp","/extension","/extension.bak",data.data(),data.size()));EXPECT_EQ(data,fs.files["/extension"]);
    data.back()^=1;ASSERT_TRUE(store.replaceExtension("/extension.tmp","/extension","/extension.bak",data.data(),data.size()));EXPECT_EQ(data,fs.files["/extension"]);
    EXPECT_LE(fs.max_read_seen,128u);EXPECT_LE(fs.max_write_seen,128u);
  }
}
TEST(ZenSettingsStore, FirstSaveInterruptedAtEveryMutationLeavesDefaultsOrCompleteNewRecord) {
  ZenPrefs next;next.display_brightness=4;strcpy(next.custom_msgs[0],"New");SettingsFS probe;ZenStore measured;measured.begin(&probe,nullptr);ASSERT_TRUE(measured.save(next));int steps=probe.mutations;
  for(int cut=1;cut<=steps;++cut){SettingsFS fs;ZenStore store;store.begin(&fs,nullptr);fs.cut=cut;
    try{store.save(next);}catch(const std::runtime_error&){}fs.cut=0;ZenStore reboot;reboot.begin(&fs,nullptr);ZenPrefs p;p.display_brightness=1;strcpy(p.custom_msgs[0],"Default");
    if(reboot.load(p)){EXPECT_EQ(4,p.display_brightness);EXPECT_STREQ("New",p.custom_msgs[0]);}
    else{EXPECT_EQ(1,p.display_brightness);EXPECT_STREQ("Default",p.custom_msgs[0]);}
  }
}
TEST(ZenSettingsStore, ReentrantSaveIsRejectedWithoutInterferingWithOuterCommit) {
  SettingsFS fs;ZenStore store;store.begin(&fs,nullptr);ZenPrefs p;bool tried=false;
  fs.on_write=[&](){if(!tried){tried=true;EXPECT_FALSE(store.save(p));EXPECT_EQ(ZenStore::Failure::BUSY,store.failure());}};
  EXPECT_TRUE(store.save(p));EXPECT_TRUE(tried);EXPECT_EQ(ZenStore::Failure::NONE,store.failure());
}

TEST(ZenSettingsStore, CanonicalHashTracksActiveEditsIgnoresPaddingAndResetPreservesBaseline) {
  ZenPrefs p;p.ble_pin=123456;p.freq=916.575;auto baseline=BaselinePrefsFingerprint::calculate(p);auto hash=ZenPrefsCodec::fingerprint(p);
  for(const auto& field:SETTINGS_FIELDS){auto* byte=reinterpret_cast<uint8_t*>(&p.zenSettings())+field.offset;*byte^=1;EXPECT_NE(hash,ZenPrefsCodec::fingerprint(p))<<unsigned(field.tag);*byte^=1;}
  auto* padding=reinterpret_cast<uint8_t*>(&p.user_radio_presets[0])+offsetof(ZenSettings::UserRadioPreset,cr)+1;
  *padding=0xAB;EXPECT_EQ(hash,ZenPrefsCodec::fingerprint(p));p.display_brightness=4;p.resetZen();
  EXPECT_EQ(0,p.display_brightness);EXPECT_EQ(baseline,BaselinePrefsFingerprint::calculate(p));EXPECT_EQ(hash,ZenPrefsCodec::fingerprint(p));
}
TEST(ZenSettingsStore, CompleteInputLimitIsIndependentOfScratchAndOversizeIsRejected) {
  ZenPrefs p;p.display_brightness=3;auto bytes=encode(p);size_t extra=4096-bytes.size();ASSERT_GT(extra,3u);
  std::vector<uint8_t> unknown(extra,0);unknown[0]=250;unknown[1]=uint8_t(extra-3);unknown[2]=uint8_t((extra-3)>>8);bytes.insert(bytes.begin()+13,unknown.begin(),unknown.end());rehash(bytes);
  SettingsFS fs;fs.files["/zen_prefs"]=bytes;ZenStore store;store.begin(&fs,nullptr);ZenPrefs restored;ASSERT_TRUE(store.load(restored));EXPECT_EQ(3,restored.display_brightness);
  bytes.insert(bytes.end()-4,0);rehash(bytes);EXPECT_FALSE(ZenPrefsCodec::decode(restored,bytes.data(),bytes.size()));EXPECT_EQ(3,restored.display_brightness);
  auto small=encode(p);std::vector<uint8_t> exact(small.size());EXPECT_EQ(small.size(),ZenPrefsCodec::encode(p,1,exact.data(),exact.size()));EXPECT_EQ(small,exact);
  EXPECT_EQ(0u,ZenPrefsCodec::encode(p,1,exact.data(),exact.size()-1));
}
TEST(ZenSettingsStore, RawExtensionTransactionsRecoverAtEveryMutationWithoutTouchingPreferences) {
  std::vector<uint8_t> before(700,1),after(700,2);SettingsFS initial;initial.files["/zen_prefs"]={9,8,7};ZenStore base;base.begin(&initial,nullptr);ASSERT_TRUE(base.replaceExtension("/room.tmp","/room","/room.bak",before.data(),before.size()));auto files=initial.files;
  SettingsFS probe;probe.files=files;ZenStore measured;measured.begin(&probe,nullptr);ASSERT_TRUE(measured.replaceExtension("/room.tmp","/room","/room.bak",after.data(),after.size()));int steps=probe.mutations;
  for(int cut=1;cut<=steps;++cut){SettingsFS fs;fs.files=files;ZenStore store;store.begin(&fs,nullptr);fs.cut=cut;
    try{store.replaceExtension("/room.tmp","/room","/room.bak",after.data(),after.size());}catch(const std::runtime_error&){}fs.cut=0;
    bool valid=false;for(const char* path:{"/room","/room.tmp","/room.bak"})if(fs.files.count(path) && (fs.files[path]==before||fs.files[path]==after))valid=true;
    EXPECT_TRUE(valid)<<cut;EXPECT_EQ((std::vector<uint8_t>{9,8,7}),fs.files["/zen_prefs"]);
  }
}

TEST(ZenSettingsStore, FrozenLegacyMappingPreservesEveryActiveField) {
  std::ifstream expected_file("test/fixtures/zen_settings/all_fields.records",std::ios::binary);
  std::vector<uint8_t> expected{std::istreambuf_iterator<char>(expected_file),std::istreambuf_iterator<char>()};ASSERT_FALSE(expected.empty());
  for(int version=1;version<=2;++version){std::ifstream input("test/fixtures/zen_settings/all_fields_v"+std::to_string(version)+".bin",std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};ASSERT_EQ(3914u,bytes.size());ZenPrefs p;
    ASSERT_TRUE(ZenPrefsCodec::decode(p,bytes.data(),bytes.size()));size_t at=0,count=0;
    while(at<expected.size()){ASSERT_LE(at+3,expected.size());uint8_t tag=expected[at++];uint16_t size=expected[at]|uint16_t(expected[at+1])<<8;at+=2;ASSERT_LE(at+size,expected.size());bool found=false;
      for(const auto& field:SETTINGS_FIELDS)if(field.tag==tag){ASSERT_EQ(size,field.size);EXPECT_EQ(0,memcmp(expected.data()+at,reinterpret_cast<const uint8_t*>(&p.zenSettings())+field.offset,size))<<unsigned(tag);found=true;++count;break;}
      EXPECT_TRUE(found);at+=size;
    }
    EXPECT_EQ(sizeof(SETTINGS_FIELDS)/sizeof(SETTINGS_FIELDS[0]),count);
  }
}

TEST(ZenSettingsStore, ValidOlderPrimaryDoesNotPermitOverwritingNewerRecoveryFiles) {
  for(const char* path:{"/zen_prefs.tmp","/zen_prefs.bak"}){SettingsFS fs;ZenPrefs p;fs.files["/zen_prefs"]=fixture(2);auto future=encode(p);++future[4];fs.files[path]=future;auto before=fs.files;
    ZenStore store;store.begin(&fs,nullptr);EXPECT_FALSE(store.load(p));EXPECT_TRUE(store.newerVersion());EXPECT_FALSE(store.save(p));EXPECT_EQ(before,fs.files);
  }
}

TEST(ZenSettingsStore, SavedQuietTimeChoiceAndScheduleOverrideNewFactoryDefaults) {
  for(uint8_t enabled : {0, 1}) {
    SettingsFS fs;
    ZenStore store;
    store.begin(&fs, nullptr);
    ZenPrefs saved;
    zen::PrefsDefaults::apply(saved);
    saved.quiet_time_enabled = enabled;
    saved.quiet_time_start_min = 20 * 60;
    saved.quiet_time_end_min = 6 * 60;
    ASSERT_TRUE(store.save(saved));
    ZenPrefs restored;
    zen::PrefsDefaults::apply(restored);
    ASSERT_EQ(1, restored.quiet_time_enabled);
    ZenStore reboot;
    reboot.begin(&fs, nullptr);
    ASSERT_TRUE(reboot.load(restored));
    zen::PrefsDefaults::normalize(restored);
    EXPECT_EQ(enabled, restored.quiet_time_enabled);
    EXPECT_EQ(20 * 60, restored.quiet_time_start_min);
    EXPECT_EQ(6 * 60, restored.quiet_time_end_min);
  }
}

TEST(ZenSettingsStore, LegacySavedQuietTimeChoiceOverridesNewFactoryDefault) {
  for(int version : {1, 2}) for(uint8_t enabled : {0, 1}) {
    auto bytes = fixture(version);
    for(size_t at = 13; at < bytes.size() - 4;) {
      uint8_t tag = bytes[at];
      uint16_t length = bytes[at + 1] | uint16_t(bytes[at + 2]) << 8;
      size_t body = at + 3;
      if(tag == 2) bytes[body] = enabled;
      if(tag == 9) bytes[body + 2611] = enabled;
      at = body + length;
    }
    rehash(bytes);
    ZenPrefs restored;
    zen::PrefsDefaults::apply(restored);
    ASSERT_TRUE(ZenPrefsCodec::decode(restored, bytes.data(), bytes.size()));
    zen::PrefsDefaults::normalize(restored);
    EXPECT_EQ(enabled, restored.quiet_time_enabled);
    EXPECT_EQ(21 * 60, restored.quiet_time_start_min);
    EXPECT_EQ(7 * 60, restored.quiet_time_end_min);
  }
}

int main(int argc,char** argv){::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}
