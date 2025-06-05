// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "quiche/quic/core/quic_stream_id_manager.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_stream_id_manager_peer.h"

using testing::_;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class MockDelegate : public QuicStreamIdManager::DelegateInterface {
 public:
  MOCK_METHOD(void, SendMaxStreams,
              (QuicStreamCount stream_count, bool unidirectional), (override));
  MOCK_METHOD(bool, CanSendMaxStreams, (), (override));
};

struct TestParams {
  TestParams(ParsedQuicVersion version, Perspective perspective,
             bool is_unidirectional)
      : version(version),
        perspective(perspective),
        is_unidirectional(is_unidirectional) {}

  ParsedQuicVersion version;
  Perspective perspective;
  bool is_unidirectional;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  return absl::StrCat(
      ParsedQuicVersionToString(p.version), "_",
      (p.perspective == Perspective::IS_CLIENT ? "Client" : "Server"),
      (p.is_unidirectional ? "Unidirectional" : "Bidirectional"));
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    if (!version.HasIetfQuicFrames()) {
      continue;
    }
    for (Perspective perspective :
         {Perspective::IS_CLIENT, Perspective::IS_SERVER}) {
      for (bool is_unidirectional : {true, false}) {
        params.push_back(TestParams(version, perspective, is_unidirectional));
      }
    }
  }
  return params;
}

class QuicStreamIdManagerTest : public QuicTestWithParam<TestParams> {
 protected:
  QuicStreamIdManagerTest()
      : stream_id_manager_(&delegate_, IsUnidirectional(), perspective(),
                           GetParam().version, 0,
                           kDefaultMaxStreamsPerConnection) {
    QUICHE_DCHECK(VersionHasIetfQuicFrames(transport_version()));
  }

  QuicTransportVersion transport_version() const {
    return GetParam().version.transport_version;
  }

  // Returns the stream ID for the Nth incoming stream (created by the peer)
  // of the corresponding directionality of this manager.
  QuicStreamId GetNthIncomingStreamId(int n) {
    return QuicUtils::StreamIdDelta(transport_version()) * n +
           (IsUnidirectional()
                ? QuicUtils::GetFirstUnidirectionalStreamId(
                      transport_version(),
                      QuicUtils::InvertPerspective(perspective()))
                : QuicUtils::GetFirstBidirectionalStreamId(
                      transport_version(),
                      QuicUtils::InvertPerspective(perspective())));
  }

  bool IsUnidirectional() { return GetParam().is_unidirectional; }
  Perspective perspective() { return GetParam().perspective; }

  StrictMock<MockDelegate> delegate_;
  QuicStreamIdManager stream_id_manager_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicStreamIdManagerTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicStreamIdManagerTest, Initialization) {
  EXPECT_EQ(0u, stream_id_manager_.outgoing_max_streams());

  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            stream_id_manager_.incoming_actual_max_streams());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            stream_id_manager_.incoming_advertised_max_streams());
  EXPECT_EQ(kDefaultMaxStreamsPerConnection,
            stream_id_manager_.incoming_initial_max_open_streams());
}

// This test checks that the stream advertisement window is set to 1
// if the number of stream ids is 1. This is a special case in the code.
TEST_P(QuicStreamIdManagerTest, CheckMaxStreamsWindowForSingleStream) {
  stream_id_manager_.SetMaxOpenIncomingStreams(1);
  EXPECT_EQ(1u, stream_id_manager_.incoming_initial_max_open_streams());
  EXPECT_EQ(1u, stream_id_manager_.incoming_actual_max_streams());
}

TEST_P(QuicStreamIdManagerTest, CheckMaxStreamsBadValuesOverMaxFailsOutgoing) {
  QuicStreamCount implementation_max = QuicUtils::GetMaxStreamCount();
  // Ensure that the limit is less than the implementation maximum.
  EXPECT_LT(stream_id_manager_.outgoing_max_streams(), implementation_max);

  EXPECT_TRUE(
      stream_id_manager_.MaybeAllowNewOutgoingStreams(implementation_max + 1));
  // Should be pegged at the max.
  EXPECT_EQ(implementation_max, stream_id_manager_.outgoing_max_streams());
}

// Check the case of the stream count in a STREAMS_BLOCKED frame is less than
// the count most recently advertised in a MAX_STREAMS frame.
TEST_P(QuicStreamIdManagerTest, ProcessStreamsBlockedOk) {
  QuicStreamCount stream_count =
      stream_id_manager_.incoming_initial_max_open_streams();
  QuicStreamsBlockedFrame frame(0, stream_count - 1, IsUnidirectional());
  // We have notified peer about current max.
  EXPECT_CALL(delegate_, SendMaxStreams(stream_count, IsUnidirectional()))
      .Times(0);
  std::string error_details;
  EXPECT_TRUE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));
}

// Check the case of the stream count in a STREAMS_BLOCKED frame is equal to the
// count most recently advertised in a MAX_STREAMS frame. No MAX_STREAMS
// should be generated.
TEST_P(QuicStreamIdManagerTest, ProcessStreamsBlockedNoOp) {
  QuicStreamCount stream_count =
      stream_id_manager_.incoming_initial_max_open_streams();
  QuicStreamsBlockedFrame frame(0, stream_count, IsUnidirectional());
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);
}

// Check the case of the stream count in a STREAMS_BLOCKED frame is greater than
// the count most recently advertised in a MAX_STREAMS frame. Expect a
// connection close with an error.
TEST_P(QuicStreamIdManagerTest, ProcessStreamsBlockedTooBig) {
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);
  QuicStreamCount stream_count =
      stream_id_manager_.incoming_initial_max_open_streams() + 1;
  QuicStreamsBlockedFrame frame(0, stream_count, IsUnidirectional());
  std::string error_details;
  EXPECT_FALSE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));
  EXPECT_EQ(
      error_details,
      "StreamsBlockedFrame's stream count 101 exceeds incoming max stream 100");
}

// Same basic tests as above, but calls
// QuicStreamIdManager::MaybeIncreaseLargestPeerStreamId directly, avoiding the
// call chain. The intent is that if there is a problem, the following tests
// will point to either the stream ID manager or the call chain. They also
// provide specific, small scale, tests of a public QuicStreamIdManager method.
// First test make sure that streams with ids below the limit are accepted.
TEST_P(QuicStreamIdManagerTest, IsIncomingStreamIdValidBelowLimit) {
  QuicStreamId stream_id = GetNthIncomingStreamId(
      stream_id_manager_.incoming_actual_max_streams() - 2);
  EXPECT_TRUE(
      stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id, nullptr));
}

// Accept a stream with an ID that equals the limit.
TEST_P(QuicStreamIdManagerTest, IsIncomingStreamIdValidAtLimit) {
  QuicStreamId stream_id = GetNthIncomingStreamId(
      stream_id_manager_.incoming_actual_max_streams() - 1);
  EXPECT_TRUE(
      stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id, nullptr));
}

// Close the connection if the id exceeds the limit.
TEST_P(QuicStreamIdManagerTest, IsIncomingStreamIdInValidAboveLimit) {
  QuicStreamId stream_id =
      GetNthIncomingStreamId(stream_id_manager_.incoming_actual_max_streams());
  std::string error_details;
  EXPECT_FALSE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(
      stream_id, &error_details));
  EXPECT_EQ(error_details,
            absl::StrCat("Stream id ", stream_id,
                         " would exceed stream count limit 100"));
}

TEST_P(QuicStreamIdManagerTest, OnStreamsBlockedFrame) {
  // Get the current maximum allowed incoming stream count.
  QuicStreamCount advertised_stream_count =
      stream_id_manager_.incoming_advertised_max_streams();

  QuicStreamsBlockedFrame frame;

  frame.unidirectional = IsUnidirectional();

  // If the peer is saying it's blocked on the stream count that
  // we've advertised, it's a noop since the peer has the correct information.
  frame.stream_count = advertised_stream_count;
  std::string error_details;
  EXPECT_TRUE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));

  // If the peer is saying it's blocked on a stream count that is larger
  // than what we've advertised, the connection should get closed.
  frame.stream_count = advertised_stream_count + 1;
  EXPECT_FALSE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));
  EXPECT_EQ(
      error_details,
      "StreamsBlockedFrame's stream count 101 exceeds incoming max stream 100");

  // If the peer is saying it's blocked on a count that is less than
  // our actual count, we send a MAX_STREAMS frame and update
  // the advertised value.
  // First, need to bump up the actual max so there is room for the MAX
  // STREAMS frame to send a larger ID.
  QuicStreamCount actual_stream_count =
      stream_id_manager_.incoming_actual_max_streams();

  // Closing a stream will result in the ability to initiate one more
  // stream
  stream_id_manager_.OnStreamClosed(
      QuicStreamIdManagerPeer::GetFirstIncomingStreamId(&stream_id_manager_));
  EXPECT_EQ(actual_stream_count + 1u,
            stream_id_manager_.incoming_actual_max_streams());
  EXPECT_EQ(stream_id_manager_.incoming_actual_max_streams(),
            stream_id_manager_.incoming_advertised_max_streams() + 1u);

  // Now simulate receiving a STREAMS_BLOCKED frame...
  // Changing the actual maximum, above, forces a MAX_STREAMS frame to be
  // sent, so the logic for that (SendMaxStreamsFrame(), etc) is tested.

  // The STREAMS_BLOCKED frame contains the previous advertised count,
  // not the one that the peer would have received as a result of the
  // MAX_STREAMS sent earler.
  frame.stream_count = advertised_stream_count;

  EXPECT_CALL(delegate_, CanSendMaxStreams()).WillOnce(testing::Return(true));
  EXPECT_CALL(delegate_,
              SendMaxStreams(stream_id_manager_.incoming_actual_max_streams(),
                             IsUnidirectional()));

  EXPECT_TRUE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));
  // Check that the saved frame is correct.
  EXPECT_EQ(stream_id_manager_.incoming_actual_max_streams(),
            stream_id_manager_.incoming_advertised_max_streams());
}

TEST_P(QuicStreamIdManagerTest, OnStreamsBlockedFrameCantSend) {
  // Get the current maximum allowed incoming stream count.
  QuicStreamCount advertised_stream_count =
      stream_id_manager_.incoming_advertised_max_streams();

  QuicStreamsBlockedFrame frame;

  frame.unidirectional = IsUnidirectional();

  // First, need to bump up the actual max so there is room for the MAX
  // STREAMS frame to send a larger ID.
  QuicStreamCount actual_stream_count =
      stream_id_manager_.incoming_actual_max_streams();

  // Closing a stream will result in the ability to initiate one more
  // stream
  stream_id_manager_.OnStreamClosed(
      QuicStreamIdManagerPeer::GetFirstIncomingStreamId(&stream_id_manager_));
  EXPECT_EQ(actual_stream_count + 1u,
            stream_id_manager_.incoming_actual_max_streams());
  EXPECT_EQ(stream_id_manager_.incoming_actual_max_streams(),
            stream_id_manager_.incoming_advertised_max_streams() + 1u);

  // Now simulate receiving a STREAMS_BLOCKED frame...
  // Changing the actual maximum, above, forces a MAX_STREAMS frame to be
  // sent, so the logic for that (SendMaxStreamsFrame(), etc) is tested.

  // The STREAMS_BLOCKED frame contains the previous advertised count,
  // not the one that the peer would have received as a result of the
  // MAX_STREAMS sent earler.
  frame.stream_count = advertised_stream_count;

  // Since the delegate returns false, no MAX_STREAMS frame should be sent,
  // and the advertised limit should not increse.
  EXPECT_CALL(delegate_, CanSendMaxStreams()).WillOnce(testing::Return(false));
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);

  const QuicStreamCount advertised_max_streams =
      stream_id_manager_.incoming_advertised_max_streams();
  std::string error_details;
  EXPECT_TRUE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));
  EXPECT_EQ(advertised_max_streams,
            stream_id_manager_.incoming_advertised_max_streams());
}

TEST_P(QuicStreamIdManagerTest, GetNextOutgoingStream) {
  // Number of streams we can open and the first one we should get when
  // opening...
  size_t number_of_streams = kDefaultMaxStreamsPerConnection;

  EXPECT_TRUE(
      stream_id_manager_.MaybeAllowNewOutgoingStreams(number_of_streams));

  QuicStreamId stream_id = IsUnidirectional()
                               ? QuicUtils::GetFirstUnidirectionalStreamId(
                                     transport_version(), perspective())
                               : QuicUtils::GetFirstBidirectionalStreamId(
                                     transport_version(), perspective());

  EXPECT_EQ(number_of_streams, stream_id_manager_.outgoing_max_streams());
  while (number_of_streams) {
    EXPECT_TRUE(stream_id_manager_.CanOpenNextOutgoingStream());
    EXPECT_EQ(stream_id, stream_id_manager_.GetNextOutgoingStreamId());
    stream_id += QuicUtils::StreamIdDelta(transport_version());
    number_of_streams--;
  }

  // If we try to check that the next outgoing stream id is available it should
  // fail.
  EXPECT_FALSE(stream_id_manager_.CanOpenNextOutgoingStream());

  // If we try to get the next id (above the limit), it should cause a quic-bug.
  EXPECT_QUIC_BUG(
      stream_id_manager_.GetNextOutgoingStreamId(),
      "Attempt to allocate a new outgoing stream that would exceed the limit");
}

TEST_P(QuicStreamIdManagerTest, MaybeIncreaseLargestPeerStreamId) {
  QuicStreamId max_stream_id = GetNthIncomingStreamId(
      stream_id_manager_.incoming_actual_max_streams() - 1);
  EXPECT_TRUE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(max_stream_id,
                                                                  nullptr));

  QuicStreamId first_stream_id = GetNthIncomingStreamId(0);
  EXPECT_TRUE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(
      first_stream_id, nullptr));
  // A bad stream ID results in a closed connection.
  std::string error_details;
  EXPECT_FALSE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(
      max_stream_id + QuicUtils::StreamIdDelta(transport_version()),
      &error_details));
  EXPECT_EQ(error_details,
            absl::StrCat(
                "Stream id ",
                max_stream_id + QuicUtils::StreamIdDelta(transport_version()),
                " would exceed stream count limit 100"));
}

TEST_P(QuicStreamIdManagerTest, MaxStreamsWindow) {
  // Open and then close a number of streams to get close to the threshold of
  // sending a MAX_STREAM_FRAME.
  int stream_count = stream_id_manager_.incoming_initial_max_open_streams() /
                         GetQuicFlag(quic_max_streams_window_divisor) -
                     1;

  // Should not get a control-frame transmission since the peer should have
  // "plenty" of stream IDs to use.
  EXPECT_CALL(delegate_, CanSendMaxStreams()).Times(0);
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);

  // Get the first incoming stream ID to try and allocate.
  QuicStreamId stream_id = GetNthIncomingStreamId(0);
  size_t old_available_incoming_streams =
      stream_id_manager_.available_incoming_streams();
  auto i = stream_count;
  while (i) {
    EXPECT_TRUE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id,
                                                                    nullptr));

    // This node should think that the peer believes it has one fewer
    // stream it can create.
    old_available_incoming_streams--;
    EXPECT_EQ(old_available_incoming_streams,
              stream_id_manager_.available_incoming_streams());

    i--;
    stream_id += QuicUtils::StreamIdDelta(transport_version());
  }

  // Now close them, still should get no MAX_STREAMS
  stream_id = GetNthIncomingStreamId(0);
  QuicStreamCount expected_actual_max =
      stream_id_manager_.incoming_actual_max_streams();
  QuicStreamCount expected_advertised_max_streams =
      stream_id_manager_.incoming_advertised_max_streams();
  while (stream_count) {
    stream_id_manager_.OnStreamClosed(stream_id);
    stream_count--;
    stream_id += QuicUtils::StreamIdDelta(transport_version());
    expected_actual_max++;
    EXPECT_EQ(expected_actual_max,
              stream_id_manager_.incoming_actual_max_streams());
    // Advertised maximum should remain the same.
    EXPECT_EQ(expected_advertised_max_streams,
              stream_id_manager_.incoming_advertised_max_streams());
  }

  // This should not change.
  EXPECT_EQ(old_available_incoming_streams,
            stream_id_manager_.available_incoming_streams());

  // Now whenever we close a stream we should get a MAX_STREAMS frame.
  // Above code closed all the open streams, so we have to open/close
  //  EXPECT_CALL(delegate_,
  //  SendMaxStreams(stream_id_manager_.incoming_actual_max_streams(),
  //  IsUnidirectional()));
  EXPECT_CALL(delegate_, CanSendMaxStreams()).WillOnce(testing::Return(true));
  EXPECT_CALL(delegate_, SendMaxStreams(_, IsUnidirectional()));
  EXPECT_TRUE(
      stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id, nullptr));
  stream_id_manager_.OnStreamClosed(stream_id);
}

TEST_P(QuicStreamIdManagerTest, MaxStreamsWindowCantSend) {
  // Open and then close a number of streams to get close to the threshold of
  // sending a MAX_STREAM_FRAME.
  int stream_count = stream_id_manager_.incoming_initial_max_open_streams() /
                         GetQuicFlag(quic_max_streams_window_divisor) -
                     1;

  // Should not get a control-frame transmission since the peer should have
  // "plenty" of stream IDs to use.
  EXPECT_CALL(delegate_, CanSendMaxStreams()).Times(0);
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);

  // Get the first incoming stream ID to try and allocate.
  QuicStreamId stream_id = GetNthIncomingStreamId(0);
  size_t old_available_incoming_streams =
      stream_id_manager_.available_incoming_streams();
  auto i = stream_count;
  while (i) {
    EXPECT_TRUE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id,
                                                                    nullptr));

    // This node should think that the peer believes it has one fewer
    // stream it can create.
    old_available_incoming_streams--;
    EXPECT_EQ(old_available_incoming_streams,
              stream_id_manager_.available_incoming_streams());

    i--;
    stream_id += QuicUtils::StreamIdDelta(transport_version());
  }

  // Now close them, still should get no MAX_STREAMS
  stream_id = GetNthIncomingStreamId(0);
  QuicStreamCount expected_actual_max =
      stream_id_manager_.incoming_actual_max_streams();
  QuicStreamCount expected_advertised_max_streams =
      stream_id_manager_.incoming_advertised_max_streams();
  while (stream_count) {
    stream_id_manager_.OnStreamClosed(stream_id);
    stream_count--;
    stream_id += QuicUtils::StreamIdDelta(transport_version());
    expected_actual_max++;
    EXPECT_EQ(expected_actual_max,
              stream_id_manager_.incoming_actual_max_streams());
    // Advertised maximum should remain the same.
    EXPECT_EQ(expected_advertised_max_streams,
              stream_id_manager_.incoming_advertised_max_streams());
  }

  // This should not change.
  EXPECT_EQ(old_available_incoming_streams,
            stream_id_manager_.available_incoming_streams());

  // Now whenever we close a stream we should get a MAX_STREAMS frame,
  // but since the delegate returns false, no MAX_STREAMS frame should
  // be send and the advertised limit will not change.
  // Above code closed all the open streams, so we have to open/close
  EXPECT_CALL(delegate_, CanSendMaxStreams()).WillOnce(testing::Return(false));
  EXPECT_CALL(delegate_, SendMaxStreams(_, IsUnidirectional())).Times(0);
  EXPECT_TRUE(
      stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id, nullptr));
  stream_id_manager_.OnStreamClosed(stream_id);
  // Advertised maximum should remain the same.
  EXPECT_EQ(expected_advertised_max_streams,
            stream_id_manager_.incoming_advertised_max_streams());
}

TEST_P(QuicStreamIdManagerTest, MaxStreamsWindowStopsIncreasing) {
  // Verify that the incoming stream limit does not increase after
  // StopIncreasingIncomingMaxStreams() is called, even when streams are closed.

  QuicStreamId stream_count =
      stream_id_manager_.incoming_initial_max_open_streams();
  // Open up to the stream limit.
  QuicStreamId stream_id = GetNthIncomingStreamId(0);
  for (QuicStreamCount i = 0; i < stream_count; ++i) {
    EXPECT_TRUE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(stream_id,
                                                                    nullptr));

    stream_id += QuicUtils::StreamIdDelta(transport_version());
  }

  // Prevent max streams from increasing.
  stream_id_manager_.StopIncreasingIncomingMaxStreams();

  // Since the limit does not increase, a MAX_STREAMS frame will not be sent.
  EXPECT_CALL(delegate_, CanSendMaxStreams()).Times(0);
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);

  // Now close them.
  stream_id = GetNthIncomingStreamId(0);
  QuicStreamCount expected_actual_max =
      stream_id_manager_.incoming_actual_max_streams();
  QuicStreamCount expected_advertised_max_streams =
      stream_id_manager_.incoming_advertised_max_streams();
  for (QuicStreamCount i = 0; i < stream_count; ++i) {
    stream_id_manager_.OnStreamClosed(stream_id);
    stream_id += QuicUtils::StreamIdDelta(transport_version());
    // Limits should not change.
    EXPECT_EQ(expected_actual_max,
              stream_id_manager_.incoming_actual_max_streams());
    EXPECT_EQ(expected_advertised_max_streams,
              stream_id_manager_.incoming_advertised_max_streams());
  }
}

TEST_P(QuicStreamIdManagerTest, StreamsBlockedEdgeConditions) {
  QuicStreamsBlockedFrame frame;
  frame.unidirectional = IsUnidirectional();

  // Check that receipt of a STREAMS BLOCKED with stream-count = 0 does nothing
  // when max_allowed_incoming_streams is 0.
  EXPECT_CALL(delegate_, CanSendMaxStreams()).Times(0);
  EXPECT_CALL(delegate_, SendMaxStreams(_, _)).Times(0);
  stream_id_manager_.SetMaxOpenIncomingStreams(0);
  frame.stream_count = 0;
  std::string error_details;
  EXPECT_TRUE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));

  // Check that receipt of a STREAMS BLOCKED with stream-count = 0 invokes a
  // MAX STREAMS, count = 123, when the MaxOpen... is set to 123.
  EXPECT_CALL(delegate_, CanSendMaxStreams()).WillOnce(testing::Return(true));
  EXPECT_CALL(delegate_, SendMaxStreams(123u, IsUnidirectional()));
  QuicStreamIdManagerPeer::set_incoming_actual_max_streams(&stream_id_manager_,
                                                           123);
  frame.stream_count = 0;
  EXPECT_TRUE(stream_id_manager_.OnStreamsBlockedFrame(frame, &error_details));
}

// Test that a MAX_STREAMS frame is generated when half the stream ids become
// available. This has a useful side effect of testing that when streams are
// closed, the number of available stream ids increases.
TEST_P(QuicStreamIdManagerTest, MaxStreamsSlidingWindow) {
  QuicStreamCount first_advert =
      stream_id_manager_.incoming_advertised_max_streams();

  // Open/close enough streams to shrink the window without causing a MAX
  // STREAMS to be generated. The loop
  // will make that many stream IDs available, so the last CloseStream should
  // cause a MAX STREAMS frame to be generated.
  int i =
      static_cast<int>(stream_id_manager_.incoming_initial_max_open_streams() /
                       GetQuicFlag(quic_max_streams_window_divisor));
  QuicStreamId id =
      QuicStreamIdManagerPeer::GetFirstIncomingStreamId(&stream_id_manager_);
  EXPECT_CALL(delegate_, CanSendMaxStreams()).WillOnce(testing::Return(true));
  EXPECT_CALL(delegate_, SendMaxStreams(first_advert + i, IsUnidirectional()));
  while (i) {
    EXPECT_TRUE(
        stream_id_manager_.MaybeIncreaseLargestPeerStreamId(id, nullptr));
    stream_id_manager_.OnStreamClosed(id);
    i--;
    id += QuicUtils::StreamIdDelta(transport_version());
  }
}

TEST_P(QuicStreamIdManagerTest, NewStreamDoesNotExceedLimit) {
  EXPECT_TRUE(stream_id_manager_.MaybeAllowNewOutgoingStreams(100));

  size_t stream_count = stream_id_manager_.outgoing_max_streams();
  EXPECT_NE(0u, stream_count);

  while (stream_count) {
    EXPECT_TRUE(stream_id_manager_.CanOpenNextOutgoingStream());
    stream_id_manager_.GetNextOutgoingStreamId();
    stream_count--;
  }

  EXPECT_EQ(stream_id_manager_.outgoing_stream_count(),
            stream_id_manager_.outgoing_max_streams());
  // Create another, it should fail.
  EXPECT_FALSE(stream_id_manager_.CanOpenNextOutgoingStream());
}

TEST_P(QuicStreamIdManagerTest, AvailableStreams) {
  stream_id_manager_.MaybeIncreaseLargestPeerStreamId(GetNthIncomingStreamId(3),
                                                      nullptr);

  EXPECT_TRUE(stream_id_manager_.IsAvailableStream(GetNthIncomingStreamId(1)));
  EXPECT_TRUE(stream_id_manager_.IsAvailableStream(GetNthIncomingStreamId(2)));
  EXPECT_FALSE(stream_id_manager_.IsAvailableStream(GetNthIncomingStreamId(3)));
  EXPECT_TRUE(stream_id_manager_.IsAvailableStream(GetNthIncomingStreamId(4)));
}

// Tests that if MaybeIncreaseLargestPeerStreamId is given an extremely
// large stream ID (larger than the limit) it is rejected.
// This is a regression for Chromium bugs 909987 and 910040
TEST_P(QuicStreamIdManagerTest, ExtremeMaybeIncreaseLargestPeerStreamId) {
  QuicStreamId too_big_stream_id = GetNthIncomingStreamId(
      stream_id_manager_.incoming_actual_max_streams() + 20);

  std::string error_details;
  EXPECT_FALSE(stream_id_manager_.MaybeIncreaseLargestPeerStreamId(
      too_big_stream_id, &error_details));
  EXPECT_EQ(error_details,
            absl::StrCat("Stream id ", too_big_stream_id,
                         " would exceed stream count limit 100"));
}

}  // namespace
}  // namespace test
}  // namespace quic
