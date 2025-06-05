// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_crypto_client_stream.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "quiche/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "quiche/quic/core/crypto/quic_decrypter.h"
#include "quiche/quic/core/crypto/quic_encrypter.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_server_id.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_stream_sequencer_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/simple_quic_framer.h"
#include "quiche/quic/test_tools/simple_session_cache.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using testing::_;

namespace quic {
namespace test {
namespace {

const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;

// This test tests the client-side of the QUIC crypto handshake. It does not
// test the TLS handshake - that is in tls_client_handshaker_test.cc.
class QuicCryptoClientStreamTest : public QuicTest {
 public:
  QuicCryptoClientStreamTest()
      : supported_versions_(AllSupportedVersionsWithQuicCrypto()),
        server_id_(kServerHostname, kServerPort),
        crypto_config_(crypto_test_utils::ProofVerifierForTesting(),
                       std::make_unique<test::SimpleSessionCache>()),
        server_crypto_config_(
            crypto_test_utils::CryptoServerConfigForTesting()) {
    CreateConnection();
  }

  void CreateSession() {
    session_ = std::make_unique<TestQuicSpdyClientSession>(
        connection_, DefaultQuicConfig(), supported_versions_, server_id_,
        &crypto_config_);
    EXPECT_CALL(*session_, GetAlpnsToOffer())
        .WillRepeatedly(testing::Return(std::vector<std::string>(
            {AlpnForVersion(connection_->version())})));
  }

  void CreateConnection() {
    connection_ =
        new PacketSavingConnection(&client_helper_, &alarm_factory_,
                                   Perspective::IS_CLIENT, supported_versions_);
    // Advance the time, because timers do not like uninitialized times.
    connection_->AdvanceTime(QuicTime::Delta::FromSeconds(1));
    CreateSession();
  }

  void CompleteCryptoHandshake() {
    int proof_verify_details_calls = 1;
    if (stream()->handshake_protocol() != PROTOCOL_TLS1_3) {
      EXPECT_CALL(*session_, OnProofValid(testing::_))
          .Times(testing::AtLeast(1));
      proof_verify_details_calls = 0;
    }
    EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_))
        .Times(testing::AtLeast(proof_verify_details_calls));
    stream()->CryptoConnect();
    QuicConfig config;
    crypto_test_utils::HandshakeWithFakeServer(
        &config, server_crypto_config_.get(), &server_helper_, &alarm_factory_,
        connection_, stream(), AlpnForVersion(connection_->version()));
  }

  QuicCryptoClientStream* stream() {
    return session_->GetMutableCryptoStream();
  }

  MockQuicConnectionHelper server_helper_;
  MockQuicConnectionHelper client_helper_;
  MockAlarmFactory alarm_factory_;
  PacketSavingConnection* connection_;
  ParsedQuicVersionVector supported_versions_;
  std::unique_ptr<TestQuicSpdyClientSession> session_;
  QuicServerId server_id_;
  CryptoHandshakeMessage message_;
  QuicCryptoClientConfig crypto_config_;
  std::unique_ptr<QuicCryptoServerConfig> server_crypto_config_;
};

TEST_F(QuicCryptoClientStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream()->encryption_established());
  EXPECT_FALSE(stream()->one_rtt_keys_available());
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterSHLO) {
  CompleteCryptoHandshake();
  EXPECT_TRUE(stream()->encryption_established());
  EXPECT_TRUE(stream()->one_rtt_keys_available());
  EXPECT_FALSE(stream()->IsResumption());
  EXPECT_EQ(stream()->EarlyDataReason(), ssl_early_data_no_session_offered);
}

TEST_F(QuicCryptoClientStreamTest, MessageAfterHandshake) {
  CompleteCryptoHandshake();

  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE, _, _));
  message_.set_tag(kCHLO);
  crypto_test_utils::SendHandshakeMessageToStream(stream(), message_,
                                                  Perspective::IS_CLIENT);
}

TEST_F(QuicCryptoClientStreamTest, BadMessageType) {
  stream()->CryptoConnect();

  message_.set_tag(kCHLO);

  EXPECT_CALL(*connection_, CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE,
                                            "Expected REJ", _));
  crypto_test_utils::SendHandshakeMessageToStream(stream(), message_,
                                                  Perspective::IS_CLIENT);
}

TEST_F(QuicCryptoClientStreamTest, NegotiatedParameters) {
  CompleteCryptoHandshake();

  const QuicConfig* config = session_->config();
  EXPECT_EQ(kMaximumIdleTimeoutSecs, config->IdleNetworkTimeout().ToSeconds());

  const QuicCryptoNegotiatedParameters& crypto_params(
      stream()->crypto_negotiated_params());
  EXPECT_EQ(crypto_config_.aead[0], crypto_params.aead);
  EXPECT_EQ(crypto_config_.kexs[0], crypto_params.key_exchange);
}

TEST_F(QuicCryptoClientStreamTest, ExpiredServerConfig) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  // Recreate connection with the new config.
  CreateConnection();

  // Advance time 5 years to ensure that we pass the expiry time of the cached
  // server config.
  connection_->AdvanceTime(
      QuicTime::Delta::FromSeconds(60 * 60 * 24 * 365 * 5));

  EXPECT_CALL(*session_, OnProofValid(testing::_));
  stream()->CryptoConnect();
  // Check that a client hello was sent.
  ASSERT_EQ(1u, connection_->encrypted_packets_.size());
  EXPECT_EQ(ENCRYPTION_INITIAL, connection_->encryption_level());
}

TEST_F(QuicCryptoClientStreamTest, ClientTurnedOffZeroRtt) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  // Recreate connection with the new config.
  CreateConnection();

  // Set connection option.
  QuicTagVector options;
  options.push_back(kQNZ2);
  session_->config()->SetClientConnectionOptions(options);

  CompleteCryptoHandshake();
  // Check that two client hellos were sent, one inchoate and one normal.
  EXPECT_EQ(2, stream()->num_sent_client_hellos());
  EXPECT_FALSE(stream()->EarlyDataAccepted());
  EXPECT_EQ(stream()->EarlyDataReason(), ssl_early_data_disabled);
}

TEST_F(QuicCryptoClientStreamTest, ClockSkew) {
  // Test that if the client's clock is skewed with respect to the server,
  // the handshake succeeds. In the past, the client would get the server
  // config, notice that it had already expired and then close the connection.

  // Advance time 5 years to ensure that we pass the expiry time in the server
  // config, but the TTL is used instead.
  connection_->AdvanceTime(
      QuicTime::Delta::FromSeconds(60 * 60 * 24 * 365 * 5));

  // The handshakes completes!
  CompleteCryptoHandshake();
}

TEST_F(QuicCryptoClientStreamTest, InvalidCachedServerConfig) {
  // Seed the config with a cached server config.
  CompleteCryptoHandshake();

  // Recreate connection with the new config.
  CreateConnection();

  QuicCryptoClientConfig::CachedState* state =
      crypto_config_.LookupOrCreate(server_id_);

  std::vector<std::string> certs = state->certs();
  std::string cert_sct = state->cert_sct();
  std::string signature = state->signature();
  std::string chlo_hash = state->chlo_hash();
  state->SetProof(certs, cert_sct, chlo_hash, signature + signature);

  EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_))
      .Times(testing::AnyNumber());
  stream()->CryptoConnect();
  // Check that a client hello was sent.
  ASSERT_EQ(1u, connection_->encrypted_packets_.size());
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdate) {
  // Test that the crypto client stream can receive server config updates after
  // the connection has been established.
  CompleteCryptoHandshake();

  QuicCryptoClientConfig::CachedState* state =
      crypto_config_.LookupOrCreate(server_id_);

  // Ensure cached STK is different to what we send in the handshake.
  EXPECT_NE("xstk", state->source_address_token());

  // Initialize using {...} syntax to avoid trailing \0 if converting from
  // string.
  unsigned char stk[] = {'x', 's', 't', 'k'};

  // Minimum SCFG that passes config validation checks.
  unsigned char scfg[] = {// SCFG
                          0x53, 0x43, 0x46, 0x47,
                          // num entries
                          0x01, 0x00,
                          // padding
                          0x00, 0x00,
                          // EXPY
                          0x45, 0x58, 0x50, 0x59,
                          // EXPY end offset
                          0x08, 0x00, 0x00, 0x00,
                          // Value
                          '1', '2', '3', '4', '5', '6', '7', '8'};

  CryptoHandshakeMessage server_config_update;
  server_config_update.set_tag(kSCUP);
  server_config_update.SetValue(kSourceAddressTokenTag, stk);
  server_config_update.SetValue(kSCFG, scfg);
  const uint64_t expiry_seconds = 60 * 60 * 24 * 2;
  server_config_update.SetValue(kSTTL, expiry_seconds);

  crypto_test_utils::SendHandshakeMessageToStream(
      stream(), server_config_update, Perspective::IS_SERVER);

  // Make sure that the STK and SCFG are cached correctly.
  EXPECT_EQ("xstk", state->source_address_token());

  const std::string& cached_scfg = state->server_config();
  quiche::test::CompareCharArraysWithHexError(
      "scfg", cached_scfg.data(), cached_scfg.length(),
      reinterpret_cast<char*>(scfg), ABSL_ARRAYSIZE(scfg));

  QuicStreamSequencer* sequencer = QuicStreamPeer::sequencer(stream());
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdateWithCert) {
  // Test that the crypto client stream can receive and use server config
  // updates with certificates after the connection has been established.
  CompleteCryptoHandshake();

  // Build a server config update message with certificates
  QuicCryptoServerConfig crypto_config(
      QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
      crypto_test_utils::ProofSourceForTesting(), KeyExchangeSource::Default());
  crypto_test_utils::SetupCryptoServerConfigForTest(
      connection_->clock(), QuicRandom::GetInstance(), &crypto_config);
  SourceAddressTokens tokens;
  QuicCompressedCertsCache cache(1);
  CachedNetworkParameters network_params;
  CryptoHandshakeMessage server_config_update;

  class Callback : public BuildServerConfigUpdateMessageResultCallback {
   public:
    Callback(bool* ok, CryptoHandshakeMessage* message)
        : ok_(ok), message_(message) {}
    void Run(bool ok, const CryptoHandshakeMessage& message) override {
      *ok_ = ok;
      *message_ = message;
    }

   private:
    bool* ok_;
    CryptoHandshakeMessage* message_;
  };

  // Note: relies on the callback being invoked synchronously
  bool ok = false;
  crypto_config.BuildServerConfigUpdateMessage(
      session_->transport_version(), stream()->chlo_hash(), tokens,
      QuicSocketAddress(QuicIpAddress::Loopback6(), 1234),
      QuicSocketAddress(QuicIpAddress::Loopback6(), 4321), connection_->clock(),
      QuicRandom::GetInstance(), &cache, stream()->crypto_negotiated_params(),
      &network_params,
      std::unique_ptr<BuildServerConfigUpdateMessageResultCallback>(
          new Callback(&ok, &server_config_update)));
  EXPECT_TRUE(ok);

  EXPECT_CALL(*session_, OnProofValid(testing::_));
  crypto_test_utils::SendHandshakeMessageToStream(
      stream(), server_config_update, Perspective::IS_SERVER);

  // Recreate connection with the new config and verify a 0-RTT attempt.
  CreateConnection();

  EXPECT_CALL(*session_, OnProofValid(testing::_));
  EXPECT_CALL(*session_, OnProofVerifyDetailsAvailable(testing::_))
      .Times(testing::AnyNumber());
  stream()->CryptoConnect();
  EXPECT_TRUE(session_->IsEncryptionEstablished());
}

TEST_F(QuicCryptoClientStreamTest, ServerConfigUpdateBeforeHandshake) {
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_CRYPTO_UPDATE_BEFORE_HANDSHAKE_COMPLETE, _, _));
  CryptoHandshakeMessage server_config_update;
  server_config_update.set_tag(kSCUP);
  crypto_test_utils::SendHandshakeMessageToStream(
      stream(), server_config_update, Perspective::IS_SERVER);
}

}  // namespace
}  // namespace test
}  // namespace quic
