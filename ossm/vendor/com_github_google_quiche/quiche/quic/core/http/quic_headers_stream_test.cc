// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_headers_stream.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/http2/core/http2_frame_decoder_adapter.h"
#include "quiche/http2/core/recording_headers_handler.h"
#include "quiche/http2/core/spdy_alt_svc_wire_format.h"
#include "quiche/http2/core/spdy_protocol.h"
#include "quiche/http2/test_tools/spdy_test_utils.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/http/spdy_utils.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_bug_tracker.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/quiche_endian.h"

using quiche::HttpHeaderBlock;
using spdy::ERROR_CODE_PROTOCOL_ERROR;
using spdy::RecordingHeadersHandler;
using spdy::SETTINGS_ENABLE_PUSH;
using spdy::SETTINGS_HEADER_TABLE_SIZE;
using spdy::SETTINGS_INITIAL_WINDOW_SIZE;
using spdy::SETTINGS_MAX_CONCURRENT_STREAMS;
using spdy::SETTINGS_MAX_FRAME_SIZE;
using spdy::Spdy3PriorityToHttp2Weight;
using spdy::SpdyAltSvcWireFormat;
using spdy::SpdyDataIR;
using spdy::SpdyErrorCode;
using spdy::SpdyFramer;
using spdy::SpdyFramerVisitorInterface;
using spdy::SpdyGoAwayIR;
using spdy::SpdyHeadersHandlerInterface;
using spdy::SpdyHeadersIR;
using spdy::SpdyPingId;
using spdy::SpdyPingIR;
using spdy::SpdyPriority;
using spdy::SpdyPriorityIR;
using spdy::SpdyPushPromiseIR;
using spdy::SpdyRstStreamIR;
using spdy::SpdySerializedFrame;
using spdy::SpdySettingsId;
using spdy::SpdySettingsIR;
using spdy::SpdyStreamId;
using spdy::SpdyWindowUpdateIR;
using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace quic {
namespace test {
namespace {

class MockVisitor : public SpdyFramerVisitorInterface {
 public:
  MOCK_METHOD(void, OnError,
              (http2::Http2DecoderAdapter::SpdyFramerError error,
               std::string detailed_error),
              (override));
  MOCK_METHOD(void, OnDataFrameHeader,
              (SpdyStreamId stream_id, size_t length, bool fin), (override));
  MOCK_METHOD(void, OnStreamFrameData,
              (SpdyStreamId stream_id, const char*, size_t len), (override));
  MOCK_METHOD(void, OnStreamEnd, (SpdyStreamId stream_id), (override));
  MOCK_METHOD(void, OnStreamPadding, (SpdyStreamId stream_id, size_t len),
              (override));
  MOCK_METHOD(SpdyHeadersHandlerInterface*, OnHeaderFrameStart,
              (SpdyStreamId stream_id), (override));
  MOCK_METHOD(void, OnHeaderFrameEnd, (SpdyStreamId stream_id), (override));
  MOCK_METHOD(void, OnRstStream,
              (SpdyStreamId stream_id, SpdyErrorCode error_code), (override));
  MOCK_METHOD(void, OnSettings, (), (override));
  MOCK_METHOD(void, OnSetting, (SpdySettingsId id, uint32_t value), (override));
  MOCK_METHOD(void, OnSettingsAck, (), (override));
  MOCK_METHOD(void, OnSettingsEnd, (), (override));
  MOCK_METHOD(void, OnPing, (SpdyPingId unique_id, bool is_ack), (override));
  MOCK_METHOD(void, OnGoAway,
              (SpdyStreamId last_accepted_stream_id, SpdyErrorCode error_code),
              (override));
  MOCK_METHOD(void, OnHeaders,
              (SpdyStreamId stream_id, size_t payload_length, bool has_priority,
               int weight, SpdyStreamId parent_stream_id, bool exclusive,
               bool fin, bool end),
              (override));
  MOCK_METHOD(void, OnWindowUpdate,
              (SpdyStreamId stream_id, int delta_window_size), (override));
  MOCK_METHOD(void, OnPushPromise,
              (SpdyStreamId stream_id, SpdyStreamId promised_stream_id,
               bool end),
              (override));
  MOCK_METHOD(void, OnContinuation,
              (SpdyStreamId stream_id, size_t payload_size, bool end),
              (override));
  MOCK_METHOD(
      void, OnAltSvc,
      (SpdyStreamId stream_id, absl::string_view origin,
       const SpdyAltSvcWireFormat::AlternativeServiceVector& altsvc_vector),
      (override));
  MOCK_METHOD(void, OnPriority,
              (SpdyStreamId stream_id, SpdyStreamId parent_stream_id,
               int weight, bool exclusive),
              (override));
  MOCK_METHOD(void, OnPriorityUpdate,
              (SpdyStreamId prioritized_stream_id,
               absl::string_view priority_field_value),
              (override));
  MOCK_METHOD(bool, OnUnknownFrame,
              (SpdyStreamId stream_id, uint8_t frame_type), (override));
  MOCK_METHOD(void, OnUnknownFrameStart,
              (SpdyStreamId stream_id, size_t length, uint8_t type,
               uint8_t flags),
              (override));
  MOCK_METHOD(void, OnUnknownFramePayload,
              (SpdyStreamId stream_id, absl::string_view payload), (override));
};

struct TestParams {
  TestParams(const ParsedQuicVersion& version, Perspective perspective)
      : version(version), perspective(perspective) {
    QUIC_LOG(INFO) << "TestParams:  " << *this;
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
  for (size_t i = 0; i < all_supported_versions.size(); ++i) {
    if (VersionUsesHttp3(all_supported_versions[i].transport_version)) {
      continue;
    }
    for (Perspective p : {Perspective::IS_SERVER, Perspective::IS_CLIENT}) {
      params.emplace_back(all_supported_versions[i], p);
    }
  }
  return params;
}

class QuicHeadersStreamTest : public QuicTestWithParam<TestParams> {
 public:
  QuicHeadersStreamTest()
      : connection_(new StrictMock<MockQuicConnection>(
            &helper_, &alarm_factory_, perspective(), GetVersion())),
        session_(connection_),
        body_("hello world"),
        stream_frame_(
            QuicUtils::GetHeadersStreamId(connection_->transport_version()),
            /*fin=*/false,
            /*offset=*/0, ""),
        next_promised_stream_id_(2) {
    QuicSpdySessionPeer::SetMaxInboundHeaderListSize(&session_, 256 * 1024);
    EXPECT_CALL(session_, OnCongestionWindowChange(_)).Times(AnyNumber());
    session_.Initialize();
    connection_->SetEncrypter(
        quic::ENCRYPTION_FORWARD_SECURE,
        std::make_unique<quic::NullEncrypter>(connection_->perspective()));
    headers_stream_ = QuicSpdySessionPeer::GetHeadersStream(&session_);
    headers_[":status"] = "200 Ok";
    headers_["content-length"] = "11";
    framer_ = std::unique_ptr<SpdyFramer>(
        new SpdyFramer(SpdyFramer::ENABLE_COMPRESSION));
    deframer_ = std::unique_ptr<http2::Http2DecoderAdapter>(
        new http2::Http2DecoderAdapter());
    deframer_->set_visitor(&visitor_);
    EXPECT_EQ(transport_version(), session_.transport_version());
    EXPECT_TRUE(headers_stream_ != nullptr);
    connection_->AdvanceTime(QuicTime::Delta::FromMilliseconds(1));
    client_id_1_ = GetNthClientInitiatedBidirectionalStreamId(
        connection_->transport_version(), 0);
    client_id_2_ = GetNthClientInitiatedBidirectionalStreamId(
        connection_->transport_version(), 1);
    client_id_3_ = GetNthClientInitiatedBidirectionalStreamId(
        connection_->transport_version(), 2);
    next_stream_id_ =
        QuicUtils::StreamIdDelta(connection_->transport_version());
  }

  QuicStreamId GetNthClientInitiatedId(int n) {
    return GetNthClientInitiatedBidirectionalStreamId(
        connection_->transport_version(), n);
  }

  QuicConsumedData SaveIov(size_t write_length) {
    char* buf = new char[write_length];
    QuicDataWriter writer(write_length, buf, quiche::NETWORK_BYTE_ORDER);
    headers_stream_->WriteStreamData(headers_stream_->stream_bytes_written(),
                                     write_length, &writer);
    saved_data_.append(buf, write_length);
    delete[] buf;
    return QuicConsumedData(write_length, false);
  }

  void SavePayload(const char* data, size_t len) {
    saved_payloads_.append(data, len);
  }

  bool SaveHeaderData(const char* data, int len) {
    saved_header_data_.append(data, len);
    return true;
  }

  void SaveHeaderDataStringPiece(absl::string_view data) {
    saved_header_data_.append(data.data(), data.length());
  }

  void SavePromiseHeaderList(QuicStreamId /* stream_id */,
                             QuicStreamId /* promised_stream_id */, size_t size,
                             const QuicHeaderList& header_list) {
    SaveToHandler(size, header_list);
  }

  void SaveHeaderList(QuicStreamId /* stream_id */, bool /* fin */, size_t size,
                      const QuicHeaderList& header_list) {
    SaveToHandler(size, header_list);
  }

  void SaveToHandler(size_t size, const QuicHeaderList& header_list) {
    headers_handler_ = std::make_unique<RecordingHeadersHandler>();
    headers_handler_->OnHeaderBlockStart();
    for (const auto& p : header_list) {
      headers_handler_->OnHeader(p.first, p.second);
    }
    headers_handler_->OnHeaderBlockEnd(size, size);
  }

  void WriteAndExpectRequestHeaders(QuicStreamId stream_id, bool fin,
                                    SpdyPriority priority) {
    WriteHeadersAndCheckData(stream_id, fin, priority, true /*is_request*/);
  }

  void WriteAndExpectResponseHeaders(QuicStreamId stream_id, bool fin) {
    WriteHeadersAndCheckData(stream_id, fin, 0, false /*is_request*/);
  }

  void WriteHeadersAndCheckData(QuicStreamId stream_id, bool fin,
                                SpdyPriority priority, bool is_request) {
    // Write the headers and capture the outgoing data
    EXPECT_CALL(session_, WritevData(QuicUtils::GetHeadersStreamId(
                                         connection_->transport_version()),
                                     _, _, NO_FIN, _, _))
        .WillOnce(WithArgs<1>(Invoke(this, &QuicHeadersStreamTest::SaveIov)));
    QuicSpdySessionPeer::WriteHeadersOnHeadersStream(
        &session_, stream_id, headers_.Clone(), fin,
        spdy::SpdyStreamPrecedence(priority), nullptr);

    // Parse the outgoing data and check that it matches was was written.
    if (is_request) {
      EXPECT_CALL(
          visitor_,
          OnHeaders(stream_id, saved_data_.length() - spdy::kFrameHeaderSize,
                    kHasPriority, Spdy3PriorityToHttp2Weight(priority),
                    /*parent_stream_id=*/0,
                    /*exclusive=*/false, fin, kFrameComplete));
    } else {
      EXPECT_CALL(
          visitor_,
          OnHeaders(stream_id, saved_data_.length() - spdy::kFrameHeaderSize,
                    !kHasPriority,
                    /*weight=*/0,
                    /*parent_stream_id=*/0,
                    /*exclusive=*/false, fin, kFrameComplete));
    }
    headers_handler_ = std::make_unique<RecordingHeadersHandler>();
    EXPECT_CALL(visitor_, OnHeaderFrameStart(stream_id))
        .WillOnce(Return(headers_handler_.get()));
    EXPECT_CALL(visitor_, OnHeaderFrameEnd(stream_id)).Times(1);
    if (fin) {
      EXPECT_CALL(visitor_, OnStreamEnd(stream_id));
    }
    deframer_->ProcessInput(saved_data_.data(), saved_data_.length());
    EXPECT_FALSE(deframer_->HasError())
        << http2::Http2DecoderAdapter::SpdyFramerErrorToString(
               deframer_->spdy_framer_error());

    CheckHeaders();
    saved_data_.clear();
  }

  void CheckHeaders() {
    ASSERT_TRUE(headers_handler_);
    EXPECT_EQ(headers_, headers_handler_->decoded_block());
    headers_handler_.reset();
  }

  Perspective perspective() const { return GetParam().perspective; }

  QuicTransportVersion transport_version() const {
    return GetParam().version.transport_version;
  }

  ParsedQuicVersionVector GetVersion() {
    ParsedQuicVersionVector versions;
    versions.push_back(GetParam().version);
    return versions;
  }

  void TearDownLocalConnectionState() {
    QuicConnectionPeer::TearDownLocalConnectionState(connection_);
  }

  QuicStreamId NextPromisedStreamId() {
    return next_promised_stream_id_ += next_stream_id_;
  }

  static constexpr bool kFrameComplete = true;
  static constexpr bool kHasPriority = true;

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  StrictMock<MockQuicSpdySession> session_;
  QuicHeadersStream* headers_stream_;
  HttpHeaderBlock headers_;
  std::unique_ptr<RecordingHeadersHandler> headers_handler_;
  std::string body_;
  std::string saved_data_;
  std::string saved_header_data_;
  std::string saved_payloads_;
  std::unique_ptr<SpdyFramer> framer_;
  std::unique_ptr<http2::Http2DecoderAdapter> deframer_;
  StrictMock<MockVisitor> visitor_;
  QuicStreamFrame stream_frame_;
  QuicStreamId next_promised_stream_id_;
  QuicStreamId client_id_1_;
  QuicStreamId client_id_2_;
  QuicStreamId client_id_3_;
  QuicStreamId next_stream_id_;
};

// Run all tests with each version and perspective (client or server).
INSTANTIATE_TEST_SUITE_P(Tests, QuicHeadersStreamTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicHeadersStreamTest, StreamId) {
  EXPECT_EQ(QuicUtils::GetHeadersStreamId(connection_->transport_version()),
            headers_stream_->id());
}

TEST_P(QuicHeadersStreamTest, WriteHeaders) {
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      if (perspective() == Perspective::IS_SERVER) {
        WriteAndExpectResponseHeaders(stream_id, fin);
      } else {
        for (SpdyPriority priority = 0; priority < 7; ++priority) {
          // TODO(rch): implement priorities correctly.
          WriteAndExpectRequestHeaders(stream_id, fin, 0);
        }
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessRawData) {
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      for (SpdyPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        SpdySerializedFrame frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          headers_frame.set_weight(Spdy3PriorityToHttp2Weight(0));
          frame = framer_->SerializeFrame(headers_frame);
          EXPECT_CALL(session_, OnStreamHeadersPriority(
                                    stream_id, spdy::SpdyStreamPrecedence(0)));
        } else {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          frame = framer_->SerializeFrame(headers_frame);
        }
        EXPECT_CALL(session_,
                    OnStreamHeaderList(stream_id, fin, frame.size(), _))
            .WillOnce(Invoke(this, &QuicHeadersStreamTest::SaveHeaderList));
        stream_frame_.data_buffer = frame.data();
        stream_frame_.data_length = frame.size();
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame.size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessPushPromise) {
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    QuicStreamId promised_stream_id = NextPromisedStreamId();
    SpdyPushPromiseIR push_promise(stream_id, promised_stream_id,
                                   headers_.Clone());
    SpdySerializedFrame frame(framer_->SerializeFrame(push_promise));
    if (perspective() == Perspective::IS_SERVER) {
      EXPECT_CALL(*connection_,
                  CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                  "PUSH_PROMISE not supported.", _))
          .WillRepeatedly(InvokeWithoutArgs(
              this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
    } else {
      EXPECT_CALL(session_, MaybeSendRstStreamFrame(promised_stream_id, _, _));
    }
    stream_frame_.data_buffer = frame.data();
    stream_frame_.data_length = frame.size();
    headers_stream_->OnStreamFrame(stream_frame_);
    stream_frame_.offset += frame.size();
  }
}

TEST_P(QuicHeadersStreamTest, ProcessPriorityFrame) {
  QuicStreamId parent_stream_id = 0;
  for (SpdyPriority priority = 0; priority < 7; ++priority) {
    for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
         stream_id += next_stream_id_) {
      int weight = Spdy3PriorityToHttp2Weight(priority);
      SpdyPriorityIR priority_frame(stream_id, parent_stream_id, weight, true);
      SpdySerializedFrame frame(framer_->SerializeFrame(priority_frame));
      parent_stream_id = stream_id;
      if (perspective() == Perspective::IS_CLIENT) {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                    "Server must not send PRIORITY frames.", _))
            .WillRepeatedly(InvokeWithoutArgs(
                this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
      } else {
        EXPECT_CALL(
            session_,
            OnPriorityFrame(stream_id, spdy::SpdyStreamPrecedence(priority)))
            .Times(1);
      }
      stream_frame_.data_buffer = frame.data();
      stream_frame_.data_length = frame.size();
      headers_stream_->OnStreamFrame(stream_frame_);
      stream_frame_.offset += frame.size();
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessPushPromiseDisabledSetting) {
  if (perspective() != Perspective::IS_CLIENT) {
    return;
  }

  session_.OnConfigNegotiated();
  SpdySettingsIR data;
  // Respect supported settings frames SETTINGS_ENABLE_PUSH.
  data.AddSetting(SETTINGS_ENABLE_PUSH, 0);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                      "Unsupported field of HTTP/2 SETTINGS frame: 2", _));
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessLargeRawData) {
  // We want to create a frame that is more than the SPDY Framer's max control
  // frame size, which is 16K, but less than the HPACK decoders max decode
  // buffer size, which is 32K.
  headers_["key0"] = std::string(1 << 13, '.');
  headers_["key1"] = std::string(1 << 13, '.');
  headers_["key2"] = std::string(1 << 13, '.');
  for (QuicStreamId stream_id = client_id_1_; stream_id < client_id_3_;
       stream_id += next_stream_id_) {
    for (bool fin : {false, true}) {
      for (SpdyPriority priority = 0; priority < 7; ++priority) {
        // Replace with "WriteHeadersAndSaveData"
        SpdySerializedFrame frame;
        if (perspective() == Perspective::IS_SERVER) {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          headers_frame.set_has_priority(true);
          headers_frame.set_weight(Spdy3PriorityToHttp2Weight(0));
          frame = framer_->SerializeFrame(headers_frame);
          EXPECT_CALL(session_, OnStreamHeadersPriority(
                                    stream_id, spdy::SpdyStreamPrecedence(0)));
        } else {
          SpdyHeadersIR headers_frame(stream_id, headers_.Clone());
          headers_frame.set_fin(fin);
          frame = framer_->SerializeFrame(headers_frame);
        }
        EXPECT_CALL(session_,
                    OnStreamHeaderList(stream_id, fin, frame.size(), _))
            .WillOnce(Invoke(this, &QuicHeadersStreamTest::SaveHeaderList));
        stream_frame_.data_buffer = frame.data();
        stream_frame_.data_length = frame.size();
        headers_stream_->OnStreamFrame(stream_frame_);
        stream_frame_.offset += frame.size();
        CheckHeaders();
      }
    }
  }
}

TEST_P(QuicHeadersStreamTest, ProcessBadData) {
  const char kBadData[] = "blah blah blah";
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA, _, _))
      .Times(::testing::AnyNumber());
  stream_frame_.data_buffer = kBadData;
  stream_frame_.data_length = strlen(kBadData);
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyDataFrame) {
  SpdyDataIR data(/* stream_id = */ 2, "ping");
  SpdySerializedFrame frame(framer_->SerializeFrame(data));

  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "SPDY DATA frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyRstStreamFrame) {
  SpdyRstStreamIR data(/* stream_id = */ 2, ERROR_CODE_PROTOCOL_ERROR);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                              "SPDY RST_STREAM frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, RespectHttp2SettingsFrameSupportedFields) {
  const uint32_t kTestHeaderTableSize = 1000;
  SpdySettingsIR data;
  // Respect supported settings frames SETTINGS_HEADER_TABLE_SIZE,
  // SETTINGS_MAX_HEADER_LIST_SIZE.
  data.AddSetting(SETTINGS_HEADER_TABLE_SIZE, kTestHeaderTableSize);
  data.AddSetting(spdy::SETTINGS_MAX_HEADER_LIST_SIZE, 2000);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
  EXPECT_EQ(kTestHeaderTableSize, QuicSpdySessionPeer::GetSpdyFramer(&session_)
                                      ->header_encoder_table_size());
}

// Regression test for b/208997000.
TEST_P(QuicHeadersStreamTest, LimitEncoderDynamicTableSize) {
  const uint32_t kVeryLargeTableSizeLimit = 1024 * 1024 * 1024;
  SpdySettingsIR data;
  data.AddSetting(SETTINGS_HEADER_TABLE_SIZE, kVeryLargeTableSizeLimit);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
  EXPECT_EQ(16384u, QuicSpdySessionPeer::GetSpdyFramer(&session_)
                        ->header_encoder_table_size());
}

TEST_P(QuicHeadersStreamTest, RespectHttp2SettingsFrameUnsupportedFields) {
  SpdySettingsIR data;
  // Does not support SETTINGS_MAX_CONCURRENT_STREAMS,
  // SETTINGS_INITIAL_WINDOW_SIZE, SETTINGS_ENABLE_PUSH and
  // SETTINGS_MAX_FRAME_SIZE.
  data.AddSetting(SETTINGS_MAX_CONCURRENT_STREAMS, 100);
  data.AddSetting(SETTINGS_INITIAL_WINDOW_SIZE, 100);
  data.AddSetting(SETTINGS_ENABLE_PUSH, 1);
  data.AddSetting(SETTINGS_MAX_FRAME_SIZE, 1250);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  absl::StrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                               SETTINGS_MAX_CONCURRENT_STREAMS),
                  _));
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  absl::StrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                               SETTINGS_INITIAL_WINDOW_SIZE),
                  _));
  if (session_.perspective() == Perspective::IS_CLIENT) {
    EXPECT_CALL(*connection_,
                CloseConnection(
                    QUIC_INVALID_HEADERS_STREAM_DATA,
                    absl::StrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                                 SETTINGS_ENABLE_PUSH),
                    _));
  }
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_HEADERS_STREAM_DATA,
                  absl::StrCat("Unsupported field of HTTP/2 SETTINGS frame: ",
                               SETTINGS_MAX_FRAME_SIZE),
                  _));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyPingFrame) {
  SpdyPingIR data(1);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "SPDY PING frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyGoAwayFrame) {
  SpdyGoAwayIR data(/* last_good_stream_id = */ 1, ERROR_CODE_PROTOCOL_ERROR,
                    "go away");
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "SPDY GOAWAY frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, ProcessSpdyWindowUpdateFrame) {
  SpdyWindowUpdateIR data(/* stream_id = */ 1, /* delta = */ 1);
  SpdySerializedFrame frame(framer_->SerializeFrame(data));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                              "SPDY WINDOW_UPDATE frame received.", _))
      .WillOnce(InvokeWithoutArgs(
          this, &QuicHeadersStreamTest::TearDownLocalConnectionState));
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  headers_stream_->OnStreamFrame(stream_frame_);
}

TEST_P(QuicHeadersStreamTest, NoConnectionLevelFlowControl) {
  EXPECT_FALSE(QuicStreamPeer::StreamContributesToConnectionFlowControl(
      headers_stream_));
}

TEST_P(QuicHeadersStreamTest, AckSentData) {
  EXPECT_CALL(session_, WritevData(QuicUtils::GetHeadersStreamId(
                                       connection_->transport_version()),
                                   _, _, NO_FIN, _, _))
      .WillRepeatedly(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  InSequence s;
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener3(
      new MockAckListener());

  // Packet 1.
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);

  // Packet 2.
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);

  // Packet 3.
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);

  // Packet 2 gets retransmitted.
  EXPECT_CALL(*ack_listener3, OnPacketRetransmitted(7)).Times(1);
  EXPECT_CALL(*ack_listener2, OnPacketRetransmitted(7)).Times(1);
  headers_stream_->OnStreamFrameRetransmitted(21, 7, false);
  headers_stream_->OnStreamFrameRetransmitted(28, 7, false);

  // Packets are acked in order: 2, 3, 1.
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      21, 7, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(7u, newly_acked_length);
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      28, 7, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(7u, newly_acked_length);

  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      35, 7, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(7u, newly_acked_length);

  EXPECT_CALL(*ack_listener1, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener1, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      0, 7, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(7u, newly_acked_length);
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      7, 7, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(7u, newly_acked_length);
  // Unsent data is acked.
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      14, 10, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(7u, newly_acked_length);
}

TEST_P(QuicHeadersStreamTest, FrameContainsMultipleHeaders) {
  // In this test, a stream frame can contain multiple headers.
  EXPECT_CALL(session_, WritevData(QuicUtils::GetHeadersStreamId(
                                       connection_->transport_version()),
                                   _, _, NO_FIN, _, _))
      .WillRepeatedly(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  InSequence s;
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener3(
      new MockAckListener());

  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);

  // Frame 1 is retransmitted.
  EXPECT_CALL(*ack_listener1, OnPacketRetransmitted(14));
  EXPECT_CALL(*ack_listener2, OnPacketRetransmitted(3));
  headers_stream_->OnStreamFrameRetransmitted(0, 17, false);

  // Frames are acked in order: 2, 3, 1.
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*ack_listener2, OnPacketAcked(4, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(2, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      17, 13, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(13u, newly_acked_length);

  EXPECT_CALL(*ack_listener2, OnPacketAcked(5, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      30, 12, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(12u, newly_acked_length);

  EXPECT_CALL(*ack_listener1, OnPacketAcked(14, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(3, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      0, 17, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(17u, newly_acked_length);
}

TEST_P(QuicHeadersStreamTest, HeadersGetAckedMultipleTimes) {
  EXPECT_CALL(session_, WritevData(QuicUtils::GetHeadersStreamId(
                                       connection_->transport_version()),
                                   _, _, NO_FIN, _, _))
      .WillRepeatedly(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  InSequence s;
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  quiche::QuicheReferenceCountedPointer<MockAckListener> ack_listener3(
      new MockAckListener());

  // Send [0, 42).
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header5", false, ack_listener1);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);
  headers_stream_->WriteOrBufferData("Header7", false, ack_listener2);
  headers_stream_->WriteOrBufferData("Header9", false, ack_listener3);

  // Ack [15, 20), [5, 25), [10, 17), [0, 12) and [22, 42).
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*ack_listener2, OnPacketAcked(5, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      15, 5, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(5u, newly_acked_length);

  EXPECT_CALL(*ack_listener1, OnPacketAcked(9, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(1, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(1, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(4, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      5, 20, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(15u, newly_acked_length);

  // Duplicate ack.
  EXPECT_FALSE(headers_stream_->OnStreamFrameAcked(
      10, 7, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(0u, newly_acked_length);

  EXPECT_CALL(*ack_listener1, OnPacketAcked(5, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      0, 12, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(5u, newly_acked_length);

  EXPECT_CALL(*ack_listener3, OnPacketAcked(3, _));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_CALL(*ack_listener3, OnPacketAcked(7, _));
  EXPECT_TRUE(headers_stream_->OnStreamFrameAcked(
      22, 20, false, QuicTime::Delta::Zero(), QuicTime::Zero(),
      &newly_acked_length, /*is_retransmission=*/false));
  EXPECT_EQ(17u, newly_acked_length);
}

TEST_P(QuicHeadersStreamTest, CloseOnPushPromiseToServer) {
  if (perspective() == Perspective::IS_CLIENT) {
    return;
  }
  QuicStreamId promised_id = 1;
  SpdyPushPromiseIR push_promise(client_id_1_, promised_id, headers_.Clone());
  SpdySerializedFrame frame = framer_->SerializeFrame(push_promise);
  stream_frame_.data_buffer = frame.data();
  stream_frame_.data_length = frame.size();
  EXPECT_CALL(session_, OnStreamHeaderList(_, _, _, _));
  // TODO(lassey): Check for HTTP_WRONG_STREAM error code.
  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA,
                                            "PUSH_PROMISE not supported.", _));
  headers_stream_->OnStreamFrame(stream_frame_);
}

}  // namespace
}  // namespace test
}  // namespace quic
