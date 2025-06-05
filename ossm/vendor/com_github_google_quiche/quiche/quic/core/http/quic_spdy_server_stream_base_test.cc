// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_spdy_server_stream_base.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/qpack/value_splitting_header_list.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/http/http_header_block.h"

using testing::_;

namespace quic {
namespace test {
namespace {

class TestQuicSpdyServerStream : public QuicSpdyServerStreamBase {
 public:
  TestQuicSpdyServerStream(QuicStreamId id, QuicSpdySession* session,
                           StreamType type)
      : QuicSpdyServerStreamBase(id, session, type) {}

  void OnBodyAvailable() override {}
};

class QuicSpdyServerStreamBaseTest : public QuicTest {
 protected:
  QuicSpdyServerStreamBaseTest()
      : session_(new MockQuicConnection(&helper_, &alarm_factory_,
                                        Perspective::IS_SERVER)) {
    session_.Initialize();
    session_.connection()->SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<NullEncrypter>(session_.perspective()));
    stream_ =
        new TestQuicSpdyServerStream(GetNthClientInitiatedBidirectionalStreamId(
                                         session_.transport_version(), 0),
                                     &session_, BIDIRECTIONAL);
    session_.ActivateStream(absl::WrapUnique(stream_));
    helper_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

  QuicSpdyServerStreamBase* stream_ = nullptr;
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicSpdySession session_;
};

TEST_F(QuicSpdyServerStreamBaseTest,
       SendQuicRstStreamNoErrorWithEarlyResponse) {
  stream_->StopReading();

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
  QuicStreamPeer::SetFinSent(stream_);
  stream_->CloseWriteSide();
}

TEST_F(QuicSpdyServerStreamBaseTest,
       DoNotSendQuicRstStreamNoErrorWithRstReceived) {
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_CALL(session_,
              MaybeSendRstStreamFrame(
                  _,
                  QuicResetStreamError::FromInternal(
                      VersionHasIetfQuicFrames(session_.transport_version())
                          ? QUIC_STREAM_CANCELLED
                          : QUIC_RST_ACKNOWLEDGEMENT),
                  _))
      .Times(1);
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_->id(),
                               QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  if (VersionHasIetfQuicFrames(session_.transport_version())) {
    // Create and inject a STOP SENDING frame to complete the close
    // of the stream. This is only needed for version 99/IETF QUIC.
    QuicStopSendingFrame stop_sending(kInvalidControlFrameId, stream_->id(),
                                      QUIC_STREAM_CANCELLED);
    session_.OnStopSendingFrame(stop_sending);
  }

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicSpdyServerStreamBaseTest, AllowExtendedConnect) {
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeader(":protocol", "webtransport");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeaderBlockEnd(128, 128);
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_EQ(GetQuicReloadableFlag(quic_act_upon_invalid_header) &&
                !session_.allow_extended_connect(),
            stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, AllowExtendedConnectProtocolFirst) {
  QuicHeaderList header_list;
  header_list.OnHeader(":protocol", "webtransport");
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeaderBlockEnd(128, 128);
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_EQ(GetQuicReloadableFlag(quic_act_upon_invalid_header) &&
                !session_.allow_extended_connect(),
            stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidExtendedConnect) {
  if (!session_.version().UsesHttp3()) {
    return;
  }
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeader(":protocol", "webtransport");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, VanillaConnectAllowed) {
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeaderBlockEnd(128, 128);
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_FALSE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidVanillaConnect) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "CONNECT");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidNonConnectWithProtocol) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "GET");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeader(":protocol", "webtransport");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidRequestWithoutScheme) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request without :scheme should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":method", "GET");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidRequestWithoutAuthority) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request without :authority should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":method", "GET");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidRequestWithoutMethod) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request without :method should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidRequestWithoutPath) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request without :path should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":method", "POST");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, InvalidRequestHeader) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request without :path should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":method", "POST");
  header_list.OnHeader("invalid:header", "value");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, HostHeaderWithoutAuthority) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request with host but without authority should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader("host", "www.google.com:4433");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":method", "POST");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, HostHeaderWitDifferentAuthority) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request with host that does not match authority should be rejected.
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":method", "POST");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeader("host", "mail.google.com:4433");
  header_list.OnHeaderBlockEnd(128, 128);

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_TRUE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, ValidHostHeader) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // A request with host that matches authority should be accepted.
  QuicHeaderList header_list;
  header_list.OnHeader(":authority", "www.google.com:4433");
  header_list.OnHeader(":scheme", "http");
  header_list.OnHeader(":method", "POST");
  header_list.OnHeader(":path", "/path");
  header_list.OnHeader("host", "www.google.com:4433");
  header_list.OnHeaderBlockEnd(128, 128);

  stream_->OnStreamHeaderList(/*fin=*/false, 0, header_list);
  EXPECT_FALSE(stream_->rst_sent());
}

TEST_F(QuicSpdyServerStreamBaseTest, EmptyHeaders) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  quiche::HttpHeaderBlock empty_header;
  quic::test::NoopQpackStreamSenderDelegate encoder_stream_sender_delegate;
  NoopDecoderStreamErrorDelegate decoder_stream_error_delegate;
  auto qpack_encoder = std::make_unique<quic::QpackEncoder>(
      &decoder_stream_error_delegate, HuffmanEncoding::kEnabled,
      CookieCrumbling::kEnabled);
  qpack_encoder->set_qpack_stream_sender_delegate(
      &encoder_stream_sender_delegate);
  std::string payload =
      qpack_encoder->EncodeHeaderList(stream_->id(), empty_header, nullptr);
  std::string headers_frame_header =
      quic::HttpEncoder::SerializeHeadersFrameHeader(payload.length());

  EXPECT_CALL(
      session_,
      MaybeSendRstStreamFrame(
          _, QuicResetStreamError::FromInternal(QUIC_BAD_APPLICATION_PAYLOAD),
          _));
  stream_->OnStreamFrame(QuicStreamFrame(
      stream_->id(), true, 0, absl::StrCat(headers_frame_header, payload)));
  EXPECT_TRUE(stream_->rst_sent());
}

}  // namespace
}  // namespace test
}  // namespace quic
