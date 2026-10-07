#include <gtest/gtest.h>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <cstring>

class FakePetFS;
class File {
  FakePetFS* _fs=nullptr; std::string _path; size_t _at=0; bool _write=false;
public:
  File()=default;
  File(FakePetFS* fs,const std::string& path,bool write):_fs(fs),_path(path),_write(write) {}
  operator bool() const { return _fs!=nullptr; }
  size_t size(); size_t write(const uint8_t* data,size_t count);
  int read(uint8_t* data,size_t count); void close() {}
};
class FakePetFS {
public:
  std::map<std::string,std::vector<uint8_t>> files;
  std::set<int> fail_rename;
  int rename_count=0,read_count=0,fail_read=0,cut_after=0,mutations=0;
  bool short_write=false,full=false;
  void mutation() { if(cut_after && ++mutations==cut_after)throw std::runtime_error("Power loss"); }
  File open(const char* path) { return files.count(path)?File(this,path,false):File(); }
  File open(const char* path,const char*,bool=true) {
    if(full)return File(); files[path].clear(); mutation(); return File(this,path,true);
  }
  bool remove(const char* path) { files.erase(path); mutation(); return true; }
  bool rename(const char* source,const char* target) {
    ++rename_count;
    if(fail_rename.count(rename_count) || !files.count(source) || files.count(target))return false;
    files[target]=files[source]; files.erase(source); mutation(); return true;
  }
};
size_t File::size() { return _fs->files[_path].size(); }
size_t File::write(const uint8_t* data,size_t count) {
  size_t written=_fs->short_write?count/2:count;
  _fs->files[_path].assign(data,data+written); _fs->mutation(); return written;
}
int File::read(uint8_t* data,size_t count) {
  if(++_fs->read_count==_fs->fail_read)return -1;
  auto& bytes=_fs->files[_path]; size_t available=bytes.size()-_at;
  if(count>available)count=available; memcpy(data,bytes.data()+_at,count); _at+=count; return count;
}
#define FILESYSTEM FakePetFS
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetSnapshotFilesystem.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/pet/PetSnapshotStore.h"
using namespace zen::pet;

TEST(PetSnapshotFilesystem, RealReplacementRecoversAtEveryMutationAndNeverTouchesBaseline) {
  for(int cut=1;cut<=9;++cut) {
    FakePetFS fs; fs.files["/identity"]={1,2,3}; fs.files["/prefs"]={4,5,6};
    zen::ZenStore zen; zen.begin(&fs,nullptr); PetSnapshotFilesystem backend(zen);
    PetSnapshotStore store; PetSnapshot s; ASSERT_TRUE(store.save(backend,s));
    s.engine.state.fullness=40; fs.mutations=0; fs.cut_after=cut;
    try { store.save(backend,s); } catch(const std::runtime_error&) {}
    fs.cut_after=0; PetSnapshot restored; PetSnapshotStore reboot;
    ASSERT_EQ(PetSnapshotStore::RESTORED,reboot.load(backend,restored)) << cut;
    EXPECT_TRUE(restored.engine.state.fullness==70 || restored.engine.state.fullness==40);
    EXPECT_EQ((std::vector<uint8_t>{1,2,3}),fs.files["/identity"]);
    EXPECT_EQ((std::vector<uint8_t>{4,5,6}),fs.files["/prefs"]);
  }
}
TEST(PetSnapshotFilesystem, ShortWritesFullStorageAndRenameFailuresPreserveCheckpoint) {
  for(int failure=0;failure<5;++failure) {
    FakePetFS fs; zen::ZenStore zen; zen.begin(&fs,nullptr);
    PetSnapshotFilesystem backend(zen); PetSnapshotStore store; PetSnapshot s;
    ASSERT_TRUE(store.save(backend,s)); fs.rename_count=0;
    if(failure==0)fs.short_write=true; if(failure==1)fs.full=true;
    if(failure==2)fs.fail_rename={2}; if(failure==3)fs.fail_rename={3};
    if(failure==4)fs.fail_rename={3,4};
    s.engine.state.fullness=40; EXPECT_FALSE(store.save(backend,s));
    fs.short_write=fs.full=false; fs.fail_rename.clear();
    PetSnapshot restored; PetSnapshotStore reboot;
    EXPECT_EQ(PetSnapshotStore::RESTORED,reboot.load(backend,restored));
  }
}
TEST(PetSnapshotFilesystem, VerificationFailureAndUnavailableStorageAreSafe) {
  FakePetFS fs; zen::ZenStore zen; zen.begin(&fs,nullptr);
  PetSnapshotFilesystem backend(zen); PetSnapshotStore store; PetSnapshot s;
  ASSERT_TRUE(store.save(backend,s)); fs.fail_read=fs.read_count+1;
  EXPECT_FALSE(store.save(backend,s)); fs.fail_read=0;
  PetSnapshotStore reboot; EXPECT_EQ(PetSnapshotStore::RESTORED,reboot.load(backend,s));
  zen::ZenStore missing; PetSnapshotFilesystem absent(missing);
  EXPECT_EQ(PetSnapshotStore::UNAVAILABLE,reboot.load(absent,s));
  EXPECT_FALSE(reboot.save(absent,s));
}
TEST(PetSnapshotFilesystem, ExtensionSaveDoesNotClearPreferenceRecoveryFlag) {
  FakePetFS fs; zen::ZenStore zen; zen.begin(&fs,nullptr);
  ZenPrefs prefs;
  ASSERT_TRUE(zen.save(prefs)); fs.files["/zen_prefs.tmp"]=fs.files["/zen_prefs"];
  fs.files.erase("/zen_prefs"); ASSERT_TRUE(zen.load(prefs)); ASSERT_TRUE(zen.recoveryNeeded());
  PetSnapshotFilesystem backend(zen); PetSnapshotStore store; PetSnapshot s;
  ASSERT_TRUE(store.save(backend,s)); EXPECT_TRUE(zen.recoveryNeeded());
}

int main(int argc,char** argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
