// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/http/quic_connection_migration_manager.h"

#include <memory>

#include "quiche/quic/core/crypto/null_encrypter.h"
#include "quiche/quic/core/http/quic_spdy_client_session_with_migration.h"
#include "quiche/quic/core/io/socket.h"
#include "quiche/quic/core/quic_force_blockable_packet_writer.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_path_validator_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace quic::test {

MATCHER_P(IsFrame, type, "") { return arg.type == type; }

class QuicConnectionMigrationManagerPeer {
 public:
  static QuicAlarm* GetWaitForMigrationAlarm(
      QuicConnectionMigrationManager* manager) {
    return manager->wait_for_migration_alarm_.get();
  }

  static QuicAlarm* GetRunPendingCallbacksAlarm(
      QuicConnectionMigrationManager* manager) {
    return manager->run_pending_callbacks_alarm_.get();
  }

  static QuicAlarm* GetMigrateBackToDefaultTimer(
      QuicConnectionMigrationManager* manager) {
    return manager->migrate_back_to_default_timer_.get();
  }
};

class TestQuicClientPathValidationContext
    : public QuicClientPathValidationContext {
 public:
  TestQuicClientPathValidationContext(
      const quic::QuicSocketAddress& self_address,
      const quic::QuicSocketAddress& peer_address, QuicNetworkHandle network)
      : QuicClientPathValidationContext(self_address, peer_address, network),
        writer_(std::make_unique<QuicForceBlockablePacketWriter>()) {
    auto* writer = new NiceMock<MockPacketWriter>();
    // Owns writer.
    writer_->set_writer(writer);
    ON_CALL(*writer, WritePacket(_, _, _, _, _, _))
        .WillByDefault(Return(WriteResult(WRITE_STATUS_OK, 0)));
    ON_CALL(*writer, GetMaxPacketSize(_))
        .WillByDefault(Return(kMaxOutgoingPacketSize));
    ON_CALL(*writer, IsBatchMode()).WillByDefault(Return(false));
    ON_CALL(*writer, GetNextWriteLocation(_, _))
        .WillByDefault(Return(QuicPacketBuffer()));
    ON_CALL(*writer, Flush())
        .WillByDefault(Return(WriteResult(WRITE_STATUS_OK, 0)));
    ON_CALL(*writer, SupportsReleaseTime()).WillByDefault(Return(false));
    ON_CALL(*writer, MessageTooBigErrorCode())
        .WillByDefault(Return(kSocketErrorMsgSize));
  }

  QuicForceBlockablePacketWriter* ForceBlockableWriterToUse() override {
    return writer_.get();
  }

  bool ShouldConnectionOwnWriter() const override { return true; }

  void ReleasePacketWriter() { writer_.release(); }

 private:
  std::unique_ptr<QuicForceBlockablePacketWriter> writer_;
};

class TestQuicPathContextFactory : public QuicPathContextFactory {
 public:
  TestQuicPathContextFactory(bool async_creation, bool has_error)
      : async_creation_(async_creation), has_error_(has_error) {}

  void CreatePathValidationContext(
      QuicNetworkHandle network, QuicSocketAddress peer_address,
      std::unique_ptr<CreationResultDelegate> result_delegate) override {
    pending_result_delegate_ = std::move(result_delegate);
    network_ = network;
    peer_address_ = peer_address;
    if (!async_creation_) {
      FinishPendingCreation();
    }
    ++num_creation_attempts_;
  }

  void FinishPendingCreation() {
    ASSERT_NE(pending_result_delegate_, nullptr)
        << "No pending path context creation";
    if (has_error_) {
      pending_result_delegate_->OnCreationFailed(
          network_, "path context creation failure.");
    } else {
      QUICHE_DCHECK(network_to_address_map_.contains(network_));
      pending_result_delegate_->OnCreationSucceeded(
          std::make_unique<TestQuicClientPathValidationContext>(
              network_to_address_map_[network_], peer_address_, network_));
      network_to_address_map_.erase(network_);
    }
    pending_result_delegate_ = nullptr;
  }

  void SetSelfAddressForNetwork(QuicNetworkHandle network,
                                const QuicSocketAddress& self_address) {
    QUICHE_DCHECK(!network_to_address_map_.contains(network));
    network_to_address_map_[network] = self_address;
  }

  size_t num_creation_attempts() const { return num_creation_attempts_; }

 private:
  bool async_creation_ = false;
  bool has_error_ = false;
  std::unique_ptr<CreationResultDelegate> pending_result_delegate_;
  QuicNetworkHandle network_ = kInvalidNetworkHandle;
  QuicSocketAddress peer_address_;
  absl::flat_hash_map<QuicNetworkHandle, QuicSocketAddress>
      network_to_address_map_;
  size_t num_creation_attempts_ = 0;
};

class TestCryptoStream : public QuicCryptoStream, public QuicCryptoHandshaker {
 public:
  explicit TestCryptoStream(QuicSession* session)
      : QuicCryptoStream(session),
        QuicCryptoHandshaker(this, session),
        params_(new QuicCryptoNegotiatedParameters) {
    // Simulate a negotiated cipher_suite with a fake value.
    params_->cipher_suite = 1;
  }

  void EstablishZeroRttEncryption() {
    encryption_established_ = true;
    session()->connection()->SetEncrypter(
        ENCRYPTION_ZERO_RTT,
        std::make_unique<NullEncrypter>(session()->perspective()));
  }

  void OnHandshakeMessage(const CryptoHandshakeMessage& /*message*/) override {
    encryption_established_ = true;
    one_rtt_keys_available_ = true;
    QuicErrorCode error;
    std::string error_details;
    session()->config()->SetInitialStreamFlowControlWindowToSend(
        test::kInitialStreamFlowControlWindowForTest);
    session()->config()->SetInitialSessionFlowControlWindowToSend(
        test::kInitialSessionFlowControlWindowForTest);
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
    session()->OnConfigNegotiated();
    EXPECT_THAT(error, test::IsQuicNoError());
    session()->OnNewEncryptionKeyAvailable(
        ENCRYPTION_FORWARD_SECURE,
        std::make_unique<NullEncrypter>(session()->perspective()));
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
  HandshakeState GetHandshakeState() const override {
    return one_rtt_keys_available() ? HANDSHAKE_CONFIRMED : HANDSHAKE_START;
  }
  void SetServerApplicationStateForResumption(
      std::unique_ptr<ApplicationState> /*application_state*/) override {}
  MOCK_METHOD(std::unique_ptr<QuicDecrypter>,
              AdvanceKeysAndCreateCurrentOneRttDecrypter, (), (override));
  MOCK_METHOD(std::unique_ptr<QuicEncrypter>, CreateCurrentOneRttEncrypter, (),
              (override));

  MOCK_METHOD(void, OnCanWrite, (), (override));
  bool HasPendingCryptoRetransmission() const override { return false; }

  MOCK_METHOD(bool, HasPendingRetransmission, (), (const, override));

  void OnConnectionClosed(const QuicConnectionCloseFrame& /*frame*/,
                          ConnectionCloseSource /*source*/) override {}

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
        return ENCRYPTION_FORWARD_SECURE;
      default:
        QUICHE_DCHECK(false);
        return NUM_ENCRYPTION_LEVELS;
    }
  }

 private:
  using QuicCryptoStream::session;

  bool encryption_established_ = false;
  bool one_rtt_keys_available_ = false;
  quiche::QuicheReferenceCountedPointer<QuicCryptoNegotiatedParameters> params_;
};

class TestStream : public QuicSpdyStream {
 public:
  TestStream(QuicStreamId id, QuicSpdySession* session, StreamType type)
      : QuicSpdyStream(id, session, type) {}

  TestStream(PendingStream* pending, QuicSpdySession* session)
      : QuicSpdyStream(pending, session) {}

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

class TestQuicSpdyClientSessionWithMigration
    : public QuicSpdyClientSessionWithMigration {
 public:
  TestQuicSpdyClientSessionWithMigration(
      QuicConnection* connection, QuicForceBlockablePacketWriter* writer,
      QuicSession::Visitor* visitor, const QuicConfig& config,
      const ParsedQuicVersionVector& supported_versions,
      QuicNetworkHandle default_network, QuicNetworkHandle current_network,
      std::unique_ptr<QuicPathContextFactory> path_context_factory,
      QuicConnectionMigrationConfig migration_config)
      : QuicSpdyClientSessionWithMigration(
            connection, writer, visitor, config, supported_versions,
            default_network, current_network, std::move(path_context_factory),
            migration_config, QuicPriorityType::kHttp),
        crypto_stream_(this) {
    ON_CALL(*this, IsSessionProxied()).WillByDefault(Return(false));
    ON_CALL(*this, OnMigrationToPathDone(_, _))
        .WillByDefault(
            [](std::unique_ptr<QuicPathValidationContext> context,
               bool success) {
              if (success) {
                static_cast<TestQuicClientPathValidationContext&>(*context)
                    .ReleasePacketWriter();
              }
            });
  }

  MOCK_METHOD(void, ResetNonMigratableStreams, (), (override));
  MOCK_METHOD(void, OnNoNewNetworkForMigration, (), (override));
  MOCK_METHOD(void, PrepareForProbingOnPath,
              (QuicPathValidationContext & context), (override));
  MOCK_METHOD(bool, IsSessionProxied, (), (const));
  MOCK_METHOD(bool, PrepareForMigrationToPath,
              (QuicClientPathValidationContext&));
  MOCK_METHOD(void, OnMigrationToPathDone,
              (std::unique_ptr<QuicClientPathValidationContext>, bool));
  MOCK_METHOD(void, OnConnectionToBeClosedDueToMigrationError,
              (MigrationCause migration_cause, QuicErrorCode quic_error),
              (override));

  // QuicSpdyClientSessionWithMigration.
  QuicNetworkHandle FindAlternateNetwork(QuicNetworkHandle network) override {
    QUICHE_DCHECK_NE(network, alternate_network_);
    return alternate_network_;
  }

  void StartDraining() override { going_away_ = true; }

  // QuicSession.
  TestCryptoStream* GetMutableCryptoStream() override {
    return &crypto_stream_;
  }

  const TestCryptoStream* GetCryptoStream() const override {
    return &crypto_stream_;
  }

  void OnProofValid(
      const QuicCryptoClientConfig::CachedState& /*cached*/) override {}

  void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& /*verify_details*/) override {}

  TestStream* CreateOutgoingBidirectionalStream() override {
    QuicStreamId id = GetNextOutgoingBidirectionalStreamId();
    if (id ==
        QuicUtils::GetInvalidStreamId(connection()->transport_version())) {
      return nullptr;
    }
    TestStream* stream = new TestStream(id, this, BIDIRECTIONAL);
    ActivateStream(absl::WrapUnique(stream));
    return stream;
  }

  TestStream* CreateIncomingStream(QuicStreamId id) override {
    // Enforce the limit on the number of open streams.
    if (!VersionHasIetfQuicFrames(connection()->transport_version()) &&
        stream_id_manager().num_open_incoming_streams() + 1 >
            max_open_incoming_bidirectional_streams()) {
      // No need to do this test for version 99; it's done by
      // QuicSession::GetOrCreateStream.
      connection()->CloseConnection(
          QUIC_TOO_MANY_OPEN_STREAMS, "Too many streams!",
          ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
      return nullptr;
    }

    TestStream* stream = new TestStream(
        id, this,
        test::DetermineStreamType(id, connection()->version(), perspective(),
                                  /*is_incoming=*/true, BIDIRECTIONAL));
    ActivateStream(absl::WrapUnique(stream));
    return stream;
  }

  TestStream* CreateIncomingStream(PendingStream* pending) override {
    TestStream* stream = new TestStream(pending, this);
    ActivateStream(absl::WrapUnique(stream));
    return stream;
  }

  void set_alternate_network(QuicNetworkHandle network) {
    alternate_network_ = network;
  }

  bool going_away() const { return going_away_; }

  using QuicSpdyClientSessionWithMigration::migration_manager;

 protected:
  bool ShouldCreateIncomingStream(QuicStreamId /*id*/) override { return true; }
  bool ShouldCreateOutgoingBidirectionalStream() override { return true; }

 private:
  QuicNetworkHandle alternate_network_ = kInvalidNetworkHandle;
  bool going_away_ = false;
  NiceMock<TestCryptoStream> crypto_stream_;
};

class QuicConnectionMigrationManagerTest
    : public test::QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicConnectionMigrationManagerTest()
      : versions_(ParsedQuicVersionVector{GetParam()}),
        config_(test::DefaultQuicConfig()),
        connection_(new StrictMock<test::MockQuicConnection>(
            &connection_helper_, &alarm_factory_, Perspective::IS_CLIENT,
            versions_)),
        default_writer_(new QuicForceBlockablePacketWriter()) {
    auto* writer = new NiceMock<MockPacketWriter>();
    // Owns writer.
    default_writer_->set_writer(writer);
    ON_CALL(*writer, WritePacket(_, _, _, _, _, _))
        .WillByDefault(Return(WriteResult(WRITE_STATUS_OK, 0)));
    ON_CALL(*writer, GetMaxPacketSize(_))
        .WillByDefault(Return(kMaxOutgoingPacketSize));
    ON_CALL(*writer, IsBatchMode()).WillByDefault(Return(false));
    ON_CALL(*writer, GetNextWriteLocation(_, _))
        .WillByDefault(Return(QuicPacketBuffer()));
    ON_CALL(*writer, Flush())
        .WillByDefault(Return(WriteResult(WRITE_STATUS_OK, 0)));
    ON_CALL(*writer, SupportsReleaseTime()).WillByDefault(Return(false));
    ON_CALL(*writer, MessageTooBigErrorCode())
        .WillByDefault(Return(kSocketErrorMsgSize));
    connection_->SetQuicPacketWriter(default_writer_, true);
  }

  void Initialize() {
    migration_config_.migrate_session_early =
        connection_migration_on_path_degrading_ &&
        connection_migration_on_network_change_;
    migration_config_.migrate_session_on_network_change =
        connection_migration_on_network_change_;
    migration_config_.allow_port_migration = port_migration_;
    migration_config_.migrate_idle_session = migrate_idle_session_;

    path_context_factory_ = new TestQuicPathContextFactory(
        async_path_context_creation_, /*has_error*/ false);
    session_ = std::make_unique<TestQuicSpdyClientSessionWithMigration>(
        connection_, default_writer_, &session_visitor_, config_, versions_,
        default_network_, initial_network_,
        std::unique_ptr<QuicPathContextFactory>(path_context_factory_),
        migration_config_);
    session_->Initialize();
    migration_manager_ = &session_->migration_manager();
    EXPECT_EQ(migration_manager_->default_network(), default_network_);
    EXPECT_EQ(migration_manager_->current_network(), initial_network_);

    connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));

    if (complete_handshake_) {
      CompleteHandshake(/*received_server_preferred_address=*/false);
      return;
    }
  }

  void CompleteHandshake(bool received_server_preferred_address) {
    const QuicConnectionId extra_connection_id = TestConnectionId(1234);
    ASSERT_NE(extra_connection_id, connection_->connection_id());
    const StatelessResetToken reset_token =
        QuicUtils::GenerateStatelessResetToken(extra_connection_id);
    if (version().HasIetfQuicFrames() && received_server_preferred_address) {
      // OnHandshakeMessage() will populate the received values with these.
      QuicIpAddress ipv4, ipv6;
      ASSERT_TRUE(ipv4.FromString("127.0.0.2"));
      ASSERT_TRUE(ipv6.FromString("::2"));
      session_->config()->SetIPv4AlternateServerAddressToSend(
          QuicSocketAddress(ipv4, 12345));
      session_->config()->SetIPv6AlternateServerAddressToSend(
          QuicSocketAddress(ipv6, 12345));
      session_->config()->SetPreferredAddressConnectionIdAndTokenToSend(
          extra_connection_id, reset_token);
    }
    CryptoHandshakeMessage msg;
    session_->GetMutableCryptoStream()->OnHandshakeMessage(msg);
    EXPECT_TRUE(session_->OneRttKeysAvailable());
    EXPECT_EQ(session_->GetHandshakeState(), HANDSHAKE_CONFIRMED);
    if (received_server_preferred_address) {
      EXPECT_TRUE(
          QuicConnectionPeer::GetReceivedServerPreferredAddress(connection_)
              .IsInitialized());
    }

    connection_->SetDefaultEncryptionLevel(ENCRYPTION_FORWARD_SECURE);
    if (version().HasIetfQuicFrames() && !received_server_preferred_address) {
      // Prepare an additional CID for future migration.
      QuicNewConnectionIdFrame frame;
      frame.connection_id = extra_connection_id;
      frame.stateless_reset_token = reset_token;
      frame.retire_prior_to = 0u;
      frame.sequence_number = 1u;
      connection_->OnNewConnectionIdFrame(frame);
    }
    connection_->OnHandshakeComplete();
  }

  ParsedQuicVersion version() const { return versions_[0]; }

 protected:
  QuicNetworkHandle default_network_ = 1;
  QuicNetworkHandle initial_network_ = 1;
  MockQuicConnectionHelper connection_helper_;
  MockAlarmFactory alarm_factory_;
  NiceMock<test::MockQuicSessionVisitor> session_visitor_;
  ParsedQuicVersionVector versions_;
  QuicConfig config_;
  QuicConnectionMigrationConfig migration_config_;
  // Owned by |session_|
  TestQuicPathContextFactory* path_context_factory_ = nullptr;
  // Owned by |session_|
  StrictMock<test::MockQuicConnection>* connection_;
  QuicForceBlockablePacketWriter* default_writer_;
  std::unique_ptr<TestQuicSpdyClientSessionWithMigration> session_;
  QuicConnectionMigrationManager* migration_manager_ = nullptr;
  bool connection_migration_on_path_degrading_ = true;
  bool port_migration_ = true;
  bool connection_migration_on_network_change_ = true;
  bool migrate_idle_session_ = false;
  bool complete_handshake_ = true;
  bool async_path_context_creation_ = false;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const ParsedQuicVersion& p) {
  return ParsedQuicVersionToString(p);
}

INSTANTIATE_TEST_SUITE_P(QuicConnectionMigrationManagerTests,
                         QuicConnectionMigrationManagerTest,
                         ::testing::ValuesIn(CurrentSupportedHttp3Versions()),
                         ::testing::PrintToStringParamName());

// This test verifies that session times out connection migration attempt
// with signals delivered in the following order (no alternate network is
// available):
// - default network disconnected is delivered: session attempts connection
//   migration but found not alternate network. Session waits for a new network
//   comes up in the next kWaitTimeForNewNetworkSecs seconds.
// - no new network is connected, migration times out. Session is closed.
TEST_P(QuicConnectionMigrationManagerTest, MigrationTimeoutWithNoNewNetwork) {
  Initialize();

  // Trigger a network disconnected signal to attempt migrating to a different
  // network. But since there is no alternative network available, no migration
  // should have happened.
  EXPECT_CALL(*session_, OnNoNewNetworkForMigration());
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 0u);
  EXPECT_TRUE(default_writer_->IsWriteBlocked());

  QuicAlarm* migration_alarm =
      QuicConnectionMigrationManagerPeer::GetWaitForMigrationAlarm(
          migration_manager_);
  EXPECT_TRUE(migration_alarm->IsSet());
  EXPECT_EQ(migration_alarm->deadline() - connection_helper_.GetClock()->Now(),
            QuicTimeDelta::FromSeconds(10));

  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK,
                      "Migration for cause OnNetworkDisconnected timed out",
                      ConnectionCloseBehavior::SILENT_CLOSE));
  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(10));
  alarm_factory_.FireAlarm(migration_alarm);
}

TEST_P(QuicConnectionMigrationManagerTest,
       MigrationDeferredUntilNewNetworkConnected) {
  migrate_idle_session_ = true;
  Initialize();

  // Trigger a network disconnected signal to attempt migrating to a different
  // network. But since there is no alternative network available, no migration
  // should have happened.
  EXPECT_CALL(*session_, OnNoNewNetworkForMigration());
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 0u);
  EXPECT_TRUE(default_writer_->IsWriteBlocked());

  QuicAlarm* migration_alarm =
      QuicConnectionMigrationManagerPeer::GetWaitForMigrationAlarm(
          migration_manager_);
  EXPECT_TRUE(migration_alarm->IsSet());
  EXPECT_EQ(migration_alarm->deadline() - connection_helper_.GetClock()->Now(),
            QuicTimeDelta::FromSeconds(10));

  // Alternative network connected. Another migration should be attempted.
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  migration_manager_->OnNetworkConnected(alternate_network);

  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_FALSE(connection_->writer()->IsWriteBlocked());
}

// This test verifies session migrates off the disconnected default network and
// migrates back to the default network later with probing.
TEST_P(QuicConnectionMigrationManagerTest,
       MigratingOffDisconnectedDefaultNetworkAndMigrateBack) {
  migrate_idle_session_ = true;
  Initialize();

  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);
  QuicSocketAddress self_address = connection_->self_address();

  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Update CIDs.
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(connection_);
  QuicAlarm* retire_cid_alarm =
      QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
  EXPECT_TRUE(retire_cid_alarm->IsSet());
  EXPECT_CALL(*connection_,
              SendControlFrame(IsFrame(RETIRE_CONNECTION_ID_FRAME)));
  alarm_factory_.FireAlarm(retire_cid_alarm);
  // Receive a new CID from peer.
  QuicNewConnectionIdFrame frame;
  frame.connection_id = test::TestConnectionId(5678);
  ASSERT_NE(frame.connection_id, connection_->connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;
  frame.sequence_number = 2u;
  connection_->OnNewConnectionIdFrame(frame);

  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));
  EXPECT_EQ(migration_manager_->default_network(), kInvalidNetworkHandle);

  // The default network is still not connected, so migration back should not
  // happen.
  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  QuicSocketAddress self_address2(self_address.host(), kTestPort + 1);
  path_context_factory_->SetSelfAddressForNetwork(initial_network_,
                                                  self_address2);
  // The default network is now connected, migration back should be attempted
  // again immediately.
  migration_manager_->OnNetworkMadeDefault(initial_network_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(migrate_back_alarm->deadline(),
            connection_helper_.GetClock()->Now());
  // Fire the alarm to migrate back to default network, starting with probing.
  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(2));
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& new_self_address,
                          const QuicSocketAddress& new_peer_address,
                          const QuicSocketAddress& /*effective_peer_address*/,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_EQ(new_peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // alternate network.
        EXPECT_EQ(new_self_address.host(), self_address2.host());
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 2u);

  // Make path validation succeeds and the connection should be migrated to the
  // default network.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      self_address2);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(2));
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), initial_network_);
  EXPECT_EQ(connection_->self_address(), self_address2);
  EXPECT_FALSE(migrate_back_alarm->IsSet());
}

// This test verifies that when the current network is disconnected, migration
// should be attempted immediately. Any write error during and after the path
// context creation should be ignored.
TEST_P(QuicConnectionMigrationManagerTest,
       NetworkDisconnectedFollowedByWriteErrorsAsyncPathContextCreation) {
  migrate_idle_session_ = true;
  async_path_context_creation_ = true;
  Initialize();

  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  // Receive a network disconnected signal, migration should be attempted
  // immediately.
  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // While waiting for the path context to be created asynchronously, any write
  // error shouldn't trigger another migration.
  migration_manager_->MaybeStartMigrateSessionOnWriteError(/*error_code=*/111);
  // An alarm should have been scheduled to run pending callbacks.
  QuicAlarm* pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_TRUE(pending_callbacks_alarm->IsSet());
  // Fire the alarm should not trigger another migration.
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Another write error which will be handle asynchronously after the path
  // context creation is finished should also be ignored.
  migration_manager_->MaybeStartMigrateSessionOnWriteError(/*error_code=*/111);
  // An alarm should have been scheduled to run pending callbacks.
  pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_TRUE(pending_callbacks_alarm->IsSet());

  // Finish creating the path context and continue the migration.
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  path_context_factory_->FinishPendingCreation();
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);

  // Fire the alarm to actually handle the 2nd write error, it should not
  // trigger another migration.
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
}

// This test verifies that sessions idle for longer than the configured
// |idle_migration_period| should not be migrated.
TEST_P(QuicConnectionMigrationManagerTest, DoNotMigrateLongIdleSession) {
  migrate_idle_session_ = true;
  Initialize();
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());

  connection_helper_.GetClock()->AdvanceTime(
      migration_config_.idle_migration_period);
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_NETWORK_IDLE_TIMEOUT,
                      "Idle session exceeds configured idle migration period",
                      ConnectionCloseBehavior::SILENT_CLOSE));
  migration_manager_->OnNetworkDisconnected(initial_network_);
}

// This test verifies that no idle sessions should be migrated if disallowed by
// config.
TEST_P(QuicConnectionMigrationManagerTest,
       DoNotMigrateIdleSessionIfDisabledByConfig) {
  migrate_idle_session_ = false;
  Initialize();
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
                              "Migrating idle session is disabled.",
                              ConnectionCloseBehavior::SILENT_CLOSE));
  migration_manager_->OnNetworkDisconnected(initial_network_);
}

TEST_P(QuicConnectionMigrationManagerTest,
       ConnectionMigrationDisabledDuringHandshakeAndNetworkDisconnected) {
  QuicConfigPeer::SetReceivedDisableConnectionMigration(&config_);
  migrate_idle_session_ = true;
  Initialize();

  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  EXPECT_TRUE(session_->config()->DisableConnectionMigration());

  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_CONNECTION_MIGRATION_DISABLED_BY_CONFIG,
                              "Migration disabled by config",
                              ConnectionCloseBehavior::SILENT_CLOSE));
  migration_manager_->OnNetworkDisconnected(initial_network_);
}

TEST_P(QuicConnectionMigrationManagerTest,
       ConnectionMigrationDisabledDuringHandshakeAndWriteError) {
  QuicConfigPeer::SetReceivedDisableConnectionMigration(&config_);
  migrate_idle_session_ = true;
  Initialize();
  EXPECT_TRUE(session_->config()->DisableConnectionMigration());

  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  migration_manager_->MaybeStartMigrateSessionOnWriteError(/*error_code=*/111);
  // An alarm should have been scheduled to run pending callbacks.
  QuicAlarm* pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_TRUE(pending_callbacks_alarm->IsSet());

  // Run pending callbacks should actually attempt migration and close the
  // connection since migration is disabled.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_CONNECTION_MIGRATION_DISABLED_BY_CONFIG,
                              "Unrecoverable write error",
                              ConnectionCloseBehavior::SILENT_CLOSE));
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
}

// This test verifies after session migrates off the default network, it keeps
// retrying migrate back to the default network until the default 30s idle
// migration period threshold is exceeded.
TEST_P(QuicConnectionMigrationManagerTest,
       MigratingOffDisconnectedDefaultNetworkAndHitIdleMigrationPeriod) {
  migrate_idle_session_ = true;
  Initialize();

  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_EQ(migration_manager_->default_network(), kInvalidNetworkHandle);
  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // The migrate back timer will fire. Due to default network being
  // disconnected, no attempt will be exercised to migrate back.
  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_)).Times(0);
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Old network now backs up. Re-attempt migration back to the default network.
  migration_manager_->OnNetworkMadeDefault(initial_network_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  // The 1st attempt starts immediately.
  EXPECT_EQ(migrate_back_alarm->deadline(),
            connection_helper_.GetClock()->Now());
  for (size_t i = 0; i < 5; ++i) {
    path_context_factory_->SetSelfAddressForNetwork(
        initial_network_,
        QuicSocketAddress(QuicIpAddress::Loopback4(), kTestPort + i));
    // Update CIDs.
    QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(
        connection_);
    QuicAlarm* retire_cid_alarm =
        QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
    EXPECT_TRUE(retire_cid_alarm->IsSet());
    // Receive a new CID from peer for the next attempt.
    QuicNewConnectionIdFrame frame;
    frame.connection_id = test::TestConnectionId(1234 + i + 1);
    ASSERT_NE(frame.connection_id, connection_->connection_id());
    frame.stateless_reset_token =
        QuicUtils::GenerateStatelessResetToken(frame.connection_id);
    frame.retire_prior_to = 2 + i;
    frame.sequence_number = 3 + i;
    connection_->OnNewConnectionIdFrame(frame);
    EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
    EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
        .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                            const QuicSocketAddress& self_address,
                            const QuicSocketAddress& peer_address,
                            const QuicSocketAddress& /*effective_peer_address*/,
                            QuicPacketWriter* writer) {
          EXPECT_EQ(peer_address, connection_->peer_address());
          // self address and writer used for probing should be for the
          // initial network.
          EXPECT_EQ(self_address.host(), QuicIpAddress::Loopback4());
          EXPECT_NE(writer, connection_->writer());
          return true;
        });
    alarm_factory_.FireAlarm(migrate_back_alarm);
    EXPECT_EQ(path_context_factory_->num_creation_attempts(), 2 + i) << i;
    EXPECT_TRUE(migrate_back_alarm->IsSet());
    // Fail the current path validation.
    auto* path_validator = QuicConnectionPeer::path_validator(connection_);
    path_validator->CancelPathValidation();
    // Following attempt should be scheduled with expotential delay.
    QuicTimeDelta next_delay = QuicTimeDelta::FromSeconds(UINT64_C(1) << i);
    EXPECT_EQ(migrate_back_alarm->deadline(),
              connection_helper_.GetClock()->Now() + next_delay);
    connection_helper_.GetClock()->AdvanceTime(next_delay);
  }

  // The connection should have been idle for longer than the idle migration
  // period. Next attempt to migrate back will close the connection.
  EXPECT_GT(session_->TimeSinceLastStreamClose(),
            migration_config_.idle_migration_period);
  //  The connection should be closed instead of attempting to migrate back.
  EXPECT_CALL(
      *connection_,
      CloseConnection(QUIC_NETWORK_IDLE_TIMEOUT,
                      "Idle session exceeds configured idle migration period",
                      ConnectionCloseBehavior::SILENT_CLOSE));
  alarm_factory_.FireAlarm(migrate_back_alarm);
}

// This test verifies after handshake completes on non-default network, it keeps
// retrying migrate back to the default network until the max time on
// non-default network (128s) is reached.
TEST_P(
    QuicConnectionMigrationManagerTest,
    MigrateBackToDefaultUponHandshakeCompleteAndHitMaxTimeOnNonDefaultNetwork) {
  complete_handshake_ = false;
  default_network_ = 2;
  Initialize();
  EXPECT_NE(migration_manager_->current_network(),
            migration_manager_->default_network());

  // Upon handshake completion, an alarm should have been scheduled to migrate
  // back to the default network in 1s.
  CompleteHandshake(false);
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // Create a stream to make the session non-idle.
  session_->CreateOutgoingBidirectionalStream();

  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));
  // Keep failing probing on the default network, and eventually hit max time on
  // non-default network (128s).
  for (size_t i = 0; i < 8; ++i) {
    path_context_factory_->SetSelfAddressForNetwork(
        default_network_,
        QuicSocketAddress(QuicIpAddress::Loopback6(), kTestPort + i));
    EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
    EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
        .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                            const QuicSocketAddress& self_address,
                            const QuicSocketAddress& peer_address,
                            const QuicSocketAddress& /*effective_peer_address*/,
                            QuicPacketWriter* writer) {
          EXPECT_EQ(peer_address, connection_->peer_address());
          // self address and writer used for probing should be for the
          // default network.
          EXPECT_EQ(self_address.host(), QuicIpAddress::Loopback6());
          EXPECT_NE(writer, connection_->writer());
          return true;
        });
    alarm_factory_.FireAlarm(migrate_back_alarm);
    EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1 + i);

    // Fail the current path validation.
    auto* path_validator = QuicConnectionPeer::path_validator(connection_);
    path_validator->CancelPathValidation();

    EXPECT_TRUE(migrate_back_alarm->IsSet());
    QuicTimeDelta next_delay = QuicTimeDelta::FromSeconds(UINT64_C(1) << i);
    EXPECT_EQ(migrate_back_alarm->deadline(),
              connection_helper_.GetClock()->Now() + next_delay)
        << i << ",  " << next_delay;
    connection_helper_.GetClock()->AdvanceTime(next_delay);

    // Update CIDs for the next attempt.
    QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(
        connection_);
    QuicAlarm* retire_cid_alarm =
        QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
    EXPECT_TRUE(retire_cid_alarm->IsSet());
    // Receive a new CID from peer for the next attempt.
    QuicNewConnectionIdFrame frame;
    frame.connection_id = test::TestConnectionId(1234 + i + 1);
    ASSERT_NE(frame.connection_id, connection_->connection_id());
    frame.stateless_reset_token =
        QuicUtils::GenerateStatelessResetToken(frame.connection_id);
    frame.retire_prior_to = 2 + i;
    frame.sequence_number = 3 + i;
    connection_->OnNewConnectionIdFrame(frame);
  }
  EXPECT_FALSE(session_->going_away());

  // Another attempt should exceeds 128s on non-default network timeout and the
  // session should be drained.
  path_context_factory_->SetSelfAddressForNetwork(
      default_network_,
      QuicSocketAddress(QuicIpAddress::Loopback6(), kTestPort + 8));
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_TRUE(session_->going_away());
}

// Test that if config.migrate_session_on_network_change is false, no migration
// back to default is scheduled after handshake completes on a non-default
// network.
TEST_P(QuicConnectionMigrationManagerTest,
       NoMigrateBackToDefaultWhenDisabledByConfig) {
  complete_handshake_ = false;
  connection_migration_on_network_change_ = false;
  default_network_ = 2;
  Initialize();
  EXPECT_NE(migration_manager_->current_network(),
            migration_manager_->default_network());

  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_FALSE(migrate_back_alarm->IsSet());

  CompleteHandshake(false);

  EXPECT_FALSE(migrate_back_alarm->IsSet());
}

// This test verifies that after receiving a signal that a new network becomes
// the default network, migration_manager attempts to probe the new default
// network, and meanwhile a signal of disconnection of the original network
// shouldn't trigger another migration attempt.
TEST_P(QuicConnectionMigrationManagerTest,
       CurrentNetworkDisconnectedWhileProbingNewDefaultNetwork) {
  Initialize();
  const QuicNetworkHandle new_default_network = 2;
  EXPECT_NE(migration_manager_->current_network(), new_default_network);

  // Create a stream to make the session non-idle.
  session_->CreateOutgoingBidirectionalStream();

  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);

  // Signal that the initial network which is already the default network
  // becomes the default. This should have no effect on migration.
  migration_manager_->OnNetworkMadeDefault(initial_network_);
  EXPECT_FALSE(migrate_back_alarm->IsSet());

  // Signal the new default network.
  path_context_factory_->SetSelfAddressForNetwork(
      new_default_network,
      QuicSocketAddress(QuicIpAddress::Loopback6(), kTestPort));
  migration_manager_->OnNetworkMadeDefault(new_default_network);

  // An alarm should have been scheduled to migrate back to the default network
  // immediately.
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(migrate_back_alarm->deadline(),
            connection_helper_.GetClock()->Now());

  // Fire the alarm to migrate back to default network, starting with probing.
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          const QuicSocketAddress& /*effective_peer_address*/,
                          QuicPacketWriter* writer) {
        EXPECT_EQ(peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // default network.
        EXPECT_EQ(self_address.host(), QuicIpAddress::Loopback6());
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_TRUE(session_->HasPendingPathValidation());
  // The alarm should be re-armed with a longer timeout.
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // Duplicated signal of new default network shouldn't trigger another probing
  // or change the migration back alarm.
  migration_manager_->OnNetworkMadeDefault(new_default_network);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_TRUE(session_->HasPendingPathValidation());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // Disconnect the current network, this should not trigger another migration
  // attempt.
  EXPECT_CALL(*session_, ResetNonMigratableStreams()).Times(0);
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).Times(0);
  EXPECT_CALL(*session_, OnNoNewNetworkForMigration()).Times(0);
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_TRUE(session_->HasPendingPathValidation());
}

TEST_P(QuicConnectionMigrationManagerTest, FailToProbeNewDefaultNetwork) {
  Initialize();
  const QuicNetworkHandle new_default_network = 2;
  EXPECT_NE(migration_manager_->current_network(), new_default_network);

  // Create a stream to make the session non-idle.
  session_->CreateOutgoingBidirectionalStream();

  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);

  // Signal the new default network.
  path_context_factory_->SetSelfAddressForNetwork(
      new_default_network,
      QuicSocketAddress(QuicIpAddress::Loopback6(), kTestPort));
  migration_manager_->OnNetworkMadeDefault(new_default_network);

  // An alarm should have been scheduled to migrate back to the default network
  // immediately.
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(migrate_back_alarm->deadline(),
            connection_helper_.GetClock()->Now());

  // Fire the alarm to migrate back to default network, starting with probing.
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .Times(3)
      .WillRepeatedly(
          [&, this](const QuicPathFrameBuffer& data_buffer,
                    const QuicSocketAddress& self_address,
                    const QuicSocketAddress& peer_address,
                    const QuicSocketAddress& /*effective_peer_address*/,
                    QuicPacketWriter* writer) {
            EXPECT_EQ(peer_address, connection_->peer_address());
            // self address and writer used for probing should be for the
            // default network.
            EXPECT_EQ(self_address.host(), QuicIpAddress::Loopback6());
            EXPECT_NE(writer, connection_->writer());
            return true;
          });
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_TRUE(session_->HasPendingPathValidation());

  // Simulate probing failure.
  auto* path_validator = QuicConnectionPeer::path_validator(connection_);
  QuicAlarm* retry_timer = QuicPathValidatorPeer::retry_timer(path_validator);
  alarm_factory_.FireAlarm(retry_timer);
  alarm_factory_.FireAlarm(retry_timer);
  alarm_factory_.FireAlarm(retry_timer);
  EXPECT_FALSE(session_->HasPendingPathValidation());
}

// This test verifies that the connection migrates to the alternate network
// early when path degrading is detected.
TEST_P(QuicConnectionMigrationManagerTest, MigrateEarlyOnPathDegrading) {
  Initialize();

  session_->CreateOutgoingBidirectionalStream();
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  // Upon path degrading, the migration manager should probe an alternative
  // network.
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          const QuicSocketAddress& effective_peer_address,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_EQ(peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // alternate network.
        EXPECT_EQ(self_address, alternate_self_address);
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  connection_->OnPathDegradingDetected();
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Make path validation succeeds and the connection should be migrated to the
  // alternate network.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      alternate_self_address);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);

  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // Notify the manager the alternate network has become default,
  // this will cancel migrate back to default network timer.
  migration_manager_->OnNetworkMadeDefault(alternate_network);
  EXPECT_EQ(migration_manager_->default_network(), alternate_network);
  EXPECT_FALSE(migrate_back_alarm->IsSet());
}

// This test verifies that the connection only migrates limited times to the
// alternate network from the default network when path degrading is detected
// for each default network.
TEST_P(QuicConnectionMigrationManagerTest,
       MigrationOnPathDegradingHitMaxLimit) {
  // Only allow one migration to non-default network on path degrading from the
  // same default network.
  migration_config_.max_migrations_to_non_default_network_on_path_degrading = 1;
  Initialize();

  session_->CreateOutgoingBidirectionalStream();
  QuicSocketAddress self_address = connection_->self_address();
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  // Upon path degrading, the migration manager should probe an alternative
  // network.
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          const QuicSocketAddress& effective_peer_address,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_EQ(peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // alternate network.
        EXPECT_EQ(self_address, alternate_self_address);
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  connection_->OnPathDegradingDetected();
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Make path validation succeeds and the connection should be migrated to the
  // alternate network.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      alternate_self_address);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);

  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // Update CIDs.
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(connection_);
  QuicAlarm* retire_cid_alarm =
      QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
  EXPECT_TRUE(retire_cid_alarm->IsSet());
  EXPECT_CALL(*connection_,
              SendControlFrame(IsFrame(RETIRE_CONNECTION_ID_FRAME)));
  alarm_factory_.FireAlarm(retire_cid_alarm);
  // Receive a new CID from peer.
  QuicNewConnectionIdFrame frame;
  frame.connection_id = test::TestConnectionId(5678);
  ASSERT_NE(frame.connection_id, connection_->connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;
  frame.sequence_number = 2u;
  connection_->OnNewConnectionIdFrame(frame);

  QuicSocketAddress self_address2(self_address.host(), kTestPort + 1);
  path_context_factory_->SetSelfAddressForNetwork(initial_network_,
                                                  self_address2);
  // Advance the clock to trigger the migrate back alarm.
  QuicPathFrameBuffer path_frame_payload2;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          const QuicSocketAddress& effective_peer_address,
                          QuicPacketWriter* writer) {
        path_frame_payload2 = data_buffer;
        EXPECT_EQ(peer_address, connection_->peer_address());
        EXPECT_EQ(self_address2, self_address);
        EXPECT_NE(writer, connection_->writer());
        return true;
      });

  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 2u);

  // Make path validation succeeds and the connection should be migrated back to
  // the default network.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      self_address2);
  const QuicPathResponseFrame path_response2(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response2);
  EXPECT_EQ(migration_manager_->current_network(), initial_network_);
  EXPECT_EQ(connection_->self_address(), self_address2);
  EXPECT_FALSE(migrate_back_alarm->IsSet());

  // Max migrations to non-default network is reached on the initial network.
  // The migration manager should not start probing when path degrading
  // is detected again.
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_)).Times(0);
  connection_->OnPathDegradingDetected();
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 2u);
}

// This test verifies that the connection migrates to the alternate port when
// path degrading is detected but connection migration is disabled. And such
// migration is only allowed 4 times.
TEST_P(QuicConnectionMigrationManagerTest,
       MigrateToDifferentPortOnPathDegrading) {
  connection_migration_on_network_change_ = false;
  port_migration_ = true;
  Initialize();

  session_->CreateOutgoingBidirectionalStream();
  // The first 4 path degrading events should trigger migration to different
  // ports on the same network.
  for (size_t i = 0; i < 4; ++i) {
    QuicSocketAddress alternate_self_address =
        QuicSocketAddress(connection_->self_address().host(),
                          /*port=*/connection_->self_address().port() + 1);
    path_context_factory_->SetSelfAddressForNetwork(initial_network_,
                                                    alternate_self_address);

    // Upon path degrading, the migration manager should probe a different port
    // on the same network.
    QuicPathFrameBuffer path_frame_payload;
    EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
    EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
        .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                            const QuicSocketAddress& self_address,
                            const QuicSocketAddress& peer_address,
                            const QuicSocketAddress& effective_peer_address,
                            QuicPacketWriter* writer) {
          path_frame_payload = data_buffer;
          EXPECT_EQ(peer_address, connection_->peer_address());
          // self address and writer used for probing should be for a
          // different port.
          EXPECT_EQ(self_address, alternate_self_address);
          EXPECT_NE(writer, connection_->writer());
          return true;
        });
    connection_->OnPathDegradingDetected();
    EXPECT_EQ(path_context_factory_->num_creation_attempts(), i + 1);

    // Make path validation succeeds and the connection should be migrated to
    // the alternate network.
    QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                        alternate_self_address);
    const QuicPathResponseFrame path_response(0, path_frame_payload);
    // No need to reset non-migratable streams before migrating to a different
    // port.
    EXPECT_CALL(*session_, ResetNonMigratableStreams()).Times(0);
    EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
    EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
    connection_->ReallyOnPathResponseFrame(path_response);
    // The network should not change.
    EXPECT_EQ(migration_manager_->current_network(), initial_network_);
    EXPECT_EQ(connection_->self_address(), alternate_self_address);

    QuicAlarm* migrate_back_alarm =
        QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
            migration_manager_);
    EXPECT_FALSE(migrate_back_alarm->IsSet());

    // Retire the old CID and prepare a new CID for the next path degrading.
    QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(
        connection_);
    QuicAlarm* retire_cid_alarm =
        QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
    EXPECT_TRUE(retire_cid_alarm->IsSet());
    EXPECT_CALL(*connection_,
                SendControlFrame(IsFrame(RETIRE_CONNECTION_ID_FRAME)))
        .Times(testing::AtMost(1));
    alarm_factory_.FireAlarm(retire_cid_alarm);

    QuicNewConnectionIdFrame frame;
    frame.connection_id = test::TestConnectionId(5678 + i);
    ASSERT_NE(frame.connection_id, connection_->connection_id());
    frame.stateless_reset_token =
        QuicUtils::GenerateStatelessResetToken(frame.connection_id);
    frame.retire_prior_to = 1u + i;
    frame.sequence_number = 2u + i;
    connection_->OnNewConnectionIdFrame(frame);
  }

  // The 5th path degrading should not trigger migration to a different port.
  connection_->OnPathDegradingDetected();
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 4u);
}

// This test verifies that the connection migrates to the alternate network
// when the alternate network is connected after path has been degrading.
TEST_P(QuicConnectionMigrationManagerTest,
       MigrateOnNewNetworkConnectAfterPathDegrading) {
  Initialize();

  session_->CreateOutgoingBidirectionalStream();
  // Path degrading failed to start migration because of lack of alternative
  // network.
  connection_->OnPathDegradingDetected();
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 0u);
  EXPECT_TRUE(connection_->IsPathDegrading());

  // When a new network become available, the migration manager should probe it.
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          const QuicSocketAddress& effective_peer_address,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_EQ(peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // alternate network.
        EXPECT_EQ(self_address, alternate_self_address);
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  migration_manager_->OnNetworkConnected(alternate_network);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Make path validation succeeds and the connection should be migrated to the
  // alternate network.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      alternate_self_address);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_FALSE(connection_->IsPathDegrading());

  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));

  // Notify the manager the alternate network has become default,
  // this will cancel migrate back to default network timer.
  migration_manager_->OnNetworkMadeDefault(alternate_network);
  EXPECT_EQ(migration_manager_->default_network(), alternate_network);
  EXPECT_FALSE(migrate_back_alarm->IsSet());
}

// This test verifies that when a write error occurs and there is no new
// network, the migration manager will wait for a new network to become
// available and then migrate to it.
TEST_P(QuicConnectionMigrationManagerTest,
       AsyncMigrationAttemptOnWriteErrorButNoNewNetwork) {
  Initialize();

  session_->CreateOutgoingBidirectionalStream();
  // Migration attempt should be made asynchronously.
  migration_manager_->MaybeStartMigrateSessionOnWriteError(123);
  QuicAlarm* pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_EQ(pending_callbacks_alarm->deadline(),
            connection_helper_.GetClock()->Now());

  // No alternative network available, an alarm should have been scheduled to
  // wait for any new network.
  EXPECT_CALL(*session_, OnNoNewNetworkForMigration());
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
  QuicAlarm* migration_alarm =
      QuicConnectionMigrationManagerPeer::GetWaitForMigrationAlarm(
          migration_manager_);
  EXPECT_TRUE(migration_alarm->IsSet());

  // Simulate a new network becomes available and migrate to it.
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  migration_manager_->OnNetworkConnected(alternate_network);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
  EXPECT_FALSE(migration_alarm->IsSet());
}

// This test verifies that session is not marked as going away after connection
// migration on write error and migrate back to default network logic is applied
// to bring the migrated session back to the default network. Migration signals
// delivered in the following order (alternate network is always available):
// - session on the default network encountered a write error;
// - session successfully migrated to the non-default network;
// - session attempts to migrate back to default network post migration;
// - migration back to the default network is successful.
TEST_P(QuicConnectionMigrationManagerTest,
       AsyncMigrationOnWriteErrorAndMigrateBack) {
  Initialize();
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  // Create a stream to make the session non-idle.
  session_->CreateOutgoingBidirectionalStream();
  // Migration attempt should be made asynchronously via an alarm scheduled for
  // next event loop.
  EXPECT_TRUE(migration_manager_->MaybeStartMigrateSessionOnWriteError(123));
  QuicAlarm* pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_TRUE(pending_callbacks_alarm->IsSet());
  QuicSocketAddress self_address = connection_->self_address();

  // Migrate to alternate network immediately.
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // Update CIDs.
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(connection_);
  QuicAlarm* retire_cid_alarm =
      QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
  EXPECT_TRUE(retire_cid_alarm->IsSet());
  EXPECT_CALL(*connection_,
              SendControlFrame(IsFrame(RETIRE_CONNECTION_ID_FRAME)));
  alarm_factory_.FireAlarm(retire_cid_alarm);
  // Receive a new CID from peer.
  QuicNewConnectionIdFrame frame;
  frame.connection_id = test::TestConnectionId(5678);
  ASSERT_NE(frame.connection_id, connection_->connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;
  frame.sequence_number = 2u;
  connection_->OnNewConnectionIdFrame(frame);

  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());
  EXPECT_EQ(
      migrate_back_alarm->deadline() - connection_helper_.GetClock()->Now(),
      QuicTimeDelta::FromSeconds(1));
  EXPECT_EQ(migration_manager_->default_network(), initial_network_);

  QuicSocketAddress self_address2(self_address.host(), kTestPort + 1);
  path_context_factory_->SetSelfAddressForNetwork(initial_network_,
                                                  self_address2);
  // Fire the alarm to migrate back to default network, starting with probing.
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& new_self_address,
                          const QuicSocketAddress& new_peer_address,
                          const QuicSocketAddress& /*effective_peer_address*/,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_EQ(new_peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // alternate network.
        EXPECT_EQ(new_self_address.host(), self_address2.host());
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 2u);

  // Make path validation succeeds and the connection should be migrated to the
  // default network.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      self_address2);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), initial_network_);
  EXPECT_EQ(connection_->self_address(), self_address2);
  EXPECT_FALSE(migrate_back_alarm->IsSet());
}

TEST_P(QuicConnectionMigrationManagerTest, MigrationToServerPreferredAddress) {
  if (!version().HasIetfQuicFrames()) {
    return;
  }
  complete_handshake_ = false;
  Initialize();

  // A new port will be used to probing to the server preferred address.
  QuicSocketAddress self_address2(QuicIpAddress::Loopback4(), kTestPort + 10);
  path_context_factory_->SetSelfAddressForNetwork(initial_network_,
                                                  self_address2);
  // Upon handshake completion, probing to the server preferred address should
  // be started.
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          const QuicSocketAddress& /*effective_peer_address*/,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_NE(peer_address, connection_->peer_address());
        // self address and writer used for probing should be for the
        // default network.
        EXPECT_EQ(self_address, QuicSocketAddress(QuicIpAddress::Loopback4(),
                                                  kTestPort + 10));
        EXPECT_NE(writer, connection_->writer());
        return true;
      });

  CompleteHandshake(/*received_server_preferred_address=*/true);

  // Make path validation succeeds and the connection should start using the
  // server preferred address.
  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      self_address2);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), initial_network_);
  EXPECT_EQ(connection_->self_address(), self_address2);
  EXPECT_EQ(connection_->peer_address().ToString(), "127.0.0.2:12345");
}

// This test verifies that if the max number of migrations is reached
// on write error, the session will be closed.
TEST_P(QuicConnectionMigrationManagerTest,
       AsyncMigrationOnWriteErrorMaxAttemptsReached) {
  migration_config_.max_migrations_to_non_default_network_on_write_error = 1;
  Initialize();
  session_->CreateOutgoingBidirectionalStream();

  // Set up an alternate network and migrate to it on write error.
  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  EXPECT_TRUE(migration_manager_->MaybeStartMigrateSessionOnWriteError(123));
  QuicAlarm* pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_TRUE(pending_callbacks_alarm->IsSet());

  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
  EXPECT_EQ(migration_manager_->current_network(), alternate_network);
  EXPECT_EQ(connection_->self_address(), alternate_self_address);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);

  // An alarm should have been scheduled to try to migrate back to the default
  // network in 1s.
  QuicAlarm* migrate_back_alarm =
      QuicConnectionMigrationManagerPeer::GetMigrateBackToDefaultTimer(
          migration_manager_);
  EXPECT_TRUE(migrate_back_alarm->IsSet());

  // Update CIDs.
  QuicConnectionPeer::RetirePeerIssuedConnectionIdsNoLongerOnPath(connection_);
  QuicAlarm* retire_cid_alarm =
      QuicConnectionPeer::GetRetirePeerIssuedConnectionIdAlarm(connection_);
  EXPECT_TRUE(retire_cid_alarm->IsSet());
  EXPECT_CALL(*connection_,
              SendControlFrame(IsFrame(RETIRE_CONNECTION_ID_FRAME)));
  alarm_factory_.FireAlarm(retire_cid_alarm);
  // Receive a new CID from peer.
  QuicNewConnectionIdFrame frame;
  frame.connection_id = test::TestConnectionId(5678);
  ASSERT_NE(frame.connection_id, connection_->connection_id());
  frame.stateless_reset_token =
      QuicUtils::GenerateStatelessResetToken(frame.connection_id);
  frame.retire_prior_to = 1u;
  frame.sequence_number = 2u;
  connection_->OnNewConnectionIdFrame(frame);

  // Migrate back to the default network.
  QuicSocketAddress self_address2(QuicIpAddress::Loopback4(), kTestPort + 1);
  path_context_factory_->SetSelfAddressForNetwork(initial_network_,
                                                  self_address2);
  QuicPathFrameBuffer path_frame_payload;
  EXPECT_CALL(*session_, PrepareForProbingOnPath(_));
  EXPECT_CALL(*connection_, SendPathChallenge(_, _, _, _, _))
      .WillOnce([&, this](const QuicPathFrameBuffer& data_buffer,
                          const QuicSocketAddress& new_self_address,
                          const QuicSocketAddress& new_peer_address,
                          const QuicSocketAddress& /*effective_peer_address*/,
                          QuicPacketWriter* writer) {
        path_frame_payload = data_buffer;
        EXPECT_EQ(new_peer_address, connection_->peer_address());
        EXPECT_EQ(new_self_address.host(), self_address2.host());
        EXPECT_NE(writer, connection_->writer());
        return true;
      });
  connection_helper_.GetClock()->AdvanceTime(QuicTimeDelta::FromSeconds(1));
  alarm_factory_.FireAlarm(migrate_back_alarm);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 2u);

  QuicConnectionPeer::SetLastPacketDestinationAddress(connection_,
                                                      self_address2);
  const QuicPathResponseFrame path_response(0, path_frame_payload);
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(true));
  EXPECT_CALL(*session_, OnMigrationToPathDone(_, true));
  connection_->ReallyOnPathResponseFrame(path_response);
  EXPECT_EQ(migration_manager_->current_network(), initial_network_);
  EXPECT_EQ(connection_->self_address(), self_address2);
  EXPECT_FALSE(migrate_back_alarm->IsSet());

  // Max migrations on write error is reached.
  // The migration manager should not start migration but close the connection.
  EXPECT_TRUE(migration_manager_->MaybeStartMigrateSessionOnWriteError(456));
  pending_callbacks_alarm =
      QuicConnectionMigrationManagerPeer::GetRunPendingCallbacksAlarm(
          migration_manager_);
  EXPECT_TRUE(pending_callbacks_alarm->IsSet());

  EXPECT_CALL(*connection_,
              CloseConnection(
                  QUIC_PACKET_WRITE_ERROR,
                  "Too many migrations for write error for the same network",
                  ConnectionCloseBehavior::SILENT_CLOSE));
  alarm_factory_.FireAlarm(pending_callbacks_alarm);
}

class QuicConnectionMigrationManagerGoogleQuicTest
    : public QuicConnectionMigrationManagerTest {};

INSTANTIATE_TEST_SUITE_P(QuicConnectionMigrationManagerGoogleQuicTests,
                         QuicConnectionMigrationManagerGoogleQuicTest,
                         ::testing::ValuesIn(ParsedQuicVersionVector{
                             quic::ParsedQuicVersion::Q046()}),
                         ::testing::PrintToStringParamName());

TEST_P(QuicConnectionMigrationManagerGoogleQuicTest, NoMigrationForGoogleQuic) {
  Initialize();
  session_->set_alternate_network(-1);
  EXPECT_FALSE(migration_manager_->MaybeStartMigrateSessionOnWriteError(111));
  // If the session has attempted to migrate, it would have found no alternate
  // network and called OnNoNewNetworkForMigration().
  EXPECT_CALL(*session_, OnNoNewNetworkForMigration()).Times(0);
  migration_manager_->OnPathDegrading();
  migration_manager_->OnNetworkDisconnected(initial_network_);
}

class QuicSpdyClientSessionWithMigrationTest
    : public QuicConnectionMigrationManagerTest {};

INSTANTIATE_TEST_SUITE_P(QuicSpdyClientSessionWithMigrationTests,
                         QuicSpdyClientSessionWithMigrationTest,
                         ::testing::ValuesIn(CurrentSupportedHttp3Versions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicSpdyClientSessionWithMigrationTest,
       SessionFailedToPrepareForMigration) {
  migrate_idle_session_ = true;
  Initialize();

  const QuicNetworkHandle alternate_network = 2;
  session_->set_alternate_network(alternate_network);
  EXPECT_NE(alternate_network, migration_manager_->current_network());
  QuicSocketAddress alternate_self_address =
      QuicSocketAddress(QuicIpAddress::Loopback6(), /*port=*/kTestPort);
  EXPECT_NE(alternate_self_address.host(), connection_->self_address().host());
  path_context_factory_->SetSelfAddressForNetwork(alternate_network,
                                                  alternate_self_address);

  EXPECT_EQ(session_->TimeSinceLastStreamClose(),
            QuicTimeDelta::FromSeconds(1));
  EXPECT_CALL(*session_, ResetNonMigratableStreams());
  // Session failed to prepare for migration. Migration should not be attempted.
  EXPECT_CALL(*session_, PrepareForMigrationToPath(_)).WillOnce(Return(false));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_CONNECTION_MIGRATION_INTERNAL_ERROR,
                              "Session failed to migrate to new path.",
                              ConnectionCloseBehavior::SILENT_CLOSE));
  migration_manager_->OnNetworkDisconnected(initial_network_);
  EXPECT_EQ(migration_manager_->current_network(), default_network_);
  EXPECT_EQ(path_context_factory_->num_creation_attempts(), 1u);
}

}  // namespace quic::test
