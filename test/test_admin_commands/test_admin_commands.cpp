#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/AdminCommands.h"

TEST(AdminCommands, DoesNotTreatErrorAsFetchedValueOrSuccess) {
  EXPECT_EQ(zen::admin::value("Error: unsupported"), nullptr);
  EXPECT_EQ(zen::admin::value("OK"), nullptr);
  EXPECT_STREQ(zen::admin::value("> name"), "name");
  EXPECT_TRUE(zen::admin::confirmed("OK"));
  EXPECT_TRUE(zen::admin::confirmed("OK - Advert sent"));
  EXPECT_FALSE(zen::admin::confirmed("Error"));
  EXPECT_FALSE(zen::admin::confirmed("Sent"));
}

TEST(AdminCommands, AcceptsRawAndPrefixedReadOnlyReplies) {
  EXPECT_STREQ(zen::admin::readValue("A1B2C3D4:12:-7"),
               "A1B2C3D4:12:-7");
  EXPECT_STREQ(zen::admin::readValue("> MeshCore 1.17.1"),
               "MeshCore 1.17.1");
}

TEST(AdminCommands, FormatsNeighbourAgeAndSignalForDisplay) {
  char text[32];
  EXPECT_TRUE(zen::formatQuarterDb(text, sizeof(text), 45));
  EXPECT_STREQ(text, "11.3");
  EXPECT_TRUE(zen::formatQuarterDb(text, sizeof(text), -7));
  EXPECT_STREQ(text, "-1.8");
  EXPECT_TRUE(zen::admin::formatNeighbourMetrics(text, sizeof(text), 2, -7));
  EXPECT_STREQ(text, "now -1.8");
  EXPECT_TRUE(zen::admin::formatNeighbourMetrics(text, sizeof(text), 45, 3));
  EXPECT_STREQ(text, "45s 0.8");
  EXPECT_TRUE(zen::admin::formatNeighbourMetrics(text, sizeof(text), 12 * 60, 45));
  EXPECT_STREQ(text, "12m 11.3");
  EXPECT_TRUE(zen::admin::formatNeighbourMetrics(text, sizeof(text), 3 * 3600, 48));
  EXPECT_STREQ(text, "3h 12.0");
  EXPECT_TRUE(zen::admin::formatNeighbourMetrics(text, sizeof(text), 2 * 86400, -2));
  EXPECT_STREQ(text, "2d -0.5");
  EXPECT_FALSE(zen::admin::formatNeighbourMetrics(text, 5, 45, -7));

  EXPECT_TRUE(zen::admin::formatNeighbourLine(
      text, sizeof(text), "Best Repeater", 13, 12 * 60, 45));
  EXPECT_STREQ(text, "Best Repea 12m 11.3");
  EXPECT_EQ(strlen(text), 19u);
  EXPECT_TRUE(zen::admin::formatNeighbourLine(
      text, sizeof(text), "RPT", 3, 12 * 60, 45));
  EXPECT_STREQ(text, "RPT        12m 11.3");
  EXPECT_EQ(strlen(text), 19u);
}

TEST(AdminCommands, RejectsMalformedOrOutOfRangeNumbers) {
  float n = 4;
  EXPECT_FALSE(zen::admin::number("Error", 0, 20, n));
  EXPECT_FALSE(zen::admin::number("nan", 0, 20, n));
  EXPECT_FALSE(zen::admin::number("10junk", 0, 20, n));
  EXPECT_FALSE(zen::admin::number("21", 0, 20, n));
  EXPECT_EQ(n, 4);
  EXPECT_TRUE(zen::admin::number("10.5", 0, 20, n));
  EXPECT_FLOAT_EQ(n, 10.5f);
  EXPECT_TRUE(zen::admin::number("50.0%", 1, 100, n));
  EXPECT_FLOAT_EQ(n, 50.0f);
}

TEST(AdminCommands, ValidatesRadioTupleAndPreservesOtherFields) {
  float freq = 0, bw = 0;
  uint8_t sf = 0, cr = 0;
  EXPECT_FALSE(zen::admin::parseRadio("915,250,99,5", freq, bw, sf, cr));
  EXPECT_FALSE(zen::admin::parseRadio("915,250,10,5junk", freq, bw, sf, cr));
  ASSERT_TRUE(zen::admin::parseRadio("915,62.500,8,5", freq, bw, sf, cr));
  char value[60], cmd[80];
  EXPECT_TRUE(zen::admin::formatRadio(value, sizeof(value), freq + 1, bw, sf, cr));
  EXPECT_TRUE(zen::admin::formatCommand(cmd, sizeof(cmd), "set radio", value));
  EXPECT_STREQ(cmd, "set radio 916.000,62.500,8,5");
  EXPECT_FALSE(zen::admin::formatCommand(cmd, 6, "set name", "too long"));
}

TEST(AdminCommands, AdvertStepsRespectBaselineDisabledAndMinimumIntervals) {
  for (const auto& field : zen::admin::FIELDS) {
    if (field.get && !strcmp(field.get, "get advert.interval")) {
      EXPECT_EQ(zen::admin::stepNumber(field, 0, 1), 60);
      EXPECT_EQ(zen::admin::stepNumber(field, 60, -1), 0);
      EXPECT_EQ(zen::admin::stepNumber(field, 60, 1), 62);
    } else if (field.get && !strcmp(field.get, "get flood.advert.interval")) {
      EXPECT_EQ(zen::admin::stepNumber(field, 0, 1), 3);
      EXPECT_EQ(zen::admin::stepNumber(field, 3, -1), 0);
    }
  }
}

TEST(AdminCommands, ExposesDedicatedOtaAction) {
  const zen::admin::Field* ota = nullptr;
  for (const auto& field : zen::admin::FIELDS) {
    if (field.get && !strcmp(field.get, "start ota")) ota = &field;
  }
  ASSERT_NE(ota, nullptr);
  EXPECT_EQ(ota->group, zen::admin::ACTIONS);
  EXPECT_EQ(ota->kind, zen::admin::ACTION);
  EXPECT_STREQ(ota->label, "Start OTA");
}

TEST(AdminCommands, FiltersGroupsByRemoteNodeType) {
  using namespace zen::admin;
  EXPECT_EQ(groupCount(TARGET_REPEATER), 6);
  EXPECT_EQ(groupCount(TARGET_ROOM), 7);
  EXPECT_EQ(groupCount(TARGET_SENSOR), 6);
  EXPECT_TRUE(groupAllowed(TARGET_REPEATER, ROUTING));
  EXPECT_FALSE(groupAllowed(TARGET_REPEATER, ROOM));
  EXPECT_TRUE(groupAllowed(TARGET_ROOM, ROOM));
  EXPECT_TRUE(groupAllowed(TARGET_ROOM, ROUTING));
  EXPECT_FALSE(groupAllowed(TARGET_SENSOR, ROOM));
  EXPECT_TRUE(groupAllowed(TARGET_SENSOR, ROUTING));
  EXPECT_EQ(groupAt(TARGET_SENSOR, 0), STATUS);
  EXPECT_EQ(groupAt(TARGET_SENSOR, 4), CONSOLE);
  EXPECT_EQ(groupAt(TARGET_SENSOR, 5), ACTIONS);
}

TEST(AdminCommands, ExposesSupportedRoutingAndWriteOnlyPasswordFields) {
  using namespace zen::admin;
  bool tx_delay = false, direct_delay = false, password = false;
  for (const auto& field : FIELDS) {
    if (field.get && !strcmp(field.get, "get txdelay"))
      tx_delay = field.group == ROUTING && !strcmp(field.label, "TX delay");
    if (field.get && !strcmp(field.get, "get direct.txdelay"))
      direct_delay = field.group == ROUTING;
    if (field.set && !strcmp(field.set, "password"))
      password = field.kind == WRITE_TEXT && field.get == nullptr;
  }
  EXPECT_TRUE(tx_delay);
  EXPECT_TRUE(direct_delay);
  EXPECT_TRUE(password);
}

TEST(AdminCommands, VerifiesReadBackUsingTheFieldType) {
  using namespace zen::admin;
  const Field number_field = {ROUTING, "TX delay", "get txdelay", "set txdelay",
                              NUMBER, 0, 2, 0.1f};
  const Field toggle_field = {ROUTING, "CAD", "get cad", "set cad",
                              TOGGLE, 0, 1, 1};
  const Field radio_field = {RADIO, "Frequency", "get radio", "set radio",
                             FREQUENCY, 150, 2500, 0.001f};
  EXPECT_TRUE(valuesEqual(number_field, "0.5", "0.5"));
  EXPECT_TRUE(valuesEqual(number_field, "0.5", "0.500"));
  EXPECT_FALSE(valuesEqual(number_field, "0.5", "0.6"));
  EXPECT_TRUE(valuesEqual(toggle_field, "on", "on"));
  EXPECT_FALSE(valuesEqual(toggle_field, "on", "off"));
  EXPECT_TRUE(valuesEqual(radio_field, "915.000,62.500,8,5", "915,62.5,8,5"));
  EXPECT_FALSE(valuesEqual(radio_field, "915.000,62.500,8,5", "916,62.5,8,5"));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
