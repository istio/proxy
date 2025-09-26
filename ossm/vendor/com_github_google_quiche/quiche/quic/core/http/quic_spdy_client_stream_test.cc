// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_spdy_client_stream.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/http/quic_spdy_client_session.h"
#include "quiche/quic/core/http/spdy_utils.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/simple_buffer_allocator.h"

using quiche::HttpHeaderBlock;
using testing::_;
using testing::ElementsAre;
using testing::StrictMock;

namespace quic {
namespace test {

namespace {

class MockQuicSpdyClientSession : public QuicSpdyClientSession {
 public:
  explicit MockQuicSpdyClientSession(
      const ParsedQuicVersionVector& supported_versions,
      QuicConnection* connection)
      : QuicSpdyClientSession(DefaultQuicConfig(), supported_versions,
                              connection, QuicServerId("example.com", 443),
                              &crypto_config_),
        crypto_config_(crypto_test_utils::ProofVerifierForTesting()) {}
  MockQuicSpdyClientSession(const MockQuicSpdyClientSession&) = delete;
  MockQuicSpdyClientSession& operator=(const MockQuicSpdyClientSession&) =
      delete;
  ~MockQuicSpdyClientSession() override = default;

  MOCK_METHOD(bool, WriteControlFrame,
              (const QuicFrame& frame, TransmissionType type), (override));

  using QuicSession::ActivateStream;

 private:
  QuicCryptoClientConfig crypto_config_;
};

class QuicSpdyClientStreamTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  class StreamVisitor;

  QuicSpdyClientStreamTest()
      : connection_(new StrictMock<MockQuicConnection>(
            &helper_, &alarm_factory_, Perspective::IS_CLIENT,
            SupportedVersions(GetParam()))),
        session_(connection_->supported_versions(), connection_),
        body_("hello world") {
    session_.Initialize();
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    connection_->SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<NullEncrypter>(connection_->perspective()));
    headers_[":status"] = "200";
    headers_["content-length"] = "11";

    auto stream = std::make_unique<QuicSpdyClientStream>(
        GetNthClientInitiatedBidirectionalStreamId(
            connection_->transport_version(), 0),
        &session_, BIDIRECTIONAL);
    stream_ = stream.get();
    session_.ActivateStream(std::move(stream));

    stream_visitor_ = std::make_unique<StreamVisitor>();
    stream_->set_visitor(stream_visitor_.get());
  }

  class StreamVisitor : public QuicSpdyClientStream::Visitor {
    void OnClose(QuicSpdyStream* stream) override {
      QUIC_DVLOG(1) << "stream " << stream->id();
    }
  };

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;

  MockQuicSpdyClientSession session_;
  QuicSpdyClientStream* stream_;
  std::unique_ptr<StreamVisitor> stream_visitor_;
  HttpHeaderBlock headers_;
  std::string body_;
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicSpdyClientStreamTest,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicSpdyClientStreamTest, TestReceivingIllegalResponseStatusCode) {
  headers_[":status"] = "200 ok";

  EXPECT_CALL(session_, WriteControlFrame(_, _));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD));
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  EXPECT_THAT(stream_->stream_error(),
              IsStreamError(QUIC_BAD_APPLICATION_PAYLOAD));
  EXPECT_EQ(stream_->ietf_application_error(),
            static_cast<uint64_t>(QuicHttp3ErrorCode::GENERAL_PROTOCOL_ERROR));
}

TEST_P(QuicSpdyClientStreamTest, InvalidResponseHeader) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  auto headers = AsHeaderList(std::vector<std::pair<std::string, std::string>>{
      {":status", "200"}, {":path", "/foo"}});
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD));
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  EXPECT_THAT(stream_->stream_error(),
              IsStreamError(QUIC_BAD_APPLICATION_PAYLOAD));
  EXPECT_EQ(stream_->ietf_application_error(),
            static_cast<uint64_t>(QuicHttp3ErrorCode::GENERAL_PROTOCOL_ERROR));
}

TEST_P(QuicSpdyClientStreamTest, MissingStatusCode) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  auto headers = AsHeaderList(
      std::vector<std::pair<std::string, std::string>>{{"key", "value"}});
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD));
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  EXPECT_THAT(stream_->stream_error(),
              IsStreamError(QUIC_BAD_APPLICATION_PAYLOAD));
  EXPECT_EQ(stream_->ietf_application_error(),
            static_cast<uint64_t>(QuicHttp3ErrorCode::GENERAL_PROTOCOL_ERROR));
}

TEST_P(QuicSpdyClientStreamTest, TestFraming) {
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), body_)
                         : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_P(QuicSpdyClientStreamTest, HostAllowedInResponseHeader) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  auto headers = AsHeaderList(std::vector<std::pair<std::string, std::string>>{
      {":status", "200"}, {"host", "example.com"}});
  EXPECT_CALL(*connection_, OnStreamReset(stream_->id(), _)).Times(0u);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  EXPECT_THAT(stream_->stream_error(), IsStreamError(QUIC_STREAM_NO_ERROR));
  EXPECT_EQ(stream_->ietf_application_error(),
            static_cast<uint64_t>(QuicHttp3ErrorCode::HTTP3_NO_ERROR));
}

TEST_P(QuicSpdyClientStreamTest, Test100ContinueBeforeSuccessful) {
  // First send 100 Continue.
  headers_[":status"] = "100";
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  ASSERT_EQ(stream_->preliminary_headers().size(), 1);
  EXPECT_EQ("100",
            stream_->preliminary_headers().front().find(":status")->second);
  EXPECT_EQ(0u, stream_->response_headers().size());
  EXPECT_EQ(100, stream_->response_code());
  EXPECT_EQ("", stream_->data());
  // Then send 200 OK.
  headers_[":status"] = "200";
  headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), body_)
                         : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  // Make sure the 200 response got parsed correctly.
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
  // Make sure the 100 response is still available.
  ASSERT_EQ(stream_->preliminary_headers().size(), 1);
  EXPECT_EQ("100",
            stream_->preliminary_headers().front().find(":status")->second);
}

TEST_P(QuicSpdyClientStreamTest, TestUnknownInformationalBeforeSuccessful) {
  // First send 199, an unknown Informational (1XX).
  headers_[":status"] = "199";
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  ASSERT_EQ(stream_->preliminary_headers().size(), 1);
  EXPECT_EQ("199",
            stream_->preliminary_headers().front().find(":status")->second);
  EXPECT_EQ(0u, stream_->response_headers().size());
  EXPECT_EQ(199, stream_->response_code());
  EXPECT_EQ("", stream_->data());
  // Then send 200 OK.
  headers_[":status"] = "200";
  headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), body_)
                         : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  // Make sure the 200 response got parsed correctly.
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
  // Make sure the 199 response is still available.
  ASSERT_EQ(stream_->preliminary_headers().size(), 1);
  EXPECT_EQ("199",
            stream_->preliminary_headers().front().find(":status")->second);
}

TEST_P(QuicSpdyClientStreamTest, TestMultipleInformationalBeforeSuccessful) {
  // First send 100 Continue.
  headers_[":status"] = "100";
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  ASSERT_EQ(stream_->preliminary_headers().size(), 1);
  EXPECT_EQ("100",
            stream_->preliminary_headers().front().find(":status")->second);
  EXPECT_EQ(0u, stream_->response_headers().size());
  EXPECT_EQ(100, stream_->response_code());
  EXPECT_EQ("", stream_->data());

  // Then send 199, an unknown Informational (1XX).
  headers_[":status"] = "199";
  headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  ASSERT_EQ(stream_->preliminary_headers().size(), 2);
  EXPECT_EQ("100",
            stream_->preliminary_headers().front().find(":status")->second);
  EXPECT_EQ("199",
            stream_->preliminary_headers().back().find(":status")->second);
  EXPECT_EQ(0u, stream_->response_headers().size());
  EXPECT_EQ(199, stream_->response_code());
  EXPECT_EQ("", stream_->data());

  // Then send 200 OK.
  headers_[":status"] = "200";
  headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), body_)
                         : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));

  // Make sure the 200 response got parsed correctly.
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());

  // Make sure the informational responses are still available.
  ASSERT_EQ(stream_->preliminary_headers().size(), 2);
  EXPECT_EQ("100",
            stream_->preliminary_headers().front().find(":status")->second);
  EXPECT_EQ("199",
            stream_->preliminary_headers().back().find(":status")->second);
}

TEST_P(QuicSpdyClientStreamTest, TestReceiving101) {
  // 101 "Switching Protocols" is forbidden in HTTP/3 as per the
  // "HTTP Upgrade" section of draft-ietf-quic-http.
  headers_[":status"] = "101";
  EXPECT_CALL(session_, WriteControlFrame(_, _));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD));
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  EXPECT_THAT(stream_->stream_error(),
              IsStreamError(QUIC_BAD_APPLICATION_PAYLOAD));
}

TEST_P(QuicSpdyClientStreamTest, TestFramingOnePacket) {
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), body_)
                         : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_P(QuicSpdyClientStreamTest,
       QUIC_TEST_DISABLED_IN_CHROME(TestFramingExtraData)) {
  std::string large_body = "hello world!!!!!!";

  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  // The headers should parse successfully.
  EXPECT_THAT(stream_->stream_error(), IsQuicStreamNoError());
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      large_body.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), large_body)
                         : large_body;
  EXPECT_CALL(session_, WriteControlFrame(_, _));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD));

  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));

  EXPECT_NE(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(stream_->ietf_application_error(),
            static_cast<uint64_t>(QuicHttp3ErrorCode::GENERAL_PROTOCOL_ERROR));
}

// Test that receiving trailing headers (on the headers stream), containing a
// final offset, results in the stream being closed at that byte offset.
TEST_P(QuicSpdyClientStreamTest, ReceivingTrailers) {
  // There is no kFinalOffsetHeaderKey if trailers are sent on the
  // request/response stream.
  if (VersionUsesHttp3(connection_->transport_version())) {
    return;
  }

  // Send headers as usual.
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);

  // Send trailers before sending the body. Even though a FIN has been received
  // the stream should not be closed, as it does not yet have all the data bytes
  // promised by the final offset field.
  HttpHeaderBlock trailer_block;
  trailer_block["trailer key"] = "trailer value";
  trailer_block[kFinalOffsetHeaderKey] = absl::StrCat(body_.size());
  auto trailers = AsHeaderList(trailer_block);
  stream_->OnStreamHeaderList(true, trailers.uncompressed_header_bytes(),
                              trailers);

  // Now send the body, which should close the stream as the FIN has been
  // received, as well as all data.
  quiche::QuicheBuffer header = HttpEncoder::SerializeDataFrameHeader(
      body_.length(), quiche::SimpleBufferAllocator::Get());
  std::string data = VersionUsesHttp3(connection_->transport_version())
                         ? absl::StrCat(header.AsStringView(), body_)
                         : body_;
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, data));
  EXPECT_TRUE(stream_->reading_stopped());
}

TEST_P(QuicSpdyClientStreamTest, Capsules) {
  if (!VersionUsesHttp3(connection_->transport_version())) {
    return;
  }
  SavingHttp3DatagramVisitor h3_datagram_visitor;
  stream_->RegisterHttp3DatagramVisitor(&h3_datagram_visitor);
  headers_.erase("content-length");
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  std::string capsule_data = {0, 6, 1, 2, 3, 4, 5, 6, 0x17, 4, 1, 2, 3, 4};
  quiche::QuicheBuffer data_frame_header =
      HttpEncoder::SerializeDataFrameHeader(
          capsule_data.length(), quiche::SimpleBufferAllocator::Get());
  std::string stream_data =
      absl::StrCat(data_frame_header.AsStringView(), capsule_data);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, stream_data));
  // Datagram capsule.
  std::string http_datagram_payload = {1, 2, 3, 4, 5, 6};
  EXPECT_THAT(h3_datagram_visitor.received_h3_datagrams(),
              ElementsAre(SavingHttp3DatagramVisitor::SavedHttp3Datagram{
                  stream_->id(), http_datagram_payload}));
  // Unknown capsule.
  uint64_t capsule_type = 0x17u;
  std::string unknown_capsule_payload = {1, 2, 3, 4};
  EXPECT_THAT(h3_datagram_visitor.received_unknown_capsules(),
              ElementsAre(SavingHttp3DatagramVisitor::SavedUnknownCapsule{
                  stream_->id(), capsule_type, unknown_capsule_payload}));
  // Cleanup.
  stream_->UnregisterHttp3DatagramVisitor();
}

TEST_P(QuicSpdyClientStreamTest, CapsulesOnUnsuccessfulResponse) {
  if (!VersionUsesHttp3(connection_->transport_version())) {
    return;
  }
  SavingHttp3DatagramVisitor h3_datagram_visitor;
  stream_->RegisterHttp3DatagramVisitor(&h3_datagram_visitor);
  headers_[":status"] = "401";
  headers_.erase("content-length");
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  std::string capsule_data = {0, 6, 1, 2, 3, 4, 5, 6, 0x17, 4, 1, 2, 3, 4};
  quiche::QuicheBuffer data_frame_header =
      HttpEncoder::SerializeDataFrameHeader(
          capsule_data.length(), quiche::SimpleBufferAllocator::Get());
  std::string stream_data =
      absl::StrCat(data_frame_header.AsStringView(), capsule_data);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, stream_data));
  // Ensure received capsules were ignored.
  EXPECT_TRUE(h3_datagram_visitor.received_h3_datagrams().empty());
  EXPECT_TRUE(h3_datagram_visitor.received_unknown_capsules().empty());
  // Cleanup.
  stream_->UnregisterHttp3DatagramVisitor();
}

}  // namespace
}  // namespace test
}  // namespace quic
