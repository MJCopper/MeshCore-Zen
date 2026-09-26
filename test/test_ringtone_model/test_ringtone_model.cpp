#include <gtest/gtest.h>
#include <cstring>

#include "../../examples/companion_radio/zen-overlay/app/zen/RingtoneModel.h"

TEST(RingtoneModel, PreservesLegacyNaturalNoteEncoding) {
  uint8_t legacy_c5_eighth = 1 | (1 << 3) | (1 << 5);
  EXPECT_EQ(zen::RingtoneModel::pitchIndex(legacy_c5_eighth), 1);
  EXPECT_EQ(zen::RingtoneModel::octave(legacy_c5_eighth), 5);
  EXPECT_EQ(zen::RingtoneModel::duration(legacy_c5_eighth), 1);
}

TEST(RingtoneModel, RoundTripsEveryChromaticPitch) {
  for (uint8_t pitch = 0; pitch < zen::RingtoneModel::PITCH_COUNT; pitch++) {
    uint8_t note = zen::RingtoneModel::pack(pitch, 6, 2);
    EXPECT_EQ(zen::RingtoneModel::pitchIndex(note), pitch);
    EXPECT_EQ(zen::RingtoneModel::octave(note), 6);
    EXPECT_EQ(zen::RingtoneModel::duration(note), 2);
  }
  char label[5];
  zen::RingtoneModel::label(zen::RingtoneModel::pack(2, 5, 1), label, sizeof(label));
  EXPECT_STREQ(label, "C#5");
}

TEST(RingtoneModel, ClampsInvalidPackInputs) {
  uint8_t low = zen::RingtoneModel::pack(1, 1, 9);
  uint8_t high = zen::RingtoneModel::pack(1, 9, 0);
  EXPECT_EQ(zen::RingtoneModel::octave(low), 4);
  EXPECT_EQ(zen::RingtoneModel::duration(low), 0);
  EXPECT_EQ(zen::RingtoneModel::octave(high), 7);
}

TEST(RingtoneModel, EmitsSharpsAndLimitsLegacyMelodiesToSixteenNotes) {
  uint8_t notes[zen::RingtoneModel::STORAGE_NOTES];
  for (uint8_t i = 0; i < zen::RingtoneModel::STORAGE_NOTES; i++) {
    notes[i] = zen::RingtoneModel::pack(2, 5, 1);
  }
  char rtttl[220];
  zen::RingtoneModel::buildRTTTL(notes, sizeof(notes), 2, rtttl, sizeof(rtttl));
  EXPECT_NE(std::strstr(rtttl, "8c#5"), nullptr);
  const char* melody = std::strchr(rtttl, ':');
  ASSERT_NE(melody, nullptr);
  melody = std::strchr(melody + 1, ':');
  ASSERT_NE(melody, nullptr);
  int separators = 0;
  for (const char* p = melody; *p; p++) if (*p == ',') separators++;
  EXPECT_EQ(separators, 15);
}

TEST(RingtoneModel, EmptyMelodyProducesEmptyRtttl) {
  char rtttl[16] = "unchanged";
  zen::RingtoneModel::buildRTTTL(nullptr, 0, 2, rtttl, sizeof(rtttl));
  EXPECT_STREQ(rtttl, "");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
