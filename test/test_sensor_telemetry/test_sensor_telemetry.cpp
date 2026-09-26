#include <gtest/gtest.h>
#include "../../examples/companion_radio/zen-overlay/app/zen/SensorTelemetry.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(SensorTelemetry, DescendingChannelsStableWithinChannel) {
  const uint8_t data[] = { 1, LPP_VOLTAGE, 1, 144,
    3, LPP_TEMPERATURE, 0, 225, 2, LPP_PERCENTAGE, 80,
    3, LPP_RELATIVE_HUMIDITY, 101 };
  zen::SensorTelemetry telemetry;
  telemetry.load(data, sizeof(data));
  ASSERT_EQ(telemetry.rows(), 4);
  const char* expected[] = { "Temp: 22.5 C", "Humidity: 50.5%", "Percent: 80%", "Voltage: 4.00 V" };
  char out[64];
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(telemetry.format(i, out, sizeof(out)));
    EXPECT_STREQ(out, expected[i]);
  }
}

TEST(SensorTelemetry, SignedValuesAndComponents) {
  const uint8_t data[] = { 2, LPP_TEMPERATURE, 255, 246,
    2, LPP_ACCELEROMETER, 3, 232, 252, 24, 0, 0 };
  zen::SensorTelemetry telemetry;
  telemetry.load(data, sizeof(data));
  ASSERT_EQ(telemetry.rows(), 4);
  const char* expected[] = { "Temp: -1.0 C", "Accel X: 1.000 g", "Accel Y: -1.000 g", "Accel Z: 0.000 g" };
  char out[64];
  for (int i = 0; i < 4; i++) {
    ASSERT_TRUE(telemetry.format(i, out, sizeof(out)));
    EXPECT_STREQ(out, expected[i]);
  }
}

TEST(SensorTelemetry, RejectsTruncatedAndUnknownRecords) {
  zen::SensorTelemetry telemetry;
  const uint8_t data[] = { 1, LPP_PERCENTAGE, 50, 2, LPP_GPS, 0 };
  telemetry.load(data, sizeof(data));
  EXPECT_TRUE(telemetry.invalid());
  EXPECT_EQ(telemetry.rows(), 1);
  const uint8_t unknown[] = { 1, 255, 0, 3, LPP_PERCENTAGE, 50 };
  telemetry.load(unknown, sizeof(unknown));
  EXPECT_TRUE(telemetry.invalid());
  EXPECT_EQ(telemetry.rows(), 0);
  telemetry.load(nullptr, 0);
  EXPECT_FALSE(telemetry.invalid());
}

TEST(SensorTelemetry, MaximumPacketAndOutputBounds) {
  uint8_t data[255];
  for (int i = 0; i < 85; i++) {
    data[i * 3] = i + 1; data[i * 3 + 1] = LPP_PERCENTAGE; data[i * 3 + 2] = i;
  }
  zen::SensorTelemetry telemetry;
  telemetry.load(data, sizeof(data));
  EXPECT_EQ(telemetry.rows(), 85);
  char out[64];
  ASSERT_TRUE(telemetry.format(0, out, sizeof(out)));
  EXPECT_STREQ(out, "Percent: 84%");
  EXPECT_FALSE(telemetry.format(85, out, sizeof(out)));
  char tiny[2] = {};
  EXPECT_TRUE(telemetry.format(0, tiny, sizeof(tiny)));
  EXPECT_EQ(tiny[1], 0);
}

TEST(SensorTelemetry, ReadsFirstScalarForMessageExpansion) {
  const uint8_t data[] = {
    1, LPP_TEMPERATURE, 0, 215,
    2, LPP_LUMINOSITY, 1, 244,
    3, LPP_DISTANCE, 0, 0, 4, 210,
    4, LPP_CONCENTRATION, 1, 144
  };
  zen::SensorTelemetry telemetry;
  telemetry.load(data, sizeof(data));
  float value = 0;
  ASSERT_TRUE(telemetry.firstValue(LPP_TEMPERATURE, value));
  EXPECT_FLOAT_EQ(value, 21.5f);
  ASSERT_TRUE(telemetry.firstValue(LPP_LUMINOSITY, value));
  EXPECT_FLOAT_EQ(value, 500.0f);
  ASSERT_TRUE(telemetry.firstValue(LPP_DISTANCE, value));
  EXPECT_FLOAT_EQ(value, 1.234f);
  ASSERT_TRUE(telemetry.firstValue(LPP_CONCENTRATION, value));
  EXPECT_FLOAT_EQ(value, 400.0f);
  EXPECT_FALSE(telemetry.firstValue(LPP_VOLTAGE, value));
}
