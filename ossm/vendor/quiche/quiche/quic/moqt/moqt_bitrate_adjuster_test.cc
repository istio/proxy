// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_bitrate_adjuster.h"

#include <memory>
#include <optional>
#include <utility>

#include "absl/time/time.h"
#include "quiche/quic/core/quic_bandwidth.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_probe_manager.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {
namespace {

using ::quic::QuicBandwidth;
using ::quic::QuicTimeDelta;
using ::testing::_;
using ::testing::Return;

// Simple adjustable object that just keeps track of whatever value has been
// assigned to it, and has a mock method to notify of it changing.
class MockBitrateAdjustable : public BitrateAdjustable {
 public:
  explicit MockBitrateAdjustable(QuicBandwidth initial_bitrate)
      : bitrate_(initial_bitrate) {}

  quic::QuicBandwidth GetCurrentBitrate() const override { return bitrate_; }
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

class MockProbeManager : public MoqtProbeManagerInterface {
 public:
  MOCK_METHOD(std::optional<ProbeId>, StartProbe,
              (quic::QuicByteCount probe_size, quic::QuicTimeDelta timeout,
               Callback callback),
              (override));
  MOCK_METHOD(std::optional<ProbeId>, StopProbe, (), (override));
  MOCK_METHOD(bool, HasActiveProbe, (), (const, override));
};

constexpr QuicBandwidth kDefaultBitrate =
    QuicBandwidth::FromKBitsPerSecond(2000);
constexpr QuicTimeDelta kDefaultRtt = QuicTimeDelta::FromMilliseconds(20);
constexpr QuicTimeDelta kDefaultTimeScale = QuicTimeDelta::FromSeconds(1);

class MoqtBitrateAdjusterTest : public quiche::test::QuicheTest {
 protected:
  MoqtBitrateAdjusterTest()
      : adjustable_(kDefaultBitrate),
        adjuster_(&clock_, &session_, CreateProbeManager(), &adjustable_) {
    stats_.min_rtt = stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
    stats_.estimated_send_rate_bps = (1.2 * kDefaultBitrate).ToBitsPerSecond();
    ON_CALL(session_, GetSessionStats()).WillByDefault([this] {
      return stats_;
    });

    clock_.AdvanceTime(quic::QuicTimeDelta::FromSeconds(10));
    adjuster_.OnObjectAckSupportKnown(kDefaultTimeScale);
  }

  std::unique_ptr<MoqtProbeManagerInterface> CreateProbeManager() {
    auto manager = std::make_unique<MockProbeManager>();
    ON_CALL(*manager, HasActiveProbe).WillByDefault(Return(false));
    probe_manager_ = manager.get();
    return manager;
  }

  MockBitrateAdjustable adjustable_;
  webtransport::SessionStats stats_;
  quic::MockClock clock_;
  webtransport::test::MockSession session_;
  MockProbeManager* probe_manager_ = nullptr;
  MoqtBitrateAdjuster adjuster_;
};

TEST_F(MoqtBitrateAdjusterTest, IgnoreCallsBeforeStart) {
  MoqtBitrateAdjuster uninitialized_adjuster(
      &clock_, &session_, CreateProbeManager(), &adjustable_);
  uninitialized_adjuster.OnNewObjectEnqueued(Location(1, 0));
  uninitialized_adjuster.OnObjectAckReceived(
      Location(1, 0), QuicTimeDelta::FromMilliseconds(100));
}

TEST_F(MoqtBitrateAdjusterTest, SteadyState) {
  // The fact that estimated bitrate is 1bps should not matter, since we never
  // have a reason to adjust down.
  stats_.estimated_send_rate_bps = 1;

  EXPECT_CALL(adjustable_, OnBitrateAdjusted).Times(0);
  EXPECT_CALL(*probe_manager_, StartProbe).WillRepeatedly(Return(std::nullopt));
  for (int i = 0; i < 250; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    for (int j = 0; j < 10; ++j) {
      adjuster_.OnObjectAckReceived(Location(i, j), kDefaultTimeScale * 0.9);
    }
  }
}

TEST_F(MoqtBitrateAdjusterTest, ProbeUp) {
  stats_.min_rtt = kDefaultRtt.ToAbsl();
  stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
  stats_.rtt_variation = absl::ZeroDuration();
  stats_.application_bytes_acknowledged = 0;
  stats_.estimated_send_rate_bps = kDefaultBitrate.ToBitsPerSecond();

  // Drive the connection in the steady state until the probe is activated.
  EXPECT_CALL(adjustable_, OnBitrateAdjusted).Times(0);
  std::optional<MoqtProbeManagerInterface::Callback> probe_callback;
  quic::QuicByteCount requested_probe_size;
  EXPECT_CALL(*probe_manager_, StartProbe)
      .WillOnce([&](quic::QuicByteCount probe_size, quic::QuicTimeDelta timeout,
                    MoqtProbeManagerInterface::Callback callback) {
        requested_probe_size = probe_size;
        probe_callback = std::move(callback);
        return 12345;
      });
  for (int i = 0; i < 2500; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    adjuster_.OnObjectAckReceived(Location(i, 0), kDefaultTimeScale * 0.9);
    if (probe_callback.has_value()) {
      break;
    }
  }
  ASSERT_TRUE(probe_callback.has_value());

  const QuicTimeDelta kProbeDuration = 20 * kDefaultRtt;
  clock_.AdvanceTime(kProbeDuration);
  stats_.application_bytes_acknowledged += (1 << 30);  // Arbitrary big number.
  stats_.estimated_send_rate_bps = 2 * kDefaultBitrate.ToBitsPerSecond();
  ProbeResult result{.id = 12345,
                     .status = ProbeStatus::kSuccess,
                     .probe_size = requested_probe_size,
                     .time_elapsed = kProbeDuration};
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(kDefaultBitrate * 1.8));
  std::move (*probe_callback)(result);
}

TEST_F(MoqtBitrateAdjusterTest, ProbeUpNotEnteredInPrecariousState) {
  stats_.min_rtt = kDefaultRtt.ToAbsl();
  stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
  stats_.rtt_variation = absl::ZeroDuration();
  stats_.application_bytes_acknowledged = 0;
  stats_.estimated_send_rate_bps = kDefaultBitrate.ToBitsPerSecond();

  EXPECT_CALL(adjustable_, OnBitrateAdjusted).Times(0);
  EXPECT_CALL(*probe_manager_, StartProbe).Times(0);
  for (int i = 0; i < 2500; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    adjuster_.OnObjectAckReceived(Location(i, 0), kDefaultTimeScale * 0.5);
  }
}

TEST_F(MoqtBitrateAdjusterTest, ProbeUpIgnoredDueToBeingTooShort) {
  stats_.min_rtt = kDefaultRtt.ToAbsl();
  stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
  stats_.rtt_variation = absl::ZeroDuration();
  stats_.application_bytes_acknowledged = 0;
  stats_.estimated_send_rate_bps = kDefaultBitrate.ToBitsPerSecond();

  // Drive the connection in the steady state until the probe is activated.
  EXPECT_CALL(adjustable_, OnBitrateAdjusted).Times(0);
  std::optional<MoqtProbeManagerInterface::Callback> probe_callback;
  quic::QuicByteCount requested_probe_size;
  EXPECT_CALL(*probe_manager_, StartProbe)
      .WillOnce([&](quic::QuicByteCount probe_size, quic::QuicTimeDelta timeout,
                    MoqtProbeManagerInterface::Callback callback) {
        requested_probe_size = probe_size;
        probe_callback = std::move(callback);
        return 12345;
      });
  for (int i = 0; i < 2500; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    adjuster_.OnObjectAckReceived(Location(i, 0), kDefaultTimeScale * 0.9);
    if (probe_callback.has_value()) {
      break;
    }
  }
  ASSERT_TRUE(probe_callback.has_value());

  const QuicTimeDelta kProbeDuration = kDefaultRtt;
  clock_.AdvanceTime(kProbeDuration);
  stats_.application_bytes_acknowledged += (1 << 30);  // Arbitrary big number.
  stats_.estimated_send_rate_bps = 2 * kDefaultBitrate.ToBitsPerSecond();
  ProbeResult result{.id = 12345,
                     .status = ProbeStatus::kSuccess,
                     .probe_size = requested_probe_size,
                     .time_elapsed = kProbeDuration};
  std::move (*probe_callback)(result);
}

TEST_F(MoqtBitrateAdjusterTest, ProbeUpUsesAverage) {
  stats_.min_rtt = kDefaultRtt.ToAbsl();
  stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
  stats_.rtt_variation = absl::ZeroDuration();
  stats_.application_bytes_acknowledged = 0;
  stats_.estimated_send_rate_bps = kDefaultBitrate.ToBitsPerSecond();

  // Drive the connection in the steady state until the probe is activated.
  EXPECT_CALL(adjustable_, OnBitrateAdjusted).Times(0);
  std::optional<MoqtProbeManagerInterface::Callback> probe_callback;
  quic::QuicByteCount requested_probe_size;
  EXPECT_CALL(*probe_manager_, StartProbe)
      .WillOnce([&](quic::QuicByteCount probe_size, quic::QuicTimeDelta timeout,
                    MoqtProbeManagerInterface::Callback callback) {
        requested_probe_size = probe_size;
        probe_callback = std::move(callback);
        return 12345;
      });
  for (int i = 0; i < 2500; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    adjuster_.OnObjectAckReceived(Location(i, 0), kDefaultTimeScale * 0.9);
    if (probe_callback.has_value()) {
      break;
    }
  }
  ASSERT_TRUE(probe_callback.has_value());

  const QuicTimeDelta kProbeDuration = 19 * kDefaultRtt;
  const QuicBandwidth kNewBandwidth = 2 * kDefaultBitrate;
  clock_.AdvanceTime(kProbeDuration);
  stats_.application_bytes_acknowledged += kNewBandwidth * kProbeDuration;
  stats_.estimated_send_rate_bps = (100 * kDefaultBitrate).ToBitsPerSecond();
  ProbeResult result{.id = 12345,
                     .status = ProbeStatus::kSuccess,
                     .probe_size = requested_probe_size,
                     .time_elapsed = kProbeDuration};
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(kNewBandwidth * (19.0 / 20.0)));
  std::move (*probe_callback)(result);
}

TEST_F(MoqtBitrateAdjusterTest, ProbeUpCancelInBadState) {
  stats_.min_rtt = kDefaultRtt.ToAbsl();
  stats_.smoothed_rtt = kDefaultRtt.ToAbsl();
  stats_.rtt_variation = absl::ZeroDuration();
  stats_.application_bytes_acknowledged = 0;
  stats_.estimated_send_rate_bps = kDefaultBitrate.ToBitsPerSecond();

  // Drive the connection in the steady state until the probe is activated.
  EXPECT_CALL(adjustable_, OnBitrateAdjusted).Times(0);
  std::optional<MoqtProbeManagerInterface::Callback> probe_callback;
  quic::QuicByteCount requested_probe_size;
  EXPECT_CALL(*probe_manager_, StartProbe)
      .WillOnce([&](quic::QuicByteCount probe_size, quic::QuicTimeDelta timeout,
                    MoqtProbeManagerInterface::Callback callback) {
        requested_probe_size = probe_size;
        probe_callback = std::move(callback);
        return 12345;
      });
  for (int i = 0; i < 2500; ++i) {
    clock_.AdvanceTime(kDefaultRtt);
    adjuster_.OnObjectAckReceived(Location(i, 0), kDefaultTimeScale * 0.9);
    if (probe_callback.has_value()) {
      break;
    }
  }
  ASSERT_TRUE(probe_callback.has_value());

  EXPECT_CALL(*probe_manager_, HasActiveProbe).WillRepeatedly(Return(true));
  EXPECT_CALL(*probe_manager_, StopProbe).Times(0);
  clock_.AdvanceTime(0.1 * kDefaultRtt);
  adjuster_.OnObjectAckReceived(Location(1000, 0), kDefaultTimeScale * 0.9);
  clock_.AdvanceTime(0.1 * kDefaultRtt);
  adjuster_.OnObjectAckReceived(Location(1001, 0), kDefaultTimeScale * 0.5);
  EXPECT_CALL(*probe_manager_, StopProbe).Times(1);
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(kDefaultBitrate * 0.9));
  adjuster_.OnObjectAckReceived(Location(1002, 0), kDefaultTimeScale * 0.1);
}

TEST_F(MoqtBitrateAdjusterTest, AdjustDownOnce) {
  stats_.estimated_send_rate_bps = (0.5 * kDefaultBitrate).ToBitsPerSecond();

  // First time will be skipped, since we aren't far enough into connection.
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_)).Times(0);
  adjuster_.OnObjectAckReceived(Location(0, 0),
                                QuicTimeDelta::FromMilliseconds(-1));

  clock_.AdvanceTime(100 * kDefaultRtt);
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_))
      .WillOnce([](QuicBandwidth new_bitrate) {
        EXPECT_LT(new_bitrate, kDefaultBitrate);
      });
  adjuster_.OnObjectAckReceived(Location(0, 1),
                                QuicTimeDelta::FromMilliseconds(-1));
}

TEST_F(MoqtBitrateAdjusterTest, AdjustDownTwice) {
  int adjusted_times = 0;
  EXPECT_CALL(adjustable_, OnBitrateAdjusted(_)).WillRepeatedly([&] {
    ++adjusted_times;
  });

  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.5 * kDefaultBitrate).ToBitsPerSecond();
  adjuster_.OnObjectAckReceived(Location(0, 0),
                                QuicTimeDelta::FromMilliseconds(-1));
  EXPECT_EQ(adjusted_times, 1);

  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.25 * kDefaultBitrate).ToBitsPerSecond();
  adjuster_.OnObjectAckReceived(Location(0, 1),
                                QuicTimeDelta::FromMilliseconds(-1));
  EXPECT_EQ(adjusted_times, 2);
}

TEST_F(MoqtBitrateAdjusterTest, OutOfOrderAckIgnored) {
  int adjusted_times = 0;
  EXPECT_CALL(adjustable_, OnBitrateAdjusted).WillRepeatedly([&] {
    ++adjusted_times;
  });

  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.5 * kDefaultBitrate).ToBitsPerSecond();
  adjuster_.OnObjectAckReceived(Location(0, 1),
                                QuicTimeDelta::FromMilliseconds(-1));
  EXPECT_EQ(adjusted_times, 1);

  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.25 * kDefaultBitrate).ToBitsPerSecond();
  adjuster_.OnObjectAckReceived(Location(0, 0),
                                QuicTimeDelta::FromMilliseconds(-1));
  EXPECT_EQ(adjusted_times, 1);
}

TEST_F(MoqtBitrateAdjusterTest, Reordering) {
  adjuster_.parameters().quality_level_reordering_thresholds[0] = 1;
  clock_.AdvanceTime(100 * kDefaultRtt);
  stats_.estimated_send_rate_bps = (0.5 * kDefaultBitrate).ToBitsPerSecond();

  adjuster_.OnNewObjectEnqueued(Location(0, 0));
  adjuster_.OnNewObjectEnqueued(Location(0, 1));
  adjuster_.OnNewObjectEnqueued(Location(0, 2));

  EXPECT_CALL(adjustable_, OnBitrateAdjusted);
  adjuster_.OnObjectAckReceived(Location(0, 2), kDefaultTimeScale);
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
