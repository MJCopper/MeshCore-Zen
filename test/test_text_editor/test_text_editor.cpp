#define FIRMWARE_ZEN_BUILD 1

#include <gtest/gtest.h>

#include "../../examples/companion_radio/zen-overlay/app/zen/TextBuffer.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/TextEditorTypes.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/SuggestionModel.h"
#include "../../examples/companion_radio/zen-overlay/app/zen/MessageComposeSession.h"
#include "../../examples/companion_radio/zen-overlay/app/ui-new/KeyboardLayout.h"
#include "../../examples/companion_radio/zen-overlay/app/ui-new/KeyboardInputActions.h"

TEST(TextBuffer, InsertsReplacesAndMovesByUtf8Codepoint) {
  zen::TextBuffer<16> text;
  text.reset("ab", 16);
  text.movePrevious();
  EXPECT_TRUE(text.insert("\xF0\x9F\x99\x82"));
  EXPECT_STREQ("a\xF0\x9F\x99\x82" "b", text.buf);
  EXPECT_EQ(5, text.cursor_pos);
  EXPECT_TRUE(text.erasePrevious());
  EXPECT_STREQ("ab", text.buf);
  EXPECT_EQ(1, text.cursor_pos);
  EXPECT_TRUE(text.replace(0, 1, "Hi"));
  EXPECT_STREQ("Hib", text.buf);
}

TEST(TextBuffer, EnforcesByteCapacityAndTermination) {
  zen::TextBuffer<5> text;
  text.reset("abcdef", 5);
  EXPECT_STREQ("abcde", text.buf);
  EXPECT_EQ(5, text.len);
  EXPECT_FALSE(text.insert("x"));
  EXPECT_EQ('\0', text.buf[text.len]);
}

TEST(TextEditorProfiles, KeepCredentialsLiteralAndMessagesPredictive) {
  zen::EditorFeatures credential = zen::featuresFor(zen::EditorProfile::CREDENTIAL);
  EXPECT_FALSE(credential.sentence_case);
  EXPECT_FALSE(credential.emoji);
  EXPECT_FALSE(credential.predictive_t9);
  EXPECT_FALSE(credential.context_prediction);

  zen::EditorFeatures message = zen::featuresFor(zen::EditorProfile::MESSAGE);
  EXPECT_TRUE(message.sentence_case);
  EXPECT_TRUE(message.emoji);
  EXPECT_TRUE(message.predictive_t9);
  EXPECT_TRUE(message.context_prediction);
}

TEST(SuggestionModel, BoundsItemsAndReplacementRange) {
  zen::SuggestionModel<2, 8> model;
  model.reset(4);
  EXPECT_TRUE(model.add("hello"));
  EXPECT_TRUE(model.add("world"));
  EXPECT_FALSE(model.add("extra"));
  model.setRange(-2, 20, 6);
  EXPECT_EQ(0, model.replace_start);
  EXPECT_EQ(6, model.replace_end);
}

TEST(MessageComposeSession, KeepsDraftsScopedToSelectedConversation) {
  zen::MessageDraftStore drafts;
  zen::MessageComposeSession compose(drafts);
  uint8_t alice[4] = { 1, 2, 3, 4 };
  char restored[32] = {};
  uint8_t prefix = 0;

  compose.selectContact(alice);
  compose.save("@Alice hello", 7);
  compose.selectChannel(2);
  EXPECT_FALSE(compose.restore(restored, sizeof(restored), &prefix));
  compose.save("channel draft");

  compose.selectContact(alice);
  EXPECT_TRUE(compose.restore(restored, sizeof(restored), &prefix));
  EXPECT_STREQ("@Alice hello", restored);
  EXPECT_EQ(7, prefix);
  compose.clear();
  EXPECT_FALSE(compose.restore(restored, sizeof(restored)));
}

TEST(KeyboardLayout, FitsFullAndCompactEditorsOnBothDisplays) {
  const int widths[] = { 128, 250 };
  const int heights[] = { 64, 122 };
  for (int i = 0; i < 2; i++) {
    KeyboardLayout full = KeyboardLayout::calculate(
        widths[i], heights[i], 1, 8, 6, 3, 3, 6, false, false);
    EXPECT_GE(full.preview_lines, 1);
    EXPECT_GE(full.characters_y, 0);
    EXPECT_LE(full.special_y + full.cell_height, heights[i]);

    KeyboardLayout compact = KeyboardLayout::calculate(
        widths[i], heights[i], 1, 8, 6, 3, 3, 6, true, false);
    EXPECT_GT(compact.preview_lines, full.preview_lines);
    EXPECT_LE(compact.characters_y + compact.cell_height * 4, heights[i]);
  }
}

TEST(KeyboardInputActions, NormalizesNavigationAndEditorCommands) {
  EXPECT_EQ(zen::EditorAction::NAV_LEFT, keyboardinput::actionForKey(KEY_LEFT));
  EXPECT_EQ(zen::EditorAction::HOLD_ACTIVATE,
            keyboardinput::actionForKey(KEY_CONTEXT_MENU));
  EXPECT_EQ(zen::EditorAction::DOUBLE_CANCEL,
            keyboardinput::actionForKey(KEY_DOUBLE_CANCEL));
  EXPECT_EQ(zen::EditorAction::SUBMIT,
            keyboardinput::actionForKey(KEY_KB_ENTER));
  EXPECT_EQ(zen::EditorAction::NONE, keyboardinput::actionForKey('a'));
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
