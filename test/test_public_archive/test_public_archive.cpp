#include <gtest/gtest.h>
#include "../../examples/simple_room_server/PublicChannelArchive.h"
TEST(PublicArchive, OnlyPlainNonemptyPublicTextIsAccepted) {
  PublicChannelArchive archive;
  uint8_t data[MAX_PACKET_PAYLOAD + 1] = {};
  uint32_t original = 1234, timestamp;
  memcpy(data, &original, 4);
  strcpy((char*)data + 5, "Alice: hello");
  const char* text = nullptr;
  ASSERT_TRUE(archive.extractText(PAYLOAD_TYPE_GRP_TXT, data, 17, timestamp, text));
  EXPECT_EQ(original, timestamp);
  EXPECT_STREQ("Alice: hello", text);
  EXPECT_FALSE(archive.extractText(PAYLOAD_TYPE_GRP_DATA, data, 17, timestamp, text));
  data[4] = 1 << 2;
  EXPECT_FALSE(archive.extractText(PAYLOAD_TYPE_GRP_TXT, data, 17, timestamp, text));
  data[4] = 0; data[5] = 0;
  EXPECT_FALSE(archive.extractText(PAYLOAD_TYPE_GRP_TXT, data, 17, timestamp, text));
}
TEST(PublicArchive, RejectsMalformedLengthsAndInvalidChannelSearchInputs) {
  PublicChannelArchive archive;
  uint8_t data[MAX_PACKET_PAYLOAD + 1] = {};
  uint32_t timestamp; const char* text;
  EXPECT_FALSE(archive.extractText(PAYLOAD_TYPE_GRP_TXT, nullptr, 6, timestamp, text));
  EXPECT_FALSE(archive.extractText(PAYLOAD_TYPE_GRP_TXT, data, 5, timestamp, text));
  EXPECT_FALSE(archive.extractText(PAYLOAD_TYPE_GRP_TXT, data, MAX_PACKET_PAYLOAD, timestamp, text));
  mesh::GroupChannel channels[1]; uint8_t hash[1] = {};
  EXPECT_EQ(0, archive.find(nullptr, channels, 1));
  EXPECT_EQ(0, archive.find(hash, nullptr, 1));
  EXPECT_EQ(0, archive.find(hash, channels, 0));
}
int main(int argc,char**argv) { ::testing::InitGoogleTest(&argc,argv); return RUN_ALL_TESTS(); }
