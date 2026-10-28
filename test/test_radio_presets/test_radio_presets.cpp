#define FIRMWARE_ZEN_BUILD 1
#include <gtest/gtest.h>
#include <vector>
#include <new>
#include <cstdlib>
#include "../../examples/companion_radio/zen-overlay/app/ui-new/RadioPresetPicker.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ZenPrefsCodec.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/BaselinePrefsFingerprint.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterTraceSession.h"
static bool track_allocations=false;
static size_t allocation_count=0;
void* operator new(std::size_t size) { if(track_allocations)++allocation_count;void* p=std::malloc(size?size:1);if(!p)throw std::bad_alloc();return p; }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

class RecordingDisplay : public ZenDisplayDriver {
public:
  struct Bounds { int x, y, w, h; };
  std::vector<Bounds> draws;
  int cursor_x = 0, cursor_y = 0;
  int char_w, line_h;
  bool single_font = false;
  bool isSingleFont() const override { return single_font; }

  RecordingDisplay(int w, int h, int cw = 6, int lh = 8)
      : ZenDisplayDriver(w, h), char_w(cw), line_h(lh) {}
  bool isOn() override { return true; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override {}
  void startFrame(ColorVal = 0) override { draws.clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int x, int y) override { cursor_x = x; cursor_y = y; }
  void print(const char* text) override {
    draws.push_back({cursor_x, cursor_y, (int)strlen(text) * char_w, line_h});
  }
  void fillRect(int x, int y, int w, int h) override { draws.push_back({x, y, w, h}); }
  void drawRect(int x, int y, int w, int h) override { draws.push_back({x, y, w, h}); }
  void drawXbm(int x, int y, const uint8_t*, int w, int h) override { draws.push_back({x, y, w, h}); }
  uint16_t getTextWidth(const char* text) override { return strlen(text) * char_w; }
  int getCharWidth() const override { return char_w; }
  int getLineHeight() const override { return line_h; }
  void endFrame() override {}
  bool inBounds() const {
    for (const Bounds& b : draws)
      if (b.x < 0 || b.y < 0 || b.w < 0 || b.h < 0 ||
          b.x + b.w > width() || b.y + b.h > height()) return false;
    return true;
  }
};

static RadioPresetPicker::Target target(ZenPrefs& p) { return {&p.freq,&p.bw,&p.sf,&p.cr}; }
static int row(const RadioPresetPicker& picker,const char* name) {
  for(int i=0;i<picker.menu.count();++i)if(!strcmp(picker.menu._items[i],name))return i;
  return -1;
}
static int builtin(const char* name) {
  for(int i=0;i<RADIO_PRESET_COUNT;++i)if(!strcmp(RADIO_PRESETS[i].name,name))return i;
  return -1;
}
static RadioPresetPicker::Result activate(RadioPresetPicker& picker,int index,ZenPrefs& p) {
  EXPECT_GE(index,0);EXPECT_LT(index,picker.menu.count());if(index<0||index>=picker.menu.count())return RadioPresetPicker::NONE;
  picker.menu.setSelected(index);EXPECT_EQ(PopupMenu::SELECTED,picker.menu.handleInput(KEY_ENTER));
  return picker.onSelected(picker.menu.selectedIndex(),&p,target(p));
}
static void goTo(RadioPresetPicker& picker,int page,ZenPrefs& p) {
  while(picker.page<page){ASSERT_GE(row(picker,"Next presets >"),0);EXPECT_EQ(RadioPresetPicker::NONE,activate(picker,row(picker,"Next presets >"),p));ASSERT_TRUE(picker.menu.active);}
  while(picker.page>page){ASSERT_GE(row(picker,"< Previous presets"),0);EXPECT_EQ(RadioPresetPicker::NONE,activate(picker,row(picker,"< Previous presets"),p));ASSERT_TRUE(picker.menu.active);}
}
static void users(ZenPrefs& p,int count) {
  for(int i=0;i<count;++i){auto& u=p.user_radio_presets[i];snprintf(u.name,sizeof(u.name),"Saved %d",i);u.freq=901+i;u.bw=125;u.sf=9;u.cr=6;}
}
TEST(RadioPresets, CurrentAustraliaNarrowAndEveryOfficialEntryCanBeSelected) {
  ASSERT_EQ(21,RADIO_PRESET_COUNT);int australia=builtin("Australia (Narrow)");ASSERT_GE(australia,0);
  EXPECT_FLOAT_EQ(916.575f,RADIO_PRESETS[australia].freq);EXPECT_FLOAT_EQ(62.5f,RADIO_PRESETS[australia].bw);EXPECT_EQ(7,RADIO_PRESETS[australia].sf);EXPECT_EQ(7,RADIO_PRESETS[australia].cr);
  for(int i=0;i<RADIO_PRESET_COUNT;++i){ZenPrefs p;p.freq=901;p.bw=125;p.sf=9;p.cr=6;p.getRadioPrefs()->setHashMode(2);users(p,4);RadioPresetPicker picker;
    picker.open(&p,target(p),"Radio Preset");goTo(picker,i/RadioPresetPicker::PAGE_SIZE,p);
    ASSERT_LE(picker.menu.count(),int(PopupMenu::PM_MAX_ITEMS));ASSERT_STREQ(RADIO_PRESETS[i].name,picker.menu._items[1+i%RadioPresetPicker::PAGE_SIZE]);
    EXPECT_EQ(RadioPresetPicker::APPLIED,activate(picker,1+i%RadioPresetPicker::PAGE_SIZE,p));EXPECT_FALSE(picker.menu.active);
    const auto& r=RADIO_PRESETS[i];EXPECT_FLOAT_EQ(r.freq,p.freq);EXPECT_FLOAT_EQ(r.bw,p.bw);EXPECT_EQ(r.sf,p.sf);EXPECT_EQ(r.cr,p.cr);
    EXPECT_EQ(r.path_hash_size?r.path_hash_size-1:2,p.getRadioPrefs()->getHashMode());
  }
}
TEST(RadioPresets, AllPagesAndSavedPresetCountsFitWithoutChangingSelectionOrStorage) {
  for(int count=0;count<=4;++count){ZenPrefs p;p.freq=901;p.bw=125;p.sf=9;p.cr=6;users(p,count);RadioPresetPicker picker;
    auto before=zen::BaselinePrefsFingerprint::calculate(p);uint64_t settings=zen::ZenPrefsCodec::fingerprint(p);
    picker.open(&p,target(p),"Radio Preset");
    for(int page=0;page<picker.pageCount();++page){goTo(picker,page,p);EXPECT_LE(picker.menu.count(),24);EXPECT_EQ(count,picker.user_count);
      EXPECT_EQ(page>0,row(picker,"< Previous presets")>=0);EXPECT_EQ(page+1<picker.pageCount(),row(picker,"Next presets >")>=0);
      EXPECT_EQ(count>0,row(picker,"- Delete preset...")>=0);
    }
    goTo(picker,0,p);EXPECT_EQ(PopupMenu::CANCELLED,picker.menu.handleInput(KEY_CANCEL));EXPECT_FALSE(picker.menu.active);
    EXPECT_EQ(before,zen::BaselinePrefsFingerprint::calculate(p));EXPECT_EQ(settings,zen::ZenPrefsCodec::fingerprint(p));
  }
}
TEST(RadioPresets, OpeningUsesCurrentPageAndNetworkRequirementsAffectMatching) {
  ZenPrefs p;int index=builtin("New Zealand (Narrow)");ASSERT_GE(index,0);const auto& r=RADIO_PRESETS[index];p.freq=r.freq;p.bw=r.bw;p.sf=r.sf;p.cr=r.cr;
  p.getRadioPrefs()->setHashMode(0);RadioPresetPicker picker;EXPECT_STREQ("Custom",picker.currentName(&p,target(p)));
  p.getRadioPrefs()->setHashMode(1);picker.open(&p,target(p),"Radio Preset");EXPECT_EQ(index/16,picker.page);EXPECT_EQ(1+index%16,picker.menu.selectedIndex());EXPECT_STREQ(r.name,picker.currentName(&p,target(p)));
  int specific=builtin("Netherlands (Limburg)");ASSERT_GE(specific,0);const auto& scoped=RADIO_PRESETS[specific];
  p.freq=scoped.freq;p.bw=scoped.bw;p.sf=scoped.sf;p.cr=scoped.cr;p.getRadioPrefs()->setHashMode(1);
  EXPECT_STREQ("Netherlands (Limburg)",picker.currentName(&p,target(p)));
  p.getRadioPrefs()->setHashMode(0);EXPECT_STREQ("EU/UK (Narrow)",picker.currentName(&p,target(p)));
  for(int i=0;i<RADIO_PRESET_COUNT;++i)if(RADIO_PRESETS[i].path_hash_size==0){const auto& plain=RADIO_PRESETS[i];p.freq=plain.freq;p.bw=plain.bw;p.sf=plain.sf;p.cr=plain.cr;p.getRadioPrefs()->setHashMode(2);
    EXPECT_TRUE(RadioPresetPicker::matchesBuiltin(&p,target(p),plain));
  }
}
TEST(RadioPresets, SaveSelectDeleteAndCancelReachCorrectUserSlotsFromEveryPage) {
  for(int page=0;page<2;++page){ZenPrefs p;p.freq=901;p.bw=125;p.sf=9;p.cr=6;RadioPresetPicker picker;picker.open(&p,target(p),"Radio Preset");goTo(picker,page,p);
    EXPECT_EQ(RadioPresetPicker::START_SAVE,activate(picker,0,p));EXPECT_TRUE(picker.saving);EXPECT_TRUE(picker.save(&p,"My saved",target(p)));
    users(p,4);picker.open(&p,target(p),"Radio Preset");goTo(picker,page,p);
    EXPECT_EQ(RadioPresetPicker::APPLIED,activate(picker,row(picker,"Saved 3"),p));EXPECT_FLOAT_EQ(904,p.freq);
    picker.open(&p,target(p),"Radio Preset");goTo(picker,page,p);EXPECT_EQ(RadioPresetPicker::NONE,activate(picker,row(picker,"- Delete preset..."),p));
    EXPECT_TRUE(picker.deleting);EXPECT_EQ(4,picker.menu.count());activate(picker,3,p);EXPECT_EQ(3,picker.confirm_slot);
    EXPECT_EQ(RadioPresetPicker::NONE,activate(picker,1,p));EXPECT_STREQ("Saved 3",p.user_radio_presets[3].name);
    picker.open(&p,target(p),"Radio Preset");goTo(picker,page,p);activate(picker,row(picker,"- Delete preset..."),p);activate(picker,3,p);
    EXPECT_EQ(RadioPresetPicker::DELETED,activate(picker,0,p));EXPECT_EQ(0,p.user_radio_presets[3].name[0]);EXPECT_STREQ("Saved 2",p.user_radio_presets[2].name);
  }
}
TEST(RadioPresets, SparseSavedSlotsAndRfOnlyFormatRemainCompatible) {
  ZenPrefs p;users(p,4);p.user_radio_presets[0].name[0]=p.user_radio_presets[2].name[0]=0;p.getRadioPrefs()->setHashMode(2);
  RadioPresetPicker picker;picker.open(&p,target(p),"Radio Preset");ASSERT_EQ(2,picker.user_count);EXPECT_EQ(1,picker.user_slot[0]);EXPECT_EQ(3,picker.user_slot[1]);
  EXPECT_EQ(RadioPresetPicker::APPLIED,activate(picker,row(picker,"Saved 3"),p));EXPECT_FLOAT_EQ(904,p.freq);EXPECT_EQ(2,p.getRadioPrefs()->getHashMode());
  picker.open(&p,target(p),"Radio Preset");activate(picker,row(picker,"- Delete preset..."),p);activate(picker,0,p);activate(picker,0,p);
  EXPECT_EQ(0,p.user_radio_presets[1].name[0]);EXPECT_STREQ("Saved 3",p.user_radio_presets[3].name);
}
TEST(RadioPresets, OldAustraliaCr8AndCustomPresetsAreNotMigratedByCatalogueUpdate) {
  ZenPrefs p;p.freq=916.575;p.bw=62.5;p.sf=7;p.cr=8;p.getRadioPrefs()->setHashMode(1);RadioPresetPicker picker;
  picker.open(&p,target(p),"Radio Preset");EXPECT_STREQ("Custom",picker.currentName(&p,target(p)));EXPECT_EQ(8,p.cr);
  ASSERT_TRUE(picker.save(&p,"Old Australia",target(p)));auto old=p.user_radio_presets[0];
  picker.open(&p,target(p),"Radio Preset");activate(picker,row(picker,"Australia (Narrow)"),p);EXPECT_EQ(7,p.cr);EXPECT_EQ(1,p.getRadioPrefs()->getHashMode());EXPECT_EQ(8,p.user_radio_presets[0].cr);
  uint8_t bytes[zen::ZenPrefsCodec::MAX_ENCODED_SIZE];size_t n=zen::ZenPrefsCodec::encode(p,1,bytes,sizeof(bytes));ASSERT_GT(n,0u);
  ZenPrefs reboot;reboot.freq=old.freq;reboot.bw=old.bw;reboot.sf=old.sf;reboot.cr=old.cr;ASSERT_TRUE(zen::ZenPrefsCodec::decode(reboot,bytes,n));
  EXPECT_EQ(8,reboot.cr);EXPECT_EQ(8,reboot.user_radio_presets[0].cr);EXPECT_STREQ("Old Australia",picker.currentName(&reboot,target(reboot)));
}
class TraceTestTransport:public zen::RepeaterTraceTransport {
public:
  int submissions=0;
  uint32_t traceNow()const override{return 0;}uint32_t traceRandom()override{return 0;}uint32_t traceDiscoveryTag()override{return 1;}
  uint32_t traceAirtime(unsigned)const override{return 1;}
  mesh::Packet* traceQueueDiscovery(const uint8_t*,uint32_t)override{++submissions;return nullptr;}
  mesh::Packet* traceQueueLoop(const zen::TraceRoute&,uint32_t,uint32_t)override{++submissions;return nullptr;}
  bool traceCancelQueued(mesh::Packet*)override{return false;}
};
TEST(RadioPresets, ThreeByteOfficialPresetIsAppliedWithoutCoercionAndTraceLimitIsExplicit) {
  ZenPrefs p;RadioPresetPicker picker;picker.open(&p,target(p),"Radio Preset");int index=builtin("Canada");ASSERT_GE(index,0);goTo(picker,index/16,p);
  ASSERT_EQ(RadioPresetPicker::APPLIED,activate(picker,1+index%16,p));ASSERT_EQ(2,p.getRadioPrefs()->getHashMode());
  TraceTestTransport tx;zen::RepeaterTraceSession session(tx);uint8_t key[32]={1};
  EXPECT_FALSE(session.start(key,true,p.getRadioPrefs()->getHashMode()+1));EXPECT_EQ(zen::RepeaterTraceSession::UNSUPPORTED_PATH,session.error());EXPECT_EQ(0,tx.submissions);EXPECT_EQ(2,p.getRadioPrefs()->getHashMode());
}
TEST(RadioPresets, PickerOperationsDoNotAllocateHeapMemory) {
  ZenPrefs p;users(p,4);RadioPresetPicker picker;allocation_count=0;track_allocations=true;
  picker.open(&p,target(p),"Radio Preset");picker.onSelected(picker.menu.count()-1,&p,target(p));
  picker.currentName(&p,target(p));picker.onSelected(1,&p,target(p));
  track_allocations=false;EXPECT_EQ(0u,allocation_count);
}
TEST(RadioPresets, AllPopupRowsFitBothDisplayGeometriesAndPortrait) {
  for(auto geometry:std::vector<std::vector<int>>{{128,64,6,8},{250,122,8,12},{122,250,8,12}}){RecordingDisplay display(geometry[0],geometry[1],geometry[2],geometry[3]);ZenPrefs p;users(p,4);RadioPresetPicker picker;picker.open(&p,target(p),"Radio Preset");
    for(int page=0;page<picker.pageCount();++page){goTo(picker,page,p);for(int index=0;index<picker.menu.count();++index){picker.menu.setSelected(index);display.startFrame();picker.menu.render(display);EXPECT_TRUE(display.inBounds())<<page<<":"<<index;}}
  }
}
int main(int argc,char** argv){::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS();}
