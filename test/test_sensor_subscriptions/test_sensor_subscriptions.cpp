#include <gtest/gtest.h>
#include "../../examples/simple_sensor/TelemetrySubscription.h"
TEST(SensorSubscriptions, HumidityKeepsOneByteEncodingAndHalfPercentUnits) {
  EXPECT_EQ(1, SensorLPP::getDataSize(LPP_RELATIVE_HUMIDITY));
  const uint8_t value[] = {101};
  EXPECT_FLOAT_EQ(50.5f, SensorLPP::getFloat(value, 1, SensorLPP::getMultiplier(LPP_RELATIVE_HUMIDITY), false));
  const uint8_t deltas[] = {2, LPP_RELATIVE_HUMIDITY, 4, 2, LPP_TEMPERATURE, 0, 10};
  EXPECT_TRUE(SensorLPP::validDeltas(deltas, sizeof(deltas)));
}
TEST(SensorSubscriptions, RejectsTruncatedAndNonScalarThresholds) {
  const uint8_t temp[] = {2, LPP_TEMPERATURE, 0, 10};
  for (size_t n = 0; n < sizeof(temp); n++) EXPECT_FALSE(SensorLPP::validDeltas(temp, n));
  EXPECT_TRUE(SensorLPP::validDeltas(temp, sizeof(temp)));
  const uint8_t bad[] = {2, LPP_GPS, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  EXPECT_FALSE(SensorLPP::validDeltas(bad, sizeof(bad)));
  const uint8_t negative[] = {2, LPP_TEMPERATURE, 255, 255};
  EXPECT_FALSE(SensorLPP::validDeltas(negative, sizeof(negative)));
}
int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
