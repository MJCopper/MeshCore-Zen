#define FIRMWARE_ZEN_BUILD 1
#define ZEN_FEATURE_ADMIN 1
#define MAX_TEXT_LEN 160
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <new>
#include <cstdlib>
#include <helpers/ContactInfo.h>
#include <helpers/AdvertDataHelpers.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/PathDetails.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageModels.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/NodeListAccess.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/DisplayActivity.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/RepeaterTraceSession.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/OperationResult.h"
#include "../../examples/companion_radio/zen-overlay/app/ZenPrefs.h"
#include "../../examples/companion_radio/zen-overlay/app/ui-new/PopupMenu.h"

mesh::Identity::Identity() { memset(pub_key,0,sizeof(pub_key)); }

static bool track_allocations=false;
static unsigned allocations=0;
void* operator new(std::size_t n) { if(track_allocations)++allocations;void* p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p; }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

#define MAX_ANON_CONTACTS 8
using Model=zen::NodeListModel;
struct Advert { uint8_t pubkey_prefix[7]{};uint8_t path_len=0;char name[32]{};uint32_t recv_timestamp=0;uint8_t path[64]{}; };
struct Result { char name[32]{};uint8_t type=2;bool is_known=false;int8_t rssi=-70,snr_x4=25,remote_snr_x4=9;uint8_t pub_key[32]{};uint32_t timestamp=0; };

struct NullTransport:zen::RepeaterTraceTransport {
  uint32_t traceNow() const override { return 0; }
  uint32_t traceRandom() override { return 1; }
  uint32_t traceDiscoveryTag() override { return 1; }
  uint32_t traceAirtime(unsigned) const override { return 1; }
  mesh::Packet* traceQueueDiscovery(const uint8_t*,uint32_t) override { return nullptr; }
  mesh::Packet* traceQueueLoop(const zen::TraceRoute&,uint32_t,uint32_t) override { return nullptr; }
  bool traceCancelQueued(mesh::Packet*) override { return true; }
} null_transport;
zen::RepeaterTraceSession trace_session(null_transport);
// Fake only the storage/radio boundary. Production NodeListAccess, model and
// NearbyScreen are used unchanged below.
struct FakeMesh {
  std::vector<ContactInfo> contacts;
  std::vector<Result> discovery;
  Advert adverts[16]{};
  uint32_t generation=1;
  uint8_t action_key[32]{};
  unsigned deleted=0,added=0,starred=0;
  int getNumContacts() const { return contacts.size(); }
  int getTotalContactSlots() const { return contacts.size()+MAX_ANON_CONTACTS; }
  bool getContactByIdx(unsigned slot,ContactInfo& out) {
    if(slot<MAX_ANON_CONTACTS || slot>=unsigned(getTotalContactSlots()))return false;
    out=contacts[slot-MAX_ANON_CONTACTS];return true;
  }
  ContactInfo* lookupContactByPubKey(const uint8_t* key,int size) {
    for(auto& c:contacts)if(!memcmp(c.id.pub_key,key,size))return &c;
    return nullptr;
  }
  unsigned nodeListCount(Model::Kind kind) const {
    return kind==Model::CONTACT?contacts.size():kind==Model::DISCOVERY?discovery.size():16;
  }
  uint32_t nodeListDiscoveryGeneration() const { return generation; }
  uint32_t generationValue() const { return generation; }
  bool readNodeListRecord(Model::Kind kind,unsigned index,Model::Record& out) {
    if(kind==Model::CONTACT)return zen::NodeListAccess::contact<FakeMesh,ContactInfo>(*this,index,8,out);
    if(kind==Model::DISCOVERY)return zen::NodeListAccess::discovery(*this,discovery.data(),discovery.size(),index,out);
    return zen::NodeListAccess::advert(adverts,16,index,out);
  }
  bool sendNodeDiscoverReq() { ++generation;discovery.clear();return true; }
  bool saveZenPrefs() { return true; }
  bool addDiscoveredContact(const uint8_t* key,const char* name,uint8_t type) {
    ++added;memcpy(action_key,key,32);
    if(!lookupContactByPubKey(key,32)) { ContactInfo c{};memcpy(c.id.pub_key,key,32);strncpy(c.name,name,31);c.type=type;contacts.push_back(c); }
    return true;
  }
  bool deleteContactByKey(const uint8_t* key) {
    ++deleted;memcpy(action_key,key,32);
    for(auto i=contacts.begin();i!=contacts.end();++i)if(!memcmp(i->id.pub_key,key,32)) { contacts.erase(i);return true; }
    return false;
  }
  bool setContactFavourite(const uint8_t* key,bool on) {
    auto c=lookupContactByPubKey(key,32);if(!c)return false;
    ++starred;memcpy(action_key,key,32);c->flags=on?c->flags|1:c->flags&~1;return true;
  }
  zen::RepeaterTraceSession& repeaterTrace() { return trace_session; }
  const char* repeaterTraceName() { return "Repeater"; }
  void traceHopName(const uint8_t*,unsigned,char* out,size_t n) { snprintf(out,n,"Hop"); }
} the_mesh;
struct Clock { uint32_t getCurrentTime() { return 1000; } } rtc_clock;

class UITask {
public:
  ZenPrefs prefs{};
  uint8_t action_key[32]{};
  unsigned ping=0,traces=0,admin=0,tools=0;
  bool locked=false;
  bool currentLocation(int32_t& lat,int32_t& lon) { lat=-33000000;lon=151000000;return true; }
  bool useImperial() const { return false; }
  bool isChildModeLocked() const { return locked; }
  int findFavouriteSlot(const uint8_t* key) { for(int i=0;i<ZenPrefs::FAVOURITES_DIAL_COUNT;++i)if(!memcmp(prefs.favourite_contacts[i],key,ZenPrefs::FAVOURITE_PREFIX_LEN))return i;return -1; }
  bool isFavouriteSlotEmpty(int slot) { uint8_t zero[ZenPrefs::FAVOURITE_PREFIX_LEN]{};return !memcmp(prefs.favourite_contacts[slot],zero,sizeof(zero)); }
  void setFavouriteSlot(int slot,const uint8_t* key) { memcpy(prefs.favourite_contacts[slot],key,ZenPrefs::FAVOURITE_PREFIX_LEN);memcpy(action_key,key,32); }
  void clearFavouriteSlot(int slot) { memset(prefs.favourite_contacts[slot],0,ZenPrefs::FAVOURITE_PREFIX_LEN); }
  ZenPrefs* getNodePrefs() { return &prefs; }
  template<class... T> void operationError(T...) {}
  void showAlert(const char*,int) {}
  void gotoToolsScreen() { ++tools; }
  void clearPing() {}
  bool startPing(const uint8_t* key) { ++ping;memcpy(action_key,key,32);return true; }
  bool isPingActive() { return false; }
  void getPingResult(int16_t&,int16_t&,uint32_t&) {}
  bool startRepeaterTrace(const uint8_t* key,const char*) { ++traces;memcpy(action_key,key,32);return true; }
  void cancelRepeaterTrace() {}
  zen::PathAttemptSnapshot latestPathAttempt(const uint8_t*) { return {}; }
  void openAdminFor(const ContactInfo& c) { ++admin;memcpy(action_key,c.id.pub_key,32); }
};
#include "../../examples/companion_radio/zen-overlay/app/ui-new/NearbyScreen.h"

struct Provider {
  unsigned count(Model::Kind k) { return the_mesh.nodeListCount(k); }
  uint32_t generation() { return the_mesh.generation; }
  bool read(Model::Kind k,unsigned i,Model::Record& out) { return the_mesh.readNodeListRecord(k,i,out); }
};
static ContactInfo contact(unsigned id,uint8_t type=2,const char* name=nullptr) {
  ContactInfo c{};c.id.pub_key[0]=id;c.id.pub_key[31]=id>>8;
  if(name)strncpy(c.name,name,31);else snprintf(c.name,32,"Node %u",id);
  c.type=type;c.lastmod=id;c.out_path_len=OUT_PATH_UNKNOWN;return c;
}
static Result result(unsigned id,int rssi=-70) { Result r{};r.pub_key[0]=id;r.pub_key[31]=id>>8;r.rssi=rssi;return r; }
static Advert advert(unsigned id,uint32_t time,const char* name) { Advert a{};a.pubkey_prefix[0]=id;a.recv_timestamp=time;strncpy(a.name,name,31);return a; }
class NodeList:public ::testing::Test {
protected:
  void SetUp() override { the_mesh=FakeMesh{};g_mock_millis=0; }
};

TEST_F(NodeList, RealAdapterSkipsReservedSlotsAndIncludesLastContact) {
  Provider p;Model model;
  EXPECT_EQ(model.rebuild(p,false,Model::ALL,false,1000),-1);EXPECT_EQ(model.count(),0u);
  for(unsigned i=1;i<=350;++i)the_mesh.contacts.push_back(contact(i));
  Model::Record r;
  ASSERT_TRUE(p.read(Model::CONTACT,0,r));EXPECT_EQ(r.pub_key[0],1);EXPECT_EQ(r.contact_idx,8);
  ASSERT_TRUE(p.read(Model::CONTACT,349,r));EXPECT_EQ(r.pub_key[31],1);EXPECT_EQ(r.pub_key[0],94);EXPECT_EQ(r.contact_idx,357);
  EXPECT_FALSE(p.read(Model::CONTACT,350,r));
  the_mesh.contacts.resize(1);model.rebuild(p,false,Model::ALL,false,1000);EXPECT_EQ(model.count(),1u);
}
TEST_F(NodeList, CapacityAndExistingAdmissionPolicy) {
  Provider p;Model m;
  for(unsigned n:{31u,32u,33u,350u}) {
    the_mesh.contacts.clear();for(unsigned i=1;i<=n;++i)the_mesh.contacts.push_back(contact(i));
    the_mesh.contacts.back().flags=1;m.rebuild(p,false,Model::ALL,true,1000);
    EXPECT_EQ(m.count(),std::min(n,32u));Model::Record r;
    ASSERT_TRUE(m.read(p,0,r));EXPECT_EQ(r.favourite,n<=32);
    m.rebuild(p,false,Model::FAV,true,1000);ASSERT_EQ(m.count(),1u);ASSERT_TRUE(m.read(p,0,r));EXPECT_EQ(r.pub_key[0],n%256);
  }
}
TEST_F(NodeList, TypeFiltersFavouritesTimeAndInvalidTimestamps) {
  Provider p;Model m;
  for(unsigned i=1;i<=4;++i)the_mesh.contacts.push_back(contact(i,i));
  for(auto pair:{std::make_pair(Model::RPT,2),{Model::ROOM,3},{Model::SNSR,4}}) {
    m.rebuild(p,false,pair.first,false,1000);ASSERT_EQ(m.count(),1u);Model::Record r;ASSERT_TRUE(m.read(p,0,r));EXPECT_EQ(r.type,pair.second);
  }
  the_mesh.contacts[0].flags=1;the_mesh.contacts[1].lastmod=0;the_mesh.contacts[2].lastmod=1001;
  m.rebuild(p,false,Model::ALL,true,1000);Model::Record r;
  ASSERT_TRUE(m.read(p,0,r));EXPECT_EQ(r.pub_key[0],4);
  ASSERT_TRUE(m.read(p,1,r));EXPECT_FALSE(r.favourite);
}
TEST_F(NodeList, AdvertsAreRecentSummariesWithoutReorderingOrPathCopies) {
  the_mesh.adverts[0]=advert(1,20,"Older");the_mesh.adverts[1]=advert(2,40,"Newest");the_mesh.adverts[2]=advert(3,40,"Tie");
  memset(the_mesh.adverts[1].path,0xAA,64);
  Advert before[16];memcpy(before,the_mesh.adverts,sizeof(before));
  Provider p;Model::Record r;
  ASSERT_TRUE(p.read(Model::ADVERT,0,r));EXPECT_STREQ(r.name,"Newest");EXPECT_EQ(r.lastmod,40u);EXPECT_FALSE(r.has_key);
  ASSERT_TRUE(p.read(Model::ADVERT,1,r));EXPECT_STREQ(r.name,"Tie");EXPECT_EQ(memcmp(before,the_mesh.adverts,sizeof(before)),0);
  EXPECT_FALSE(p.read(Model::ADVERT,16,r));
  memset(the_mesh.adverts[1].name,'a',32);ASSERT_TRUE(p.read(Model::ADVERT,0,r));EXPECT_EQ(strlen(r.name),31u);
}
TEST_F(NodeList, UntypedAdvertsAreExcludedWithoutChangingSharedCache) {
  the_mesh.contacts.push_back(contact(1,2,"Known"));the_mesh.adverts[0]=advert(1,100,"Different name");the_mesh.adverts[1]=advert(2,99,"Known");
  for(unsigned i=2;i<16;++i)the_mesh.adverts[i]=advert(i+2,98-i,("Other "+std::to_string(i)).c_str());
  Advert before[16];memcpy(before,the_mesh.adverts,sizeof(before));
  Provider p;Model m;m.rebuild(p,false,Model::ALL,true,1000);EXPECT_EQ(m.count(),1u);
  EXPECT_EQ(memcmp(before,the_mesh.adverts,sizeof(before)),0);
  m.rebuild(p,false,Model::RPT,true,1000);EXPECT_EQ(m.count(),1u);
  for(unsigned i=2;i<=32;++i)the_mesh.contacts.push_back(contact(i));m.rebuild(p,false,Model::ALL,true,1000);EXPECT_EQ(m.count(),32u);
}
TEST_F(NodeList, DiscoverySignalsKnownStateAndGeneration) {
  the_mesh.discovery={result(1,-90),result(2,-30),result(3,-30)};
  Provider p;Model m;m.rebuild(p,true,Model::RPT,false,1000);EXPECT_EQ(m.count(),3u);
  ASSERT_TRUE(m.select(p,0));ASSERT_NE(m.selected(),nullptr);EXPECT_EQ(m.selected()->pub_key[0],2);EXPECT_EQ(m.selected()->snr_x4,25);EXPECT_EQ(m.selected()->remote_snr_x4,9);
  EXPECT_FALSE(m.selected()->is_known);the_mesh.contacts.push_back(contact(2));ASSERT_TRUE(m.validate(p));EXPECT_TRUE(m.selected()->is_known);
  the_mesh.contacts.clear();ASSERT_TRUE(m.validate(p));EXPECT_FALSE(m.selected()->is_known);
  ++the_mesh.generation;EXPECT_FALSE(m.validate(p));EXPECT_EQ(m.selected(),nullptr);
}
TEST_F(NodeList, SelectedFullIdentitySurvivesReorderButRejectsReplacementWithSamePrefix) {
  the_mesh.contacts={contact(1),contact(2)};Provider p;Model m;m.rebuild(p,false,Model::ALL,false,1000);ASSERT_TRUE(m.select(p,0));
  std::swap(the_mesh.contacts[0],the_mesh.contacts[1]);ASSERT_TRUE(m.validate(p));EXPECT_EQ(m.selected()->pub_key[0],1);EXPECT_EQ(m.selected()->contact_idx,9);
  EXPECT_EQ(m.rebuild(p,false,Model::ALL,false,1000),1);
  the_mesh.contacts[1].id.pub_key[31]=17;EXPECT_FALSE(m.validate(p));
}
TEST_F(NodeList, AdvertOnlyRecordsCannotBecomeSelectableInfrastructure) {
  the_mesh.adverts[0]=advert(1,100,"Repeater sounding name");Provider p;Model m;
  m.rebuild(p,false,Model::ALL,false,1000);EXPECT_EQ(m.count(),0u);EXPECT_FALSE(m.select(p,0));
  the_mesh.adverts[0]=advert(2,100,"Replacement");
  m.rebuild(p,false,Model::ALL,false,1000);EXPECT_EQ(m.count(),0u);
}
TEST_F(NodeList, FixedMemoryAndNoModelOrAdapterAllocations) {
  the_mesh.contacts={contact(1),contact(2)};Provider p;Model m;Model::Record r;
  allocations=0;track_allocations=true;
  m.rebuild(p,false,Model::ALL,true,1000);m.select(p,0);m.validate(p);m.read(p,1,r);
  track_allocations=false;EXPECT_EQ(allocations,0u);EXPECT_LE(sizeof(Model),640u);
}

class RecordingDisplay:public ZenDisplayDriver {
public:
  struct Rect { int x,y,w,h; };
  std::vector<Rect> draws;
  std::vector<std::string> text;
  int x=0,y=0,cw=6,lh=9;
  RecordingDisplay(int w,int h,int char_width=6,int line_height=9):ZenDisplayDriver(w,h),cw(char_width),lh(line_height) {}
  bool isSingleFont() const override { return true; }
  bool isOn() override { return true; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override {}
  void startFrame(ColorVal=0) override { draws.clear();text.clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int xx,int yy) override { x=xx;y=yy; }
  void print(const char* value) override { draws.push_back({x,y,int(strlen(value))*cw,lh});text.emplace_back(value); }
  void fillRect(int xx,int yy,int w,int h) override { draws.push_back({xx,yy,w,h}); }
  void drawRect(int xx,int yy,int w,int h) override { draws.push_back({xx,yy,w,h}); }
  void drawXbm(int xx,int yy,const uint8_t*,int w,int h) override { draws.push_back({xx,yy,w,h}); }
  uint16_t getTextWidth(const char* value) override { return strlen(value)*cw; }
  int getCharWidth() const override { return cw; }
  int getLineHeight() const override { return lh; }
  void endFrame() override {}
  bool contains(const char* value) { for(const auto& t:text)if(t.find(value)!=std::string::npos)return true;return false; }
  bool inBounds() { for(auto r:draws)if(r.x<0||r.y<0||r.w<0||r.h<0||r.x+r.w>width()||r.y+r.h>height()) { fprintf(stderr,"Out of bounds %dx%d: %d %d %d %d\n",width(),height(),r.x,r.y,r.w,r.h);return false; }return true; }
};
static void draw(NearbyScreen& screen,RecordingDisplay& display) { display.startFrame();screen.render(display); }
static void action(NearbyScreen& screen,int row) {
  screen.handleInput(KEY_CONTEXT_MENU);for(int i=0;i<row;++i)screen.handleInput(KEY_DOWN);screen.handleInput(KEY_ENTER);
}
TEST_F(NodeList, ScreenRendersListDetailsMenusEmptyAndScrollingOnBothTargets) {
  for(auto size:{std::make_pair(128,64),{250,122}}) {
    the_mesh=FakeMesh{};UITask task;NearbyScreen screen(&task);RecordingDisplay display(size.first,size.second,size.first==250?12:6,size.first==250?18:9);
    screen.onShow();draw(screen,display);EXPECT_TRUE(display.contains("No contacts"));EXPECT_TRUE(display.inBounds());
    for(unsigned i=1;i<=35;++i)the_mesh.contacts.push_back(contact(i,2,"Repeater with a very long name"));
    screen.onShow();draw(screen,display);EXPECT_TRUE(display.inBounds());
    for(unsigned i=0;i<34;++i) { screen.handleInput(KEY_DOWN);draw(screen,display);EXPECT_TRUE(display.inBounds()); }
    screen.handleInput(KEY_ENTER);draw(screen,display);EXPECT_TRUE(display.contains("Type: Repeater"));EXPECT_TRUE(display.inBounds());
    screen.handleInput(KEY_CONTEXT_MENU);draw(screen,display);EXPECT_TRUE(display.contains("Options"));EXPECT_TRUE(display.inBounds());
    screen.handleInput(KEY_CANCEL);screen.handleInput(KEY_CANCEL);
    screen.startDiscoverScan();draw(screen,display);EXPECT_TRUE(display.contains("Waiting"));EXPECT_TRUE(display.inBounds());
    the_mesh.discovery={result(77)};draw(screen,display);EXPECT_TRUE(display.inBounds());
    screen.handleInput(KEY_ENTER);draw(screen,display);EXPECT_TRUE(display.contains("RSSI:"));EXPECT_TRUE(display.contains("SNR:"));EXPECT_TRUE(display.inBounds());
  }
}
TEST_F(NodeList, ScreenDistanceSortingFiltersAndSelectionRefresh) {
  auto near=contact(1,4,"Near"),far=contact(2,2,"Far"),unknown=contact(3,3,"Unknown");
  near.gps_lat=-33001000;near.gps_lon=151000000;far.gps_lat=-34000000;far.gps_lon=151000000;
  the_mesh.contacts={far,unknown,near};UITask task;NearbyScreen screen(&task);screen.onShow();
  action(screen,0);EXPECT_EQ(task.action_key[0],1);screen.handleInput(KEY_CANCEL);
  screen.handleInput(KEY_RIGHT); // Fav
  RecordingDisplay display(128,64);draw(screen,display);EXPECT_TRUE(display.contains("No Fav"));
  screen.handleInput(KEY_RIGHT); // Rpt
  action(screen,0);EXPECT_EQ(task.action_key[0],2);screen.handleInput(KEY_CANCEL);
  screen.handleInput(KEY_RIGHT); // Room
  action(screen,0);EXPECT_EQ(task.action_key[0],3);screen.handleInput(KEY_CANCEL);
  screen.handleInput(KEY_RIGHT); // Snsr
  action(screen,0);EXPECT_EQ(task.action_key[0],1);
}
TEST_F(NodeList, MenuPingTraceAdminFavouriteAndPathUseFullIdentity) {
  for(unsigned which:{0u,1u,2u,3u,4u}) {
    the_mesh.contacts={contact(1),contact(2)};UITask task;NearbyScreen screen(&task);screen.onShow();
    screen.handleInput(KEY_CONTEXT_MENU);
    std::swap(the_mesh.contacts[0],the_mesh.contacts[1]);
    for(unsigned i=0;i<which;++i)screen.handleInput(KEY_DOWN);screen.handleInput(KEY_ENTER);
    if(which==0) { EXPECT_EQ(task.ping,1u);EXPECT_EQ(task.action_key[0],1); }
    if(which==1) { EXPECT_EQ(task.traces,1u);EXPECT_EQ(task.action_key[0],1); }
    if(which==2) { RecordingDisplay d(128,64);draw(screen,d);EXPECT_TRUE(d.contains("Path Details"));EXPECT_TRUE(d.contains("Node 1")); }
    if(which==3) { EXPECT_EQ(the_mesh.starred,1u);EXPECT_EQ(the_mesh.action_key[0],1); }
    if(which==4) { EXPECT_EQ(task.admin,1u);EXPECT_EQ(task.action_key[0],1); }
  }
}
TEST_F(NodeList, DeleteConfirmationSurvivesReorderAndDefaultsToCancel) {
  the_mesh.contacts={contact(1),contact(2)};UITask task;NearbyScreen screen(&task);screen.onShow();
  action(screen,5);std::swap(the_mesh.contacts[0],the_mesh.contacts[1]);screen.handleInput(KEY_ENTER);EXPECT_EQ(the_mesh.deleted,0u);
  action(screen,5);screen.handleInput(KEY_UP);screen.handleInput(KEY_ENTER);
  EXPECT_EQ(the_mesh.deleted,1u);EXPECT_EQ(the_mesh.action_key[0],1);ASSERT_EQ(the_mesh.contacts.size(),1u);EXPECT_EQ(the_mesh.contacts[0].id.pub_key[0],2);
}
TEST_F(NodeList, ReplacementWhileConfirmingDoesNotDeleteNewNodeWithSamePrefix) {
  the_mesh.contacts={contact(1),contact(2)};UITask task;NearbyScreen screen(&task);screen.onShow();action(screen,5);
  the_mesh.contacts[0].id.pub_key[31]=42;screen.handleInput(KEY_UP);screen.handleInput(KEY_ENTER);
  EXPECT_EQ(the_mesh.deleted,0u);EXPECT_EQ(the_mesh.contacts.size(),2u);
}
TEST_F(NodeList, RemovalWhileMenuOrDetailIsOpenClosesWithoutRetargetingAction) {
  for(bool detail:{false,true}) {
    the_mesh.contacts={contact(1),contact(2)};UITask task;NearbyScreen screen(&task);screen.onShow();
    if(detail)screen.handleInput(KEY_ENTER);screen.handleInput(KEY_CONTEXT_MENU);
    the_mesh.contacts.erase(the_mesh.contacts.begin());screen.handleInput(KEY_ENTER);
    EXPECT_EQ(task.ping,0u);RecordingDisplay display(128,64);draw(screen,display);EXPECT_FALSE(display.contains("Options"));
  }
}
TEST_F(NodeList, DiscoveryAddAndRescanInvalidationUseActualStorageAdapter) {
  UITask task;NearbyScreen screen(&task);screen.onShow();screen.startDiscoverScan();
  the_mesh.discovery={result(7)};RecordingDisplay d(128,64);draw(screen,d);
  action(screen,2);EXPECT_EQ(the_mesh.added,1u);EXPECT_EQ(the_mesh.action_key[0],7);ASSERT_EQ(the_mesh.contacts.size(),1u);
  screen.onShow();screen.startDiscoverScan();the_mesh.discovery={result(8)};draw(screen,d);
  screen.handleInput(KEY_CONTEXT_MENU);++the_mesh.generation;the_mesh.discovery={result(9)};screen.handleInput(KEY_ENTER);EXPECT_EQ(task.ping,0u);
}
TEST_F(NodeList, DiscoveryKeepsSelectedNodeWhenStrongerResultsArrive) {
  UITask task;NearbyScreen screen(&task);RecordingDisplay d(128,64);screen.onShow();screen.startDiscoverScan();the_mesh.discovery={result(1,-80)};draw(screen,d);
  screen.handleInput(KEY_CONTEXT_MENU);the_mesh.discovery.push_back(result(2,-20));draw(screen,d);screen.handleInput(KEY_ENTER);
  EXPECT_EQ(task.ping,1u);EXPECT_EQ(task.action_key[0],1);
}
TEST_F(NodeList, LastStoredContactPathDetailsIsAvailable) {
  for(unsigned i=1;i<=12;++i)the_mesh.contacts.push_back(contact(i));
  UITask task;NearbyScreen screen(&task);screen.onShow();for(int i=0;i<11;++i)screen.handleInput(KEY_DOWN);
  action(screen,2);RecordingDisplay d(128,64);draw(screen,d);EXPECT_TRUE(d.contains("Node 12"));EXPECT_FALSE(d.contains("unavailable"));
}
TEST_F(NodeList, ScreenRefreshAndNavigationDoNotAllocate) {
  the_mesh.contacts={contact(1),contact(2)};UITask task;NearbyScreen screen(&task);
  allocations=0;track_allocations=true;screen.onShow();screen.handleInput(KEY_DOWN);screen.handleInput(KEY_ENTER);screen.handleInput(KEY_CANCEL);screen.handleInput(KEY_RIGHT);screen.handleInput(KEY_LEFT);track_allocations=false;
  EXPECT_EQ(allocations,0u);
}

TEST_F(NodeList, OnlyIdentifiableInfrastructureIsEligibleInStoredAndScanViews) {
  Provider p;Model m;Model::Record value;
  for(uint8_t type:{uint8_t(0),uint8_t(1),uint8_t(2),uint8_t(3),uint8_t(4),uint8_t(5),uint8_t(255)}) {
    the_mesh.contacts={contact(7,type)};
    the_mesh.discovery={result(7)};the_mesh.discovery[0].type=type;
    bool expected=type>=2 && type<=4;
    for(bool scan:{false,true}) {
      m.clearSelection();m.rebuild(p,scan,Model::ALL,false,1000);
      EXPECT_EQ(m.count(),expected?1u:0u)<<int(type)<<" scan="<<scan;
    }
  }
  value.type=2;value.has_key=false;EXPECT_FALSE(Model::eligible(value));
  value.has_key=true;EXPECT_TRUE(Model::eligible(value));
  EXPECT_EQ(Model::FILTER_COUNT,5);
  for(unsigned i=0;i<Model::FILTER_COUNT;++i)EXPECT_STRNE(Model::filterLabels()[i],"Comp");
}
TEST_F(NodeList, CompanionsCannotConsumeCapacityOrAppearAsFavourites) {
  for(unsigned i=1;i<=100;++i) { auto c=contact(i,1);c.flags=1;the_mesh.contacts.push_back(c); }
  for(unsigned i=101;i<=135;++i)the_mesh.contacts.push_back(contact(i,uint8_t(2+i%3)));
  the_mesh.contacts.back().flags=1;
  Provider p;Model m;Model::Record value;
  m.rebuild(p,false,Model::ALL,false,1000);ASSERT_EQ(m.count(),32u);
  for(unsigned i=0;i<m.count();++i) { ASSERT_TRUE(m.read(p,i,value));EXPECT_GE(value.pub_key[0],101);EXPECT_LE(value.pub_key[0],132);EXPECT_GE(value.type,2); }
  m.rebuild(p,false,Model::FAV,false,1000);ASSERT_EQ(m.count(),1u);ASSERT_TRUE(m.read(p,0,value));EXPECT_EQ(value.pub_key[0],135);
}
TEST_F(NodeList, TypeChangeInvalidatesSelectedIdentityAndRowWithoutKeyChange) {
  Provider p;Model m;Model::Record value;
  for(uint8_t invalid:{uint8_t(1),uint8_t(0),uint8_t(255)}) {
    the_mesh.contacts={contact(2)};m.clearSelection();m.rebuild(p,false,Model::ALL,false,1000);ASSERT_TRUE(m.select(p,0));
    the_mesh.contacts[0].type=invalid;EXPECT_FALSE(m.read(p,0,value));EXPECT_FALSE(m.validate(p));
    m.rebuild(p,false,Model::ALL,false,1000);EXPECT_EQ(m.count(),0u);
    the_mesh.discovery={result(3)};m.rebuild(p,true,Model::ALL,false,1000);ASSERT_TRUE(m.select(p,0));
    the_mesh.discovery[0].type=invalid;EXPECT_FALSE(m.validate(p));
  }
}
TEST_F(NodeList, BrowsingDoesNotChangeCompanionContactsFlagsOrSpeedDialPins) {
  auto companion=contact(1,1,"Companion");companion.flags=0x1f;
  the_mesh.contacts={companion,contact(2),contact(3,3),contact(4,4)};
  UITask task;task.setFavouriteSlot(2,companion.id.pub_key);
  std::vector<ContactInfo> before=the_mesh.contacts;uint8_t prefs_before[sizeof(task.prefs)];memcpy(prefs_before,&task.prefs,sizeof(task.prefs));
  NearbyScreen screen(&task);RecordingDisplay d(128,64);screen.onShow();
  for(int i=0;i<10;++i) { draw(screen,d);screen.handleInput(KEY_RIGHT);screen.handleInput(KEY_DOWN); }
  ASSERT_EQ(the_mesh.contacts.size(),before.size());EXPECT_EQ(memcmp(the_mesh.contacts.data(),before.data(),before.size()*sizeof(ContactInfo)),0);
  EXPECT_EQ(memcmp(prefs_before,&task.prefs,sizeof(task.prefs)),0);
  EXPECT_EQ(the_mesh.deleted,0u);EXPECT_EQ(the_mesh.added,0u);EXPECT_EQ(the_mesh.starred,0u);
}
TEST_F(NodeList, FiveFiltersCycleAndWrapWithCorrectTargets) {
  auto sensor=contact(4,4);sensor.flags=1;
  the_mesh.contacts={contact(1,1),contact(2),contact(3,3),sensor};
  UITask task;NearbyScreen screen(&task);screen.onShow();
  for(unsigned expected:{4u,4u,2u,3u,4u,4u}) {
    action(screen,0);EXPECT_EQ(task.action_key[0],expected);screen.handleInput(KEY_CANCEL);screen.handleInput(KEY_RIGHT);
  }
  // Now on Fav; two left movements wrap All -> Snsr.
  screen.handleInput(KEY_LEFT);screen.handleInput(KEY_LEFT);action(screen,0);EXPECT_EQ(task.action_key[0],4);
}
TEST_F(NodeList, DiscoveryReturnRestoresFilterAndAdminReturnPreservesInfrastructureSelection) {
  the_mesh.contacts={contact(1,1),contact(2),contact(3,3)};
  UITask task;NearbyScreen screen(&task);screen.onShow();screen.handleInput(KEY_RIGHT);screen.handleInput(KEY_RIGHT);screen.handleInput(KEY_RIGHT); // Room
  screen.startDiscoverScan();screen.handleInput(KEY_CANCEL);EXPECT_EQ(task.tools,1u);
  action(screen,0);EXPECT_EQ(task.action_key[0],3);screen.handleInput(KEY_CANCEL);
  action(screen,3);EXPECT_EQ(task.admin,1u);EXPECT_EQ(task.action_key[0],3);
  screen.onHide();screen.resumeFromAdmin();screen.onShow();action(screen,0);EXPECT_EQ(task.action_key[0],3);
}
TEST_F(NodeList, IneligibleTypeUpdateClosesEveryOpenActionViewBeforeInput) {
  for(int view:{0,1,2})for(uint8_t type:{uint8_t(1),uint8_t(0),uint8_t(255),uint8_t(3)}) {
    the_mesh=FakeMesh{};the_mesh.contacts={contact(1),contact(2)};UITask task;NearbyScreen screen(&task);screen.onShow();
    if(view==0)screen.handleInput(KEY_ENTER);
    else if(view==1)screen.handleInput(KEY_CONTEXT_MENU);
    else { action(screen,5);screen.handleInput(KEY_UP); }
    the_mesh.contacts[0].type=type;
    screen.handleInput(KEY_ENTER);
    EXPECT_EQ(task.ping,0u);EXPECT_EQ(task.traces,0u);EXPECT_EQ(task.admin,0u);EXPECT_EQ(the_mesh.deleted,0u);
    RecordingDisplay d(128,64);draw(screen,d);EXPECT_FALSE(d.contains("Options"));EXPECT_FALSE(d.contains("Delete contact?"));
    screen.handleInput(KEY_CONTEXT_MENU);draw(screen,d);EXPECT_FALSE(d.contains("Pin to dial"));EXPECT_FALSE(d.contains("Unpin"));
  }
}
TEST_F(NodeList, RoomsAndSensorsRetainApplicableActionsWithoutTraceOrPin) {
  for(uint8_t type:{uint8_t(3),uint8_t(4)})for(int row:{0,1,2,3,4}) {
    the_mesh=FakeMesh{};the_mesh.contacts={contact(2,type)};UITask task;NearbyScreen screen(&task);screen.onShow();
    screen.handleInput(KEY_CONTEXT_MENU);RecordingDisplay d(250,122,12,18);draw(screen,d);
    EXPECT_FALSE(d.contains("Trace"));EXPECT_FALSE(d.contains("Pin to dial"));
    screen.handleInput(KEY_CANCEL);action(screen,row);
    if(row==0)EXPECT_EQ(task.ping,1u);
    if(row==1) { draw(screen,d);EXPECT_TRUE(d.contains("Path Details")); }
    if(row==2)EXPECT_EQ(the_mesh.starred,1u);
    if(row==3)EXPECT_EQ(task.admin,1u);
    if(row==4) { screen.handleInput(KEY_UP);screen.handleInput(KEY_ENTER);EXPECT_EQ(the_mesh.deleted,1u); }
  }
}
TEST_F(NodeList, CompanionOnlyListAndUntypedAdvertsShowEmptyOnBothDisplays) {
  the_mesh.contacts={contact(1,1)};the_mesh.adverts[0]=advert(2,100,"Some repeater");
  for(auto size:{std::make_pair(128,64),{250,122}}) {
    UITask task;NearbyScreen screen(&task);RecordingDisplay d(size.first,size.second,size.first==250?12:6,size.first==250?18:9);screen.onShow();draw(screen,d);
    EXPECT_TRUE(d.contains("No contacts"));EXPECT_FALSE(d.contains("Companion"));EXPECT_TRUE(d.inBounds());
    screen.handleInput(KEY_CONTEXT_MENU);draw(screen,d);EXPECT_FALSE(d.contains("Ping"));EXPECT_FALSE(d.contains("Delete contact"));EXPECT_FALSE(d.contains("Pin to dial"));
  }
}

int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv);return RUN_ALL_TESTS(); }
