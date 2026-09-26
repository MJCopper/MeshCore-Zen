#include <gtest/gtest.h>
#include <cstring>
#include <set>
#include <string>

#include "../../examples/companion_radio/zen-overlay/app/zen/WordCompleter.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/T9Predictor.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/SentenceCase.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/ContextPredictor.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageDraftStore.h"

static_assert(zen::WordCompleter::MAX_SUGGESTIONS == 12,
              "Zen predictive menus expose twelve ranked suggestions");

TEST(ZenMessageDraftStore, KeepsDraftsSeparateByConversation) {
  zen::MessageDraftStore drafts;
  uint8_t alice[] = { 1, 2, 3, 4 };
  uint8_t bob[] = { 5, 6, 7, 8 };
  char text[zen::MessageDraftStore::TEXT_CAPACITY];
  drafts.saveContact(alice, "Hello Alice");
  drafts.saveContact(bob, "Hello Bob");
  drafts.saveChannel(2, "Hello channel");
  ASSERT_TRUE(drafts.loadContact(alice, text, sizeof(text)));
  EXPECT_STREQ("Hello Alice", text);
  ASSERT_TRUE(drafts.loadContact(bob, text, sizeof(text)));
  EXPECT_STREQ("Hello Bob", text);
  ASSERT_TRUE(drafts.loadChannel(2, text, sizeof(text)));
  EXPECT_STREQ("Hello channel", text);
}

TEST(ZenMessageDraftStore, EmptyTextClearsDraftAndReplyStateIsRestored) {
  zen::MessageDraftStore drafts;
  uint8_t alice[] = { 1, 2, 3, 4 };
  char text[zen::MessageDraftStore::TEXT_CAPACITY];
  uint8_t prefix_len = 0;
  drafts.saveContact(alice, "@[Alice] hello", 9);
  ASSERT_TRUE(drafts.loadContact(alice, text, sizeof(text), &prefix_len));
  EXPECT_EQ(9u, prefix_len);
  drafts.saveContact(alice, "");
  EXPECT_FALSE(drafts.loadContact(alice, text, sizeof(text)));
}

TEST(ZenSentenceCase, RecognisesMessageSentenceBoundaries) {
  EXPECT_TRUE(zen::SentenceCase::shouldCapitalize("", 0));
  EXPECT_FALSE(zen::SentenceCase::shouldCapitalize("Hello ", 6));
  EXPECT_TRUE(zen::SentenceCase::shouldCapitalize("Hello. ", 7));
  EXPECT_TRUE(zen::SentenceCase::shouldCapitalize("Really?!  ", 10));
  EXPECT_FALSE(zen::SentenceCase::shouldCapitalize("e.g. text", 9));
}

TEST(ZenSentenceCase, IgnoresReplyPrefixesAndDecoration) {
  const char* reply = "@Marek ";
  EXPECT_TRUE(zen::SentenceCase::shouldCapitalize(reply, strlen(reply)));
  const char* decorated = "\xF0\x9F\x98\x8A \"";
  EXPECT_TRUE(zen::SentenceCase::shouldCapitalize(decorated, strlen(decorated)));
  EXPECT_EQ('H', zen::SentenceCase::apply('h', reply, strlen(reply)));
}

TEST(ZenContextPredictor, PromotesLikelyPrefixCompletions) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestPrefix(
                "thank y", 6, "y", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
}

TEST(ZenContextPredictor, PromotesLikelyT9Words) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestT9(
                "how ", 4, "273", 3, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("are", matches[0]);
}

TEST(ZenContextPredictor, UsesSentenceRankingAtSentenceAndReplyBoundaries) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestPrefix(
                "thank. y", 7, "y", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
  memset(matches, 0, sizeof(matches));
  ASSERT_GT(zen::ContextPredictor::suggestPrefix(
                "@Marek y", 7, "y", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
}

TEST(ZenContextPredictor, UsesEightSuccessorsAcrossTwoThousandWords) {
  EXPECT_EQ(2000u, zen_context_data::ENTRY_COUNT);
  EXPECT_EQ(8u, zen_context_data::SUCCESSOR_COUNT);
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestPrefix(
                "how m", 4, "m", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("much", matches[0]);  // fifth ranked successor of "how"
}

TEST(ZenContextPredictor, TrigramsPrecedeBigramPredictions) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestPrefix(
                "how are y", 8, "y", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("you", matches[0]);
}

TEST(ZenContextPredictor, UsesContractionsAsContext) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestPrefix(
                "I'm g", 4, "g", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("going", matches[0]);
}

TEST(ZenContextPredictor, RanksSentenceOpenings) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::ContextPredictor::suggestT9(
                "", 0, "4", 1, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("I", matches[0]);
}

TEST(T9Predictor, ChoosesShorterWordWhenContractionCannotFit) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN]{};
  ASSERT_GT(zen::T9Predictor::suggest("46", 2, matches, 8, 2), 0);
  for (const auto& word : matches) EXPECT_LE(strlen(word), 2u);
  EXPECT_EQ(0, zen::T9Predictor::suggest("46", 2, matches, 8, 1));
  EXPECT_GT(zen::T9Predictor::suggest("46", 2, matches, 8), 0);
}

TEST(ZenWordCompleter, HonoursRequestedResultLimit) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = zen::WordCompleter::suggest("th", 2, matches, 3);
  ASSERT_EQ(3u, count);
  EXPECT_STREQ("the", matches[0]);
  EXPECT_STREQ("that", matches[1]);
  EXPECT_STREQ("this", matches[2]);
}

TEST(ZenWordCompleter, MatchesCaseInsensitivelyAndPreservesInitialCapital) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::WordCompleter::suggest("Plea", 4, matches, 3), 0u);
  EXPECT_STREQ("Please", matches[0]);
}

TEST(ZenWordCompleter, SkipsExactWordAndReturnsLongerCompletions) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = zen::WordCompleter::suggest("he", 2, matches,
                                                zen::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  for (uint8_t i = 0; i < count; i++) EXPECT_STRNE("he", matches[i]);
}

TEST(ZenWordCompleter, DictionaryHasExactlyFourThousandUniqueBoundedWords) {
  ASSERT_EQ(4000u, zen::WordCompleter::dictionarySize());
  std::set<std::string> unique;
  for (size_t i = 0; i < zen::WordCompleter::dictionarySize(); i++) {
    const char* word = zen::WordCompleter::wordAt(i);
    ASSERT_NE(nullptr, word);
    ASSERT_GT(std::strlen(word), 1u);
    ASSERT_LT(std::strlen(word), zen::WordCompleter::MAX_WORD_LEN);
    for (const char* p = word; *p; p++)
      EXPECT_TRUE((*p >= 'a' && *p <= 'z') || *p == '\'');
    EXPECT_TRUE(unique.insert(word).second) << word;
  }
}

TEST(ZenWordCompleter, UsesAustralianSpellingsAndEverydayTerms) {
  std::set<std::string> words;
  for (size_t i = 0; i < zen::WordCompleter::dictionarySize(); i++)
    words.insert(zen::WordCompleter::wordAt(i));

  const char* expected[] = {
    "mum", "colour", "favourite", "centre", "theatre", "neighbourhood",
    "realise", "apologise", "defence", "licence", "arvo", "brekkie",
    "servo", "mozzie", "footy", "bushwalk", "humour", "judgement",
    "aeroplane", "counsellor", "practising", "maths"
  };
  for (const char* word : expected) EXPECT_EQ(1u, words.count(word)) << word;

  const char* replaced[] = {
    "mom", "color", "favorite", "center", "theater", "neighborhood",
    "realize", "apologize", "defense", "humor", "judgment", "airplane",
    "counselor", "practicing", "math"
  };
  for (const char* word : replaced) EXPECT_EQ(0u, words.count(word)) << word;
}

TEST(ZenWordCompleter, IncludesPinnedLocalCommands) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS]
              [zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::WordCompleter::suggest(
                "hill", 4, matches,
                zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("hillvue", matches[0]);
}

TEST(ZenWordCompleter, FindsWholeWordAroundCursorAtPunctuation) {
  const char* text = "hello, schoo!";
  zen::WordCompleter::WordRange range = zen::WordCompleter::currentWord(
      text, std::strlen(text), 11);
  EXPECT_EQ(7u, range.start);
  EXPECT_EQ(12u, range.end);
}

TEST(ZenWordCompleter, EmptyPrefixDoesNotDumpDictionary) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  EXPECT_EQ(0u, zen::WordCompleter::suggest("", 0, matches, 3));
}

TEST(ZenT9Predictor, MapsClassicPhoneKeypadLetters) {
  EXPECT_EQ('2', zen::T9Predictor::digitFor('a'));
  EXPECT_EQ('7', zen::T9Predictor::digitFor('s'));
  EXPECT_EQ('9', zen::T9Predictor::digitFor('z'));
  EXPECT_EQ(0, zen::T9Predictor::digitFor('\''));
}

TEST(ZenT9Predictor, ProvidesCommonSingleLetterWords) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::T9Predictor::suggest("2", 1, matches,
                                       zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("a", matches[0]);
  memset(matches, 0, sizeof(matches));
  ASSERT_GT(zen::T9Predictor::suggest("4", 1, matches,
                                       zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("I", matches[0]);
}

TEST(ZenT9Predictor, RanksHelloForItsCompleteSequence) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = zen::T9Predictor::suggest("43556", 5, matches,
                                              zen::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  EXPECT_STREQ("hello", matches[0]);
}

TEST(ZenT9Predictor, ReturnsFrequencyRankedAlternatives) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = zen::T9Predictor::suggest("4663", 4, matches,
                                              zen::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GE(count, 2u);
  EXPECT_STREQ("good", matches[0]);
}

TEST(ZenT9Predictor, PrefersExactWordsBeforeLongerCompletions) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = zen::T9Predictor::suggest("43", 2, matches,
                                              zen::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  EXPECT_EQ(2u, zen::T9Predictor::digitLength(matches[0]));
}

TEST(ZenT9Predictor, MatchesContractionsWithoutAnApostropheKey) {
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  uint8_t count = zen::T9Predictor::suggest("3668", 4, matches,
                                              zen::WordCompleter::MAX_SUGGESTIONS);
  ASSERT_GT(count, 0u);
  bool found = false;
  for (uint8_t i = 0; i < count; i++) found |= std::strcmp(matches[i], "don't") == 0;
  EXPECT_TRUE(found);
}

TEST(ZenT9Predictor, FindsVisiblePrefixBeforeGhostCompletion) {
  EXPECT_EQ(2u, zen::T9Predictor::prefixBytes("hello", 2));
  EXPECT_EQ(3u, zen::T9Predictor::prefixBytes("I'm", 2));
}

TEST(ZenT9Predictor, RecentlyAcceptedWordsLeadTheirSequence) {
  zen::T9Predictor::remember("home");
  char matches[zen::WordCompleter::MAX_SUGGESTIONS][zen::WordCompleter::MAX_WORD_LEN] = {};
  ASSERT_GT(zen::T9Predictor::suggest("4663", 4, matches,
                                       zen::WordCompleter::MAX_SUGGESTIONS), 0u);
  EXPECT_STREQ("home", matches[0]);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
