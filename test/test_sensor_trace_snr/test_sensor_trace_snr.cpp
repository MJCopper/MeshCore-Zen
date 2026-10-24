#include "../sensor_mocks/SensorIntegrationFixture.h"
#include <tuple>

class SensorTraceSnr : public SensorIntegration {
protected:
  void expectReport(const char* expected, size_t index = 1, uint32_t now = 4000) {
    tick(now);
    ASSERT_GT(radio.sent.size(), index);
    EXPECT_EQ(expected, publicText(index));
    finish(now + 100);
    EXPECT_EQ(32, pool.getFreeCount());
  }
};

TEST_F(SensorTraceSnr, ListsEachNodeOnceWithItsPrecedingLinkAverage) {
  radio.incoming = command("Alice: !hillvue trace", 3);
  tick(100); finish(200);
  // S -> R1 -> R2 -> R3 -> R2 -> R1 -> S:
  // -6, +2, -10, +2, 0, -2 dB. The return sample for R1 is sensor RX.
  traceReply(0, 1100, {-24, 8, -40, 8, 0, -8});
  expectReport("BME680 Sensor: Trace: 1000 ms RTT (avg SNR)\n"
               "1 16 -4.0 dB\n2 15 +1.0 dB\n3 14 -4.0 dB");
  const auto& selected = bot.diagnostics().selected;
  EXPECT_EQ((uint8_t)(int8_t)-24, selected.snrs[0]);
  EXPECT_EQ((uint8_t)(int8_t)-8, selected.snrs[5]);
}

TEST_F(SensorTraceSnr, OneRepeaterIncludesSensorReceiveSampleAndSignedExtremes) {
  radio.incoming = command("Alice: !hillvue trace", 1);
  tick(100); finish(200);
  traceReply(0, 1100, {-128, 127});
  // Sum is -1 quarter-dB: -0.125 dB before one-decimal formatting.
  expectReport("BME680 Sensor: Trace: 1000 ms RTT (avg SNR)\n1 14 -0.1 dB");
}

TEST_F(SensorTraceSnr, SameSignExtremesCannotOverflowBeforeAveraging) {
  radio.incoming = command("Alice: !hillvue trace", 2);
  tick(100); finish(200);
  traceReply(0, 1100, {-128, 127, 127, -128});
  expectReport("BME680 Sensor: Trace: 1000 ms RTT (avg SNR)\n"
               "1 15 -32.0 dB\n2 14 +31.8 dB");
}

TEST_F(SensorTraceSnr, DoesNotTruncateOrRoundSamplesBeforeAveraging) {
  radio.incoming = command("Alice: !hillvue trace", 3);
  tick(100); finish(200);
  traceReply(0, 1100, {1, -1, -1, 0, 1, 0});
  expectReport("BME680 Sensor: Trace: 1000 ms RTT (avg SNR)\n"
               "1 16 +0.1 dB\n2 15 +0.0 dB\n3 14 -0.1 dB");
}

TEST_F(SensorTraceSnr, RetryUsesItsOwnSamplesInsteadOfStoredFirstAttempt) {
  radio.incoming = command("Alice: !hillvue trace", 2);
  tick(100); finish(200); tick(10100); tick(12200); finish(12300);
  traceReply(0, 13000, {-24, -32, -16, -8});
  EXPECT_FALSE(bot.diagnostics().has_result);
  traceReply(1, 14000, {8, -8, 24, 16});
  expectReport("BME680 Sensor: Trace: 1800 ms RTT (avg SNR)\n"
               "1 15 +3.0 dB\n2 14 +2.0 dB", 2, 18000);
  EXPECT_EQ(1, bot.diagnostics().selected.attempt);
}

TEST_F(SensorTraceSnr, FallbackUsesOriginalRouteAndBothOriginalDirections) {
  auto bytes = command("Alice: !hillvue trace", 2);
  radio.incoming = bytes;
  tick(100); finish(200);
  bytes[2] = 40; bytes[3] = 41;
  radio.incoming = bytes;
  tick(300); tick(10100); tick(12200); finish(12300);
  traceReply(0, 13000, {-24, -32, -16, -8});
  tick(22200);
  expectReport("BME680 Sensor: Trace: 12900 ms RTT (avg SNR)\n"
               "1 15 -4.0 dB\n2 14 -6.0 dB", 2, 26000);
  EXPECT_EQ(0, bot.diagnostics().selected.attempt);
}

class EveryTraceSnrRoute : public SensorTraceSnr,
                           public ::testing::WithParamInterface<std::tuple<int, int>> {};

TEST_P(EveryTraceSnrRoute, PairsAllLinksAndPreservesOutwardOrderAcrossReportParts) {
  const auto width = std::get<0>(GetParam());
  const auto hops = std::get<1>(GetParam());
  radio.incoming = command("Alice: !hillvue trace", hops, nullptr, width);
  tick(100); finish(200);
  std::vector<int8_t> samples(2 * hops);
  // Independent per-link test inputs: outward -16+i dB, return +8-i/2 dB.
  // Expected average -4+i/4 dB. Every reverse-pair index matters.
  for (int i = 0; i < hops; i++) {
    samples[i] = -64 + 4 * i;
    samples[2 * hops - 1 - i] = 32 - 2 * i;
  }
  traceReply(0, 1100, samples);
  tick(4000); finish(4100);
  ASSERT_GE(radio.sent.size(), 2u);
  std::string text = publicText(1);
  if (text.find("1/2\n") != std::string::npos) {
    tick(7100); tick(7101); finish(7200);
    ASSERT_EQ(3u, radio.sent.size());
    text += "\n" + publicText(2);
  } else EXPECT_EQ(2u, radio.sent.size());
  EXPECT_NE(std::string::npos, text.find("Trace: 1000 ms RTT (avg SNR)"));
  EXPECT_EQ(std::string::npos, text.find("truncated"));
  size_t previous = 0;
  for (int i = 0; i < hops; i++) {
    char label[5]{};
    for (int b = 0; b < width; b++)
      snprintf(label + 2 * b, sizeof(label) - 2 * b, "%02X", 20 + (hops - 1 - i) * width + b);
    char line[40];
    snprintf(line, sizeof(line), "%d %s %+.1f dB", i + 1, label, -4.0 + i / 4.0);
    auto position = text.find(line, previous);
    ASSERT_NE(std::string::npos, position) << line << "\n" << text;
    EXPECT_EQ(std::string::npos, text.find(line, position + strlen(line)));
    previous = position + strlen(line);
  }
  EXPECT_EQ(32, pool.getFreeCount());
}

INSTANTIATE_TEST_SUITE_P(OneThroughTenRepeatersBothWidths, EveryTraceSnrRoute,
    ::testing::Combine(::testing::Values(1, 2), ::testing::Range(1, 11)));

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
