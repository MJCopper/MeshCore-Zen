#include <gtest/gtest.h>
#include <helpers/DoublerFileStorage.h>
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
