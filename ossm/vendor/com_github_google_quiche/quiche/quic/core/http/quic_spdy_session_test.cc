// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_spdy_session.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/http2/core/spdy_framer.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/frames/quic_stream_frame.h"
#include "quiche/quic/core/frames/quic_streams_blocked_frame.h"
#include "quiche/quic/core/http/http_constants.h"
#include "quiche/quic/core/http/http_encoder.h"
#include "quiche/quic/core/http/quic_header_list.h"
#include "quiche/quic/core/http/web_transport_http3.h"
#include "quiche/quic/core/qpack/qpack_header_table.h"
#include "quiche/quic/core/quic_config.h"
#include "quiche/quic/core/quic_crypto_stream.h"
#include "quiche/quic/core/quic_data_writer.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_stream.h"
#include "quiche/quic/core/quic_stream_priority.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/qpack/qpack_encoder_peer.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_flow_controller_peer.h"
#include "quiche/quic/test_tools/quic_session_peer.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_stream_send_buffer_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/common/quiche_endian.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using quiche::HttpHeaderBlock;
using spdy::kV3HighestPriority;
using spdy::Spdy3PriorityToHttp2Weight;
using spdy::SpdyFramer;
using spdy::SpdyPriority;
using spdy::SpdyPriorityIR;
using spdy::SpdySerializedFrame;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace quic {
namespace test {
namespace {

bool VerifyAndClearStopSendingFrame(const QuicFrame& frame) {
  EXPECT_EQ(STOP_SENDING_FRAME, frame.type);
  return ClearControlFrame(frame);
}

class TestCryptoStream : public QuicCryptoStream, public QuicCryptoHandshaker {
 public:
  explicit TestCryptoStream(QuicSession* session)
      : QuicCryptoStream(session),
        QuicCryptoHandshaker(this, session),
        encryption_established_(false),
        one_rtt_keys_available_(false),
        params_(new QuicCryptoNegotiatedParameters) {
    // Simulate a negotiated cipher_suite with a fake value.
    params_->cipher_suite = 1;
  }

  void EstablishZeroRttEncryption() {
    encryption_established_ = true;
    session()->connection()->SetEncrypter(
        ENCRYPTION_ZERO_RTT,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_ZERO_RTT));
  }

  void OnHandshakeMessage(const CryptoHandshakeMessage& /*message*/) override {
    encryption_established_ = true;
    one_rtt_keys_available_ = true;
    QuicErrorCode error;
    std::string error_details;
    session()->config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session()->config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    if (session()->version().UsesTls()) {
      if (session()->perspective() == Perspective::IS_CLIENT) {
        session()->config()->SetOriginalConnectionIdToSend(
            session()->connection()->connection_id());
        session()->config()->SetInitialSourceConnectionIdToSend(
            session()->connection()->connection_id());
      } else {
        session()->config()->SetInitialSourceConnectionIdToSend(
            session()->connection()->client_connection_id());
      }
      TransportParameters transport_parameters;
      EXPECT_TRUE(
          session()->config()->FillTransportParameters(&transport_parameters));
      error = session()->config()->ProcessTransportParameters(
          transport_parameters, /* is_resumption = */ false, &error_details);
    } else {
      CryptoHandshakeMessage msg;
      session()->config()->ToHandshakeMessage(&msg, transport_version());
      error =
          session()->config()->ProcessPeerHello(msg, CLIENT, &error_details);
    }
    EXPECT_THAT(error, IsQuicNoError());
    session()->OnNewEncryptionKeyAvailable(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
    session()->OnConfigNegotiated();
    if (session()->connection()->version().handshake_protocol ==
        PROTOCOL_TLS1_3) {
      session()->OnTlsHandshakeComplete();
    } else {
      session()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    }
    session()->DiscardOldEncryptionKey(ENCRYPTION_INITIAL);
  }

  // QuicCryptoStream implementation
  ssl_early_data_reason_t EarlyDataReason() const override {
    return ssl_early_data_unknown;
  }
  bool encryption_established() const override {
    return encryption_established_;
  }
  bool one_rtt_keys_available() const override {
    return one_rtt_keys_available_;
  }
  HandshakeState GetHandshakeState() const override {
    return one_rtt_keys_available() ? HANDSHAKE_COMPLETE : HANDSHAKE_START;
  }
  void SetServerApplicationStateForResumption(
      std::unique_ptr<ApplicationState> /*application_state*/) override {}
  std::unique_ptr<QuicDecrypter> AdvanceKeysAndCreateCurrentOneRttDecrypter()
      override {
    return nullptr;
  }
  std::unique_ptr<QuicEncrypter> CreateCurrentOneRttEncrypter() override {
    return nullptr;
  }
  const QuicCryptoNegotiatedParameters& crypto_negotiated_params()
      const override {
    return *params_;
  }
  CryptoMessageParser* crypto_message_parser() override {
    return QuicCryptoHandshaker::crypto_message_parser();
  }
  void OnPacketDecrypted(EncryptionLevel /*level*/) override {}
  void OnOneRttPacketAcknowledged() override {}
  void OnHandshakePacketSent() override {}
  void OnHandshakeDoneReceived() override {}
  void OnNewTokenReceived(absl::string_view /*token*/) override {}
  std::string GetAddressToken(
      const CachedNetworkParameters* /*cached_network_params*/) const override {
    return "";
  }
  bool ValidateAddressToken(absl::string_view /*token*/) const override {
    return true;
  }
  const CachedNetworkParameters* PreviousCachedNetworkParams() const override {
    return nullptr;
  }
  void SetPreviousCachedNetworkParams(
      CachedNetworkParameters /*cached_network_params*/) override {}

  MOCK_METHOD(void, OnCanWrite, (), (override));

  bool HasPendingCryptoRetransmission() const override { return false; }

  MOCK_METHOD(bool, HasPendingRetransmission, (), (const, override));

  void OnConnectionClosed(const QuicConnectionCloseFrame& /*frame*/,
                          ConnectionCloseSource /*source*/) override {}
  SSL* GetSsl() const override { return nullptr; }
  bool IsCryptoFrameExpectedForEncryptionLevel(
      EncryptionLevel level) const override {
    return level != ENCRYPTION_ZERO_RTT;
  }
  EncryptionLevel GetEncryptionLevelToSendCryptoDataOfSpace(
      PacketNumberSpace space) const override {
    switch (space) {
      case INITIAL_DATA:
        return ENCRYPTION_INITIAL;
      case HANDSHAKE_DATA:
        return ENCRYPTION_HANDSHAKE;
      case APPLICATION_DATA:
        return ENCRYPTION_FORWARD_SECURE;
      default:
        QUICHE_DCHECK(false);
        return NUM_ENCRYPTION_LEVELS;
    }
  }

  bool ExportKeyingMaterial(absl::string_view /*label*/,
                            absl::string_view /*context*/,
                            size_t /*result_len*/, std::string*
                            /*result*/) override {
    return false;
  }

 private:
  using QuicCryptoStream::session;

  bool encryption_established_;
  bool one_rtt_keys_available_;
  quiche::QuicheReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
};

class TestHeadersStream : public QuicHeadersStream {
 public:
  explicit TestHeadersStream(QuicSpdySession* session)
      : QuicHeadersStream(session) {}

  MOCK_METHOD(void, OnCanWrite, (), (override));
};

class TestStream : public QuicSpdyStream {
 public:
  TestStream(QuicStreamId id, QuicSpdySession* session, StreamType type)
      : QuicSpdyStream(id, session, type) {}

  TestStream(PendingStream* pending, QuicSpdySession* session)
      : QuicSpdyStream(pending, session) {}

  using QuicStream::CloseWriteSide;

  void OnBodyAvailable() override {}

  MOCK_METHOD(void, OnCanWrite, (), (override));
  MOCK_METHOD(bool, RetransmitStreamData,
              (QuicStreamOffset, QuicByteCount, bool, TransmissionType),
              (override));

  MOCK_METHOD(bool, HasPendingRetransmission, (), (const, override));

 protected:
  bool ValidateReceivedHeaders(const QuicHeaderList& /*header_list*/) override {
    return true;
  }
};

class TestSession : public QuicSpdySession {
 public:
  explicit TestSession(QuicConnection* connection)
      : QuicSpdySession(connection, nullptr, DefaultQuicConfig(),
                        CurrentSupportedVersions()),
        crypto_stream_(this),
        writev_consumes_all_data_(false) {
    this->connection()->SetEncrypter(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<TaggingEncrypter>(ENCRYPTION_FORWARD_SECURE));
    if (this->connection()->version().SupportsAntiAmplificationLimit()) {
      QuicConnectionPeer::SetAddressValidated(this->connection());
    }
  }

  ~TestSession() override { DeleteConnection(); }

  TestCryptoStream* GetMutableCryptoStream() override {
    return &crypto_stream_;
  }

  const TestCryptoStream* GetCryptoStream() const override {
    return &crypto_stream_;
  }

  TestStream* CreateOutgoingBidirectionalStream() override {
    TestStream* stream = new TestStream(GetNextOutgoingBidirectionalStreamId(),
                                        this, BIDIRECTIONAL);
    ActivateStream(absl::WrapUnique(stream));
    return stream;
  }

  TestStream* CreateIncomingStream(QuicStreamId id) override {
    // Enforce the limit on the number of open streams.
    if (!VersionHasIetfQuicFrames(connection()->transport_version()) &&
        stream_id_manager().num_open_incoming_streams() + 1 >
            max_open_incoming_bidirectional_streams()) {
      connection()->CloseConnection(
          QUIC_TOO_MANY_OPEN_STREAMS, "Too many streams!",
          ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
      return nullptr;
    } else {
      TestStream* stream = new TestStream(
          id, this,
          DetermineStreamType(id, connection()->version(), perspective(),
                              /*is_incoming=*/true, BIDIRECTIONAL));
      ActivateStream(absl::WrapUnique(stream));
      return stream;
    }
  }

  TestStream* CreateIncomingStream(PendingStream* pending) override {
    TestStream* stream = new TestStream(pending, this);
    ActivateStream(absl::WrapUnique(stream));
    return stream;
  }

  bool ShouldCreateIncomingStream(QuicStreamId /*id*/) override { return true; }

  bool ShouldCreateOutgoingBidirectionalStream() override { return true; }

  bool IsClosedStream(QuicStreamId id) {
    return QuicSession::IsClosedStream(id);
  }

  QuicStream* GetOrCreateStream(QuicStreamId stream_id) {
    return QuicSpdySession::GetOrCreateStream(stream_id);
  }

  QuicConsumedData WritevData(QuicStreamId id, size_t write_length,
                              QuicStreamOffset offset, StreamSendingState state,
                              TransmissionType type,
                              EncryptionLevel level) override {
    bool fin = state != NO_FIN;
    QuicConsumedData consumed(write_length, fin);
    if (!writev_consumes_all_data_) {
      consumed =
          QuicSession::WritevData(id, write_length, offset, state, type, level);
    }
    QuicSessionPeer::GetWriteBlockedStreams(this)->UpdateBytesForStream(
        id, consumed.bytes_consumed);
    return consumed;
  }

  void set_writev_consumes_all_data(bool val) {
    writev_consumes_all_data_ = val;
  }

  QuicConsumedData SendStreamData(QuicStream* stream) {
    if (!QuicUtils::IsCryptoStreamId(connection()->transport_version(),
                                     stream->id()) &&
        connection()->encryption_level() != ENCRYPTION_FORWARD_SECURE) {
      this->connection()->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    }
    QuicStreamPeer::SendBuffer(stream).SaveStreamData("not empty");
    QuicConsumedData consumed =
        WritevData(stream->id(), 9, 0, FIN, NOT_RETRANSMISSION,
                   GetEncryptionLevelToSendApplicationData());
    QuicStreamPeer::SendBuffer(stream).OnStreamDataConsumed(
        consumed.bytes_consumed);
    return consumed;
  }

  QuicConsumedData SendLargeFakeData(QuicStream* stream, int bytes) {
    QUICHE_DCHECK(writev_consumes_all_data_);
    return WritevData(stream->id(), bytes, 0, FIN, NOT_RETRANSMISSION,
                      GetEncryptionLevelToSendApplicationData());
  }

  WebTransportHttp3VersionSet LocallySupportedWebTransportVersions()
      const override {
    return locally_supported_web_transport_versions_;
  }
  void set_supports_webtransport(bool value) {
    locally_supported_web_transport_versions_ =
        value ? kDefaultSupportedWebTransportVersions
              : WebTransportHttp3VersionSet();
  }
  void set_locally_supported_web_transport_versions(
      WebTransportHttp3VersionSet versions) {
    locally_supported_web_transport_versions_ = std::move(versions);
  }

  HttpDatagramSupport LocalHttpDatagramSupport() override {
    return local_http_datagram_support_;
  }
  void set_local_http_datagram_support(HttpDatagramSupport value) {
    local_http_datagram_support_ = value;
  }

  MOCK_METHOD(void, OnAcceptChFrame, (const AcceptChFrame&), (override));

  using QuicSession::closed_streams;
  using QuicSession::pending_streams_size;
  using QuicSession::ShouldKeepConnectionAlive;
  using QuicSpdySession::settings;
  using QuicSpdySession::UsesPendingStreamForFrame;

 private:
  StrictMock<TestCryptoStream> crypto_stream_;

  bool writev_consumes_all_data_;
  WebTransportHttp3VersionSet locally_supported_web_transport_versions_;
  HttpDatagramSupport local_http_datagram_support_ = HttpDatagramSupport::kNone;
};

class QuicSpdySessionTestBase : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  bool ClearMaxStreamsControlFrame(const QuicFrame& frame) {
    if (frame.type == MAX_STREAMS_FRAME) {
      DeleteFrame(&const_cast<QuicFrame&>(frame));
      return true;
    }
    return false;
  }

 protected:
  explicit QuicSpdySessionTestBase(Perspective perspective,
                                   bool allow_extended_connect)
      : connection_(new StrictMock<MockQuicConnection>(
            &helper_, &alarm_factory_, perspective,
            SupportedVersions(GetParam()))),
        allow_extended_connect_(allow_extended_connect) {}

  void Initialize() {
    session_.emplace(connection_);
    if (qpack_maximum_dynamic_table_capacity_.has_value()) {
      session_->set_qpack_maximum_dynamic_table_capacity(
          *qpack_maximum_dynamic_table_capacity_);
    }
    if (connection_->perspective() == Perspective::IS_SERVER &&
        VersionUsesHttp3(transport_version())) {
      session_->set_allow_extended_connect(allow_extended_connect_);
    }
    session_->Initialize();
    session_->config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_->config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    if (VersionUsesHttp3(transport_version())) {
      QuicConfigPeer::SetReceivedMaxUnidirectionalStreams(
          session_->config(), kHttp3StaticUnidirectionalStreamCount);
    }
    QuicConfigPeer::SetReceivedInitialSessionFlowControlWindow(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesUnidirectional(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesIncomingBidirectional(
        session_->config(), kMinimumFlowControlSendWindow);
    QuicConfigPeer::SetReceivedInitialMaxStreamDataBytesOutgoingBidirectional(
        session_->config(), kMinimumFlowControlSendWindow);
    session_->OnConfigNegotiated();
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    TestCryptoStream* crypto_stream = session_->GetMutableCryptoStream();
    EXPECT_CALL(*crypto_stream, HasPendingRetransmission())
        .Times(testing::AnyNumber());
    writer_ = static_cast<MockPacketWriter*>(
        QuicConnectionPeer::GetWriter(session_->connection()));
  }

  void CheckClosedStreams() {
    QuicStreamId first_stream_id = QuicUtils::GetFirstBidirectionalStreamId(
        transport_version(), Perspective::IS_CLIENT);
    if (!QuicVersionUsesCryptoFrames(transport_version())) {
      first_stream_id = QuicUtils::GetCryptoStreamId(transport_version());
    }
    for (QuicStreamId i = first_stream_id; i < 100; i++) {
      if (closed_streams_.find(i) == closed_streams_.end()) {
        EXPECT_FALSE(session_->IsClosedStream(i)) << " stream id: " << i;
      } else {
        EXPECT_TRUE(session_->IsClosedStream(i)) << " stream id: " << i;
      }
    }
  }

  void CloseStream(QuicStreamId id) {
    if (!VersionHasIetfQuicFrames(transport_version())) {
      EXPECT_CALL(*connection_, SendControlFrame(_))
          .WillOnce(&ClearControlFrame);
    } else {
      // IETF QUIC has two frames, RST_STREAM and STOP_SENDING
      EXPECT_CALL(*connection_, SendControlFrame(_))
          .Times(2)
          .WillRepeatedly(&ClearControlFrame);
    }
    EXPECT_CALL(*connection_, OnStreamReset(id, _));

    // QPACK streams might write data upon stream reset. Let the test session
    // handle the data.
    session_->set_writev_consumes_all_data(true);

    session_->ResetStream(id, QUIC_STREAM_CANCELLED);
    closed_streams_.insert(id);
  }

  ParsedQuicVersion version() const { return connection_->version(); }

  QuicTransportVersion transport_version() const {
    return connection_->transport_version();
  }

  QuicStreamId GetNthClientInitiatedBidirectionalId(int n) {
    return GetNthClientInitiatedBidirectionalStreamId(transport_version(), n);
  }

  QuicStreamId GetNthServerInitiatedBidirectionalId(int n) {
    return GetNthServerInitiatedBidirectionalStreamId(transport_version(), n);
  }

  QuicStreamId IdDelta() {
    return QuicUtils::StreamIdDelta(transport_version());
  }

  QuicStreamId StreamCountToId(QuicStreamCount stream_count,
                               Perspective perspective, bool bidirectional) {
    // Calculate and build up stream ID rather than use
    // GetFirst... because the test that relies on this method
    // needs to do the stream count where #1 is 0/1/2/3, and not
    // take into account that stream 0 is special.
    QuicStreamId id =
        ((stream_count - 1) * QuicUtils::StreamIdDelta(transport_version()));
    if (!bidirectional) {
      id |= 0x2;
    }
    if (perspective == Perspective::IS_SERVER) {
      id |= 0x1;
    }
    return id;
  }

  void CompleteHandshake() {
    if (VersionHasIetfQuicFrames(transport_version())) {
      EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
          .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
    }
    if (connection_->version().UsesTls() &&
        connection_->perspective() == Perspective::IS_SERVER) {
      // HANDSHAKE_DONE frame.
      EXPECT_CALL(*connection_, SendControlFrame(_))
          .WillOnce(&ClearControlFrame);
    }

    CryptoHandshakeMessage message;
    session_->GetMutableCryptoStream()->OnHandshakeMessage(message);
    testing::Mock::VerifyAndClearExpectations(writer_);
    testing::Mock::VerifyAndClearExpectations(connection_);
  }

  void ReceiveWebTransportSettings(WebTransportHttp3VersionSet versions =
                                       kDefaultSupportedWebTransportVersions) {
    SettingsFrame settings;
    settings.values[SETTINGS_H3_DATAGRAM] = 1;
    if (versions.IsSet(WebTransportHttp3Version::kDraft02)) {
      settings.values[SETTINGS_WEBTRANS_DRAFT00] = 1;
    }
    if (versions.IsSet(WebTransportHttp3Version::kDraft07)) {
      settings.values[SETTINGS_WEBTRANS_MAX_SESSIONS_DRAFT07] = 16;
    }
    settings.values[SETTINGS_ENABLE_CONNECT_PROTOCOL] = 1;
    std::string data = std::string(1, kControlStream) +
                       HttpEncoder::SerializeSettingsFrame(settings);
    QuicStreamId control_stream_id =
        session_->perspective() == Perspective::IS_SERVER
            ? GetNthClientInitiatedUnidirectionalStreamId(transport_version(),
                                                          3)
            : GetNthServerInitiatedUnidirectionalStreamId(transport_version(),
                                                          3);
    QuicStreamFrame frame(control_stream_id, /*fin=*/false, /*offset=*/0, data);
    session_->OnStreamFrame(frame);
  }

  void ReceiveWebTransportSession(WebTransportSessionId session_id) {
    QuicStreamFrame frame(session_id, /*fin=*/false, /*offset=*/0,
                          absl::string_view());
    session_->OnStreamFrame(frame);
    QuicSpdyStream* stream =
        static_cast<QuicSpdyStream*>(session_->GetOrCreateStream(session_id));
    QuicHeaderList headers;
    headers.OnHeader(":method", "CONNECT");
    headers.OnHeader(":protocol", "webtransport");
    stream->OnStreamHeaderList(/*fin=*/true, 0, headers);
    WebTransportHttp3* web_transport =
        session_->GetWebTransportSession(session_id);
    ASSERT_TRUE(web_transport != nullptr);
    quiche::HttpHeaderBlock header_block;
    web_transport->HeadersReceived(header_block);
  }

  void ReceiveWebTransportUnidirectionalStream(WebTransportSessionId session_id,
                                               QuicStreamId stream_id) {
    char buffer[256];
    QuicDataWriter data_writer(sizeof(buffer), buffer);
    ASSERT_TRUE(data_writer.WriteVarInt62(kWebTransportUnidirectionalStream));
    ASSERT_TRUE(data_writer.WriteVarInt62(session_id));
    ASSERT_TRUE(data_writer.WriteStringPiece("test data"));
    std::string data(buffer, data_writer.length());
    QuicStreamFrame frame(stream_id, /*fin=*/false, /*offset=*/0, data);
    session_->OnStreamFrame(frame);
  }

  void TestHttpDatagramSetting(HttpDatagramSupport local_support,
                               HttpDatagramSupport remote_support,
                               HttpDatagramSupport expected_support,
                               bool expected_datagram_supported);

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  bool allow_extended_connect_;
  std::optional<TestSession> session_;
  std::set<QuicStreamId> closed_streams_;
  std::optional<uint64_t> qpack_maximum_dynamic_table_capacity_;
  MockPacketWriter* writer_;
};

class QuicSpdySessionTestServer : public QuicSpdySessionTestBase {
 protected:
  QuicSpdySessionTestServer()
      : QuicSpdySessionTestBase(Perspective::IS_SERVER, true) {}
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicSpdySessionTestServer,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicSpdySessionTestServer, UsesPendingStreamsForFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  EXPECT_TRUE(session_->UsesPendingStreamForFrame(
      STREAM_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                        transport_version(), Perspective::IS_CLIENT)));
  EXPECT_TRUE(session_->UsesPendingStreamForFrame(
      RST_STREAM_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                            transport_version(), Perspective::IS_CLIENT)));
  EXPECT_FALSE(session_->UsesPendingStreamForFrame(
      RST_STREAM_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                            transport_version(), Perspective::IS_SERVER)));
  EXPECT_FALSE(session_->UsesPendingStreamForFrame(
      STOP_SENDING_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                              transport_version(), Perspective::IS_CLIENT)));
  EXPECT_FALSE(session_->UsesPendingStreamForFrame(
      RST_STREAM_FRAME, QuicUtils::GetFirstBidirectionalStreamId(
                            transport_version(), Perspective::IS_CLIENT)));
}

TEST_P(QuicSpdySessionTestServer, PeerAddress) {
  Initialize();
  EXPECT_EQ(QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort),
            session_->peer_address());
}

TEST_P(QuicSpdySessionTestServer, SelfAddress) {
  Initialize();
  EXPECT_TRUE(session_->self_address().IsInitialized());
}

TEST_P(QuicSpdySessionTestServer, OneRttKeysAvailable) {
  Initialize();
  EXPECT_FALSE(session_->OneRttKeysAvailable());
  CompleteHandshake();
  EXPECT_TRUE(session_->OneRttKeysAvailable());
}

TEST_P(QuicSpdySessionTestServer, IsClosedStreamDefault) {
  Initialize();
  // Ensure that no streams are initially closed.
  QuicStreamId first_stream_id = QuicUtils::GetFirstBidirectionalStreamId(
      transport_version(), Perspective::IS_CLIENT);
  if (!QuicVersionUsesCryptoFrames(transport_version())) {
    first_stream_id = QuicUtils::GetCryptoStreamId(transport_version());
  }
  for (QuicStreamId i = first_stream_id; i < 100; i++) {
    EXPECT_FALSE(session_->IsClosedStream(i)) << "stream id: " << i;
  }
}

TEST_P(QuicSpdySessionTestServer, AvailableStreams) {
  Initialize();
  ASSERT_TRUE(session_->GetOrCreateStream(
                  GetNthClientInitiatedBidirectionalId(2)) != nullptr);
  // Both client initiated streams with smaller stream IDs are available.
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(
      &*session_, GetNthClientInitiatedBidirectionalId(0)));
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(
      &*session_, GetNthClientInitiatedBidirectionalId(1)));
  ASSERT_TRUE(session_->GetOrCreateStream(
                  GetNthClientInitiatedBidirectionalId(1)) != nullptr);
  ASSERT_TRUE(session_->GetOrCreateStream(
                  GetNthClientInitiatedBidirectionalId(0)) != nullptr);
}

TEST_P(QuicSpdySessionTestServer, IsClosedStreamLocallyCreated) {
  Initialize();
  CompleteHandshake();
  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  EXPECT_EQ(GetNthServerInitiatedBidirectionalId(0), stream2->id());
  QuicSpdyStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  EXPECT_EQ(GetNthServerInitiatedBidirectionalId(1), stream4->id());

  CheckClosedStreams();
  CloseStream(GetNthServerInitiatedBidirectionalId(0));
  CheckClosedStreams();
  CloseStream(GetNthServerInitiatedBidirectionalId(1));
  CheckClosedStreams();
}

TEST_P(QuicSpdySessionTestServer, IsClosedStreamPeerCreated) {
  Initialize();
  CompleteHandshake();
  QuicStreamId stream_id1 = GetNthClientInitiatedBidirectionalId(0);
  QuicStreamId stream_id2 = GetNthClientInitiatedBidirectionalId(1);
  session_->GetOrCreateStream(stream_id1);
  session_->GetOrCreateStream(stream_id2);

  CheckClosedStreams();
  CloseStream(stream_id1);
  CheckClosedStreams();
  CloseStream(stream_id2);
  // Create a stream, and make another available.
  QuicStream* stream3 = session_->GetOrCreateStream(stream_id2 + 4);
  CheckClosedStreams();
  // Close one, but make sure the other is still not closed
  CloseStream(stream3->id());
  CheckClosedStreams();
}

TEST_P(QuicSpdySessionTestServer, MaximumAvailableOpenedStreams) {
  Initialize();
  if (VersionHasIetfQuicFrames(transport_version())) {
    // For IETF QUIC, we should be able to obtain the max allowed
    // stream ID, the next ID should fail. Since the actual limit
    // is not the number of open streams, we allocate the max and the max+2.
    // Get the max allowed stream ID, this should succeed.
    QuicStreamId stream_id = StreamCountToId(
        QuicSessionPeer::ietf_streamid_manager(&*session_)
            ->max_incoming_bidirectional_streams(),
        Perspective::IS_CLIENT,  // Client initates stream, allocs stream id.
        /*bidirectional=*/true);
    EXPECT_NE(nullptr, session_->GetOrCreateStream(stream_id));
    stream_id =
        StreamCountToId(QuicSessionPeer::ietf_streamid_manager(&*session_)
                            ->max_incoming_unidirectional_streams(),
                        Perspective::IS_CLIENT,
                        /*bidirectional=*/false);
    EXPECT_NE(nullptr, session_->GetOrCreateStream(stream_id));
    EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(2);
    // Get the (max allowed stream ID)++. These should all fail.
    stream_id =
        StreamCountToId(QuicSessionPeer::ietf_streamid_manager(&*session_)
                                ->max_incoming_bidirectional_streams() +
                            1,
                        Perspective::IS_CLIENT,
                        /*bidirectional=*/true);
    EXPECT_EQ(nullptr, session_->GetOrCreateStream(stream_id));

    stream_id =
        StreamCountToId(QuicSessionPeer::ietf_streamid_manager(&*session_)
                                ->max_incoming_unidirectional_streams() +
                            1,
                        Perspective::IS_CLIENT,
                        /*bidirectional=*/false);
    EXPECT_EQ(nullptr, session_->GetOrCreateStream(stream_id));
  } else {
    QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
    session_->GetOrCreateStream(stream_id);
    EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
    EXPECT_NE(
        nullptr,
        session_->GetOrCreateStream(
            stream_id +
            IdDelta() *
                (session_->max_open_incoming_bidirectional_streams() - 1)));
  }
}

TEST_P(QuicSpdySessionTestServer, TooManyAvailableStreams) {
  Initialize();
  QuicStreamId stream_id1 = GetNthClientInitiatedBidirectionalId(0);
  QuicStreamId stream_id2;
  EXPECT_NE(nullptr, session_->GetOrCreateStream(stream_id1));
  // A stream ID which is too large to create.
  stream_id2 = GetNthClientInitiatedBidirectionalId(
      2 * session_->MaxAvailableBidirectionalStreams() + 4);
  if (VersionHasIetfQuicFrames(transport_version())) {
    EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_STREAM_ID, _, _));
  } else {
    EXPECT_CALL(*connection_,
                CloseConnection(QUIC_TOO_MANY_AVAILABLE_STREAMS, _, _));
  }
  EXPECT_EQ(nullptr, session_->GetOrCreateStream(stream_id2));
}

TEST_P(QuicSpdySessionTestServer, ManyAvailableStreams) {
  Initialize();
  // When max_open_streams_ is 200, should be able to create 200 streams
  // out-of-order, that is, creating the one with the largest stream ID first.
  if (VersionHasIetfQuicFrames(transport_version())) {
    QuicSessionPeer::SetMaxOpenIncomingBidirectionalStreams(&*session_, 200);
  } else {
    QuicSessionPeer::SetMaxOpenIncomingStreams(&*session_, 200);
  }
  QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  // Create one stream.
  session_->GetOrCreateStream(stream_id);
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  // Stream count is 200, GetNth... starts counting at 0, so the 200'th stream
  // is 199. BUT actually we need to do 198 because the crypto stream (Stream
  // ID 0) has not been registered, but GetNth... assumes that it has.
  EXPECT_NE(nullptr, session_->GetOrCreateStream(
                         GetNthClientInitiatedBidirectionalId(198)));
}

TEST_P(QuicSpdySessionTestServer,
       DebugDFatalIfMarkingClosedStreamWriteBlocked) {
  Initialize();
  CompleteHandshake();
  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillRepeatedly(Return(WriteResult(WRITE_STATUS_OK, 0)));

  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  QuicStreamId closed_stream_id = stream2->id();
  // Close the stream.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(closed_stream_id, _));
  stream2->Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  std::string msg =
      absl::StrCat("Marking unknown stream ", closed_stream_id, " blocked.");
  EXPECT_QUIC_BUG(session_->MarkConnectionLevelWriteBlocked(closed_stream_id),
                  msg);
}

TEST_P(QuicSpdySessionTestServer, TooLargeStreamBlocked) {
  Initialize();
  // STREAMS_BLOCKED frame is IETF QUIC only.
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Simualte the situation where the incoming stream count is at its limit and
  // the peer is blocked.
  QuicSessionPeer::SetMaxOpenIncomingBidirectionalStreams(
      static_cast<QuicSession*>(&*session_), QuicUtils::GetMaxStreamCount());
  QuicStreamsBlockedFrame frame;
  frame.stream_count = QuicUtils::GetMaxStreamCount();
  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  EXPECT_CALL(debug_visitor, OnGoAwayFrameSent(_));
  session_->OnStreamsBlockedFrame(frame);
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteBundlesStreams) {
  Initialize();
  // Encryption needs to be established before data can be sent.
  CompleteHandshake();

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_->connection(), send_algorithm);

  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_->CreateOutgoingBidirectionalStream();

  session_->MarkConnectionLevelWriteBlocked(stream2->id());
  session_->MarkConnectionLevelWriteBlocked(stream6->id());
  session_->MarkConnectionLevelWriteBlocked(stream4->id());

  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*send_algorithm, GetCongestionWindow())
      .WillRepeatedly(Return(kMaxOutgoingPacketSize * 10));
  EXPECT_CALL(*send_algorithm, InRecovery()).WillRepeatedly(Return(false));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce([this, stream2]() {
    session_->SendStreamData(stream2);
  });
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce([this, stream4]() {
    session_->SendStreamData(stream4);
  });
  EXPECT_CALL(*stream6, OnCanWrite()).WillOnce([this, stream6]() {
    session_->SendStreamData(stream6);
  });

  // Expect that we only send one packet, the writes from different streams
  // should be bundled together.
  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  EXPECT_CALL(*send_algorithm, OnPacketSent(_, _, _, _, _));
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));
  session_->OnCanWrite();
  EXPECT_FALSE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteCongestionControlBlocks) {
  Initialize();
  CompleteHandshake();
  session_->set_writev_consumes_all_data(true);
  InSequence s;

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_->connection(), send_algorithm);

  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_->CreateOutgoingBidirectionalStream();

  session_->MarkConnectionLevelWriteBlocked(stream2->id());
  session_->MarkConnectionLevelWriteBlocked(stream6->id());
  session_->MarkConnectionLevelWriteBlocked(stream4->id());

  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce([this, stream2]() {
    session_->SendStreamData(stream2);
  });
  EXPECT_CALL(*send_algorithm, GetCongestionWindow()).Times(AnyNumber());
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream6, OnCanWrite()).WillOnce([this, stream6]() {
    session_->SendStreamData(stream6);
  });
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(false));
  // stream4->OnCanWrite is not called.

  session_->OnCanWrite();
  EXPECT_TRUE(session_->WillingAndAbleToWrite());

  // Still congestion-control blocked.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(false));
  session_->OnCanWrite();
  EXPECT_TRUE(session_->WillingAndAbleToWrite());

  // stream4->OnCanWrite is called once the connection stops being
  // congestion-control blocked.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce([this, stream4]() {
    session_->SendStreamData(stream4);
  });
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));
  session_->OnCanWrite();
  EXPECT_FALSE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteWriterBlocks) {
  Initialize();
  CompleteHandshake();
  // Drive congestion control manually in order to ensure that
  // application-limited signaling is handled correctly.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_->connection(), send_algorithm);
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(true));

  // Drive packet writer manually.
  EXPECT_CALL(*writer_, IsWriteBlocked()).WillRepeatedly(Return(true));
  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _)).Times(0);

  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();

  session_->MarkConnectionLevelWriteBlocked(stream2->id());

  EXPECT_CALL(*stream2, OnCanWrite()).Times(0);
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_)).Times(0);

  session_->OnCanWrite();
  EXPECT_TRUE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, BufferedHandshake) {
  Initialize();
  // This tests prioritization of the crypto stream when flow control limits are
  // reached. When CRYPTO frames are in use, there is no flow control for the
  // crypto handshake, so this test is irrelevant.
  if (QuicVersionUsesCryptoFrames(transport_version())) {
    return;
  }
  session_->set_writev_consumes_all_data(true);
  EXPECT_FALSE(session_->HasPendingHandshake());  // Default value.

  // Test that blocking other streams does not change our status.
  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  session_->MarkConnectionLevelWriteBlocked(stream2->id());
  EXPECT_FALSE(session_->HasPendingHandshake());

  TestStream* stream3 = session_->CreateOutgoingBidirectionalStream();
  session_->MarkConnectionLevelWriteBlocked(stream3->id());
  EXPECT_FALSE(session_->HasPendingHandshake());

  // Blocking (due to buffering of) the Crypto stream is detected.
  session_->MarkConnectionLevelWriteBlocked(
      QuicUtils::GetCryptoStreamId(transport_version()));
  EXPECT_TRUE(session_->HasPendingHandshake());

  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  session_->MarkConnectionLevelWriteBlocked(stream4->id());
  EXPECT_TRUE(session_->HasPendingHandshake());

  InSequence s;
  // Force most streams to re-register, which is common scenario when we block
  // the Crypto stream, and only the crypto stream can "really" write.

  // Due to prioritization, we *should* be asked to write the crypto stream
  // first.
  // Don't re-register the crypto stream (which signals complete writing).
  TestCryptoStream* crypto_stream = session_->GetMutableCryptoStream();
  EXPECT_CALL(*crypto_stream, OnCanWrite());

  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce([this, stream2]() {
    session_->SendStreamData(stream2);
  });
  EXPECT_CALL(*stream3, OnCanWrite()).WillOnce([this, stream3]() {
    session_->SendStreamData(stream3);
  });
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce([this, stream4]() {
    session_->SendStreamData(stream4);
    session_->MarkConnectionLevelWriteBlocked(stream4->id());
  });

  session_->OnCanWrite();
  EXPECT_TRUE(session_->WillingAndAbleToWrite());
  EXPECT_FALSE(session_->HasPendingHandshake());  // Crypto stream wrote.
}

TEST_P(QuicSpdySessionTestServer, OnCanWriteWithClosedStream) {
  Initialize();
  CompleteHandshake();
  session_->set_writev_consumes_all_data(true);
  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_->CreateOutgoingBidirectionalStream();

  session_->MarkConnectionLevelWriteBlocked(stream2->id());
  session_->MarkConnectionLevelWriteBlocked(stream6->id());
  session_->MarkConnectionLevelWriteBlocked(stream4->id());
  CloseStream(stream6->id());

  InSequence s;
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce([this, stream2]() {
    session_->SendStreamData(stream2);
  });
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce([this, stream4]() {
    session_->SendStreamData(stream4);
  });
  session_->OnCanWrite();
  EXPECT_FALSE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer,
       OnCanWriteLimitsNumWritesIfFlowControlBlocked) {
  Initialize();
  CompleteHandshake();
  // Drive congestion control manually in order to ensure that
  // application-limited signaling is handled correctly.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_->connection(), send_algorithm);
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(true));

  // Ensure connection level flow control blockage.
  QuicFlowControllerPeer::SetSendWindowOffset(session_->flow_controller(), 0);
  EXPECT_TRUE(session_->flow_controller()->IsBlocked());
  EXPECT_TRUE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());

  // Mark the crypto and headers streams as write blocked, we expect them to be
  // allowed to write later.
  if (!QuicVersionUsesCryptoFrames(transport_version())) {
    session_->MarkConnectionLevelWriteBlocked(
        QuicUtils::GetCryptoStreamId(transport_version()));
  }

  // Create a data stream, and although it is write blocked we never expect it
  // to be allowed to write as we are connection level flow control blocked.
  TestStream* stream = session_->CreateOutgoingBidirectionalStream();
  session_->MarkConnectionLevelWriteBlocked(stream->id());
  EXPECT_CALL(*stream, OnCanWrite()).Times(0);

  // The crypto and headers streams should be called even though we are
  // connection flow control blocked.
  if (!QuicVersionUsesCryptoFrames(transport_version())) {
    TestCryptoStream* crypto_stream = session_->GetMutableCryptoStream();
    EXPECT_CALL(*crypto_stream, OnCanWrite());
  }

  if (!VersionUsesHttp3(transport_version())) {
    TestHeadersStream* headers_stream;
    QuicSpdySessionPeer::SetHeadersStream(&*session_, nullptr);
    headers_stream = new TestHeadersStream(&*session_);
    QuicSpdySessionPeer::SetHeadersStream(&*session_, headers_stream);
    session_->MarkConnectionLevelWriteBlocked(
        QuicUtils::GetHeadersStreamId(transport_version()));
    EXPECT_CALL(*headers_stream, OnCanWrite());
  }

  // After the crypto and header streams perform a write, the connection will be
  // blocked by the flow control, hence it should become application-limited.
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));

  session_->OnCanWrite();
  EXPECT_FALSE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, SendGoAway) {
  Initialize();
  CompleteHandshake();
  if (VersionHasIetfQuicFrames(transport_version())) {
    // HTTP/3 GOAWAY has different semantic and thus has its own test.
    return;
  }
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallySendControlFrame));
  session_->SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_->goaway_sent());

  const QuicStreamId kTestStreamId = 5u;
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(0);
  EXPECT_CALL(*connection_,
              OnStreamReset(kTestStreamId, QUIC_STREAM_PEER_GOING_AWAY))
      .Times(0);
  EXPECT_TRUE(session_->GetOrCreateStream(kTestStreamId));
}

TEST_P(QuicSpdySessionTestServer, SendGoAwayWithoutEncryption) {
  Initialize();
  if (VersionHasIetfQuicFrames(transport_version())) {
    // HTTP/3 GOAWAY has different semantic and thus has its own test.
    return;
  }
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_PEER_GOING_AWAY, "Going Away.",
                      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(0);
  session_->SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_FALSE(session_->goaway_sent());
}

TEST_P(QuicSpdySessionTestServer, SendHttp3GoAway) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  // Send max stream id (currently 32 bits).
  EXPECT_CALL(debug_visitor, OnGoAwayFrameSent(/* stream_id = */ 0xfffffffc));
  session_->SendHttp3GoAway(QUIC_PEER_GOING_AWAY, "Goaway");
  EXPECT_TRUE(session_->goaway_sent());

  // New incoming stream is not reset.
  const QuicStreamId kTestStreamId =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 0);
  EXPECT_CALL(*connection_, OnStreamReset(kTestStreamId, _)).Times(0);
  EXPECT_TRUE(session_->GetOrCreateStream(kTestStreamId));

  // No more GOAWAY frames are sent because they could not convey new
  // information to the client.
  session_->SendHttp3GoAway(QUIC_PEER_GOING_AWAY, "Goaway");
}

TEST_P(QuicSpdySessionTestServer, SendHttp3GoAwayAndNoMoreMaxStreams) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  // Send max stream id (currently 32 bits).
  EXPECT_CALL(debug_visitor, OnGoAwayFrameSent(/* stream_id = */ 0xfffffffc));
  session_->SendHttp3GoAway(QUIC_PEER_GOING_AWAY, "Goaway");
  EXPECT_TRUE(session_->goaway_sent());

  // No MAX_STREAMS frames should be sent, even after all available
  // streams are opened and then closed.
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(0);

  const QuicStreamCount max_streams =
      QuicSessionPeer::ietf_streamid_manager(&*session_)
          ->max_incoming_bidirectional_streams();
  for (QuicStreamCount i = 0; i < max_streams; ++i) {
    QuicStreamId stream_id = StreamCountToId(
        i + 1,
        Perspective::IS_CLIENT,  // Client initates stream, allocs stream id.
        /*bidirectional=*/true);
    EXPECT_NE(nullptr, session_->GetOrCreateStream(stream_id));

    CloseStream(stream_id);
    QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_id,
                                 QUIC_STREAM_CANCELLED,
                                 /* bytes_written = */ 0);
    session_->OnRstStream(rst_frame);
  }
  EXPECT_EQ(max_streams, QuicSessionPeer::ietf_streamid_manager(&*session_)
                             ->max_incoming_bidirectional_streams());
}

TEST_P(QuicSpdySessionTestServer, SendHttp3GoAwayWithoutEncryption) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_PEER_GOING_AWAY, "Goaway",
                      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_->SendHttp3GoAway(QUIC_PEER_GOING_AWAY, "Goaway");
  EXPECT_FALSE(session_->goaway_sent());
}

TEST_P(QuicSpdySessionTestServer, SendHttp3GoAwayAfterStreamIsCreated) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  const QuicStreamId kTestStreamId =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 0);
  EXPECT_TRUE(session_->GetOrCreateStream(kTestStreamId));

  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  // Send max stream id (currently 32 bits).
  EXPECT_CALL(debug_visitor, OnGoAwayFrameSent(/* stream_id = */ 0xfffffffc));
  session_->SendHttp3GoAway(QUIC_PEER_GOING_AWAY, "Goaway");
  EXPECT_TRUE(session_->goaway_sent());

  // No more GOAWAY frames are sent because they could not convey new
  // information to the client.
  session_->SendHttp3GoAway(QUIC_PEER_GOING_AWAY, "Goaway");
}

TEST_P(QuicSpdySessionTestServer, DoNotSendGoAwayTwice) {
  Initialize();
  CompleteHandshake();
  if (VersionHasIetfQuicFrames(transport_version())) {
    // HTTP/3 GOAWAY doesn't have such restriction.
    return;
  }
  EXPECT_CALL(*connection_, SendControlFrame(_)).WillOnce(&ClearControlFrame);
  session_->SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_->goaway_sent());
  session_->SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
}

TEST_P(QuicSpdySessionTestServer, InvalidGoAway) {
  Initialize();
  if (VersionHasIetfQuicFrames(transport_version())) {
    // HTTP/3 GOAWAY has different semantics and thus has its own test.
    return;
  }
  QuicGoAwayFrame go_away(kInvalidControlFrameId, QUIC_PEER_GOING_AWAY,
                          session_->next_outgoing_bidirectional_stream_id(),
                          "");
  session_->OnGoAway(go_away);
}

TEST_P(QuicSpdySessionTestServer, Http3GoAwayLargerIdThanBefore) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  EXPECT_FALSE(session_->goaway_received());
  session_->OnHttp3GoAway(/* id = */ 0);
  EXPECT_TRUE(session_->goaway_received());

  EXPECT_CALL(
      *connection_,
      CloseConnection(
          QUIC_HTTP_GOAWAY_ID_LARGER_THAN_PREVIOUS,
          "GOAWAY received with ID 1 greater than previously received ID 0",
          _));
  session_->OnHttp3GoAway(/* id = */ 1);
}

// Test that server session will send a connectivity probe in response to a
// connectivity probe on the same path.
TEST_P(QuicSpdySessionTestServer, ServerReplyToConnecitivityProbe) {
  Initialize();
  if (VersionHasIetfQuicFrames(transport_version()) ||
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  QuicSocketAddress old_peer_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort);
  EXPECT_EQ(old_peer_address, session_->peer_address());

  QuicSocketAddress new_peer_address =
      QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort + 1);

  EXPECT_CALL(*connection_,
              SendConnectivityProbingPacket(nullptr, new_peer_address));

  session_->OnPacketReceived(session_->self_address(), new_peer_address,
                             /*is_connectivity_probe=*/true);
  EXPECT_EQ(old_peer_address, session_->peer_address());
}

TEST_P(QuicSpdySessionTestServer, IncreasedTimeoutAfterCryptoHandshake) {
  Initialize();
  EXPECT_EQ(kInitialIdleTimeoutSecs + 3,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
  CompleteHandshake();
  EXPECT_EQ(kMaximumIdleTimeoutSecs + 3,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
}

TEST_P(QuicSpdySessionTestServer, RstStreamBeforeHeadersDecompressed) {
  Initialize();
  CompleteHandshake();
  // Send two bytes of payload.
  QuicStreamFrame data1(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        absl::string_view("HT"));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(1u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));

  if (!VersionHasIetfQuicFrames(transport_version())) {
    // For version99, OnStreamReset gets called because of the STOP_SENDING,
    // below. EXPECT the call there.
    EXPECT_CALL(*connection_,
                OnStreamReset(GetNthClientInitiatedBidirectionalId(0), _));
  }

  EXPECT_CALL(*connection_, SendControlFrame(_));
  QuicRstStreamFrame rst1(kInvalidControlFrameId,
                          GetNthClientInitiatedBidirectionalId(0),
                          QUIC_ERROR_PROCESSING_STREAM, 0);
  session_->OnRstStream(rst1);

  // Create and inject a STOP_SENDING frame. In GOOGLE QUIC, receiving a
  // RST_STREAM frame causes a two-way close. For IETF QUIC, RST_STREAM causes a
  // one-way close.
  if (VersionHasIetfQuicFrames(transport_version())) {
    // Only needed for version 99/IETF QUIC.
    QuicStopSendingFrame stop_sending(kInvalidControlFrameId,
                                      GetNthClientInitiatedBidirectionalId(0),
                                      QUIC_ERROR_PROCESSING_STREAM);
    // Expect the RESET_STREAM that is generated in response to receiving a
    // STOP_SENDING.
    EXPECT_CALL(*connection_,
                OnStreamReset(GetNthClientInitiatedBidirectionalId(0),
                              QUIC_ERROR_PROCESSING_STREAM));
    session_->OnStopSendingFrame(stop_sending);
  }

  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));
  // Connection should remain alive.
  EXPECT_TRUE(connection_->connected());
}

TEST_P(QuicSpdySessionTestServer, OnStreamFrameFinStaticStreamId) {
  Initialize();
  QuicStreamId id;
  // Initialize HTTP/3 control stream.
  if (VersionUsesHttp3(transport_version())) {
    CompleteHandshake();
    id = GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
    char type[] = {kControlStream};

    QuicStreamFrame data1(id, false, 0, absl::string_view(type, 1));
    session_->OnStreamFrame(data1);
  } else {
    id = QuicUtils::GetHeadersStreamId(transport_version());
  }

  // Send two bytes of payload.
  QuicStreamFrame data1(id, true, 0, absl::string_view("HT"));
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Attempt to close a static stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_->OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestServer, OnRstStreamStaticStreamId) {
  Initialize();
  QuicStreamId id;
  QuicErrorCode expected_error;
  std::string error_message;
  // Initialize HTTP/3 control stream.
  if (VersionUsesHttp3(transport_version())) {
    CompleteHandshake();
    id = GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
    char type[] = {kControlStream};

    QuicStreamFrame data1(id, false, 0, absl::string_view(type, 1));
    session_->OnStreamFrame(data1);
    expected_error = QUIC_HTTP_CLOSED_CRITICAL_STREAM;
    error_message = "RESET_STREAM received for receive control stream";
  } else {
    id = QuicUtils::GetHeadersStreamId(transport_version());
    expected_error = QUIC_INVALID_STREAM_ID;
    error_message = "Attempt to reset headers stream";
  }

  // Send two bytes of payload.
  QuicRstStreamFrame rst1(kInvalidControlFrameId, id,
                          QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(
      *connection_,
      CloseConnection(expected_error, error_message,
                      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_->OnRstStream(rst1);
}

TEST_P(QuicSpdySessionTestServer, OnStreamFrameInvalidStreamId) {
  Initialize();
  // Send two bytes of payload.
  QuicStreamFrame data1(QuicUtils::GetInvalidStreamId(transport_version()),
                        true, 0, absl::string_view("HT"));
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Received data for an invalid stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_->OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestServer, OnRstStreamInvalidStreamId) {
  Initialize();
  // Send two bytes of payload.
  QuicRstStreamFrame rst1(kInvalidControlFrameId,
                          QuicUtils::GetInvalidStreamId(transport_version()),
                          QUIC_ERROR_PROCESSING_STREAM, 0);
  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_INVALID_STREAM_ID, "Received data for an invalid stream",
                  ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_->OnRstStream(rst1);
}

TEST_P(QuicSpdySessionTestServer, HandshakeUnblocksFlowControlBlockedStream) {
  Initialize();
  if (connection_->version().handshake_protocol == PROTOCOL_TLS1_3) {
    // This test requires Google QUIC crypto because it assumes streams start
    // off unblocked.
    return;
  }
  // Test that if a stream is flow control blocked, then on receipt of the SHLO
  // containing a suitable send window offset, the stream becomes unblocked.

  // Ensure that Writev consumes all the data it is given (simulate no socket
  // blocking).
  session_->GetMutableCryptoStream()->EstablishZeroRttEncryption();
  session_->set_writev_consumes_all_data(true);

  // Create a stream, and send enough data to make it flow control blocked.
  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  std::string body(kMinimumFlowControlSendWindow, '.');
  EXPECT_FALSE(stream2->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(AtLeast(1));
  stream2->WriteOrBufferBody(body, false);
  EXPECT_TRUE(stream2->IsFlowControlBlocked());
  EXPECT_TRUE(session_->IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_->IsStreamFlowControlBlocked());

  // Now complete the crypto handshake, resulting in an increased flow control
  // send window.
  CompleteHandshake();
  EXPECT_TRUE(QuicSessionPeer::IsStreamWriteBlocked(&*session_, stream2->id()));
  // Stream is now unblocked.
  EXPECT_FALSE(stream2->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
}

#if !defined(OS_IOS)
// This test is failing flakily for iOS bots.
// http://crbug.com/425050
// NOTE: It's not possible to use the standard MAYBE_ convention to disable
// this test on iOS because when this test gets instantiated it ends up with
// various names that are dependent on the parameters passed.
TEST_P(QuicSpdySessionTestServer,
       HandshakeUnblocksFlowControlBlockedHeadersStream) {
  Initialize();
  // This test depends on stream-level flow control for the crypto stream, which
  // doesn't exist when CRYPTO frames are used.
  if (QuicVersionUsesCryptoFrames(transport_version())) {
    return;
  }

  // This test depends on the headers stream, which does not exist when QPACK is
  // used.
  if (VersionUsesHttp3(transport_version())) {
    return;
  }

  // Test that if the header stream is flow control blocked, then if the SHLO
  // contains a larger send window offset, the stream becomes unblocked.
  session_->GetMutableCryptoStream()->EstablishZeroRttEncryption();
  session_->set_writev_consumes_all_data(true);
  TestCryptoStream* crypto_stream = session_->GetMutableCryptoStream();
  EXPECT_FALSE(crypto_stream->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(&*session_);
  EXPECT_FALSE(headers_stream->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
  QuicStreamId stream_id = 5;
  // Write until the header stream is flow control blocked.
  EXPECT_CALL(*connection_, SendControlFrame(_)).WillOnce(&ClearControlFrame);
  HttpHeaderBlock headers;
  SimpleRandom random;
  while (!headers_stream->IsFlowControlBlocked() && stream_id < 2000) {
    EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
    EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
    headers["header"] = absl::StrCat(random.RandUint64(), random.RandUint64(),
                                     random.RandUint64());
    session_->WriteHeadersOnHeadersStream(stream_id, headers.Clone(), true,
                                          spdy::SpdyStreamPrecedence(0),
                                          nullptr);
    stream_id += IdDelta();
  }
  // Write once more to ensure that the headers stream has buffered data. The
  // random headers may have exactly filled the flow control window.
  session_->WriteHeadersOnHeadersStream(stream_id, std::move(headers), true,
                                        spdy::SpdyStreamPrecedence(0), nullptr);
  EXPECT_TRUE(headers_stream->HasBufferedData());

  EXPECT_TRUE(headers_stream->IsFlowControlBlocked());
  EXPECT_FALSE(crypto_stream->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_->IsStreamFlowControlBlocked());
  EXPECT_FALSE(session_->HasDataToWrite());

  // Now complete the crypto handshake, resulting in an increased flow control
  // send window.
  CompleteHandshake();

  // Stream is now unblocked and will no longer have buffered data.
  EXPECT_FALSE(headers_stream->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
  EXPECT_TRUE(headers_stream->HasBufferedData());
  EXPECT_TRUE(QuicSessionPeer::IsStreamWriteBlocked(
      &*session_, QuicUtils::GetHeadersStreamId(transport_version())));
}
#endif  // !defined(OS_IOS)

TEST_P(QuicSpdySessionTestServer,
       ConnectionFlowControlAccountingRstOutOfOrder) {
  Initialize();

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  CompleteHandshake();
  // Test that when we receive an out of order stream RST we correctly adjust
  // our connection level flow control receive window.
  // On close, the stream should mark as consumed all bytes between the highest
  // byte consumed so far and the final byte offset from the RST frame.
  TestStream* stream = session_->CreateOutgoingBidirectionalStream();

  const QuicStreamOffset kByteOffset =
      1 + kInitialSessionFlowControlWindowForTest / 2;

  if (!VersionHasIetfQuicFrames(transport_version())) {
    // For version99 the call to OnStreamReset happens as a result of receiving
    // the STOP_SENDING, so set up the EXPECT there.
    EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
    EXPECT_CALL(*connection_, SendControlFrame(_));
  }
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream->id(),
                               QUIC_STREAM_CANCELLED, kByteOffset);
  session_->OnRstStream(rst_frame);
  // Create and inject a STOP_SENDING frame. In GOOGLE QUIC, receiving a
  // RST_STREAM frame causes a two-way close. For IETF QUIC, RST_STREAM causes a
  // one-way close.
  if (VersionHasIetfQuicFrames(transport_version())) {
    // Only needed for version 99/IETF QUIC.
    QuicStopSendingFrame stop_sending(kInvalidControlFrameId, stream->id(),
                                      QUIC_STREAM_CANCELLED);
    // Expect the RESET_STREAM that is generated in response to receiving a
    // STOP_SENDING.
    EXPECT_CALL(*connection_,
                OnStreamReset(stream->id(), QUIC_STREAM_CANCELLED));
    EXPECT_CALL(*connection_, SendControlFrame(_));
    session_->OnStopSendingFrame(stop_sending);
  }

  EXPECT_EQ(kByteOffset, session_->flow_controller()->bytes_consumed());
}

TEST_P(QuicSpdySessionTestServer, InvalidStreamFlowControlWindowInHandshake) {
  Initialize();
  if (GetParam().handshake_protocol == PROTOCOL_TLS1_3) {
    // IETF Quic doesn't require a minimum flow control window.
    return;
  }
  // Test that receipt of an invalid (< default) stream flow control window from
  // the peer results in the connection being torn down.
  const uint32_t kInvalidWindow = kMinimumFlowControlSendWindow - 1;
  QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(session_->config(),
                                                            kInvalidWindow);

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_INVALID_WINDOW, _, _));
  session_->OnConfigNegotiated();
}

TEST_P(QuicSpdySessionTestServer, TooLowUnidirectionalStreamLimitHttp3) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  session_->GetMutableCryptoStream()->EstablishZeroRttEncryption();
  QuicConfigPeer::SetReceivedMaxUnidirectionalStreams(session_->config(), 2u);
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);

  EXPECT_CALL(
      *connection_,
      CloseConnection(
          _, "new unidirectional limit 2 decreases the current limit: 3", _));
  session_->OnConfigNegotiated();
}

// Test negotiation of custom server initial flow control window.
TEST_P(QuicSpdySessionTestServer, CustomFlowControlWindow) {
  Initialize();
  QuicTagVector copt;
  copt.push_back(kIFW7);
  QuicConfigPeer::SetReceivedConnectionOptions(session_->config(), copt);
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  session_->OnConfigNegotiated();
  EXPECT_EQ(192 * 1024u, QuicFlowControllerPeer::ReceiveWindowSize(
                             session_->flow_controller()));
}

TEST_P(QuicSpdySessionTestServer, WindowUpdateUnblocksHeadersStream) {
  Initialize();
  if (VersionUsesHttp3(transport_version())) {
    // The test relies on headers stream, which no longer exists in IETF QUIC.
    return;
  }

  // Test that a flow control blocked headers stream gets unblocked on recipt of
  // a WINDOW_UPDATE frame.

  // Set the headers stream to be flow control blocked.
  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(&*session_);
  QuicStreamPeer::SetSendWindowOffset(headers_stream, 0);
  EXPECT_TRUE(headers_stream->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_TRUE(session_->IsStreamFlowControlBlocked());

  // Unblock the headers stream by supplying a WINDOW_UPDATE.
  QuicWindowUpdateFrame window_update_frame(kInvalidControlFrameId,
                                            headers_stream->id(),
                                            2 * kMinimumFlowControlSendWindow);
  session_->OnWindowUpdateFrame(window_update_frame);
  EXPECT_FALSE(headers_stream->IsFlowControlBlocked());
  EXPECT_FALSE(session_->IsConnectionFlowControlBlocked());
  EXPECT_FALSE(session_->IsStreamFlowControlBlocked());
}

TEST_P(QuicSpdySessionTestServer,
       TooManyUnfinishedStreamsCauseServerRejectStream) {
  Initialize();
  // If a buggy/malicious peer creates too many streams that are not ended
  // with a FIN or RST then we send an RST to refuse streams for versions other
  // than version 99. In version 99 the connection gets closed.
  CompleteHandshake();
  const QuicStreamId kMaxStreams = 5;
  if (VersionHasIetfQuicFrames(transport_version())) {
    QuicSessionPeer::SetMaxOpenIncomingBidirectionalStreams(&*session_,
                                                            kMaxStreams);
  } else {
    QuicSessionPeer::SetMaxOpenIncomingStreams(&*session_, kMaxStreams);
  }
  // GetNth assumes that both the crypto and header streams have been
  // open, but the stream id manager, using GetFirstBidirectional... only
  // assumes that the crypto stream is open. This means that GetNth...(0)
  // Will return stream ID == 8 (with id ==0 for crypto and id==4 for headers).
  // It also means that GetNth(kMax..=5) returns 28 (streams 0/1/2/3/4 are ids
  // 8, 12, 16, 20, 24, respectively, so stream#5 is stream id 28).
  // However, the stream ID manager does not assume stream 4 is for headers.
  // The ID manager would assume that stream#5 is streamid 24.
  // In order to make this all work out properly, kFinalStreamId will
  // be set to GetNth...(kMaxStreams-1)... but only for IETF QUIC
  const QuicStreamId kFirstStreamId = GetNthClientInitiatedBidirectionalId(0);
  const QuicStreamId kFinalStreamId =
      GetNthClientInitiatedBidirectionalId(kMaxStreams);
  // Create kMaxStreams data streams, and close them all without receiving a
  // FIN or a RST_STREAM from the client.
  const QuicStreamId kNextId = QuicUtils::StreamIdDelta(transport_version());
  for (QuicStreamId i = kFirstStreamId; i < kFinalStreamId; i += kNextId) {
    QuicStreamFrame data1(i, false, 0, absl::string_view("HT"));
    session_->OnStreamFrame(data1);
    CloseStream(i);
  }
  // Try and open a stream that exceeds the limit.
  if (!VersionHasIetfQuicFrames(transport_version())) {
    // On versions other than 99, opening such a stream results in a
    // RST_STREAM.
    EXPECT_CALL(*connection_, SendControlFrame(_)).Times(1);
    EXPECT_CALL(*connection_,
                OnStreamReset(kFinalStreamId, QUIC_REFUSED_STREAM))
        .Times(1);
  } else {
    // On version 99 opening such a stream results in a connection close.
    EXPECT_CALL(
        *connection_,
        CloseConnection(QUIC_INVALID_STREAM_ID,
                        testing::MatchesRegex(
                            "Stream id \\d+ would exceed stream count limit 5"),
                        _));
  }
  // Create one more data streams to exceed limit of open stream.
  QuicStreamFrame data1(kFinalStreamId, false, 0, absl::string_view("HT"));
  session_->OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestServer, DrainingStreamsDoNotCountAsOpened) {
  Initialize();
  // Verify that a draining stream (which has received a FIN but not consumed
  // it) does not count against the open quota (because it is closed from the
  // protocol point of view).
  CompleteHandshake();
  if (VersionHasIetfQuicFrames(transport_version())) {
    // Simulate receiving a config. so that MAX_STREAMS/etc frames may
    // be transmitted
    QuicSessionPeer::set_is_configured(&*session_, true);
    // Version 99 will result in a MAX_STREAMS frame as streams are consumed
    // (via the OnStreamFrame call) and then released (via
    // StreamDraining). Eventually this node will believe that the peer is
    // running low on available stream ids and then send a MAX_STREAMS frame,
    // caught by this EXPECT_CALL.
    EXPECT_CALL(*connection_, SendControlFrame(_)).Times(1);
  } else {
    EXPECT_CALL(*connection_, SendControlFrame(_)).Times(0);
  }
  EXPECT_CALL(*connection_, OnStreamReset(_, QUIC_REFUSED_STREAM)).Times(0);
  const QuicStreamId kMaxStreams = 5;
  if (VersionHasIetfQuicFrames(transport_version())) {
    QuicSessionPeer::SetMaxOpenIncomingBidirectionalStreams(&*session_,
                                                            kMaxStreams);
  } else {
    QuicSessionPeer::SetMaxOpenIncomingStreams(&*session_, kMaxStreams);
  }

  // Create kMaxStreams + 1 data streams, and mark them draining.
  const QuicStreamId kFirstStreamId = GetNthClientInitiatedBidirectionalId(0);
  const QuicStreamId kFinalStreamId =
      GetNthClientInitiatedBidirectionalId(kMaxStreams + 1);
  for (QuicStreamId i = kFirstStreamId; i < kFinalStreamId; i += IdDelta()) {
    QuicStreamFrame data1(i, true, 0, absl::string_view("HT"));
    session_->OnStreamFrame(data1);
    EXPECT_EQ(1u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));
    session_->StreamDraining(i, /*unidirectional=*/false);
    EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));
  }
}

class QuicSpdySessionTestClient : public QuicSpdySessionTestBase {
 protected:
  QuicSpdySessionTestClient()
      : QuicSpdySessionTestBase(Perspective::IS_CLIENT, false) {}
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicSpdySessionTestClient,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicSpdySessionTestClient, UsesPendingStreamsForFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  EXPECT_TRUE(session_->UsesPendingStreamForFrame(
      STREAM_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                        transport_version(), Perspective::IS_SERVER)));
  EXPECT_TRUE(session_->UsesPendingStreamForFrame(
      RST_STREAM_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                            transport_version(), Perspective::IS_SERVER)));
  EXPECT_FALSE(session_->UsesPendingStreamForFrame(
      RST_STREAM_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                            transport_version(), Perspective::IS_CLIENT)));
  EXPECT_FALSE(session_->UsesPendingStreamForFrame(
      STOP_SENDING_FRAME, QuicUtils::GetFirstUnidirectionalStreamId(
                              transport_version(), Perspective::IS_SERVER)));
  EXPECT_FALSE(session_->UsesPendingStreamForFrame(
      RST_STREAM_FRAME, QuicUtils::GetFirstBidirectionalStreamId(
                            transport_version(), Perspective::IS_SERVER)));
}

// Regression test for crbug.com/977581.
TEST_P(QuicSpdySessionTestClient, BadStreamFramePendingStream) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));
  QuicStreamId stream_id1 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  // A bad stream frame with no data and no fin.
  QuicStreamFrame data1(stream_id1, false, 0, 0);
  session_->OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestClient, PendingStreamKeepsConnectionAlive) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  QuicStreamId stream_id = QuicUtils::GetFirstUnidirectionalStreamId(
      transport_version(), Perspective::IS_SERVER);

  QuicStreamFrame frame(stream_id, false, 1, "test");
  EXPECT_FALSE(session_->ShouldKeepConnectionAlive());
  session_->OnStreamFrame(frame);
  EXPECT_TRUE(QuicSessionPeer::GetPendingStream(&*session_, stream_id));
  EXPECT_TRUE(session_->ShouldKeepConnectionAlive());
}

TEST_P(QuicSpdySessionTestClient, AvailableStreamsClient) {
  Initialize();
  ASSERT_TRUE(session_->GetOrCreateStream(
                  GetNthServerInitiatedBidirectionalId(2)) != nullptr);
  // Both server initiated streams with smaller stream IDs should be available.
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(
      &*session_, GetNthServerInitiatedBidirectionalId(0)));
  EXPECT_TRUE(QuicSessionPeer::IsStreamAvailable(
      &*session_, GetNthServerInitiatedBidirectionalId(1)));
  ASSERT_TRUE(session_->GetOrCreateStream(
                  GetNthServerInitiatedBidirectionalId(0)) != nullptr);
  ASSERT_TRUE(session_->GetOrCreateStream(
                  GetNthServerInitiatedBidirectionalId(1)) != nullptr);
  // And client initiated stream ID should be not available.
  EXPECT_FALSE(QuicSessionPeer::IsStreamAvailable(
      &*session_, GetNthClientInitiatedBidirectionalId(0)));
}

// Regression test for b/130740258 and https://crbug.com/971779.
// If headers that are too large or empty are received (these cases are handled
// the same way, as QuicHeaderList clears itself when headers exceed the limit),
// then the stream is reset.  No more frames must be sent in this case.
TEST_P(QuicSpdySessionTestClient, TooLargeHeadersMustNotCauseWriteAfterReset) {
  Initialize();
  // In IETF QUIC, HEADERS do not carry FIN flag, and OnStreamHeaderList() is
  // never called after an error, including too large headers.
  if (VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  TestStream* stream = session_->CreateOutgoingBidirectionalStream();

  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillOnce(Return(WriteResult(WRITE_STATUS_OK, 0)));
  // Write headers with FIN set to close write side of stream.
  // Header block does not matter.
  stream->WriteHeaders(HttpHeaderBlock(), /* fin = */ true, nullptr);

  // Receive headers that are too large or empty, with FIN set.
  // This causes the stream to be reset.  No frames must be written after this.
  QuicHeaderList headers;
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream->id(), QUIC_HEADERS_TOO_LARGE));
  stream->OnStreamHeaderList(/* fin = */ true,
                             headers.uncompressed_header_bytes(), headers);
}

TEST_P(QuicSpdySessionTestClient, RecordFinAfterReadSideClosed) {
  Initialize();
  // Verify that an incoming FIN is recorded in a stream object even if the read
  // side has been closed.  This prevents an entry from being made in
  // locally_closed_streams_highest_offset_ (which will never be deleted).
  CompleteHandshake();
  TestStream* stream = session_->CreateOutgoingBidirectionalStream();
  QuicStreamId stream_id = stream->id();

  // Close the read side manually.
  QuicStreamPeer::CloseReadSide(stream);

  // Receive a stream data frame with FIN.
  QuicStreamFrame frame(stream_id, true, 0, absl::string_view());
  session_->OnStreamFrame(frame);
  EXPECT_TRUE(stream->fin_received());

  // Reset stream locally.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream->id(), _));
  stream->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream));

  EXPECT_TRUE(connection_->connected());
  EXPECT_TRUE(QuicSessionPeer::IsStreamClosed(&*session_, stream_id));
  EXPECT_FALSE(QuicSessionPeer::IsStreamCreated(&*session_, stream_id));

  // The stream is not waiting for the arrival of the peer's final offset as it
  // was received with the FIN earlier.
  EXPECT_EQ(
      0u,
      QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(&*session_).size());
}

TEST_P(QuicSpdySessionTestClient, WritePriority) {
  Initialize();
  if (VersionUsesHttp3(transport_version())) {
    // IETF QUIC currently doesn't support PRIORITY.
    return;
  }
  CompleteHandshake();

  TestHeadersStream* headers_stream;
  QuicSpdySessionPeer::SetHeadersStream(&*session_, nullptr);
  headers_stream = new TestHeadersStream(&*session_);
  QuicSpdySessionPeer::SetHeadersStream(&*session_, headers_stream);

  // Make packet writer blocked so |headers_stream| will buffer its write data.
  EXPECT_CALL(*writer_, IsWriteBlocked()).WillRepeatedly(Return(true));

  const QuicStreamId id = 4;
  const QuicStreamId parent_stream_id = 9;
  const SpdyPriority priority = kV3HighestPriority;
  const bool exclusive = true;
  session_->WritePriority(id, parent_stream_id,
                          Spdy3PriorityToHttp2Weight(priority), exclusive);

  QuicStreamSendBufferBase& send_buffer =
      QuicStreamPeer::SendBuffer(headers_stream);
  ASSERT_EQ(1u, send_buffer.size());

  SpdyPriorityIR priority_frame(
      id, parent_stream_id, Spdy3PriorityToHttp2Weight(priority), exclusive);
  SpdyFramer spdy_framer(SpdyFramer::ENABLE_COMPRESSION);
  SpdySerializedFrame frame = spdy_framer.SerializeFrame(priority_frame);

  EXPECT_EQ(absl::string_view(frame.data(), frame.size()),
            send_buffer.LatestWriteForTest());
}

TEST_P(QuicSpdySessionTestClient, Http3ServerPush) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));

  // Push unidirectional stream is type 0x01.
  std::string frame_type1;
  ASSERT_TRUE(absl::HexStringToBytes("01", &frame_type1));
  QuicStreamId stream_id1 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_RECEIVE_SERVER_PUSH, _, _))
      .Times(1);
  session_->OnStreamFrame(QuicStreamFrame(stream_id1, /* fin = */ false,
                                          /* offset = */ 0, frame_type1));
}

TEST_P(QuicSpdySessionTestClient, Http3ServerPushOutofOrderFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));

  // Push unidirectional stream is type 0x01.
  std::string frame_type;
  ASSERT_TRUE(absl::HexStringToBytes("01", &frame_type));
  // The first field of a push stream is the Push ID.
  std::string push_id;
  ASSERT_TRUE(absl::HexStringToBytes("4000", &push_id));

  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);

  QuicStreamFrame data1(stream_id,
                        /* fin = */ false, /* offset = */ 0, frame_type);
  QuicStreamFrame data2(stream_id,
                        /* fin = */ false, /* offset = */ frame_type.size(),
                        push_id);

  // Receiving some stream data without stream type does not open the stream.
  session_->OnStreamFrame(data2);
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_RECEIVE_SERVER_PUSH, _, _))
      .Times(1);
  session_->OnStreamFrame(data1);
}

TEST_P(QuicSpdySessionTestClient, ServerDisableQpackDynamicTable) {
  SetQuicFlag(quic_server_disable_qpack_dynamic_table, true);
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  // Use an arbitrary stream id for creating the receive control stream.
  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  QuicStreamFrame data1(stream_id, false, 0, absl::string_view(type, 1));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());
  // Receive the QPACK dynamic table capacity from the peer.
  const uint64_t capacity = 512;
  SettingsFrame settings;
  settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] = capacity;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamFrame frame(stream_id, false, 1, data);
  session_->OnStreamFrame(frame);

  // Verify that the encoder's dynamic table capacity is limited to the
  // peer's value.
  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  EXPECT_EQ(capacity, qpack_encoder->MaximumDynamicTableCapacity());
  QpackEncoderHeaderTable* encoder_header_table =
      QpackEncoderPeer::header_table(qpack_encoder);
  EXPECT_EQ(capacity, encoder_header_table->dynamic_table_capacity());
  EXPECT_EQ(capacity, encoder_header_table->maximum_dynamic_table_capacity());

  // Verify that the advertised capacity is the default.
  SettingsFrame outgoing_settings = session_->settings();
  EXPECT_EQ(kDefaultQpackMaxDynamicTableCapacity,
            outgoing_settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY]);
}

TEST_P(QuicSpdySessionTestClient, DisableQpackDynamicTable) {
  SetQuicFlag(quic_server_disable_qpack_dynamic_table, false);
  qpack_maximum_dynamic_table_capacity_ = 0;
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  // Use an arbitrary stream id for creating the receive control stream.
  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  QuicStreamFrame data1(stream_id, false, 0, absl::string_view(type, 1));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());
  // Receive the QPACK dynamic table capacity from the peer.
  const uint64_t capacity = 512;
  SettingsFrame settings;
  settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] = capacity;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamFrame frame(stream_id, false, 1, data);
  session_->OnStreamFrame(frame);

  // Verify that the encoder's dynamic table capacity is 0.
  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  EXPECT_EQ(capacity, qpack_encoder->MaximumDynamicTableCapacity());
  QpackEncoderHeaderTable* encoder_header_table =
      QpackEncoderPeer::header_table(qpack_encoder);
  EXPECT_EQ(0, encoder_header_table->dynamic_table_capacity());
  EXPECT_EQ(capacity, encoder_header_table->maximum_dynamic_table_capacity());

  // Verify that the advertised capacity is 0.
  SettingsFrame outgoing_settings = session_->settings();
  EXPECT_EQ(0, outgoing_settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY]);
}

TEST_P(QuicSpdySessionTestServer, OnStreamFrameLost) {
  Initialize();
  CompleteHandshake();
  InSequence s;

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_->connection(), send_algorithm);

  TestCryptoStream* crypto_stream = session_->GetMutableCryptoStream();
  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();

  QuicStreamFrame frame2(stream2->id(), false, 0, 9);
  QuicStreamFrame frame3(stream4->id(), false, 0, 9);

  // Lost data on cryption stream, streams 2 and 4.
  EXPECT_CALL(*stream4, HasPendingRetransmission()).WillOnce(Return(true));
  if (!QuicVersionUsesCryptoFrames(transport_version())) {
    EXPECT_CALL(*crypto_stream, HasPendingRetransmission())
        .WillOnce(Return(true));
  }
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(true));
  session_->OnFrameLost(QuicFrame(frame3));
  if (!QuicVersionUsesCryptoFrames(transport_version())) {
    QuicStreamFrame frame1(QuicUtils::GetCryptoStreamId(transport_version()),
                           false, 0, 1300);
    session_->OnFrameLost(QuicFrame(frame1));
  } else {
    QuicCryptoFrame crypto_frame(ENCRYPTION_INITIAL, 0, 1300);
    session_->OnFrameLost(QuicFrame(&crypto_frame));
  }
  session_->OnFrameLost(QuicFrame(frame2));
  EXPECT_TRUE(session_->WillingAndAbleToWrite());

  // Mark streams 2 and 4 write blocked.
  session_->MarkConnectionLevelWriteBlocked(stream2->id());
  session_->MarkConnectionLevelWriteBlocked(stream4->id());

  // Lost data is retransmitted before new data, and retransmissions for crypto
  // stream go first.
  // Do not check congestion window when crypto stream has lost data.
  EXPECT_CALL(*send_algorithm, CanSend(_)).Times(0);
  if (!QuicVersionUsesCryptoFrames(transport_version())) {
    EXPECT_CALL(*crypto_stream, OnCanWrite());
    EXPECT_CALL(*crypto_stream, HasPendingRetransmission())
        .WillOnce(Return(false));
  }
  // Check congestion window for non crypto streams.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream4, OnCanWrite());
  EXPECT_CALL(*stream4, HasPendingRetransmission()).WillOnce(Return(false));
  // Connection is blocked.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillRepeatedly(Return(false));

  session_->OnCanWrite();
  EXPECT_TRUE(session_->WillingAndAbleToWrite());

  // Unblock connection.
  // Stream 2 retransmits lost data.
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(false));
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  // Stream 2 sends new data.
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*send_algorithm, CanSend(_)).WillOnce(Return(true));
  EXPECT_CALL(*stream4, OnCanWrite());
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));

  session_->OnCanWrite();
  EXPECT_FALSE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, DonotRetransmitDataOfClosedStreams) {
  Initialize();
  // Resetting a stream will send a QPACK Stream Cancellation instruction on the
  // decoder stream.  For simplicity, ignore writes on this stream.
  CompleteHandshake();
  NoopQpackStreamSenderDelegate qpack_stream_sender_delegate;
  if (VersionUsesHttp3(transport_version())) {
    session_->qpack_decoder()->set_qpack_stream_sender_delegate(
        &qpack_stream_sender_delegate);
  }

  InSequence s;

  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_->CreateOutgoingBidirectionalStream();

  QuicStreamFrame frame1(stream2->id(), false, 0, 9);
  QuicStreamFrame frame2(stream4->id(), false, 0, 9);
  QuicStreamFrame frame3(stream6->id(), false, 0, 9);

  EXPECT_CALL(*stream6, HasPendingRetransmission()).WillOnce(Return(true));
  EXPECT_CALL(*stream4, HasPendingRetransmission()).WillOnce(Return(true));
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(true));
  session_->OnFrameLost(QuicFrame(frame3));
  session_->OnFrameLost(QuicFrame(frame2));
  session_->OnFrameLost(QuicFrame(frame1));

  session_->MarkConnectionLevelWriteBlocked(stream2->id());
  session_->MarkConnectionLevelWriteBlocked(stream4->id());
  session_->MarkConnectionLevelWriteBlocked(stream6->id());

  // Reset stream 4 locally.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  EXPECT_CALL(*connection_, OnStreamReset(stream4->id(), _));
  stream4->Reset(QUIC_STREAM_CANCELLED);

  // Verify stream 4 is removed from streams with lost data list.
  EXPECT_CALL(*stream6, OnCanWrite());
  EXPECT_CALL(*stream6, HasPendingRetransmission()).WillOnce(Return(false));
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream2, HasPendingRetransmission()).WillOnce(Return(false));
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream6, OnCanWrite());
  session_->OnCanWrite();
}

TEST_P(QuicSpdySessionTestServer, RetransmitFrames) {
  Initialize();
  CompleteHandshake();
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_->connection(), send_algorithm);
  InSequence s;

  TestStream* stream2 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream4 = session_->CreateOutgoingBidirectionalStream();
  TestStream* stream6 = session_->CreateOutgoingBidirectionalStream();
  EXPECT_CALL(*connection_, SendControlFrame(_)).WillOnce(&ClearControlFrame);
  session_->SendWindowUpdate(stream2->id(), 9);

  QuicStreamFrame frame1(stream2->id(), false, 0, 9);
  QuicStreamFrame frame2(stream4->id(), false, 0, 9);
  QuicStreamFrame frame3(stream6->id(), false, 0, 9);
  QuicWindowUpdateFrame window_update(1, stream2->id(), 9);
  QuicFrames frames;
  frames.push_back(QuicFrame(frame1));
  frames.push_back(QuicFrame(window_update));
  frames.push_back(QuicFrame(frame2));
  frames.push_back(QuicFrame(frame3));
  EXPECT_FALSE(session_->WillingAndAbleToWrite());

  EXPECT_CALL(*stream2, RetransmitStreamData(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*connection_, SendControlFrame(_)).WillOnce(&ClearControlFrame);
  EXPECT_CALL(*stream4, RetransmitStreamData(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*stream6, RetransmitStreamData(_, _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*send_algorithm, OnApplicationLimited(_));
  session_->RetransmitFrames(frames, PTO_RETRANSMISSION);
}

TEST_P(QuicSpdySessionTestServer, OnPriorityFrame) {
  Initialize();
  QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  TestStream* stream = session_->CreateIncomingStream(stream_id);
  session_->OnPriorityFrame(stream_id,
                            spdy::SpdyStreamPrecedence(kV3HighestPriority));

  EXPECT_EQ((QuicStreamPriority(HttpStreamPriority{
                kV3HighestPriority, HttpStreamPriority::kDefaultIncremental})),
            stream->priority());
}

TEST_P(QuicSpdySessionTestServer, OnPriorityUpdateFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnSettingsFrameSent(_));
  CompleteHandshake();

  // Create control stream.
  QuicStreamId receive_control_stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  absl::string_view stream_type(type, 1);
  QuicStreamOffset offset = 0;
  QuicStreamFrame data1(receive_control_stream_id, false, offset, stream_type);
  offset += stream_type.length();
  EXPECT_CALL(debug_visitor,
              OnPeerControlStreamCreated(receive_control_stream_id));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(receive_control_stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  // Send SETTINGS frame.
  std::string serialized_settings = HttpEncoder::SerializeSettingsFrame({});
  QuicStreamFrame data2(receive_control_stream_id, false, offset,
                        serialized_settings);
  offset += serialized_settings.length();
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(_));
  session_->OnStreamFrame(data2);

  // PRIORITY_UPDATE frame for first request stream.
  const QuicStreamId stream_id1 = GetNthClientInitiatedBidirectionalId(0);
  PriorityUpdateFrame priority_update1{stream_id1, "u=2"};
  std::string serialized_priority_update1 =
      HttpEncoder::SerializePriorityUpdateFrame(priority_update1);
  QuicStreamFrame data3(receive_control_stream_id,
                        /* fin = */ false, offset, serialized_priority_update1);
  offset += serialized_priority_update1.size();

  // PRIORITY_UPDATE frame arrives after stream creation.
  TestStream* stream1 = session_->CreateIncomingStream(stream_id1);
  EXPECT_EQ(QuicStreamPriority(
                HttpStreamPriority{HttpStreamPriority::kDefaultUrgency,
                                   HttpStreamPriority::kDefaultIncremental}),
            stream1->priority());
  EXPECT_CALL(debug_visitor, OnPriorityUpdateFrameReceived(priority_update1));
  session_->OnStreamFrame(data3);
  EXPECT_EQ(QuicStreamPriority(HttpStreamPriority{
                2u, HttpStreamPriority::kDefaultIncremental}),
            stream1->priority());

  // PRIORITY_UPDATE frame for second request stream.
  const QuicStreamId stream_id2 = GetNthClientInitiatedBidirectionalId(1);
  PriorityUpdateFrame priority_update2{stream_id2, "u=5, i"};
  std::string serialized_priority_update2 =
      HttpEncoder::SerializePriorityUpdateFrame(priority_update2);
  QuicStreamFrame stream_frame3(receive_control_stream_id,
                                /* fin = */ false, offset,
                                serialized_priority_update2);

  // PRIORITY_UPDATE frame arrives before stream creation,
  // priority value is buffered.
  EXPECT_CALL(debug_visitor, OnPriorityUpdateFrameReceived(priority_update2));
  session_->OnStreamFrame(stream_frame3);
  // Priority is applied upon stream construction.
  TestStream* stream2 = session_->CreateIncomingStream(stream_id2);
  EXPECT_EQ(QuicStreamPriority(HttpStreamPriority{5u, true}),
            stream2->priority());
}

TEST_P(QuicSpdySessionTestServer, OnInvalidPriorityUpdateFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Create control stream.
  QuicStreamId receive_control_stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  absl::string_view stream_type(type, 1);
  QuicStreamOffset offset = 0;
  QuicStreamFrame data1(receive_control_stream_id, false, offset, stream_type);
  offset += stream_type.length();
  EXPECT_CALL(debug_visitor,
              OnPeerControlStreamCreated(receive_control_stream_id));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(receive_control_stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  // Send SETTINGS frame.
  std::string serialized_settings = HttpEncoder::SerializeSettingsFrame({});
  QuicStreamFrame data2(receive_control_stream_id, false, offset,
                        serialized_settings);
  offset += serialized_settings.length();
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(_));
  session_->OnStreamFrame(data2);

  // PRIORITY_UPDATE frame with Priority Field Value that is not valid
  // Structured Headers.
  const QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  PriorityUpdateFrame priority_update{stream_id, "00"};

  EXPECT_CALL(debug_visitor, OnPriorityUpdateFrameReceived(priority_update));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_PRIORITY_UPDATE,
                              "Invalid PRIORITY_UPDATE frame payload.", _));

  std::string serialized_priority_update =
      HttpEncoder::SerializePriorityUpdateFrame(priority_update);
  QuicStreamFrame data3(receive_control_stream_id,
                        /* fin = */ false, offset, serialized_priority_update);
  session_->OnStreamFrame(data3);
}

TEST_P(QuicSpdySessionTestServer, OnPriorityUpdateFrameOutOfBoundsUrgency) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Create control stream.
  QuicStreamId receive_control_stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  absl::string_view stream_type(type, 1);
  QuicStreamOffset offset = 0;
  QuicStreamFrame data1(receive_control_stream_id, false, offset, stream_type);
  offset += stream_type.length();
  EXPECT_CALL(debug_visitor,
              OnPeerControlStreamCreated(receive_control_stream_id));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(receive_control_stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  // Send SETTINGS frame.
  std::string serialized_settings = HttpEncoder::SerializeSettingsFrame({});
  QuicStreamFrame data2(receive_control_stream_id, false, offset,
                        serialized_settings);
  offset += serialized_settings.length();
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(_));
  session_->OnStreamFrame(data2);

  // PRIORITY_UPDATE frame with urgency not in [0,7].
  const QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  PriorityUpdateFrame priority_update{stream_id, "u=9"};

  EXPECT_CALL(debug_visitor, OnPriorityUpdateFrameReceived(priority_update));
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);

  std::string serialized_priority_update =
      HttpEncoder::SerializePriorityUpdateFrame(priority_update);
  QuicStreamFrame data3(receive_control_stream_id,
                        /* fin = */ false, offset, serialized_priority_update);
  session_->OnStreamFrame(data3);
}

TEST_P(QuicSpdySessionTestServer, SimplePendingStreamType) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  char input[] = {0x04,            // type
                  'a', 'b', 'c'};  // data
  absl::string_view payload(input, ABSL_ARRAYSIZE(input));

  // This is a server test with a client-initiated unidirectional stream.
  QuicStreamId stream_id = QuicUtils::GetFirstUnidirectionalStreamId(
      transport_version(), Perspective::IS_CLIENT);

  for (bool fin : {true, false}) {
    QuicStreamFrame frame(stream_id, fin, /* offset = */ 0, payload);

    // A STOP_SENDING frame is sent in response to the unknown stream type.
    EXPECT_CALL(*connection_, SendControlFrame(_))
        .WillOnce([stream_id](const QuicFrame& frame) {
          EXPECT_EQ(STOP_SENDING_FRAME, frame.type);

          const QuicStopSendingFrame& stop_sending = frame.stop_sending_frame;
          EXPECT_EQ(stream_id, stop_sending.stream_id);
          EXPECT_EQ(QUIC_STREAM_STREAM_CREATION_ERROR, stop_sending.error_code);
          EXPECT_EQ(
              static_cast<uint64_t>(QuicHttp3ErrorCode::STREAM_CREATION_ERROR),
              stop_sending.ietf_error_code);

          return ClearControlFrame(frame);
        });
    session_->OnStreamFrame(frame);

    PendingStream* pending =
        QuicSessionPeer::GetPendingStream(&*session_, stream_id);
    if (fin) {
      // Stream is closed if FIN is received.
      EXPECT_FALSE(pending);
    } else {
      ASSERT_TRUE(pending);
      // The pending stream must ignore read data.
      EXPECT_TRUE(pending->sequencer()->ignore_read_data());
    }

    stream_id += QuicUtils::StreamIdDelta(transport_version());
  }
}

TEST_P(QuicSpdySessionTestServer, SimplePendingStreamTypeOutOfOrderDelivery) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  char input[] = {0x04,            // type
                  'a', 'b', 'c'};  // data
  absl::string_view payload(input, ABSL_ARRAYSIZE(input));

  // This is a server test with a client-initiated unidirectional stream.
  QuicStreamId stream_id = QuicUtils::GetFirstUnidirectionalStreamId(
      transport_version(), Perspective::IS_CLIENT);

  for (bool fin : {true, false}) {
    QuicStreamFrame frame1(stream_id, /* fin = */ false, /* offset = */ 0,
                           payload.substr(0, 1));
    QuicStreamFrame frame2(stream_id, fin, /* offset = */ 1, payload.substr(1));

    // Deliver frames out of order.
    session_->OnStreamFrame(frame2);
    // A STOP_SENDING frame is sent in response to the unknown stream type.
    EXPECT_CALL(*connection_, SendControlFrame(_))
        .WillOnce(&VerifyAndClearStopSendingFrame);
    session_->OnStreamFrame(frame1);

    PendingStream* pending =
        QuicSessionPeer::GetPendingStream(&*session_, stream_id);
    if (fin) {
      // Stream is closed if FIN is received.
      EXPECT_FALSE(pending);
    } else {
      ASSERT_TRUE(pending);
      // The pending stream must ignore read data.
      EXPECT_TRUE(pending->sequencer()->ignore_read_data());
    }

    stream_id += QuicUtils::StreamIdDelta(transport_version());
  }
}

TEST_P(QuicSpdySessionTestServer,
       MultipleBytesPendingStreamTypeOutOfOrderDelivery) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  char input[] = {0x41, 0x00,      // type (256)
                  'a', 'b', 'c'};  // data
  absl::string_view payload(input, ABSL_ARRAYSIZE(input));

  // This is a server test with a client-initiated unidirectional stream.
  QuicStreamId stream_id = QuicUtils::GetFirstUnidirectionalStreamId(
      transport_version(), Perspective::IS_CLIENT);

  for (bool fin : {true, false}) {
    QuicStreamFrame frame1(stream_id, /* fin = */ false, /* offset = */ 0,
                           payload.substr(0, 1));
    QuicStreamFrame frame2(stream_id, /* fin = */ false, /* offset = */ 1,
                           payload.substr(1, 1));
    QuicStreamFrame frame3(stream_id, fin, /* offset = */ 2, payload.substr(2));

    // Deliver frames out of order.
    session_->OnStreamFrame(frame3);
    // The first byte does not contain the entire type varint.
    session_->OnStreamFrame(frame1);
    // A STOP_SENDING frame is sent in response to the unknown stream type.
    EXPECT_CALL(*connection_, SendControlFrame(_))
        .WillOnce(&VerifyAndClearStopSendingFrame);
    session_->OnStreamFrame(frame2);

    PendingStream* pending =
        QuicSessionPeer::GetPendingStream(&*session_, stream_id);
    if (fin) {
      // Stream is closed if FIN is received.
      EXPECT_FALSE(pending);
    } else {
      ASSERT_TRUE(pending);
      // The pending stream must ignore read data.
      EXPECT_TRUE(pending->sequencer()->ignore_read_data());
    }

    stream_id += QuicUtils::StreamIdDelta(transport_version());
  }
}

TEST_P(QuicSpdySessionTestServer, ReceiveControlStream) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Use an arbitrary stream id.
  QuicStreamId stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};

  QuicStreamFrame data1(stream_id, false, 0, absl::string_view(type, 1));
  EXPECT_CALL(debug_visitor, OnPeerControlStreamCreated(stream_id));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  SettingsFrame settings;
  settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] = 512;
  settings.values[SETTINGS_MAX_FIELD_SECTION_SIZE] = 5;
  settings.values[SETTINGS_QPACK_BLOCKED_STREAMS] = 42;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamFrame frame(stream_id, false, 1, data);

  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  QpackEncoderHeaderTable* header_table =
      QpackEncoderPeer::header_table(qpack_encoder);

  EXPECT_NE(512u, header_table->maximum_dynamic_table_capacity());
  EXPECT_NE(5u, session_->max_outbound_header_list_size());
  EXPECT_NE(42u, QpackEncoderPeer::maximum_blocked_streams(qpack_encoder));

  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(settings));
  session_->OnStreamFrame(frame);

  EXPECT_EQ(512u, header_table->maximum_dynamic_table_capacity());
  EXPECT_EQ(5u, session_->max_outbound_header_list_size());
  EXPECT_EQ(42u, QpackEncoderPeer::maximum_blocked_streams(qpack_encoder));
}

TEST_P(QuicSpdySessionTestServer, ServerDisableQpackDynamicTable) {
  SetQuicFlag(quic_server_disable_qpack_dynamic_table, true);
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  // Use an arbitrary stream id for creating the receive control stream.
  QuicStreamId stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  QuicStreamFrame data1(stream_id, false, 0, absl::string_view(type, 1));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());
  // Receive the QPACK dynamic table capacity from the peer.
  const uint64_t capacity = 512;
  SettingsFrame settings;
  settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] = capacity;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamFrame frame(stream_id, false, 1, data);
  session_->OnStreamFrame(frame);

  // Verify that the encoder's dynamic table capacity is 0.
  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  EXPECT_EQ(capacity, qpack_encoder->MaximumDynamicTableCapacity());
  QpackEncoderHeaderTable* encoder_header_table =
      QpackEncoderPeer::header_table(qpack_encoder);
  EXPECT_EQ(capacity, encoder_header_table->maximum_dynamic_table_capacity());
  EXPECT_EQ(0, encoder_header_table->dynamic_table_capacity());

  // Verify that the advertised capacity is 0.
  SettingsFrame outgoing_settings = session_->settings();
  EXPECT_EQ(0, outgoing_settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY]);
}

TEST_P(QuicSpdySessionTestServer, DisableQpackDynamicTable) {
  SetQuicFlag(quic_server_disable_qpack_dynamic_table, false);
  qpack_maximum_dynamic_table_capacity_ = 0;
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  // Use an arbitrary stream id for creating the receive control stream.
  QuicStreamId stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  QuicStreamFrame data1(stream_id, false, 0, absl::string_view(type, 1));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());
  // Receive the QPACK dynamic table capacity from the peer.
  const uint64_t capacity = 512;
  SettingsFrame settings;
  settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY] = capacity;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamFrame frame(stream_id, false, 1, data);
  session_->OnStreamFrame(frame);

  // Verify that the encoder's dynamic table capacity is 0.
  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  EXPECT_EQ(capacity, qpack_encoder->MaximumDynamicTableCapacity());
  QpackEncoderHeaderTable* encoder_header_table =
      QpackEncoderPeer::header_table(qpack_encoder);
  EXPECT_EQ(capacity, encoder_header_table->maximum_dynamic_table_capacity());
  EXPECT_EQ(0, encoder_header_table->dynamic_table_capacity());

  // Verify that the advertised capacity is 0.
  SettingsFrame outgoing_settings = session_->settings();
  EXPECT_EQ(0, outgoing_settings.values[SETTINGS_QPACK_MAX_TABLE_CAPACITY]);
}

TEST_P(QuicSpdySessionTestServer, ReceiveControlStreamOutOfOrderDelivery) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  // Use an arbitrary stream id.
  QuicStreamId stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  SettingsFrame settings;
  settings.values[10] = 2;
  settings.values[SETTINGS_MAX_FIELD_SECTION_SIZE] = 5;
  std::string data = HttpEncoder::SerializeSettingsFrame(settings);

  QuicStreamFrame data1(stream_id, false, 1, data);
  QuicStreamFrame data2(stream_id, false, 0, absl::string_view(type, 1));

  session_->OnStreamFrame(data1);
  EXPECT_NE(5u, session_->max_outbound_header_list_size());
  session_->OnStreamFrame(data2);
  EXPECT_EQ(5u, session_->max_outbound_header_list_size());
}

// Regression test for https://crbug.com/1009551.
TEST_P(QuicSpdySessionTestServer, StreamClosedWhileHeaderDecodingBlocked) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();
  session_->qpack_decoder()->OnSetDynamicTableCapacity(1024);

  QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  TestStream* stream = session_->CreateIncomingStream(stream_id);

  // HEADERS frame referencing first dynamic table entry.
  std::string headers_frame_payload;
  ASSERT_TRUE(absl::HexStringToBytes("020080", &headers_frame_payload));
  std::string headers_frame_header =
      HttpEncoder::SerializeHeadersFrameHeader(headers_frame_payload.length());
  std::string headers_frame =
      absl::StrCat(headers_frame_header, headers_frame_payload);
  stream->OnStreamFrame(QuicStreamFrame(stream_id, false, 0, headers_frame));

  // Decoding is blocked because dynamic table entry has not been received yet.
  EXPECT_FALSE(stream->headers_decompressed());

  // Stream is closed and destroyed.
  CloseStream(stream_id);
  session_->CleanUpClosedStreams();

  // Dynamic table entry arrived on the decoder stream.
  // The destroyed stream object must not be referenced.
  session_->qpack_decoder()->OnInsertWithoutNameReference("foo", "bar");
}

// Regression test for https://crbug.com/1011294.
TEST_P(QuicSpdySessionTestServer, SessionDestroyedWhileHeaderDecodingBlocked) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  session_->qpack_decoder()->OnSetDynamicTableCapacity(1024);

  QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  TestStream* stream = session_->CreateIncomingStream(stream_id);

  // HEADERS frame referencing first dynamic table entry.
  std::string headers_frame_payload;
  ASSERT_TRUE(absl::HexStringToBytes("020080", &headers_frame_payload));
  std::string headers_frame_header =
      HttpEncoder::SerializeHeadersFrameHeader(headers_frame_payload.length());
  std::string headers_frame =
      absl::StrCat(headers_frame_header, headers_frame_payload);
  stream->OnStreamFrame(QuicStreamFrame(stream_id, false, 0, headers_frame));

  // Decoding is blocked because dynamic table entry has not been received yet.
  EXPECT_FALSE(stream->headers_decompressed());

  // |session_| gets destoyed.  That destroys QpackDecoder, a member of
  // QuicSpdySession (derived class), which destroys QpackDecoderHeaderTable.
  // Then |*stream|, owned by QuicSession (base class) get destroyed, which
  // destroys QpackProgessiveDecoder, a registered Observer of
  // QpackDecoderHeaderTable.  This must not cause a crash.
}

TEST_P(QuicSpdySessionTestClient, ResetAfterInvalidIncomingStreamType) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  const QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  ASSERT_TRUE(session_->UsesPendingStreamForFrame(STREAM_FRAME, stream_id));

  // Payload consists of two bytes.  The first byte is an unknown unidirectional
  // stream type.  The second one would be the type of a push stream, but it
  // must not be interpreted as stream type.
  std::string payload;
  ASSERT_TRUE(absl::HexStringToBytes("3f01", &payload));
  QuicStreamFrame frame(stream_id, /* fin = */ false, /* offset = */ 0,
                        payload);

  // A STOP_SENDING frame is sent in response to the unknown stream type.
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(&VerifyAndClearStopSendingFrame);
  session_->OnStreamFrame(frame);

  // There are no active streams.
  EXPECT_EQ(0u, QuicSessionPeer::GetNumOpenDynamicStreams(&*session_));

  // The pending stream is still around, because it did not receive a FIN.
  PendingStream* pending =
      QuicSessionPeer::GetPendingStream(&*session_, stream_id);
  ASSERT_TRUE(pending);

  // The pending stream must ignore read data.
  EXPECT_TRUE(pending->sequencer()->ignore_read_data());

  // If the stream frame is received again, it should be ignored.
  session_->OnStreamFrame(frame);

  // Receive RESET_STREAM.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_id,
                               QUIC_STREAM_CANCELLED,
                               /* bytes_written = */ payload.size());

  session_->OnRstStream(rst_frame);

  // The stream is closed.
  EXPECT_FALSE(QuicSessionPeer::GetPendingStream(&*session_, stream_id));
}

TEST_P(QuicSpdySessionTestClient, FinAfterInvalidIncomingStreamType) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  const QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  ASSERT_TRUE(session_->UsesPendingStreamForFrame(STREAM_FRAME, stream_id));

  // Payload consists of two bytes.  The first byte is an unknown unidirectional
  // stream type.  The second one would be the type of a push stream, but it
  // must not be interpreted as stream type.
  std::string payload;
  ASSERT_TRUE(absl::HexStringToBytes("3f01", &payload));
  QuicStreamFrame frame(stream_id, /* fin = */ false, /* offset = */ 0,
                        payload);

  // A STOP_SENDING frame is sent in response to the unknown stream type.
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillOnce(&VerifyAndClearStopSendingFrame);
  session_->OnStreamFrame(frame);

  // The pending stream is still around, because it did not receive a FIN.
  PendingStream* pending =
      QuicSessionPeer::GetPendingStream(&*session_, stream_id);
  EXPECT_TRUE(pending);

  // The pending stream must ignore read data.
  EXPECT_TRUE(pending->sequencer()->ignore_read_data());

  // If the stream frame is received again, it should be ignored.
  session_->OnStreamFrame(frame);

  // Receive FIN.
  session_->OnStreamFrame(QuicStreamFrame(stream_id, /* fin = */ true,
                                          /* offset = */ payload.size(), ""));

  EXPECT_FALSE(QuicSessionPeer::GetPendingStream(&*session_, stream_id));
}

TEST_P(QuicSpdySessionTestClient, ResetInMiddleOfStreamType) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  const QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  ASSERT_TRUE(session_->UsesPendingStreamForFrame(STREAM_FRAME, stream_id));

  // Payload is the first byte of a two byte varint encoding.
  std::string payload;
  ASSERT_TRUE(absl::HexStringToBytes("40", &payload));
  QuicStreamFrame frame(stream_id, /* fin = */ false, /* offset = */ 0,
                        payload);

  session_->OnStreamFrame(frame);
  EXPECT_TRUE(QuicSessionPeer::GetPendingStream(&*session_, stream_id));

  // Receive RESET_STREAM.
  QuicRstStreamFrame rst_frame(kInvalidControlFrameId, stream_id,
                               QUIC_STREAM_CANCELLED,
                               /* bytes_written = */ payload.size());

  session_->OnRstStream(rst_frame);

  // The stream is closed.
  EXPECT_FALSE(QuicSessionPeer::GetPendingStream(&*session_, stream_id));
}

TEST_P(QuicSpdySessionTestClient, FinInMiddleOfStreamType) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  const QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  ASSERT_TRUE(session_->UsesPendingStreamForFrame(STREAM_FRAME, stream_id));

  // Payload is the first byte of a two byte varint encoding with a FIN.
  std::string payload;
  ASSERT_TRUE(absl::HexStringToBytes("40", &payload));
  QuicStreamFrame frame(stream_id, /* fin = */ true, /* offset = */ 0, payload);

  session_->OnStreamFrame(frame);
  EXPECT_FALSE(QuicSessionPeer::GetPendingStream(&*session_, stream_id));
}

TEST_P(QuicSpdySessionTestClient, DuplicateHttp3UnidirectionalStreams) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  QuicStreamId id1 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  char type1[] = {kControlStream};

  QuicStreamFrame data1(id1, false, 0, absl::string_view(type1, 1));
  EXPECT_CALL(debug_visitor, OnPeerControlStreamCreated(id1));
  session_->OnStreamFrame(data1);
  QuicStreamId id2 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 1);
  QuicStreamFrame data2(id2, false, 0, absl::string_view(type1, 1));
  EXPECT_CALL(debug_visitor, OnPeerControlStreamCreated(id2)).Times(0);
  EXPECT_QUIC_PEER_BUG(
      {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_HTTP_DUPLICATE_UNIDIRECTIONAL_STREAM,
                                    "Control stream is received twice.", _));
        session_->OnStreamFrame(data2);
      },
      "Received a duplicate Control stream: Closing connection.");

  QuicStreamId id3 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 2);
  char type2[]{kQpackEncoderStream};

  QuicStreamFrame data3(id3, false, 0, absl::string_view(type2, 1));
  EXPECT_CALL(debug_visitor, OnPeerQpackEncoderStreamCreated(id3));
  session_->OnStreamFrame(data3);

  QuicStreamId id4 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  QuicStreamFrame data4(id4, false, 0, absl::string_view(type2, 1));
  EXPECT_CALL(debug_visitor, OnPeerQpackEncoderStreamCreated(id4)).Times(0);
  EXPECT_QUIC_PEER_BUG(
      {
        EXPECT_CALL(
            *connection_,
            CloseConnection(QUIC_HTTP_DUPLICATE_UNIDIRECTIONAL_STREAM,
                            "QPACK encoder stream is received twice.", _));
        session_->OnStreamFrame(data4);
      },
      "Received a duplicate QPACK encoder stream: Closing connection.");

  QuicStreamId id5 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 4);
  char type3[]{kQpackDecoderStream};

  QuicStreamFrame data5(id5, false, 0, absl::string_view(type3, 1));
  EXPECT_CALL(debug_visitor, OnPeerQpackDecoderStreamCreated(id5));
  session_->OnStreamFrame(data5);

  QuicStreamId id6 =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 5);
  QuicStreamFrame data6(id6, false, 0, absl::string_view(type3, 1));
  EXPECT_CALL(debug_visitor, OnPeerQpackDecoderStreamCreated(id6)).Times(0);
  EXPECT_QUIC_PEER_BUG(
      {
        EXPECT_CALL(
            *connection_,
            CloseConnection(QUIC_HTTP_DUPLICATE_UNIDIRECTIONAL_STREAM,
                            "QPACK decoder stream is received twice.", _));
        session_->OnStreamFrame(data6);
      },
      "Received a duplicate QPACK decoder stream: Closing connection.");
}

TEST_P(QuicSpdySessionTestClient, EncoderStreamError) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  std::string data;
  ASSERT_TRUE(
      absl::HexStringToBytes("02"   // Encoder stream.
                             "00",  // Duplicate entry 0, but no entries exist.
                             &data));

  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);

  QuicStreamFrame frame(stream_id, /* fin = */ false, /* offset = */ 0, data);

  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_QPACK_ENCODER_STREAM_DUPLICATE_INVALID_RELATIVE_INDEX,
                  "Encoder stream error: Invalid relative index.", _));
  session_->OnStreamFrame(frame);
}

TEST_P(QuicSpdySessionTestClient, DecoderStreamError) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  std::string data;
  ASSERT_TRUE(absl::HexStringToBytes(
      "03"   // Decoder stream.
      "00",  // Insert Count Increment with forbidden increment value of zero.
      &data));

  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);

  QuicStreamFrame frame(stream_id, /* fin = */ false, /* offset = */ 0, data);

  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_QPACK_DECODER_STREAM_INVALID_ZERO_INCREMENT,
                      "Decoder stream error: Invalid increment value 0.", _));
  session_->OnStreamFrame(frame);
}

TEST_P(QuicSpdySessionTestClient, InvalidHttp3GoAway) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_GOAWAY_INVALID_STREAM_ID,
                              "GOAWAY with invalid stream ID", _));
  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  session_->OnHttp3GoAway(stream_id);
}

TEST_P(QuicSpdySessionTestClient, Http3GoAwayLargerIdThanBefore) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  EXPECT_FALSE(session_->goaway_received());
  QuicStreamId stream_id1 =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 0);
  session_->OnHttp3GoAway(stream_id1);
  EXPECT_TRUE(session_->goaway_received());

  EXPECT_CALL(
      *connection_,
      CloseConnection(
          QUIC_HTTP_GOAWAY_ID_LARGER_THAN_PREVIOUS,
          "GOAWAY received with ID 4 greater than previously received ID 0",
          _));
  QuicStreamId stream_id2 =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 1);
  session_->OnHttp3GoAway(stream_id2);
}

TEST_P(QuicSpdySessionTestClient, CloseConnectionOnCancelPush) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Create control stream.
  QuicStreamId receive_control_stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  absl::string_view stream_type(type, 1);
  QuicStreamOffset offset = 0;
  QuicStreamFrame data1(receive_control_stream_id, /* fin = */ false, offset,
                        stream_type);
  offset += stream_type.length();
  EXPECT_CALL(debug_visitor,
              OnPeerControlStreamCreated(receive_control_stream_id));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(receive_control_stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  // First frame has to be SETTINGS.
  std::string serialized_settings = HttpEncoder::SerializeSettingsFrame({});
  QuicStreamFrame data2(receive_control_stream_id, /* fin = */ false, offset,
                        serialized_settings);
  offset += serialized_settings.length();
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(_));
  session_->OnStreamFrame(data2);

  std::string cancel_push_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("03"   // CANCEL_PUSH
                             "01"   // length
                             "00",  // push ID
                             &cancel_push_frame));
  QuicStreamFrame data3(receive_control_stream_id, /* fin = */ false, offset,
                        cancel_push_frame);
  EXPECT_CALL(*connection_, CloseConnection(QUIC_HTTP_FRAME_ERROR,
                                            "CANCEL_PUSH frame received.", _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_,
              SendConnectionClosePacket(QUIC_HTTP_FRAME_ERROR, _,
                                        "CANCEL_PUSH frame received."));
  session_->OnStreamFrame(data3);
}

TEST_P(QuicSpdySessionTestServer, OnSetting) {
  Initialize();
  CompleteHandshake();
  if (VersionUsesHttp3(transport_version())) {
    EXPECT_EQ(std::numeric_limits<size_t>::max(),
              session_->max_outbound_header_list_size());
    session_->OnSetting(SETTINGS_MAX_FIELD_SECTION_SIZE, 5);
    EXPECT_EQ(5u, session_->max_outbound_header_list_size());

    EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
        .WillRepeatedly(Return(WriteResult(WRITE_STATUS_OK, 0)));
    QpackEncoder* qpack_encoder = session_->qpack_encoder();
    EXPECT_EQ(0u, QpackEncoderPeer::maximum_blocked_streams(qpack_encoder));
    session_->OnSetting(SETTINGS_QPACK_BLOCKED_STREAMS, 12);
    EXPECT_EQ(12u, QpackEncoderPeer::maximum_blocked_streams(qpack_encoder));

    QpackEncoderHeaderTable* header_table =
        QpackEncoderPeer::header_table(qpack_encoder);
    EXPECT_EQ(0u, header_table->maximum_dynamic_table_capacity());
    session_->OnSetting(SETTINGS_QPACK_MAX_TABLE_CAPACITY, 37);
    EXPECT_EQ(37u, header_table->maximum_dynamic_table_capacity());

    return;
  }

  EXPECT_EQ(std::numeric_limits<size_t>::max(),
            session_->max_outbound_header_list_size());
  session_->OnSetting(SETTINGS_MAX_FIELD_SECTION_SIZE, 5);
  EXPECT_EQ(5u, session_->max_outbound_header_list_size());

  spdy::HpackEncoder* hpack_encoder =
      QuicSpdySessionPeer::GetSpdyFramer(&*session_)->GetHpackEncoder();
  EXPECT_EQ(4096u, hpack_encoder->CurrentHeaderTableSizeSetting());
  session_->OnSetting(spdy::SETTINGS_HEADER_TABLE_SIZE, 59);
  EXPECT_EQ(59u, hpack_encoder->CurrentHeaderTableSizeSetting());
}

TEST_P(QuicSpdySessionTestServer, FineGrainedHpackErrorCodes) {
  Initialize();
  if (VersionUsesHttp3(transport_version())) {
    // HPACK is not used in HTTP/3.
    return;
  }

  QuicStreamId request_stream_id = 5;
  session_->CreateIncomingStream(request_stream_id);

  // Index 126 does not exist (static table has 61 entries and dynamic table is
  // empty).
  std::string headers_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("000006"    // length
                             "01"        // type
                             "24"        // flags: PRIORITY | END_HEADERS
                             "00000005"  // stream_id
                             "00000000"  // stream dependency
                             "10"        // weight
                             "fe",       // payload: reference to index 126.
                             &headers_frame));
  QuicStreamId headers_stream_id =
      QuicUtils::GetHeadersStreamId(transport_version());
  QuicStreamFrame data(headers_stream_id, false, 0, headers_frame);

  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HPACK_INVALID_INDEX,
                      "SPDY framing error: HPACK_INVALID_INDEX",
                      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  session_->OnStreamFrame(data);
}

TEST_P(QuicSpdySessionTestServer, PeerClosesCriticalReceiveStream) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  CompleteHandshake();

  struct {
    char type;
    const char* error_details;
  } kTestData[] = {
      {kControlStream, "RESET_STREAM received for receive control stream"},
      {kQpackEncoderStream, "RESET_STREAM received for QPACK receive stream"},
      {kQpackDecoderStream, "RESET_STREAM received for QPACK receive stream"},
  };
  for (size_t i = 0; i < ABSL_ARRAYSIZE(kTestData); ++i) {
    QuicStreamId stream_id =
        GetNthClientInitiatedUnidirectionalStreamId(transport_version(), i + 1);
    const QuicByteCount data_length = 1;
    QuicStreamFrame data(stream_id, false, 0,
                         absl::string_view(&kTestData[i].type, data_length));
    session_->OnStreamFrame(data);

    EXPECT_CALL(*connection_, CloseConnection(QUIC_HTTP_CLOSED_CRITICAL_STREAM,
                                              kTestData[i].error_details, _));

    QuicRstStreamFrame rst(kInvalidControlFrameId, stream_id,
                           QUIC_STREAM_CANCELLED, data_length);
    session_->OnRstStream(rst);
  }
}

TEST_P(QuicSpdySessionTestServer,
       H3ControlStreamsLimitedByConnectionFlowControl) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }
  // Ensure connection level flow control blockage.
  QuicFlowControllerPeer::SetSendWindowOffset(session_->flow_controller(), 0);
  EXPECT_TRUE(session_->IsConnectionFlowControlBlocked());

  QuicSendControlStream* send_control_stream =
      QuicSpdySessionPeer::GetSendControlStream(&*session_);
  // Mark send_control stream write blocked.
  session_->MarkConnectionLevelWriteBlocked(send_control_stream->id());
  EXPECT_FALSE(session_->WillingAndAbleToWrite());
}

TEST_P(QuicSpdySessionTestServer, PeerClosesCriticalSendStream) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  QuicSendControlStream* control_stream =
      QuicSpdySessionPeer::GetSendControlStream(&*session_);
  ASSERT_TRUE(control_stream);

  QuicStopSendingFrame stop_sending_control_stream(
      kInvalidControlFrameId, control_stream->id(), QUIC_STREAM_CANCELLED);
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HTTP_CLOSED_CRITICAL_STREAM,
                      "STOP_SENDING received for send control stream", _));
  session_->OnStopSendingFrame(stop_sending_control_stream);

  QpackSendStream* decoder_stream =
      QuicSpdySessionPeer::GetQpackDecoderSendStream(&*session_);
  ASSERT_TRUE(decoder_stream);

  QuicStopSendingFrame stop_sending_decoder_stream(
      kInvalidControlFrameId, decoder_stream->id(), QUIC_STREAM_CANCELLED);
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HTTP_CLOSED_CRITICAL_STREAM,
                      "STOP_SENDING received for QPACK send stream", _));
  session_->OnStopSendingFrame(stop_sending_decoder_stream);

  QpackSendStream* encoder_stream =
      QuicSpdySessionPeer::GetQpackEncoderSendStream(&*session_);
  ASSERT_TRUE(encoder_stream);

  QuicStopSendingFrame stop_sending_encoder_stream(
      kInvalidControlFrameId, encoder_stream->id(), QUIC_STREAM_CANCELLED);
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HTTP_CLOSED_CRITICAL_STREAM,
                      "STOP_SENDING received for QPACK send stream", _));
  session_->OnStopSendingFrame(stop_sending_encoder_stream);
}

TEST_P(QuicSpdySessionTestServer, CloseConnectionOnCancelPush) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Create control stream.
  QuicStreamId receive_control_stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  absl::string_view stream_type(type, 1);
  QuicStreamOffset offset = 0;
  QuicStreamFrame data1(receive_control_stream_id, /* fin = */ false, offset,
                        stream_type);
  offset += stream_type.length();
  EXPECT_CALL(debug_visitor,
              OnPeerControlStreamCreated(receive_control_stream_id));
  session_->OnStreamFrame(data1);
  EXPECT_EQ(receive_control_stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  // First frame has to be SETTINGS.
  std::string serialized_settings = HttpEncoder::SerializeSettingsFrame({});
  QuicStreamFrame data2(receive_control_stream_id, /* fin = */ false, offset,
                        serialized_settings);
  offset += serialized_settings.length();
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(_));
  session_->OnStreamFrame(data2);

  std::string cancel_push_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("03"   // CANCEL_PUSH
                             "01"   // length
                             "00",  // push ID
                             &cancel_push_frame));
  QuicStreamFrame data3(receive_control_stream_id, /* fin = */ false, offset,
                        cancel_push_frame);
  EXPECT_CALL(*connection_, CloseConnection(QUIC_HTTP_FRAME_ERROR,
                                            "CANCEL_PUSH frame received.", _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_,
              SendConnectionClosePacket(QUIC_HTTP_FRAME_ERROR, _,
                                        "CANCEL_PUSH frame received."));
  session_->OnStreamFrame(data3);
}

TEST_P(QuicSpdySessionTestServer, Http3GoAwayWhenClosingConnection) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  EXPECT_CALL(debug_visitor, OnSettingsFrameSent(_));
  CompleteHandshake();

  QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);

  // Create stream by receiving some data (CreateIncomingStream() would not
  // update the session's largest peer created stream ID).
  const QuicByteCount headers_payload_length = 10;
  std::string headers_frame_header =
      HttpEncoder::SerializeHeadersFrameHeader(headers_payload_length);
  EXPECT_CALL(debug_visitor,
              OnHeadersFrameReceived(stream_id, headers_payload_length));
  session_->OnStreamFrame(
      QuicStreamFrame(stream_id, false, 0, headers_frame_header));

  EXPECT_EQ(stream_id, QuicSessionPeer::GetLargestPeerCreatedStreamId(
                           &*session_, /*unidirectional = */ false));

  // Stream with stream_id is already received and potentially processed,
  // therefore a GOAWAY frame is sent with the next stream ID.
  EXPECT_CALL(debug_visitor,
              OnGoAwayFrameSent(stream_id +
                                QuicUtils::StreamIdDelta(transport_version())));

  // Close connection.
  EXPECT_CALL(*writer_, WritePacket(_, _, _, _, _, _))
      .WillRepeatedly(Return(WriteResult(WRITE_STATUS_OK, 0)));
  EXPECT_CALL(*connection_, CloseConnection(QUIC_NO_ERROR, _, _))
      .WillOnce(
          Invoke(connection_, &MockQuicConnection::ReallyCloseConnection));
  EXPECT_CALL(*connection_, SendConnectionClosePacket(QUIC_NO_ERROR, _, _))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::ReallySendConnectionClosePacket));
  connection_->CloseConnection(
      QUIC_NO_ERROR, "closing connection",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
}

TEST_P(QuicSpdySessionTestClient, DoNotSendInitialMaxPushIdIfNotSet) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  InSequence s;
  EXPECT_CALL(debug_visitor, OnSettingsFrameSent(_));

  CompleteHandshake();
}

TEST_P(QuicSpdySessionTestClient, ReceiveSpdySettingInHttp3) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  SettingsFrame frame;
  frame.values[SETTINGS_MAX_FIELD_SECTION_SIZE] = 5;
  // https://datatracker.ietf.org/doc/html/draft-ietf-quic-http-30#section-7.2.4.1
  // specifies the presence of HTTP/2 setting as error.
  frame.values[spdy::SETTINGS_INITIAL_WINDOW_SIZE] = 100;

  CompleteHandshake();

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_HTTP_RECEIVE_SPDY_SETTING, _, _));
  session_->OnSettingsFrame(frame);
}

TEST_P(QuicSpdySessionTestClient, ReceiveAcceptChFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  // Create control stream.
  QuicStreamId receive_control_stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  char type[] = {kControlStream};
  absl::string_view stream_type(type, 1);
  QuicStreamOffset offset = 0;
  QuicStreamFrame data1(receive_control_stream_id, /* fin = */ false, offset,
                        stream_type);
  offset += stream_type.length();
  EXPECT_CALL(debug_visitor,
              OnPeerControlStreamCreated(receive_control_stream_id));

  session_->OnStreamFrame(data1);
  EXPECT_EQ(receive_control_stream_id,
            QuicSpdySessionPeer::GetReceiveControlStream(&*session_)->id());

  // First frame has to be SETTINGS.
  std::string serialized_settings = HttpEncoder::SerializeSettingsFrame({});
  QuicStreamFrame data2(receive_control_stream_id, /* fin = */ false, offset,
                        serialized_settings);
  offset += serialized_settings.length();
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(_));

  session_->OnStreamFrame(data2);

  // Receive ACCEPT_CH frame.
  AcceptChFrame accept_ch;
  accept_ch.entries.push_back({"foo", "bar"});
  std::string accept_ch_frame = HttpEncoder::SerializeAcceptChFrame(accept_ch);
  QuicStreamFrame data3(receive_control_stream_id, /* fin = */ false, offset,
                        accept_ch_frame);

  EXPECT_CALL(debug_visitor, OnAcceptChFrameReceived(accept_ch));
  EXPECT_CALL(*session_, OnAcceptChFrame(accept_ch));

  session_->OnStreamFrame(data3);
}

TEST_P(QuicSpdySessionTestClient, AcceptChViaAlps) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  std::string serialized_accept_ch_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("4089"     // type (ACCEPT_CH)
                             "08"       // length
                             "03"       // length of origin
                             "666f6f"   // origin "foo"
                             "03"       // length of value
                             "626172",  // value "bar"
                             &serialized_accept_ch_frame));

  AcceptChFrame expected_accept_ch_frame{{{"foo", "bar"}}};
  EXPECT_CALL(debug_visitor,
              OnAcceptChFrameReceivedViaAlps(expected_accept_ch_frame));

  auto error = session_->OnAlpsData(
      reinterpret_cast<const uint8_t*>(serialized_accept_ch_frame.data()),
      serialized_accept_ch_frame.size());
  EXPECT_FALSE(error);
}

TEST_P(QuicSpdySessionTestClient, AlpsForbiddenFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  std::string forbidden_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("00"       // type (DATA)
                             "03"       // length
                             "66666f",  // "foo"
                             &forbidden_frame));

  auto error = session_->OnAlpsData(
      reinterpret_cast<const uint8_t*>(forbidden_frame.data()),
      forbidden_frame.size());
  ASSERT_TRUE(error);
  EXPECT_EQ("DATA frame forbidden", error.value());
}

TEST_P(QuicSpdySessionTestClient, AlpsIncompleteFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  std::string incomplete_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"   // type (SETTINGS)
                             "03",  // non-zero length but empty payload
                             &incomplete_frame));

  auto error = session_->OnAlpsData(
      reinterpret_cast<const uint8_t*>(incomplete_frame.data()),
      incomplete_frame.size());
  ASSERT_TRUE(error);
  EXPECT_EQ("incomplete HTTP/3 frame", error.value());
}

// After receiving a SETTINGS frame via ALPS,
// another SETTINGS frame is still allowed on control frame.
TEST_P(QuicSpdySessionTestClient, SettingsViaAlpsThenOnControlStream) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  EXPECT_EQ(0u, qpack_encoder->MaximumDynamicTableCapacity());
  EXPECT_EQ(0u, qpack_encoder->maximum_blocked_streams());

  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);

  std::string serialized_settings_frame1;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"    // type (SETTINGS)
                             "05"    // length
                             "01"    // SETTINGS_QPACK_MAX_TABLE_CAPACITY
                             "4400"  // 0x0400 = 1024
                             "07"    // SETTINGS_QPACK_BLOCKED_STREAMS
                             "20",   // 0x20 = 32
                             &serialized_settings_frame1));

  SettingsFrame expected_settings_frame1{
      {{SETTINGS_QPACK_MAX_TABLE_CAPACITY, 1024},
       {SETTINGS_QPACK_BLOCKED_STREAMS, 32}}};
  EXPECT_CALL(debug_visitor,
              OnSettingsFrameReceivedViaAlps(expected_settings_frame1));

  auto error = session_->OnAlpsData(
      reinterpret_cast<const uint8_t*>(serialized_settings_frame1.data()),
      serialized_settings_frame1.size());
  EXPECT_FALSE(error);

  EXPECT_EQ(1024u, qpack_encoder->MaximumDynamicTableCapacity());
  EXPECT_EQ(32u, qpack_encoder->maximum_blocked_streams());

  const QuicStreamId control_stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);
  EXPECT_CALL(debug_visitor, OnPeerControlStreamCreated(control_stream_id));

  std::string stream_type;
  ASSERT_TRUE(absl::HexStringToBytes("00", &stream_type));
  session_->OnStreamFrame(QuicStreamFrame(control_stream_id, /* fin = */ false,
                                          /* offset = */ 0, stream_type));

  // SETTINGS_QPACK_MAX_TABLE_CAPACITY, if advertised again, MUST have identical
  // value.
  // SETTINGS_QPACK_BLOCKED_STREAMS is a limit.  Limits MUST NOT be reduced, but
  // increasing is okay.
  SettingsFrame expected_settings_frame2{
      {{SETTINGS_QPACK_MAX_TABLE_CAPACITY, 1024},
       {SETTINGS_QPACK_BLOCKED_STREAMS, 48}}};
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(expected_settings_frame2));
  std::string serialized_settings_frame2;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"    // type (SETTINGS)
                             "05"    // length
                             "01"    // SETTINGS_QPACK_MAX_TABLE_CAPACITY
                             "4400"  // 0x0400 = 1024
                             "07"    // SETTINGS_QPACK_BLOCKED_STREAMS
                             "30",   // 0x30 = 48
                             &serialized_settings_frame2));
  session_->OnStreamFrame(QuicStreamFrame(control_stream_id, /* fin = */ false,
                                          /* offset = */ stream_type.length(),
                                          serialized_settings_frame2));

  EXPECT_EQ(1024u, qpack_encoder->MaximumDynamicTableCapacity());
  EXPECT_EQ(48u, qpack_encoder->maximum_blocked_streams());
}

// A SETTINGS frame received via ALPS and another one on the control stream
// cannot have conflicting values.
TEST_P(QuicSpdySessionTestClient,
       SettingsViaAlpsConflictsSettingsViaControlStream) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  CompleteHandshake();
  QpackEncoder* qpack_encoder = session_->qpack_encoder();
  EXPECT_EQ(0u, qpack_encoder->MaximumDynamicTableCapacity());

  std::string serialized_settings_frame1;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"     // type (SETTINGS)
                             "03"     // length
                             "01"     // SETTINGS_QPACK_MAX_TABLE_CAPACITY
                             "4400",  // 0x0400 = 1024
                             &serialized_settings_frame1));

  auto error = session_->OnAlpsData(
      reinterpret_cast<const uint8_t*>(serialized_settings_frame1.data()),
      serialized_settings_frame1.size());
  EXPECT_FALSE(error);

  EXPECT_EQ(1024u, qpack_encoder->MaximumDynamicTableCapacity());

  const QuicStreamId control_stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 0);

  std::string stream_type;
  ASSERT_TRUE(absl::HexStringToBytes("00", &stream_type));
  session_->OnStreamFrame(QuicStreamFrame(control_stream_id, /* fin = */ false,
                                          /* offset = */ 0, stream_type));

  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_HTTP_ZERO_RTT_RESUMPTION_SETTINGS_MISMATCH,
                      "Server sent an SETTINGS_QPACK_MAX_TABLE_CAPACITY: "
                      "32 while current value is: 1024",
                      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET));
  std::string serialized_settings_frame2;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"   // type (SETTINGS)
                             "02"   // length
                             "01"   // SETTINGS_QPACK_MAX_TABLE_CAPACITY
                             "20",  // 0x20 = 32
                             &serialized_settings_frame2));
  session_->OnStreamFrame(QuicStreamFrame(control_stream_id, /* fin = */ false,
                                          /* offset = */ stream_type.length(),
                                          serialized_settings_frame2));
}

TEST_P(QuicSpdySessionTestClient, AlpsTwoSettingsFrame) {
  Initialize();
  if (!VersionUsesHttp3(transport_version())) {
    return;
  }

  std::string banned_frame;
  ASSERT_TRUE(
      absl::HexStringToBytes("04"   // type (SETTINGS)
                             "00"   // length
                             "04"   // type (SETTINGS)
                             "00",  // length
                             &banned_frame));

  auto error = session_->OnAlpsData(
      reinterpret_cast<const uint8_t*>(banned_frame.data()),
      banned_frame.size());
  ASSERT_TRUE(error);
  EXPECT_EQ("multiple SETTINGS frames", error.value());
}

void QuicSpdySessionTestBase::TestHttpDatagramSetting(
    HttpDatagramSupport local_support, HttpDatagramSupport remote_support,
    HttpDatagramSupport expected_support, bool expected_datagram_supported) {
  if (!version().UsesHttp3()) {
    return;
  }
  CompleteHandshake();
  session_->set_local_http_datagram_support(local_support);
  // HTTP/3 datagrams aren't supported before SETTINGS are received.
  EXPECT_FALSE(session_->SupportsH3Datagram());
  EXPECT_EQ(session_->http_datagram_support(), HttpDatagramSupport::kNone);
  // Receive SETTINGS.
  SettingsFrame settings;
  switch (remote_support) {
    case HttpDatagramSupport::kNone:
      break;
    case HttpDatagramSupport::kDraft04:
      settings.values[SETTINGS_H3_DATAGRAM_DRAFT04] = 1;
      break;
    case HttpDatagramSupport::kRfc:
      settings.values[SETTINGS_H3_DATAGRAM] = 1;
      break;
    case HttpDatagramSupport::kRfcAndDraft04:
      settings.values[SETTINGS_H3_DATAGRAM] = 1;
      settings.values[SETTINGS_H3_DATAGRAM_DRAFT04] = 1;
      break;
  }
  std::string data = std::string(1, kControlStream) +
                     HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  QuicStreamFrame frame(stream_id, /*fin=*/false, /*offset=*/0, data);
  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  session_->set_debug_visitor(&debug_visitor);
  EXPECT_CALL(debug_visitor, OnPeerControlStreamCreated(stream_id));
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(settings));
  session_->OnStreamFrame(frame);
  EXPECT_EQ(session_->http_datagram_support(), expected_support);
  EXPECT_EQ(session_->SupportsH3Datagram(), expected_datagram_supported);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal04Remote04) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kDraft04,
      /*remote_support=*/HttpDatagramSupport::kDraft04,
      /*expected_support=*/HttpDatagramSupport::kDraft04,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal04Remote09) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kDraft04,
      /*remote_support=*/HttpDatagramSupport::kRfc,
      /*expected_support=*/HttpDatagramSupport::kNone,
      /*expected_datagram_supported=*/false);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal04Remote04And09) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kDraft04,
      /*remote_support=*/HttpDatagramSupport::kRfcAndDraft04,
      /*expected_support=*/HttpDatagramSupport::kDraft04,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal09Remote04) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kRfc,
      /*remote_support=*/HttpDatagramSupport::kDraft04,
      /*expected_support=*/HttpDatagramSupport::kNone,
      /*expected_datagram_supported=*/false);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal09Remote09) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kRfc,
      /*remote_support=*/HttpDatagramSupport::kRfc,
      /*expected_support=*/HttpDatagramSupport::kRfc,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal09Remote04And09) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kRfc,
      /*remote_support=*/HttpDatagramSupport::kRfcAndDraft04,
      /*expected_support=*/HttpDatagramSupport::kRfc,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal04And09Remote04) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kRfcAndDraft04,
      /*remote_support=*/HttpDatagramSupport::kDraft04,
      /*expected_support=*/HttpDatagramSupport::kDraft04,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient, HttpDatagramSettingLocal04And09Remote09) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kRfcAndDraft04,
      /*remote_support=*/HttpDatagramSupport::kRfc,
      /*expected_support=*/HttpDatagramSupport::kRfc,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient,
       HttpDatagramSettingLocal04And09Remote04And09) {
  Initialize();
  TestHttpDatagramSetting(
      /*local_support=*/HttpDatagramSupport::kRfcAndDraft04,
      /*remote_support=*/HttpDatagramSupport::kRfcAndDraft04,
      /*expected_support=*/HttpDatagramSupport::kRfc,
      /*expected_datagram_supported=*/true);
}

TEST_P(QuicSpdySessionTestClient, WebTransportSettingDraft02OnlyBothSides) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_locally_supported_web_transport_versions(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft02}));

  EXPECT_FALSE(session_->SupportsWebTransport());
  CompleteHandshake();
  ReceiveWebTransportSettings(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft02}));
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());
  EXPECT_TRUE(session_->SupportsWebTransport());
  EXPECT_EQ(session_->SupportedWebTransportVersion(),
            WebTransportHttp3Version::kDraft02);
}

TEST_P(QuicSpdySessionTestClient, WebTransportSettingDraft07OnlyBothSides) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_locally_supported_web_transport_versions(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft07}));

  EXPECT_FALSE(session_->SupportsWebTransport());
  CompleteHandshake();
  ReceiveWebTransportSettings(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft07}));
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());
  EXPECT_TRUE(session_->SupportsWebTransport());
  EXPECT_EQ(session_->SupportedWebTransportVersion(),
            WebTransportHttp3Version::kDraft07);
}

TEST_P(QuicSpdySessionTestClient, WebTransportSettingBothDraftsBothSides) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_locally_supported_web_transport_versions(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft02,
                                   WebTransportHttp3Version::kDraft07}));

  EXPECT_FALSE(session_->SupportsWebTransport());
  CompleteHandshake();
  ReceiveWebTransportSettings(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft02,
                                   WebTransportHttp3Version::kDraft07}));
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());
  EXPECT_TRUE(session_->SupportsWebTransport());
  EXPECT_EQ(session_->SupportedWebTransportVersion(),
            WebTransportHttp3Version::kDraft07);
}

TEST_P(QuicSpdySessionTestClient, WebTransportSettingVersionMismatch) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_locally_supported_web_transport_versions(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft07}));

  EXPECT_FALSE(session_->SupportsWebTransport());
  CompleteHandshake();
  ReceiveWebTransportSettings(
      WebTransportHttp3VersionSet({WebTransportHttp3Version::kDraft02}));
  EXPECT_FALSE(session_->SupportsWebTransport());
  EXPECT_EQ(session_->SupportedWebTransportVersion(), std::nullopt);
}

TEST_P(QuicSpdySessionTestClient, WebTransportSettingSetToZero) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_supports_webtransport(true);

  EXPECT_FALSE(session_->SupportsWebTransport());

  StrictMock<MockHttp3DebugVisitor> debug_visitor;
  // Note that this does not actually fill out correct settings because the
  // settings are filled in at the construction time.
  EXPECT_CALL(debug_visitor, OnSettingsFrameSent(_));
  session_->set_debug_visitor(&debug_visitor);
  CompleteHandshake();

  SettingsFrame server_settings;
  server_settings.values[SETTINGS_H3_DATAGRAM_DRAFT04] = 1;
  server_settings.values[SETTINGS_WEBTRANS_DRAFT00] = 0;
  std::string data = std::string(1, kControlStream) +
                     HttpEncoder::SerializeSettingsFrame(server_settings);
  QuicStreamId stream_id =
      GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  QuicStreamFrame frame(stream_id, /*fin=*/false, /*offset=*/0, data);
  EXPECT_CALL(debug_visitor, OnPeerControlStreamCreated(stream_id));
  EXPECT_CALL(debug_visitor, OnSettingsFrameReceived(server_settings));
  session_->OnStreamFrame(frame);
  EXPECT_FALSE(session_->SupportsWebTransport());
}

TEST_P(QuicSpdySessionTestServer, WebTransportSetting) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_supports_webtransport(true);

  EXPECT_FALSE(session_->SupportsWebTransport());
  EXPECT_FALSE(session_->ShouldProcessIncomingRequests());

  CompleteHandshake();

  ReceiveWebTransportSettings();
  EXPECT_TRUE(session_->SupportsWebTransport());
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());
}

TEST_P(QuicSpdySessionTestServer, BufferingIncomingStreams) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_supports_webtransport(true);

  CompleteHandshake();
  QuicStreamId session_id =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 1);

  QuicStreamId data_stream_id =
      GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 4);
  ReceiveWebTransportUnidirectionalStream(session_id, data_stream_id);

  ReceiveWebTransportSettings();

  ReceiveWebTransportSession(session_id);
  WebTransportHttp3* web_transport =
      session_->GetWebTransportSession(session_id);
  ASSERT_TRUE(web_transport != nullptr);

  EXPECT_EQ(web_transport->NumberOfAssociatedStreams(), 1u);

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  EXPECT_CALL(*connection_, OnStreamReset(session_id, _));
  EXPECT_CALL(
      *connection_,
      OnStreamReset(data_stream_id, QUIC_STREAM_WEBTRANSPORT_SESSION_GONE));
  session_->ResetStream(session_id, QUIC_STREAM_INTERNAL_ERROR);
}

TEST_P(QuicSpdySessionTestServer, BufferingIncomingStreamsLimit) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_supports_webtransport(true);

  CompleteHandshake();
  QuicStreamId session_id =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 1);

  const int streams_to_send = kMaxUnassociatedWebTransportStreams + 4;
  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  EXPECT_CALL(*connection_,
              OnStreamReset(
                  _, QUIC_STREAM_WEBTRANSPORT_BUFFERED_STREAMS_LIMIT_EXCEEDED))
      .Times(4);
  for (int i = 0; i < streams_to_send; i++) {
    QuicStreamId data_stream_id =
        GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 4 + i);
    ReceiveWebTransportUnidirectionalStream(session_id, data_stream_id);
  }

  ReceiveWebTransportSettings();

  ReceiveWebTransportSession(session_id);
  WebTransportHttp3* web_transport =
      session_->GetWebTransportSession(session_id);
  ASSERT_TRUE(web_transport != nullptr);

  EXPECT_EQ(web_transport->NumberOfAssociatedStreams(),
            kMaxUnassociatedWebTransportStreams);

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  EXPECT_CALL(*connection_, OnStreamReset(_, _))
      .Times(kMaxUnassociatedWebTransportStreams + 1);
  session_->ResetStream(session_id, QUIC_STREAM_INTERNAL_ERROR);
}

TEST_P(QuicSpdySessionTestServer, BufferingIncomingStreamsWithFin) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }

  CompleteHandshake();

  const UberQuicStreamIdManager& stream_id_manager =
      *QuicSessionPeer::ietf_streamid_manager(&*session_);
  const QuicStreamId initial_advertized_max_streams =
      stream_id_manager.advertised_max_incoming_unidirectional_streams();
  const size_t num_streams_to_open =
      session_->max_open_incoming_unidirectional_streams();
  // The max_streams limit should be increased repeatedly.
  EXPECT_CALL(*connection_, SendControlFrame(_)).Times(testing::AnyNumber());
  for (size_t i = 0; i < num_streams_to_open; i++) {
    const QuicStreamId stream_id =
        GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 4 + i);
    QuicStreamFrame frame(stream_id, /*fin=*/true, /*offset=*/0, /*data=*/"");
    session_->OnStreamFrame(frame);
  }
  EXPECT_LT(initial_advertized_max_streams,
            stream_id_manager.advertised_max_incoming_unidirectional_streams());
  EXPECT_EQ(0, session_->pending_streams_size());
}

TEST_P(QuicSpdySessionTestServer, ResetOutgoingWebTransportStreams) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_supports_webtransport(true);

  CompleteHandshake();
  QuicStreamId session_id =
      GetNthClientInitiatedBidirectionalStreamId(transport_version(), 1);

  ReceiveWebTransportSettings();
  ReceiveWebTransportSession(session_id);
  WebTransportHttp3* web_transport =
      session_->GetWebTransportSession(session_id);
  ASSERT_TRUE(web_transport != nullptr);

  session_->set_writev_consumes_all_data(true);
  EXPECT_TRUE(web_transport->CanOpenNextOutgoingUnidirectionalStream());
  EXPECT_EQ(web_transport->NumberOfAssociatedStreams(), 0u);
  WebTransportStream* stream =
      web_transport->OpenOutgoingUnidirectionalStream();
  EXPECT_EQ(web_transport->NumberOfAssociatedStreams(), 1u);
  ASSERT_TRUE(stream != nullptr);
  QuicStreamId stream_id = stream->GetStreamId();

  EXPECT_CALL(*connection_, SendControlFrame(_))
      .WillRepeatedly(&ClearControlFrame);
  EXPECT_CALL(*connection_, OnStreamReset(session_id, _));
  EXPECT_CALL(*connection_,
              OnStreamReset(stream_id, QUIC_STREAM_WEBTRANSPORT_SESSION_GONE));
  session_->ResetStream(session_id, QUIC_STREAM_INTERNAL_ERROR);
  EXPECT_EQ(web_transport->NumberOfAssociatedStreams(), 0u);
}

TEST_P(QuicSpdySessionTestClient, WebTransportWithoutExtendedConnect) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  session_->set_local_http_datagram_support(
      HttpDatagramSupport::kRfcAndDraft04);
  session_->set_supports_webtransport(true);

  EXPECT_FALSE(session_->SupportsWebTransport());
  CompleteHandshake();

  SettingsFrame settings;
  settings.values[SETTINGS_H3_DATAGRAM_DRAFT04] = 1;
  settings.values[SETTINGS_WEBTRANS_DRAFT00] = 1;
  // No SETTINGS_ENABLE_CONNECT_PROTOCOL here.
  std::string data = std::string(1, kControlStream) +
                     HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamId control_stream_id =
      session_->perspective() == Perspective::IS_SERVER
          ? GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3)
          : GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  QuicStreamFrame frame(control_stream_id, /*fin=*/false, /*offset=*/0, data);
  session_->OnStreamFrame(frame);

  EXPECT_TRUE(session_->SupportsWebTransport());
}

// Regression test for b/208997000.
TEST_P(QuicSpdySessionTestClient, LimitEncoderDynamicTableSize) {
  Initialize();
  if (version().UsesHttp3()) {
    return;
  }
  CompleteHandshake();

  QuicSpdySessionPeer::SetHeadersStream(&*session_, nullptr);
  TestHeadersStream* headers_stream =
      new StrictMock<TestHeadersStream>(&*session_);
  QuicSpdySessionPeer::SetHeadersStream(&*session_, headers_stream);
  session_->MarkConnectionLevelWriteBlocked(headers_stream->id());

  // Peer sends very large value.
  session_->OnSetting(spdy::SETTINGS_HEADER_TABLE_SIZE, 1024 * 1024 * 1024);

  TestStream* stream = session_->CreateOutgoingBidirectionalStream();
  EXPECT_CALL(*writer_, IsWriteBlocked()).WillRepeatedly(Return(true));
  HttpHeaderBlock headers;
  headers[":method"] = "GET";  // entry with index 2 in HPACK static table
  stream->WriteHeaders(std::move(headers), /* fin = */ true, nullptr);

  EXPECT_TRUE(headers_stream->HasBufferedData());
  QuicStreamSendBufferBase& send_buffer =
      QuicStreamPeer::SendBuffer(headers_stream);
  ASSERT_EQ(1u, send_buffer.size());

  absl::string_view stream_data = send_buffer.LatestWriteForTest();

  std::string expected_stream_data_1;
  ASSERT_TRUE(
      absl::HexStringToBytes("000009"  // frame length
                             "01"      // frame type HEADERS
                             "25",  // flags END_STREAM | END_HEADERS | PRIORITY
                             &expected_stream_data_1));
  EXPECT_EQ(expected_stream_data_1, stream_data.substr(0, 5));
  stream_data.remove_prefix(5);

  // Ignore stream ID as it might differ between QUIC versions.
  stream_data.remove_prefix(4);

  std::string expected_stream_data_2;

  ASSERT_TRUE(
      absl::HexStringToBytes("00000000"  // stream dependency
                             "92",       // stream weight
                             &expected_stream_data_2));
  EXPECT_EQ(expected_stream_data_2, stream_data.substr(0, 5));
  stream_data.remove_prefix(5);

  std::string expected_stream_data_3;
  ASSERT_TRUE(absl::HexStringToBytes(
      "3fe17f"  // Dynamic Table Size Update to 16384
      "82",     // Indexed Header Field Representation with index 2
      &expected_stream_data_3));
  EXPECT_EQ(expected_stream_data_3, stream_data);
}

class QuicSpdySessionTestServerNoExtendedConnect
    : public QuicSpdySessionTestBase {
 public:
  QuicSpdySessionTestServerNoExtendedConnect()
      : QuicSpdySessionTestBase(Perspective::IS_SERVER, false) {}
};

INSTANTIATE_TEST_SUITE_P(Tests, QuicSpdySessionTestServerNoExtendedConnect,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

// Tests that receiving SETTINGS_ENABLE_CONNECT_PROTOCOL = 1 doesn't enable
// server session to support extended CONNECT.
TEST_P(QuicSpdySessionTestServerNoExtendedConnect,
       WebTransportSettingNoEffect) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }

  EXPECT_FALSE(session_->SupportsWebTransport());
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());

  CompleteHandshake();

  ReceiveWebTransportSettings();
  EXPECT_FALSE(session_->allow_extended_connect());
  EXPECT_FALSE(session_->SupportsWebTransport());
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());
}

TEST_P(QuicSpdySessionTestServerNoExtendedConnect, BadExtendedConnectSetting) {
  Initialize();
  if (!version().UsesHttp3()) {
    return;
  }
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);

  EXPECT_FALSE(session_->SupportsWebTransport());
  EXPECT_TRUE(session_->ShouldProcessIncomingRequests());

  CompleteHandshake();

  // ENABLE_CONNECT_PROTOCOL setting value has to be 1 or 0;
  SettingsFrame settings;
  settings.values[SETTINGS_ENABLE_CONNECT_PROTOCOL] = 2;
  std::string data = std::string(1, kControlStream) +
                     HttpEncoder::SerializeSettingsFrame(settings);
  QuicStreamId control_stream_id =
      session_->perspective() == Perspective::IS_SERVER
          ? GetNthClientInitiatedUnidirectionalStreamId(transport_version(), 3)
          : GetNthServerInitiatedUnidirectionalStreamId(transport_version(), 3);
  QuicStreamFrame frame(control_stream_id, /*fin=*/false, /*offset=*/0, data);
  EXPECT_QUIC_PEER_BUG(
      {
        EXPECT_CALL(*connection_,
                    CloseConnection(QUIC_HTTP_INVALID_SETTING_VALUE, _, _));
        session_->OnStreamFrame(frame);
      },
      "Received SETTINGS_ENABLE_CONNECT_PROTOCOL with invalid value");
}

}  // namespace
}  // namespace test
}  // namespace quic
