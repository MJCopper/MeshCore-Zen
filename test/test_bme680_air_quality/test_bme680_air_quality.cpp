#include <gtest/gtest.h>
#include <limits>
#include "../../examples/bme680_sensor/BME680AirQuality.h"

TEST(BME680AirQuality, RequiresTenValidSamplesAndResetsAtBoot) {
  BME680AirQuality quality;
  float score = 0;
  for (int i = 0; i < 9; i++) EXPECT_FALSE(quality.update(100000, 40, score));
  EXPECT_TRUE(quality.update(100000, 40, score));
  EXPECT_FLOAT_EQ(0, score);
  BME680AirQuality rebooted;
  EXPECT_FALSE(rebooted.update(100000, 40, score));
}

TEST(BME680AirQuality, InvalidSamplesDoNotAdvanceWarmup) {
  BME680AirQuality quality;
  float score = 0;
  for (int i = 0; i < 20; i++) {
    EXPECT_FALSE(quality.update(0, 40, score));
    EXPECT_FALSE(quality.update(100000, -1, score));
    EXPECT_FALSE(quality.update(100000, 101, score));
  }
  for (int i = 0; i < 9; i++) EXPECT_FALSE(quality.update(100000, 40, score));
  EXPECT_TRUE(quality.update(100000, 40, score));
}

TEST(BME680AirQuality, DetectsFallingGasResistanceAndBoundsScore) {
  BME680AirQuality quality;
  float score = 0;
  for (int i = 0; i < 10; i++) quality.update(100000, 40, score);
  EXPECT_TRUE(quality.update(10000, 40, score));
  EXPECT_GT(score, 300);
  EXPECT_LE(score, 500);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
