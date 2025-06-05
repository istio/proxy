// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_probe_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/test_tools/mock_clock.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_stream.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

using TestAlarm = quic::test::MockAlarmFactory::TestAlarm;

class MoqtProbeManagerPeer {
 public:
  static TestAlarm* GetAlarm(MoqtProbeManager& manager) {
    return static_cast<TestAlarm*>(manager.timeout_alarm_.get());
  }
};

namespace {

using ::testing::_;
using ::testing::Return;

// Two-byte varint.
constexpr size_t kProbeStreamHeaderSize = 2;

class MockStream : public webtransport::test::MockStream {
 public:
  MockStream(webtransport::StreamId id) : id_(id) {}

  webtransport::StreamId GetStreamId() const override { return id_; }
  absl::Status Writev(absl::Span<const absl::string_view> data,
                      const quiche::StreamWriteOptions& options) override {
    QUICHE_CHECK(!fin_) << "FIN written twice.";
    for (absl::string_view chunk : data) {
      data_.append(chunk);
    }
    fin_ = options.send_fin();
    return absl::OkStatus();
  }
  void SetVisitor(std::unique_ptr<webtransport::StreamVisitor> visitor) {
    visitor_ = std::move(visitor);
  }
  webtransport::StreamVisitor* visitor() override { return visitor_.get(); }

  absl::string_view data() const { return data_; }
  bool fin() const { return fin_; }

 private:
  webtransport::StreamId id_;
  std::unique_ptr<webtransport::StreamVisitor> visitor_ = nullptr;
  std::string data_;
  bool fin_ = false;
};

class MoqtProbeManagerTest : public quiche::test::QuicheTest {
 protected:
  MoqtProbeManagerTest() : manager_(&session_, &clock_, alarm_factory_) {}

  webtransport::test::MockSession session_;
  quic::MockClock clock_;
  quic::test::MockAlarmFactory alarm_factory_;
  MoqtProbeManager manager_;
};

TEST_F(MoqtProbeManagerTest, AddProbe) {
  constexpr webtransport::StreamId kStreamId = 17;
  constexpr quic::QuicByteCount kProbeSize = 8192 + 1;
  constexpr quic::QuicTimeDelta kProbeDuration =
      quic::QuicTimeDelta::FromMilliseconds(100);

  MockStream stream(kStreamId);
  EXPECT_CALL(session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, CanWrite()).WillRepeatedly(Return(true));
  std::optional<ProbeResult> result;
  std::optional<ProbeId> probe_id =
      manager_.StartProbe(kProbeSize, 3 * kProbeDuration,
                          [&](const ProbeResult& r) { result = r; });
  ASSERT_NE(probe_id, std::nullopt);
  ASSERT_EQ(result, std::nullopt);

  EXPECT_TRUE(stream.fin());
  EXPECT_EQ(stream.data().size(), kProbeSize + kProbeStreamHeaderSize);

  clock_.AdvanceTime(kProbeDuration);
  stream.visitor()->OnWriteSideInDataRecvdState();

  ASSERT_NE(result, std::nullopt);
  EXPECT_EQ(result->id, probe_id);
  EXPECT_EQ(result->status, ProbeStatus::kSuccess);
  EXPECT_EQ(result->probe_size, kProbeSize);
  EXPECT_EQ(result->time_elapsed, kProbeDuration);
}

TEST_F(MoqtProbeManagerTest, AddProbeWriteBlockedInTheMiddle) {
  constexpr webtransport::StreamId kStreamId = 17;
  constexpr quic::QuicByteCount kProbeSize = 8192 + 1;
  constexpr quic::QuicTimeDelta kProbeDuration =
      quic::QuicTimeDelta::FromMilliseconds(100);

  MockStream stream(kStreamId);
  EXPECT_CALL(session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, CanWrite())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  std::optional<ProbeId> probe_id = manager_.StartProbe(
      kProbeSize, 3 * kProbeDuration, [&](const ProbeResult& r) {});
  ASSERT_NE(probe_id, std::nullopt);

  EXPECT_FALSE(stream.fin());
  EXPECT_LT(stream.data().size(), kProbeSize);

  EXPECT_CALL(stream, CanWrite()).WillRepeatedly(Return(true));
  stream.visitor()->OnCanWrite();
  EXPECT_TRUE(stream.fin());
  EXPECT_EQ(stream.data().size(), kProbeSize + kProbeStreamHeaderSize);
}

TEST_F(MoqtProbeManagerTest, ProbeCancelledByPeer) {
  constexpr webtransport::StreamId kStreamId = 17;
  constexpr quic::QuicByteCount kProbeSize = 8192 + 1;
  constexpr quic::QuicTimeDelta kProbeDuration =
      quic::QuicTimeDelta::FromMilliseconds(100);

  MockStream stream(kStreamId);
  EXPECT_CALL(session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, CanWrite()).WillRepeatedly(Return(true));
  std::optional<ProbeResult> result;
  std::optional<ProbeId> probe_id =
      manager_.StartProbe(kProbeSize, 3 * kProbeDuration,
                          [&](const ProbeResult& r) { result = r; });
  ASSERT_NE(probe_id, std::nullopt);
  ASSERT_EQ(result, std::nullopt);

  EXPECT_TRUE(stream.fin());
  EXPECT_EQ(stream.data().size(), kProbeSize + kProbeStreamHeaderSize);

  clock_.AdvanceTime(kProbeDuration * 0.5);
  stream.visitor()->OnStopSendingReceived(/*error=*/0);

  ASSERT_NE(result, std::nullopt);
  EXPECT_EQ(result->id, probe_id);
  EXPECT_EQ(result->status, ProbeStatus::kAborted);
  EXPECT_EQ(result->time_elapsed, kProbeDuration * 0.5);
}

TEST_F(MoqtProbeManagerTest, ProbeCancelledByClient) {
  constexpr webtransport::StreamId kStreamId = 17;
  constexpr quic::QuicByteCount kProbeSize = 8192 + 1;
  constexpr quic::QuicTimeDelta kProbeDuration =
      quic::QuicTimeDelta::FromMilliseconds(100);

  MockStream stream(kStreamId);
  EXPECT_CALL(session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, CanWrite()).WillRepeatedly(Return(true));
  std::optional<ProbeResult> result;
  std::optional<ProbeId> probe_id =
      manager_.StartProbe(kProbeSize, 3 * kProbeDuration,
                          [&](const ProbeResult& r) { result = r; });
  ASSERT_NE(probe_id, std::nullopt);
  ASSERT_EQ(result, std::nullopt);

  EXPECT_TRUE(stream.fin());
  EXPECT_EQ(stream.data().size(), kProbeSize + kProbeStreamHeaderSize);

  EXPECT_CALL(session_, GetStreamById(kStreamId)).WillOnce(Return(&stream));
  EXPECT_CALL(stream, ResetWithUserCode(_));
  clock_.AdvanceTime(kProbeDuration * 0.5);
  manager_.StopProbe();
  ASSERT_NE(result, std::nullopt);
  EXPECT_EQ(result->id, probe_id);
  EXPECT_EQ(result->status, ProbeStatus::kAborted);
  EXPECT_EQ(result->time_elapsed, kProbeDuration * 0.5);
}

TEST_F(MoqtProbeManagerTest, Timeout) {
  constexpr webtransport::StreamId kStreamId = 17;
  constexpr quic::QuicByteCount kProbeSize = 8192 + 1;
  constexpr quic::QuicTimeDelta kProbeDuration =
      quic::QuicTimeDelta::FromMilliseconds(100);
  const quic::QuicTimeDelta kTimeout = 0.5 * kProbeDuration;

  MockStream stream(kStreamId);
  EXPECT_CALL(session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&stream));
  EXPECT_CALL(stream, CanWrite()).WillRepeatedly(Return(true));
  std::optional<ProbeResult> result;
  std::optional<ProbeId> probe_id = manager_.StartProbe(
      kProbeSize, kTimeout, [&](const ProbeResult& r) { result = r; });
  ASSERT_NE(probe_id, std::nullopt);
  ASSERT_EQ(result, std::nullopt);

  EXPECT_TRUE(stream.fin());
  EXPECT_EQ(stream.data().size(), kProbeSize + kProbeStreamHeaderSize);

  clock_.AdvanceTime(kTimeout);
  TestAlarm* alarm = MoqtProbeManagerPeer::GetAlarm(manager_);
  EXPECT_EQ(alarm->deadline(), clock_.Now());

  EXPECT_CALL(session_, GetStreamById(kStreamId)).WillOnce(Return(&stream));
  EXPECT_CALL(stream, ResetWithUserCode(_));
  alarm->Fire();
  ASSERT_NE(result, std::nullopt);
  EXPECT_EQ(result->id, probe_id);
  EXPECT_EQ(result->status, ProbeStatus::kTimeout);
  EXPECT_EQ(result->probe_size, kProbeSize);
  EXPECT_EQ(result->time_elapsed, kTimeout);
}

}  // namespace
}  // namespace moqt::test
