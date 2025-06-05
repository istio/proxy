// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "quiche/quic/core/crypto/aes_128_gcm_12_encrypter.h"
#include "quiche/quic/core/crypto/crypto_framer.h"
#include "quiche/quic/core/crypto/crypto_handshake.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/crypto/crypto_utils.h"
#include "quiche/quic/core/crypto/quic_crypto_server_config.h"
#include "quiche/quic/core/crypto/quic_decrypter.h"
#include "quiche/quic/core/crypto/quic_encrypter.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/quic_crypto_client_stream.h"
#include "quiche/quic/core/quic_crypto_server_stream_base.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_session.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/failing_proof_source.h"
#include "quiche/quic/test_tools/fake_proof_source.h"
#include "quiche/quic/test_tools/quic_crypto_server_config_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {
class QuicConnection;
class QuicStream;
}  // namespace quic

using testing::_;
using testing::NiceMock;

namespace quic {
namespace test {

namespace {

const char kServerHostname[] = "test.example.com";
const uint16_t kServerPort = 443;

// This test tests the server-side of the QUIC crypto handshake. It does not
// test the TLS handshake - that is in tls_server_handshaker_test.cc.
class QuicCryptoServerStreamTest : public QuicTest {
 public:
  QuicCryptoServerStreamTest()
      : QuicCryptoServerStreamTest(crypto_test_utils::ProofSourceForTesting()) {
  }

  explicit QuicCryptoServerStreamTest(std::unique_ptr<ProofSource> proof_source)
      : server_crypto_config_(
            QuicCryptoServerConfig::TESTING, QuicRandom::GetInstance(),
            std::move(proof_source), KeyExchangeSource::Default()),
        server_compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize),
        server_id_(kServerHostname, kServerPort),
        client_crypto_config_(crypto_test_utils::ProofVerifierForTesting()) {}

  void Initialize() { InitializeServer(); }

  ~QuicCryptoServerStreamTest() override {
    // Ensure that anything that might reference |helpers_| is destroyed before
    // |helpers_| is destroyed.
    server_session_.reset();
    client_session_.reset();
    helpers_.clear();
    alarm_factories_.clear();
  }

  // Initializes the crypto server stream state for testing.  May be
  // called multiple times.
  void InitializeServer() {
    TestQuicSpdyServerSession* server_session = nullptr;
    helpers_.push_back(std::make_unique<NiceMock<MockQuicConnectionHelper>>());
    alarm_factories_.push_back(std::make_unique<MockAlarmFactory>());
    CreateServerSessionForTest(
        server_id_, QuicTime::Delta::FromSeconds(100000), supported_versions_,
        helpers_.back().get(), alarm_factories_.back().get(),
        &server_crypto_config_, &server_compressed_certs_cache_,
        &server_connection_, &server_session);
    QUICHE_CHECK(server_session);
    server_session_.reset(server_session);
    EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*server_session_, SelectAlpn(_))
        .WillRepeatedly([this](const std::vector<absl::string_view>& alpns) {
          return std::find(
              alpns.cbegin(), alpns.cend(),
              AlpnForVersion(server_session_->connection()->version()));
        });
    crypto_test_utils::SetupCryptoServerConfigForTest(
        server_connection_->clock(), server_connection_->random_generator(),
        &server_crypto_config_);
  }

  QuicCryptoServerStreamBase* server_stream() {
    return server_session_->GetMutableCryptoStream();
  }

  QuicCryptoClientStream* client_stream() {
    return client_session_->GetMutableCryptoStream();
  }

  // Initializes a fake client, and all its associated state, for
  // testing.  May be called multiple times.
  void InitializeFakeClient() {
    TestQuicSpdyClientSession* client_session = nullptr;
    helpers_.push_back(std::make_unique<NiceMock<MockQuicConnectionHelper>>());
    alarm_factories_.push_back(std::make_unique<MockAlarmFactory>());
    CreateClientSessionForTest(
        server_id_, QuicTime::Delta::FromSeconds(100000), supported_versions_,
        helpers_.back().get(), alarm_factories_.back().get(),
        &client_crypto_config_, &client_connection_, &client_session);
    QUICHE_CHECK(client_session);
    client_session_.reset(client_session);
  }

  int CompleteCryptoHandshake() {
    QUICHE_CHECK(server_connection_);
    QUICHE_CHECK(server_session_ != nullptr);

    return crypto_test_utils::HandshakeWithFakeClient(
        helpers_.back().get(), alarm_factories_.back().get(),
        server_connection_, server_stream(), server_id_, client_options_,
        /*alpn=*/"");
  }

  // Performs a single round of handshake message-exchange between the
  // client and server.
  void AdvanceHandshakeWithFakeClient() {
    QUICHE_CHECK(server_connection_);
    QUICHE_CHECK(client_session_ != nullptr);

    EXPECT_CALL(*client_session_, OnProofValid(_)).Times(testing::AnyNumber());
    EXPECT_CALL(*client_session_, OnProofVerifyDetailsAvailable(_))
        .Times(testing::AnyNumber());
    EXPECT_CALL(*client_connection_, OnCanWrite()).Times(testing::AnyNumber());
    EXPECT_CALL(*server_connection_, OnCanWrite()).Times(testing::AnyNumber());
    client_stream()->CryptoConnect();
    crypto_test_utils::AdvanceHandshake(client_connection_, client_stream(), 0,
                                        server_connection_, server_stream(), 0);
  }

 protected:
  // Every connection gets its own MockQuicConnectionHelper and
  // MockAlarmFactory, tracked separately from the server and client state so
  // their lifetimes persist through the whole test.
  std::vector<std::unique_ptr<MockQuicConnectionHelper>> helpers_;
  std::vector<std::unique_ptr<MockAlarmFactory>> alarm_factories_;

  // Server state.
  PacketSavingConnection* server_connection_;
  std::unique_ptr<TestQuicSpdyServerSession> server_session_;
  QuicCryptoServerConfig server_crypto_config_;
  QuicCompressedCertsCache server_compressed_certs_cache_;
  QuicServerId server_id_;

  // Client state.
  PacketSavingConnection* client_connection_;
  QuicCryptoClientConfig client_crypto_config_;
  std::unique_ptr<TestQuicSpdyClientSession> client_session_;

  CryptoHandshakeMessage message_;
  crypto_test_utils::FakeClientOptions client_options_;

  // Which QUIC versions the client and server support.
  ParsedQuicVersionVector supported_versions_ =
      AllSupportedVersionsWithQuicCrypto();
};

TEST_F(QuicCryptoServerStreamTest, NotInitiallyConected) {
  Initialize();
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
}

TEST_F(QuicCryptoServerStreamTest, ConnectedAfterCHLO) {
  // CompleteCryptoHandshake returns the number of client hellos sent. This
  // test should send:
  //   * One to get a source-address token and certificates.
  //   * One to complete the handshake.
  Initialize();
  EXPECT_EQ(2, CompleteCryptoHandshake());
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->one_rtt_keys_available());
}

TEST_F(QuicCryptoServerStreamTest, ForwardSecureAfterCHLO) {
  Initialize();
  InitializeFakeClient();

  // Do a first handshake in order to prime the client config with the server's
  // information.
  AdvanceHandshakeWithFakeClient();
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());

  // Now do another handshake, with the blocking SHLO connection option.
  InitializeServer();
  InitializeFakeClient();

  AdvanceHandshakeWithFakeClient();
  if (GetQuicReloadableFlag(quic_require_handshake_confirmation)) {
    crypto_test_utils::AdvanceHandshake(client_connection_, client_stream(), 0,
                                        server_connection_, server_stream(), 0);
  }
  EXPECT_TRUE(server_stream()->encryption_established());
  EXPECT_TRUE(server_stream()->one_rtt_keys_available());
  EXPECT_EQ(ENCRYPTION_FORWARD_SECURE,
            server_session_->connection()->encryption_level());
}

TEST_F(QuicCryptoServerStreamTest, ZeroRTT) {
  Initialize();
  InitializeFakeClient();

  // Do a first handshake in order to prime the client config with the server's
  // information.
  AdvanceHandshakeWithFakeClient();
  EXPECT_FALSE(server_stream()->ResumptionAttempted());

  // Now do another handshake, hopefully in 0-RTT.
  QUIC_LOG(INFO) << "Resetting for 0-RTT handshake attempt";
  InitializeFakeClient();
  InitializeServer();

  EXPECT_CALL(*client_session_, OnProofValid(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*client_session_, OnProofVerifyDetailsAvailable(_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*client_connection_, OnCanWrite()).Times(testing::AnyNumber());
  client_stream()->CryptoConnect();

  EXPECT_CALL(*client_session_, OnProofValid(_)).Times(testing::AnyNumber());
  EXPECT_CALL(*client_session_, OnProofVerifyDetailsAvailable(_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*client_connection_, OnCanWrite()).Times(testing::AnyNumber());
  crypto_test_utils::CommunicateHandshakeMessages(
      client_connection_, client_stream(), server_connection_, server_stream());

  EXPECT_EQ(
      (GetQuicReloadableFlag(quic_require_handshake_confirmation) ? 2 : 1),
      client_stream()->num_sent_client_hellos());
  EXPECT_TRUE(server_stream()->ResumptionAttempted());
}

TEST_F(QuicCryptoServerStreamTest, FailByPolicy) {
  Initialize();
  InitializeFakeClient();

  EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_HANDSHAKE_FAILED, _, _));

  AdvanceHandshakeWithFakeClient();
}

TEST_F(QuicCryptoServerStreamTest, MessageAfterHandshake) {
  Initialize();
  CompleteCryptoHandshake();
  EXPECT_CALL(
      *server_connection_,
      CloseConnection(QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE, _, _));
  message_.set_tag(kCHLO);
  crypto_test_utils::SendHandshakeMessageToStream(server_stream(), message_,
                                                  Perspective::IS_CLIENT);
}

TEST_F(QuicCryptoServerStreamTest, BadMessageType) {
  Initialize();

  message_.set_tag(kSHLO);
  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_INVALID_CRYPTO_MESSAGE_TYPE, _, _));
  crypto_test_utils::SendHandshakeMessageToStream(server_stream(), message_,
                                                  Perspective::IS_SERVER);
}

TEST_F(QuicCryptoServerStreamTest, OnlySendSCUPAfterHandshakeComplete) {
  // An attempt to send a SCUP before completing handshake should fail.
  Initialize();

  server_stream()->SendServerConfigUpdate(nullptr);
  EXPECT_EQ(0, server_stream()->NumServerConfigUpdateMessagesSent());
}

TEST_F(QuicCryptoServerStreamTest, SendSCUPAfterHandshakeComplete) {
  Initialize();

  InitializeFakeClient();

  // Do a first handshake in order to prime the client config with the server's
  // information.
  AdvanceHandshakeWithFakeClient();

  // Now do another handshake, with the blocking SHLO connection option.
  InitializeServer();
  InitializeFakeClient();
  AdvanceHandshakeWithFakeClient();
  if (GetQuicReloadableFlag(quic_require_handshake_confirmation)) {
    crypto_test_utils::AdvanceHandshake(client_connection_, client_stream(), 0,
                                        server_connection_, server_stream(), 0);
  }

  // Send a SCUP message and ensure that the client was able to verify it.
  EXPECT_CALL(*client_connection_, CloseConnection(_, _, _)).Times(0);
  server_stream()->SendServerConfigUpdate(nullptr);
  crypto_test_utils::AdvanceHandshake(client_connection_, client_stream(), 1,
                                      server_connection_, server_stream(), 1);

  EXPECT_EQ(1, server_stream()->NumServerConfigUpdateMessagesSent());
  EXPECT_EQ(1, client_stream()->num_scup_messages_received());
}

class QuicCryptoServerStreamTestWithFailingProofSource
    : public QuicCryptoServerStreamTest {
 public:
  QuicCryptoServerStreamTestWithFailingProofSource()
      : QuicCryptoServerStreamTest(
            std::unique_ptr<FailingProofSource>(new FailingProofSource)) {}
};

TEST_F(QuicCryptoServerStreamTestWithFailingProofSource, Test) {
  Initialize();
  InitializeFakeClient();

  EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*server_connection_,
              CloseConnection(QUIC_HANDSHAKE_FAILED, "Failed to get proof", _));
  // Regression test for b/31521252, in which a crash would happen here.
  AdvanceHandshakeWithFakeClient();
  EXPECT_FALSE(server_stream()->encryption_established());
  EXPECT_FALSE(server_stream()->one_rtt_keys_available());
}

class QuicCryptoServerStreamTestWithFakeProofSource
    : public QuicCryptoServerStreamTest {
 public:
  QuicCryptoServerStreamTestWithFakeProofSource()
      : QuicCryptoServerStreamTest(
            std::unique_ptr<FakeProofSource>(new FakeProofSource)),
        crypto_config_peer_(&server_crypto_config_) {}

  FakeProofSource* GetFakeProofSource() const {
    return static_cast<FakeProofSource*>(crypto_config_peer_.GetProofSource());
  }

 protected:
  QuicCryptoServerConfigPeer crypto_config_peer_;
};

// Regression test for b/35422225, in which multiple CHLOs arriving on the same
// connection in close succession could cause a crash.
TEST_F(QuicCryptoServerStreamTestWithFakeProofSource, MultipleChlo) {
  Initialize();
  GetFakeProofSource()->Activate();
  EXPECT_CALL(*server_session_->helper(), CanAcceptClientHello(_, _, _, _, _))
      .WillOnce(testing::Return(true));

  // The methods below use a PROTOCOL_QUIC_CRYPTO version so we pick the
  // first one from the list of supported versions.
  QuicTransportVersion transport_version = QUIC_VERSION_UNSUPPORTED;
  for (const ParsedQuicVersion& version : AllSupportedVersions()) {
    if (version.handshake_protocol == PROTOCOL_QUIC_CRYPTO) {
      transport_version = version.transport_version;
      break;
    }
  }
  ASSERT_NE(QUIC_VERSION_UNSUPPORTED, transport_version);

  // Create a minimal CHLO
  MockClock clock;
  CryptoHandshakeMessage chlo = crypto_test_utils::GenerateDefaultInchoateCHLO(
      &clock, transport_version, &server_crypto_config_);

  // Send in the CHLO, and check that a callback is now pending in the
  // ProofSource.
  crypto_test_utils::SendHandshakeMessageToStream(server_stream(), chlo,
                                                  Perspective::IS_CLIENT);
  EXPECT_EQ(GetFakeProofSource()->NumPendingCallbacks(), 1);

  // Send in a second CHLO while processing of the first is still pending.
  // Verify that the server closes the connection rather than crashing.  Note
  // that the crash is a use-after-free, so it may only show up consistently in
  // ASAN tests.
  EXPECT_CALL(
      *server_connection_,
      CloseConnection(QUIC_CRYPTO_MESSAGE_WHILE_VALIDATING_CLIENT_HELLO,
                      "Unexpected handshake message while processing CHLO", _));
  crypto_test_utils::SendHandshakeMessageToStream(server_stream(), chlo,
                                                  Perspective::IS_CLIENT);
}

}  // namespace
}  // namespace test
}  // namespace quic
