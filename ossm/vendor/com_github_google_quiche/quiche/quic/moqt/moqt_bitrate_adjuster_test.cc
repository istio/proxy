// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_bitrate_adjuster.h"

#include "quiche/quic/core/quic_bandwidth.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {
namespace {

using ::quic::QuicBandwidth;
using ::quic::QuicTimeDelta;
using ::testing::_;

// Simple adjustable object that just keeps track of whatever value has been
// assigned to it, and has a mock method to notify of it changing.
class MockBitrateAdjustable : public BitrateAdjustable {
 public:
  explicit MockBitrateAdjustable(QuicBandwidth initial_bitrate)
      : bitrate_(initial_bitrate) {}

  quic::QuicBandwidth GetCurrentBitrate() const { return bitrate_; }
  bool CouldUseExtraBandwidth() override { return true; }
  void ConsiderAdjustingBitrate(QuicBandwidth bandwidth,
                                BitrateAdjustmentType /*type*/) override {
    bitrate_ = bandwidth;
    OnBitrateAdjusted(bandwidth);
  }

  MOCK_METHOD(void, OnBitrateAdjusted, (QuicBandwidth new_bitrate), ());

 private:
  QuicBandwidth bitrate_;
};

constexpr QuicBandwidth kDefaultBitrate =
    QuicBandwidth::FromBitsPerSecond(2000);
constexpr QuicTimeDelta kDefaultRtt = QuicTimeDelta::FromMilliseconds(20);
constexpr QuicTimeDelta kDefaultTimeScale = QuicTimeDelta::FromSeconds(1);

class MoqtBitrateAdjusterTest : public quiche::test::QuicheTest {
 protected:
  MoqtBitrateAdjusterTest()
      : adjustable_(kDefaultBitrate),
        adjuster_(&clock_, &session_, &adjustable_) {
    stats_.min_rtt = stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
    stats_.estimated_send_rate_bps = (1.2 * kDefaultBitrate).ToBitsPerSecond();
    ON_CALL(session_, GetSessionStats()).WillByDefault([this] {
      return stats_;
    });

    clock_.AdvanceTime(quic::QuicTimeDelta::FromSeconds(10));
    adjuster_.OnObjectAckSupportKnown(kDefaultTimeScale);
  }

  MockBitrateAdjustable adjustable_;
  webtransport::SessionStats stats_;
  quic::MockClock clock_;
  webtransport::test::MockSession session_;
  MoqtBitrateAdjuster adjuster_;
};

TEST_F(MoqtBitrateAdjusterTest, SteadyState) {
  // The fact that estimated bitrate is 1bps should not matter, since we never
  // have a reason to adjust down.
  stats_.estimated_send_rate_bps = 1;

  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_)).Times(0);
  for (int i = 0; i < 250; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    for (int j = 0; j < 10; ++j) {
      adjuster_.OnObjectAckReceived(i, j, kDefaultRtt * 2);
    }
  }
}

TEST_F(MoqtBitrateAdjusterTest, AdjustDownOnce) {
  stats_.estimated_send_rate_bps = (0.5 * kDefaultBitrate).ToBitsPerSecond();

  // First time will be skipped, since we aren't far enough into connection.
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_)).Times(0);
  adjuster_.OnObjectAckReceived(0, 0, QuicTimeDelta::FromMilliseconds(-1));

  clock_.AdvanceTime(100 * kDefaultRtt);
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_))
      .WillOnce([](QuicBandwidth new_bitrate) {
        EXPECT_LT(new_bitrate, kDefaultBitrate);
      });
  adjuster_.OnObjectAckReceived(0, 1, QuicTimeDelta::FromMilliseconds(-1));
}

TEST_F(MoqtBitrateAdjusterTest, AdjustDownTwice) {
  int adjusted_times = 0;
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_)).WillRepeatedly([&] {
    ++adjusted_times;
  });

  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.5 * kDefaultBitrate).ToBitsPerSecond();
  adjuster_.OnObjectAckReceived(0, 0, QuicTimeDelta::FromMilliseconds(-1));
  EXPECT_EQ(adjusted_times, 1);

  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.25 * kDefaultBitrate).ToBitsPerSecond();
  adjuster_.OnObjectAckReceived(0, 1, QuicTimeDelta::FromMilliseconds(-1));
  EXPECT_EQ(adjusted_times, 2);
}

TEST_F(MoqtBitrateAdjusterTest, ShouldIgnoreBitrateAdjustment) {
  constexpr quic::QuicBandwidth kOldBandwith =
      quic::QuicBandwidth::FromKBitsPerSecond(1024);
  constexpr float kMinChange = 0.01;
  EXPECT_FALSE(ShouldIgnoreBitrateAdjustment(kOldBandwith * 0.5,
                                             BitrateAdjustmentType::kDown,
                                             kOldBandwith, kMinChange));
  EXPECT_FALSE(ShouldIgnoreBitrateAdjustment(kOldBandwith * 1.5,
                                             BitrateAdjustmentType::kUp,
                                             kOldBandwith, kMinChange));

  // Always ignore change if new bandwidth is the old bandwidth.
  EXPECT_TRUE(ShouldIgnoreBitrateAdjustment(
      kOldBandwith, BitrateAdjustmentType::kUp, kOldBandwith, kMinChange));
  EXPECT_TRUE(ShouldIgnoreBitrateAdjustment(
      kOldBandwith, BitrateAdjustmentType::kDown, kOldBandwith, kMinChange));

  // Ignore very small changes to bitrate.
  const quic::QuicBandwidth kTinyDelta =
      quic::QuicBandwidth::FromBitsPerSecond(1);
  EXPECT_TRUE(ShouldIgnoreBitrateAdjustment(kOldBandwith - kTinyDelta,
                                            BitrateAdjustmentType::kDown,
                                            kOldBandwith, kMinChange));
  EXPECT_TRUE(ShouldIgnoreBitrateAdjustment(kOldBandwith + kTinyDelta,
                                            BitrateAdjustmentType::kUp,
                                            kOldBandwith, kMinChange));

  // Ignore if the direction of change stated by the bitrate adjuster is
  // different from the actual direction suggested by the new bitrate value.
  EXPECT_TRUE(ShouldIgnoreBitrateAdjustment(kOldBandwith * 0.5,
                                            BitrateAdjustmentType::kUp,
                                            kOldBandwith, kMinChange));
  EXPECT_TRUE(ShouldIgnoreBitrateAdjustment(kOldBandwith * 1.5,
                                            BitrateAdjustmentType::kDown,
                                            kOldBandwith, kMinChange));
}

}  // namespace
}  // namespace moqt::test
