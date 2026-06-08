// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/test_tools/moqt_simulator.h"

#include "quiche/quic/core/quic_bandwidth.h"
#include "quiche/quic/core/quic_connection_stats.h"
#include "quiche/quic/core/quic_session.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quic_trace/quic_trace.pb.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace moqt::test {
namespace {

using ::quic_trace::EventType;

class MoqtSimulatorTest : public quiche::test::QuicheTest {};

int CountEventType(const quic_trace::Trace& trace, quic_trace::EventType type) {
  int count = 0;
  for (const quic_trace::Event& event : trace.events()) {
    if (event.event_type() == type) {
      ++count;
    }
  }
  return count;
}

quic::CongestionControlType GetCongestionControlType(
    const quic::QuicSession& session) {
  return session.connection()
      ->sent_packet_manager()
      .GetSendAlgorithm()
      ->GetCongestionControlType();
}

// Ensure the simulation works with default parameters.
TEST_F(MoqtSimulatorTest, DefaultSettings) {
  MoqtSimulator simulator(SimulationParameters{});
  simulator.Run();
  EXPECT_NEAR(simulator.received_on_time_fraction(), 1.0f, 0.001f);

  EXPECT_EQ(GetCongestionControlType(*simulator.client_quic_session()),
            quic::CongestionControlType::kBBR);
  EXPECT_EQ(GetCongestionControlType(*simulator.server_quic_session()),
            quic::CongestionControlType::kBBR);

  int bitrate_events = 0;
  for (const quic_trace::Event& event : simulator.client_trace().events()) {
    if (event.event_type() != EventType::MOQT_TARGET_BITRATE_SET) {
      continue;
    }
    ++bitrate_events;
    EXPECT_GT(event.bandwidth_estimate_bps(),
              SimulationParameters().bitrate.ToBitsPerSecond());
    EXPECT_LT(event.bandwidth_estimate_bps(),
              SimulationParameters().bandwidth.ToBitsPerSecond());
  }
  EXPECT_GT(bitrate_events, 0);
}

// Ensure that the bitrate adaptation down works.
TEST_F(MoqtSimulatorTest, BandwidthTooLow) {
  SimulationParameters parameters;
  parameters.bandwidth = quic::QuicBandwidth::FromKBitsPerSecond(900);
  parameters.delivery_timeout = quic::QuicTimeDelta::FromSeconds(1);

  MoqtSimulator simulator(parameters);
  simulator.Run();
  EXPECT_GE(simulator.received_on_time_fraction(), 0.8f);
  EXPECT_LT(simulator.received_on_time_fraction(), 0.999f);
  EXPECT_GT(CountEventType(simulator.client_trace(),
                           EventType::MOQT_TARGET_BITRATE_SET),
            0);

  quic::QuicConnectionStats stats =
      simulator.client_quic_session()->connection()->GetStats();
  EXPECT_LT(stats.blocked_frames_sent, 16);
}

}  // namespace
}  // namespace moqt::test
