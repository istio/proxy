// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_crypto_stream.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "quiche/quic/core/crypto/crypto_handshake.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace quic {
namespace test {
namespace {

class MockQuicCryptoStream : public QuicCryptoStream,
                             public QuicCryptoHandshaker {
 public:
  explicit MockQuicCryptoStream(QuicSession* session)
      : QuicCryptoStream(session),
        QuicCryptoHandshaker(this, session),
        params_(new QuicCryptoNegotiatedParameters) {}
  MockQuicCryptoStream(const MockQuicCryptoStream&) = delete;
  MockQuicCryptoStream& operator=(const MockQuicCryptoStream&) = delete;

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) override {
    messages_.push_back(message);
  }

  std::vector<CryptoHandshakeMessage>* messages() { return &messages_; }

  ssl_early_data_reason_t EarlyDataReason() const override {
    return ssl_early_data_unknown;
  }
  bool encryption_established() const override { return false; }
  bool one_rtt_keys_available() const override { return false; }

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
      const CachedNetworkParameters* /*cached_network_parameters*/)
      const override {
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
  HandshakeState GetHandshakeState() const override { return HANDSHAKE_START; }
  void SetServerApplicationStateForResumption(
      std::unique_ptr<ApplicationState> /*application_state*/) override {}
  std::unique_ptr<QuicDecrypter> AdvanceKeysAndCreateCurrentOneRttDecrypter()
      override {
    return nullptr;
  }
  std::unique_ptr<QuicEncrypter> CreateCurrentOneRttEncrypter() override {
    return nullptr;
  }
  bool ExportKeyingMaterial(absl::string_view /*label*/,
                            absl::string_view /*context*/,
                            size_t /*result_len*/,
                            std::string* /*result*/) override {
    return false;
  }
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
        return QuicCryptoStream::session()
            ->GetEncryptionLevelToSendApplicationData();
      default:
        QUICHE_DCHECK(false);
        return NUM_ENCRYPTION_LEVELS;
    }
  }

 private:
  quiche::QuicheReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
  std::vector<CryptoHandshakeMessage> messages_;
};

class QuicCryptoStreamTest : public QuicTest {
 public:
  QuicCryptoStreamTest()
      : connection_(new MockQuicConnection(&helper_, &alarm_factory_,
                                           Perspective::IS_CLIENT)),
        session_(connection_, /*create_mock_crypto_stream=*/false) {
    EXPECT_CALL(*static_cast<MockPacketWriter*>(connection_->writer()),
                WritePacket(_, _, _, _, _, _))
        .WillRepeatedly(Return(WriteResult(WRITE_STATUS_OK, 0)));
    stream_ = new MockQuicCryptoStream(&session_);
    session_.SetCryptoStream(stream_);
    session_.Initialize();
    message_.set_tag(kSHLO);
    message_.SetStringPiece(1, "abc");
    message_.SetStringPiece(2, "def");
    ConstructHandshakeMessage();
  }
  QuicCryptoStreamTest(const QuicCryptoStreamTest&) = delete;
  QuicCryptoStreamTest& operator=(const QuicCryptoStreamTest&) = delete;

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_ = framer.ConstructHandshakeMessage(message_);
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  MockQuicSpdySession session_;
  MockQuicCryptoStream* stream_;
  CryptoHandshakeMessage message_;
  std::unique_ptr<QuicData> message_data_;
};

TEST_F(QuicCryptoStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_->encryption_established());
  EXPECT_FALSE(stream_->one_rtt_keys_available());
}

TEST_F(QuicCryptoStreamTest, ProcessRawData) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    stream_->OnStreamFrame(QuicStreamFrame(
        QuicUtils::GetCryptoStreamId(connection_->transport_version()),
        /*fin=*/false,
        /*offset=*/0, message_data_->AsStringPiece()));
  } else {
    stream_->OnCryptoFrame(QuicCryptoFrame(ENCRYPTION_INITIAL, /*offset*/ 0,
                                           message_data_->AsStringPiece()));
  }
  ASSERT_EQ(1u, stream_->messages()->size());
  const CryptoHandshakeMessage& message = (*stream_->messages())[0];
  EXPECT_EQ(kSHLO, message.tag());
  EXPECT_EQ(2u, message.tag_value_map().size());
  EXPECT_EQ("abc", crypto_test_utils::GetValueForTag(message, 1));
  EXPECT_EQ("def", crypto_test_utils::GetValueForTag(message, 2));
}

TEST_F(QuicCryptoStreamTest, ProcessBadData) {
  std::string bad(message_data_->data(), message_data_->length());
  const int kFirstTagIndex = sizeof(uint32_t) +  // message tag
                             sizeof(uint16_t) +  // number of tag-value pairs
                             sizeof(uint16_t);   // padding
  EXPECT_EQ(1, bad[kFirstTagIndex]);
  bad[kFirstTagIndex] = 0x7F;  // out of order tag

  EXPECT_CALL(*connection_, CloseConnection(QUIC_CRYPTO_TAGS_OUT_OF_ORDER,
                                            testing::_, testing::_));
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    stream_->OnStreamFrame(QuicStreamFrame(
        QuicUtils::GetCryptoStreamId(connection_->transport_version()),
        /*fin=*/false, /*offset=*/0, bad));
  } else {
    stream_->OnCryptoFrame(
        QuicCryptoFrame(ENCRYPTION_INITIAL, /*offset*/ 0, bad));
  }
}

TEST_F(QuicCryptoStreamTest, NoConnectionLevelFlowControl) {
  EXPECT_FALSE(
      QuicStreamPeer::StreamContributesToConnectionFlowControl(stream_));
}

TEST_F(QuicCryptoStreamTest, RetransmitCryptoData) {
  if (QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 0, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);
  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT.
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 1350, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Lost [0, 1000).
  stream_->OnStreamFrameLost(0, 1000, false);
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  // Lost [1200, 2000).
  stream_->OnStreamFrameLost(1200, 800, false);
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1000, 0, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  // Verify [1200, 2000) are sent in [1200, 1350) and [1350, 2000) because of
  // they are in different encryption levels.
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 150, 1200, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 650, 1350, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());
}

TEST_F(QuicCryptoStreamTest, RetransmitCryptoDataInCryptoFrames) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT.
  std::unique_ptr<NullEncrypter> encrypter =
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT);
  connection_->SetEncrypter(ENCRYPTION_ZERO_RTT, std::move(encrypter));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_ZERO_RTT, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_ZERO_RTT, data);

  // Before encryption moves to ENCRYPTION_FORWARD_SECURE, ZERO RTT data are
  // retranmitted at ENCRYPTION_ZERO_RTT.
  QuicCryptoFrame lost_frame = QuicCryptoFrame(ENCRYPTION_ZERO_RTT, 0, 650);
  stream_->OnCryptoFrameLost(&lost_frame);

  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_ZERO_RTT, 650, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WritePendingCryptoRetransmission();

  connection_->SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Lost [0, 1000).
  lost_frame = QuicCryptoFrame(ENCRYPTION_INITIAL, 0, 1000);
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
  // Lost [1200, 2000).
  lost_frame = QuicCryptoFrame(ENCRYPTION_INITIAL, 1200, 150);
  stream_->OnCryptoFrameLost(&lost_frame);
  lost_frame = QuicCryptoFrame(ENCRYPTION_ZERO_RTT, 0, 650);
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1000, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  // Verify [1200, 2000) are sent in [1200, 1350) and [1350, 2000) because of
  // they are in different encryption levels.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 150, 1200))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_FORWARD_SECURE, 650, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WritePendingCryptoRetransmission();
  EXPECT_FALSE(stream_->HasPendingCryptoRetransmission());
  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());
}

// Regression test for handling the missing ENCRYPTION_HANDSHAKE in
// quic_crypto_stream.cc. This test is essentially the same as
// RetransmitCryptoDataInCryptoFrames, except it uses ENCRYPTION_HANDSHAKE in
// place of ENCRYPTION_ZERO_RTT.
TEST_F(QuicCryptoStreamTest, RetransmitEncryptionHandshakeLevelCryptoFrames) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  InSequence s;
  // Send [0, 1000) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1000, 'a');
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1000, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  // Send [1000, 2000) in ENCRYPTION_HANDSHAKE.
  std::unique_ptr<NullEncrypter> encrypter =
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT);
  connection_->SetEncrypter(ENCRYPTION_HANDSHAKE, std::move(encrypter));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_HANDSHAKE);
  EXPECT_EQ(ENCRYPTION_HANDSHAKE, connection_->encryption_level());
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_HANDSHAKE, 1000, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_HANDSHAKE, data);
  connection_->SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Lost [1000, 1200).
  QuicCryptoFrame lost_frame(ENCRYPTION_HANDSHAKE, 0, 200);
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
  // Verify [1000, 1200) is sent.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_HANDSHAKE, 200, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WritePendingCryptoRetransmission();
  EXPECT_FALSE(stream_->HasPendingCryptoRetransmission());
}

TEST_F(QuicCryptoStreamTest, NeuterUnencryptedStreamData) {
  if (QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 0, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);
  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT.
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 1350, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);

  // Lost [0, 1350).
  stream_->OnStreamFrameLost(0, 1350, false);
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  // Neuters [0, 1350).
  stream_->NeuterUnencryptedStreamData();
  EXPECT_FALSE(stream_->HasPendingRetransmission());
  // Lost [0, 1350) again.
  stream_->OnStreamFrameLost(0, 1350, false);
  EXPECT_FALSE(stream_->HasPendingRetransmission());

  // Lost [1350, 2000).
  stream_->OnStreamFrameLost(1350, 650, false);
  EXPECT_TRUE(stream_->HasPendingRetransmission());
  stream_->NeuterUnencryptedStreamData();
  EXPECT_TRUE(stream_->HasPendingRetransmission());
}

TEST_F(QuicCryptoStreamTest, NeuterUnencryptedCryptoData) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT.
  connection_->SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  std::unique_ptr<NullEncrypter> encrypter =
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT);
  connection_->SetEncrypter(ENCRYPTION_ZERO_RTT, std::move(encrypter));
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_ZERO_RTT, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_ZERO_RTT, data);

  // Lost [0, 1350).
  QuicCryptoFrame lost_frame(ENCRYPTION_INITIAL, 0, 1350);
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
  // Neuters [0, 1350).
  stream_->NeuterUnencryptedStreamData();
  EXPECT_FALSE(stream_->HasPendingCryptoRetransmission());
  // Lost [0, 1350) again.
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_FALSE(stream_->HasPendingCryptoRetransmission());

  // Lost [1350, 2000), which starts at offset 0 at the ENCRYPTION_ZERO_RTT
  // level.
  lost_frame = QuicCryptoFrame(ENCRYPTION_ZERO_RTT, 0, 650);
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
  stream_->NeuterUnencryptedStreamData();
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
}

TEST_F(QuicCryptoStreamTest, RetransmitStreamData) {
  if (QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 0, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);
  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT.
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 1350, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->WriteOrBufferData(data, false, nullptr);
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Ack [2000, 2500).
  QuicByteCount newly_acked_length = 0;
  stream_->OnStreamFrameAcked(2000, 500, false, QuicTime::Delta::Zero(),
                              QuicTime::Zero(), &newly_acked_length,
                              /*is_retransmission=*/false);
  EXPECT_EQ(500u, newly_acked_length);

  // Force crypto stream to send [1350, 2700) and only [1350, 1500) is consumed.
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 650, 1350, _, _, _))
      .WillOnce(InvokeWithoutArgs([this]() {
        return session_.ConsumeData(
            QuicUtils::GetCryptoStreamId(connection_->transport_version()), 150,
            1350, NO_FIN, HANDSHAKE_RETRANSMISSION, std::nullopt);
      }));

  EXPECT_FALSE(stream_->RetransmitStreamData(1350, 1350, false,
                                             HANDSHAKE_RETRANSMISSION));
  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Force session to send [1350, 1500) again and all data is consumed.
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 650, 1350, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 200, 2500, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  EXPECT_TRUE(stream_->RetransmitStreamData(1350, 1350, false,
                                            HANDSHAKE_RETRANSMISSION));
  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(0);
  // Force to send an empty frame.
  EXPECT_TRUE(
      stream_->RetransmitStreamData(0, 0, false, HANDSHAKE_RETRANSMISSION));
}

TEST_F(QuicCryptoStreamTest, RetransmitStreamDataWithCryptoFrames) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT.
  std::unique_ptr<NullEncrypter> encrypter =
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT);
  connection_->SetEncrypter(ENCRYPTION_ZERO_RTT, std::move(encrypter));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_ZERO_RTT, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_ZERO_RTT, data);
  connection_->SetEncrypter(
      ENCRYPTION_FORWARD_SECURE,
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Ack [2000, 2500).
  QuicCryptoFrame acked_frame(ENCRYPTION_ZERO_RTT, 650, 500);
  EXPECT_TRUE(
      stream_->OnCryptoFrameAcked(acked_frame, QuicTime::Delta::Zero()));

  // Retransmit only [1350, 1500).
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_FORWARD_SECURE, 150, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  QuicCryptoFrame frame_to_retransmit(ENCRYPTION_ZERO_RTT, 0, 150);
  stream_->RetransmitData(&frame_to_retransmit, HANDSHAKE_RETRANSMISSION);

  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  // Retransmit [1350, 2700) again and all data is sent.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_FORWARD_SECURE, 650, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  EXPECT_CALL(*connection_,
              SendCryptoData(ENCRYPTION_FORWARD_SECURE, 200, 1150))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  frame_to_retransmit = QuicCryptoFrame(ENCRYPTION_ZERO_RTT, 0, 1350);
  stream_->RetransmitData(&frame_to_retransmit, HANDSHAKE_RETRANSMISSION);
  // Verify connection's encryption level has restored.
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE, connection_->encryption_level());

  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  // Force to send an empty frame.
  QuicCryptoFrame empty_frame(ENCRYPTION_FORWARD_SECURE, 0, 0);
  stream_->RetransmitData(&empty_frame, HANDSHAKE_RETRANSMISSION);
}

// Regression test for b/115926584.
TEST_F(QuicCryptoStreamTest, HasUnackedCryptoData) {
  if (QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  std::string data(1350, 'a');
  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 0, _, _, _))
      .WillOnce(testing::Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(data, false, nullptr);
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  // Although there is no outstanding data, verify session has pending crypto
  // data.
  EXPECT_TRUE(session_.HasUnackedCryptoData());

  EXPECT_CALL(
      session_,
      WritevData(QuicUtils::GetCryptoStreamId(connection_->transport_version()),
                 1350, 0, _, _, _))
      .WillOnce(Invoke(&session_, &MockQuicSpdySession::ConsumeData));
  stream_->OnCanWrite();
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_.HasUnackedCryptoData());
}

TEST_F(QuicCryptoStreamTest, HasUnackedCryptoDataWithCryptoFrames) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_TRUE(session_.HasUnackedCryptoData());
}

// Regression test for bugfix of GetPacketHeaderSize.
TEST_F(QuicCryptoStreamTest, CryptoMessageFramingOverhead) {
  for (const ParsedQuicVersion& version :
       AllSupportedVersionsWithQuicCrypto()) {
    SCOPED_TRACE(version);
    QuicByteCount expected_overhead = 52;
    if (version.HasLongHeaderLengths()) {
      expected_overhead += 3;
    }
    if (version.HasLengthPrefixedConnectionIds()) {
      expected_overhead += 1;
    }
    EXPECT_EQ(expected_overhead,
              QuicCryptoStream::CryptoMessageFramingOverhead(
                  version.transport_version, TestConnectionId()));
  }
}

TEST_F(QuicCryptoStreamTest, WriteCryptoDataExceedsSendBufferLimit) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  int32_t buffer_limit = GetQuicFlag(quic_max_buffered_crypto_bytes);

  // Write data larger than the buffer limit, when there is no existing data in
  // the buffer. Data is sent rather than closing the connection.
  EXPECT_FALSE(stream_->HasBufferedCryptoFrames());
  int32_t over_limit = buffer_limit + 1;
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, over_limit, 0))
      // All the data is sent, no resulting buffer.
      .WillOnce(Return(over_limit));
  std::string large_data(over_limit, 'a');
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, large_data);

  // Write data to the buffer up to the limit. One byte gets sent.
  EXPECT_FALSE(stream_->HasBufferedCryptoFrames());
  EXPECT_CALL(*connection_,
              SendCryptoData(ENCRYPTION_INITIAL, buffer_limit, over_limit))
      .WillOnce(Return(1));
  std::string data(buffer_limit, 'a');
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  EXPECT_TRUE(stream_->HasBufferedCryptoFrames());

  // Write another byte that is not sent (due to there already being data in the
  // buffer); send buffer is now full.
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  std::string data2(1, 'a');
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data2);
  EXPECT_TRUE(stream_->HasBufferedCryptoFrames());

  // Writing an additional byte to the send buffer closes the connection.
  if (GetQuicFlag(quic_bounded_crypto_send_buffer)) {
    EXPECT_CALL(*connection_, CloseConnection(QUIC_INTERNAL_ERROR, _, _));
    EXPECT_QUIC_BUG(
        stream_->WriteCryptoData(ENCRYPTION_INITIAL, data2),
        "Too much data for crypto send buffer with level: ENCRYPTION_INITIAL, "
        "current_buffer_size: 16384, data length: 1");
  }
}

TEST_F(QuicCryptoStreamTest, WriteBufferedCryptoFrames) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  EXPECT_FALSE(stream_->HasBufferedCryptoFrames());
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  // Only consumed 1000 bytes.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1350, 0))
      .WillOnce(Return(1000));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);
  EXPECT_TRUE(stream_->HasBufferedCryptoFrames());

  // Send [1350, 2700) in ENCRYPTION_ZERO_RTT and verify no write is attempted
  // because there is buffered data.
  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  connection_->SetEncrypter(
      ENCRYPTION_ZERO_RTT,
      std::make_unique<NullEncrypter>(Perspective::IS_CLIENT));
  connection_->SetDefaultEncryptionLevel(ENCRYPTION_ZERO_RTT);
  stream_->WriteCryptoData(ENCRYPTION_ZERO_RTT, data);
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());

  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 350, 1000))
      .WillOnce(Return(350));
  // Partial write of ENCRYPTION_ZERO_RTT data.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_ZERO_RTT, 1350, 0))
      .WillOnce(Return(1000));
  stream_->WriteBufferedCryptoFrames();
  EXPECT_TRUE(stream_->HasBufferedCryptoFrames());
  EXPECT_EQ(ENCRYPTION_ZERO_RTT, connection_->encryption_level());

  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_ZERO_RTT, 350, 1000))
      .WillOnce(Return(350));
  stream_->WriteBufferedCryptoFrames();
  EXPECT_FALSE(stream_->HasBufferedCryptoFrames());
}

TEST_F(QuicCryptoStreamTest, LimitBufferedCryptoData) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  std::string large_frame(2 * GetQuicFlag(quic_max_buffered_crypto_bytes), 'a');

  // Set offset to 1 so that we guarantee the data gets buffered instead of
  // immediately processed.
  QuicStreamOffset offset = 1;
  stream_->OnCryptoFrame(
      QuicCryptoFrame(ENCRYPTION_INITIAL, offset, large_frame));
}

TEST_F(QuicCryptoStreamTest, CloseConnectionWithZeroRttCryptoFrame) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }

  EXPECT_CALL(*connection_,
              CloseConnection(IETF_QUIC_PROTOCOL_VIOLATION, _, _));

  test::QuicConnectionPeer::SetLastDecryptedLevel(connection_,
                                                  ENCRYPTION_ZERO_RTT);
  QuicStreamOffset offset = 1;
  stream_->OnCryptoFrame(QuicCryptoFrame(ENCRYPTION_ZERO_RTT, offset, "data"));
}

TEST_F(QuicCryptoStreamTest, RetransmitCryptoFramesAndPartialWrite) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }

  EXPECT_CALL(*connection_, SendCryptoData(_, _, _)).Times(0);
  InSequence s;
  // Send [0, 1350) in ENCRYPTION_INITIAL.
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
  std::string data(1350, 'a');
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1350, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WriteCryptoData(ENCRYPTION_INITIAL, data);

  // Lost [0, 1000).
  QuicCryptoFrame lost_frame(ENCRYPTION_INITIAL, 0, 1000);
  stream_->OnCryptoFrameLost(&lost_frame);
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
  // Simulate connection is constrained by amplification restriction.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1000, 0))
      .WillOnce(Return(0));
  stream_->WritePendingCryptoRetransmission();
  EXPECT_TRUE(stream_->HasPendingCryptoRetransmission());
  // Connection gets unblocked.
  EXPECT_CALL(*connection_, SendCryptoData(ENCRYPTION_INITIAL, 1000, 0))
      .WillOnce(Invoke(connection_,
                       &MockQuicConnection::QuicConnection_SendCryptoData));
  stream_->WritePendingCryptoRetransmission();
  EXPECT_FALSE(stream_->HasPendingCryptoRetransmission());
}

// Regression test for b/203199510
TEST_F(QuicCryptoStreamTest, EmptyCryptoFrame) {
  if (!QuicVersionUsesCryptoFrames(connection_->transport_version())) {
    return;
  }
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  QuicCryptoFrame empty_crypto_frame(ENCRYPTION_INITIAL, 0, nullptr, 0);
  stream_->OnCryptoFrame(empty_crypto_frame);
}

}  // namespace
}  // namespace test
}  // namespace quic
