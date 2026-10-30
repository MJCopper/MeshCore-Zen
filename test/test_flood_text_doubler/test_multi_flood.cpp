#include <gtest/gtest.h>
#include <helpers/DoublerFileStorage.h>
#include <helpers/CompanionFloodDoubler.h>
#include <helpers/StaticPoolPacketManager.h>
#include <map>
#include <string>
#include <vector>

// Instantiate the production filesystem adapter against a fault-injectable FS.
// This tests write/readback/rename ordering, not merely a fake replace() result.
struct TestSettingsFS {
  std::map<std::string, std::string> files;
  bool failReadOpen = false, failWriteOpen = false, shortWrite = false;
  bool failClose = false, failVerify = false, failRename = false, failRemove = false;
  unsigned writes = 0, renames = 0;

  struct File {
    TestSettingsFS* fs;
    std::string path, data;
    size_t position = 0;
    bool valid, writing;
    operator bool() const { return valid; }
    size_t size() const { return data.size(); }
    size_t read(uint8_t* destination, size_t count) {
      if (fs->failVerify && path == "/doubler.tmp") return 0;
      size_t available = data.size() - position;
      if (count > available) count = available;
      memcpy(destination, data.data() + position, count);
      position += count;
      return count;
    }
    size_t write(const uint8_t* source, size_t count) {
      fs->writes++;
      if (fs->shortWrite) count /= 2;
      data.append((const char*)source, count);
      return count;
    }
    void close() {
      if (valid && writing && !fs->failClose) fs->files[path] = data;
    }
  };

  bool exists(const char* path) const { return files.count(path) != 0; }
  File open(const char* path, uint8_t mode = 0) {
    bool writing = mode != 0;
    bool valid = writing ? !failWriteOpen : (!failReadOpen && exists(path));
    if (valid && writing) files[path] = "";
    return {this, path, writing || !exists(path) ? "" : files.at(path), 0, valid, writing};
  }
  bool remove(const char* path) {
    if (failRemove) return false;
    return files.erase(path) != 0;
  }
  bool rename(const char* from, const char* to) {
    renames++;
    if (failRename || !exists(from)) return false;
    files[to] = files.at(from);
    files.erase(from);
    return true;
  }
};

class MultiFloodSettingsTest : public ::testing::Test {
protected:
  TestSettingsFS fs;
  DoublerFileStorage<TestSettingsFS> storage{fs, 1};
  DoublerSettings settings;
  char reply[128] = {};
  std::string json(unsigned mode) {
    return "{\"schema\":1,\"multi_flood\":" + std::to_string(mode) + "}\n";
  }
  void load(unsigned mode) {
    fs.files["/doubler.json"] = json(mode);
    ASSERT_EQ(DoublerSettings::VALID, settings.load(storage));
  }
  bool command(const char* text) { return settings.handleCommand(text, reply, storage); }
};

TEST_F(MultiFloodSettingsTest, MissingFileDefaultsToZeroWithoutWriting) {
  EXPECT_EQ(0, settings.getMode());
  EXPECT_EQ(DoublerSettings::DEFAULTED, settings.load(storage));
  EXPECT_EQ(0, settings.getMode());
  EXPECT_EQ(0u, fs.writes);
  EXPECT_TRUE(command("get multi.flood"));
  EXPECT_STREQ("> 0", reply);
  EXPECT_TRUE(command("set multi.flood 0"));
  EXPECT_STREQ("OK", reply);
  EXPECT_TRUE(fs.files.empty());
}

TEST_F(MultiFloodSettingsTest, EveryModeSurvivesReload) {
  for (unsigned mode = 0; mode <= 2; mode++) {
    load((mode + 1) % 3);
    std::string text = "set multi.flood " + std::to_string(mode);
    ASSERT_TRUE(command(text.c_str()));
    EXPECT_STREQ("OK", reply);
    EXPECT_EQ(mode, settings.getMode());
    DoublerSettings reboot;
    EXPECT_EQ(DoublerSettings::VALID, reboot.load(storage));
    EXPECT_EQ(mode, reboot.getMode());
    EXPECT_FALSE(fs.exists("/doubler.tmp"));
  }
}

TEST_F(MultiFloodSettingsTest, QueryAndUnchangedSetNeverWrite) {
  load(2);
  EXPECT_TRUE(command("get multi.flood  \t\r\n"));
  EXPECT_STREQ("> 2", reply);
  EXPECT_TRUE(command("set multi.flood 2"));
  EXPECT_STREQ("OK", reply);
  EXPECT_EQ(0u, fs.writes);
  EXPECT_EQ(0u, fs.renames);
}

TEST_F(MultiFloodSettingsTest, StrictlyRejectsInvalidArgumentsWithoutSaving) {
  load(1);
  for (const char* argument : {"", "-1", "3", "256", "01", "+1", "1abc", "1 2", "1.0", "true"}) {
    std::string text = "set multi.flood " + std::string(argument);
    EXPECT_TRUE(command(text.c_str())) << text;
    EXPECT_STREQ("Err - expected 0, 1 or 2", reply) << text;
    EXPECT_EQ(1, settings.getMode());
  }
  EXPECT_TRUE(command("set multi.flood"));
  EXPECT_STREQ("Err - expected 0, 1 or 2", reply);
  EXPECT_TRUE(command("get multi.flood 1"));
  EXPECT_STREQ("Err - usage: get multi.flood", reply);
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(MultiFloodSettingsTest, UnrelatedCommandsAreLeftToBaselineCli) {
  for (const char* text : {"", "get", "set", "get multi.acks", "set multi.acks 1",
                           "get multi.flooding", "set multi.floodish 2", "multi.flood 2"}) {
    strcpy(reply, "untouched");
    EXPECT_FALSE(command(text)) << text;
    EXPECT_STREQ("untouched", reply);
  }
}

TEST_F(MultiFloodSettingsTest, AcceptsWhitespaceAndReorderedJsonFields) {
  fs.files["/doubler.json"] = " \n{\"multi_flood\":2, \"schema\":1}\t\r\n";
  EXPECT_EQ(DoublerSettings::VALID, settings.load(storage));
  EXPECT_EQ(2, settings.getMode());
  EXPECT_TRUE(command("set multi.flood \t0\t\r\n"));
  EXPECT_STREQ("OK", reply);
  EXPECT_EQ(0, settings.getMode());
}

TEST_F(MultiFloodSettingsTest, CorruptDataFailsOffAndCanBeExplicitlyRepaired) {
  for (const char* contents : {"", "{", "null", "[]", "{}", "{\"schema\":1}",
       "{\"schema\":1,\"multi_flood\":3}", "{\"schema\":1,\"multi_flood\":-1}",
       "{\"schema\":1,\"multi_flood\":true}", "{\"schema\":1,\"multi_flood\":\"1\"}",
       "{\"schema\":1,\"multi_flood\":1.0}", "{\"schema\":1,\"multi_flood\":1}x",
       "{\"schema\":1,\"multi_flood\":1}}"}) {
    fs.files["/doubler.json"] = contents;
    EXPECT_EQ(DoublerSettings::CORRUPT, settings.load(storage)) << contents;
    EXPECT_EQ(0, settings.getMode());
    EXPECT_EQ(contents, fs.files.at("/doubler.json"));
    EXPECT_TRUE(command("set multi.flood 0"));
    EXPECT_STREQ("OK", reply);
    EXPECT_EQ(json(0), fs.files.at("/doubler.json"));
  }
}

TEST_F(MultiFloodSettingsTest, NewerSchemaAndUnknownFieldsArePreservedAndLocked) {
  for (const char* contents : {"{\"schema\":2,\"multi_flood\":2}",
       "{\"future\":{\"options\":[1,2]},\"schema\":2}",
       "{\"schema\":1,\"multi_flood\":2,\"future\":true}"}) {
    fs.files["/doubler.json"] = contents;
    EXPECT_EQ(DoublerSettings::UNSUPPORTED, settings.load(storage));
    EXPECT_EQ(0, settings.getMode());
    EXPECT_TRUE(command("set multi.flood 1"));
    EXPECT_NE(nullptr, strstr(reply, "file preserved"));
    EXPECT_EQ(contents, fs.files.at("/doubler.json"));
  }
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(MultiFloodSettingsTest, ReadFailureOrOversizeFileCannotBeOverwritten) {
  load(2);
  auto original = fs.files.at("/doubler.json");
  fs.failReadOpen = true;
  EXPECT_EQ(DoublerSettings::STORAGE_ERROR, settings.load(storage));
  EXPECT_EQ(0, settings.getMode());
  command("set multi.flood 1");
  EXPECT_NE(nullptr, strstr(reply, "file preserved"));
  EXPECT_EQ(original, fs.files.at("/doubler.json"));
  fs.failReadOpen = false;
  fs.files["/doubler.json"] = std::string(257, 'x');
  EXPECT_EQ(DoublerSettings::STORAGE_ERROR, settings.load(storage));
  command("set multi.flood 1");
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(MultiFloodSettingsTest, AllSaveFailuresKeepPreviousFileAndRuntimeMode) {
  bool TestSettingsFS::* failures[] = {&TestSettingsFS::failWriteOpen, &TestSettingsFS::shortWrite,
      &TestSettingsFS::failClose, &TestSettingsFS::failVerify, &TestSettingsFS::failRename};
  for (auto failure : failures) {
    load(1);
    fs.*failure = true;
    ASSERT_TRUE(command("set multi.flood 2"));
    EXPECT_STREQ("Err - unable to save multi.flood", reply);
    EXPECT_EQ(1, settings.getMode());
    EXPECT_EQ(json(1), fs.files.at("/doubler.json"));
    fs.*failure = false;
    DoublerSettings reboot;
    EXPECT_EQ(DoublerSettings::VALID, reboot.load(storage));
    EXPECT_EQ(1, reboot.getMode());
  }
}

TEST_F(MultiFloodSettingsTest, InterruptedTemporaryWriteIsIgnoredAtBoot) {
  load(2);
  fs.files["/doubler.tmp"] = "{\"schema\":1,";
  EXPECT_EQ(DoublerSettings::VALID, settings.load(storage));
  EXPECT_EQ(2, settings.getMode());
  command("set multi.flood 0");
  EXPECT_STREQ("OK", reply);
  EXPECT_EQ(json(0), fs.files.at("/doubler.json"));
}

TEST_F(MultiFloodSettingsTest, UnableToRemoveOldTemporaryFileDoesNotChangeSettings) {
  load(1);
  fs.files["/doubler.tmp"] = "interrupted";
  fs.failRemove = true;
  command("set multi.flood 2");
  EXPECT_STREQ("Err - unable to save multi.flood", reply);
  EXPECT_EQ(1, settings.getMode());
  EXPECT_EQ(json(1), fs.files.at("/doubler.json"));
}

TEST_F(MultiFloodSettingsTest, BaselinePreferenceSavesDoNotTouchModuleFile) {
  load(2);
  fs.files["/prefs.json"] = "{\"radio\":{\"multi_ack\":1}}";
  auto baseline = fs.files.at("/prefs.json");
  command("set multi.flood 0");
  EXPECT_EQ(baseline, fs.files.at("/prefs.json"));
  fs.files["/prefs.json"] = "{\"radio\":{\"multi_ack\":0}}";
  EXPECT_EQ(DoublerSettings::VALID, settings.load(storage));
  EXPECT_EQ(0, settings.getMode());
}

TEST_F(MultiFloodSettingsTest, CompanionCliSupportsPlainAndPrefixedCommands) {
  CompanionFloodDoubler companion;
  EXPECT_EQ(DoublerSettings::DEFAULTED, companion.load(fs, 1));
  EXPECT_TRUE(companion.handleCommand("  ab|get multi.flood", reply, fs, 1));
  EXPECT_STREQ("ab|> 0", reply);
  EXPECT_TRUE(companion.handleCommand("xy|set multi.flood 1", reply, fs, 1));
  EXPECT_STREQ("xy|OK", reply);
  EXPECT_EQ(1, companion.packets.getMode());
  EXPECT_TRUE(companion.handleCommand("get multi.flood", reply, fs, 1));
  EXPECT_STREQ("> 1", reply);
  CompanionFloodDoubler reboot;
  EXPECT_EQ(DoublerSettings::VALID, reboot.load(fs, 1));
  EXPECT_EQ(1, reboot.packets.getMode());
}

TEST_F(MultiFloodSettingsTest, CompanionCliLeavesUnrelatedPrefixHandlingToBaseline) {
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  for (auto command : {"ab|get name", "ab|get multi.acks", "get name", "abc", "", "a|"}) {
    strcpy(reply, "untouched");
    EXPECT_FALSE(companion.handleCommand(command, reply, fs, 1));
    EXPECT_STREQ("untouched", reply);
  }
  EXPECT_TRUE(companion.handleCommand("ab|set multi.flood 9", reply, fs, 1));
  EXPECT_STREQ("ab|Err - expected 0, 1 or 2", reply);
  EXPECT_EQ(0, companion.packets.getMode());
}

TEST_F(MultiFloodSettingsTest, CompanionSaveFailureKeepsPolicyModeAndReplyPrefix) {
  load(1);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  fs.failRename = true;
  EXPECT_TRUE(companion.handleCommand("ab|set multi.flood 2", reply, fs, 1));
  EXPECT_STREQ("ab|Err - unable to save multi.flood", reply);
  EXPECT_EQ(1, companion.packets.getMode());
  EXPECT_EQ(json(1), fs.files.at("/doubler.json"));
}

TEST_F(MultiFloodSettingsTest, CompanionFutureSchemaRemainsOffAndPreserved) {
  fs.files["/doubler.json"] = "{\"schema\":2,\"multi_flood\":2}";
  CompanionFloodDoubler companion;
  EXPECT_EQ(DoublerSettings::UNSUPPORTED, companion.load(fs, 1));
  EXPECT_EQ(0, companion.packets.getMode());
  EXPECT_TRUE(companion.handleCommand("ab|set multi.flood 1", reply, fs, 1));
  EXPECT_NE(nullptr, strstr(reply, "file preserved"));
  EXPECT_EQ(0u, fs.writes);
}

TEST_F(MultiFloodSettingsTest, CompanionSettingsUseSuppliedPrimaryFsOnly) {
  TestSettingsFS secondary;
  secondary.files["/doubler.json"] = json(2);
  CompanionFloodDoubler companion;
  EXPECT_EQ(DoublerSettings::DEFAULTED, companion.load(fs, 1));
  EXPECT_EQ(0, companion.packets.getMode());
  companion.handleCommand("set multi.flood 1", reply, fs, 1);
  EXPECT_EQ(json(1), fs.files.at("/doubler.json"));
  EXPECT_EQ(json(2), secondary.files.at("/doubler.json"));
}

TEST_F(MultiFloodSettingsTest, CompanionPendingWorkSurvivesUntilCancellationOrCompletion) {
  struct ZeroRNG : mesh::RNG {
    void random(uint8_t* destination, size_t length) override { memset(destination, 0, length); }
  } rng;
  StaticPoolPacketManager manager(16);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  companion.handleCommand("set multi.flood 1", reply, fs, 1);
  auto p = manager.allocNew();
  p->header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  p->setPathHashSizeAndCount(1, 0);
  p->payload_len = 19;
  memset(p->payload, 0, p->payload_len);
  EXPECT_FALSE(companion.hasPendingWork());
  companion.packets.onTransmitStarted(p);
  ASSERT_TRUE(companion.packets.onTransmitComplete(p, manager, rng, 1000));
  EXPECT_TRUE(companion.hasPendingWork());
  EXPECT_EQ(0, manager.getOutboundTotal());  // optional work must inhibit sleep independently
  companion.handleCommand("set multi.flood 0", reply, fs, 1);
  EXPECT_EQ(nullptr, companion.packets.nextReady(1500));
  EXPECT_EQ(nullptr, companion.packets.takeExpired(1500, p));  // active copy is never freed
  EXPECT_TRUE(companion.hasPendingWork());
  companion.packets.onTransmitStarted(p);
  EXPECT_FALSE(companion.packets.onTransmitComplete(p, manager, rng, 1600));
  manager.free(p);
  EXPECT_FALSE(companion.hasPendingWork());
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(MultiFloodSettingsTest, ExplicitForwardingTrackingExcludesEvenZeroEntryRelays) {
  struct ZeroRNG : mesh::RNG {
    void random(uint8_t* destination, size_t length) override { memset(destination, 0, length); }
  } rng;
  StaticPoolPacketManager manager(16);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  companion.handleCommand("set multi.flood 2", reply, fs, 1);
  for (unsigned round = 0; round < 2; round++) {
    std::vector<mesh::Packet*> relays;
    for (unsigned i = 0; i < 16; i++) {
      auto p = manager.allocNew();
      ASSERT_NE(nullptr, p);
      p->header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
      p->setPathHashSizeAndCount(1, 0);  // unusual forwarded metadata, including a path-count wrap
      companion.noteForwarded(p);
      relays.push_back(p);
    }
    for (auto p : relays) {
      companion.onTransmitStarted(p);
      EXPECT_FALSE(companion.onTransmitComplete(p, manager, rng, 1000));
      companion.onPacketReleased(p);
      manager.free(p);
    }
    EXPECT_EQ(0u, companion.packets.pendingCount());
    EXPECT_EQ(16, manager.getFreeCount());
  }
  // After release, a local packet reusing the same pool is eligible normally.
  auto local = manager.allocNew();
  local->header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  local->setPathHashSizeAndCount(1, 0);
  companion.onTransmitStarted(local);
  EXPECT_TRUE(companion.onTransmitComplete(local, manager, rng, 2000));
  companion.onTransmitStarted(local);
  EXPECT_FALSE(companion.onTransmitComplete(local, manager, rng, 2600));
  companion.onPacketReleased(local);
  manager.free(local);
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(MultiFloodSettingsTest, ForwardedStartFailureReleasesOriginRecord) {
  struct ZeroRNG : mesh::RNG {
    void random(uint8_t* destination, size_t length) override { memset(destination, 0, length); }
  } rng;
  StaticPoolPacketManager manager(16);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  companion.handleCommand("set multi.flood 2", reply, fs, 1);
  auto p = manager.allocNew();
  p->header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  p->setPathHashSizeAndCount(1, 0);
  companion.noteForwarded(p);
  companion.onPacketReleased(p);  // same cleanup hook used for failed starts/timeouts
  companion.onTransmitStarted(p);
  EXPECT_TRUE(companion.onTransmitComplete(p, manager, rng, 1000));
  companion.onPacketReleased(p);
  manager.free(p);
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(MultiFloodSettingsTest, UnexpectedForwardingTrackerOverflowFailsClosed) {
  struct ZeroRNG : mesh::RNG {
    void random(uint8_t* destination, size_t length) override { memset(destination, 0, length); }
  } rng;
  StaticPoolPacketManager futurePool(17);  // exercise a future pool/adapter capacity mismatch
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  companion.handleCommand("set multi.flood 2", reply, fs, 1);
  std::vector<mesh::Packet*> relays;
  for (unsigned i = 0; i < 17; i++) {
    auto p = futurePool.allocNew();
    ASSERT_NE(nullptr, p);
    p->header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
    p->setPathHashSizeAndCount(1, 0);
    companion.noteForwarded(p);
    relays.push_back(p);
  }
  for (auto p : relays) {
    companion.onTransmitStarted(p);
    EXPECT_FALSE(companion.onTransmitComplete(p, futurePool, rng, 1000));
    companion.onPacketReleased(p);
    futurePool.free(p);
  }
  auto local = futurePool.allocNew();
  companion.onTransmitStarted(local);
  EXPECT_FALSE(companion.onTransmitComplete(local, futurePool, rng, 2000));
  companion.onPacketReleased(local);
  futurePool.free(local);
  EXPECT_EQ(17, futurePool.getFreeCount());
}

namespace {
struct ContextRNG : mesh::RNG {
  void random(uint8_t* destination, size_t length) override { memset(destination, 0, length); }
};
mesh::Packet* contextPacket(StaticPoolPacketManager& manager) {
  auto p = manager.allocNew();
  p->header = ROUTE_TYPE_FLOOD | (PAYLOAD_TYPE_GRP_TXT << PH_TYPE_SHIFT);
  p->setPathHashSizeAndCount(1, 0);
  p->payload_len = 19;
  memset(p->payload, 0, p->payload_len);
  return p;
}
}

TEST_F(MultiFloodSettingsTest, SuccessfulCliOffOnAndQueryCannotRevivePendingCopy) {
  load(1);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  StaticPoolPacketManager manager(16);
  ContextRNG rng;
  auto p = contextPacket(manager);
  companion.onTransmitStarted(p);
  ASSERT_TRUE(companion.onTransmitComplete(p, manager, rng, 1000));
  ASSERT_TRUE(companion.handleCommand("set multi.flood 0", reply, fs, 1));
  EXPECT_STREQ("OK", reply);
  ASSERT_TRUE(companion.handleCommand("set multi.flood 1", reply, fs, 1));
  EXPECT_STREQ("OK", reply);
  companion.handleCommand("get multi.flood", reply, fs, 1);
  EXPECT_STREQ("> 1", reply);
  EXPECT_EQ(nullptr, companion.packets.nextReady(1500));
  EXPECT_TRUE(companion.hasPendingWork());
  auto cancelled = companion.packets.takeExpired(1001, nullptr);
  ASSERT_EQ(p, cancelled);
  companion.onPacketReleased(cancelled);
  manager.free(cancelled);
  EXPECT_FALSE(companion.hasPendingWork());
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(MultiFloodSettingsTest, FailedCliDisablePreservesWaitingCopyAndStoredMode) {
  load(1);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  StaticPoolPacketManager manager(16);
  ContextRNG rng;
  auto p = contextPacket(manager);
  companion.onTransmitStarted(p);
  ASSERT_TRUE(companion.onTransmitComplete(p, manager, rng, 1000));
  fs.failRename = true;
  companion.handleCommand("set multi.flood 0", reply, fs, 1);
  EXPECT_STREQ("Err - unable to save multi.flood", reply);
  EXPECT_EQ(1, companion.packets.getMode());
  EXPECT_EQ(json(1), fs.files.at("/doubler.json"));
  EXPECT_EQ(p, companion.packets.nextReady(1500));
  companion.onTransmitStarted(p);
  EXPECT_FALSE(companion.onTransmitComplete(p, manager, rng, 1600));
  companion.onPacketReleased(p);
  manager.free(p);
  EXPECT_EQ(16, manager.getFreeCount());
}

TEST_F(MultiFloodSettingsTest, ContextInvalidationReleasesPendingWorkWithoutAnySettingsWrite) {
  load(1);
  CompanionFloodDoubler companion;
  companion.load(fs, 1);
  companion.radioApplied(916.575f, 62.5f, 7, 7);
  StaticPoolPacketManager manager(16);
  ContextRNG rng;
  auto p = contextPacket(manager);
  companion.onTransmitStarted(p);
  ASSERT_TRUE(companion.onTransmitComplete(p, manager, rng, 1000));
  auto writes = fs.writes;
  companion.radioApplied(917.0f, 62.5f, 7, 7);
  companion.radioApplied(916.575f, 62.5f, 7, 7);
  EXPECT_EQ(nullptr, companion.packets.nextReady(1500));
  EXPECT_TRUE(companion.hasPendingWork());
  ASSERT_EQ(p, companion.packets.takeExpired(1001, nullptr));
  companion.onPacketReleased(p);
  manager.free(p);
  EXPECT_FALSE(companion.hasPendingWork());
  EXPECT_EQ(writes, fs.writes);
  EXPECT_EQ(json(1), fs.files.at("/doubler.json"));
  EXPECT_EQ(16, manager.getFreeCount());
}
