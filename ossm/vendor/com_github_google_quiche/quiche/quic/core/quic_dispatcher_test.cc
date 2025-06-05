// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_dispatcher.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>


#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/chlo_extractor.h"
#include "quiche/quic/core/connection_id_generator.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/crypto/quic_compressed_certs_cache.h"
#include "quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "quiche/quic/core/crypto/quic_crypto_server_config.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/crypto/transport_parameters.h"
#include "quiche/quic/core/frames/quic_connection_close_frame.h"
#include "quiche/quic/core/http/quic_server_session_base.h"
#include "quiche/quic/core/http/quic_spdy_stream.h"
#include "quiche/quic/core/quic_config.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_crypto_server_stream_base.h"
#include "quiche/quic/core/quic_crypto_stream.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_packet_writer.h"
#include "quiche/quic/core/quic_packet_writer_wrapper.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_stream.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_time_wait_list_manager.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_version_manager.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/first_flight.h"
#include "quiche/quic/test_tools/mock_connection_id_generator.h"
#include "quiche/quic/test_tools/mock_quic_time_wait_list_manager.h"
#include "quiche/quic/test_tools/quic_buffered_packet_store_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_dispatcher_peer.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/tools/quic_simple_crypto_server_stream_helper.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

using testing::_;
using testing::AllOf;
using testing::ByMove;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Not;
using testing::Ref;
using testing::Return;
using testing::ReturnRef;
using testing::WithArg;
using testing::WithoutArgs;

static const size_t kDefaultMaxConnectionsInStore = 100;
static const size_t kMaxConnectionsWithoutCHLO =
    kDefaultMaxConnectionsInStore / 2;
static const int16_t kMaxNumSessionsToCreate = 16;

namespace quic {
namespace test {
namespace {

const QuicConnectionId kReturnConnectionId{
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}};

class TestQuicSpdyServerSession : public QuicServerSessionBase {
 public:
  TestQuicSpdyServerSession(const QuicConfig& config,
                            QuicConnection* connection,
                            const QuicCryptoServerConfig* crypto_config,
                            QuicCompressedCertsCache* compressed_certs_cache)
      : QuicServerSessionBase(config, CurrentSupportedVersions(), connection,
                              nullptr, nullptr, crypto_config,
                              compressed_certs_cache) {
    Initialize();
  }
  TestQuicSpdyServerSession(const TestQuicSpdyServerSession&) = delete;
  TestQuicSpdyServerSession& operator=(const TestQuicSpdyServerSession&) =
      delete;

  ~TestQuicSpdyServerSession() override { DeleteConnection(); }

  MOCK_METHOD(void, OnConnectionClosed,
              (const QuicConnectionCloseFrame& frame,
               ConnectionCloseSource source),
              (override));
  MOCK_METHOD(QuicSpdyStream*, CreateIncomingStream, (QuicStreamId id),
              (override));
  MOCK_METHOD(QuicSpdyStream*, CreateIncomingStream, (PendingStream*),
              (override));
  MOCK_METHOD(QuicSpdyStream*, CreateOutgoingBidirectionalStream, (),
              (override));
  MOCK_METHOD(QuicSpdyStream*, CreateOutgoingUnidirectionalStream, (),
              (override));

  std::unique_ptr<QuicCryptoServerStreamBase> CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) override {
    return CreateCryptoServerStream(crypto_config, compressed_certs_cache, this,
                                    stream_helper());
  }

  QuicCryptoServerStreamBase::Helper* stream_helper() {
    return QuicServerSessionBase::stream_helper();
  }
};

class TestDispatcher : public QuicDispatcher {
 public:
  TestDispatcher(const QuicConfig* config,
                 const QuicCryptoServerConfig* crypto_config,
                 QuicVersionManager* version_manager, QuicRandom* random,
                 ConnectionIdGeneratorInterface& generator)
      : QuicDispatcher(config, crypto_config, version_manager,
                       std::make_unique<MockQuicConnectionHelper>(),
                       std::unique_ptr<QuicCryptoServerStreamBase::Helper>(
                           new QuicSimpleCryptoServerStreamHelper()),
                       std::make_unique<TestAlarmFactory>(),
                       kQuicDefaultConnectionIdLength, generator),
        random_(random) {
    EXPECT_CALL(*this, ConnectionIdGenerator())
        .WillRepeatedly(ReturnRef(generator));
  }

  MOCK_METHOD(std::unique_ptr<QuicSession>, CreateQuicSession,
              (QuicConnectionId connection_id,
               const QuicSocketAddress& self_address,
               const QuicSocketAddress& peer_address, absl::string_view alpn,
               const ParsedQuicVersion& version,
               const ParsedClientHello& parsed_chlo,
               ConnectionIdGeneratorInterface& connection_id_generator),
              (override));
  MOCK_METHOD(ConnectionIdGeneratorInterface&, ConnectionIdGenerator, (),
              (override));

  struct TestQuicPerPacketContext : public QuicPerPacketContext {
    std::string custom_packet_context;
  };

  std::unique_ptr<QuicPerPacketContext> GetPerPacketContext() const override {
    auto test_context = std::make_unique<TestQuicPerPacketContext>();
    test_context->custom_packet_context = custom_packet_context_;
    return std::move(test_context);
  }

  void RestorePerPacketContext(
      std::unique_ptr<QuicPerPacketContext> context) override {
    TestQuicPerPacketContext* test_context =
        static_cast<TestQuicPerPacketContext*>(context.get());
    custom_packet_context_ = test_context->custom_packet_context;
  }

  std::string custom_packet_context_;

  using QuicDispatcher::ConnectionIdGenerator;
  using QuicDispatcher::MaybeDispatchPacket;
  using QuicDispatcher::writer;

  QuicRandom* random_;
};

// A Connection class which unregisters the session from the dispatcher when
// sending connection close.
// It'd be slightly more realistic to do this from the Session but it would
// involve a lot more mocking.
class MockServerConnection : public MockQuicConnection {
 public:
  MockServerConnection(QuicConnectionId connection_id,
                       MockQuicConnectionHelper* helper,
                       MockAlarmFactory* alarm_factory,
                       QuicDispatcher* dispatcher)
      : MockQuicConnection(connection_id, helper, alarm_factory,
                           Perspective::IS_SERVER),
        dispatcher_(dispatcher),
        active_connection_ids_({connection_id}) {}

  void AddNewConnectionId(QuicConnectionId id) {
    if (!dispatcher_->TryAddNewConnectionId(active_connection_ids_.back(),
                                            id)) {
      return;
    }
    QuicConnectionPeer::SetServerConnectionId(this, id);
    active_connection_ids_.push_back(id);
  }

  void UnconditionallyAddNewConnectionIdForTest(QuicConnectionId id) {
    dispatcher_->TryAddNewConnectionId(active_connection_ids_.back(), id);
    active_connection_ids_.push_back(id);
  }

  void RetireConnectionId(QuicConnectionId id) {
    auto it = std::find(active_connection_ids_.begin(),
                        active_connection_ids_.end(), id);
    QUICHE_DCHECK(it != active_connection_ids_.end());
    dispatcher_->OnConnectionIdRetired(id);
    active_connection_ids_.erase(it);
  }

  std::vector<QuicConnectionId> GetActiveServerConnectionIds() const override {
    std::vector<QuicConnectionId> result;
    for (const auto& cid : active_connection_ids_) {
      result.push_back(cid);
    }
    auto original_connection_id = GetOriginalDestinationConnectionId();
    if (std::find(result.begin(), result.end(), original_connection_id) ==
        result.end()) {
      result.push_back(original_connection_id);
    }
    return result;
  }

  void UnregisterOnConnectionClosed() {
    QUIC_LOG(ERROR) << "Unregistering " << connection_id();
    dispatcher_->OnConnectionClosed(connection_id(), QUIC_NO_ERROR,
                                    "Unregistering.",
                                    ConnectionCloseSource::FROM_SELF);
  }

 private:
  QuicDispatcher* dispatcher_;
  std::vector<QuicConnectionId> active_connection_ids_;
};

class QuicDispatcherTestBase : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicDispatcherTestBase()
      : QuicDispatcherTestBase(crypto_test_utils::ProofSourceForTesting()) {}

  explicit QuicDispatcherTestBase(std::unique_ptr<ProofSource> proof_source)
      : QuicDispatcherTestBase(std::move(proof_source),
                               AllSupportedVersions()) {}

  explicit QuicDispatcherTestBase(
      const ParsedQuicVersionVector& supported_versions)
      : QuicDispatcherTestBase(crypto_test_utils::ProofSourceForTesting(),
                               supported_versions) {}

  explicit QuicDispatcherTestBase(
      std::unique_ptr<ProofSource> proof_source,
      const ParsedQuicVersionVector& supported_versions)
      : version_(GetParam()),
        version_manager_(supported_versions),
        crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance(), std::move(proof_source),
                       KeyExchangeSource::Default()),
        server_address_(QuicIpAddress::Any4(), 5),
        dispatcher_(new NiceMock<TestDispatcher>(
            &config_, &crypto_config_, &version_manager_,
            mock_helper_.GetRandomGenerator(), connection_id_generator_)),
        time_wait_list_manager_(nullptr),
        session1_(nullptr),
        session2_(nullptr),
        store_(nullptr),
        connection_id_(1) {}

  void SetUp() override {
    dispatcher_->InitializeWithWriter(new NiceMock<MockPacketWriter>());
    // Set the counter to some value to start with.
    QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(
        dispatcher_.get(), kMaxNumSessionsToCreate);
  }

  MockQuicConnection* connection1() {
    if (session1_ == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<MockQuicConnection*>(session1_->connection());
  }

  MockQuicConnection* connection2() {
    if (session2_ == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<MockQuicConnection*>(session2_->connection());
  }

  // Process a packet with an 8 byte connection id,
  // 6 byte packet number, default path id, and packet number 1,
  // using the version under test.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag, const std::string& data) {
    ProcessPacket(peer_address, server_connection_id, has_version_flag, data,
                  CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER);
  }

  // Process a packet with a default path id, and packet number 1,
  // using the version under test.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag, const std::string& data,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicPacketNumberLength packet_number_length) {
    ProcessPacket(peer_address, server_connection_id, has_version_flag, data,
                  server_connection_id_included, packet_number_length, 1);
  }

  // Process a packet using the version under test.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag, const std::string& data,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicPacketNumberLength packet_number_length,
                     uint64_t packet_number) {
    ProcessPacket(peer_address, server_connection_id, has_version_flag,
                  version_, data, true, server_connection_id_included,
                  packet_number_length, packet_number);
  }

  // Processes a packet.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     bool has_version_flag, ParsedQuicVersion version,
                     const std::string& data, bool full_padding,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicPacketNumberLength packet_number_length,
                     uint64_t packet_number) {
    ProcessPacket(peer_address, server_connection_id, EmptyQuicConnectionId(),
                  has_version_flag, version, data, full_padding,
                  server_connection_id_included, CONNECTION_ID_ABSENT,
                  packet_number_length, packet_number);
  }

  // Processes a packet.
  void ProcessPacket(QuicSocketAddress peer_address,
                     QuicConnectionId server_connection_id,
                     QuicConnectionId client_connection_id,
                     bool has_version_flag, ParsedQuicVersion version,
                     const std::string& data, bool full_padding,
                     QuicConnectionIdIncluded server_connection_id_included,
                     QuicConnectionIdIncluded client_connection_id_included,
                     QuicPacketNumberLength packet_number_length,
                     uint64_t packet_number) {
    ParsedQuicVersionVector versions(SupportedVersions(version));
    std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
        server_connection_id, client_connection_id, has_version_flag, false,
        packet_number, data, full_padding, server_connection_id_included,
        client_connection_id_included, packet_number_length, &versions));
    std::unique_ptr<QuicReceivedPacket> received_packet(
        ConstructReceivedPacket(*packet, mock_helper_.GetClock()->Now()));
    // Call ConnectionIdLength if the packet clears the Long Header bit, or
    // if the test involves sending a connection ID that is too short
    if (!has_version_flag || !version.AllowsVariableLengthConnectionIds() ||
        server_connection_id.length() == 0 ||
        server_connection_id_included == CONNECTION_ID_ABSENT) {
      // Short headers will ask for the length
      EXPECT_CALL(connection_id_generator_, ConnectionIdLength(_))
          .WillRepeatedly(Return(generated_connection_id_.has_value()
                                     ? generated_connection_id_->length()
                                     : kQuicDefaultConnectionIdLength));
    }
    ProcessReceivedPacket(std::move(received_packet), peer_address, version,
                          server_connection_id);
  }

  void ProcessReceivedPacket(
      std::unique_ptr<QuicReceivedPacket> received_packet,
      const QuicSocketAddress& peer_address, const ParsedQuicVersion& version,
      const QuicConnectionId& server_connection_id) {
    if (version.UsesQuicCrypto() &&
        ChloExtractor::Extract(*received_packet, version, {}, nullptr,
                               server_connection_id.length())) {
      // Add CHLO packet to the beginning to be verified first, because it is
      // also processed first by new session.
      data_connection_map_[server_connection_id].push_front(
          std::string(received_packet->data(), received_packet->length()));
    } else {
      // For non-CHLO, always append to last.
      data_connection_map_[server_connection_id].push_back(
          std::string(received_packet->data(), received_packet->length()));
    }
    dispatcher_->ProcessPacket(server_address_, peer_address, *received_packet);
  }

  void ValidatePacket(QuicConnectionId conn_id,
                      const QuicEncryptedPacket& packet) {
    EXPECT_EQ(data_connection_map_[conn_id].front().length(),
              packet.AsStringPiece().length());
    EXPECT_EQ(data_connection_map_[conn_id].front(), packet.AsStringPiece());
    data_connection_map_[conn_id].pop_front();
  }

  std::unique_ptr<QuicSession> CreateSession(
      TestDispatcher* dispatcher, const QuicConfig& config,
      QuicConnectionId connection_id, const QuicSocketAddress& /*peer_address*/,
      MockQuicConnectionHelper* helper, MockAlarmFactory* alarm_factory,
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache,
      TestQuicSpdyServerSession** session_ptr) {
    MockServerConnection* connection = new MockServerConnection(
        connection_id, helper, alarm_factory, dispatcher);
    connection->SetQuicPacketWriter(dispatcher->writer(),
                                    /*owns_writer=*/false);
    auto session = std::make_unique<TestQuicSpdyServerSession>(
        config, connection, crypto_config, compressed_certs_cache);
    *session_ptr = session.get();
    connection->set_visitor(session.get());
    ON_CALL(*connection, CloseConnection(_, _, _))
        .WillByDefault(WithoutArgs(Invoke(
            connection, &MockServerConnection::UnregisterOnConnectionClosed)));
    return session;
  }

  void CreateTimeWaitListManager() {
    time_wait_list_manager_ = new MockTimeWaitListManager(
        QuicDispatcherPeer::GetWriter(dispatcher_.get()), dispatcher_.get(),
        mock_helper_.GetClock(), &mock_alarm_factory_);
    // dispatcher_ takes the ownership of time_wait_list_manager_.
    QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                               time_wait_list_manager_);
  }

  std::string SerializeCHLO() {
    CryptoHandshakeMessage client_hello;
    client_hello.set_tag(kCHLO);
    client_hello.SetStringPiece(kALPN, ExpectedAlpn());
    return std::string(client_hello.GetSerialized().AsStringPiece());
  }

  void ProcessUndecryptableEarlyPacket(
      const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    ProcessUndecryptableEarlyPacket(version_, peer_address,
                                    server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const ParsedQuicVersion& version, const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    std::unique_ptr<QuicEncryptedPacket> encrypted_packet =
        GetUndecryptableEarlyPacket(version, server_connection_id);
    std::unique_ptr<QuicReceivedPacket> received_packet(ConstructReceivedPacket(
        *encrypted_packet, mock_helper_.GetClock()->Now()));
    ProcessReceivedPacket(std::move(received_packet), peer_address, version,
                          server_connection_id);
  }

  void ProcessFirstFlight(const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version_, peer_address, server_connection_id);
  }

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version, peer_address, server_connection_id,
                       EmptyQuicConnectionId());
  }

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id,
                          const QuicConnectionId& client_connection_id) {
    ProcessFirstFlight(version, peer_address, server_connection_id,
                       client_connection_id, TestClientCryptoConfig());
  }

  void ProcessFirstFlight(
      const ParsedQuicVersion& version, const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id,
      const QuicConnectionId& client_connection_id,
      std::unique_ptr<QuicCryptoClientConfig> client_crypto_config) {
    if (expect_generator_is_called_) {
      if (version.AllowsVariableLengthConnectionIds()) {
        EXPECT_CALL(connection_id_generator_,
                    MaybeReplaceConnectionId(server_connection_id, version))
            .WillOnce(Return(generated_connection_id_));
      } else {
        EXPECT_CALL(connection_id_generator_,
                    MaybeReplaceConnectionId(server_connection_id, version))
            .WillOnce(Return(std::nullopt));
      }
    }
    std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
        GetFirstFlightOfPackets(version, DefaultQuicConfig(),
                                server_connection_id, client_connection_id,
                                std::move(client_crypto_config));
    for (auto&& packet : packets) {
      ProcessReceivedPacket(std::move(packet), peer_address, version,
                            server_connection_id);
    }
  }

  std::unique_ptr<QuicCryptoClientConfig> TestClientCryptoConfig() {
    auto client_crypto_config = std::make_unique<QuicCryptoClientConfig>(
        crypto_test_utils::ProofVerifierForTesting());
    if (address_token_.has_value()) {
      client_crypto_config->LookupOrCreate(TestServerId())
          ->set_source_address_token(*address_token_);
    }
    return client_crypto_config;
  }

  // If called, the first flight packets generated in |ProcessFirstFlight| will
  // contain the given |address_token|.
  void SetAddressToken(std::string address_token) {
    address_token_ = std::move(address_token);
  }

  std::string ExpectedAlpnForVersion(ParsedQuicVersion version) {
    return AlpnForVersion(version);
  }

  std::string ExpectedAlpn() { return ExpectedAlpnForVersion(version_); }

  auto MatchParsedClientHello() {
    if (version_.UsesQuicCrypto()) {
      return AllOf(
          Field(&ParsedClientHello::alpns, ElementsAreArray({ExpectedAlpn()})),
          Field(&ParsedClientHello::sni, Eq(TestHostname())),
          Field(&ParsedClientHello::supported_groups, IsEmpty()));
    }
    return AllOf(
        Field(&ParsedClientHello::alpns, ElementsAreArray({ExpectedAlpn()})),
        Field(&ParsedClientHello::sni, Eq(TestHostname())),
        Field(&ParsedClientHello::supported_groups, Not(IsEmpty())));
  }

  void MarkSession1Deleted() { session1_ = nullptr; }

  void VerifyVersionSupported(ParsedQuicVersion version) {
    expect_generator_is_called_ = true;
    QuicConnectionId connection_id = TestConnectionId(++connection_id_);
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(connection_id, _, client_address,
                                  Eq(ExpectedAlpnForVersion(version)), _, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, connection_id, client_address,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, connection_id](const QuicEncryptedPacket& packet) {
              ValidatePacket(connection_id, packet);
            })));
    ProcessFirstFlight(version, client_address, connection_id);
  }

  void VerifyVersionNotSupported(ParsedQuicVersion version) {
    QuicConnectionId connection_id = TestConnectionId(++connection_id_);
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
    EXPECT_CALL(*dispatcher_,
                CreateQuicSession(connection_id, _, client_address, _, _, _, _))
        .Times(0);
    expect_generator_is_called_ = false;
    ProcessFirstFlight(version, client_address, connection_id);
  }

  void TestTlsMultiPacketClientHello(bool add_reordering,
                                     bool long_connection_id);

  void TestVersionNegotiationForUnknownVersionInvalidShortInitialConnectionId(
      const QuicConnectionId& server_connection_id,
      const QuicConnectionId& client_connection_id);

  TestAlarmFactory::TestAlarm* GetClearResetAddressesAlarm() {
    return reinterpret_cast<TestAlarmFactory::TestAlarm*>(
        QuicDispatcherPeer::GetClearResetAddressesAlarm(dispatcher_.get()));
  }

  ParsedQuicVersion version_;
  MockQuicConnectionHelper mock_helper_;
  MockAlarmFactory mock_alarm_factory_;
  QuicConfig config_;
  QuicVersionManager version_manager_;
  QuicCryptoServerConfig crypto_config_;
  QuicSocketAddress server_address_;
  // Set to false if the dispatcher won't create a session.
  bool expect_generator_is_called_ = true;
  // Set in conditions where the generator should return a different connection
  // ID.
  std::optional<QuicConnectionId> generated_connection_id_;
  MockConnectionIdGenerator connection_id_generator_;
  std::unique_ptr<NiceMock<TestDispatcher>> dispatcher_;
  MockTimeWaitListManager* time_wait_list_manager_;
  TestQuicSpdyServerSession* session1_;
  TestQuicSpdyServerSession* session2_;
  std::map<QuicConnectionId, std::list<std::string>> data_connection_map_;
  QuicBufferedPacketStore* store_;
  uint64_t connection_id_;
  std::optional<std::string> address_token_;
};

class QuicDispatcherTestAllVersions : public QuicDispatcherTestBase {};
class QuicDispatcherTestOneVersion : public QuicDispatcherTestBase {};

class QuicDispatcherTestNoVersions : public QuicDispatcherTestBase {
 public:
  QuicDispatcherTestNoVersions()
      : QuicDispatcherTestBase(ParsedQuicVersionVector{}) {}
};

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsAllVersions,
                         QuicDispatcherTestAllVersions,
                         ::testing::ValuesIn(CurrentSupportedVersions()),
                         ::testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsOneVersion,
                         QuicDispatcherTestOneVersion,
                         ::testing::Values(CurrentSupportedVersions().front()),
                         ::testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsNoVersion,
                         QuicDispatcherTestNoVersions,
                         ::testing::ValuesIn(AllSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicDispatcherTestAllVersions, TlsClientHelloCreatesSession) {
  if (version_.UsesQuicCrypto()) {
    return;
  }
  SetAddressToken("hsdifghdsaifnasdpfjdsk");

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(
      *dispatcher_,
      CreateQuicSession(TestConnectionId(1), _, client_address,
                        Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              OnParsedClientHelloInfo(MatchParsedClientHello()))
      .Times(1);

  ProcessFirstFlight(client_address, TestConnectionId(1));
}

TEST_P(QuicDispatcherTestAllVersions,
       TlsClientHelloCreatesSessionWithCorrectConnectionIdGenerator) {
  if (version_.UsesQuicCrypto()) {
    return;
  }
  SetAddressToken("hsdifghdsaifnasdpfjdsk");

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  MockConnectionIdGenerator mock_connection_id_generator;
  EXPECT_CALL(*dispatcher_, ConnectionIdGenerator())
      .WillRepeatedly(ReturnRef(mock_connection_id_generator));
  ConnectionIdGeneratorInterface& expected_generator =
      mock_connection_id_generator;
  EXPECT_CALL(mock_connection_id_generator,
              MaybeReplaceConnectionId(TestConnectionId(1), version_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), _, client_address,
                                Eq(ExpectedAlpn()), _, MatchParsedClientHello(),
                                Ref(expected_generator)))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  expect_generator_is_called_ = false;
  ProcessFirstFlight(client_address, TestConnectionId(1));
}

TEST_P(QuicDispatcherTestAllVersions, VariableServerConnectionIdLength) {
  QuicConnectionId old_id = TestConnectionId(1);
  // Return a connection ID that is not expected_server_connection_id_length_
  // bytes long.
  if (version_.HasIetfQuicFrames()) {
    generated_connection_id_ =
        QuicConnectionId({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                          0x09, 0x0a, 0x0b});
  }
  QuicConnectionId new_id =
      generated_connection_id_.has_value() ? *generated_connection_id_ : old_id;
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(new_id, _, client_address, Eq(ExpectedAlpn()),
                                _, MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, new_id, client_address, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, old_id);

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1);
  ProcessPacket(client_address, new_id, false, "foo");
}

void QuicDispatcherTestBase::TestTlsMultiPacketClientHello(
    bool add_reordering, bool long_connection_id) {
  if (!version_.UsesTls()) {
    return;
  }
  SetAddressToken("857293462398");

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId original_connection_id, new_connection_id;
  if (long_connection_id) {
    original_connection_id = TestConnectionIdNineBytesLong(1);
    new_connection_id = kReturnConnectionId;
    EXPECT_CALL(connection_id_generator_,
                MaybeReplaceConnectionId(original_connection_id, version_))
        .WillOnce(Return(new_connection_id));

  } else {
    original_connection_id = TestConnectionId();
    new_connection_id = original_connection_id;
    EXPECT_CALL(connection_id_generator_,
                MaybeReplaceConnectionId(original_connection_id, version_))
        .WillOnce(Return(std::nullopt));
  }
  QuicConfig client_config = DefaultQuicConfig();
  // Add a 2000-byte custom parameter to increase the length of the CHLO.
  constexpr auto kCustomParameterId =
      static_cast<TransportParameters::TransportParameterId>(0xff33);
  std::string kCustomParameterValue(2000, '-');
  client_config.custom_transport_parameters_to_send()[kCustomParameterId] =
      kCustomParameterValue;
  std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
      GetFirstFlightOfPackets(version_, client_config, original_connection_id,
                              EmptyQuicConnectionId(),
                              TestClientCryptoConfig());
  ASSERT_EQ(packets.size(), 2u);
  if (add_reordering) {
    std::swap(packets[0], packets[1]);
  }

  // Processing the first packet should not create a new session.
  ProcessReceivedPacket(std::move(packets[0]), client_address, version_,
                        original_connection_id);

  EXPECT_EQ(dispatcher_->NumSessions(), 0u)
      << "No session should be created before the rest of the CHLO arrives.";

  // Processing the second packet should create the new session.
  EXPECT_CALL(
      *dispatcher_,
      CreateQuicSession(new_connection_id, _, client_address,
                        Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, new_connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2);

  ProcessReceivedPacket(std::move(packets[1]), client_address, version_,
                        original_connection_id);
  EXPECT_EQ(dispatcher_->NumSessions(), 1u);
}

TEST_P(QuicDispatcherTestAllVersions, TlsMultiPacketClientHello) {
  TestTlsMultiPacketClientHello(/*add_reordering=*/false,
                                /*long_connection_id=*/false);
}

TEST_P(QuicDispatcherTestAllVersions, TlsMultiPacketClientHelloWithReordering) {
  TestTlsMultiPacketClientHello(/*add_reordering=*/true,
                                /*long_connection_id=*/false);
}

TEST_P(QuicDispatcherTestAllVersions, TlsMultiPacketClientHelloWithLongId) {
  TestTlsMultiPacketClientHello(/*add_reordering=*/false,
                                /*long_connection_id=*/true);
}

TEST_P(QuicDispatcherTestAllVersions,
       TlsMultiPacketClientHelloWithReorderingAndLongId) {
  TestTlsMultiPacketClientHello(/*add_reordering=*/true,
                                /*long_connection_id=*/true);
}

TEST_P(QuicDispatcherTestAllVersions, ProcessPackets) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(
      *dispatcher_,
      CreateQuicSession(TestConnectionId(1), _, client_address,
                        Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  EXPECT_CALL(
      *dispatcher_,
      CreateQuicSession(TestConnectionId(2), _, client_address,
                        Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(2), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(2), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(2));

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessPacket(client_address, TestConnectionId(1), false, "data");
}

// Regression test of b/93325907.
TEST_P(QuicDispatcherTestAllVersions, DispatcherDoesNotRejectPacketNumberZero) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  // Verify both packets 1 and 2 are processed by connection 1.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2)
      .WillRepeatedly(
          WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
            ValidatePacket(TestConnectionId(1), packet);
          })));
  ProcessFirstFlight(client_address, TestConnectionId(1));
  // Packet number 256 with packet number length 1 would be considered as 0 in
  // dispatcher.
  ProcessPacket(client_address, TestConnectionId(1), false, version_, "", true,
                CONNECTION_ID_PRESENT, PACKET_1BYTE_PACKET_NUMBER, 256);
}

TEST_P(QuicDispatcherTestOneVersion, StatelessVersionNegotiation) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(TestConnectionId(1), _, _, _, _, _, _, _))
      .Times(1);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     TestConnectionId(1));
}

TEST_P(QuicDispatcherTestOneVersion,
       StatelessVersionNegotiationWithVeryLongConnectionId) {
  QuicConnectionId connection_id = QuicUtils::CreateRandomConnectionId(33);
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(connection_id, _, _, _, _, _, _, _))
      .Times(1);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     connection_id);
}

TEST_P(QuicDispatcherTestOneVersion,
       StatelessVersionNegotiationWithClientConnectionId) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(
                  TestConnectionId(1), TestConnectionId(2), _, _, _, _, _, _))
      .Times(1);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     TestConnectionId(1), TestConnectionId(2));
}

TEST_P(QuicDispatcherTestOneVersion, NoVersionNegotiationWithSmallPacket) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
      .Times(0);
  std::string chlo = SerializeCHLO() + std::string(1200, 'a');
  // Truncate to 1100 bytes of payload which results in a packet just
  // under 1200 bytes after framing, packet, and encryption overhead.
  QUICHE_DCHECK_LE(1200u, chlo.length());
  std::string truncated_chlo = chlo.substr(0, 1100);
  QUICHE_DCHECK_EQ(1100u, truncated_chlo.length());
  ProcessPacket(client_address, TestConnectionId(1), true,
                QuicVersionReservedForNegotiation(), truncated_chlo, false,
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER, 1);
}

TEST_P(QuicDispatcherTestOneVersion,
       NoVersionNegotiationWithVersionNegotiationPacket) {
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  ParsedQuicVersionVector supported_versions;
  for (QuicByteCount i = 0; i < kMinPacketSizeForVersionNegotiation; i += 4) {
    supported_versions.push_back(ParsedQuicVersion::RFCv1());
  }

  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          TestConnectionId(), EmptyQuicConnectionId(), /*ietf_quic=*/true,
          version_.HasLengthPrefixedConnectionIds(), supported_versions));
  ASSERT_GT(packet->length(), kMinPacketSizeForVersionNegotiation);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
      .Times(0);

  dispatcher_->ProcessPacket(
      server_address_, client_address,
      QuicReceivedPacket(packet->data(), packet->length(), QuicTime::Zero(),
                         /*owns_buffer=*/false));
}

// Disabling CHLO size validation allows the dispatcher to send version
// negotiation packets in response to a CHLO that is otherwise too small.
TEST_P(QuicDispatcherTestOneVersion,
       VersionNegotiationWithoutChloSizeValidation) {
  crypto_config_.set_validate_chlo_size(false);

  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(_, _, _, _, _, _, _, _))
      .Times(1);
  std::string chlo = SerializeCHLO() + std::string(1200, 'a');
  // Truncate to 1100 bytes of payload which results in a packet just
  // under 1200 bytes after framing, packet, and encryption overhead.
  QUICHE_DCHECK_LE(1200u, chlo.length());
  std::string truncated_chlo = chlo.substr(0, 1100);
  QUICHE_DCHECK_EQ(1100u, truncated_chlo.length());
  ProcessPacket(client_address, TestConnectionId(1), true,
                QuicVersionReservedForNegotiation(), truncated_chlo, true,
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER, 1);
}

TEST_P(QuicDispatcherTestAllVersions, Shutdown) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, client_address,
                                              Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));

  ProcessFirstFlight(client_address, TestConnectionId(1));

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              CloseConnection(QUIC_PEER_GOING_AWAY, _, _));

  dispatcher_->Shutdown();
}

TEST_P(QuicDispatcherTestAllVersions, TimeWaitListManager) {
  CreateTimeWaitListManager();

  // Create a new session.
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(connection_id, _, client_address,
                                              Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, connection_id, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));

  ProcessFirstFlight(client_address, connection_id);

  // Now close the connection, which should add it to the time wait list.
  session1_->connection()->CloseConnection(
      QUIC_INVALID_VERSION,
      "Server: Packet 2 without version flag before version negotiated.",
      ConnectionCloseBehavior::SILENT_CLOSE);
  EXPECT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(connection_id));

  // Dispatcher forwards subsequent packets for this connection_id to the time
  // wait list manager.
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _, _))
      .Times(1);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  ProcessPacket(client_address, connection_id, true, "data");
}

TEST_P(QuicDispatcherTestAllVersions, NoVersionPacketToTimeWaitListManager) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);
  // Dispatcher forwards all packets for this connection_id to the time wait
  // list manager.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              ProcessPacket(_, _, connection_id, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(1);
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data");
}

TEST_P(QuicDispatcherTestAllVersions,
       DonotTimeWaitPacketsWithUnknownConnectionIdAndNoVersion) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();

  uint8_t short_packet[22] = {0x70, 0xa7, 0x02, 0x6b};
  uint8_t valid_size_packet[23] = {0x70, 0xa7, 0x02, 0x6c};
  size_t short_packet_len = 21;
  QuicReceivedPacket packet(reinterpret_cast<char*>(short_packet),
                            short_packet_len, QuicTime::Zero());
  QuicReceivedPacket packet2(reinterpret_cast<char*>(valid_size_packet),
                             short_packet_len + 1, QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  // Verify small packet is silently dropped.
  EXPECT_CALL(connection_id_generator_, ConnectionIdLength(0xa7))
      .WillOnce(Return(kQuicDefaultConnectionIdLength));
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(0);
  dispatcher_->ProcessPacket(server_address_, client_address, packet);
  EXPECT_CALL(connection_id_generator_, ConnectionIdLength(0xa7))
      .WillOnce(Return(kQuicDefaultConnectionIdLength));
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, packet2);
}

TEST_P(QuicDispatcherTestOneVersion, DropPacketWithInvalidFlags) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t all_zero_packet[1200] = {};
  QuicReceivedPacket packet(reinterpret_cast<char*>(all_zero_packet),
                            sizeof(all_zero_packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(connection_id_generator_, ConnectionIdLength(_))
      .WillOnce(Return(kQuicDefaultConnectionIdLength));
  dispatcher_->ProcessPacket(server_address_, client_address, packet);
}

TEST_P(QuicDispatcherTestAllVersions, LimitResetsToSameClientAddress) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress client_address2(QuicIpAddress::Loopback4(), 2);
  QuicSocketAddress client_address3(QuicIpAddress::Loopback6(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);

  // Verify only one reset is sent to the address, although multiple packets
  // are received.
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(1);
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data");
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data2");
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data3");

  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(2);
  ProcessPacket(client_address2, connection_id, /*has_version_flag=*/false,
                "data");
  ProcessPacket(client_address3, connection_id, /*has_version_flag=*/false,
                "data");
}

TEST_P(QuicDispatcherTestAllVersions,
       StopSendingResetOnTooManyRecentAddresses) {
  SetQuicFlag(quic_max_recent_stateless_reset_addresses, 2);
  const size_t kTestLifeTimeMs = 10;
  SetQuicFlag(quic_recent_stateless_reset_addresses_lifetime_ms,
              kTestLifeTimeMs);
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicSocketAddress client_address2(QuicIpAddress::Loopback4(), 2);
  QuicSocketAddress client_address3(QuicIpAddress::Loopback6(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);

  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(2);
  EXPECT_FALSE(GetClearResetAddressesAlarm()->IsSet());
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data");
  const QuicTime expected_deadline =
      mock_helper_.GetClock()->Now() +
      QuicTime::Delta::FromMilliseconds(kTestLifeTimeMs);
  ASSERT_TRUE(GetClearResetAddressesAlarm()->IsSet());
  EXPECT_EQ(expected_deadline, GetClearResetAddressesAlarm()->deadline());
  // Received no version packet 2 after 5ms.
  mock_helper_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  ProcessPacket(client_address2, connection_id, /*has_version_flag=*/false,
                "data");
  ASSERT_TRUE(GetClearResetAddressesAlarm()->IsSet());
  // Verify deadline does not change.
  EXPECT_EQ(expected_deadline, GetClearResetAddressesAlarm()->deadline());
  // Verify reset gets throttled since there are too many recent addresses.
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(0);
  ProcessPacket(client_address3, connection_id, /*has_version_flag=*/false,
                "data");

  mock_helper_.AdvanceTime(QuicTime::Delta::FromMilliseconds(5));
  GetClearResetAddressesAlarm()->Fire();
  EXPECT_CALL(*time_wait_list_manager_, SendPublicReset(_, _, _, _, _, _))
      .Times(2);
  ProcessPacket(client_address, connection_id, /*has_version_flag=*/false,
                "data");
  ProcessPacket(client_address2, connection_id, /*has_version_flag=*/false,
                "data");
  ProcessPacket(client_address3, connection_id, /*has_version_flag=*/false,
                "data");
}

// Makes sure nine-byte connection IDs are replaced by 8-byte ones.
TEST_P(QuicDispatcherTestAllVersions, LongConnectionIdLengthReplaced) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    // When variable length connection IDs are not supported, the connection
    // fails. See StrayPacketTruncatedConnectionId.
    return;
  }
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  QuicConnectionId bad_connection_id = TestConnectionIdNineBytesLong(2);
  generated_connection_id_ = kReturnConnectionId;

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(*generated_connection_id_, _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, *generated_connection_id_, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, bad_connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(bad_connection_id, packet);
          })));
  ProcessFirstFlight(client_address, bad_connection_id);
}

// Makes sure TestConnectionId(1) creates a new connection and
// TestConnectionIdNineBytesLong(2) gets replaced.
TEST_P(QuicDispatcherTestAllVersions, MixGoodAndBadConnectionIdLengthPackets) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    return;
  }

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId bad_connection_id = TestConnectionIdNineBytesLong(2);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  generated_connection_id_ = kReturnConnectionId;
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(*generated_connection_id_, _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, *generated_connection_id_, client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(
          Invoke([this, bad_connection_id](const QuicEncryptedPacket& packet) {
            ValidatePacket(bad_connection_id, packet);
          })));
  ProcessFirstFlight(client_address, bad_connection_id);

  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessPacket(client_address, TestConnectionId(1), false, "data");
}

TEST_P(QuicDispatcherTestAllVersions, ProcessPacketWithZeroPort) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 0);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(TestConnectionId(1), _,
                                              client_address, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  ProcessPacket(client_address, TestConnectionId(1), /*has_version_flag=*/true,
                "data");
}

TEST_P(QuicDispatcherTestAllVersions, ProcessPacketWithBlockedPort) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 17);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(TestConnectionId(1), _,
                                              client_address, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  ProcessPacket(client_address, TestConnectionId(1), /*has_version_flag=*/true,
                "data");
}

TEST_P(QuicDispatcherTestAllVersions, ProcessPacketWithNonBlockedPort) {
  CreateTimeWaitListManager();

  // Port 443 must not be blocked because it might be useful for proxies to send
  // proxied traffic with source port 443 as that allows building a full QUIC
  // proxy using a single UDP socket.
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 443);

  // dispatcher_ should not drop this packet.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  ProcessFirstFlight(client_address, TestConnectionId(1));
}

TEST_P(QuicDispatcherTestAllVersions,
       DropPacketWithKnownVersionAndInvalidShortInitialConnectionId) {
  if (!version_.AllowsVariableLengthConnectionIds()) {
    return;
  }
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  // dispatcher_ should drop this packet.
  EXPECT_CALL(connection_id_generator_, ConnectionIdLength(0x00))
      .WillOnce(Return(10));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(client_address, EmptyQuicConnectionId());
}

TEST_P(QuicDispatcherTestAllVersions,
       DropPacketWithKnownVersionAndInvalidInitialConnectionId) {
  CreateTimeWaitListManager();

  QuicSocketAddress server_address;
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  // dispatcher_ should drop this packet with invalid connection ID.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  absl::string_view cid_str = "123456789abcdefg123456789abcdefg";
  QuicConnectionId invalid_connection_id(cid_str.data(), cid_str.length());
  QuicReceivedPacket packet("packet", 6, QuicTime::Zero());
  ReceivedPacketInfo packet_info(server_address, client_address, packet);
  packet_info.version_flag = true;
  packet_info.version = version_;
  packet_info.destination_connection_id = invalid_connection_id;

  ASSERT_TRUE(dispatcher_->MaybeDispatchPacket(packet_info));
}

void QuicDispatcherTestBase::
    TestVersionNegotiationForUnknownVersionInvalidShortInitialConnectionId(
        const QuicConnectionId& server_connection_id,
        const QuicConnectionId& client_connection_id) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(
                  server_connection_id, client_connection_id,
                  /*ietf_quic=*/true,
                  /*use_length_prefix=*/true, _, _, client_address, _))
      .Times(1);
  expect_generator_is_called_ = false;
  EXPECT_CALL(connection_id_generator_, ConnectionIdLength(_)).Times(0);
  ProcessFirstFlight(ParsedQuicVersion::ReservedForNegotiation(),
                     client_address, server_connection_id,
                     client_connection_id);
}

TEST_P(QuicDispatcherTestOneVersion,
       VersionNegotiationForUnknownVersionInvalidShortInitialConnectionId) {
  TestVersionNegotiationForUnknownVersionInvalidShortInitialConnectionId(
      EmptyQuicConnectionId(), EmptyQuicConnectionId());
}

TEST_P(QuicDispatcherTestOneVersion,
       VersionNegotiationForUnknownVersionInvalidShortInitialConnectionId2) {
  char server_connection_id_bytes[3] = {1, 2, 3};
  QuicConnectionId server_connection_id(server_connection_id_bytes,
                                        sizeof(server_connection_id_bytes));
  TestVersionNegotiationForUnknownVersionInvalidShortInitialConnectionId(
      server_connection_id, EmptyQuicConnectionId());
}

TEST_P(QuicDispatcherTestOneVersion,
       VersionNegotiationForUnknownVersionInvalidShortInitialConnectionId3) {
  char client_connection_id_bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  QuicConnectionId client_connection_id(client_connection_id_bytes,
                                        sizeof(client_connection_id_bytes));
  TestVersionNegotiationForUnknownVersionInvalidShortInitialConnectionId(
      EmptyQuicConnectionId(), client_connection_id);
}

TEST_P(QuicDispatcherTestOneVersion, VersionsChangeInFlight) {
  VerifyVersionNotSupported(QuicVersionReservedForNegotiation());
  for (ParsedQuicVersion version : CurrentSupportedVersions()) {
    VerifyVersionSupported(version);
    QuicDisableVersion(version);
    VerifyVersionNotSupported(version);
    QuicEnableVersion(version);
    VerifyVersionSupported(version);
  }
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionDraft28WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 0xFF, 0x00, 0x00, 28, /*destination connection ID length*/ 0x08};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionDraft27WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 0xFF, 0x00, 0x00, 27, /*destination connection ID length*/ 0x08};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionDraft25WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 0xFF, 0x00, 0x00, 25, /*destination connection ID length*/ 0x08};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionT050WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 'T', '0', '5', '0', /*destination connection ID length*/ 0x08};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionQ049WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 'Q', '0', '4', '9', /*destination connection ID length*/ 0x08};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionQ048WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 'Q', '0', '4', '8', /*connection ID length byte*/ 0x50};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/false, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionQ047WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 'Q', '0', '4', '7', /*connection ID length byte*/ 0x50};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/false, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionQ045WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xC0, 'Q', '0', '4', '5', /*connection ID length byte*/ 0x50};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     ABSL_ARRAYSIZE(packet), QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/false, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionQ044WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet44[kMinPacketSizeForVersionNegotiation] = {
      0xFF, 'Q', '0', '4', '4', /*connection ID length byte*/ 0x50};
  QuicReceivedPacket received_packet44(reinterpret_cast<char*>(packet44),
                                       kMinPacketSizeForVersionNegotiation,
                                       QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/false, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address,
                             received_packet44);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionQ050WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xFF, 'Q', '0', '5', '0', /*connection ID length byte*/ 0x50};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     kMinPacketSizeForVersionNegotiation,
                                     QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

TEST_P(QuicDispatcherTestOneVersion,
       RejectDeprecatedVersionT051WithVersionNegotiation) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  uint8_t packet[kMinPacketSizeForVersionNegotiation] = {
      0xFF, 'T', '0', '5', '1', /*destination connection ID length*/ 0x08};
  QuicReceivedPacket received_packet(reinterpret_cast<char*>(packet),
                                     kMinPacketSizeForVersionNegotiation,
                                     QuicTime::Zero());
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(_, _, /*ietf_quic=*/true,
                                   /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  dispatcher_->ProcessPacket(server_address_, client_address, received_packet);
}

static_assert(quic::SupportedVersions().size() == 4u,
              "Please add new RejectDeprecatedVersion tests above this assert "
              "when deprecating versions");

TEST_P(QuicDispatcherTestOneVersion, VersionNegotiationProbe) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  CreateTimeWaitListManager();
  char packet[1200];
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  QuicEncryptedPacket encrypted(packet, sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  QuicConnectionId client_connection_id = EmptyQuicConnectionId();
  QuicConnectionId server_connection_id(
      destination_connection_id_bytes, sizeof(destination_connection_id_bytes));
  EXPECT_CALL(*time_wait_list_manager_,
              SendVersionNegotiationPacket(
                  server_connection_id, client_connection_id,
                  /*ietf_quic=*/true, /*use_length_prefix=*/true, _, _, _, _))
      .Times(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);

  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
}

// Testing packet writer that saves all packets instead of sending them.
// Useful for tests that need access to sent packets.
class SavingWriter : public QuicPacketWriterWrapper {
 public:
  bool IsWriteBlocked() const override { return false; }

  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& /*self_client_address*/,
                          const QuicSocketAddress& /*peer_client_address*/,
                          PerPacketOptions* /*options*/,
                          const QuicPacketWriterParams& /*params*/) override {
    packets_.push_back(
        QuicEncryptedPacket(buffer, buf_len, /*owns_buffer=*/false).Clone());
    return WriteResult(WRITE_STATUS_OK, buf_len);
  }

  std::vector<std::unique_ptr<QuicEncryptedPacket>>* packets() {
    return &packets_;
  }

 private:
  std::vector<std::unique_ptr<QuicEncryptedPacket>> packets_;
};

TEST_P(QuicDispatcherTestOneVersion, VersionNegotiationProbeEndToEnd) {
  SavingWriter* saving_writer = new SavingWriter();
  // dispatcher_ takes ownership of saving_writer.
  QuicDispatcherPeer::UseWriter(dispatcher_.get(), saving_writer);

  QuicTimeWaitListManager* time_wait_list_manager = new QuicTimeWaitListManager(
      saving_writer, dispatcher_.get(), mock_helper_.GetClock(),
      &mock_alarm_factory_);
  // dispatcher_ takes ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                             time_wait_list_manager);
  char packet[1200] = {};
  char destination_connection_id_bytes[] = {0x56, 0x4e, 0x20, 0x70,
                                            0x6c, 0x7a, 0x20, 0x21};
  EXPECT_TRUE(QuicFramer::WriteClientVersionNegotiationProbePacket(
      packet, sizeof(packet), destination_connection_id_bytes,
      sizeof(destination_connection_id_bytes)));
  QuicEncryptedPacket encrypted(packet, sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
  ASSERT_EQ(1u, saving_writer->packets()->size());

  char source_connection_id_bytes[255] = {};
  uint8_t source_connection_id_length = sizeof(source_connection_id_bytes);
  std::string detailed_error = "foobar";
  EXPECT_TRUE(QuicFramer::ParseServerVersionNegotiationProbeResponse(
      (*(saving_writer->packets()))[0]->data(),
      (*(saving_writer->packets()))[0]->length(), source_connection_id_bytes,
      &source_connection_id_length, &detailed_error));
  EXPECT_EQ("", detailed_error);

  // The source connection ID of the probe response should match the
  // destination connection ID of the probe request.
  quiche::test::CompareCharArraysWithHexError(
      "parsed probe", source_connection_id_bytes, source_connection_id_length,
      destination_connection_id_bytes, sizeof(destination_connection_id_bytes));
}

TEST_P(QuicDispatcherTestOneVersion, AndroidConformanceTest) {
  // WARNING: do not remove or modify this test without making sure that we
  // still have adequate coverage for the Android conformance test.
  SavingWriter* saving_writer = new SavingWriter();
  // dispatcher_ takes ownership of saving_writer.
  QuicDispatcherPeer::UseWriter(dispatcher_.get(), saving_writer);

  QuicTimeWaitListManager* time_wait_list_manager = new QuicTimeWaitListManager(
      saving_writer, dispatcher_.get(), mock_helper_.GetClock(),
      &mock_alarm_factory_);
  // dispatcher_ takes ownership of time_wait_list_manager.
  QuicDispatcherPeer::SetTimeWaitListManager(dispatcher_.get(),
                                             time_wait_list_manager);
  // clang-format off
  static const unsigned char packet[1200] = {
    // Android UDP network conformance test packet as it was after this change:
    // https://android-review.googlesource.com/c/platform/cts/+/1454515
    0xc0,  // long header
    0xaa, 0xda, 0xca, 0xca,  // reserved-space version number
    0x08,  // destination connection ID length
    0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,  // 8-byte connection ID
    0x00,  // source connection ID length
  };
  // clang-format on

  QuicEncryptedPacket encrypted(reinterpret_cast<const char*>(packet),
                                sizeof(packet), false);
  std::unique_ptr<QuicReceivedPacket> received_packet(
      ConstructReceivedPacket(encrypted, mock_helper_.GetClock()->Now()));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  dispatcher_->ProcessPacket(server_address_, client_address, *received_packet);
  ASSERT_EQ(1u, saving_writer->packets()->size());

  // The Android UDP network conformance test directly checks that these bytes
  // of the response match the connection ID that was sent.
  ASSERT_GE((*(saving_writer->packets()))[0]->length(), 15u);
  quiche::test::CompareCharArraysWithHexError(
      "response connection ID", &(*(saving_writer->packets()))[0]->data()[7], 8,
      reinterpret_cast<const char*>(&packet[6]), 8);
}

TEST_P(QuicDispatcherTestAllVersions, DoNotProcessSmallPacket) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, SendPacket(_, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);
  ProcessPacket(client_address, TestConnectionId(1), /*has_version_flag=*/true,
                version_, SerializeCHLO(), /*full_padding=*/false,
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER, 1);
}

TEST_P(QuicDispatcherTestAllVersions, ProcessSmallCoalescedPacket) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*time_wait_list_manager_, SendPacket(_, _, _)).Times(0);

  // clang-format off
  uint8_t coalesced_packet[1200] = {
    // first coalesced packet
      // public flags (long header with packet type INITIAL and
      // 4-byte packet number)
      0xC3,
      // version
      'Q', '0', '9', '9',
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x05,
      // packet number
      0x12, 0x34, 0x56, 0x78,
      // Padding
      0x00,
    // second coalesced packet
      // public flags (long header with packet type ZERO_RTT_PROTECTED and
      // 4-byte packet number)
      0xC3,
      // version
      'Q', '0', '9', '9',
      // destination connection ID length
      0x08,
      // destination connection ID
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
      // source connection ID length
      0x00,
      // long header packet length
      0x1E,
      // packet number
      0x12, 0x34, 0x56, 0x79,
  };
  // clang-format on
  QuicReceivedPacket packet(reinterpret_cast<char*>(coalesced_packet), 1200,
                            QuicTime::Zero());
  dispatcher_->ProcessPacket(server_address_, client_address, packet);
}

TEST_P(QuicDispatcherTestAllVersions, StopAcceptingNewConnections) {
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(1));

  dispatcher_->StopAcceptingNewConnections();
  EXPECT_FALSE(dispatcher_->accept_new_connections());

  // No more new connections afterwards.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(2), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .Times(0u);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(client_address, TestConnectionId(2));

  // Existing connections should be able to continue.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(1u)
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessPacket(client_address, TestConnectionId(1), false, "data");
}

TEST_P(QuicDispatcherTestAllVersions, StartAcceptingNewConnections) {
  dispatcher_->StopAcceptingNewConnections();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  // No more new connections afterwards.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(2), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .Times(0u);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(client_address, TestConnectionId(2));

  dispatcher_->StartAcceptingNewConnections();
  EXPECT_TRUE(dispatcher_->accept_new_connections());

  expect_generator_is_called_ = true;
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(1), _, client_address,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(1), client_address,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
        ValidatePacket(TestConnectionId(1), packet);
      })));
  ProcessFirstFlight(client_address, TestConnectionId(1));
}

TEST_P(QuicDispatcherTestOneVersion, SelectAlpn) {
  EXPECT_EQ(QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {}), "");
  EXPECT_EQ(QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {""}), "");
  EXPECT_EQ(QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {"hq"}), "hq");
  // Q033 is no longer supported but Q046 is.
  QuicEnableVersion(ParsedQuicVersion::Q046());
  EXPECT_EQ(
      QuicDispatcherPeer::SelectAlpn(dispatcher_.get(), {"h3-Q033", "h3-Q046"}),
      "h3-Q046");
}

TEST_P(QuicDispatcherTestNoVersions, VersionNegotiationFromReservedVersion) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(TestConnectionId(1), _, _, _, _, _, _, _))
      .Times(1);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(QuicVersionReservedForNegotiation(), client_address,
                     TestConnectionId(1));
}

TEST_P(QuicDispatcherTestNoVersions, VersionNegotiationFromRealVersion) {
  CreateTimeWaitListManager();
  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(
      *time_wait_list_manager_,
      SendVersionNegotiationPacket(TestConnectionId(1), _, _, _, _, _, _, _))
      .Times(1);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(version_, client_address, TestConnectionId(1));
}

// Verify the stopgap test: Packets with truncated connection IDs should be
// dropped.
class QuicDispatcherTestStrayPacketConnectionId
    : public QuicDispatcherTestBase {};

INSTANTIATE_TEST_SUITE_P(QuicDispatcherTestsStrayPacketConnectionId,
                         QuicDispatcherTestStrayPacketConnectionId,
                         ::testing::ValuesIn(CurrentSupportedVersions()),
                         ::testing::PrintToStringParamName());

// Packets with truncated connection IDs should be dropped.
TEST_P(QuicDispatcherTestStrayPacketConnectionId,
       StrayPacketTruncatedConnectionId) {
  CreateTimeWaitListManager();

  QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
  QuicConnectionId connection_id = TestConnectionId(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, _, _, _, _, _)).Times(0);
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*time_wait_list_manager_, AddConnectionIdToTimeWait(_, _))
      .Times(0);

  ProcessPacket(client_address, connection_id, true, "data",
                CONNECTION_ID_ABSENT, PACKET_4BYTE_PACKET_NUMBER);
}

class BlockingWriter : public QuicPacketWriterWrapper {
 public:
  BlockingWriter() : write_blocked_(false) {}

  bool IsWriteBlocked() const override { return write_blocked_; }
  void SetWritable() override { write_blocked_ = false; }

  WriteResult WritePacket(const char* /*buffer*/, size_t /*buf_len*/,
                          const QuicIpAddress& /*self_client_address*/,
                          const QuicSocketAddress& /*peer_client_address*/,
                          PerPacketOptions* /*options*/,
                          const QuicPacketWriterParams& /*params*/) override {
    // It would be quite possible to actually implement this method here with
    // the fake blocked status, but it would be significantly more work in
    // Chromium, and since it's not called anyway, don't bother.
    QUIC_LOG(DFATAL) << "Not supported";
    return WriteResult();
  }

  bool write_blocked_;
};

class QuicDispatcherWriteBlockedListTest : public QuicDispatcherTestBase {
 public:
  void SetUp() override {
    QuicDispatcherTestBase::SetUp();
    writer_ = new BlockingWriter;
    QuicDispatcherPeer::UseWriter(dispatcher_.get(), writer_);

    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);

    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, client_address,
                                                Eq(ExpectedAlpn()), _, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(1), client_address,
            &helper_, &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(TestConnectionId(1), packet);
        })));
    ProcessFirstFlight(client_address, TestConnectionId(1));

    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, client_address,
                                                Eq(ExpectedAlpn()), _, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(2), client_address,
            &helper_, &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(TestConnectionId(2), packet);
        })));
    ProcessFirstFlight(client_address, TestConnectionId(2));

    blocked_list_ = QuicDispatcherPeer::GetWriteBlockedList(dispatcher_.get());
  }

  void TearDown() override {
    if (connection1() != nullptr) {
      EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
    }

    if (connection2() != nullptr) {
      EXPECT_CALL(*connection2(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
    }
    dispatcher_->Shutdown();
  }

  // Set the dispatcher's writer to be blocked. By default, all connections use
  // the same writer as the dispatcher in this test.
  void SetBlocked() {
    QUIC_LOG(INFO) << "set writer " << writer_ << " to blocked";
    writer_->write_blocked_ = true;
  }

  // Simulate what happens when connection1 gets blocked when writing.
  void BlockConnection1() {
    Connection1Writer()->write_blocked_ = true;
    dispatcher_->OnWriteBlocked(connection1());
  }

  BlockingWriter* Connection1Writer() {
    return static_cast<BlockingWriter*>(connection1()->writer());
  }

  // Simulate what happens when connection2 gets blocked when writing.
  void BlockConnection2() {
    Connection2Writer()->write_blocked_ = true;
    dispatcher_->OnWriteBlocked(connection2());
  }

  BlockingWriter* Connection2Writer() {
    return static_cast<BlockingWriter*>(connection2()->writer());
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  BlockingWriter* writer_;
  QuicBlockedWriterList* blocked_list_;
};

INSTANTIATE_TEST_SUITE_P(QuicDispatcherWriteBlockedListTests,
                         QuicDispatcherWriteBlockedListTest,
                         ::testing::Values(CurrentSupportedVersions().front()),
                         ::testing::PrintToStringParamName());

TEST_P(QuicDispatcherWriteBlockedListTest, BasicOnCanWrite) {
  // No OnCanWrite calls because no connections are blocked.
  dispatcher_->OnCanWrite();

  // Register connection 1 for events, and make sure it's notified.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // It should get only one notification.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteOrder) {
  // Make sure we handle events in order.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // Check the other ordering.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection2());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  EXPECT_CALL(*connection1(), OnCanWrite());
  dispatcher_->OnCanWrite();
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteRemove) {
  // Add and remove one connction.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->Remove(*connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // Add and remove one connction and make sure it doesn't affect others.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  blocked_list_->Remove(*connection1());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // Add it, remove it, and add it back and make sure things are OK.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->Remove(*connection1());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_->OnCanWrite();
}

TEST_P(QuicDispatcherWriteBlockedListTest, DoubleAdd) {
  // Make sure a double add does not necessitate a double remove.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  blocked_list_->Remove(*connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();

  // Make sure a double add does not result in two OnCanWrite calls.
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection1());
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(1);
  dispatcher_->OnCanWrite();
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlockConnection1) {
  // If the 1st blocked writer gets blocked in OnCanWrite, it will be added back
  // into the write blocked list.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection1));
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();

  // connection1 should be still in the write blocked list.
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, connection1 should get its second chance.
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite()).Times(0);
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest, OnCanWriteHandleBlockConnection2) {
  // If the 2nd blocked writer gets blocked in OnCanWrite, it will be added back
  // into the write blocked list.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_->OnCanWrite();

  // connection2 should be still in the write blocked list.
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, connection2 should get its second chance.
  EXPECT_CALL(*connection1(), OnCanWrite()).Times(0);
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest,
       OnCanWriteHandleBlockBothConnections) {
  // Both connections get blocked in OnCanWrite, and added back into the write
  // blocked list.
  InSequence s;
  SetBlocked();
  dispatcher_->OnWriteBlocked(connection1());
  dispatcher_->OnWriteBlocked(connection2());
  EXPECT_CALL(*connection1(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection1));
  EXPECT_CALL(*connection2(), OnCanWrite())
      .WillOnce(
          Invoke(this, &QuicDispatcherWriteBlockedListTest::BlockConnection2));
  dispatcher_->OnCanWrite();

  // Both connections should be still in the write blocked list.
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  // Now call OnCanWrite again, both connections should get its second chance.
  EXPECT_CALL(*connection1(), OnCanWrite());
  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest, PerConnectionWriterBlocked) {
  // By default, all connections share the same packet writer with the
  // dispatcher.
  EXPECT_EQ(dispatcher_->writer(), connection1()->writer());
  EXPECT_EQ(dispatcher_->writer(), connection2()->writer());

  // Test the case where connection1 shares the same packet writer as the
  // dispatcher, whereas connection2 owns it's packet writer.
  // Change connection2's writer.
  connection2()->SetQuicPacketWriter(new BlockingWriter, /*owns_writer=*/true);
  EXPECT_NE(dispatcher_->writer(), connection2()->writer());

  BlockConnection2();
  EXPECT_TRUE(dispatcher_->HasPendingWrites());

  EXPECT_CALL(*connection2(), OnCanWrite());
  dispatcher_->OnCanWrite();
  EXPECT_FALSE(dispatcher_->HasPendingWrites());
}

TEST_P(QuicDispatcherWriteBlockedListTest,
       RemoveConnectionFromWriteBlockedListWhenDeletingSessions) {
  EXPECT_QUIC_BUG(
      {
        dispatcher_->OnConnectionClosed(
            connection1()->connection_id(), QUIC_PACKET_WRITE_ERROR,
            "Closed by test.", ConnectionCloseSource::FROM_SELF);

        SetBlocked();

        ASSERT_FALSE(dispatcher_->HasPendingWrites());
        SetBlocked();
        dispatcher_->OnWriteBlocked(connection1());
        ASSERT_TRUE(dispatcher_->HasPendingWrites());

        dispatcher_->DeleteSessions();
        MarkSession1Deleted();
      },
      "QuicConnection was in WriteBlockedList before destruction");
}

class QuicDispatcherSupportMultipleConnectionIdPerConnectionTest
    : public QuicDispatcherTestBase {
 public:
  QuicDispatcherSupportMultipleConnectionIdPerConnectionTest()
      : QuicDispatcherTestBase(crypto_test_utils::ProofSourceForTesting()) {
    dispatcher_ = std::make_unique<NiceMock<TestDispatcher>>(
        &config_, &crypto_config_, &version_manager_,
        mock_helper_.GetRandomGenerator(), connection_id_generator_);
  }
  void AddConnection1() {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 1);
    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, client_address,
                                                Eq(ExpectedAlpn()), _, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(1), client_address,
            &helper_, &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(TestConnectionId(1), packet);
        })));
    ProcessFirstFlight(client_address, TestConnectionId(1));
  }

  void AddConnection2() {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 2);
    EXPECT_CALL(*dispatcher_, CreateQuicSession(_, _, client_address,
                                                Eq(ExpectedAlpn()), _, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(2), client_address,
            &helper_, &alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session2_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session2_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(Invoke([this](const QuicEncryptedPacket& packet) {
          ValidatePacket(TestConnectionId(2), packet);
        })));
    ProcessFirstFlight(client_address, TestConnectionId(2));
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
};

INSTANTIATE_TEST_SUITE_P(
    QuicDispatcherSupportMultipleConnectionIdPerConnectionTests,
    QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
    ::testing::Values(CurrentSupportedVersions().front()),
    ::testing::PrintToStringParamName());

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       FailToAddExistingConnectionId) {
  AddConnection1();
  EXPECT_FALSE(dispatcher_->TryAddNewConnectionId(TestConnectionId(1),
                                                  TestConnectionId(1)));
}

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       TryAddNewConnectionId) {
  AddConnection1();
  ASSERT_EQ(dispatcher_->NumSessions(), 1u);
  ASSERT_THAT(session1_, testing::NotNull());
  MockServerConnection* mock_server_connection1 =
      reinterpret_cast<MockServerConnection*>(connection1());

  {
    mock_server_connection1->AddNewConnectionId(TestConnectionId(3));
    EXPECT_EQ(dispatcher_->NumSessions(), 1u);
    auto* session =
        QuicDispatcherPeer::FindSession(dispatcher_.get(), TestConnectionId(3));
    ASSERT_EQ(session, session1_);
  }

  {
    mock_server_connection1->AddNewConnectionId(TestConnectionId(4));
    EXPECT_EQ(dispatcher_->NumSessions(), 1u);
    auto* session =
        QuicDispatcherPeer::FindSession(dispatcher_.get(), TestConnectionId(4));
    ASSERT_EQ(session, session1_);
  }

  EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
  // Would timed out unless all sessions have been removed from the session map.
  dispatcher_->Shutdown();
}

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       TryAddNewConnectionIdWithCollision) {
  AddConnection1();
  AddConnection2();
  ASSERT_EQ(dispatcher_->NumSessions(), 2u);
  ASSERT_THAT(session1_, testing::NotNull());
  ASSERT_THAT(session2_, testing::NotNull());
  MockServerConnection* mock_server_connection1 =
      reinterpret_cast<MockServerConnection*>(connection1());
  MockServerConnection* mock_server_connection2 =
      reinterpret_cast<MockServerConnection*>(connection2());

  {
    // TestConnectionId(2) is already claimed by connection2 but connection1
    // still thinks it owns it.
    mock_server_connection1->UnconditionallyAddNewConnectionIdForTest(
        TestConnectionId(2));
    EXPECT_EQ(dispatcher_->NumSessions(), 2u);
    auto* session =
        QuicDispatcherPeer::FindSession(dispatcher_.get(), TestConnectionId(2));
    ASSERT_EQ(session, session2_);
    EXPECT_THAT(mock_server_connection1->GetActiveServerConnectionIds(),
                testing::ElementsAre(TestConnectionId(1), TestConnectionId(2)));
  }

  {
    mock_server_connection2->AddNewConnectionId(TestConnectionId(3));
    EXPECT_EQ(dispatcher_->NumSessions(), 2u);
    auto* session =
        QuicDispatcherPeer::FindSession(dispatcher_.get(), TestConnectionId(3));
    ASSERT_EQ(session, session2_);
    EXPECT_THAT(mock_server_connection2->GetActiveServerConnectionIds(),
                testing::ElementsAre(TestConnectionId(2), TestConnectionId(3)));
  }

  // Connection2 removes both TestConnectionId(2) & TestConnectionId(3) from the
  // session map.
  dispatcher_->OnConnectionClosed(TestConnectionId(2),
                                  QuicErrorCode::QUIC_NO_ERROR, "detail",
                                  quic::ConnectionCloseSource::FROM_SELF);
  // QUICHE_BUG fires when connection1 tries to remove TestConnectionId(2)
  // again from the session_map.
  EXPECT_QUICHE_BUG(dispatcher_->OnConnectionClosed(
                        TestConnectionId(1), QuicErrorCode::QUIC_NO_ERROR,
                        "detail", quic::ConnectionCloseSource::FROM_SELF),
                    "Missing session for cid");
}

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       MismatchedSessionAfterAddingCollidedConnectionId) {
  AddConnection1();
  AddConnection2();
  MockServerConnection* mock_server_connection1 =
      reinterpret_cast<MockServerConnection*>(connection1());

  {
    // TestConnectionId(2) is already claimed by connection2 but connection1
    // still thinks it owns it.
    mock_server_connection1->UnconditionallyAddNewConnectionIdForTest(
        TestConnectionId(2));
    EXPECT_EQ(dispatcher_->NumSessions(), 2u);
    auto* session =
        QuicDispatcherPeer::FindSession(dispatcher_.get(), TestConnectionId(2));
    ASSERT_EQ(session, session2_);
    EXPECT_THAT(mock_server_connection1->GetActiveServerConnectionIds(),
                testing::ElementsAre(TestConnectionId(1), TestConnectionId(2)));
  }

  // Connection1 tries to remove both Cid1 & Cid2, but they point to different
  // sessions.
  EXPECT_QUIC_BUG(dispatcher_->OnConnectionClosed(
                      TestConnectionId(1), QuicErrorCode::QUIC_NO_ERROR,
                      "detail", quic::ConnectionCloseSource::FROM_SELF),
                  "Session is mismatched in the map");
}

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       RetireConnectionIdFromSingleConnection) {
  AddConnection1();
  ASSERT_EQ(dispatcher_->NumSessions(), 1u);
  ASSERT_THAT(session1_, testing::NotNull());
  MockServerConnection* mock_server_connection1 =
      reinterpret_cast<MockServerConnection*>(connection1());

  // Adds 1 new connection id every turn and retires 2 connection ids every
  // other turn.
  for (int i = 2; i < 10; ++i) {
    mock_server_connection1->AddNewConnectionId(TestConnectionId(i));
    ASSERT_EQ(
        QuicDispatcherPeer::FindSession(dispatcher_.get(), TestConnectionId(i)),
        session1_);
    ASSERT_EQ(QuicDispatcherPeer::FindSession(dispatcher_.get(),
                                              TestConnectionId(i - 1)),
              session1_);
    EXPECT_EQ(dispatcher_->NumSessions(), 1u);
    if (i % 2 == 1) {
      mock_server_connection1->RetireConnectionId(TestConnectionId(i - 2));
      mock_server_connection1->RetireConnectionId(TestConnectionId(i - 1));
    }
  }

  EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
  // Would timed out unless all sessions have been removed from the session map.
  dispatcher_->Shutdown();
}

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       RetireConnectionIdFromMultipleConnections) {
  AddConnection1();
  AddConnection2();
  ASSERT_EQ(dispatcher_->NumSessions(), 2u);
  MockServerConnection* mock_server_connection1 =
      reinterpret_cast<MockServerConnection*>(connection1());
  MockServerConnection* mock_server_connection2 =
      reinterpret_cast<MockServerConnection*>(connection2());

  for (int i = 2; i < 10; ++i) {
    mock_server_connection1->AddNewConnectionId(TestConnectionId(2 * i - 1));
    mock_server_connection2->AddNewConnectionId(TestConnectionId(2 * i));
    ASSERT_EQ(QuicDispatcherPeer::FindSession(dispatcher_.get(),
                                              TestConnectionId(2 * i - 1)),
              session1_);
    ASSERT_EQ(QuicDispatcherPeer::FindSession(dispatcher_.get(),
                                              TestConnectionId(2 * i)),
              session2_);
    EXPECT_EQ(dispatcher_->NumSessions(), 2u);
    mock_server_connection1->RetireConnectionId(TestConnectionId(2 * i - 3));
    mock_server_connection2->RetireConnectionId(TestConnectionId(2 * i - 2));
  }

  mock_server_connection1->AddNewConnectionId(TestConnectionId(19));
  mock_server_connection2->AddNewConnectionId(TestConnectionId(20));
  EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
  EXPECT_CALL(*connection2(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
  // Would timed out unless all sessions have been removed from the session map.
  dispatcher_->Shutdown();
}

TEST_P(QuicDispatcherSupportMultipleConnectionIdPerConnectionTest,
       TimeWaitListPoplulateCorrectly) {
  QuicTimeWaitListManager* time_wait_list_manager =
      QuicDispatcherPeer::GetTimeWaitListManager(dispatcher_.get());
  AddConnection1();
  MockServerConnection* mock_server_connection1 =
      reinterpret_cast<MockServerConnection*>(connection1());

  mock_server_connection1->AddNewConnectionId(TestConnectionId(2));
  mock_server_connection1->AddNewConnectionId(TestConnectionId(3));
  mock_server_connection1->AddNewConnectionId(TestConnectionId(4));
  mock_server_connection1->RetireConnectionId(TestConnectionId(1));
  mock_server_connection1->RetireConnectionId(TestConnectionId(2));

  EXPECT_CALL(*connection1(), CloseConnection(QUIC_PEER_GOING_AWAY, _, _));
  connection1()->CloseConnection(
      QUIC_PEER_GOING_AWAY, "Close for testing",
      ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);

  EXPECT_FALSE(
      time_wait_list_manager->IsConnectionIdInTimeWait(TestConnectionId(1)));
  EXPECT_FALSE(
      time_wait_list_manager->IsConnectionIdInTimeWait(TestConnectionId(2)));
  EXPECT_TRUE(
      time_wait_list_manager->IsConnectionIdInTimeWait(TestConnectionId(3)));
  EXPECT_TRUE(
      time_wait_list_manager->IsConnectionIdInTimeWait(TestConnectionId(4)));

  dispatcher_->Shutdown();
}

class BufferedPacketStoreTest : public QuicDispatcherTestBase {
 public:
  BufferedPacketStoreTest()
      : QuicDispatcherTestBase(),
        client_addr_(QuicIpAddress::Loopback4(), 1234) {}

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    QuicDispatcherTestBase::ProcessFirstFlight(version, peer_address,
                                               server_connection_id);
  }

  void ProcessFirstFlight(const QuicSocketAddress& peer_address,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version_, peer_address, server_connection_id);
  }

  void ProcessFirstFlight(const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(client_addr_, server_connection_id);
  }

  void ProcessFirstFlight(const ParsedQuicVersion& version,
                          const QuicConnectionId& server_connection_id) {
    ProcessFirstFlight(version, client_addr_, server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const ParsedQuicVersion& version, const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    QuicDispatcherTestBase::ProcessUndecryptableEarlyPacket(
        version, peer_address, server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const QuicSocketAddress& peer_address,
      const QuicConnectionId& server_connection_id) {
    ProcessUndecryptableEarlyPacket(version_, peer_address,
                                    server_connection_id);
  }

  void ProcessUndecryptableEarlyPacket(
      const QuicConnectionId& server_connection_id) {
    ProcessUndecryptableEarlyPacket(version_, client_addr_,
                                    server_connection_id);
  }

 protected:
  QuicSocketAddress client_addr_;
};

INSTANTIATE_TEST_SUITE_P(BufferedPacketStoreTests, BufferedPacketStoreTest,
                         ::testing::ValuesIn(CurrentSupportedVersions()),
                         ::testing::PrintToStringParamName());

TEST_P(BufferedPacketStoreTest, ProcessNonChloPacketBeforeChlo) {
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  // Process non-CHLO packet.
  ProcessUndecryptableEarlyPacket(conn_id);
  EXPECT_EQ(0u, dispatcher_->NumSessions())
      << "No session should be created before CHLO arrives.";

  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(connection_id_generator_,
              MaybeReplaceConnectionId(conn_id, version_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, _, client_addr_, Eq(ExpectedAlpn()), _,
                                MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2)  // non-CHLO + CHLO.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(conn_id, packet);
            }
          })));
  expect_generator_is_called_ = false;
  ProcessFirstFlight(conn_id);
}

TEST_P(BufferedPacketStoreTest, ProcessNonChloPacketsUptoLimitAndProcessChlo) {
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  for (size_t i = 1; i <= kDefaultMaxUndecryptablePackets + 1; ++i) {
    ProcessUndecryptableEarlyPacket(conn_id);
  }
  EXPECT_EQ(0u, dispatcher_->NumSessions())
      << "No session should be created before CHLO arrives.";

  // Pop out the last packet as it is also be dropped by the store.
  data_connection_map_[conn_id].pop_back();
  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(connection_id_generator_,
              MaybeReplaceConnectionId(conn_id, version_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, _, client_addr_,
                                              Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));

  // Only |kDefaultMaxUndecryptablePackets| packets were buffered, and they
  // should be delivered in arrival order.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(kDefaultMaxUndecryptablePackets + 1)  // + 1 for CHLO.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(conn_id, packet);
            }
          })));
  expect_generator_is_called_ = false;
  ProcessFirstFlight(conn_id);
}

TEST_P(BufferedPacketStoreTest,
       ProcessNonChloPacketsForDifferentConnectionsUptoLimit) {
  InSequence s;
  // A bunch of non-CHLO should be buffered upon arrival.
  size_t kNumConnections = kMaxConnectionsWithoutCHLO + 1;
  for (size_t i = 1; i <= kNumConnections; ++i) {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 20000 + i);
    QuicConnectionId conn_id = TestConnectionId(i);
    ProcessUndecryptableEarlyPacket(client_address, conn_id);
  }

  // Pop out the packet on last connection as it shouldn't be enqueued in store
  // as well.
  data_connection_map_[TestConnectionId(kNumConnections)].pop_front();

  // Reset session creation counter to ensure processing CHLO can always
  // create session.
  QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(dispatcher_.get(),
                                                              kNumConnections);
  // Deactivate the EXPECT_CALL in ProcessFirstFlight() because we have to be
  // in sequence, so the EXPECT_CALL has to explicitly be in order here.
  expect_generator_is_called_ = false;
  // Process CHLOs to create session for these connections.
  for (size_t i = 1; i <= kNumConnections; ++i) {
    QuicSocketAddress client_address(QuicIpAddress::Loopback4(), 20000 + i);
    QuicConnectionId conn_id = TestConnectionId(i);
    EXPECT_CALL(connection_id_generator_,
                MaybeReplaceConnectionId(conn_id, version_))
        .WillOnce(Return(std::nullopt));
    EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, _, client_address,
                                                Eq(ExpectedAlpn()), _, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, conn_id, client_address, &mock_helper_,
            &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    // First |kNumConnections| - 1 connections should have buffered
    // a packet in store. The rest should have been dropped.
    size_t num_packet_to_process = i <= kMaxConnectionsWithoutCHLO ? 2u : 1u;
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, client_address, _))
        .Times(num_packet_to_process)
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(conn_id, packet);
              }
            })));
    ProcessFirstFlight(client_address, conn_id);
  }
}

// Tests that store delivers empty packet list if CHLO arrives firstly.
TEST_P(BufferedPacketStoreTest, DeliverEmptyPackets) {
  QuicConnectionId conn_id = TestConnectionId(1);
  EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, _, client_addr_,
                                              Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, client_addr_, _));
  ProcessFirstFlight(conn_id);
}

// Tests that a retransmitted CHLO arrives after a connection for the
// CHLO has been created.
TEST_P(BufferedPacketStoreTest, ReceiveRetransmittedCHLO) {
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  ProcessUndecryptableEarlyPacket(conn_id);

  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(connection_id_generator_,
              MaybeReplaceConnectionId(conn_id, version_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*dispatcher_, CreateQuicSession(conn_id, _, client_addr_,
                                              Eq(ExpectedAlpn()), _, _, _))
      .Times(1)  // Only triggered by 1st CHLO.
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(3)  // Triggered by 1 data packet and 2 CHLOs.
      .WillRepeatedly(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(conn_id, packet);
            }
          })));

  std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
      GetFirstFlightOfPackets(version_, conn_id);
  ASSERT_EQ(packets.size(), 1u);
  // Receive the CHLO once.
  ProcessReceivedPacket(packets[0]->Clone(), client_addr_, version_, conn_id);
  // Receive the CHLO a second time to simulate retransmission.
  ProcessReceivedPacket(std::move(packets[0]), client_addr_, version_, conn_id);
}

// Tests that expiration of a connection add connection id to time wait list.
TEST_P(BufferedPacketStoreTest, ReceiveCHLOAfterExpiration) {
  InSequence s;
  CreateTimeWaitListManager();
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  QuicBufferedPacketStorePeer::set_clock(store, mock_helper_.GetClock());

  QuicConnectionId conn_id = TestConnectionId(1);
  ProcessPacket(client_addr_, conn_id, true, absl::StrCat("data packet ", 2),
                CONNECTION_ID_PRESENT, PACKET_4BYTE_PACKET_NUMBER,
                /*packet_number=*/2);

  mock_helper_.AdvanceTime(
      QuicTime::Delta::FromSeconds(kInitialIdleTimeoutSecs));
  QuicAlarm* alarm = QuicBufferedPacketStorePeer::expiration_alarm(store);
  // Cancel alarm as if it had been fired.
  alarm->Cancel();
  store->OnExpirationTimeout();
  // New arrived CHLO will be dropped because this connection is in time wait
  // list.
  ASSERT_TRUE(time_wait_list_manager_->IsConnectionIdInTimeWait(conn_id));
  EXPECT_CALL(*time_wait_list_manager_, ProcessPacket(_, _, conn_id, _, _, _));
  expect_generator_is_called_ = false;
  ProcessFirstFlight(conn_id);
}

TEST_P(BufferedPacketStoreTest, ProcessCHLOsUptoLimitAndBufferTheRest) {
  // Process more than (|kMaxNumSessionsToCreate| +
  // |kDefaultMaxConnectionsInStore|) CHLOs,
  // the first |kMaxNumSessionsToCreate| should create connections immediately,
  // the next |kDefaultMaxConnectionsInStore| should be buffered,
  // the rest should be dropped.
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  const size_t kNumCHLOs =
      kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore + 1;
  for (uint64_t conn_id = 1; conn_id <= kNumCHLOs; ++conn_id) {
    const bool should_drop =
        (conn_id > kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore);
    if (!should_drop) {
      // MaybeReplaceConnectionId will be called once per connection, whether it
      // is buffered or not.
      EXPECT_CALL(connection_id_generator_,
                  MaybeReplaceConnectionId(TestConnectionId(conn_id), version_))
          .WillOnce(Return(std::nullopt));
    }

    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(
          *dispatcher_,
          CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                            Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    expect_generator_is_called_ = false;
    ProcessFirstFlight(TestConnectionId(conn_id));
    if (conn_id <= kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore &&
        conn_id > kMaxNumSessionsToCreate) {
      EXPECT_TRUE(store->HasChloForConnection(TestConnectionId(conn_id)));
    } else {
      // First |kMaxNumSessionsToCreate| CHLOs should be passed to new
      // connections immediately, and the last CHLO should be dropped as the
      // store is full.
      EXPECT_FALSE(store->HasChloForConnection(TestConnectionId(conn_id)));
    }
  }

  // Gradually consume buffered CHLOs. The buffered connections should be
  // created but the dropped one shouldn't.
  for (uint64_t conn_id = kMaxNumSessionsToCreate + 1;
       conn_id <= kMaxNumSessionsToCreate + kDefaultMaxConnectionsInStore;
       ++conn_id) {
    // MaybeReplaceConnectionId should have been called once per buffered
    // session.
    EXPECT_CALL(
        *dispatcher_,
        CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                          Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(conn_id), client_addr_,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(TestConnectionId(conn_id), packet);
              }
            })));
  }
  EXPECT_CALL(connection_id_generator_,
              MaybeReplaceConnectionId(TestConnectionId(kNumCHLOs), version_))
      .Times(0);
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(TestConnectionId(kNumCHLOs), _, client_addr_,
                                Eq(ExpectedAlpn()), _, _, _))
      .Times(0);

  while (store->HasChlosBuffered()) {
    dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
  }

  EXPECT_EQ(TestConnectionId(static_cast<size_t>(kMaxNumSessionsToCreate) +
                             kDefaultMaxConnectionsInStore),
            session1_->connection_id());
}

TEST_P(BufferedPacketStoreTest,
       ProcessCHLOsUptoLimitAndBufferWithDifferentConnectionIdGenerator) {
  // Process (|kMaxNumSessionsToCreate| + 1) CHLOs,
  // the first |kMaxNumSessionsToCreate| should create connections immediately,
  // the last should be buffered.
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  const size_t kNumCHLOs = kMaxNumSessionsToCreate + 1;
  for (uint64_t conn_id = 1; conn_id < kNumCHLOs; ++conn_id) {
    EXPECT_CALL(
        *dispatcher_,
        CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                          Eq(ExpectedAlpn()), _, MatchParsedClientHello(), _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(conn_id), client_addr_,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillOnce(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(TestConnectionId(conn_id), packet);
              }
            })));
    ProcessFirstFlight(TestConnectionId(conn_id));
  }
  uint64_t conn_id = kNumCHLOs;
  expect_generator_is_called_ = false;
  MockConnectionIdGenerator generator2;
  EXPECT_CALL(*dispatcher_, ConnectionIdGenerator())
      .WillRepeatedly(ReturnRef(generator2));
  const bool buffered_store_replace_cid = version_.UsesTls();
  if (buffered_store_replace_cid) {
    // generator2 should be used to replace the connection ID when the first
    // IETF INITIAL is enqueued.
    EXPECT_CALL(generator2,
                MaybeReplaceConnectionId(TestConnectionId(conn_id), version_))
        .WillOnce(Return(std::nullopt));
  }
  ProcessFirstFlight(TestConnectionId(conn_id));
  EXPECT_TRUE(store->HasChloForConnection(TestConnectionId(conn_id)));
  // Change the generator back so that the session can only access generator2
  // by using the buffer entry.
  EXPECT_CALL(*dispatcher_, ConnectionIdGenerator())
      .WillRepeatedly(ReturnRef(connection_id_generator_));

  if (!buffered_store_replace_cid) {
    // QuicDispatcher should attempt to replace the CID when creating the
    // QuicSession.
    EXPECT_CALL(connection_id_generator_,
                MaybeReplaceConnectionId(TestConnectionId(conn_id), version_))
        .WillOnce(Return(std::nullopt));
  }
  EXPECT_CALL(*dispatcher_, CreateQuicSession(TestConnectionId(conn_id), _,
                                              client_addr_, Eq(ExpectedAlpn()),
                                              _, MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, TestConnectionId(conn_id), client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .WillOnce(
          WithArg<2>(Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(TestConnectionId(conn_id), packet);
            }
          })));
  while (store->HasChlosBuffered()) {
    dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
  }
}

// Duplicated CHLO shouldn't be buffered.
TEST_P(BufferedPacketStoreTest, BufferDuplicatedCHLO) {
  for (uint64_t conn_id = 1; conn_id <= kMaxNumSessionsToCreate + 1;
       ++conn_id) {
    // Last CHLO will be buffered. Others will create connection right away.
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                                    Eq(ExpectedAlpn()), _, _, _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
  }
  // Retransmit CHLO on last connection should be dropped.
  QuicConnectionId last_connection =
      TestConnectionId(kMaxNumSessionsToCreate + 1);
  expect_generator_is_called_ = false;
  ProcessFirstFlight(last_connection);

  size_t packets_buffered = 2;

  // Reset counter and process buffered CHLO.
  EXPECT_CALL(*dispatcher_, CreateQuicSession(last_connection, _, client_addr_,
                                              Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, last_connection, client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  // Only one packet(CHLO) should be process.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(packets_buffered)
      .WillRepeatedly(WithArg<2>(
          Invoke([this, last_connection](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(last_connection, packet);
            }
          })));
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

TEST_P(BufferedPacketStoreTest, BufferNonChloPacketsUptoLimitWithChloBuffered) {
  uint64_t last_conn_id = kMaxNumSessionsToCreate + 1;
  QuicConnectionId last_connection_id = TestConnectionId(last_conn_id);
  for (uint64_t conn_id = 1; conn_id <= last_conn_id; ++conn_id) {
    // Last CHLO will be buffered. Others will create connection right away.
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                                    Eq(ExpectedAlpn()), _, _, _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillRepeatedly(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
  }

  // |last_connection_id| has 1 packet buffered now. Process another
  // |kDefaultMaxUndecryptablePackets| + 1 data packets to reach max number of
  // buffered packets per connection.
  for (uint64_t i = 0; i <= kDefaultMaxUndecryptablePackets; ++i) {
    ProcessPacket(client_addr_, last_connection_id, false, "data packet");
  }

  // Reset counter and process buffered CHLO.
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(last_connection_id, _, client_addr_,
                                Eq(ExpectedAlpn()), _, _, _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, last_connection_id, client_addr_,
          &mock_helper_, &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));

  const QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  const QuicBufferedPacketStore::BufferedPacketList*
      last_connection_buffered_packets =
          QuicBufferedPacketStorePeer::FindBufferedPackets(store,
                                                           last_connection_id);
  ASSERT_NE(last_connection_buffered_packets, nullptr);
  ASSERT_EQ(last_connection_buffered_packets->buffered_packets.size(),
            kDefaultMaxUndecryptablePackets);
  // All buffered packets should be delivered to the session.
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(last_connection_buffered_packets->buffered_packets.size())
      .WillRepeatedly(WithArg<2>(
          Invoke([this, last_connection_id](const QuicEncryptedPacket& packet) {
            if (version_.UsesQuicCrypto()) {
              ValidatePacket(last_connection_id, packet);
            }
          })));
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

// Tests that when dispatcher's packet buffer is full, a CHLO on connection
// which doesn't have buffered CHLO should be buffered.
TEST_P(BufferedPacketStoreTest, ReceiveCHLOForBufferedConnection) {
  QuicBufferedPacketStore* store =
      QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());

  uint64_t conn_id = 1;
  ProcessUndecryptableEarlyPacket(TestConnectionId(conn_id));
  // Fill packet buffer to full with CHLOs on other connections. Need to feed
  // extra CHLOs because the first |kMaxNumSessionsToCreate| are going to create
  // session directly.
  for (conn_id = 2;
       conn_id <= kDefaultMaxConnectionsInStore + kMaxNumSessionsToCreate;
       ++conn_id) {
    if (conn_id <= kMaxNumSessionsToCreate + 1) {
      EXPECT_CALL(*dispatcher_,
                  CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                                    Eq(ExpectedAlpn()), _, _, _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillOnce(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    } else if (!version_.UsesTls()) {
      expect_generator_is_called_ = false;
    }
    ProcessFirstFlight(TestConnectionId(conn_id));
  }
  EXPECT_FALSE(store->HasChloForConnection(
      /*connection_id=*/TestConnectionId(1)));

  // CHLO on connection 1 should still be buffered.
  ProcessFirstFlight(TestConnectionId(1));
  EXPECT_TRUE(store->HasChloForConnection(
      /*connection_id=*/TestConnectionId(1)));
}

// Regression test for b/117874922.
TEST_P(BufferedPacketStoreTest, ProcessBufferedChloWithDifferentVersion) {
  // Ensure the preferred version is not supported by the server.
  QuicDisableVersion(AllSupportedVersions().front());

  uint64_t last_connection_id = kMaxNumSessionsToCreate + 5;
  ParsedQuicVersionVector supported_versions = CurrentSupportedVersions();
  for (uint64_t conn_id = 1; conn_id <= last_connection_id; ++conn_id) {
    // Last 5 CHLOs will be buffered. Others will create connection right away.
    ParsedQuicVersion version =
        supported_versions[(conn_id - 1) % supported_versions.size()];
    if (conn_id <= kMaxNumSessionsToCreate) {
      EXPECT_CALL(
          *dispatcher_,
          CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                            Eq(ExpectedAlpnForVersion(version)), version, _, _))
          .WillOnce(Return(ByMove(CreateSession(
              dispatcher_.get(), config_, TestConnectionId(conn_id),
              client_addr_, &mock_helper_, &mock_alarm_factory_,
              &crypto_config_, QuicDispatcherPeer::GetCache(dispatcher_.get()),
              &session1_))));
      EXPECT_CALL(
          *reinterpret_cast<MockQuicConnection*>(session1_->connection()),
          ProcessUdpPacket(_, _, _))
          .WillRepeatedly(WithArg<2>(
              Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
                if (version_.UsesQuicCrypto()) {
                  ValidatePacket(TestConnectionId(conn_id), packet);
                }
              })));
    }
    ProcessFirstFlight(version, TestConnectionId(conn_id));
  }

  // Process buffered CHLOs. Verify the version is correct.
  for (uint64_t conn_id = kMaxNumSessionsToCreate + 1;
       conn_id <= last_connection_id; ++conn_id) {
    ParsedQuicVersion version =
        supported_versions[(conn_id - 1) % supported_versions.size()];
    EXPECT_CALL(
        *dispatcher_,
        CreateQuicSession(TestConnectionId(conn_id), _, client_addr_,
                          Eq(ExpectedAlpnForVersion(version)), version, _, _))
        .WillOnce(Return(ByMove(CreateSession(
            dispatcher_.get(), config_, TestConnectionId(conn_id), client_addr_,
            &mock_helper_, &mock_alarm_factory_, &crypto_config_,
            QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
    EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
                ProcessUdpPacket(_, _, _))
        .WillRepeatedly(WithArg<2>(
            Invoke([this, conn_id](const QuicEncryptedPacket& packet) {
              if (version_.UsesQuicCrypto()) {
                ValidatePacket(TestConnectionId(conn_id), packet);
              }
            })));
  }
  dispatcher_->ProcessBufferedChlos(kMaxNumSessionsToCreate);
}

TEST_P(BufferedPacketStoreTest, BufferedChloWithEcn) {
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  InSequence s;
  QuicConnectionId conn_id = TestConnectionId(1);
  // Process non-CHLO packet. This ProcessUndecryptableEarlyPacket() but with
  // an injected step to set the ECN bits.
  std::unique_ptr<QuicEncryptedPacket> encrypted_packet =
      GetUndecryptableEarlyPacket(version_, conn_id);
  std::unique_ptr<QuicReceivedPacket> received_packet(ConstructReceivedPacket(
      *encrypted_packet, mock_helper_.GetClock()->Now(), ECN_ECT1));
  ProcessReceivedPacket(std::move(received_packet), client_addr_, version_,
                        conn_id);
  EXPECT_EQ(0u, dispatcher_->NumSessions())
      << "No session should be created before CHLO arrives.";

  // When CHLO arrives, a new session should be created, and all packets
  // buffered should be delivered to the session.
  EXPECT_CALL(connection_id_generator_,
              MaybeReplaceConnectionId(conn_id, version_))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*dispatcher_,
              CreateQuicSession(conn_id, _, client_addr_, Eq(ExpectedAlpn()), _,
                                MatchParsedClientHello(), _))
      .WillOnce(Return(ByMove(CreateSession(
          dispatcher_.get(), config_, conn_id, client_addr_, &mock_helper_,
          &mock_alarm_factory_, &crypto_config_,
          QuicDispatcherPeer::GetCache(dispatcher_.get()), &session1_))));
  bool got_ect1 = false;
  bool got_ce = false;
  EXPECT_CALL(*reinterpret_cast<MockQuicConnection*>(session1_->connection()),
              ProcessUdpPacket(_, _, _))
      .Times(2)  // non-CHLO + CHLO.
      .WillRepeatedly(WithArg<2>(Invoke([&](const QuicReceivedPacket& packet) {
        switch (packet.ecn_codepoint()) {
          case ECN_ECT1:
            got_ect1 = true;
            break;
          case ECN_CE:
            got_ce = true;
            break;
          default:
            break;
        }
      })));
  QuicConnectionId client_connection_id = TestConnectionId(2);
  std::vector<std::unique_ptr<QuicReceivedPacket>> packets =
      GetFirstFlightOfPackets(version_, DefaultQuicConfig(), conn_id,
                              client_connection_id, TestClientCryptoConfig(),
                              ECN_CE);
  for (auto&& packet : packets) {
    ProcessReceivedPacket(std::move(packet), client_addr_, version_, conn_id);
  }
  EXPECT_TRUE(got_ect1);
  EXPECT_TRUE(got_ce);
}

class DualCIDBufferedPacketStoreTest : public BufferedPacketStoreTest {
 protected:
  void SetUp() override {
    BufferedPacketStoreTest::SetUp();
    QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(
        dispatcher_.get(), 0);

    // Prevent |ProcessFirstFlight| from setting up expectations for
    // MaybeReplaceConnectionId.
    expect_generator_is_called_ = false;
    EXPECT_CALL(connection_id_generator_, MaybeReplaceConnectionId(_, _))
        .WillRepeatedly(Invoke(
            this, &DualCIDBufferedPacketStoreTest::ReplaceConnectionIdInTest));
  }

  std::optional<QuicConnectionId> ReplaceConnectionIdInTest(
      const QuicConnectionId& original, const ParsedQuicVersion& version) {
    auto it = replaced_cid_map_.find(original);
    if (it == replaced_cid_map_.end()) {
      ADD_FAILURE() << "Bad test setup: no replacement CID for " << original
                    << ", version " << version;
      return std::nullopt;
    }
    return it->second;
  }

  QuicBufferedPacketStore& store() {
    return *QuicDispatcherPeer::GetBufferedPackets(dispatcher_.get());
  }

  using BufferedPacketList = QuicBufferedPacketStore::BufferedPacketList;
  const BufferedPacketList* FindBufferedPackets(
      QuicConnectionId connection_id) {
    return QuicBufferedPacketStorePeer::FindBufferedPackets(&store(),
                                                            connection_id);
  }

  absl::flat_hash_map<QuicConnectionId, std::optional<QuicConnectionId>>
      replaced_cid_map_;

 private:
  using BufferedPacketStoreTest::expect_generator_is_called_;
};

INSTANTIATE_TEST_SUITE_P(DualCIDBufferedPacketStoreTests,
                         DualCIDBufferedPacketStoreTest,
                         ::testing::ValuesIn(CurrentSupportedVersionsWithTls()),
                         ::testing::PrintToStringParamName());

TEST_P(DualCIDBufferedPacketStoreTest, CanLookUpByBothCIDs) {
  replaced_cid_map_[TestConnectionId(1)] = TestConnectionId(2);
  ProcessFirstFlight(TestConnectionId(1));

  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(1)));
  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(2)));

  const BufferedPacketList* packets1 = FindBufferedPackets(TestConnectionId(1));
  const BufferedPacketList* packets2 = FindBufferedPackets(TestConnectionId(2));
  EXPECT_EQ(packets1, packets2);
  EXPECT_EQ(packets1->original_connection_id, TestConnectionId(1));
  EXPECT_EQ(packets1->replaced_connection_id, TestConnectionId(2));
}

TEST_P(DualCIDBufferedPacketStoreTest, DeliverPacketsByOriginalCID) {
  replaced_cid_map_[TestConnectionId(1)] = TestConnectionId(2);
  ProcessFirstFlight(TestConnectionId(1));

  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(1)));
  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(2)));
  ASSERT_TRUE(store().HasChloForConnection(TestConnectionId(1)));
  ASSERT_TRUE(store().HasChloForConnection(TestConnectionId(2)));
  ASSERT_TRUE(store().HasChlosBuffered());

  BufferedPacketList packets = store().DeliverPackets(TestConnectionId(1));
  EXPECT_EQ(packets.original_connection_id, TestConnectionId(1));
  EXPECT_EQ(packets.replaced_connection_id, TestConnectionId(2));

  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(1)));
  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(2)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(1)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(2)));
  EXPECT_FALSE(store().HasChlosBuffered());
}

TEST_P(DualCIDBufferedPacketStoreTest, DeliverPacketsByReplacedCID) {
  replaced_cid_map_[TestConnectionId(1)] = TestConnectionId(2);
  replaced_cid_map_[TestConnectionId(3)] = TestConnectionId(4);
  ProcessFirstFlight(TestConnectionId(1));
  ProcessFirstFlight(TestConnectionId(3));

  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(1)));
  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(3)));
  ASSERT_TRUE(store().HasChloForConnection(TestConnectionId(1)));
  ASSERT_TRUE(store().HasChloForConnection(TestConnectionId(3)));
  ASSERT_TRUE(store().HasChlosBuffered());

  BufferedPacketList packets2 = store().DeliverPackets(TestConnectionId(2));
  EXPECT_EQ(packets2.original_connection_id, TestConnectionId(1));
  EXPECT_EQ(packets2.replaced_connection_id, TestConnectionId(2));

  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(1)));
  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(2)));
  EXPECT_TRUE(store().HasBufferedPackets(TestConnectionId(3)));
  EXPECT_TRUE(store().HasBufferedPackets(TestConnectionId(4)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(1)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(2)));
  EXPECT_TRUE(store().HasChloForConnection(TestConnectionId(3)));
  EXPECT_TRUE(store().HasChloForConnection(TestConnectionId(4)));
  EXPECT_TRUE(store().HasChlosBuffered());

  BufferedPacketList packets4 = store().DeliverPackets(TestConnectionId(4));
  EXPECT_EQ(packets4.original_connection_id, TestConnectionId(3));
  EXPECT_EQ(packets4.replaced_connection_id, TestConnectionId(4));

  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(3)));
  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(4)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(3)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(4)));
  EXPECT_FALSE(store().HasChlosBuffered());
}

TEST_P(DualCIDBufferedPacketStoreTest, DiscardPacketsByOriginalCID) {
  replaced_cid_map_[TestConnectionId(1)] = TestConnectionId(2);
  ProcessFirstFlight(TestConnectionId(1));

  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(1)));

  store().DiscardPackets(TestConnectionId(1));

  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(1)));
  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(2)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(1)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(2)));
  EXPECT_FALSE(store().HasChlosBuffered());
}

TEST_P(DualCIDBufferedPacketStoreTest, DiscardPacketsByReplacedCID) {
  replaced_cid_map_[TestConnectionId(1)] = TestConnectionId(2);
  replaced_cid_map_[TestConnectionId(3)] = TestConnectionId(4);
  ProcessFirstFlight(TestConnectionId(1));
  ProcessFirstFlight(TestConnectionId(3));

  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(2)));
  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(4)));

  store().DiscardPackets(TestConnectionId(2));

  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(1)));
  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(2)));
  EXPECT_TRUE(store().HasBufferedPackets(TestConnectionId(3)));
  EXPECT_TRUE(store().HasBufferedPackets(TestConnectionId(4)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(1)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(2)));
  EXPECT_TRUE(store().HasChloForConnection(TestConnectionId(3)));
  EXPECT_TRUE(store().HasChloForConnection(TestConnectionId(4)));
  EXPECT_TRUE(store().HasChlosBuffered());

  store().DiscardPackets(TestConnectionId(4));

  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(3)));
  EXPECT_FALSE(store().HasBufferedPackets(TestConnectionId(4)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(3)));
  EXPECT_FALSE(store().HasChloForConnection(TestConnectionId(4)));
  EXPECT_FALSE(store().HasChlosBuffered());
}

TEST_P(DualCIDBufferedPacketStoreTest, CIDCollision) {
  replaced_cid_map_[TestConnectionId(1)] = TestConnectionId(2);
  replaced_cid_map_[TestConnectionId(3)] = TestConnectionId(2);
  ProcessFirstFlight(TestConnectionId(1));
  ProcessFirstFlight(TestConnectionId(3));

  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(1)));
  ASSERT_TRUE(store().HasBufferedPackets(TestConnectionId(2)));

  // QuicDispatcher should discard connection 3 after CID collision.
  ASSERT_FALSE(store().HasBufferedPackets(TestConnectionId(3)));
}

}  // namespace
}  // namespace test
}  // namespace quic
