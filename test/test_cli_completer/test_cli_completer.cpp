#include <gtest/gtest.h>
#include <set>
#include <string>
#include "../../examples/companion_radio/zen-overlay/app/zen/CliCompleter.h"

static std::string digits(const char* word) {
  std::string out;
  while (*word) {
    char digit = zen::T9Predictor::digitFor(*word++);
    if (digit) out += digit;
  }
  return out;
}

TEST(CliCompleter, PrioritisesCommandsOverRecentlyUsedChat) {
  char out[8][zen::WordCompleter::MAX_WORD_LEN];
  zen::T9Predictor::remember("pet");
  ASSERT_GT(zen::CliCompleter::suggestT9("738", 3, out, 8), 0);
  EXPECT_STREQ(out[0], "set");
  auto sequence = digits("radio");
  ASSERT_GT(zen::CliCompleter::suggestT9(sequence.c_str(), sequence.size(), out, 8), 0);
  EXPECT_STREQ(out[0], "radio");
}

TEST(CliCompleter, FallsBackToChatWithoutDuplicatingCommands) {
  char out[8][zen::WordCompleter::MAX_WORD_LEN];
  auto sequence = digits("hello");
  ASSERT_GT(zen::CliCompleter::suggestT9(sequence.c_str(), sequence.size(), out, 8), 0);
  EXPECT_STREQ(out[0], "hello");
  uint8_t n = zen::CliCompleter::suggestT9("438", 3, out, 8);
  std::set<std::string> unique;
  for (uint8_t i = 0; i < n; i++) EXPECT_TRUE(unique.insert(out[i]).second);
}

TEST(CliCompleter, PreservesDottedKeysAndRespectsByteCapacity) {
  char out[8][zen::WordCompleter::MAX_WORD_LEN];
  auto sequence = digits("flood.advert.interval");
  ASSERT_GT(zen::CliCompleter::suggestT9(sequence.c_str(), sequence.size(), out, 8), 0);
  EXPECT_STREQ(out[0], "flood.advert.interval");
  EXPECT_EQ(zen::CliCompleter::suggestT9(sequence.c_str(), sequence.size(), out, 8, 10), 0);
  ASSERT_GT(zen::CliCompleter::suggest("flood.ad", 8, out, 8), 0);
  EXPECT_STREQ(out[0], "flood.advert.interval");
}

TEST(CliCompleter, TokenRangeIncludesDotsButNotNeighbouringArguments) {
  const char* text = "set flood.advert.interval 3";
  auto range = zen::CliCompleter::currentToken(text, strlen(text), 12);
  EXPECT_EQ(range.start, 4u);
  EXPECT_EQ(range.end, 25u);
}

TEST(CliCompleter, PrefixCompletionPrefersCliAndRetainsChatFallback) {
  char out[8][zen::WordCompleter::MAX_WORD_LEN];
  ASSERT_GT(zen::CliCompleter::suggest("ra", 2, out, 8), 0);
  EXPECT_STREQ(out[0], "radio");
  uint8_t n = zen::CliCompleter::suggest("hell", 4, out, 8);
  bool hello = false;
  for (uint8_t i = 0; i < n; i++) if (!strcmp(out[i], "hello")) hello = true;
  EXPECT_TRUE(hello);
  EXPECT_EQ(zen::CliCompleter::suggest("", 0, out, 8), 0);
}

TEST(CliCompleter, DictionaryIsUniqueAndFitsEveryCompletionBuffer) {
  std::set<std::string> unique;
  EXPECT_GT(zen::CliCompleter::dictionarySize(), 80u);
  for (size_t i = 0; i < zen::CliCompleter::dictionarySize(); i++) {
    const char* word = zen::CliCompleter::wordAt(i);
    EXPECT_LT(strlen(word), zen::WordCompleter::MAX_WORD_LEN);
    EXPECT_TRUE(unique.insert(word).second) << word;
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
