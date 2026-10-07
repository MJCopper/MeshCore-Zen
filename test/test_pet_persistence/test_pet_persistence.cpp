#include <gtest/gtest.h>
#include <array>
#include <vector>
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetPersistence.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetPage.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/OperationResult.h"

using namespace zen::pet;

struct MemoryPetFiles {
  std::array<std::vector<uint8_t>,3> files;
  bool online=true,fail_write=false,fail_read=false;
  int writes=0,cut=-1;
  bool available() const { return online; }
  bool read(unsigned i,uint8_t* bytes,size_t capacity,size_t& size) {
    size=files[i].size(); if(fail_read || !size)return false;
    memcpy(bytes,files[i].data(),size<capacity?size:capacity); return true;
  }
  bool replace(const uint8_t* bytes,size_t size) {
    ++writes; if(fail_write)return false;
    files[1].assign(bytes,bytes+size);
    if(cut==0) { files[1].resize(size/2); return false; }
    if(cut==1)return false;
    files[2]=files[0]; files[0].clear(); if(cut==2)return false;
    files[0]=files[1]; files[1].clear(); if(cut==3)return false;
    files[2].clear(); return true;
  }
};

static void requestSave(Page& page) {
  page.close(); page.input('e','e','b','h','u','d');
  page.input('u','e','b','h','u','d'); // Last ordinary menu item: Save Pet.
  page.input('e','e','b','h','u','d');
}

TEST(PetPersistence, CodecAllFormsTemperamentsCreditsAndRewardsRoundTrip) {
  for(unsigned id=0;id<Evolution::FORMS;++id)for(unsigned nature=0;nature<4;++nature) {
    PetSnapshot s; s.engine.state.form=id; s.engine.state.xp=41050;
    s.engine.state.bond=100; s.temperament=nature;
    s.engine.cooldown=12345; s.engine.energy_credit=234;
    s.engine.food_credit=567; s.engine.hunger_credit=890;
    s.rewards.progress={true,true,true,true,5,35,5};
    s.rewards.day={20500,1234567,true}; s.saved_date=20500;
    uint8_t bytes[PetSnapshotCodec::SIZE]; ASSERT_TRUE(PetSnapshotCodec::encode(s,9,bytes));
    PetSnapshot decoded; uint32_t generation=0;
    ASSERT_EQ(PetSnapshotCodec::VALID,PetSnapshotCodec::decode(bytes,sizeof(bytes),decoded,generation));
    EXPECT_EQ(9u,generation); EXPECT_EQ(id,decoded.engine.state.form);
    EXPECT_EQ(nature,decoded.temperament); EXPECT_EQ(12345u,decoded.engine.cooldown);
    EXPECT_EQ(234u,decoded.engine.energy_credit); EXPECT_EQ(567u,decoded.engine.food_credit);
    EXPECT_EQ(890u,decoded.engine.hunger_credit); EXPECT_EQ(35,decoded.rewards.progress.pending_xp);
    EXPECT_EQ(5,decoded.rewards.progress.pending_bond); EXPECT_EQ(20500,decoded.saved_date);
    EXPECT_EQ(PetSnapshotCodec::fingerprint(s),PetSnapshotCodec::fingerprint(decoded));
  }
}
TEST(PetPersistence, CodecRejectsEveryBitCorruptionTruncationAndInvalidState) {
  PetSnapshot s,decoded; uint32_t generation=0; uint8_t bytes[PetSnapshotCodec::SIZE];
  ASSERT_TRUE(PetSnapshotCodec::encode(s,1,bytes));
  for(unsigned length=0;length<sizeof(bytes);++length)
    EXPECT_EQ(PetSnapshotCodec::INVALID,PetSnapshotCodec::decode(bytes,length,decoded,generation));
  for(unsigned i=8;i<sizeof(bytes);++i) {
    bytes[i]^=1;
    EXPECT_EQ(PetSnapshotCodec::INVALID,PetSnapshotCodec::decode(bytes,sizeof(bytes),decoded,generation));
    bytes[i]^=1;
  }
  s.engine.state.form=189; EXPECT_FALSE(PetSnapshotCodec::encode(s,1,bytes));
  s.engine.state.form=0; s.engine.cooldown=300001; EXPECT_FALSE(PetSnapshotCodec::encode(s,1,bytes));
  s.engine.cooldown=0; s.temperament=4; EXPECT_FALSE(PetSnapshotCodec::encode(s,1,bytes));
}
TEST(PetPersistence, FutureSchemaInAnyCandidateLocksWithoutWriting) {
  for(unsigned slot=0;slot<3;++slot) {
    MemoryPetFiles files; PetSnapshot s; uint8_t bytes[PetSnapshotCodec::SIZE];
    ASSERT_TRUE(PetSnapshotCodec::encode(s,1,bytes));
    files.files[0].assign(bytes,bytes+sizeof(bytes));
    bytes[4]=2; files.files[slot].assign(bytes,bytes+sizeof(bytes));
    files.files[slot].resize(140); // Future larger envelopes must also lock.
    PetSnapshotStore store; EXPECT_EQ(PetSnapshotStore::INCOMPATIBLE,store.load(files,s));
    EXPECT_FALSE(store.save(files,s)); EXPECT_EQ(0,files.writes);
  }
}
TEST(PetPersistence, StoreRecoversEveryInterruptedReplacementStage) {
  for(int cut=0;cut<=3;++cut) {
    MemoryPetFiles files; PetSnapshotStore store; PetSnapshot s;
    ASSERT_TRUE(store.save(files,s)); s.engine.state.fullness=50; files.cut=cut;
    EXPECT_FALSE(store.save(files,s));
    PetSnapshot restored; PetSnapshotStore reboot;
    ASSERT_EQ(PetSnapshotStore::RESTORED,reboot.load(files,restored));
    EXPECT_TRUE(restored.engine.state.fullness==70 || restored.engine.state.fullness==50);
    EXPECT_EQ(cut<2?70:50,restored.engine.state.fullness);
  }
}
TEST(PetPersistence, MissingCorruptBackupUnavailableAndVerificationFailures) {
  MemoryPetFiles files; PetSnapshotStore store; PetSnapshot s;
  EXPECT_EQ(PetSnapshotStore::MISSING,store.load(files,s));
  files.files[0]={1,2,3}; EXPECT_EQ(PetSnapshotStore::CORRUPT,store.load(files,s));
  files.files[0].clear(); ASSERT_TRUE(store.save(files,s));
  files.files[2]=files.files[0]; files.files[0][20]^=1;
  EXPECT_EQ(PetSnapshotStore::RESTORED,store.load(files,s));
  files.fail_write=true; EXPECT_FALSE(store.save(files,s)); files.fail_write=false;
  files.fail_read=true; EXPECT_FALSE(store.save(files,s)); files.fail_read=false;
  files.online=false; EXPECT_EQ(PetSnapshotStore::UNAVAILABLE,store.load(files,s));
}
TEST(PetPersistence, RestoreFreezesOfflineTimeCooldownAndFractionalCredits) {
  Engine a; a.update(0,true,false,false); ASSERT_EQ(Engine::OK,a.train());
  a.update(1200,true,false,false); auto saved=a.checkpoint();
  Engine b; b.restore(saved,9000000); b.update(9000000,true,false,false);
  EXPECT_EQ(saved.cooldown,b.trainingRestMillis()); EXPECT_EQ(saved.state.fullness,b.state().fullness);
  EXPECT_EQ(saved.energy_credit,b.checkpoint().energy_credit);
  b.update(9000001,true,false,false); EXPECT_EQ(saved.cooldown-1,b.trainingRestMillis());
}
TEST(PetPersistence, InitialManualDuplicateDisabledBatteryAndStorageDeferral) {
  g_mock_millis=0; MemoryPetFiles files; Page page; PetPersistence p;
  ASSERT_EQ(PetSnapshotStore::MISSING,p.start(0,files,page)); page.update(0,true,false,false);
  EXPECT_EQ(PetPersistence::NONE,p.process(0,files,page,true,false,0,false,true));
  EXPECT_EQ(0,files.writes);
  requestSave(page);
  EXPECT_EQ(PetPersistence::DEFERRED,p.process(1,files,page,true,false,0,true,false));
  EXPECT_EQ(0,files.writes);
  EXPECT_EQ(PetPersistence::SAVED,p.process(2,files,page,true,false,0,true,true));
  EXPECT_EQ(1,files.writes);
  g_mock_millis=3000; requestSave(page);
  EXPECT_EQ(PetPersistence::UNCHANGED,p.process(3000,files,page,true,false,0,true,true));
  EXPECT_EQ(1,files.writes);
  requestSave(page); p.process(3001,files,page,false,false,0,true,true);
  p.process(86403001,files,page,false,false,0,true,true); EXPECT_EQ(1,files.writes);
}
TEST(PetPersistence, CorruptionNeverAutoOverwritesButManualCanReplace) {
  MemoryPetFiles files; files.files[0]={1,2,3}; Page page; PetPersistence p;
  EXPECT_EQ(PetSnapshotStore::CORRUPT,p.start(0,files,page)); page.update(0,true,false,false);
  p.process(86400001,files,page,true,false,0,true,true); EXPECT_EQ(0,files.writes);
  g_mock_millis=86400002; requestSave(page);
  EXPECT_EQ(PetPersistence::SAVED,p.process(86400002,files,page,true,false,0,true,true));
}
TEST(PetPersistence, SleepingManualSaveDoesNotWakePetAndRestoreClearsTransients) {
  g_mock_millis=0; Page page; page.update(0,true,true,false);
  page.input('e','e','b','h','u','d');
  for(int i=0;i<5;++i)page.input('d','e','b','h','u','d');
  page.input('e','e','b','h','u','d'); EXPECT_TRUE(page.takeSaveRequest());
  EXPECT_EQ(PetPersonality::SLEEP,page.personality().presentation(0).pose);
  auto s=page.checkpoint(); page.restore(s,100); page.update(100,true,true,false);
  EXPECT_FALSE(page.trainingActive()); EXPECT_FALSE(page.menuOpen());
  EXPECT_EQ(PetPersonality::SLEEP,page.personality().presentation(100).pose);
}
TEST(PetPersistence, PendingRewardsApplyOnceAndNewRewardsBlockedUntilSafeDay) {
  PetMeshRewards a; PetMeshRewards::Checkpoint c;
  c.progress.pending_xp=35; c.progress.pending_bond=5; a.restore(c,0);
  Engine pet; pet.update(0,true,false,false); a.update(0,true,false,false,false,0);
  EXPECT_TRUE(a.apply(pet)); EXPECT_FALSE(a.apply(pet)); EXPECT_EQ(35,pet.state().xp);
  uint8_t key[32]={1}; a.started(PetMeshRewards::DM,key,1,0,9,true);
  a.completed(9,PetMeshRewards::DM,true,true); EXPECT_EQ(0,a.progress().pending_xp);
  a.update(86400000,true,false,false,false,0);
  a.started(PetMeshRewards::DM,key,2,0,10,true); a.completed(10,PetMeshRewards::DM,true,true);
  EXPECT_EQ(20,a.progress().pending_xp);
}
TEST(PetPersistence, MidnightManualGateDstCorrectionsAndUnsychronizedFallback) {
  PetSavePolicy p; p.begin(0,true,false,-1); p.update(0,true,true,10*3600);
  EXPECT_TRUE(p.due(0)); p.complete(true,0);
  p.update(14*3600000,true,true,86400); EXPECT_FALSE(p.due(14*3600000));
  p.update(24*3600000,true,true,86400+10*3600); EXPECT_FALSE(p.due(24*3600000));
  p.update(38*3600000,true,true,2*86400); EXPECT_TRUE(p.due(38*3600000)); p.complete(true,38*3600000);
  p.update(39*3600000,true,true,20*86400); EXPECT_FALSE(p.due(39*3600000));
  p.update(40*3600000,true,true,86400); EXPECT_FALSE(p.due(40*3600000));
  PetSavePolicy fallback; fallback.begin(0,false,false,-1);
  fallback.update(86399999,true,false,0); EXPECT_FALSE(fallback.due(86399999));
  fallback.update(86400000,true,false,0); EXPECT_TRUE(fallback.due(86400000));
}
TEST(PetPersistence, PolicyWrapRetryBackoffAndDuplicateRequests) {
  PetSavePolicy p; uint32_t start=0xfffffff0u; p.begin(start,true,false,-1);
  p.update(start,true,false,0); p.complete(false,start);
  EXPECT_FALSE(p.due(start+3599999)); EXPECT_TRUE(p.due(start+3600000));
  p.request(start+1); EXPECT_TRUE(p.manual()); p.complete(true,start+1);
  p.request(start+2); EXPECT_FALSE(p.manual()); p.request(start+2001); EXPECT_TRUE(p.manual());
}
TEST(PetPersistence, BackgroundFailuresLogWithoutPopup) {
  auto r=zen::OperationResult::make(zen::Operation::PET_SAVE,zen::OperationOutcome::FAULT,
      zen::OperationReason::SAVE_FAILED,zen::RESULT_LOG_ONLY);
  EXPECT_FALSE(zen::OperationResultCatalog::shouldPopup(r));
  r.flags=0; EXPECT_TRUE(zen::OperationResultCatalog::shouldPopup(r));
}

TEST(PetPersistence, InitialSaveWaitsForBatteryAndRestoreDoesNotWriteOrAge) {
  g_mock_millis=0; MemoryPetFiles files; Page page; PetPersistence saving;
  saving.start(0,files,page); page.update(0,true,false,false);
  saving.process(0,files,page,true,false,0,false,true); EXPECT_EQ(0,files.writes);
  saving.process(100,files,page,true,false,0,true,true); ASSERT_EQ(1,files.writes);
  auto checkpoint=page.checkpoint(); Page restored; PetPersistence reboot;
  EXPECT_EQ(PetSnapshotStore::RESTORED,reboot.start(1000000,files,restored));
  restored.update(1000000,true,false,false);
  EXPECT_EQ(checkpoint.engine.state.fullness,restored.checkpoint().engine.state.fullness);
  EXPECT_EQ(checkpoint.temperament,restored.checkpoint().temperament);
  reboot.process(1000000,files,restored,true,false,0,true,true); EXPECT_EQ(1,files.writes);
  g_mock_millis=1000001; requestSave(restored);
  EXPECT_EQ(PetPersistence::LOW_BATTERY,
      reboot.process(1000001,files,restored,true,false,0,false,true)); EXPECT_EQ(1,files.writes);
}
TEST(PetPersistence, ManualRequestsCoalesceAndLowPowerDoesNotPreventSafeSave) {
  g_mock_millis=0; MemoryPetFiles files; Page page; PetPersistence p;
  p.start(0,files,page); page.update(0,true,false,true);
  requestSave(page); requestSave(page);
  EXPECT_EQ(PetPersistence::DEFERRED,p.process(0,files,page,true,false,0,true,false));
  EXPECT_EQ(PetPersistence::NONE,p.process(1000,files,page,true,false,0,true,false));
  EXPECT_EQ(PetPersistence::SAVED,p.process(1001,files,page,true,false,0,true,true));
  EXPECT_EQ(1,files.writes); EXPECT_EQ(PetPersonality::REST,page.personality().presentation(1001).pose);
}
TEST(PetPersistence, PendingManualSaveWaitsForTrainingAndDisabledDropsRequest) {
  g_mock_millis=0; MemoryPetFiles files; Page page; PetPersistence p;
  p.start(0,files,page); page.update(0,true,false,false);
  requestSave(page); page.close();
  page.input('e','e','b','h','u','d'); page.input('d','e','b','h','u','d'); page.input('e','e','b','h','u','d');
  ASSERT_TRUE(page.trainingActive());
  EXPECT_EQ(PetPersistence::NONE,p.process(0,files,page,true,false,0,true,true)); EXPECT_EQ(0,files.writes);
  page.cancelTraining(); EXPECT_EQ(PetPersistence::SAVED,p.process(1,files,page,true,false,0,true,true));
  g_mock_millis=3000; requestSave(page); p.process(3000,files,page,false,false,0,true,true);
  p.process(3001,files,page,true,false,0,true,true); EXPECT_EQ(1,files.writes);
}
TEST(PetPersistence, SpringDstManualNearMidnightAndRebootGuards) {
  PetSavePolicy p; p.begin(0,false,false,10); p.update(0,true,true,10*86400);
  p.update(23*3600000,true,true,11*86400); EXPECT_FALSE(p.due(23*3600000));
  p.update(24*3600000,true,true,11*86400+3600); EXPECT_FALSE(p.due(24*3600000));
  p.request(24*3600000); ASSERT_TRUE(p.due(24*3600000)); p.complete(true,24*3600000);
  p.update(47*3600000,true,true,12*86400); EXPECT_FALSE(p.due(47*3600000));
  p.update(71*3600000,true,true,13*86400); EXPECT_TRUE(p.due(71*3600000));
  PetSavePolicy reboot; reboot.begin(0,false,false,13);
  reboot.update(0,true,true,14*86400); EXPECT_FALSE(reboot.due(0));
  reboot.update(1000,true,true,100*86400); EXPECT_FALSE(reboot.due(1000));
}

int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
