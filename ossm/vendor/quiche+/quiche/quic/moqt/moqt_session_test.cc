// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_session.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_data_reader.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_bidi_stream.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_fetch_task.h"
#include "quiche/quic/moqt/moqt_framer.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_known_track_publisher.h"
#include "quiche/quic/moqt/moqt_live_publisher.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_object_subscriber.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_publisher.h"
#include "quiche/quic/moqt/moqt_session_callbacks.h"
#include "quiche/quic/moqt/moqt_session_interface.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/moqt/test_tools/mock_moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_framer_utils.h"
#include "quiche/quic/moqt/test_tools/moqt_mock_visitor.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_data_reader.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/quiche_weak_ptr.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/test_tools/in_memory_stream.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt {

namespace test {

namespace {

using ::testing::_;
using ::testing::IsNull;
using ::testing::Optional;
using ::testing::Return;
using ::testing::StrictMock;

constexpr webtransport::StreamId kIncomingUniStreamId = 15;
constexpr webtransport::StreamId kOutgoingUniStreamId = 14;
constexpr uint64_t kDefaultLocalRequestId = 0;
constexpr uint64_t kDefaultPeerRequestId = 1;
const MoqtDataStreamType kDefaultSubgroupStreamType =
    MoqtDataStreamType::Subgroup(2, 4, false, false, true);
constexpr MoqtPriority kDefaultPublisherPriority = 0x80;
const TrackProperties kNoProperties;

std::vector<quiche::QuicheMemSlice> PayloadFromString(absl::string_view s) {
  std::vector<quiche::QuicheMemSlice> payload;
  payload.push_back(quiche::QuicheMemSlice::Copy(s));
  return payload;
}

FullTrackName kDefaultTrackName() { return FullTrackName("foo", "bar"); }

MoqtSubscribe DefaultSubscribe(uint64_t request_id) {
  MoqtSubscribe subscribe = {
      request_id,
      kDefaultTrackName(),
      MessageParameters(),
  };
  return subscribe;
}

MessageParameters SubscribeForTest() {
  MessageParameters parameters;
  parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromMilliseconds(10000);
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "bar");
  parameters.set_forward(true);
  parameters.subscriber_priority = 0x20;
  parameters.location_filter =
      LocationFilter::AbsoluteStartFilter(Location(4, 1));
  parameters.group_order = MoqtGroupOrder::kDescending;
  return parameters;
}

// The usual test case is that a SUBSCRIBE is coming in.
MoqtSubscribe DefaultSubscribe() {
  return DefaultSubscribe(kDefaultPeerRequestId);
}

// Used when a test sets up a remote track.
MoqtSubscribe DefaultLocalSubscribe() {
  return DefaultSubscribe(kDefaultLocalRequestId);
}

MoqtTrackStatus DefaultTrackStatus() {
  return MoqtTrackStatus{
      kDefaultPeerRequestId,
      kDefaultTrackName(),
      MessageParameters(),
  };
}

MoqtTrackStatus DefaultLocalTrackStatus() {
  return MoqtTrackStatus{
      kDefaultLocalRequestId,
      kDefaultTrackName(),
      MessageParameters(),
  };
}

MoqtFetch DefaultFetch() {
  MoqtFetch fetch = {
      kDefaultPeerRequestId,
      StandaloneFetch(kDefaultTrackName(), Location(0, 0),
                      Location(1, kMaxObjectId)),
      MessageParameters(),
  };
  return fetch;
}

}  // namespace

class MoqtSessionTest : public quic::test::QuicTest {
 public:
  MoqtSessionTest()
      : session_(&mock_session_,
                 MoqtSessionParameters(quic::Perspective::IS_CLIENT, "", ""),
                 std::make_unique<quic::test::TestAlarmFactory>(),
                 session_callbacks_.AsSessionCallbacks()) {
    session_.set_publisher(&publisher_);
    MoqtSessionPeer::set_peer_setup_received(&session_, true);
    ON_CALL(mock_session_, GetStreamById)
        .WillByDefault(Return(&mock_bidi_stream_));
    EXPECT_EQ(MoqtSessionPeer::GetImplementationString(&session_),
              kImplementationName);
  }
  ~MoqtSessionTest() {
    EXPECT_CALL(session_callbacks_.session_deleted_callback, Call());
  }

  MockTrackPublisher* CreateTrackPublisher() {
    auto publisher = std::make_shared<MockTrackPublisher>(kDefaultTrackName());
    publisher_.Add(publisher);
    ON_CALL(*publisher, largest_location()).WillByDefault(Return(std::nullopt));
    ON_CALL(*publisher, expiration()).WillByDefault(Return(std::nullopt));
    ON_CALL(*publisher, properties())
        .WillByDefault(testing::ReturnRef(kNoProperties));
    return publisher.get();
  }

  void SetLargestId(MockTrackPublisher* publisher, Location largest_id) {
    ON_CALL(*publisher, largest_location()).WillByDefault(Return(largest_id));
  }

  // Opens an incoming request stream, determines the type based on first_byte
  // and returns MoqtBidiStreamBase that can be downcast to the correct type.
  // |wt_stream| is the underlying mock WebTransport stream; if nullptr, use
  // mock_bidi_stream_.
  static constexpr absl::string_view kSubscribeByte = "\x03";
  static constexpr absl::string_view kSubscribeNamespaceByte = "\x50";
  static constexpr absl::string_view kPublishByte = "\x1d";
  static constexpr absl::string_view kTrackStatusByte = "\x0d";
  static constexpr absl::string_view kPublishNamespaceByte = "\x06";
  static constexpr absl::string_view kFetchByte = "\x16";
  std::unique_ptr<MoqtBidiStreamBase> ResponseStream(
      absl::string_view first_byte,
      webtransport::test::MockStream* wt_stream = nullptr) {
    webtransport::test::MockStream* stream =
        wt_stream != nullptr ? wt_stream : &mock_bidi_stream_;
    EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream())
        .WillOnce(Return(stream))
        .WillOnce(Return(nullptr));
    std::unique_ptr<webtransport::StreamVisitor> unknown_bidi_stream;
    std::unique_ptr<MoqtBidiStreamBase> final_stream;
    EXPECT_CALL(*stream, SetVisitor)
        .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
          unknown_bidi_stream = std::move(visitor);
          bidi_visitor_ = unknown_bidi_stream.get();
        })
        .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
          final_stream = std::unique_ptr<MoqtBidiStreamBase>(
              absl::down_cast<MoqtBidiStreamBase*>(visitor.release()));
          bidi_visitor_ = final_stream.get();
        });
    ON_CALL(*stream, visitor).WillByDefault([this]() { return bidi_visitor_; });
    EXPECT_CALL(*stream, PeekNextReadableRegion)
        .WillOnce(
            Return(webtransport::Stream::PeekResult(first_byte, false, false)))
        .WillRepeatedly(
            Return(webtransport::Stream::PeekResult("", false, false)));
    EXPECT_CALL(*stream, ReadableBytes())
        .WillOnce(Return(first_byte.length()))
        .WillRepeatedly(Return(0));
    EXPECT_CALL(*stream, Read(::testing::An<absl::Span<char>>()))
        .WillOnce([&](absl::Span<char> bytes_to_read) {
          memcpy(bytes_to_read.data(), first_byte.data(), first_byte.length());
          return webtransport::Stream::ReadResult(first_byte.length(), false);
        });
    session_.OnIncomingBidirectionalStreamAvailable();
    EXPECT_NE(final_stream, nullptr);
    EXPECT_CALL(*stream, CanWrite).WillRepeatedly(Return(true));
    return final_stream;
  }

  void PrepareRequestStream(
      std::unique_ptr<MoqtBidiStreamTestWrapper>& stream_wrapper,
      webtransport::test::MockStream* wt_stream = nullptr) {
    webtransport::test::MockStream* stream =
        wt_stream != nullptr ? wt_stream : &mock_bidi_stream_;
    ON_CALL(mock_session_, CanOpenNextOutgoingBidirectionalStream())
        .WillByDefault(Return(true));
    EXPECT_CALL(mock_session_, OpenOutgoingBidirectionalStream())
        .WillOnce(Return(stream))
        // TODO(martinduke): The line below is necessary because JoiningFetch()
        // opens two streams. It can be removed once these are implemented on a
        // single bidi stream.
        .RetiresOnSaturation();
    EXPECT_CALL(*stream, SetVisitor)
        .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
          stream_wrapper = std::make_unique<MoqtBidiStreamTestWrapper>(
              std::unique_ptr<MoqtBidiStreamBase>(
                  absl::down_cast<MoqtBidiStreamBase*>(visitor.release())));
          bidi_visitor_ = &stream_wrapper->stream();
        });
    ON_CALL(*stream, visitor).WillByDefault([this]() { return bidi_visitor_; });
    EXPECT_CALL(*stream, CanWrite).WillRepeatedly(Return(true));
  }

  // The publisher receives SUBSCRIBE and synchronously publishes namespaces it
  // supports.
  MoqtObjectListener* ReceiveSubscribeSynchronousOk(
      MockTrackPublisher* publisher, MoqtSubscribe& subscribe,
      MoqtBidiStreamTestWrapper* control_parser, uint64_t track_alias = 0,
      TrackProperties properties = TrackProperties()) {
    MoqtObjectListener* listener_ptr = nullptr;
    EXPECT_CALL(*publisher, AddObjectListener)
        .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
          listener_ptr = listener;
          listener->OnSubscribeAccepted();
        });
    MessageParameters parameters;
    parameters.expires = publisher->expiration();
    parameters.largest_object = publisher->largest_location();
    MoqtSubscribeOk expected_ok(track_alias, parameters, properties);
    EXPECT_CALL(mock_bidi_stream_,
                Writev(SerializedControlMessage(expected_ok), _));
    control_parser->ReceiveMessage(subscribe);
    return listener_ptr;
  }

  // If visitor == nullptr, it's the first object in the stream, and will be
  // assigned to the visitor the session creates.
  // TODO(martinduke): Support delivering object payload.
  void DeliverObject(MoqtObject& object, bool fin,
                     webtransport::test::MockSession& session,
                     webtransport::test::MockStream* stream,
                     std::unique_ptr<webtransport::StreamVisitor>& visitor,
                     MockLiveSubscriberVisitor* track_visitor) {
    MoqtFramer framer(true, quic::Perspective::IS_SERVER);
    std::optional<PublishedObjectMetadata> previous_object;
    if (visitor != nullptr) {
      previous_object = PublishedObjectMetadata();
      previous_object->location.object = object.object_id - 1;
    }
    quiche::QuicheBuffer buffer = framer.SerializeObjectHeader(
        object,
        MoqtDataStreamType::Subgroup(
            *object.subgroup_id, object.object_id, false, false,
            object.first_object_in_subgroup.value_or(true)),
        previous_object);
    size_t data_read = 0;
    if (visitor == nullptr) {  // It's the first object in the stream
      EXPECT_CALL(session, AcceptIncomingUnidirectionalStream())
          .WillOnce(Return(stream))
          .WillOnce(Return(nullptr));
      EXPECT_CALL(*stream, SetVisitor(_))
          .WillRepeatedly(
              [&](std::unique_ptr<webtransport::StreamVisitor> new_visitor) {
                visitor = std::move(new_visitor);
              });
      EXPECT_CALL(*stream, visitor()).WillRepeatedly([&]() {
        return visitor.get();
      });
    }
    EXPECT_CALL(*stream, PeekNextReadableRegion()).WillRepeatedly([&]() {
      return webtransport::Stream::PeekResult(
          absl::string_view(buffer.data() + data_read,
                            buffer.size() - data_read),
          fin && data_read == buffer.size(), fin);
    });
    EXPECT_CALL(*stream, ReadableBytes()).WillRepeatedly([&]() {
      return buffer.size() - data_read;
    });
    EXPECT_CALL(*stream, Read(testing::An<absl::Span<char>>()))
        .WillRepeatedly([&](absl::Span<char> bytes_to_read) {
          size_t read_size =
              std::min(bytes_to_read.size(), buffer.size() - data_read);
          memcpy(bytes_to_read.data(), buffer.data() + data_read, read_size);
          data_read += read_size;
          return webtransport::Stream::ReadResult(
              read_size, fin && data_read == buffer.size());
        });
    EXPECT_CALL(*stream, SkipBytes(_)).WillRepeatedly([&](size_t bytes) {
      data_read += bytes;
      return fin && data_read == buffer.size();
    });
    EXPECT_CALL(*track_visitor, OnObjectFragment).Times(1);
    if (visitor == nullptr) {
      session_.OnIncomingUnidirectionalStreamAvailable();
    } else {
      visitor->OnCanRead();
    }
  }

  MockLiveSubscriberVisitor remote_track_visitor_;
  MoqtKnownTrackPublisher publisher_;
  webtransport::test::MockSession mock_session_;
  MockSessionCallbacks session_callbacks_;
  MoqtSession session_;
  std::unique_ptr<MoqtBidiStreamTestWrapper> bidi_wrapper_;
  webtransport::test::MockStream mock_bidi_stream_, mock_uni_stream_;
  webtransport::StreamVisitor* bidi_visitor_ = nullptr;
};

TEST_F(MoqtSessionTest, Queries) {
  EXPECT_EQ(session_.perspective(), quic::Perspective::IS_CLIENT);
}

// Verify the session sends SETUP on the control stream.
TEST_F(MoqtSessionTest, OnSessionReady) {
  MoqtSessionPeer::set_peer_setup_received(&session_, false);
  EXPECT_CALL(mock_session_, GetNegotiatedSubprotocol)
      .WillOnce(Return(std::optional<std::string>(kDefaultMoqtVersion)));
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  std::unique_ptr<webtransport::StreamVisitor> visitor;
  EXPECT_CALL(mock_uni_stream_, SetVisitor(_))
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> new_visitor) {
        visitor = std::move(new_visitor);
      });
  EXPECT_CALL(mock_uni_stream_, GetStreamId())
      .WillRepeatedly(Return(webtransport::StreamId(4)));
  EXPECT_CALL(mock_uni_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSetup), _));
  session_.OnSessionReady();

  // Receive SERVER_SETUP on an incoming unidirectional stream
  webtransport::test::InMemoryStreamWithWriteBuffer in_memory_stream(0);
  MoqtFramer framer(true, quic::Perspective::IS_SERVER);
  MoqtSetup setup;
  quiche::QuicheBuffer buffer = framer.SerializeSetup(setup);
  in_memory_stream.Receive(absl::string_view(buffer.data(), buffer.size()),
                           /*fin=*/false);

  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&in_memory_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(session_callbacks_.session_established_callback, Call()).Times(1);
  session_.OnIncomingUnidirectionalStreamAvailable();
}

TEST_F(MoqtSessionTest, OnSessionReadyNoControlStream) {
  EXPECT_CALL(mock_session_, GetNegotiatedSubprotocol)
      .WillOnce(Return(std::optional<std::string>(kDefaultMoqtVersion)));
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream)
      .WillOnce(Return(false));
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call);
  session_.OnSessionReady();
}

TEST_F(MoqtSessionTest, PeerOpensBidiStream) {
  MoqtSession server_session(
      &mock_session_, MoqtSessionParameters(quic::Perspective::IS_SERVER),
      std::make_unique<quic::test::TestAlarmFactory>(),
      session_callbacks_.AsSessionCallbacks());
  MoqtSessionPeer::set_peer_setup_received(&server_session, true);
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream())
      .WillOnce(Return(&mock_bidi_stream_))
      .WillOnce(Return(nullptr));
  std::unique_ptr<webtransport::StreamVisitor> visitor;
  webtransport::test::MockStreamVisitor mock_stream_visitor;
  EXPECT_CALL(mock_bidi_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> new_visitor) {
        visitor = std::move(new_visitor);
        EXPECT_CALL(mock_bidi_stream_, visitor).WillOnce(Return(visitor.get()));
      });
  EXPECT_CALL(mock_bidi_stream_, PeekNextReadableRegion())
      .WillRepeatedly(Return(
          webtransport::Stream::PeekResult(absl::string_view(), false, false)));
  server_session.OnIncomingBidirectionalStreamAvailable();
}

TEST_F(MoqtSessionTest, OnClientSetup) {
  MoqtSessionParameters session_parameters(quic::Perspective::IS_SERVER);
  MoqtSession server_session(&mock_session_, session_parameters,
                             std::make_unique<quic::test::TestAlarmFactory>(),
                             session_callbacks_.AsSessionCallbacks());
  // Load a SETUP message into an in-memory stream.
  webtransport::test::InMemoryStreamWithWriteBuffer in_memory_stream(0);
  MoqtFramer framer(session_parameters.using_webtrans,
                    quic::Perspective::IS_CLIENT);
  MoqtSetup setup;
  session_parameters.ToSetupOptions(setup.options);
  quiche::QuicheBuffer buffer = framer.SerializeSetup(setup);
  in_memory_stream.Receive(absl::string_view(buffer.data(), buffer.size()),
                           /*fin=*/false);

  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&in_memory_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(session_callbacks_.session_established_callback, Call());
  server_session.OnIncomingUnidirectionalStreamAvailable();
  EXPECT_NE(MoqtSessionPeer::GetIncomingControlStream(&server_session),
            nullptr);
}

TEST_F(MoqtSessionTest, DuplicateSetup) {
  MoqtSessionParameters session_parameters(quic::Perspective::IS_SERVER);
  MoqtSession server_session(&mock_session_, session_parameters,
                             std::make_unique<quic::test::TestAlarmFactory>(),
                             session_callbacks_.AsSessionCallbacks());
  webtransport::test::InMemoryStreamWithWriteBuffer in_memory_stream(0);
  MoqtFramer framer(session_parameters.using_webtrans,
                    quic::Perspective::IS_CLIENT);
  MoqtSetup setup;
  session_parameters.ToSetupOptions(setup.options);
  quiche::QuicheBuffer buffer = framer.SerializeSetup(setup);
  in_memory_stream.Receive(absl::string_view(buffer.data(), buffer.size()),
                           /*fin=*/false);

  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&in_memory_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(session_callbacks_.session_established_callback, Call());
  server_session.OnIncomingUnidirectionalStreamAvailable();
  EXPECT_NE(MoqtSessionPeer::GetIncomingControlStream(&server_session),
            nullptr);

  // Send a duplicate SETUP message on the control stream.
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation),
                           "Duplicate SETUP message"));
  EXPECT_CALL(session_callbacks_.session_terminated_callback,
              Call(absl::string_view("Duplicate SETUP message")));
  in_memory_stream.Receive(absl::string_view(buffer.data(), buffer.size()),
                           /*fin=*/false);
}

TEST_F(MoqtSessionTest, TwoStreamsStartWithSetup) {
  MoqtSessionParameters session_parameters(quic::Perspective::IS_SERVER);
  MoqtSession server_session(&mock_session_, session_parameters,
                             std::make_unique<quic::test::TestAlarmFactory>(),
                             session_callbacks_.AsSessionCallbacks());
  webtransport::test::InMemoryStreamWithWriteBuffer stream1(0);
  webtransport::test::InMemoryStreamWithWriteBuffer stream2(1);

  MoqtFramer framer(session_parameters.using_webtrans,
                    quic::Perspective::IS_CLIENT);
  MoqtSetup setup;
  session_parameters.ToSetupOptions(setup.options);
  quiche::QuicheBuffer buffer = framer.SerializeSetup(setup);

  stream1.Receive(absl::string_view(buffer.data(), buffer.size()),
                  /*fin=*/false);
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&stream1))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(session_callbacks_.session_established_callback, Call());
  server_session.OnIncomingUnidirectionalStreamAvailable();
  EXPECT_NE(MoqtSessionPeer::GetIncomingControlStream(&server_session),
            nullptr);

  stream2.Receive(absl::string_view(buffer.data(), buffer.size()),
                  /*fin=*/false);
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&stream2))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation),
                           "Multiple control streams"));
  EXPECT_CALL(session_callbacks_.session_terminated_callback,
              Call(absl::string_view("Multiple control streams")));
  server_session.OnIncomingUnidirectionalStreamAvailable();
}

TEST_F(MoqtSessionTest, OnSessionClosed) {
  bool reported_error = false;
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call(_))
      .WillOnce([&](absl::string_view error_message) {
        reported_error = true;
        EXPECT_EQ(error_message, "foo");
      });
  session_.OnSessionClosed(webtransport::SessionErrorCode(1), "foo");
  EXPECT_TRUE(reported_error);
}

TEST_F(MoqtSessionTest, OnIncomingBidirectionalStream) {
  ::testing::InSequence seq;
  StrictMock<webtransport::test::MockStreamVisitor> mock_stream_visitor;
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream())
      .WillOnce(Return(&mock_bidi_stream_));
  EXPECT_CALL(mock_bidi_stream_, SetVisitor).Times(1);
  EXPECT_CALL(mock_bidi_stream_, visitor())
      .WillOnce(Return(&mock_stream_visitor));
  EXPECT_CALL(mock_stream_visitor, OnCanRead()).Times(1);
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream())
      .WillOnce(Return(nullptr));
  session_.OnIncomingBidirectionalStreamAvailable();
}

TEST_F(MoqtSessionTest, OnIncomingUnidirectionalStream) {
  ::testing::InSequence seq;
  StrictMock<webtransport::test::MockStreamVisitor> mock_stream_visitor;
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, SetVisitor(_)).Times(1);
  EXPECT_CALL(mock_uni_stream_, visitor())
      .WillOnce(Return(&mock_stream_visitor));
  EXPECT_CALL(mock_stream_visitor, OnCanRead()).Times(1);
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(nullptr));
  session_.OnIncomingUnidirectionalStreamAvailable();
}

TEST_F(MoqtSessionTest, IncomingPaddingStream) {
  webtransport::test::InMemoryStream padding_stream(/*stream_id=*/10);
  char buffer[64];
  quic::QuicDataWriter writer(sizeof(buffer), buffer);
  ASSERT_TRUE(writer.WriteMoqVarInt(kPaddingStreamType));
  ASSERT_TRUE(writer.WriteStringPiece("padding_data"));
  padding_stream.Receive(absl::string_view(writer.data(), writer.length()),
                         true);
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&padding_stream))
      .WillOnce(Return(nullptr));
  session_.OnIncomingUnidirectionalStreamAvailable();
  EXPECT_EQ(padding_stream.ReadableBytes(), 0);

  padding_stream.Receive("even_more_padding", true);
  EXPECT_EQ(padding_stream.ReadableBytes(), 0);
}

TEST_F(MoqtSessionTest, IncomingUnknownStreamType) {
  webtransport::test::InMemoryStream unknown_stream(/*stream_id=*/10);
  char buffer[16];
  quic::QuicDataWriter writer(sizeof(buffer), buffer);
  ASSERT_TRUE(writer.WriteMoqVarInt(0x99));
  unknown_stream.Receive(absl::string_view(writer.data(), writer.length()),
                         false);
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream())
      .WillOnce(Return(&unknown_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation),
                           "Unknown stream type"));
  EXPECT_CALL(session_callbacks_.session_terminated_callback,
              Call(absl::string_view("Unknown stream type")));
  session_.OnIncomingUnidirectionalStreamAvailable();
}

TEST_F(MoqtSessionTest, PaddingDatagramDiscarded) {
  EXPECT_CALL(mock_session_, CloseSession).Times(0);
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call).Times(0);
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment).Times(0);

  char buffer[64];
  quic::QuicDataWriter writer(sizeof(buffer), buffer);
  ASSERT_TRUE(writer.WriteMoqVarInt(kPaddingDatagramType));
  ASSERT_TRUE(writer.WriteStringPiece("padding_data"));
  session_.OnDatagramReceived(
      absl::string_view(writer.data(), writer.length()));

  char empty_buffer[16];
  quic::QuicDataWriter empty_writer(sizeof(empty_buffer), empty_buffer);
  ASSERT_TRUE(empty_writer.WriteMoqVarInt(kPaddingDatagramType));
  session_.OnDatagramReceived(
      absl::string_view(empty_writer.data(), empty_writer.length()));
}

TEST_F(MoqtSessionTest, Error) {
  bool reported_error = false;
  EXPECT_CALL(
      mock_session_,
      CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation), "foo"))
      .Times(1);
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call(_))
      .WillOnce([&](absl::string_view error_message) {
        reported_error = (error_message == "foo");
      });
  session_.Error(MoqtError::kProtocolViolation, "foo");
  EXPECT_TRUE(reported_error);
}

TEST_F(MoqtSessionTest, AddLocalTrack) {
  MoqtSubscribe request = DefaultSubscribe();
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  // Request for track returns REQUEST_ERROR.
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
  bidi_wrapper_->ReceiveMessage(request);

  // Add the track. Now Subscribe should succeed.
  MockTrackPublisher* track = CreateTrackPublisher();
  request.request_id += 2;
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get(),
                                MoqtSessionPeer::GetLastTrackAlias(&session_));
}

TEST_F(MoqtSessionTest, IncomingPublishRejected) {
  MoqtPublish publish = {
      .request_id = 1,
      .full_track_name = FullTrackName("foo", "bar"),
      .track_alias = 2,
      .parameters = MessageParameters(),
  };
  publish.parameters.largest_object = Location(4, 5);
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kPublishByte));
  // With the default incoming_publish_callbackm, will return REQUEST_ERROR.
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
  bidi_wrapper_->ReceiveMessage(publish);
}

TEST_F(MoqtSessionTest, PublishNamespaceWithOkAndCancel) {
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo> error_message)>
      publish_namespace_response_callback;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(
      mock_bidi_stream_,
      Writev(ControlMessageOfType(MoqtMessageType::kPublishNamespace), _));
  bool cancel_called = false;
  session_.PublishNamespace(TrackNamespace({"foo"}), MessageParameters(),
                            publish_namespace_response_callback.AsStdFunction(),
                            [&]() { cancel_called = true; });

  MoqtRequestOk ok;
  EXPECT_CALL(publish_namespace_response_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            EXPECT_TRUE(std::holds_alternative<MessageParameters>(response));
          });
  bidi_wrapper_->ReceiveMessage(ok);

  bidi_wrapper_->stream().OnResetStreamReceived(
      webtransport::StreamErrorCode(kResetCodeInternalError));
  EXPECT_TRUE(cancel_called);
  // State is gone.
  EXPECT_QUIC_BUG(session_.PublishNamespaceDone(TrackNamespace({"foo"})),
                  "Tried to reset PUBLISH_NAMESPACE for unknown namespace foo");
}

TEST_F(MoqtSessionTest, PublishNamespaceWithOkAndPublishNamespaceDone) {
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      publish_namespace_resolved_callback;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(
      mock_bidi_stream_,
      Writev(ControlMessageOfType(MoqtMessageType::kPublishNamespace), _));
  session_.PublishNamespace(TrackNamespace{"foo"}, MessageParameters(),
                            publish_namespace_resolved_callback.AsStdFunction(),
                            []() {});

  MoqtRequestOk ok;
  EXPECT_CALL(publish_namespace_resolved_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            EXPECT_TRUE(std::holds_alternative<MessageParameters>(response));
          });
  bidi_wrapper_->ReceiveMessage(ok);

  EXPECT_CALL(mock_bidi_stream_, SendStopSending);
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode);
  session_.PublishNamespaceDone(TrackNamespace{"foo"});
  // State is gone.
  EXPECT_QUIC_BUG(session_.PublishNamespaceDone(TrackNamespace({"foo"})),
                  "Tried to reset PUBLISH_NAMESPACE for unknown namespace foo");
}

TEST_F(MoqtSessionTest, PublishNamespaceWithError) {
  testing::MockFunction<void(
      std::variant<MessageParameters, MoqtRequestErrorInfo>)>
      publish_namespace_resolved_callback;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(
      mock_bidi_stream_,
      Writev(ControlMessageOfType(MoqtMessageType::kPublishNamespace), _));
  session_.PublishNamespace(TrackNamespace{"foo"}, MessageParameters(),
                            publish_namespace_resolved_callback.AsStdFunction(),
                            []() {});

  MoqtRequestError error{RequestErrorCode::kInternalError, std::nullopt,
                         "Test error"};
  EXPECT_CALL(publish_namespace_resolved_callback, Call)
      .WillOnce(
          [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
            ASSERT_TRUE(std::holds_alternative<MoqtRequestErrorInfo>(response));
            const MoqtRequestErrorInfo& error_info =
                std::get<MoqtRequestErrorInfo>(response);
            EXPECT_EQ(error_info, error);
          });
  ExpectFin(mock_bidi_stream_);
  bidi_wrapper_->ReceiveMessage(error);
  // State is gone.
  EXPECT_QUIC_BUG(session_.PublishNamespaceDone(TrackNamespace({"foo"})),
                  "Tried to reset PUBLISH_NAMESPACE for unknown namespace foo");
}

TEST_F(MoqtSessionTest, AsynchronousSubscribeReturnsOk) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MoqtSubscribe request = DefaultSubscribe();
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtObjectListener* listener;
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener_ptr,
                    const MessageParameters&) { listener = listener_ptr; });
  bidi_wrapper_->ReceiveMessage(request);

  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribeOk), _));
  listener->OnSubscribeAccepted();
  EXPECT_TRUE(MoqtSessionPeer::RequestIdIsLivePublisher(&session_,
                                                        kDefaultPeerRequestId));
}

TEST_F(MoqtSessionTest, AsynchronousSubscribeReturnsError) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MoqtSubscribe request = DefaultSubscribe();
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtObjectListener* listener;
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener_ptr,
                    const MessageParameters&) { listener = listener_ptr; });
  bidi_wrapper_->ReceiveMessage(request);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  listener->OnSubscribeRejected(MoqtRequestErrorInfo(
      RequestErrorCode::kInternalError, std::nullopt, "Test error"));
}

TEST_F(MoqtSessionTest, SynchronousSubscribeReturnsError) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MoqtSubscribe request = DefaultSubscribe();
  MockTrackPublisher* track = CreateTrackPublisher();
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
        EXPECT_CALL(*track, RemoveObjectListener);
        listener->OnSubscribeRejected(MoqtRequestErrorInfo(
            RequestErrorCode::kInternalError, std::nullopt, "Test error"));
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  bidi_wrapper_->ReceiveMessage(request);
}

TEST_F(MoqtSessionTest, SubscribeForPast) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  SetLargestId(track, Location(10, 20));
  MoqtSubscribe request = DefaultSubscribe();
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());
}

TEST_F(MoqtSessionTest, SubscribeDoNotForward) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtSubscribe request = DefaultSubscribe();
  request.parameters.set_forward(false);
  request.parameters.location_filter = LocationFilter::NextObjectFilter();
  MoqtObjectListener* listener =
      ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());
  // forward=false, so incoming objects are ignored.
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .Times(0);
  listener->OnNewObjectAvailable(Location(0, 0), 0, kDefaultPublisherPriority);
}

TEST_F(MoqtSessionTest, SubscribeAbsoluteStartNoDataYet) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtSubscribe request = DefaultSubscribe();
  request.parameters.location_filter =
      LocationFilter::AbsoluteStartFilter(Location(1, 0));
  MoqtObjectListener* listener =
      ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());
  // Window was not set to (0, 0) by SUBSCRIBE acceptance.
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .Times(0);
  listener->OnNewObjectAvailable(Location(0, 0), 0, kDefaultPublisherPriority);
}

TEST_F(MoqtSessionTest, SubscribeNextGroup) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtSubscribe request = DefaultSubscribe();
  request.parameters.location_filter = LocationFilter::RelativeStartFilter(0);
  SetLargestId(track, Location(10, 20));
  MoqtObjectListener* listener =
      ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());
  // Later objects in group 10 ignored.
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .Times(0);
  listener->OnNewObjectAvailable(Location(10, 21), 0,
                                 kDefaultPublisherPriority);
  // Group 11 is sent.
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(false));
  listener->OnNewObjectAvailable(Location(11, 0), 0, kDefaultPublisherPriority);
}

TEST_F(MoqtSessionTest, TwoSubscribesForTrack) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtSubscribe request = DefaultSubscribe();
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());

  request.request_id = 3;
  request.parameters.location_filter =
      LocationFilter::AbsoluteStartFilter(Location(12, 0));
  webtransport::test::MockStream bidi_stream_2;
  auto bidi_wrapper_2 = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte, &bidi_stream_2));
  EXPECT_CALL(bidi_stream_2,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
  bidi_wrapper_2->ReceiveMessage(request);
}

TEST_F(MoqtSessionTest, UnsubscribeAllowsSecondSubscribe) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtSubscribe request = DefaultSubscribe();
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());

  // Peer unsubscribes.
  bidi_wrapper_->stream().Reset(kResetCodeCancelled);
  bidi_wrapper_ = nullptr;
  EXPECT_FALSE(MoqtSessionPeer::RequestIdIsLivePublisher(&session_, 1));

  // Subscribe again, succeeds.
  request.request_id = 3;
  request.parameters.location_filter =
      LocationFilter::AbsoluteStartFilter(Location(12, 0));
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get(),
                                /*track_alias=*/1);
}

TEST_F(MoqtSessionTest, RequestIdWrongLsb) {
  // Client session expects odd request IDs from the server.
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kInvalidRequestId),
                           "Request ID evenness incorrect"));
  EXPECT_FALSE(
      MoqtSessionPeer::ValidateNewIncomingRequestId(&session_, 0).ok());

  // Server session expects even request IDs from the client.
  MoqtSessionParameters server_parameters(quic::Perspective::IS_SERVER);
  MoqtSession server_session(&mock_session_, server_parameters,
                             std::make_unique<quic::test::TestAlarmFactory>(),
                             session_callbacks_.AsSessionCallbacks());
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kInvalidRequestId),
                           "Request ID evenness incorrect"));
  EXPECT_FALSE(
      MoqtSessionPeer::ValidateNewIncomingRequestId(&server_session, 1).ok());
  QUICHE_EXPECT_OK(
      MoqtSessionPeer::ValidateNewIncomingRequestId(&server_session, 0));
}

TEST_F(MoqtSessionTest, RequestIdNotIncreasing) {
  MoqtSubscribe request = DefaultSubscribe();
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  EXPECT_CALL(*track, AddObjectListener);
  bidi_wrapper_->ReceiveMessage(request);

  // Second request is a protocol violation.
  request.full_track_name = FullTrackName({"dead", "beef"});
  auto publisher =
      std::make_shared<MockTrackPublisher>(request.full_track_name);
  publisher_.Add(publisher);
  webtransport::test::MockStream bidi_stream_2;
  auto bidi_wrapper_2 = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte, &bidi_stream_2));
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kInvalidRequestId),
                           "Duplicate Request ID"));
  bidi_wrapper_2->ReceiveMessage(request);
}

TEST_F(MoqtSessionTest, SubscribeDuplicateTrackName) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters(SubscribeForTest());
  EXPECT_TRUE(session_.Subscribe(FullTrackName("foo", "bar"),
                                 &remote_track_visitor_, parameters));
  EXPECT_FALSE(session_.Subscribe(FullTrackName("foo", "bar"),
                                  &remote_track_visitor_, parameters));
}

TEST_F(MoqtSessionTest, SubscribeWithOk) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters(SubscribeForTest());
  session_.Subscribe(FullTrackName("foo", "bar"), &remote_track_visitor_,
                     parameters);
  MoqtSubscribeOk ok(/*track_alias=*/2, MessageParameters(), TrackProperties());
  EXPECT_CALL(remote_track_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName& ftn,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            EXPECT_EQ(ftn, FullTrackName("foo", "bar"));
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(response));
          });
  bidi_wrapper_->ReceiveMessage(ok);
}

TEST_F(MoqtSessionTest, SubscribeNextGroupWithOk) {
  PrepareRequestStream(bidi_wrapper_);
  MoqtSubscribe subscribe = DefaultLocalSubscribe();
  subscribe.parameters.location_filter = LocationFilter::RelativeStartFilter(0);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(subscribe), _));
  session_.Subscribe(FullTrackName("foo", "bar"), &remote_track_visitor_,
                     subscribe.parameters);
  MoqtSubscribeOk ok(/*track_alias=*/2, MessageParameters(), TrackProperties());
  EXPECT_CALL(remote_track_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName& ftn,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            EXPECT_EQ(ftn, FullTrackName("foo", "bar"));
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(response));
          });
  bidi_wrapper_->ReceiveMessage(ok);
}

TEST_F(MoqtSessionTest, OutgoingSubscribeUpdate) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters(SubscribeForTest());
  parameters.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(1, 0), 10);
  session_.Subscribe(FullTrackName("foo", "bar"), &remote_track_visitor_,
                     parameters);
  MoqtSubscribeOk ok(/*track_alias=*/2, MessageParameters(), TrackProperties());
  EXPECT_CALL(remote_track_visitor_, OnReply);
  bidi_wrapper_->ReceiveMessage(ok);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestUpdate), _));
  MessageParameters update_parameters;
  update_parameters.location_filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(2, 1), 9);
  // Set to a non-null value to ensure that the callback is called.
  bool got_response = false;
  EXPECT_TRUE(session_.SubscribeUpdate(
      FullTrackName("foo", "bar"), update_parameters,
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> info) {
        got_response = true;
        ASSERT_TRUE(std::holds_alternative<MessageParameters>(info));
        EXPECT_EQ(std::get<MessageParameters>(info), MessageParameters());
      }));
  bidi_wrapper_->ReceiveMessage(MoqtRequestOk());
  EXPECT_TRUE(got_response);
  // Check if window is functional by receiving datagrams. Type = 8, alias = 2,
  // Location = (2,0), payload = "foo".
  char datagram[] = {0x08, 0x02, 0x02, 0x00, 0x66, 0x6f, 0x6f};
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment).Times(0);
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
  datagram[3] = 0x01;  // Location is (2,1), in window.
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment);
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
  datagram[2] = 0x09;  // Location is (9, 63), in window.
  datagram[3] = 0x3f;
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment);
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
  datagram[2] = 0x0a;  // Location is (10, 0), not in window.
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment).Times(0);
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
}

TEST_F(MoqtSessionTest, OutgoingRequestUpdateInvalid) {
  // Wrong track name.
  EXPECT_FALSE(session_.SubscribeUpdate(
      FullTrackName("foo", "bar"), MessageParameters(),
      +[](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}));
}

TEST_F(MoqtSessionTest, SubscribeWithError) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters(SubscribeForTest());
  parameters.location_filter = LocationFilter::NextObjectFilter();
  session_.Subscribe(FullTrackName("foo", "bar"), &remote_track_visitor_,
                     parameters);
  MoqtRequestError error(RequestErrorCode::kInvalidRange, std::nullopt,
                         "deadbeef");
  EXPECT_CALL(remote_track_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName& ftn,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            EXPECT_EQ(ftn, FullTrackName("foo", "bar"));
            EXPECT_TRUE(
                std::holds_alternative<MoqtRequestErrorInfo>(response) &&
                std::get<MoqtRequestErrorInfo>(response) == error);
          });
  EXPECT_CALL(mock_bidi_stream_, Writev(testing::IsEmpty(), _));  // FIN.
  bidi_wrapper_->ReceiveMessage(error);
}

TEST_F(MoqtSessionTest, Unsubscribe) {
  PrepareRequestStream(bidi_wrapper_);
  FullTrackName ftn = FullTrackName("foo", "bar");
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  EXPECT_TRUE(
      session_.Subscribe(ftn, &remote_track_visitor_, MessageParameters()));
  EXPECT_CALL(mock_bidi_stream_, SendStopSending);
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode);
  EXPECT_CALL(remote_track_visitor_, OnPublishDone);
  session_.Unsubscribe(ftn);
  // Verify it was destroyed.
  EXPECT_CALL(mock_bidi_stream_, SendStopSending).Times(0);
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode).Times(0);
  EXPECT_CALL(remote_track_visitor_, OnPublishDone).Times(0);
  session_.Unsubscribe(ftn);
}

TEST_F(MoqtSessionTest, ReplyToPublishNamespaceWithOkThenPublishNamespaceDone) {
  TrackNamespace track_namespace{"foo"};
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kPublishNamespaceByte));
  MoqtPublishNamespace publish_namespace = {
      kDefaultPeerRequestId,
      track_namespace,
      parameters,
  };
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(track_namespace, _, _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters* params,
                    MoqtResponseCallback callback) {
        EXPECT_TRUE(params != nullptr && *params == parameters);
        std::move(callback)(MessageParameters());
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _));
  bidi_wrapper_->ReceiveMessage(publish_namespace);
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(track_namespace, IsNull(), IsNull()));
  bidi_wrapper_->stream().OnResetStreamReceived(kResetCodeCancelled);
}

TEST_F(MoqtSessionTest,
       ReplyToPublishNamespaceWithOkThenPublishNamespaceCancel) {
  TrackNamespace track_namespace{"foo"};
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kPublishNamespaceByte));
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  MoqtPublishNamespace publish_namespace = {
      kDefaultPeerRequestId,
      track_namespace,
      parameters,
  };
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(track_namespace, _, _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters* params,
                    MoqtResponseCallback callback) {
        EXPECT_TRUE(params != nullptr && *params == parameters);
        std::move(callback)(MessageParameters());
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _));
  bidi_wrapper_->ReceiveMessage(publish_namespace);
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(track_namespace, IsNull(), IsNull()));
  EXPECT_TRUE(
      session_.PublishNamespaceCancel(track_namespace, kResetCodeCancelled));
  // State is gone.
  EXPECT_QUIC_BUG(session_.PublishNamespaceDone(TrackNamespace({"foo"})),
                  "Tried to reset PUBLISH_NAMESPACE for unknown namespace foo");
}

TEST_F(MoqtSessionTest, ReplyToPublishNamespaceWithError) {
  TrackNamespace track_namespace{"foo"};
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kPublishNamespaceByte));
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  MoqtPublishNamespace publish_namespace = {
      kDefaultPeerRequestId,
      track_namespace,
      parameters,
  };
  MoqtRequestErrorInfo error = {
      RequestErrorCode::kNotSupported,
      /*retry_interval=*/std::nullopt,
      "deadbeef",
  };
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(track_namespace, _, _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters* params,
                    MoqtResponseCallback callback) {
        EXPECT_TRUE(params != nullptr && *params == parameters);
        std::move(callback)(error);
      })
      .WillOnce([&](const TrackNamespace&, const MessageParameters* params,
                    MoqtResponseCallback callback) {  // Teardown
        EXPECT_TRUE(params == nullptr);
        EXPECT_TRUE(callback == nullptr);
      });
  EXPECT_CALL(mock_bidi_stream_, Writev(SerializedControlMessage(error), _));
  bidi_wrapper_->ReceiveMessage(publish_namespace);
}

TEST_F(MoqtSessionTest, SubscribeNamespaceLifeCycle) {
  TrackNamespace prefix({"foo"});
  bool got_callback = false;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(
      mock_bidi_stream_,
      Writev(ControlMessageOfType(MoqtMessageType::kSubscribeNamespace), _));
  std::unique_ptr<MoqtNamespaceTask> task = session_.SubscribeNamespace(
      prefix, MessageParameters(),
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
        got_callback = true;
        EXPECT_TRUE(std::holds_alternative<MessageParameters>(response));
      });
  bidi_wrapper_->ReceiveMessage(MoqtRequestOk());
  EXPECT_TRUE(got_callback);
  EXPECT_CALL(mock_bidi_stream_, SendStopSending);
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode);
}

TEST_F(MoqtSessionTest, SubscribeNamespaceError) {
  TrackNamespace prefix({"foo"});
  bool got_callback = false;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(
      mock_bidi_stream_,
      Writev(ControlMessageOfType(MoqtMessageType::kSubscribeNamespace), _));
  std::unique_ptr<MoqtNamespaceTask> task = session_.SubscribeNamespace(
      prefix, MessageParameters(),
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {
        got_callback = true;
        EXPECT_TRUE(std::holds_alternative<MoqtRequestErrorInfo>(response));
        const MoqtRequestErrorInfo& error =
            std::get<MoqtRequestErrorInfo>(response);
        EXPECT_EQ(error.error_code, RequestErrorCode::kInvalidRange);
        EXPECT_EQ(error.reason_phrase, "deadbeef");
      });
  MoqtRequestError error(RequestErrorCode::kInvalidRange, std::nullopt,
                         "deadbeef");
  bidi_wrapper_->ReceiveMessage(error);
  EXPECT_TRUE(got_callback);
}

TEST_F(MoqtSessionTest, SubscribeOkWithBadTrackAlias) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  session_.Subscribe(FullTrackName("foo", "bar"), &remote_track_visitor_,
                     MessageParameters());
  MoqtSubscribeOk subscribe_ok(/*track_alias=*/2, MessageParameters(),
                               TrackProperties());
  bidi_wrapper_->ReceiveMessage(subscribe_ok);
  // Second subscribe, but OK has the same track alias.
  webtransport::test::MockStream bidi_stream_2;
  std::unique_ptr<MoqtBidiStreamTestWrapper> bidi_wrapper_2;
  PrepareRequestStream(bidi_wrapper_2, &bidi_stream_2);
  EXPECT_CALL(bidi_stream_2,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  session_.Subscribe(FullTrackName("foo2", "bar2"), &remote_track_visitor_,
                     MessageParameters());
  EXPECT_CALL(
      mock_session_,
      CloseSession(static_cast<uint64_t>(MoqtError::kDuplicateTrackAlias),
                   "Track alias already exists"));
  bidi_wrapper_2->ReceiveMessage(subscribe_ok);
}

TEST_F(MoqtSessionTest, ReceiveUnsubscribe) {
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtSubscribe request = DefaultSubscribe();
  const MoqtPriority kLocalDefaultPriority = 0x20;
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  TrackProperties properties(std::nullopt, std::nullopt, std::nullopt,
                             kLocalDefaultPriority, std::nullopt, std::nullopt,
                             std::nullopt);
  EXPECT_CALL(*track, properties)
      .WillRepeatedly(testing::ReturnRef(properties));
  MoqtObjectListener* listener = ReceiveSubscribeSynchronousOk(
      track, request, bidi_wrapper_.get(), /*track_alias=*/0, properties);
  EXPECT_CALL(*track, RemoveObjectListener(listener));
  bidi_wrapper_->stream().OnResetStreamReceived(kResetCodeCancelled);
}

TEST_F(MoqtSessionTest, ReceiveDatagram) {
  FullTrackName ftn("foo", "bar");
  const MoqtPriority kPeerDefaultPriority = 0x20;
  PrepareRequestStream(bidi_wrapper_);
  std::string payload = "deadbeef";
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  session_.Subscribe(ftn, &remote_track_visitor_, MessageParameters());
  MoqtSubscribeOk ok(2, MessageParameters(),
                     TrackProperties(std::nullopt, std::nullopt, std::nullopt,
                                     kPeerDefaultPriority, std::nullopt,
                                     std::nullopt, std::nullopt));
  EXPECT_CALL(remote_track_visitor_, OnReply);
  bidi_wrapper_->ReceiveMessage(ok);

  MoqtObject object = {
      /*track_alias=*/2,
      /*group_sequence=*/0,
      /*object_sequence=*/0,
      /*publisher_priority=*/0,
      /*properties=*/"foo",
      /*object_status=*/MoqtObjectStatus::kNormal,
      /*subgroup_id=*/std::nullopt,
      /*first_object_in_subgroup=*/std::nullopt,
      /*payload_length=*/8,
  };
  char datagram[] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x03, 0x66, 0x6f, 0x6f,
                     0x64, 0x65, 0x61, 0x64, 0x62, 0x65, 0x65, 0x66};
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment)
      .WillOnce([&](const FullTrackName& track_name,
                    const PublishedObjectMetadata& metadata,
                    absl::string_view received_payload, uint64_t offset) {
        EXPECT_EQ(track_name, ftn);
        EXPECT_EQ(metadata.location,
                  Location(object.group_id, object.object_id));
        EXPECT_EQ(metadata.subgroup, object.subgroup_id);
        EXPECT_EQ(metadata.properties, object.properties);
        EXPECT_EQ(metadata.publisher_priority, object.publisher_priority);
        EXPECT_EQ(metadata.status, object.object_status);
        EXPECT_EQ(metadata.payload_length, payload.length());
        EXPECT_EQ(payload, received_payload);
        EXPECT_EQ(offset, 0);
      });
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
}

TEST_F(MoqtSessionTest, UsePeerDefaultPriority) {
  FullTrackName ftn("foo", "bar");
  const MoqtPriority kPeerDefaultPriority = 0x20;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  session_.Subscribe(ftn, &remote_track_visitor_, MessageParameters());
  MoqtSubscribeOk ok(2, MessageParameters(),
                     TrackProperties(std::nullopt, std::nullopt, std::nullopt,
                                     kPeerDefaultPriority, std::nullopt,
                                     std::nullopt, std::nullopt));
  EXPECT_CALL(remote_track_visitor_, OnReply);
  bidi_wrapper_->ReceiveMessage(ok);
  // Omit priority from a datagram.
  char datagram[] = {0x0c, 0x02, 0x05, 0x64, 0x65, 0x61,
                     0x64, 0x62, 0x65, 0x65, 0x66};
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment)
      .WillOnce([&](const FullTrackName&,
                    const PublishedObjectMetadata& metadata, absl::string_view,
                    uint64_t offset) {
        EXPECT_EQ(metadata.publisher_priority, kPeerDefaultPriority);
        EXPECT_EQ(metadata.payload_length, 8u);
        EXPECT_EQ(offset, 0);
      });
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
  // Omit priority from a stream.
  webtransport::test::InMemoryStream in_memory_stream(2);
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor =
      MoqtSessionPeer::CreateIncomingStreamVisitor(&session_,
                                                   &in_memory_stream);
  in_memory_stream.SetVisitor(std::move(stream_visitor));
  char stream_data[] = {0x30, 0x02, 0x06, 0x00, 0x03, 0x66, 0x6f, 0x6f};
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment)
      .WillOnce([&](const FullTrackName&,
                    const PublishedObjectMetadata& metadata, absl::string_view,
                    uint64_t offset) {
        EXPECT_EQ(metadata.publisher_priority, kPeerDefaultPriority);
        EXPECT_EQ(metadata.payload_length, 3u);
        EXPECT_EQ(offset, 0);
      });
  in_memory_stream.Receive(absl::string_view(stream_data, sizeof(stream_data)),
                           false);
}

TEST_F(MoqtSessionTest, OmitPublisherPriority) {
  MoqtSubscribe request = DefaultSubscribe();
  const MoqtPriority kLocalDefaultPriority = 0x20;
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  // Create the publisher and the SUBSCRIBE with kLocalDefaultPriority.
  MockTrackPublisher* track = CreateTrackPublisher();
  std::make_shared<MockTrackPublisher>(request.full_track_name);
  TrackProperties properties(std::nullopt, std::nullopt, std::nullopt,
                             kLocalDefaultPriority, std::nullopt, std::nullopt,
                             std::nullopt);
  EXPECT_CALL(*track, properties)
      .WillRepeatedly(testing::ReturnRef(properties));
  MoqtObjectListener* listener = ReceiveSubscribeSynchronousOk(
      track, request, bidi_wrapper_.get(), /*track_alias=*/0, properties);

  // Deliver an object with kLocalDefaultPriority; stream_type will omit
  // the priority.
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, GetStreamId()).WillRepeatedly(Return(1));
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        stream_visitor = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(mock_uni_stream_, visitor()).WillRepeatedly([&]() {
    return stream_visitor.get();
  });
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  EXPECT_CALL(*track, GetCachedObject(_, _, _, _))
      .WillOnce(Return(PublishedObject{
          PublishedObjectMetadata{
              Location(0, 0), 0, "", MoqtObjectStatus::kNormal,
              kLocalDefaultPriority, true, 8, MoqtSessionPeer::Now(&session_)},
          PayloadFromString("deadbeef")}))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(mock_uni_stream_, Writev)
      .WillOnce([&](absl::Span<quiche::QuicheMemSlice> data,
                    const webtransport::StreamWriteOptions& options) {
        // The stream type omits the priority.
        EXPECT_TRUE(static_cast<const uint8_t>(data[0].AsStringView()[0]) &
                    MoqtDataStreamType::kDefaultPriority);
        return absl::OkStatus();
      });
  listener->OnNewObjectAvailable(Location(0, 0), 0, kLocalDefaultPriority);
  // Send a datagram with the default priority.
  EXPECT_CALL(*track, GetCachedObject(_, _, _, _))
      .WillOnce(Return(PublishedObject{
          PublishedObjectMetadata{Location(0, 1), std::nullopt, "",
                                  MoqtObjectStatus::kNormal,
                                  kLocalDefaultPriority, std::nullopt, 8,
                                  MoqtSessionPeer::Now(&session_)},
          PayloadFromString("deadbeef")}));
  EXPECT_CALL(mock_session_, SendOrQueueDatagram)
      .WillOnce([](absl::string_view datagram) {
        EXPECT_TRUE(static_cast<const uint8_t>(datagram[0]) &
                    MoqtDatagramType::kDefaultPriority);
        return webtransport::DatagramStatus{
            webtransport::DatagramStatusCode::kSuccess, ""};
      });
  listener->OnNewObjectAvailable(Location(0, 1), std::nullopt,
                                 kLocalDefaultPriority);
  // Non-default priority
  EXPECT_CALL(*track, GetCachedObject(_, _, _, _))
      .WillOnce(Return(PublishedObject{
          PublishedObjectMetadata{Location(0, 2), std::nullopt, "",
                                  MoqtObjectStatus::kNormal,
                                  kLocalDefaultPriority + 1, std::nullopt, 8,
                                  MoqtSessionPeer::Now(&session_)},
          PayloadFromString("deadbeef")}));
  EXPECT_CALL(mock_session_, SendOrQueueDatagram)
      .WillOnce([](absl::string_view datagram) {
        EXPECT_FALSE(static_cast<const uint8_t>(datagram[0]) &
                     MoqtDatagramType::kDefaultPriority);
        return webtransport::DatagramStatus{
            webtransport::DatagramStatusCode::kSuccess, ""};
      });
  listener->OnNewObjectAvailable(Location(0, 2), std::nullopt,
                                 kLocalDefaultPriority + 1);
}

TEST_F(MoqtSessionTest, DatagramOutOfWindow) {
  FullTrackName ftn("foo", "bar");
  const MoqtPriority kPeerDefaultPriority = 0x20;
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters params;
  params.location_filter = LocationFilter::AbsoluteStartFilter(Location(1, 0));
  session_.Subscribe(ftn, &remote_track_visitor_, params);
  MoqtSubscribeOk ok(2, MessageParameters(),
                     TrackProperties(std::nullopt, std::nullopt, std::nullopt,
                                     kPeerDefaultPriority, std::nullopt,
                                     std::nullopt, std::nullopt));
  EXPECT_CALL(remote_track_visitor_, OnReply);
  bidi_wrapper_->ReceiveMessage(ok);
  char datagram[] = {0x01, 0x02, 0x00, 0x00, 0x80, 0x00, 0x08, 0x64,
                     0x65, 0x61, 0x64, 0x62, 0x65, 0x65, 0x66};
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment).Times(0);
  session_.OnDatagramReceived(absl::string_view(datagram, sizeof(datagram)));
}

TEST_F(MoqtSessionTest, UpdateTrackPriority) {
  EXPECT_CALL(mock_session_, GetStreamById).WillRepeatedly(Return(nullptr));
  session_.UpdateTrackPriority(4, std::nullopt, MoqtTrackPriority{0x40, 0x82});
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 4);
  // Same track, higher priority.
  session_.UpdateTrackPriority(4, MoqtTrackPriority{0x40, 0x82},
                               MoqtTrackPriority{0x40, 0x80});
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 4);
  // New track, higher priority.
  session_.UpdateTrackPriority(6, std::nullopt, MoqtTrackPriority{0x20, 0x82});
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 6);
  // Pop request ID 6 from the queue.  The subscription doesn't really exist, so
  // nothing else happens.
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  session_.OnCanCreateNewOutgoingUnidirectionalStream();
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 4);
  // There's another stream for request ID 6.
  session_.UpdateTrackPriority(6, std::nullopt, MoqtTrackPriority{0x20, 0x81});
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 6);
  // The subscriber demotes track 6. Track 4 is first now due to higher
  // publisher priority.
  session_.UpdateTrackPriority(6, MoqtTrackPriority{0x20, 0x81},
                               MoqtTrackPriority{0x40, 0x81});
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 4);
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  session_.OnCanCreateNewOutgoingUnidirectionalStream();
  // The subscription will update with the first stream. It's lower priority
  // than request ID 6.
  session_.UpdateTrackPriority(4, std::nullopt, MoqtTrackPriority{0x40, 0x82});
  EXPECT_EQ(MoqtSessionPeer::NextQueuedStreamIdToServer(&session_), 6);
}

// Helper functions to handle the many EXPECT_CALLs for FETCH processing and
// delivery.
namespace {
// Handles all the mock calls for the first object available for a FETCH.
void ExpectStreamOpen(
    webtransport::test::MockSession& session, MockFetchTask* fetch_task,
    webtransport::test::MockStream& data_stream,
    std::unique_ptr<webtransport::StreamVisitor>& stream_visitor) {
  EXPECT_CALL(session, CanOpenNextOutgoingUnidirectionalStream)
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(session, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&data_stream));
  EXPECT_CALL(data_stream, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        stream_visitor = std::move(visitor);
      });
  EXPECT_CALL(data_stream, SetPriority);
}

// Sets expectations to send one object at the start of the stream, and then
// return a different status on the second GetNextObject call. |second_result|
// cannot be kSuccess.
void ExpectSendObject(MockFetchTask* fetch_task,
                      webtransport::test::MockStream& data_stream,
                      MoqtObjectStatus status, Location location,
                      absl::string_view payload,
                      MoqtFetchTask::GetNextObjectResult second_result) {
  QUICHE_DCHECK(second_result != MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_CALL(data_stream, CanWrite).WillRepeatedly(Return(true));
  EXPECT_CALL(*fetch_task, GetNextObject)
      .WillOnce([=](PublishedObject& output) {
        output.metadata.location = location;
        output.metadata.subgroup = 0;
        output.metadata.status = status;
        output.metadata.publisher_priority = 128;
        output.metadata.payload_length = payload.length();
        output.payload = PayloadFromString(payload);
        output.fin_after_this = true;  // should be ignored.
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce([=](PublishedObject& /*output*/) { return second_result; });
  if (second_result == MoqtFetchTask::GetNextObjectResult::kEof) {
    EXPECT_CALL(data_stream, Writev)
        .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                     const webtransport::StreamWriteOptions& options) {
          quic::QuicDataReader reader(data[0].AsStringView());
          uint64_t type;
          EXPECT_TRUE(reader.ReadMoqVarInt(&type));
          EXPECT_EQ(type, MoqtDataStreamType::Fetch().value());
          EXPECT_FALSE(options.send_fin());  // fin_after_this is ignored.
          return absl::OkStatus();
        })
        .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                     const webtransport::StreamWriteOptions& options) {
          EXPECT_TRUE(data.empty());
          EXPECT_TRUE(options.send_fin());
          return absl::OkStatus();
        });
    return;
  }
  EXPECT_CALL(data_stream, Writev)
      .WillOnce([](absl::Span<quiche::QuicheMemSlice> data,
                   const webtransport::StreamWriteOptions& options) {
        quic::QuicDataReader reader(data[0].AsStringView());
        uint64_t type;
        EXPECT_TRUE(reader.ReadMoqVarInt(&type));
        EXPECT_EQ(type, MoqtDataStreamType::Fetch().value());
        EXPECT_FALSE(options.send_fin());  // fin_after_this is ignored.
        return absl::OkStatus();
      });
  if (second_result == MoqtFetchTask::GetNextObjectResult::kError) {
    EXPECT_CALL(data_stream, ResetWithUserCode);
  }
}
}  // namespace

// All callbacks are called asynchronously.
TEST_F(MoqtSessionTest, ProcessFetchGetEverythingFromUpstream) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  MockTrackPublisher* track = CreateTrackPublisher();

  // No callbacks are synchronous. MockFetchTask will store the callbacks.
  auto fetch_task_ptr = std::make_unique<MockFetchTask>();
  MockFetchTask* fetch_task = fetch_task_ptr.get();

  FetchResponseCallback fetch_response_callback;
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce([&](Location, Location, MoqtGroupOrder,
                    FetchResponseCallback callback) {
        fetch_response_callback = std::move(callback);
        return std::move(fetch_task_ptr);
      });
  bidi_wrapper_->ReceiveMessage(fetch);

  // Compose and send the FETCH_OK.
  MoqtFetchOk expected_ok(false, Location(1, 4));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_ok), _));
  std::move(fetch_response_callback)(expected_ok);

  // Data arrives.
  webtransport::test::MockStream data_stream;
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  ExpectStreamOpen(mock_session_, fetch_task, data_stream, stream_visitor);
  ExpectSendObject(fetch_task, data_stream, MoqtObjectStatus::kNormal,
                   Location(0, 0), "foo",
                   MoqtFetchTask::GetNextObjectResult::kPending);
  fetch_task->CallObjectsAvailableCallback();
}

// All callbacks are called synchronously. All relevant data is cached (or this
// is the original publisher).
TEST_F(MoqtSessionTest, ProcessFetchWholeRangeIsPresent) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  MockTrackPublisher* track = CreateTrackPublisher();

  MoqtFetchOk expected_ok(false, Location(1, 4));
  auto fetch_task_ptr = std::make_unique<MockFetchTask>(true);
  MockFetchTask* fetch_task = fetch_task_ptr.get();
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce([&](Location, Location, MoqtGroupOrder,
                    FetchResponseCallback callback) {
        std::move(callback)(expected_ok);
        return std::move(fetch_task_ptr);
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_ok), _));
  webtransport::test::MockStream data_stream;
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  ExpectStreamOpen(mock_session_, fetch_task, data_stream, stream_visitor);
  ExpectSendObject(fetch_task, data_stream, MoqtObjectStatus::kNormal,
                   Location(0, 0), "foo",
                   MoqtFetchTask::GetNextObjectResult::kPending);
  // Everything spins upon message receipt. FetchTask is generating the
  // necessary callbacks.
  bidi_wrapper_->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, SendFragmentedFetchObject) {
  using ::testing::ByMove;
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  fetch.request_id = 3;  // Use an odd ID for peer request in client session.
  MockTrackPublisher* track = CreateTrackPublisher();

  // Disable synchronous callback to have more control.
  auto fetch_task_ptr = std::make_unique<MockFetchTask>();
  MockFetchTask* fetch_task = fetch_task_ptr.get();
  FetchResponseCallback fetch_response_callback;
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce([&](Location, Location, MoqtGroupOrder,
                    FetchResponseCallback callback) {
        fetch_response_callback = std::move(callback);
        return std::move(fetch_task_ptr);
      });

  // Receive FETCH, send FETCH_OK.
  bidi_wrapper_->ReceiveMessage(fetch);
  // FETCH_OK responding to the request.
  MoqtFetchOk expected_ok(false, Location(1, 0));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_ok), _));
  std::move(fetch_response_callback)(expected_ok);

  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingUnidirectionalStream())
      .WillOnce(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        stream_visitor = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, SetPriority);
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  // Trigger stream opening (calls SetObjectAvailableCallback with lambda1).
  // Setting the stream visitor will cause a second call to the callback.
  PublishedObjectMetadata metadata = {
      Location(0, 0), 0, "", MoqtObjectStatus::kNormal, 128, true, 10};
  EXPECT_CALL(*fetch_task, GetNextObject)
      .WillOnce([&](PublishedObject& output) {
        output.metadata = metadata;
        output.payload = PayloadFromString("part1");
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillOnce(Return(MoqtFetchTask::GetNextObjectResult::kPending));
  EXPECT_CALL(mock_uni_stream_, Writev)
      .WillOnce([&](absl::Span<const quiche::QuicheMemSlice> data,
                    const webtransport::StreamWriteOptions& options) {
        EXPECT_EQ(data.size(), 2);
        EXPECT_EQ(data[1].AsStringView(), "part1");
        return absl::OkStatus();
      });
  fetch_task->CallObjectsAvailableCallback();
  // lambda1 ran, mock_uni_stream_ captured, stream_visitor set.
  ASSERT_NE(stream_visitor, nullptr);

  // The second fragment is available.
  EXPECT_CALL(*fetch_task, GetNextObject)
      .WillOnce([&](PublishedObject& output) {
        output.metadata = metadata;
        output.payload = PayloadFromString("part2");
        return MoqtFetchTask::GetNextObjectResult::kSuccess;
      })
      .WillRepeatedly(Return(MoqtFetchTask::GetNextObjectResult::kPending));
  EXPECT_CALL(mock_uni_stream_, Writev)
      .WillOnce([&](absl::Span<const quiche::QuicheMemSlice> data,
                    const webtransport::StreamWriteOptions& options) {
        EXPECT_EQ(data.size(), 1);  // No header.
        EXPECT_EQ(data[0].AsStringView(), "part2");
        return absl::OkStatus();
      });
  fetch_task->CallObjectsAvailableCallback();
}

// The publisher has the first object locally, but has to go upstream to get
// the rest.
TEST_F(MoqtSessionTest, FetchReturnsObjectBeforeOk) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  MockTrackPublisher* track = CreateTrackPublisher();

  // Object returns synchronously.
  auto fetch_task_ptr = std::make_unique<MockFetchTask>(true);
  MockFetchTask* fetch_task = fetch_task_ptr.get();
  FetchResponseCallback fetch_response_callback;
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce([&](Location, Location, MoqtGroupOrder,
                    FetchResponseCallback callback) {
        fetch_response_callback = std::move(callback);
        return std::move(fetch_task_ptr);
      });
  webtransport::test::MockStream data_stream;
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  ExpectStreamOpen(mock_session_, fetch_task, data_stream, stream_visitor);
  ExpectSendObject(fetch_task, data_stream, MoqtObjectStatus::kNormal,
                   Location(0, 0), "foo",
                   MoqtFetchTask::GetNextObjectResult::kPending);
  bidi_wrapper_->ReceiveMessage(fetch);

  MoqtFetchOk expected_ok(false, Location(1, 4));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_ok), _));
  std::move(fetch_response_callback)(expected_ok);
}

TEST_F(MoqtSessionTest, FetchReturnsObjectBeforeError) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  MockTrackPublisher* track = CreateTrackPublisher();

  auto fetch_task_ptr = std::make_unique<MockFetchTask>(true);
  MockFetchTask* fetch_task = fetch_task_ptr.get();
  FetchResponseCallback fetch_response_callback;
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce([&](Location, Location, MoqtGroupOrder,
                    FetchResponseCallback callback) {
        fetch_response_callback = std::move(callback);
        return std::move(fetch_task_ptr);
      });
  webtransport::test::MockStream data_stream;
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  ExpectStreamOpen(mock_session_, fetch_task, data_stream, stream_visitor);
  ExpectSendObject(fetch_task, data_stream, MoqtObjectStatus::kNormal,
                   Location(0, 0), "foo",
                   MoqtFetchTask::GetNextObjectResult::kPending);
  bidi_wrapper_->ReceiveMessage(fetch);

  MoqtRequestError expected_error{RequestErrorCode::kDoesNotExist, std::nullopt,
                                  "foo"};
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_error), _));
  std::move(fetch_response_callback)(expected_error);
}

TEST_F(MoqtSessionTest, InvalidFetch) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  MoqtFetch fetch = DefaultFetch();
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce(Return(std::make_unique<MockFetchTask>()));
  bidi_wrapper_->ReceiveMessage(fetch);
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation),
                           "FETCH received on stream that already has a fetch"))
      .Times(1);
  bidi_wrapper_->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, FetchFails) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  MockTrackPublisher* track = CreateTrackPublisher();

  auto fetch_task_ptr = std::make_unique<MockFetchTask>();
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce([&](Location, Location, MoqtGroupOrder,
                    FetchResponseCallback callback) {
        std::move(callback)(MoqtRequestErrorInfo(
            RequestErrorCode::kDoesNotExist, std::nullopt, "foo"));
        return std::move(fetch_task_ptr);
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
  bidi_wrapper_->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, FullFetchDeliveryWithFlowControl) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  MockTrackPublisher* track = CreateTrackPublisher();

  auto fetch_task_ptr = std::make_unique<MockFetchTask>(true);
  MockFetchTask* fetch_task = fetch_task_ptr.get();
  EXPECT_CALL(*track, StandaloneFetch)
      .WillOnce(Return(std::move(fetch_task_ptr)));

  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream())
      .WillOnce(Return(false));
  bidi_wrapper_->ReceiveMessage(fetch);

  // Stream opens, but with no credit.
  webtransport::test::MockStream data_stream;
  std::unique_ptr<webtransport::StreamVisitor> stream_visitor;
  ExpectStreamOpen(mock_session_, fetch_task, data_stream, stream_visitor);
  EXPECT_CALL(data_stream, CanWrite()).WillRepeatedly(Return(false));
  session_.OnCanCreateNewOutgoingUnidirectionalStream();
  // Object with FIN
  ExpectSendObject(fetch_task, data_stream, MoqtObjectStatus::kNormal,
                   Location(0, 0), "foo",
                   MoqtFetchTask::GetNextObjectResult::kEof);
  stream_visitor->OnCanWrite();
}

TEST_F(MoqtSessionTest, IncomingRelativeJoiningFetch) {
  MoqtSubscribe subscribe = DefaultSubscribe();
  // Give it the latest object filter.
  subscribe.parameters.location_filter = LocationFilter::NextObjectFilter();
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  SetLargestId(track, Location(4, 10));
  ReceiveSubscribeSynchronousOk(track, subscribe, bidi_wrapper_.get());

  webtransport::test::MockStream fetch_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> fetch_wrapper =
      std::make_unique<MoqtBidiStreamTestWrapper>(
          ResponseStream(kFetchByte, &fetch_stream));
  ASSERT_TRUE(MoqtSessionPeer::RequestIdIsLivePublisher(&session_,
                                                        subscribe.request_id));
  MoqtFetch fetch = DefaultFetch();
  fetch.request_id = 3;
  fetch.fetch = JoiningFetchRelative(1, 2);
  EXPECT_CALL(*track, StandaloneFetch(Location(2, 0), Location(4, 10), _, _))
      .WillOnce(Return(std::make_unique<MockFetchTask>()));
  fetch_wrapper->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, IncomingAbsoluteJoiningFetch) {
  MoqtSubscribe subscribe = DefaultSubscribe();
  // Give it the latest object filter.
  subscribe.parameters.location_filter = LocationFilter::NextObjectFilter();
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  SetLargestId(track, Location(4, 10));
  ReceiveSubscribeSynchronousOk(track, subscribe, bidi_wrapper_.get());

  ASSERT_TRUE(MoqtSessionPeer::RequestIdIsLivePublisher(&session_,
                                                        subscribe.request_id));
  webtransport::test::MockStream fetch_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> fetch_wrapper =
      std::make_unique<MoqtBidiStreamTestWrapper>(
          ResponseStream(kFetchByte, &fetch_stream));
  MoqtFetch fetch = DefaultFetch();
  fetch.request_id = 3;
  fetch.fetch = JoiningFetchAbsolute(1, 2);
  EXPECT_CALL(*track, StandaloneFetch(Location(2, 0), Location(4, 10), _, _))
      .WillOnce(Return(std::make_unique<MockFetchTask>()));
  fetch_wrapper->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, IncomingJoiningFetchBadRequestId) {
  bidi_wrapper_ =
      std::make_unique<MoqtBidiStreamTestWrapper>(ResponseStream(kFetchByte));
  MoqtFetch fetch = DefaultFetch();
  fetch.fetch = JoiningFetchRelative(1, 2);
  MoqtRequestError expected_error = {
      RequestErrorCode::kInvalidJoiningRequestId,
      /*retry_interval=*/std::nullopt,
      "Joining Fetch for non-existent request",
  };
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_error), _));
  bidi_wrapper_->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, IncomingJoiningFetchForwardZero) {
  MoqtSubscribe subscribe = DefaultSubscribe();
  subscribe.parameters.set_forward(false);
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  SetLargestId(track, Location(2, 10));
  ReceiveSubscribeSynchronousOk(track, subscribe, bidi_wrapper_.get());

  webtransport::test::MockStream fetch_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> fetch_wrapper =
      std::make_unique<MoqtBidiStreamTestWrapper>(
          ResponseStream(kFetchByte, &fetch_stream));
  MoqtFetch fetch = DefaultFetch();
  fetch.request_id = 3;
  fetch.fetch = JoiningFetchRelative(1, 2);
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation),
                           "Joining Fetch for non-forwarding subscribe"))
      .Times(1);
  fetch_wrapper->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, IncomingJoiningFetchAfterRequestUpdate) {
  MoqtSubscribe subscribe = DefaultSubscribe();
  subscribe.parameters.set_forward(false);
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  SetLargestId(track, Location(2, 10));
  MoqtObjectListener* listener =
      ReceiveSubscribeSynchronousOk(track, subscribe, bidi_wrapper_.get());
  LivePublisher* subscription = absl::down_cast<LivePublisher*>(listener);
  ASSERT_NE(subscription, nullptr);

  SetLargestId(track, Location(4, 10));
  MessageParameters update_parameters;
  update_parameters.set_forward(true);
  update_parameters.location_filter = LocationFilter::NextObjectFilter();
  MessageParameters expected_ok_parameters;
  expected_ok_parameters.largest_object = Location(4, 10);
  EXPECT_CALL(
      mock_bidi_stream_,
      Writev(SerializedControlMessage(MoqtRequestOk{expected_ok_parameters}),
             _));
  bidi_wrapper_->ReceiveMessage(MoqtRequestUpdate{3, update_parameters});
  ASSERT_TRUE(subscription->parameters().location_filter.has_value());
  EXPECT_EQ(subscription->parameters().location_filter->start(),
            Location(4, 11));

  webtransport::test::MockStream fetch_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> fetch_wrapper =
      std::make_unique<MoqtBidiStreamTestWrapper>(
          ResponseStream(kFetchByte, &fetch_stream));
  MoqtFetch fetch = DefaultFetch();
  fetch.request_id = 5;
  fetch.fetch = JoiningFetchRelative(subscribe.request_id, 2);
  EXPECT_CALL(*track, StandaloneFetch(Location(2, 0), Location(4, 10), _, _))
      .WillOnce(Return(std::make_unique<MockFetchTask>()));
  fetch_wrapper->ReceiveMessage(fetch);
}

TEST_F(MoqtSessionTest, SendJoiningFetch) {
  webtransport::test::MockStream subscribe_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> subscribe_wrapper;
  // Call in reverse order of opening, so that the EXPECT_CALLs execute in the
  // proper order.
  PrepareRequestStream(bidi_wrapper_);
  PrepareRequestStream(subscribe_wrapper, &subscribe_stream);
  MoqtSubscribe expected_subscribe(
      0, FullTrackName("foo", "bar"),
      MessageParameters(LocationFilter::NextObjectFilter()));
  MoqtFetch expected_fetch(2, JoiningFetchRelative(0, 1), MessageParameters());
  EXPECT_CALL(subscribe_stream,
              Writev(SerializedControlMessage(expected_subscribe), _));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_fetch), _));
  EXPECT_NE(session_.RelativeJoiningFetch(expected_subscribe.full_track_name,
                                          &remote_track_visitor_, nullptr, 1,
                                          MessageParameters()),
            nullptr);
}

TEST_F(MoqtSessionTest, SendJoiningFetchNoFlowControl) {
  webtransport::test::MockStream subscribe_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> subscribe_wrapper;
  // Call in reverse order of opening, so that the EXPECT_CALLs execute in the
  // proper order.
  PrepareRequestStream(bidi_wrapper_);
  PrepareRequestStream(subscribe_wrapper, &subscribe_stream);
  EXPECT_CALL(subscribe_stream,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kFetch), _));
  EXPECT_TRUE(session_.RelativeJoiningFetch(FullTrackName("foo", "bar"),
                                            &remote_track_visitor_, 0,
                                            MessageParameters()));
  EXPECT_CALL(remote_track_visitor_, OnReply).Times(1);
  MessageParameters parameters;
  parameters.largest_object = Location(2, 0);
  subscribe_wrapper->ReceiveMessage(
      MoqtSubscribeOk(0, parameters, TrackProperties()));
  bidi_wrapper_->ReceiveMessage(MoqtFetchOk(
      false, Location(2, 0), MessageParameters(), TrackProperties()));
  // Packet arrives on FETCH stream.
  MoqtObject object = {
      /*request_id=*/2,
      /*group_id, object_id=*/2,
      0,
      /*publisher_priority=*/128,
      /*properties=*/"",
      /*status=*/MoqtObjectStatus::kNormal,
      /*subgroup=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/3,
  };
  MoqtFramer framer(true, quic::Perspective::IS_SERVER);
  std::optional<PublishedObjectMetadata> metadata;
  quiche::QuicheBuffer header = framer.SerializeObjectHeader(
      object, MoqtDataStreamType::Fetch(), metadata);
  webtransport::test::InMemoryStream data_stream(kIncomingUniStreamId);
  data_stream.SetVisitor(
      MoqtSessionPeer::CreateIncomingStreamVisitor(&session_, &data_stream));
  data_stream.Receive(header.AsStringView(), false);
  EXPECT_CALL(remote_track_visitor_, OnObjectFragment).Times(1);
  data_stream.Receive("foo", false);
}

TEST_F(MoqtSessionTest, IncomingSubscribeNamespace) {
  TrackNamespace prefix{"foo"};
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");

  MoqtSubscribeNamespace subscribe_namespace = {/*request_id=*/1, prefix,
                                                parameters};
  quiche::QuicheWeakPtr<MockNamespaceTask> task;
  MessageParameters ok_params;
  ok_params.expires = quic::QuicTimeDelta::FromSeconds(60);
  EXPECT_CALL(session_callbacks_.incoming_subscribe_namespace_callback,
              Call(prefix, parameters, _))
      .WillOnce([&](const TrackNamespace& prefix, const MessageParameters&,
                    MoqtResponseCallback response_callback) {
        std::move(response_callback)(ok_params);
        auto task_ptr = std::make_unique<MockNamespaceTask>(prefix);
        task = task_ptr->GetWeakPtr();
        return task_ptr;
      });
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeNamespaceByte));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  bidi_wrapper_->ReceiveMessage(subscribe_namespace);

  // Deliver a NAMESPACE
  ASSERT_TRUE(task.IsValid());
  EXPECT_CALL(*task.GetIfAvailable(), GetNextSuffix)
      .WillOnce([](TrackNamespace& prefix, TransactionType& type) {
        prefix = TrackNamespace({"bar"});
        type = TransactionType::kAdd;
        return GetNextResult::kSuccess;
      })
      .WillOnce(Return(GetNextResult::kPending));
  MoqtNamespace expected_namespace = {
      TrackNamespace({"bar"}),
  };
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_namespace), _))
      .WillOnce(Return(absl::OkStatus()));
  task.GetIfAvailable()->InvokeCallback();

  // Unsubscribe
  bidi_wrapper_->stream().OnResetStreamReceived(kResetCodeCancelled);
  EXPECT_FALSE(task.IsValid());
  bidi_wrapper_.reset();

  // Resubscribing to the same prefix should succeed.
  subscribe_namespace.request_id += 2;
  EXPECT_CALL(session_callbacks_.incoming_subscribe_namespace_callback,
              Call(prefix, parameters, _))
      .WillOnce([&](const TrackNamespace& prefix, const MessageParameters&,
                    MoqtResponseCallback response_callback) {
        std::move(response_callback)(ok_params);
        auto task_ptr = std::make_unique<MockNamespaceTask>(prefix);
        task = task_ptr->GetWeakPtr();
        return task_ptr;
      });
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeNamespaceByte));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _))
      .WillOnce(Return(absl::OkStatus()));
  bidi_wrapper_->ReceiveMessage(subscribe_namespace);
  EXPECT_TRUE(task.IsValid());
}

TEST_F(MoqtSessionTest, IncomingSubscribeNamespaceWithSynchronousError) {
  TrackNamespace prefix{"foo"};
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeNamespaceByte));
  MoqtSubscribeNamespace subscribe_namespace = {/*request_id=*/1, prefix,
                                                parameters};
  EXPECT_CALL(session_callbacks_.incoming_subscribe_namespace_callback,
              Call(prefix, parameters, _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters&,
                    MoqtResponseCallback response_callback) {
        std::move(response_callback)(MoqtRequestErrorInfo{
            RequestErrorCode::kUnauthorized, std::nullopt, "foo"});
        return nullptr;
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  bidi_wrapper_->ReceiveMessage(subscribe_namespace);
}

TEST_F(MoqtSessionTest, IncomingSubscribeNamespaceWithPrefixOverlap) {
  TrackNamespace foo{"foo"}, foobar{"foo", "bar"};
  MessageParameters parameters;
  parameters.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                               "foo");
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeNamespaceByte));
  MoqtSubscribeNamespace subscribe_namespace = {/*request_id=*/1, foo,
                                                parameters};
  EXPECT_CALL(session_callbacks_.incoming_subscribe_namespace_callback,
              Call(foo, parameters, _))
      .WillOnce([&](const TrackNamespace& prefix, const MessageParameters&,
                    MoqtResponseCallback response_callback) {
        std::move(response_callback)(MessageParameters());
        auto task_ptr = std::make_unique<MockNamespaceTask>(prefix);
        return task_ptr;
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestOk), _))
      .WillOnce(Return(absl::OkStatus()));
  bidi_wrapper_->ReceiveMessage(subscribe_namespace);

  subscribe_namespace.request_id += 2;
  subscribe_namespace.track_namespace_prefix = foobar;
  webtransport::test::MockStream bidi_stream_2;
  auto bidi_wrapper_2 = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeNamespaceByte, &bidi_stream_2));
  EXPECT_CALL(bidi_stream_2,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  bidi_wrapper_2->ReceiveMessage(subscribe_namespace);
}

TEST_F(MoqtSessionTest, FetchThenOkThenCancel) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kFetch), _));
  bool fetch_ok = false;
  std::unique_ptr<MoqtFetchTask> fetch_task = session_.Fetch(
      FullTrackName("foo", "bar"),
      [&](std::variant<FetchOkData, MoqtRequestErrorInfo> res) {
        fetch_ok = std::holds_alternative<FetchOkData>(res);
      },
      Location(0, 0), 4, std::nullopt, MessageParameters());
  ASSERT_NE(fetch_task, nullptr);
  MoqtFetchOk ok(/*end_of_track=*/false, Location(3, 25));
  bidi_wrapper_->ReceiveMessage(ok);
  EXPECT_TRUE(fetch_ok);
  EXPECT_TRUE(fetch_task->GetStatus().ok());
  PublishedObject object;
  EXPECT_EQ(fetch_task->GetNextObject(object),
            MoqtFetchTask::GetNextObjectResult::kPending);
  // Cancel the fetch.
  EXPECT_CALL(mock_bidi_stream_, SendStopSending(kResetCodeCancelled));
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode(kResetCodeCancelled));
  fetch_task.reset();
}

TEST_F(MoqtSessionTest, FetchThenError) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kFetch), _));
  MoqtRequestError error(RequestErrorCode::kUnauthorized,
                         /*retry_interval=*/std::nullopt,
                         "No username provided");
  std::unique_ptr<MoqtFetchTask> fetch_task = session_.Fetch(
      FullTrackName("foo", "bar"),
      [&](std::variant<FetchOkData, MoqtRequestErrorInfo> res) {
        EXPECT_EQ(std::get<MoqtRequestErrorInfo>(res), error);
      },
      Location(0, 0), 4, std::nullopt, MessageParameters());
  ASSERT_NE(fetch_task, nullptr);
  ExpectFin(mock_bidi_stream_);
  bidi_wrapper_->ReceiveMessage(error);
}

// The application takes objects as they arrive.
TEST_F(MoqtSessionTest, IncomingFetchObjectsGreedyApp) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kFetch), _));
  uint64_t expected_object_id = 0;
  bool fetch_ok = false;
  std::unique_ptr<MoqtFetchTask> fetch_task = session_.Fetch(
      FullTrackName("foo", "bar"),
      [&](std::variant<FetchOkData, MoqtRequestErrorInfo> res) {
        fetch_ok = std::holds_alternative<FetchOkData>(res);
      },
      Location(0, 0), 4, std::nullopt, MessageParameters());
  ASSERT_NE(fetch_task, nullptr);
  fetch_task->SetObjectAvailableCallback([&]() {
    PublishedObject object;
    MoqtFetchTask::GetNextObjectResult result;
    do {
      result = fetch_task->GetNextObject(object);
      if (result == MoqtFetchTask::GetNextObjectResult::kSuccess) {
        EXPECT_EQ(object.metadata.location.object, expected_object_id);
        ++expected_object_id;
      }
    } while (result == MoqtFetchTask::GetNextObjectResult::kSuccess);
  });
  // Build queue of packets to arrive.
  std::queue<quiche::QuicheBuffer> headers;
  std::queue<std::string> payloads;
  MoqtObject object = {
      /*request_id=*/0,
      /*group_id, object_id=*/0,
      0,
      /*publisher_priority=*/128,
      /*properties=*/"",
      /*status=*/MoqtObjectStatus::kNormal,
      /*subgroup=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/3,
  };
  MoqtFramer framer(true, quic::Perspective::IS_SERVER);
  std::optional<PublishedObjectMetadata> metadata;
  for (int i = 0; i < 4; ++i) {
    object.object_id = i;
    headers.push(framer.SerializeObjectHeader(
        object, MoqtDataStreamType::Fetch(), metadata));
    metadata = PublishedObjectMetadata();
    metadata->location = Location(0, i);
    metadata->subgroup = 0;
    metadata->publisher_priority = 128;
    payloads.push("foo");
  }

  // Open stream, deliver two objects before FETCH_OK.
  webtransport::test::InMemoryStream data_stream(kIncomingUniStreamId);
  data_stream.SetVisitor(
      MoqtSessionPeer::CreateIncomingStreamVisitor(&session_, &data_stream));
  for (int i = 0; i < 2; ++i) {
    data_stream.Receive(headers.front().AsStringView(), false);
    data_stream.Receive(payloads.front(), false);
    headers.pop();
    payloads.pop();
  }

  // FETCH_OK arrives, objects are delivered.
  MoqtFetchOk ok(/*end_of_track=*/false, Location(3, 25));
  bidi_wrapper_->ReceiveMessage(ok);
  EXPECT_TRUE(fetch_ok);
  EXPECT_EQ(expected_object_id, 2);

  // Deliver the rest of the objects.
  for (int i = 2; i < 4; ++i) {
    data_stream.Receive(headers.front().AsStringView(), false);
    data_stream.Receive(payloads.front(), false);
    headers.pop();
    payloads.pop();
  }
  EXPECT_EQ(expected_object_id, 4);
}

TEST_F(MoqtSessionTest, IncomingFetchObjectsSlowApp) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kFetch), _));
  uint64_t expected_object_id = 0;
  bool objects_available = false;
  bool fetch_ok = false;
  std::unique_ptr<MoqtFetchTask> fetch_task = session_.Fetch(
      FullTrackName("foo", "bar"),
      [&](std::variant<FetchOkData, MoqtRequestErrorInfo> res) {
        fetch_ok = std::holds_alternative<FetchOkData>(res);
      },
      Location(0, 0), 4, std::nullopt, MessageParameters());
  ASSERT_NE(fetch_task, nullptr);
  fetch_task->SetObjectAvailableCallback([&]() { objects_available = true; });
  // Build queue of packets to arrive.
  std::queue<quiche::QuicheBuffer> headers;
  std::queue<std::string> payloads;
  MoqtObject object = {
      /*request_id=*/0,
      /*group_id, object_id=*/0,
      0,
      /*publisher_priority=*/128,
      /*properties=*/"",
      /*status=*/MoqtObjectStatus::kNormal,
      /*subgroup=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/3,
  };
  MoqtFramer framer(true, quic::Perspective::IS_SERVER);
  std::optional<PublishedObjectMetadata> metadata;
  for (int i = 0; i < 4; ++i) {
    object.object_id = i;
    headers.push(framer.SerializeObjectHeader(
        object, MoqtDataStreamType::Fetch(), metadata));
    metadata = PublishedObjectMetadata();
    metadata->location = Location(0, i);
    metadata->subgroup = 0;
    metadata->publisher_priority = 128;
    payloads.push("foo");
  }

  // Open stream, deliver two objects before FETCH_OK.
  webtransport::test::InMemoryStream data_stream(kIncomingUniStreamId);
  data_stream.SetVisitor(
      MoqtSessionPeer::CreateIncomingStreamVisitor(&session_, &data_stream));
  for (int i = 0; i < 2; ++i) {
    data_stream.Receive(headers.front().AsStringView(), false);
    data_stream.Receive(payloads.front(), false);
    headers.pop();
    payloads.pop();
  }
  EXPECT_TRUE(objects_available);

  // FETCH_OK arrives, objects are available.
  MoqtFetchOk ok(/*end_of_track=*/false, Location(3, 25));
  bidi_wrapper_->ReceiveMessage(ok);
  EXPECT_TRUE(fetch_ok);
  EXPECT_TRUE(objects_available);

  // Get the objects
  MoqtFetchTask::GetNextObjectResult result;
  do {
    PublishedObject new_object;
    result = fetch_task->GetNextObject(new_object);
    if (result == MoqtFetchTask::GetNextObjectResult::kSuccess) {
      EXPECT_EQ(new_object.metadata.location.object, expected_object_id);
      ++expected_object_id;
    }
  } while (result == MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_EQ(expected_object_id, 2);
  objects_available = false;

  // Deliver the rest of the objects.
  for (int i = 2; i < 4; ++i) {
    data_stream.Receive(headers.front().AsStringView(), false);
    data_stream.Receive(payloads.front(), false);
    headers.pop();
    payloads.pop();
  }
  EXPECT_TRUE(objects_available);
  EXPECT_EQ(expected_object_id, 2);  // Not delivered yet.
  // Get the objects
  do {
    PublishedObject new_object;
    result = fetch_task->GetNextObject(new_object);
    if (result == MoqtFetchTask::GetNextObjectResult::kSuccess) {
      EXPECT_EQ(new_object.metadata.location.object, expected_object_id);
      ++expected_object_id;
    }
  } while (result == MoqtFetchTask::GetNextObjectResult::kSuccess);
  EXPECT_EQ(expected_object_id, 4);
}

TEST_F(MoqtSessionTest, DeliveryTimeoutParameter) {
  MoqtSubscribe request = DefaultSubscribe();
  request.parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromSeconds(1);
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MockTrackPublisher* track = CreateTrackPublisher();
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());
  std::optional<quic::QuicTimeDelta> delivery_timeout =
      MoqtSessionPeer::GetObjectDeliveryTimeout(&session_, request.request_id);
  EXPECT_TRUE(delivery_timeout.has_value() &&
              *delivery_timeout == quic::QuicTimeDelta::FromSeconds(1));
}

TEST_F(MoqtSessionTest, ReceiveGoAwayEnforcement) {
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  EXPECT_CALL(session_callbacks_.goaway_received_callback,
              Call("", quic::QuicTimeDelta::Zero()));
  bidi_wrapper_->ReceiveMessage(MoqtGoAway("", quic::QuicTimeDelta::Zero()));
  // New requests not allowed.
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);
  MessageParameters parameters = SubscribeForTest();
  parameters.location_filter = LocationFilter::NextObjectFilter();
  EXPECT_FALSE(session_.Subscribe(FullTrackName("foo", "bar"),
                                  &remote_track_visitor_, parameters));
  TrackNamespace prefix({"foo"});
  EXPECT_EQ(session_.SubscribeNamespace(
                prefix, MessageParameters(),
                +[](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}),
            nullptr);
  EXPECT_FALSE(session_.PublishNamespace(
      TrackNamespace{"foo"}, MessageParameters(),
      +[](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}, +[]() {}));
  EXPECT_EQ(session_.Fetch(
                FullTrackName{TrackNamespace({"foo"}), "bar"},
                +[](std::variant<FetchOkData, MoqtRequestErrorInfo>) {},
                Location(0, 0), 5, std::nullopt, MessageParameters()),
            nullptr);
  // Error on additional GOAWAY.
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<uint64_t>(MoqtError::kProtocolViolation),
                           "Received multiple GOAWAY on control stream"))
      .Times(1);
  bool reported_error = false;
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call)
      .WillOnce([&](absl::string_view error_message) {
        reported_error = true;
        EXPECT_EQ(error_message, "Received multiple GOAWAY on control stream");
      });
  bidi_wrapper_->ReceiveMessage(MoqtGoAway{"", quic::QuicTimeDelta::Zero()});
}

TEST_F(MoqtSessionTest, SendGoAwayEnforcement) {
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  CreateTrackPublisher();
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kGoAway), _));
  session_.GoAway("");

  // All new bidi streams types are immediately rejected.
  webtransport::test::MockStream new_request_stream;
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream)
      .WillOnce(Return(&new_request_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(new_request_stream, CanWrite()).WillOnce(Return(true));
  EXPECT_CALL(new_request_stream,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _))
      .WillOnce([](absl::Span<quiche::QuicheMemSlice>,
                   const webtransport::StreamWriteOptions& options) {
        EXPECT_TRUE(options.send_fin());
        return absl::OkStatus();
      });
  session_.OnIncomingBidirectionalStreamAvailable();

  // If a new stream can't be written, reset it.
  webtransport::test::MockStream new_request_stream_2;
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream)
      .WillOnce(Return(&new_request_stream_2))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(new_request_stream_2, CanWrite()).WillOnce(Return(false));
  EXPECT_CALL(new_request_stream_2, SendStopSending);
  EXPECT_CALL(new_request_stream_2, ResetWithUserCode);
  session_.OnIncomingBidirectionalStreamAvailable();

  // Block all outgoing PUBLISH_NAMESPACE, GOAWAY,etc.
  MessageParameters parameters = SubscribeForTest();
  parameters.location_filter = LocationFilter::NextObjectFilter();
  EXPECT_FALSE(session_.Subscribe(FullTrackName({"foo"}, "bar"),
                                  &remote_track_visitor_, parameters));
  TrackNamespace prefix({"foo"});
  EXPECT_EQ(session_.SubscribeNamespace(
                prefix, MessageParameters(),
                +[](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}),
            nullptr);
  EXPECT_FALSE(session_.PublishNamespace(
      TrackNamespace{"foo"}, MessageParameters(),
      +[](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}, +[]() {}));
  EXPECT_EQ(session_.Fetch(
                FullTrackName(TrackNamespace({"foo"}), "bar"),
                +[](std::variant<FetchOkData, MoqtRequestErrorInfo>) {},
                Location(0, 0), 5, std::nullopt, MessageParameters()),
            nullptr);
  session_.GoAway("");
  // GoAway timer fires.
  auto* goaway_alarm =
      absl::down_cast<quic::test::MockAlarmFactory::TestAlarm*>(
          MoqtSessionPeer::GetGoAwayTimeoutAlarm(&session_));
  EXPECT_CALL(mock_session_,
              CloseSession(static_cast<webtransport::SessionErrorCode>(
                               MoqtError::kGoawayTimeout),
                           _));
  goaway_alarm->Fire();
}

TEST_F(MoqtSessionTest, ClientCannotSendNewSessionUri) {
  // session_ is a client session.
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  // Client GOAWAY not sent.
  EXPECT_CALL(mock_bidi_stream_, Writev).Times(0);
  session_.GoAway("foo");
}

TEST_F(MoqtSessionTest, IncomingTrackStatusBeforeSetup) {
  MoqtSessionParameters session_parameters(quic::Perspective::IS_SERVER);
  MoqtSession server_session(&mock_session_, session_parameters,
                             std::make_unique<quic::test::TestAlarmFactory>(),
                             session_callbacks_.AsSessionCallbacks());
  server_session.set_publisher(&publisher_);
  MockTrackPublisher* track = CreateTrackPublisher();

  // Receive TRACK_STATUS on an incoming bidirectional stream.
  webtransport::test::InMemoryStreamWithWriteBuffer bidi_stream(0);
  MoqtFramer client_framer(session_parameters.using_webtrans,
                           quic::Perspective::IS_CLIENT);
  MoqtTrackStatus track_status = DefaultLocalTrackStatus();
  quiche::QuicheBuffer serialized_track_status =
      client_framer.SerializeTrackStatus(track_status);
  bidi_stream.Receive(serialized_track_status.AsStringView(),
                      /*fin=*/false);

  // Before SETUP is received, the incoming bidirectional stream is not
  // accepted.
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream).Times(0);
  EXPECT_CALL(*track, AddObjectListener).Times(0);
  server_session.OnIncomingBidirectionalStreamAvailable();

  // Receive CLIENT_SETUP on an incoming unidirectional stream.
  webtransport::test::InMemoryStreamWithWriteBuffer control_stream(1);
  MoqtSetup setup;
  session_parameters.ToSetupOptions(setup.options);
  quiche::QuicheBuffer setup_buffer = client_framer.SerializeSetup(setup);
  control_stream.Receive(
      absl::string_view(setup_buffer.data(), setup_buffer.size()),
      /*fin=*/false);

  MoqtObjectListener* listener = nullptr;
  EXPECT_CALL(session_callbacks_.session_established_callback, Call);
  EXPECT_CALL(mock_session_, AcceptIncomingUnidirectionalStream)
      .WillOnce(Return(&control_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(mock_session_, AcceptIncomingBidirectionalStream)
      .WillOnce(Return(&bidi_stream))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce(testing::SaveArg<0>(&listener));

  server_session.OnIncomingUnidirectionalStreamAvailable();
  EXPECT_NE(listener, nullptr);
}

TEST_F(MoqtSessionTest, IncomingTrackStatusThenSynchronousOk) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kTrackStatusByte));
  auto* track = CreateTrackPublisher();

  MoqtTrackStatus track_status = DefaultTrackStatus();
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
        EXPECT_CALL(*track, largest_location)
            .WillRepeatedly(Return(Location(5, 30)));
        MoqtRequestOk expected_ok;
        expected_ok.parameters.largest_object = Location(5, 30);
        EXPECT_CALL(mock_bidi_stream_,
                    Writev(SerializedControlMessage(expected_ok), _));
        EXPECT_CALL(*track, RemoveObjectListener);
        listener->OnSubscribeAccepted();
      });
  bidi_wrapper_->ReceiveMessage(track_status);
}

TEST_F(MoqtSessionTest, IncomingTrackStatusThenAsynchronousOk) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kTrackStatusByte));
  auto* track = CreateTrackPublisher();

  MoqtTrackStatus track_status = DefaultTrackStatus();
  MoqtObjectListener* listener = nullptr;
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce(testing::SaveArg<0>(&listener));
  bidi_wrapper_->ReceiveMessage(track_status);
  ASSERT_NE(listener, nullptr);
  EXPECT_CALL(*track, largest_location).WillRepeatedly(Return(Location(5, 30)));
  MoqtRequestOk expected_ok;
  expected_ok.parameters.largest_object = Location(5, 30);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_ok), _));
  EXPECT_CALL(*track, RemoveObjectListener(listener));
  listener->OnSubscribeAccepted();
}

TEST_F(MoqtSessionTest, IncomingTrackStatusThenSynchronousError) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kTrackStatusByte));
  auto* track = CreateTrackPublisher();

  MoqtTrackStatus track_status = DefaultTrackStatus();
  bool executed_AddObjectListener = false;
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce([&](MoqtObjectListener* listener, const MessageParameters&) {
        EXPECT_CALL(
            mock_bidi_stream_,
            Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
        EXPECT_CALL(*track, RemoveObjectListener);
        listener->OnSubscribeRejected(MoqtRequestErrorInfo(
            RequestErrorCode::kInternalError, std::nullopt, "Test error"));
        executed_AddObjectListener = true;
      });
  bidi_wrapper_->ReceiveMessage(track_status);
  EXPECT_TRUE(executed_AddObjectListener);
}

TEST_F(MoqtSessionTest, IncomingTrackStatusThenAsynchronousError) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kTrackStatusByte));
  auto* track = CreateTrackPublisher();

  MoqtTrackStatus track_status = DefaultTrackStatus();
  MoqtObjectListener* listener;
  EXPECT_CALL(*track, AddObjectListener)
      .WillOnce(testing::SaveArg<0>(&listener));
  bidi_wrapper_->ReceiveMessage(track_status);
  ASSERT_NE(listener, nullptr);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
  EXPECT_CALL(*track, RemoveObjectListener(listener));
  listener->OnSubscribeRejected(MoqtRequestErrorInfo(
      RequestErrorCode::kInternalError, std::nullopt, "Test error"));
}

TEST_F(MoqtSessionTest, FinReportedToVisitor) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters = SubscribeForTest();
  parameters.location_filter = LocationFilter::NextObjectFilter();
  EXPECT_TRUE(session_.Subscribe(FullTrackName("foo", "bar"),
                                 &remote_track_visitor_, parameters));
  MoqtSubscribeOk ok(/*track_alias=*/2, MessageParameters(), TrackProperties());
  EXPECT_CALL(remote_track_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName& ftn,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            EXPECT_EQ(ftn, FullTrackName("foo", "bar"));
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(response));
          });
  bidi_wrapper_->ReceiveMessage(ok);
  MoqtObject object = {
      /*track_alias=*/2,
      /*group_id=*/0,
      /*object_id=*/0,
      /*publisher_priority=*/7,
      /*properties=*/"",
      /*object_status=*/MoqtObjectStatus::kEndOfGroup,
      /*subgroup_id=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/0,
  };
  EXPECT_CALL(mock_uni_stream_, GetStreamId())
      .WillRepeatedly(Return(kIncomingUniStreamId));
  EXPECT_CALL(remote_track_visitor_,
              OnStreamFin(FullTrackName("foo", "bar"), DataStreamIndex(0, 0)));
  std::unique_ptr<webtransport::StreamVisitor> data_stream;
  DeliverObject(object, /*fin=*/true, mock_session_, &mock_uni_stream_,
                data_stream, &remote_track_visitor_);
  data_stream.reset();
}

TEST_F(MoqtSessionTest, ResetReportedToVisitor) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters = SubscribeForTest();
  parameters.location_filter = LocationFilter::NextObjectFilter();
  EXPECT_TRUE(session_.Subscribe(FullTrackName("foo", "bar"),
                                 &remote_track_visitor_, parameters));
  MoqtSubscribeOk ok(/*track_alias=*/2, MessageParameters(), TrackProperties());
  EXPECT_CALL(remote_track_visitor_, OnReply)
      .WillOnce(
          [&](const FullTrackName& ftn,
              std::variant<SubscribeOkData, MoqtRequestErrorInfo> response) {
            EXPECT_EQ(ftn, FullTrackName("foo", "bar"));
            EXPECT_TRUE(std::holds_alternative<SubscribeOkData>(response));
          });
  bidi_wrapper_->ReceiveMessage(ok);
  MoqtObject object = {
      /*track_alias=*/2,
      /*group_id=*/0,
      /*object_id=*/0,
      /*publisher_priority=*/7,
      /*properties=*/"",
      /*object_status=*/MoqtObjectStatus::kEndOfGroup,
      /*subgroup_id=*/0,
      /*first_object_in_subgroup=*/true,
      /*payload_length=*/0,
  };
  EXPECT_CALL(mock_uni_stream_, GetStreamId())
      .WillRepeatedly(Return(kIncomingUniStreamId));
  EXPECT_CALL(mock_session_, GetStreamById(kIncomingUniStreamId))
      .WillRepeatedly(Return(&mock_uni_stream_));
  std::unique_ptr<webtransport::StreamVisitor> data_stream;
  DeliverObject(object, /*fin=*/false, mock_session_, &mock_uni_stream_,
                data_stream, &remote_track_visitor_);
  // The data stream died and notified the visitor (IncomingDataStream).
  EXPECT_CALL(remote_track_visitor_, OnStreamReset(FullTrackName("foo", "bar"),
                                                   DataStreamIndex(0, 0)));
  data_stream->OnResetStreamReceived(kResetCodeCancelled);
  data_stream.reset();
}

TEST_F(MoqtSessionTest, IncomingPublishNamespaceCleanup) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kPublishNamespaceByte));
  // Register two incoming PUBLISH_NAMESPACE.
  MoqtPublishNamespace publish_namespace{
      /*request_id=*/1, TrackNamespace{"foo"}, MessageParameters()};
  MessageParameters ok_params;
  ok_params.expires = quic::QuicTimeDelta::FromSeconds(60);
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, _, _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters*,
                    MoqtResponseCallback callback) {
        std::move(callback)(ok_params);
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(MoqtRequestOk()), _));
  bidi_wrapper_->ReceiveMessage(publish_namespace);

  auto bidi_wrapper_2 = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kPublishNamespaceByte));
  publish_namespace = MoqtPublishNamespace(
      /*request_id=*/3, TrackNamespace{"bar"}, MessageParameters());
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"bar"}, _, _))
      .WillOnce([&](const TrackNamespace&, const MessageParameters*,
                    MoqtResponseCallback callback) {
        std::move(callback)(MessageParameters());
      });
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestOk), _));
  bidi_wrapper_2->ReceiveMessage(publish_namespace);

  // Revoke "bar"
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"bar"}, IsNull(), IsNull()));
  bidi_wrapper_2->stream().OnResetStreamReceived(kResetCodeCancelled);

  // Destroying the session should revoke "foo".
  EXPECT_CALL(session_callbacks_.incoming_publish_namespace_callback,
              Call(TrackNamespace{"foo"}, IsNull(), IsNull()));
  // Test teardown will destroy session_, triggering removal of "foo".
}

TEST_F(MoqtSessionTest, WrongSubprotocol) {
  EXPECT_CALL(mock_session_, GetNegotiatedSubprotocol)
      .WillOnce(
          Return(std::optional<std::string>(kUnrecognizedVersionForTests)));
  EXPECT_CALL(mock_session_, CloseSession);
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call);
  session_.OnSessionReady();
}

TEST_F(MoqtSessionTest, NoSubprotocol) {
  EXPECT_CALL(mock_session_, GetNegotiatedSubprotocol)
      .WillOnce(Return(std::optional<std::string>()));
  EXPECT_CALL(mock_session_, CloseSession);
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call);
  session_.OnSessionReady();
}

TEST_F(MoqtSessionTest, NamespaceNotAllowedOnControlStream) {
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  EXPECT_CALL(mock_session_, CloseSession);
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call);
  bidi_wrapper_->ReceiveMessage(MoqtNamespace());
}

TEST_F(MoqtSessionTest, NamespaceDoneNotAllowedOnControlStream) {
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  EXPECT_CALL(mock_session_, CloseSession);
  EXPECT_CALL(session_callbacks_.session_terminated_callback, Call);
  bidi_wrapper_->ReceiveMessage(MoqtNamespaceDone());
}

TEST_F(MoqtSessionTest, IncomingRequestUpdateTriggersRequestOk) {
  MoqtSubscribe subscribe = DefaultSubscribe();
  MockTrackPublisher* track = CreateTrackPublisher();
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  ReceiveSubscribeSynchronousOk(track, subscribe, bidi_wrapper_.get(), 0);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestOk), _));
  bidi_wrapper_->ReceiveMessage(MoqtRequestUpdate{3, MessageParameters()});
}

TEST_F(MoqtSessionTest, IncomingRequestUpdateTriggersRequestError) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kRequestError), _));
  EXPECT_QUICHE_BUG(
      bidi_wrapper_->ReceiveMessage(MoqtRequestUpdate{3, MessageParameters()}),
      "Received REQUEST_UPDATE, no subscription state");
}

TEST_F(MoqtSessionTest, StopSendingBlocksSubgroup) {
  MoqtSubscribe subscribe = DefaultSubscribe();
  MockTrackPublisher* track = CreateTrackPublisher();
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MoqtObjectListener* listener =
      ReceiveSubscribeSynchronousOk(track, subscribe, bidi_wrapper_.get(), 0);
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingUnidirectionalStream)
      .WillOnce(Return(&mock_uni_stream_));
  std::unique_ptr<webtransport::StreamVisitor> data_stream_visitor;
  EXPECT_CALL(mock_uni_stream_, SetVisitor)
      .WillOnce([&](std::unique_ptr<webtransport::StreamVisitor> visitor) {
        data_stream_visitor = std::move(visitor);
      });
  EXPECT_CALL(mock_uni_stream_, GetStreamId())
      .WillRepeatedly(Return(kOutgoingUniStreamId));
  EXPECT_CALL(mock_session_, GetStreamById(kOutgoingUniStreamId))
      .WillRepeatedly(Return(&mock_uni_stream_));
  EXPECT_CALL(mock_uni_stream_, visitor).WillRepeatedly([&]() {
    return data_stream_visitor.get();
  });
  EXPECT_CALL(mock_uni_stream_, CanWrite).WillRepeatedly(Return(true));
  EXPECT_CALL(*track, GetCachedObject(0, Optional(1), 0, 0))
      .WillOnce(Return(PublishedObject{
          PublishedObjectMetadata{Location(0, 0), 1, "",
                                  MoqtObjectStatus::kNormal, 0x80, true, 0,
                                  MoqtSessionPeer::Now(&session_)},
          PayloadFromString(""), false}));
  EXPECT_CALL(*track, GetCachedObject(0, Optional(1), 1, 0))
      .WillOnce(Return(std::nullopt));
  SetLargestId(track, Location(0, 0));
  EXPECT_CALL(mock_uni_stream_, Writev).WillOnce(Return(absl::OkStatus()));
  listener->OnNewObjectAvailable(Location(0, 0), 1, 0x80);

  EXPECT_CALL(mock_uni_stream_, ResetWithUserCode(kResetCodeCancelled));
  data_stream_visitor->OnStopSendingReceived(kResetCodeCancelled);
  // New object in the same subgroup should not be sent.
  EXPECT_CALL(*track, GetCachedObject).Times(0);
  EXPECT_CALL(mock_uni_stream_, Writev).Times(0);
  listener->OnNewObjectAvailable(Location(0, 1), 1, 0x80);
}

TEST_F(MoqtSessionTest, PublishSuccess) {
  CreateTrackPublisher();
  std::shared_ptr<MoqtTrackPublisher> track_publisher =
      publisher_.GetTrack(kDefaultTrackName());

  PrepareRequestStream(bidi_wrapper_);
  // Verify PUBLISH message is sent on the publish stream.
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));

  std::optional<std::variant<MessageParameters, MoqtRequestErrorInfo>> response;
  ASSERT_TRUE(session_.Publish(
      track_publisher, MessageParameters(), TrackProperties(),
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> resp) {
        response = resp;
      }));

  MoqtRequestOk request_ok;
  request_ok.parameters.object_delivery_timeout =
      quic::QuicTimeDelta::FromSeconds(2);
  bidi_wrapper_->ReceiveMessage(request_ok);

  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(std::holds_alternative<MessageParameters>(*response));
  EXPECT_EQ(std::get<MessageParameters>(*response).object_delivery_timeout,
            quic::QuicTimeDelta::FromSeconds(2));
}

TEST_F(MoqtSessionTest, PublishCannotOpenStream) {
  CreateTrackPublisher();
  std::shared_ptr<MoqtTrackPublisher> track_publisher =
      publisher_.GetTrack(kDefaultTrackName());
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingBidirectionalStream())
      .WillOnce(Return(false));
  EXPECT_FALSE(session_.Publish(
      track_publisher, MessageParameters(), TrackProperties(),
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> response) {}));
}

TEST_F(MoqtSessionTest, PublishAfterGoaway) {
  bidi_wrapper_ =
      MoqtSessionPeer::CreateControlStream(&session_, &mock_bidi_stream_);
  MoqtGoAway goaway;
  goaway.new_session_uri = "";
  bidi_wrapper_->ReceiveMessage(goaway);
  CreateTrackPublisher();
  std::shared_ptr<MoqtTrackPublisher> track_publisher =
      publisher_.GetTrack(kDefaultTrackName());
  EXPECT_FALSE(session_.Publish(
      track_publisher, MessageParameters(), TrackProperties(),
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}));
}

TEST_F(MoqtSessionTest, PublishUpdateOnPublish) {
  CreateTrackPublisher();
  std::shared_ptr<MoqtTrackPublisher> track_publisher =
      publisher_.GetTrack(kDefaultTrackName());

  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kPublish), _))
      .WillOnce(Return(absl::OkStatus()));

  ASSERT_TRUE(session_.Publish(
      track_publisher, MessageParameters(), TrackProperties(),
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}));
  bidi_wrapper_->ReceiveMessage(MoqtRequestOk());

  // Unknown track returns true.
  MessageParameters update_params;
  update_params.authorization_tokens.emplace_back(AuthTokenType::kOutOfBand,
                                                  "token");
  EXPECT_TRUE(session_.PublishUpdate(
      FullTrackName("unknown", "track"), update_params,
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}));

  MoqtRequestUpdate expected_update;
  expected_update.request_id = 2;
  expected_update.parameters.authorization_tokens =
      update_params.authorization_tokens;
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_update), _))
      .WillOnce(Return(absl::OkStatus()));

  bool update_callback_called = false;
  EXPECT_TRUE(session_.PublishUpdate(
      kDefaultTrackName(), update_params,
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> resp) {
        update_callback_called = true;
        EXPECT_TRUE(std::holds_alternative<MessageParameters>(resp));
      }));
  EXPECT_FALSE(update_callback_called);
  bidi_wrapper_->ReceiveMessage(MoqtRequestOk());
  EXPECT_TRUE(update_callback_called);
}

TEST_F(MoqtSessionTest, PublishUpdateOnSubscribe) {
  bidi_wrapper_ = std::make_unique<MoqtBidiStreamTestWrapper>(
      ResponseStream(kSubscribeByte));
  MoqtSubscribe request = DefaultSubscribe();
  MockTrackPublisher* track = CreateTrackPublisher();
  ReceiveSubscribeSynchronousOk(track, request, bidi_wrapper_.get());

  MessageParameters notify_params;
  notify_params.set_forward(false);
  MoqtPublishStateNotify expected_notify;
  expected_notify.parameters.set_forward(false);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(SerializedControlMessage(expected_notify), _))
      .WillOnce(Return(absl::OkStatus()));
  bool callback_called = false;
  EXPECT_TRUE(session_.PublishUpdate(
      kDefaultTrackName(), notify_params,
      [&](std::variant<MessageParameters, MoqtRequestErrorInfo> resp) {
        callback_called = true;
        EXPECT_TRUE(std::holds_alternative<MessageParameters>(resp));
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(MoqtSessionTest, IncomingPublishAbortsPendingSubscribe) {
  PrepareRequestStream(bidi_wrapper_);
  EXPECT_CALL(mock_bidi_stream_,
              Writev(ControlMessageOfType(MoqtMessageType::kSubscribe), _));
  MessageParameters parameters(SubscribeForTest());
  session_.Subscribe(kDefaultTrackName(), &remote_track_visitor_, parameters);

  // Configure incoming publish callback to accept. Return nullptr because this
  // should never be called.
  bool incoming_publish_callback_called = false;
  session_.callbacks().incoming_publish_callback =
      [&](const FullTrackName&, const MessageParameters&,
          const TrackProperties&, MoqtResponseCallback callback) {
        incoming_publish_callback_called = true;
        return nullptr;
      };

  // Prepare PUBLISH message.
  MoqtPublish publish{1, kDefaultTrackName(), 10, MessageParameters(),
                      TrackProperties()};
  webtransport::test::MockStream publish_stream;
  std::unique_ptr<MoqtBidiStreamTestWrapper> publish_wrapper =
      std::make_unique<MoqtBidiStreamTestWrapper>(
          ResponseStream(kPublishByte, &publish_stream));
  EXPECT_CALL(mock_bidi_stream_, SendStopSending(kResetCodeCancelled));
  EXPECT_CALL(mock_bidi_stream_, ResetWithUserCode(kResetCodeCancelled));
  MoqtRequestOk expected_request_ok;
  expected_request_ok.parameters = parameters;  // params from the SUBSCRIBE.
  // group_order is not stored in ObjectSubscriber::parameters_, and
  // authorization_tokens is sanitized out of PUBLISH_OK.
  expected_request_ok.parameters.group_order = std::nullopt;
  expected_request_ok.parameters.authorization_tokens.clear();
  EXPECT_CALL(publish_stream,
              Writev(SerializedControlMessage(expected_request_ok), _));
  // remote_track_visitor_ is reused, not destroyed.
  EXPECT_CALL(remote_track_visitor_, OnReply);
  publish_wrapper->ReceiveMessage(publish);
  EXPECT_FALSE(incoming_publish_callback_called);
  // Verify it was aborted immediately (not at teardown).
  EXPECT_TRUE(
      testing::Mock::VerifyAndClearExpectations(&remote_track_visitor_));
}

TEST_F(MoqtSessionTest, IncrementRequestId) {
  // Set up writable control stream.
  webtransport::test::InMemoryStreamWithWriteBuffer control_stream(0);
  EXPECT_CALL(mock_session_, GetNegotiatedSubprotocol)
      .WillOnce(Return(std::string(kDefaultMoqtVersion)));
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingUnidirectionalStream)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingUnidirectionalStream)
      .WillOnce(Return(&control_stream));
  session_.OnSessionReady();
  control_stream.write_buffer().clear();

  // Helper lambda to parse request ID from written_data.
  auto get_request_id =
      [&](webtransport::test::InMemoryStreamWithWriteBuffer& stream) {
        quiche::QuicheDataReader reader(stream.write_buffer());
        uint64_t type;
        uint16_t length;
        uint64_t request_id;
        bool type_read = reader.ReadMoqVarInt(&type);
        bool length_read = reader.ReadUInt16(&length);
        bool req_id_read = reader.ReadMoqVarInt(&request_id);
        EXPECT_TRUE(type_read) << "Failed to read type, written_data.size()="
                               << stream.write_buffer().length();
        EXPECT_TRUE(length_read);
        EXPECT_TRUE(req_id_read);
        return request_id;
      };

  uint64_t next_request_id = 0;
  // 1. SubscribeNamespace
  webtransport::test::InMemoryStreamWithWriteBuffer sub_ns_stream(4);
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingBidirectionalStream())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingBidirectionalStream())
      .WillOnce(Return(&sub_ns_stream));
  TrackNamespace namespace1({"namespace1"});
  std::unique_ptr<MoqtNamespaceTask> task1 = session_.SubscribeNamespace(
      namespace1, MessageParameters(),
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {});
  ASSERT_NE(task1, nullptr);
  EXPECT_EQ(get_request_id(sub_ns_stream), next_request_id);
  next_request_id += 2;

  // 2. PublishNamespace
  webtransport::test::InMemoryStreamWithWriteBuffer pub_ns_stream(5);
  EXPECT_CALL(mock_session_, OpenOutgoingBidirectionalStream)
      .WillOnce(Return(&pub_ns_stream));
  TrackNamespace namespace2({"namespace2"});
  bool p1 = session_.PublishNamespace(
      namespace2, MessageParameters(),
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {}, []() {});
  EXPECT_TRUE(p1);
  EXPECT_EQ(next_request_id, get_request_id(pub_ns_stream));
  next_request_id += 2;
  pub_ns_stream.write_buffer().clear();

  // 3. PublishNamespaceUpdate
  MessageParameters params_update;
  bool p_update = session_.PublishNamespaceUpdate(
      namespace2, params_update,
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {});
  EXPECT_TRUE(p_update);
  EXPECT_EQ(get_request_id(pub_ns_stream), next_request_id);
  next_request_id += 2;
  pub_ns_stream.write_buffer().clear();

  // 4. Subscribe
  webtransport::test::InMemoryStreamWithWriteBuffer sub_stream(5);
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingBidirectionalStream)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingBidirectionalStream)
      .WillOnce(Return(&sub_stream));
  FullTrackName track_name1("namespace2", "track1");
  bool s1 = session_.Subscribe(track_name1, &remote_track_visitor_,
                               MessageParameters());
  EXPECT_TRUE(s1);
  EXPECT_EQ(get_request_id(sub_stream), next_request_id);
  next_request_id += 2;
  sub_stream.write_buffer().clear();

  // 5. SubscribeUpdate
  bool s_update = session_.SubscribeUpdate(
      track_name1, MessageParameters(),
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {});
  EXPECT_TRUE(s_update);
  EXPECT_EQ(get_request_id(sub_stream), next_request_id);
  next_request_id += 2;
  sub_stream.write_buffer().clear();

  // 6. Fetch
  webtransport::test::InMemoryStreamWithWriteBuffer fetch_stream(7);
  EXPECT_CALL(mock_session_, OpenOutgoingBidirectionalStream)
      .WillOnce(Return(&fetch_stream));
  FullTrackName fetch_track("namespace2", "fetch_track");
  std::unique_ptr<MoqtFetchTask> f1 = session_.Fetch(
      fetch_track, [](std::variant<FetchOkData, MoqtRequestErrorInfo>) {},
      Location(0, 0), 1, std::nullopt, MessageParameters());
  EXPECT_NE(f1, nullptr);
  EXPECT_EQ(get_request_id(fetch_stream), next_request_id);
  next_request_id += 2;
  fetch_stream.write_buffer().clear();

  // 7. SubscribeNamespace (duplicating the first call)
  webtransport::test::InMemoryStreamWithWriteBuffer sub_ns_stream_2(8);
  EXPECT_CALL(mock_session_, CanOpenNextOutgoingBidirectionalStream)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_session_, OpenOutgoingBidirectionalStream)
      .WillOnce(Return(&sub_ns_stream_2));
  TrackNamespace namespace_dup({"namespace_dup"});
  std::unique_ptr<MoqtNamespaceTask> task_dup = session_.SubscribeNamespace(
      namespace_dup, MessageParameters(),
      [](std::variant<MessageParameters, MoqtRequestErrorInfo>) {});
  ASSERT_NE(task_dup, nullptr);
  EXPECT_EQ(next_request_id, get_request_id(sub_ns_stream_2));
  sub_ns_stream_2.write_buffer().clear();
}

}  // namespace test

}  // namespace moqt
