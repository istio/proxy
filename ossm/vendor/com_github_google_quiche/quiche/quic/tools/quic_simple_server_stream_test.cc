// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/tools/quic_simple_server_stream.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/http/http_encoder.h"
#include "quiche/quic/core/http/spdy_utils.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_session_peer.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simulator/simulator.h"
#include "quiche/quic/tools/quic_backend_response.h"
#include "quiche/quic/tools/quic_memory_cache_backend.h"
#include "quiche/quic/tools/quic_simple_server_backend.h"
#include "quiche/quic/tools/quic_simple_server_session.h"
#include "quiche/common/simple_buffer_allocator.h"

using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;
using testing::StrictMock;

namespace quic {
namespace test {

const size_t kFakeFrameLen = 60;
const size_t kErrorLength = strlen(QuicSimpleServerStream::kErrorResponseBody);
const size_t kDataFrameHeaderLength = 2;

class TestStream : public QuicSimpleServerStream {
 public:
  TestStream(QuicStreamId stream_id, QuicSpdySession* session, StreamType type,
             QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerStream(stream_id, session, type,
                               quic_simple_server_backend) {
    EXPECT_CALL(*this, WriteOrBufferBody(_, _))
        .Times(AnyNumber())
        .WillRepeatedly([this](absl::string_view data, bool fin) {
          this->QuicSimpleServerStream::WriteOrBufferBody(data, fin);
        });
  }

  ~TestStream() override = default;

  MOCK_METHOD(void, FireAlarmMock, (), ());
  MOCK_METHOD(void, WriteHeadersMock, (bool fin), ());
  MOCK_METHOD(void, WriteEarlyHintsHeadersMock, (bool fin), ());
  MOCK_METHOD(void, WriteOrBufferBody, (absl::string_view data, bool fin),
              (override));

  size_t WriteHeaders(
      quiche::HttpHeaderBlock header_block, bool fin,
      quiche::QuicheReferenceCountedPointer<QuicAckListenerInterface>
      /*ack_listener*/) override {
    if (header_block[":status"] == "103") {
      WriteEarlyHintsHeadersMock(fin);
    } else {
      WriteHeadersMock(fin);
    }
    return 0;
  }

  // Expose protected QuicSimpleServerStream methods.
  void DoSendResponse() { SendResponse(); }
  void DoSendErrorResponse() { QuicSimpleServerStream::SendErrorResponse(); }

  quiche::HttpHeaderBlock* mutable_headers() { return &request_headers_; }
  void set_body(std::string body) { body_ = std::move(body); }
  const std::string& body() const { return body_; }
  int content_length() const { return content_length_; }
  bool send_response_was_called() const { return send_response_was_called_; }
  bool send_error_response_was_called() const {
    return send_error_response_was_called_;
  }

  absl::string_view GetHeader(absl::string_view key) const {
    auto it = request_headers_.find(key);
    QUICHE_DCHECK(it != request_headers_.end());
    return it->second;
  }

  void ReplaceBackend(QuicSimpleServerBackend* backend) {
    set_quic_simple_server_backend_for_test(backend);
  }

 protected:
  void SendResponse() override {
    send_response_was_called_ = true;
    QuicSimpleServerStream::SendResponse();
  }

  void SendErrorResponse(int resp_code) override {
    send_error_response_was_called_ = true;
    QuicSimpleServerStream::SendErrorResponse(resp_code);
  }

 private:
  bool send_response_was_called_ = false;
  bool send_error_response_was_called_ = false;
};

namespace {

class MockQuicSimpleServerSession : public QuicSimpleServerSession {
 public:
  const size_t kMaxStreamsForTest = 100;

  MockQuicSimpleServerSession(
      QuicConnection* connection, MockQuicSessionVisitor* owner,
      MockQuicCryptoServerStreamHelper* helper,
      QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerSession(DefaultQuicConfig(), CurrentSupportedVersions(),
                                connection, owner, helper, crypto_config,
                                compressed_certs_cache,
                                quic_simple_server_backend) {
    if (VersionHasIetfQuicFrames(connection->transport_version())) {
      QuicSessionPeer::SetMaxOpenIncomingUnidirectionalStreams(
          this, kMaxStreamsForTest);
      QuicSessionPeer::SetMaxOpenIncomingBidirectionalStreams(
          this, kMaxStreamsForTest);
    } else {
      QuicSessionPeer::SetMaxOpenIncomingStreams(this, kMaxStreamsForTest);
      QuicSessionPeer::SetMaxOpenOutgoingStreams(this, kMaxStreamsForTest);
    }
    ON_CALL(*this, WritevData(_, _, _, _, _, _))
        .WillByDefault(Invoke(this, &MockQuicSimpleServerSession::ConsumeData));
  }

  MockQuicSimpleServerSession(const MockQuicSimpleServerSession&) = delete;
  MockQuicSimpleServerSession& operator=(const MockQuicSimpleServerSession&) =
      delete;
  ~MockQuicSimpleServerSession() override = default;

  MOCK_METHOD(void, OnConnectionClosed,
              (const QuicConnectionCloseFrame& frame,
               ConnectionCloseSource source),
              (override));
  MOCK_METHOD(QuicSpdyStream*, CreateIncomingStream, (QuicStreamId id),
              (override));
  MOCK_METHOD(QuicConsumedData, WritevData,
              (QuicStreamId id, size_t write_length, QuicStreamOffset offset,
               StreamSendingState state, TransmissionType type,
               EncryptionLevel level),
              (override));
  MOCK_METHOD(void, OnStreamHeaderList,
              (QuicStreamId stream_id, bool fin, size_t frame_len,
               const QuicHeaderList& header_list),
              (override));
  MOCK_METHOD(void, OnStreamHeadersPriority,
              (QuicStreamId stream_id,
               const spdy::SpdyStreamPrecedence& precedence),
              (override));
  MOCK_METHOD(void, MaybeSendRstStreamFrame,
              (QuicStreamId stream_id, QuicResetStreamError error,
               QuicStreamOffset bytes_written),
              (override));
  MOCK_METHOD(void, MaybeSendStopSendingFrame,
              (QuicStreamId stream_id, QuicResetStreamError error), (override));

  using QuicSession::ActivateStream;

  QuicConsumedData ConsumeData(QuicStreamId id, size_t write_length,
                               QuicStreamOffset offset,
                               StreamSendingState state,
                               TransmissionType /*type*/,
                               std::optional<EncryptionLevel> /*level*/) {
    if (write_length > 0) {
      auto buf = std::make_unique<char[]>(write_length);
      QuicStream* stream = GetOrCreateStream(id);
      QUICHE_DCHECK(stream);
      QuicDataWriter writer(write_length, buf.get(), quiche::HOST_BYTE_ORDER);
      stream->WriteStreamData(offset, write_length, &writer);
    } else {
      QUICHE_DCHECK(state != NO_FIN);
    }
    return QuicConsumedData(write_length, state != NO_FIN);
  }

  quiche::HttpHeaderBlock original_request_headers_;
};

class QuicSimpleServerStreamTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicSimpleServerStreamTest()
      : connection_(new StrictMock<MockQuicConnection>(
            &simulator_, simulator_.GetAlarmFactory(), Perspective::IS_SERVER,
            SupportedVersions(GetParam()))),
        crypto_config_(new QuicCryptoServerConfig(
            QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
            crypto_test_utils::ProofSourceForTesting(),
            KeyExchangeSource::Default())),
        compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
        session_(connection_, &session_owner_, &session_helper_,
                 crypto_config_.get(), &compressed_certs_cache_,
                 &memory_cache_backend_),
        quic_response_(new QuicBackendResponse),
        body_("hello world") {
    connection_->set_visitor(&session_);
    header_list_.OnHeader(":authority", "www.google.com");
    header_list_.OnHeader(":path", "/");
    header_list_.OnHeader(":method", "POST");
    header_list_.OnHeader(":scheme", "https");
    header_list_.OnHeader("content-length", "11");

    header_list_.OnHeaderBlockEnd(128, 128);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    session_.Initialize();
    connection_->SetEncrypter(
        quic::ENCRYPTION_FORWARD_SECURE,
        std::make_unique<quic::NullEncrypter>(connection_->perspective()));
    if (connection_->version().SupportsAntiAmplificationLimit()) {
      QuicConnectionPeer::SetAddressValidated(connection_);
    }
    stream_ = new StrictMock<TestStream>(
        GetNthClientInitiatedBidirectionalStreamId(
            connection_->transport_version(), 0),
        &session_, BIDIRECTIONAL, &memory_cache_backend_);
    // Register stream_ in dynamic_stream_map_ and pass ownership to session_.
    session_.ActivateStream(absl::WrapUnique(stream_));
    QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
        session_.config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesUnidirectional(
        session_.config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesIncomingBidirectional(
        session_.config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesOutgoingBidirectional(
        session_.config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedMaxUnidirectionalStreams(session_.config(), 10);
    session_.OnConfigNegotiated();
    simulator_.RunFor(QuicTime::Delta::FromSeconds(1));
  }

  const std::string& StreamBody() { return stream_->body(); }

  std::string StreamHeadersValue(const std::string& key) {
    return (*stream_->mutable_headers())[key].as_string();
  }

  bool UsesHttp3() const {
    return VersionUsesHttp3(connection_->transport_version());
  }

  void ReplaceBackend(std::unique_ptr<QuicSimpleServerBackend> backend) {
    replacement_backend_ = std::move(backend);
    stream_->ReplaceBackend(replacement_backend_.get());
  }

  quic::simulator::Simulator simulator_;
  quiche::HttpHeaderBlock response_headers_;
  MockQuicConnectionHelper helper_;
  StrictMock<MockQuicConnection>* connection_;
  StrictMock<MockQuicSessionVisitor> session_owner_;
  StrictMock<MockQuicCryptoServerStreamHelper> session_helper_;
  std::unique_ptr<QuicCryptoServerConfig> crypto_config_;
  QuicCompressedCertsCache compressed_certs_cache_;
  QuicMemoryCacheBackend memory_cache_backend_;
  std::unique_ptr<QuicSimpleServerBackend> replacement_backend_;
  StrictMock<MockQuicSimpleServerSession> session_;
  StrictMock<TestStream>* stream_;  // Owned by session_.
  std::unique_ptr<QuicBackendResponse> quic_response_;
  std::string body_;
  QuicHeaderList header_list_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicSimpleServerStreamTest,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicSimpleServerStreamTest, TestFraming) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));
  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data =
      UsesHttp3() ? absl::StrCat(header.AsStringView(), body_) : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
  EXPECT_EQ(body_, StreamBody());
}

TEST_P(QuicSimpleServerStreamTest, TestFramingOnePacket) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));

  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data =
      UsesHttp3() ? absl::StrCat(header.AsStringView(), body_) : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
  EXPECT_EQ(body_, StreamBody());
}

TEST_P(QuicSimpleServerStreamTest, SendQuicRstStreamNoErrorInStopReading) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));

  EXPECT_FALSE(stream_->fin_received());
  EXPECT_FALSE(stream_->rst_received());

  QuicStreamPeer::SetFinSent(stream_);
  stream_->CloseWriteSide();

  if (session_.version().UsesHttp3()) {
    EXPECT_CALL(session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_NO_ERROR)))
        .Times(1);
  } else {
    EXPECT_CALL(
        session_,
        MaybeSendRstStreamFrame(
            _, QuicResetStreamError::FromInternal(QUIC_STREAM_NO_ERROR), _))
        .Times(1);
  }
  stream_->StopReading();
}

TEST_P(QuicSimpleServerStreamTest, TestFramingExtraData) {
  InSequence seq;
  std::string large_body = "hello world!!!!!!";

  // We'll automatically write out an error (headers + body)
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  if (UsesHttp3()) {
    EXPECT_CALL(session_,
                WritevData(_, kDataFrameHeaderLength, _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, kErrorLength, _, FIN, _, _));

  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data =
      UsesHttp3() ? absl::StrCat(header.AsStringView(), body_) : body_;

  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  // Content length is still 11.  This will register as an error and we won't
  // accept the bytes.
  header = HttpEncoder::SerializeDataFrameHeader(
      large_body.length(), quiche::SimpleBufferAllocator::Get());
  std::string data2 = UsesHttp3()
                          ? absl::StrCat(header.AsStringView(), large_body)
                          : large_body;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/true, data.size(), data2));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithIllegalResponseStatus) {
  // Send an illegal response with response status not supported by HTTP/2.
  quiche::HttpHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":method"] = "GET";

  // HTTP/2 only supports integer responsecode, so "200 OK" is illegal.
  response_headers_[":status"] = "200 OK";
  response_headers_["content-length"] = "5";
  std::string body = "Yummm";
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body.length(), quiche::SimpleBufferAllocator::Get());

  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);

  QuicStreamPeer::SetFinReceived(stream_);

  InSequence s;
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  if (UsesHttp3()) {
    EXPECT_CALL(session_, WritevData(_, header.size(), _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, kErrorLength, _, FIN, _, _));

  stream_->DoSendResponse();
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithIllegalResponseStatus2) {
  // Send an illegal response with response status not supported by HTTP/2.
  quiche::HttpHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":method"] = "GET";

  // HTTP/2 only supports 3-digit-integer, so "+200" is illegal.
  response_headers_[":status"] = "+200";
  response_headers_["content-length"] = "5";
  std::string body = "Yummm";

  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body.length(), quiche::SimpleBufferAllocator::Get());

  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);

  QuicStreamPeer::SetFinReceived(stream_);

  InSequence s;
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  if (UsesHttp3()) {
    EXPECT_CALL(session_, WritevData(_, header.size(), _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, kErrorLength, _, FIN, _, _));

  stream_->DoSendResponse();
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithValidHeaders) {
  // Add a request and response with valid headers.
  quiche::HttpHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/bar";
  (*request_headers)[":authority"] = "www.google.com";
  (*request_headers)[":method"] = "GET";

  response_headers_[":status"] = "200";
  response_headers_["content-length"] = "5";
  std::string body = "Yummm";

  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body.length(), quiche::SimpleBufferAllocator::Get());

  memory_cache_backend_.AddResponse("www.google.com", "/bar",
                                    std::move(response_headers_), body);
  QuicStreamPeer::SetFinReceived(stream_);

  InSequence s;
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  if (UsesHttp3()) {
    EXPECT_CALL(session_, WritevData(_, header.size(), _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, body.length(), _, FIN, _, _));

  stream_->DoSendResponse();
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, SendResponseWithEarlyHints) {
  std::string host = "www.google.com";
  std::string request_path = "/foo";
  std::string body = "Yummm";

  // Add a request and response with early hints.
  quiche::HttpHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = request_path;
  (*request_headers)[":authority"] = host;
  (*request_headers)[":method"] = "GET";

  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body.length(), quiche::SimpleBufferAllocator::Get());
  std::vector<quiche::HttpHeaderBlock> early_hints;
  // Add two Early Hints.
  const size_t kNumEarlyHintsResponses = 2;
  for (size_t i = 0; i < kNumEarlyHintsResponses; ++i) {
    quiche::HttpHeaderBlock hints;
    hints["link"] = "</image.png>; rel=preload; as=image";
    early_hints.push_back(std::move(hints));
  }

  response_headers_[":status"] = "200";
  response_headers_["content-length"] = "5";
  memory_cache_backend_.AddResponseWithEarlyHints(
      host, request_path, std::move(response_headers_), body, early_hints);
  QuicStreamPeer::SetFinReceived(stream_);

  InSequence s;
  for (size_t i = 0; i < kNumEarlyHintsResponses; ++i) {
    EXPECT_CALL(*stream_, WriteEarlyHintsHeadersMock(false));
  }
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  if (UsesHttp3()) {
    EXPECT_CALL(session_, WritevData(_, header.size(), _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, body.length(), _, FIN, _, _));

  stream_->DoSendResponse();
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

class AlarmTestDelegate : public QuicAlarm::DelegateWithoutContext {
 public:
  AlarmTestDelegate(TestStream* stream) : stream_(stream) {}

  void OnAlarm() override { stream_->FireAlarmMock(); }

 private:
  TestStream* stream_;
};

TEST_P(QuicSimpleServerStreamTest, SendResponseWithDelay) {
  // Add a request and response with valid headers.
  quiche::HttpHeaderBlock* request_headers = stream_->mutable_headers();
  std::string host = "www.google.com";
  std::string path = "/bar";
  (*request_headers)[":path"] = path;
  (*request_headers)[":authority"] = host;
  (*request_headers)[":method"] = "GET";

  response_headers_[":status"] = "200";
  response_headers_["content-length"] = "5";
  std::string body = "Yummm";
  QuicTime::Delta delay = QuicTime::Delta::FromMilliseconds(3000);

  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body.length(), quiche::SimpleBufferAllocator::Get());

  memory_cache_backend_.AddResponse(host, path, std::move(response_headers_),
                                    body);
  auto did_delay_succeed =
      memory_cache_backend_.SetResponseDelay(host, path, delay);
  EXPECT_TRUE(did_delay_succeed);
  auto did_invalid_delay_succeed =
      memory_cache_backend_.SetResponseDelay(host, "nonsense", delay);
  EXPECT_FALSE(did_invalid_delay_succeed);
  std::unique_ptr<QuicAlarm> alarm(connection_->alarm_factory()->CreateAlarm(
      new AlarmTestDelegate(stream_)));
  alarm->Set(connection_->clock()->Now() + delay);
  QuicStreamPeer::SetFinReceived(stream_);
  InSequence s;
  EXPECT_CALL(*stream_, FireAlarmMock());
  EXPECT_CALL(*stream_, WriteHeadersMock(false));

  if (UsesHttp3()) {
    EXPECT_CALL(session_, WritevData(_, header.size(), _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, body.length(), _, FIN, _, _));

  stream_->DoSendResponse();
  simulator_.RunFor(delay);

  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, TestSendErrorResponse) {
  QuicStreamPeer::SetFinReceived(stream_);

  InSequence s;
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  if (UsesHttp3()) {
    EXPECT_CALL(session_,
                WritevData(_, kDataFrameHeaderLength, _, NO_FIN, _, _));
  }
  EXPECT_CALL(session_, WritevData(_, kErrorLength, _, FIN, _, _));

  stream_->DoSendErrorResponse();
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidMultipleContentLength) {
  quiche::HttpHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  header_list_.OnHeader("content-length", absl::string_view("11\00012", 5));

  if (session_.version().UsesHttp3()) {
    EXPECT_CALL(session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_NO_ERROR)));
  }
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));
  stream_->OnStreamHeaderList(true, kFakeFrameLen, header_list_);

  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidLeadingNullContentLength) {
  quiche::HttpHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  header_list_.OnHeader("content-length", absl::string_view("\00012", 3));

  if (session_.version().UsesHttp3()) {
    EXPECT_CALL(session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_NO_ERROR)));
  }
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));
  stream_->OnStreamHeaderList(true, kFakeFrameLen, header_list_);

  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidMultipleContentLengthII) {
  quiche::HttpHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  header_list_.OnHeader("content-length", absl::string_view("11\00011", 5));

  if (session_.version().UsesHttp3()) {
    EXPECT_CALL(session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_NO_ERROR)));
    EXPECT_CALL(*stream_, WriteHeadersMock(false));
    EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
        .WillRepeatedly(
            Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));
  }

  stream_->OnStreamHeaderList(false, kFakeFrameLen, header_list_);

  if (session_.version().UsesHttp3()) {
    EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
    EXPECT_TRUE(stream_->reading_stopped());
    EXPECT_TRUE(stream_->write_side_closed());
  } else {
    EXPECT_EQ(11, stream_->content_length());
    EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
    EXPECT_FALSE(stream_->reading_stopped());
    EXPECT_FALSE(stream_->write_side_closed());
  }
}

TEST_P(QuicSimpleServerStreamTest,
       DoNotSendQuicRstStreamNoErrorWithRstReceived) {
  EXPECT_FALSE(stream_->reading_stopped());

  if (VersionUsesHttp3(connection_->transport_version())) {
    // Unidirectional stream type and then a Stream Cancellation instruction is
    // sent on the QPACK decoder stream.  Ignore these writes without any
    // assumption on their number or size.
    auto* qpack_decoder_stream =
        QuicSpdySessionPeer::GetQpackDecoderSendStream(&session_);
    EXPECT_CALL(session_, WritevData(qpack_decoder_stream->id(), _, _, _, _, _))
        .Times(AnyNumber());
  }

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _,
          session_.version().UsesHttp3()
              ? QuicResetStreamError::FromInternal(QUIC_STREAM_CANCELLED)
              : QuicResetStreamError::FromInternal(QUIC_RST_ACKNOWLEDGEMENT),
          _))
      .Times(1);
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  if (VersionHasIetfQuicFrames(connection_->transport_version())) {
    EXPECT_CALL(session_owner_, OnStopSendingReceived(_));
    // Create and inject a STOP SENDING frame to complete the close
    // of the stream. This is only needed for version 99/IETF QUIC.
    QuicStopSendingFrame stop_sending(kInvalidControlFrameId, stream_->id(),
                                      QUIC_STREAM_CANCELLED);
    session_.OnStopSendingFrame(stop_sending);
  }
  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSimpleServerStreamTest, InvalidHeadersWithFin) {
  char arr[] = {
      0x3a,   0x68, 0x6f, 0x73,  // :hos
      0x74,   0x00, 0x00, 0x00,  // t...
      0x00,   0x00, 0x00, 0x00,  // ....
      0x07,   0x3a, 0x6d, 0x65,  // .:me
      0x74,   0x68, 0x6f, 0x64,  // thod
      0x00,   0x00, 0x00, 0x03,  // ....
      0x47,   0x45, 0x54, 0x00,  // GET.
      0x00,   0x00, 0x05, 0x3a,  // ...:
      0x70,   0x61, 0x74, 0x68,  // path
      0x00,   0x00, 0x00, 0x04,  // ....
      0x2f,   0x66, 0x6f, 0x6f,  // /foo
      0x00,   0x00, 0x00, 0x07,  // ....
      0x3a,   0x73, 0x63, 0x68,  // :sch
      0x65,   0x6d, 0x65, 0x00,  // eme.
      0x00,   0x00, 0x00, 0x00,  // ....
      0x00,   0x00, 0x08, 0x3a,  // ...:
      0x76,   0x65, 0x72, 0x73,  // vers
      '\x96', 0x6f, 0x6e, 0x00,  // <i(69)>on.
      0x00,   0x00, 0x08, 0x48,  // ...H
      0x54,   0x54, 0x50, 0x2f,  // TTP/
      0x31,   0x2e, 0x31,        // 1.1
  };
  absl::string_view data(arr, ABSL_ARRAYSIZE(arr));
  QuicStreamFrame frame(stream_->id(), true, 0, data);
  // Verify that we don't crash when we get a invalid headers in stream frame.
  if (GetQuicReloadableFlag(quic_fin_before_completed_http_headers) &&
      UsesHttp3()) {
    EXPECT_CALL(
        *connection_,
        CloseConnection(QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_SPDY_STREAM, _, _));
  }
  stream_->OnStreamFrame(frame);
}

// Basic QuicSimpleServerBackend that implements its behavior through mocking.
class TestQuicSimpleServerBackend : public QuicSimpleServerBackend {
 public:
  TestQuicSimpleServerBackend() = default;
  ~TestQuicSimpleServerBackend() override = default;

  // QuicSimpleServerBackend:
  bool InitializeBackend(const std::string& /*backend_url*/) override {
    return true;
  }
  bool IsBackendInitialized() const override { return true; }
  MOCK_METHOD(void, FetchResponseFromBackend,
              (const quiche::HttpHeaderBlock&, const std::string&,
               RequestHandler*),
              (override));
  MOCK_METHOD(void, HandleConnectHeaders,
              (const quiche::HttpHeaderBlock&, RequestHandler*), (override));
  MOCK_METHOD(void, HandleConnectData,
              (absl::string_view, bool, RequestHandler*), (override));
  void CloseBackendResponseStream(
      RequestHandler* /*request_handler*/) override {}
};

ACTION_P(SendHeadersResponse, response_ptr) {
  arg1->OnResponseBackendComplete(response_ptr);
}

ACTION_P(SendStreamData, data, close_stream) {
  arg2->SendStreamData(data, close_stream);
}

ACTION_P(TerminateStream, error) { arg1->TerminateStreamWithError(error); }

TEST_P(QuicSimpleServerStreamTest, ConnectSendsIntermediateResponses) {
  auto test_backend = std::make_unique<TestQuicSimpleServerBackend>();
  TestQuicSimpleServerBackend* test_backend_ptr = test_backend.get();
  ReplaceBackend(std::move(test_backend));

  constexpr absl::string_view kRequestBody = "\x11\x11";
  quiche::HttpHeaderBlock response_headers;
  response_headers[":status"] = "200";
  QuicBackendResponse headers_response;
  headers_response.set_headers(response_headers.Clone());
  headers_response.set_response_type(QuicBackendResponse::INCOMPLETE_RESPONSE);
  constexpr absl::string_view kBody1 = "\x22\x22";
  constexpr absl::string_view kBody2 = "\x33\x33";

  // Expect an initial headers-only request to result in a headers-only
  // incomplete response. Then a data frame without fin, resulting in stream
  // data. Then a data frame with fin, resulting in stream data with fin.
  InSequence s;
  EXPECT_CALL(*test_backend_ptr, HandleConnectHeaders(_, _))
      .WillOnce(SendHeadersResponse(&headers_response));
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  EXPECT_CALL(*test_backend_ptr, HandleConnectData(kRequestBody, false, _))
      .WillOnce(SendStreamData(kBody1,
                               /*close_stream=*/false));
  EXPECT_CALL(*stream_, WriteOrBufferBody(kBody1, false));
  EXPECT_CALL(*test_backend_ptr, HandleConnectData(kRequestBody, true, _))
      .WillOnce(SendStreamData(kBody2,
                               /*close_stream=*/true));
  EXPECT_CALL(*stream_, WriteOrBufferBody(kBody2, true));

  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeaderBlockEnd(128, 128);

  stream_->OnStreamHeaderList(/*fin=*/false, kFakeFrameLen, header_list);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      kRequestBody.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = UsesHttp3()
                         ? absl::StrCat(header.AsStringView(), kRequestBody)
                         : std::string(kRequestBody);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/true, data.length(), data));

  // Expect to not go through SendResponse().
  EXPECT_FALSE(stream_->send_response_was_called());
  EXPECT_FALSE(stream_->send_error_response_was_called());
}

TEST_P(QuicSimpleServerStreamTest, ErrorOnUnhandledConnect) {
  // Expect single set of failure response headers with FIN in response to the
  // headers. Then, expect abrupt stream termination in response to the body.
  EXPECT_CALL(*stream_, WriteHeadersMock(true));
  if (VersionHasIetfQuicFrames(connection_->transport_version())) {
    EXPECT_CALL(session_, MaybeSendStopSendingFrame(stream_->id(), _));
  }
  EXPECT_CALL(session_, MaybeSendRstStreamFrame(stream_->id(), _, _));

  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeaderBlockEnd(128, 128);
  constexpr absl::string_view kRequestBody = "\x11\x11";

  stream_->OnStreamHeaderList(/*fin=*/false, kFakeFrameLen, header_list);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      kRequestBody.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = UsesHttp3()
                         ? absl::StrCat(header.AsStringView(), kRequestBody)
                         : std::string(kRequestBody);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/true, /*offset=*/0, data));

  // Expect failure to not go through SendResponse().
  EXPECT_FALSE(stream_->send_response_was_called());
  EXPECT_FALSE(stream_->send_error_response_was_called());
}

TEST_P(QuicSimpleServerStreamTest, ConnectWithInvalidHeader) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  // QUIC requires lower-case header names.
  header_list.OnHeader("InVaLiD-HeAdEr", "Well that's just wrong!");
  header_list.OnHeaderBlockEnd(128, 128);

  if (UsesHttp3()) {
    EXPECT_CALL(session_,
                MaybeSendStopSendingFrame(_, QuicResetStreamError::FromInternal(
                                                 QUIC_STREAM_NO_ERROR)))
        .Times(1);
  } else {
    EXPECT_CALL(
        session_,
        MaybeSendRstStreamFrame(
            _, QuicResetStreamError::FromInternal(QUIC_STREAM_NO_ERROR), _))
        .Times(1);
  }
  EXPECT_CALL(*stream_, WriteHeadersMock(/*fin=*/false));
  stream_->OnStreamHeaderList(/*fin=*/false, kFakeFrameLen, header_list);
  EXPECT_FALSE(stream_->send_response_was_called());
  EXPECT_TRUE(stream_->send_error_response_was_called());
}

TEST_P(QuicSimpleServerStreamTest, BackendCanTerminateStream) {
  auto test_backend = std::make_unique<TestQuicSimpleServerBackend>();
  TestQuicSimpleServerBackend* test_backend_ptr = test_backend.get();
  ReplaceBackend(std::move(test_backend));

  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(
          Invoke(&session_, &MockQuicSimpleServerSession::ConsumeData));

  QuicResetStreamError expected_error =
      QuicResetStreamError::FromInternal(QUIC_STREAM_CONNECT_ERROR);
  EXPECT_CALL(*test_backend_ptr, HandleConnectHeaders(_, _))
      .WillOnce(TerminateStream(expected_error));
  if (VersionHasIetfQuicFrames(connection_->transport_version())) {
    EXPECT_CALL(session_,
                MaybeSendStopSendingFrame(stream_->id(), expected_error));
  }
  EXPECT_CALL(session_,
              MaybeSendRstStreamFrame(stream_->id(), expected_error, _));

  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeaderBlockEnd(128, 128);
  stream_->OnStreamHeaderList(/*fin=*/false, kFakeFrameLen, header_list);
}

}  // namespace
}  // namespace test
}  // namespace quic
