// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_receive_control_stream.h"

#include <ostream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/http/http_constants.h"
#include "quiche/quic/core/qpack/qpack_header_table.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/test_tools/qpack/qpack_encoder_peer.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/simple_buffer_allocator.h"

namespace quic {

class QpackEncoder;

namespace test {

namespace {
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::StrictMock;

struct TestParams {
  TestParams(const ParsedQuicVersion& version, Perspective perspective)
      : version(version), perspective(perspective) {
    QUIC_LOG(INFO) << "TestParams: " << *this;
  }

  TestParams(const TestParams& other)
      : version(other.version), perspective(other.perspective) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& tp) {
    os << "{ version: " << ParsedQuicVersionToString(tp.version)
       << ", perspective: "
       << (tp.perspective == Perspective::IS_CLIENT ? "client" : "server")
       << "}";
    return os;
  }

  ParsedQuicVersion version;
  Perspective perspective;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& tp) {
  return absl::StrCat(
      ParsedQuicVersionToString(tp.version), "_",
      (tp.perspective == Perspective::IS_CLIENT ? "client" : "server"));
}

std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  ParsedQuicVersionVector all_supported_versions = AllSupportedVersions();
  for (const auto& version : AllSupportedVersions()) {
    if (!VersionUsesHttp3(version.transport_version)) {
      continue;
    }
    for (Perspective p : {Perspective::IS_SERVER, Perspective::IS_CLIENT}) {
      params.emplace_back(version, p);
    }
  }
  return params;
}

class TestStream : public QuicSpdyStream {
 public:
  TestStream(QuicStreamId id, QuicSpdySession* session)
      : QuicSpdyStream(id, session, BIDIRECTIONAL) {}
  ~TestStream() override = default;

  void OnBodyAvailable() override {}
};

class QuicReceiveControlStreamTest : public QuicTestWithParam<TestParams> {
 public:
  QuicReceiveControlStreamTest()
      : connection_(new StrictMock<MockQuicConnection>(
            &helper_, &alarm_factory_, perspective(),
            SupportedVersions(GetParam().version))),
        session_(connection_) {
    EXPECT_CALL(session_, OnCongestionWindowChange(_)).Times(AnyNumber());
    session_.Initialize();
    EXPECT_CALL(
        static_cast<const MockQuicCryptoStream&>(*session_.GetCryptoStream()),
        encryption_established())
        .WillRepeatedly(testing::Return(true));
    QuicStreamId id = perspective() == Perspective::IS_SERVER
                          ? GetNthClientInitiatedUnidirectionalStreamId(
                                session_.transport_version(), 3)
                          : GetNthServerInitiatedUnidirectionalStreamId(
                                session_.transport_version(), 3);
    char type[] = {kControlStream};

    QuicStreamFrame data1(id, false, 0, absl::string_view(type, 1));
    session_.OnStreamFrame(data1);

    receive_control_stream_ =
        QuicSpdySessionPeer::GetReceiveControlStream(&session_);

    stream_ = new TestStream(GetNthClientInitiatedBidirectionalStreamId(
                                 GetParam().version.transport_version, 0),
                             &session_);
    session_.ActivateStream(absl::WrapUnique(stream_));
  }

  Perspective perspective() const { return GetParam().perspective; }

  QuicStreamOffset NumBytesConsumed() {
    return QuicStreamPeer::sequencer(receive_control_stream_)
        ->NumBytesConsumed();
  }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  StrictMock<MockQuicSpdySession> session_;
  QuicReceiveControlStream* receive_control_stream_;
  TestStream* stream_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicReceiveControlStreamTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicReceiveControlStreamTest, ResetControlStream) {
  EXPECT_TRUE(receive_control_stream_->is_static());
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId,
                               receive_control_stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_CLOSED_CRITICAL_STREAM, _, _));
  receive_control_stream_->OnStreamReset(rst_frame);
}

TEST_P(QuicReceiveControlStreamTest, ReceiveSettings) {
  SettingsFrame settings;
  settings.values[10] = 2;
  settings.values[SETTINGS_MAX_FIELD_SECTION_SIZE] = 5;
  settings.values[SETTINGS_QPACK_BLOCKED_STREAMS] = 12;
  settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] = 37;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamFrame frame(receive_control_stream_->id(), false, 1, data);

  QpackEncoder* qpack_encoder = session_.qpack_encoder();
  QpackEncoderHeaderTable* header_table =
      QpackEncoderPeer::header_table(qpack_encoder);
  EXPECT_EQ(std::numeric_limits<size_t>::max(),
            session_.max_outbound_header_list_size());
  EXPECT_EQ(0u, QpackEncoderPeer::maximum_blocked_streams(qpack_encoder));
  EXPECT_EQ(0u, header_table->maximum_dynamic_table_capacity());

  receive_control_stream_->OnStreamFrame(frame);

  EXPECT_EQ(5u, session_.max_outbound_header_list_size());
  EXPECT_EQ(12u, QpackEncoderPeer::maximum_blocked_streams(qpack_encoder));
  EXPECT_EQ(37u, header_table->maximum_dynamic_table_capacity());
}

// Regression test for https://crbug.com/982648.
// QuicReceiveControlStream::OnDataAvailable() must stop processing input as
// soon as OnSettingsFrameStart() is called by HttpDecoder for the second frame.
TEST_P(QuicReceiveControlStreamTest, ReceiveSettingsTwice) {
  SettingsFrame settings;
  // Reserved identifiers, must be ignored.
  settings.values[0x21] = 100;
  settings.values[0x40] = 200;

  std::string settings_frame = HttpEncoder::SerializeSettingsFrame(settings);

  QuicStreamOffset offset = 1;
  EXPECT_EQ(offset, NumBytesConsumed());

  // Receive first SETTINGS frame.
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false, offset,
                      settings_frame));
  offset += settings_frame.length();

  // First SETTINGS frame is consumed.
  EXPECT_EQ(offset, NumBytesConsumed());

  // Second SETTINGS frame causes the connection to be closed.
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_CONTROL_STREAM,
                      "SETTINGS frame can only be received once.", _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
  EXPECT_CALL(session_, OnConnectionClosed(_, _));

  // Receive second SETTINGS frame.
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false, offset,
                      settings_frame));

  // Frame header of second SETTINGS frame is consumed, but not frame payload.
  QuicByteCount settings_frame_header_length = 2;
  EXPECT_EQ(offset + settings_frame_header_length, NumBytesConsumed());
}

TEST_P(QuicReceiveControlStreamTest, ReceiveSettingsFragments) {
  SettingsFrame settings;
  settings.values[10] = 2;
  settings.values[SETTINGS_MAX_FIELD_SECTION_SIZE] = 5;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  std::string data1 = data.substr(0, 1);
  std::string data2 = data.substr(1, data.length() - 1);

  QuicStreamFrame frame(receive_control_stream_->id(), false, 1, data1);
  QuicStreamFrame frame2(receive_control_stream_->id(), false, 2, data2);
  EXPECT_NE(5u, session_.max_outbound_header_list_size());
  receive_control_stream_->OnStreamFrame(frame);
  receive_control_stream_->OnStreamFrame(frame2);
  EXPECT_EQ(5u, session_.max_outbound_header_list_size());
}

TEST_P(QuicReceiveControlStreamTest, ReceiveWrongFrame) {
  // DATA frame header without payload.
  quiche::QuicheBuffer data = HttpEncoder::SerializeDataFrameHeader(
      /* payload_length = */ 2, quiche::SimpleBufferAllocator::Get());

  QuicStreamFrame frame(receive_control_stream_->id(), false, 1,
                        data.AsStringView());
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HTTP_FRAME_UNEXPECTED_ON_CONTROL_STREAM, _, _));
  receive_control_stream_->OnStreamFrame(frame);
}

TEST_P(QuicReceiveControlStreamTest,
       ReceivePriorityUpdateFrameBeforeSettingsFrame) {
  std::string serialized_frame = HttpEncoder::SerializePriorityUpdateFrame({});
  QuicStreamFrame data(receive_control_stream_->id(), /* fin = */ false,
                       /* offset = */ 1, serialized_frame);

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_MISSING_SETTINGS_FRAME,
                              "First frame received on control stream is type "
                              "984832, but it must be SETTINGS.",
                              _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
  EXPECT_CALL(session_, OnConnectionClosed(_, _));

  receive_control_stream_->OnStreamFrame(data);
}

TEST_P(QuicReceiveControlStreamTest, ReceiveGoAwayFrame) {
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_.set_debug_visitor(&debug_visitor);

  QuicStreamOffset offset = 1;

  // Receive SETTINGS frame.
  SettingsFrame settings;
  std::string settings_frame = HttpEncoder::SerializeSettingsFrame(settings);
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(settings));
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false, offset,
                      settings_frame));
  offset += settings_frame.length();

  GoAwayFrame goaway{/* id = */ 0};
  std::string goaway_frame = HttpEncoder::SerializeGoAwayFrame(goaway);
  QuicStreamFrame frame(receive_control_stream_->id(), false, offset,
                        goaway_frame);

  EXPECT_FALSE(session_.goaway_received());

  EXPECT_CALL(debug_visitor, OnGoAwayFrameReceived(goaway));
  receive_control_stream_->OnStreamFrame(frame);

  EXPECT_TRUE(session_.goaway_received());
}

TEST_P(QuicReceiveControlStreamTest, PushPromiseOnControlStreamShouldClose) {
  std::string push_promise_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("05"   // PUSH_PROMISE
                             "01"   // length
                             "00",  // push ID
                             &push_promise_frame));
  QuicStreamFrame frame(receive_control_stream_->id(), false, 1,
                        push_promise_frame);
  EXPECT_CALL(*connection_, CloseConnection(QUIC_HTTP_FRAME_ERROR, _, _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
  EXPECT_CALL(session_, OnConnectionClosed(_, _));
  receive_control_stream_->OnStreamFrame(frame);
}

// Regression test for b/137554973: unknown frames should be consumed.
TEST_P(QuicReceiveControlStreamTest, ConsumeUnknownFrame) {
  EXPECT_EQ(1u, NumBytesConsumed());

  QuicStreamOffset offset = 1;

  // Receive SETTINGS frame.
  std::string settings_frame = HttpEncoder::SerializeSettingsFrame({});
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false, offset,
                      settings_frame));
  offset += settings_frame.length();

  // SETTINGS frame is consumed.
  EXPECT_EQ(offset, NumBytesConsumed());

  // Receive unknown frame.
  std::string unknown_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("21"       // reserved frame type
                             "03"       // payload length
                             "666f6f",  // payload "foo"
                             &unknown_frame));

  receive_control_stream_->OnStreamFrame(QuicStreamFrame(
      receive_control_stream_->id(), /* fin = */ false, offset, unknown_frame));
  offset += unknown_frame.size();

  // Unknown frame is consumed.
  EXPECT_EQ(offset, NumBytesConsumed());
}

TEST_P(QuicReceiveControlStreamTest, ReceiveUnknownFrame) {
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_.set_debug_visitor(&debug_visitor);

  const QuicStreamId id = receive_control_stream_->id();
  QuicStreamOffset offset = 1;

  // Receive SETTINGS frame.
  SettingsFrame settings;
  std::string settings_frame = HttpEncoder::SerializeSettingsFrame(settings);
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(settings));
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(id, /* fin = */ false, offset, settings_frame));
  offset += settings_frame.length();

  // Receive unknown frame.
  std::string unknown_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("21"       // reserved frame type
                             "03"       // payload length
                             "666f6f",  // payload "foo"
                             &unknown_frame));

  EXPECT_CALL(debug_visitor, OnUnknownFrameReceived(id, /* frame_type = */ 0x21,
                                                    /* payload_length = */ 3));
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(id, /* fin = */ false, offset, unknown_frame));
}

TEST_P(QuicReceiveControlStreamTest, CancelPushFrameBeforeSettings) {
  std::string cancel_push_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("03"   // type CANCEL_PUSH
                             "01"   // payload length
                             "01",  // push ID
                             &cancel_push_frame));

  EXPECT_CALL(*connection_, CloseConnection(QUIC_HTTP_FRAME_ERROR,
                                            "CANCEL_PUSH frame received.", _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
  EXPECT_CALL(session_, OnConnectionClosed(_, _));

  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false,
                      /* offset = */ 1, cancel_push_frame));
}

TEST_P(QuicReceiveControlStreamTest, AcceptChFrameBeforeSettings) {
  std::string accept_ch_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("4089"  // type (ACCEPT_CH)
                             "00",   // length
                             &accept_ch_frame));

  if (perspective() == Perspective::IS_SERVER) {
    EXPECT_CALL(*connection_,
                CloseConnection(
                    QUIC_HTTP_FRAME_UNEXPECTED_ON_CONTROL_STREAM,
                    "Invalid frame type 137 received on control stream.", _))
        .WillOnce(
            Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  } else {
    EXPECT_CALL(*connection_,
                CloseConnection(QUIC_HTTP_MISSING_SETTINGS_FRAME,
                                "First frame received on control stream is "
                                "type 137, but it must be SETTINGS.",
                                _))
        .WillOnce(
            Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  }
  EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
  EXPECT_CALL(session_, OnConnectionClosed(_, _));

  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false,
                      /* offset = */ 1, accept_ch_frame));
}

TEST_P(QuicReceiveControlStreamTest, ReceiveAcceptChFrame) {
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_.set_debug_visitor(&debug_visitor);

  const QuicStreamId id = receive_control_stream_->id();
  QuicStreamOffset offset = 1;

  // Receive SETTINGS frame.
  SettingsFrame settings;
  std::string settings_frame = HttpEncoder::SerializeSettingsFrame(settings);
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(settings));
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(id, /* fin = */ false, offset, settings_frame));
  offset += settings_frame.length();

  // Receive ACCEPT_CH frame.
  std::string accept_ch_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("4089"  // type (ACCEPT_CH)
                             "00",   // length
                             &accept_ch_frame));

  if (perspective() == Perspective::IS_CLIENT) {
    EXPECT_CALL(debug_visitor, OnAcceptChFrameReceived(_));
  } else {
    EXPECT_CALL(*connection_,
                CloseConnection(
                    QUIC_HTTP_FRAME_UNEXPECTED_ON_CONTROL_STREAM,
                    "Invalid frame type 137 received on control stream.", _))
        .WillOnce(
            Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
    EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
    EXPECT_CALL(session_, OnConnectionClosed(_, _));
  }

  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(id, /* fin = */ false, offset, accept_ch_frame));
}

TEST_P(QuicReceiveControlStreamTest, ReceiveOriginFrame) {
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_.set_debug_visitor(&debug_visitor);

  const QuicStreamId id = receive_control_stream_->id();
  QuicStreamOffset offset = 1;

  // Receive SETTINGS frame.
  SettingsFrame settings;
  std::string settings_frame = HttpEncoder::SerializeSettingsFrame(settings);
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(settings));
  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(id, /* fin = */ false, offset, settings_frame));
  offset += settings_frame.length();

  // Receive ORIGIN frame.
  std::string origin_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("0C"   // type (ORIGIN)
                             "00",  // length
                             &origin_frame));

  if (GetQuicReloadableFlag(enable_h3_origin_frame)) {
    if (perspective() == Perspective::IS_CLIENT) {
      EXPECT_CALL(debug_visitor, OnOriginFrameReceived(_));
    } else {
      EXPECT_CALL(*connection_,
                  CloseConnection(
                      QUIC_HTTP_FRAME_UNEXPECTED_ON_CONTROL_STREAM,
                      "Invalid frame type 12 received on control stream.", _))
          .WillOnce(
              Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
      EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
      EXPECT_CALL(session_, OnConnectionClosed(_, _));
    }
  } else {
    EXPECT_CALL(debug_visitor,
                OnUnknownFrameReceived(id, /* frame_type = */ 0x0c,
                                       /* payload_length = */ 0));
  }

  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(id, /* fin = */ false, offset, origin_frame));
}

TEST_P(QuicReceiveControlStreamTest, UnknownFrameBeforeSettings) {
  std::string unknown_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("21"       // reserved frame type
                             "03"       // payload length
                             "666f6f",  // payload "foo"
                             &unknown_frame));

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_MISSING_SETTINGS_FRAME,
                              "First frame received on control stream is type "
                              "33, but it must be SETTINGS.",
                              _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_, SendConnectionClosePacket(_, _, _));
  EXPECT_CALL(session_, OnConnectionClosed(_, _));

  receive_control_stream_->OnStreamFrame(
      QuicStreamFrame(receive_control_stream_->id(), /* fin = */ false,
                      /* offset = */ 1, unknown_frame));
}

}  // namespace
}  // namespace test
}  // namespace quic
