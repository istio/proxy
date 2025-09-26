// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "quiche/http2/core/spdy_framer.h"
#include "quiche/http2/core/spdy_protocol.h"
#include "openssl/ssl.h"
#include "quiche/quic/core/congestion_control/rtt_stats.h"
#include "quiche/quic/core/crypto/crypto_protocol.h"
#include "quiche/quic/core/crypto/quic_client_session_cache.h"
#include "quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "quiche/quic/core/crypto/quic_random.h"
#include "quiche/quic/core/crypto/transport_parameters.h"
#include "quiche/quic/core/frames/quic_blocked_frame.h"
#include "quiche/quic/core/frames/quic_crypto_frame.h"
#include "quiche/quic/core/frames/quic_ping_frame.h"
#include "quiche/quic/core/frames/quic_window_update_frame.h"
#include "quiche/quic/core/http/http_constants.h"
#include "quiche/quic/core/http/quic_spdy_client_stream.h"
#include "quiche/quic/core/http/quic_spdy_session.h"
#include "quiche/quic/core/http/web_transport_http3.h"
#include "quiche/quic/core/io/quic_default_event_loop.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/qpack/qpack_encoder.h"
#include "quiche/quic/core/qpack/qpack_instruction_encoder.h"
#include "quiche/quic/core/qpack/value_splitting_header_list.h"
#include "quiche/quic/core/quic_ack_listener_interface.h"
#include "quiche/quic/core/quic_bandwidth.h"
#include "quiche/quic/core/quic_connection.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_connection_stats.h"
#include "quiche/quic/core/quic_constants.h"
#include "quiche/quic/core/quic_default_clock.h"
#include "quiche/quic/core/quic_dispatcher.h"
#include "quiche/quic/core/quic_dispatcher_stats.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_framer.h"
#include "quiche/quic/core/quic_interval.h"
#include "quiche/quic/core/quic_interval_set.h"
#include "quiche/quic/core/quic_packet_creator.h"
#include "quiche/quic/core/quic_packet_number.h"
#include "quiche/quic/core/quic_packet_writer.h"
#include "quiche/quic/core/quic_packet_writer_wrapper.h"
#include "quiche/quic/core/quic_packets.h"
#include "quiche/quic/core/quic_session.h"
#include "quiche/quic/core/quic_stream.h"
#include "quiche/quic/core/quic_tag.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/quic_udp_socket.h"
#include "quiche/quic/core/quic_utils.h"
#include "quiche/quic/core/quic_versions.h"
#include "quiche/quic/core/web_transport_interface.h"
#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_flags.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_logging.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/test_tools/bad_packet_writer.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/test_tools/packet_dropping_test_writer.h"
#include "quiche/quic/test_tools/packet_reordering_writer.h"
#include "quiche/quic/test_tools/qpack/qpack_encoder_peer.h"
#include "quiche/quic/test_tools/qpack/qpack_test_utils.h"
#include "quiche/quic/test_tools/quic_client_session_cache_peer.h"
#include "quiche/quic/test_tools/quic_config_peer.h"
#include "quiche/quic/test_tools/quic_connection_peer.h"
#include "quiche/quic/test_tools/quic_dispatcher_peer.h"
#include "quiche/quic/test_tools/quic_flow_controller_peer.h"
#include "quiche/quic/test_tools/quic_sent_packet_manager_peer.h"
#include "quiche/quic/test_tools/quic_server_peer.h"
#include "quiche/quic/test_tools/quic_session_peer.h"
#include "quiche/quic/test_tools/quic_spdy_session_peer.h"
#include "quiche/quic/test_tools/quic_spdy_stream_peer.h"
#include "quiche/quic/test_tools/quic_stream_id_manager_peer.h"
#include "quiche/quic/test_tools/quic_stream_peer.h"
#include "quiche/quic/test_tools/quic_stream_sequencer_peer.h"
#include "quiche/quic/test_tools/quic_test_backend.h"
#include "quiche/quic/test_tools/quic_test_client.h"
#include "quiche/quic/test_tools/quic_test_server.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/test_tools/server_thread.h"
#include "quiche/quic/test_tools/simple_quic_framer.h"
#include "quiche/quic/test_tools/web_transport_test_tools.h"
#include "quiche/quic/tools/quic_backend_response.h"
#include "quiche/quic/tools/quic_default_client.h"
#include "quiche/quic/tools/quic_server.h"
#include "quiche/quic/tools/quic_simple_server_backend.h"
#include "quiche/quic/tools/quic_simple_server_stream.h"
#include "quiche/quic/tools/quic_spdy_client_base.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_reference_counted.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_mem_slice.h"
#include "quiche/common/quiche_stream.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/web_transport_headers.h"

using quiche::HttpHeaderBlock;
using spdy::SpdyFramer;
using spdy::SpdySerializedFrame;
using spdy::SpdySettingsIR;
using ::testing::_;
using ::testing::Assign;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAreArray;

#ifndef NDEBUG
// Debug build.
#define EXPECT_DEBUG_EQ(val1, val2) EXPECT_EQ(val1, val2)
#else
// Release build.
#define EXPECT_DEBUG_EQ(val1, val2)
#endif

namespace quic {
namespace test {
namespace {

const char kFooResponseBody[] = "Artichoke hearts make me happy.";
const char kBarResponseBody[] = "Palm hearts are pretty delicious, also.";
const char kTestUserAgentId[] = "quic/core/http/end_to_end_test.cc";
const float kSessionToStreamRatio = 1.5;
const int kLongConnectionIdLength = 16;

// Run all tests with the cross products of all versions.
struct TestParams {
  TestParams(const ParsedQuicVersion& version, QuicTag congestion_control_tag,
             QuicEventLoopFactory* event_loop,
             int override_server_connection_id_length)
      : version(version),
        congestion_control_tag(congestion_control_tag),
        event_loop(event_loop),
        override_server_connection_id_length(
            override_server_connection_id_length) {}

  friend std::ostream& operator<<(std::ostream& os, const TestParams& p) {
    os << "{ version: " << ParsedQuicVersionToString(p.version);
    os << " congestion_control_tag: "
       << QuicTagToString(p.congestion_control_tag)
       << " event loop: " << p.event_loop->GetName()
       << " connection ID length: " << p.override_server_connection_id_length
       << " }";
    return os;
  }

  ParsedQuicVersion version;
  QuicTag congestion_control_tag;
  QuicEventLoopFactory* event_loop;
  int override_server_connection_id_length;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(const TestParams& p) {
  std::string rv = absl::StrCat(
      ParsedQuicVersionToString(p.version), "_",
      QuicTagToString(p.congestion_control_tag), "_", p.event_loop->GetName(),
      "_",
      std::to_string((p.override_server_connection_id_length == -1)
                         ? static_cast<int>(kQuicDefaultConnectionIdLength)
                         : p.override_server_connection_id_length));
  return EscapeTestParamName(rv);
}

// Constructs various test permutations.
std::vector<TestParams> GetTestParams() {
  std::vector<TestParams> params;
  std::vector<int> connection_id_lengths{-1, kLongConnectionIdLength};
  for (auto connection_id_length : connection_id_lengths) {
    for (const QuicTag congestion_control_tag : {kTBBR, kQBIC, kB2ON}) {
      if (!GetQuicReloadableFlag(quic_allow_client_enabled_bbr_v2) &&
          congestion_control_tag == kB2ON) {
        continue;
      }
      for (const ParsedQuicVersion& version : CurrentSupportedVersions()) {
        // TODO(b/232269029): Q050 should be able to handle 0-RTT when the
        // initial connection ID is > 8 bytes, but it cannot. This is an
        // invasive fix that has no impact as long as gQUIC clients always use
        // 8B server connection IDs. If this bug is fixed, we can change
        // 'UsesTls' to 'AllowsVariableLengthConnectionIds()' below to test
        // qQUIC as well.
        if (connection_id_length == -1 || version.UsesTls()) {
          params.push_back(TestParams(version, congestion_control_tag,
                                      GetDefaultEventLoop(),
                                      connection_id_length));
        }
      }  // End of outer version loop.
    }  // End of congestion_control_tag loop.
  }  // End of connection_id_length loop.

  // Only run every event loop implementation for one fixed configuration.
  for (QuicEventLoopFactory* event_loop : GetAllSupportedEventLoops()) {
    if (event_loop == GetDefaultEventLoop()) {
      continue;
    }
    params.push_back(
        TestParams(ParsedQuicVersion::RFCv1(), kTBBR, event_loop, -1));
  }

  return params;
}

void WriteHeadersOnStream(QuicSpdyStream* stream) {
  // Since QuicSpdyStream uses QuicHeaderList::empty() to detect too large
  // headers, it also fails when receiving empty headers.
  HttpHeaderBlock headers;
  headers[":authority"] = "test.example.com:443";
  headers[":path"] = "/path";
  headers[":method"] = "GET";
  headers[":scheme"] = "https";
  stream->WriteHeaders(std::move(headers), /* fin = */ false, nullptr);
}

class ServerDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ServerDelegate(QuicDispatcher* dispatcher)
      : dispatcher_(dispatcher) {}
  ~ServerDelegate() override = default;
  void OnCanWrite() override { dispatcher_->OnCanWrite(); }

 private:
  QuicDispatcher* dispatcher_;
};

class ClientDelegate : public PacketDroppingTestWriter::Delegate {
 public:
  explicit ClientDelegate(QuicDefaultClient* client) : client_(client) {}
  ~ClientDelegate() override = default;
  void OnCanWrite() override {
    client_->default_network_helper()->OnSocketEvent(
        nullptr, client_->GetLatestFD(), kSocketEventWritable);
  }

 private:
  QuicDefaultClient* client_;
};

class EndToEndTest : public QuicTestWithParam<TestParams> {
 protected:
  EndToEndTest()
      : initialized_(false),
        connect_to_server_on_initialize_(true),
        server_address_(QuicSocketAddress(TestLoopback(), 0)),
        server_hostname_("test.example.com"),
        fd_(kQuicInvalidSocketFd),
        client_writer_(nullptr),
        server_writer_(nullptr),
        version_(GetParam().version),
        client_supported_versions_({version_}),
        server_supported_versions_(CurrentSupportedVersions()),
        chlo_multiplier_(0),
        stream_factory_(nullptr),
        override_server_connection_id_length_(
            GetParam().override_server_connection_id_length),
        expected_server_connection_id_length_(kQuicDefaultConnectionIdLength) {
    QUIC_LOG(INFO) << "Using Configuration: " << GetParam();

    // Use different flow control windows for client/server.
    client_config_.SetInitialStreamFlowControlWindowToSend(
        2 * kInitialStreamFlowControlWindowForTest);
    client_config_.SetInitialSessionFlowControlWindowToSend(
        2 * kInitialSessionFlowControlWindowForTest);
    server_config_.SetInitialStreamFlowControlWindowToSend(
        3 * kInitialStreamFlowControlWindowForTest);
    server_config_.SetInitialSessionFlowControlWindowToSend(
        3 * kInitialSessionFlowControlWindowForTest);

    // The default idle timeouts can be too strict when running on a busy
    // machine.
    const QuicTime::Delta timeout = QuicTime::Delta::FromSeconds(30);
    client_config_.set_max_time_before_crypto_handshake(timeout);
    client_config_.set_max_idle_time_before_crypto_handshake(timeout);
    server_config_.set_max_time_before_crypto_handshake(timeout);
    server_config_.set_max_idle_time_before_crypto_handshake(timeout);

    AddToCache("/foo", 200, kFooResponseBody);
    AddToCache("/bar", 200, kBarResponseBody);
    // Enable fixes for bugs found in tests and prod.
  }

  virtual void CreateClientWithWriter() {
    client_.reset(CreateQuicClient(client_writer_));
  }

  QuicTestClient* CreateQuicClient(QuicPacketWriterWrapper* writer) {
    return CreateQuicClient(writer, /*connect=*/true);
  }

  QuicTestClient* CreateQuicClient(QuicPacketWriterWrapper* writer,
                                   bool connect) {
    QuicTestClient* client = new QuicTestClient(
        server_address_, server_hostname_, client_config_,
        client_supported_versions_,
        crypto_test_utils::ProofVerifierForTesting(),
        std::make_unique<QuicClientSessionCache>(),
        GetParam().event_loop->Create(QuicDefaultClock::Get()));
    client->SetUserAgentID(kTestUserAgentId);
    if (enable_mlkem_in_client_) {
      std::vector<uint16_t> client_supported_groups = {
          SSL_GROUP_X25519_MLKEM768, SSL_GROUP_X25519};
      client->SetPreferredGroups(client_supported_groups);
    }
    client->UseWriter(writer);
    if (!pre_shared_key_client_.empty()) {
      client->client()->SetPreSharedKey(pre_shared_key_client_);
    }
    if (override_server_connection_id_length_ >= 0) {
      client->UseConnectionIdLength(override_server_connection_id_length_);
    }
    if (override_client_connection_id_length_ >= 0) {
      client->UseClientConnectionIdLength(
          override_client_connection_id_length_);
    }
    client->client()->set_connection_debug_visitor(connection_debug_visitor_);
    client->client()->set_enable_web_transport(enable_web_transport_);
    if (connect) {
      client->Connect();
    }
    return client;
  }

  void set_smaller_flow_control_receive_window() {
    const uint32_t kClientIFCW = 64 * 1024;
    const uint32_t kServerIFCW = 1024 * 1024;
    set_client_initial_stream_flow_control_receive_window(kClientIFCW);
    set_client_initial_session_flow_control_receive_window(
        kSessionToStreamRatio * kClientIFCW);
    set_server_initial_stream_flow_control_receive_window(kServerIFCW);
    set_server_initial_session_flow_control_receive_window(
        kSessionToStreamRatio * kServerIFCW);
  }

  void set_client_initial_stream_flow_control_receive_window(uint32_t window) {
    ASSERT_TRUE(client_ == nullptr);
    QUIC_DLOG(INFO) << "Setting client initial stream flow control window: "
                    << window;
    client_config_.SetInitialStreamFlowControlWindowToSend(window);
  }

  void set_client_initial_session_flow_control_receive_window(uint32_t window) {
    ASSERT_TRUE(client_ == nullptr);
    QUIC_DLOG(INFO) << "Setting client initial session flow control window: "
                    << window;
    client_config_.SetInitialSessionFlowControlWindowToSend(window);
  }

  void set_client_initial_max_stream_data_incoming_bidirectional(
      uint32_t window) {
    ASSERT_TRUE(client_ == nullptr);
    QUIC_DLOG(INFO)
        << "Setting client initial max stream data incoming bidirectional: "
        << window;
    client_config_.SetInitialMaxStreamDataBytesIncomingBidirectionalToSend(
        window);
  }

  void set_server_initial_max_stream_data_outgoing_bidirectional(
      uint32_t window) {
    ASSERT_TRUE(client_ == nullptr);
    QUIC_DLOG(INFO)
        << "Setting server initial max stream data outgoing bidirectional: "
        << window;
    server_config_.SetInitialMaxStreamDataBytesOutgoingBidirectionalToSend(
        window);
  }

  void set_server_initial_stream_flow_control_receive_window(uint32_t window) {
    ASSERT_TRUE(server_thread_ == nullptr);
    QUIC_DLOG(INFO) << "Setting server initial stream flow control window: "
                    << window;
    server_config_.SetInitialStreamFlowControlWindowToSend(window);
  }

  void set_server_initial_session_flow_control_receive_window(uint32_t window) {
    ASSERT_TRUE(server_thread_ == nullptr);
    QUIC_DLOG(INFO) << "Setting server initial session flow control window: "
                    << window;
    server_config_.SetInitialSessionFlowControlWindowToSend(window);
  }

  const QuicSentPacketManager* GetSentPacketManagerFromFirstServerSession() {
    QuicConnection* server_connection = GetServerConnection();
    if (server_connection == nullptr) {
      ADD_FAILURE() << "Missing server connection";
      return nullptr;
    }
    return &server_connection->sent_packet_manager();
  }

  const QuicSentPacketManager* GetSentPacketManagerFromClientSession() {
    QuicConnection* client_connection = GetClientConnection();
    if (client_connection == nullptr) {
      ADD_FAILURE() << "Missing client connection";
      return nullptr;
    }
    return &client_connection->sent_packet_manager();
  }

  QuicSpdyClientSession* GetClientSession() {
    if (!client_) {
      ADD_FAILURE() << "Missing QuicTestClient";
      return nullptr;
    }
    if (client_->client() == nullptr) {
      ADD_FAILURE() << "Missing MockableQuicClient";
      return nullptr;
    }
    return client_->client()->client_session();
  }

  QuicConnection* GetClientConnection() {
    QuicSpdyClientSession* client_session = GetClientSession();
    if (client_session == nullptr) {
      ADD_FAILURE() << "Missing client session";
      return nullptr;
    }
    return client_session->connection();
  }

  // Must be called while `server_thread_` is paused.
  QuicConnection* GetServerConnection() {
    QuicSpdySession* server_session = GetServerSession();
    if (server_session == nullptr) {
      ADD_FAILURE() << "Missing server session";
      return nullptr;
    }
    return server_session->connection();
  }

  // Must be called while `server_thread_` is paused.
  QuicSpdySession* GetServerSession() {
    QuicDispatcher* dispatcher = GetDispatcher();
    if (dispatcher == nullptr) {
      ADD_FAILURE() << "Missing dispatcher";
      return nullptr;
    }
    if (dispatcher->NumSessions() == 0) {
      ADD_FAILURE() << "Empty dispatcher session map";
      return nullptr;
    }
    EXPECT_EQ(1u, dispatcher->NumSessions());
    return static_cast<QuicSpdySession*>(
        QuicDispatcherPeer::GetFirstSessionIfAny(dispatcher));
  }

  // Must be called while `server_thread_` is paused.
  QuicDispatcher* GetDispatcher() {
    if (!server_thread_) {
      ADD_FAILURE() << "Missing server thread";
      return nullptr;
    }
    QuicServer* quic_server = server_thread_->server();
    if (quic_server == nullptr) {
      ADD_FAILURE() << "Missing server";
      return nullptr;
    }
    return QuicServerPeer::GetDispatcher(quic_server);
  }

  // Must be called while `server_thread_` is paused.
  const QuicDispatcherStats& GetDispatcherStats() {
    return GetDispatcher()->stats();
  }

  QuicDispatcherStats GetDispatcherStatsThreadSafe() {
    QuicDispatcherStats stats;
    server_thread_->ScheduleAndWaitForCompletion(
        [&] { stats = GetDispatcherStats(); });
    return stats;
  }

  bool Initialize() {
    if (enable_web_transport_) {
      memory_cache_backend_.set_enable_webtransport(true);
    }

    QuicTagVector copt;
    server_config_.SetConnectionOptionsToSend(copt);
    copt = client_extra_copts_;

    // TODO(nimia): Consider setting the congestion control algorithm for the
    // client as well according to the test parameter.
    copt.push_back(GetParam().congestion_control_tag);
    copt.push_back(k2PTO);
    if (version_.HasIetfQuicFrames()) {
      copt.push_back(kILD0);
    }
    copt.push_back(kPLE1);
    client_config_.SetConnectionOptionsToSend(copt);

    // Start the server first, because CreateQuicClient() attempts
    // to connect to the server.
    StartServer();

    if (use_preferred_address_) {
      SetQuicReloadableFlag(quic_use_received_client_addresses_cache, true);
      // At this point, the server has an ephemeral port to listen on. Restart
      // the server with the preferred address.
      StopServer();
      // server_address_ now contains the random listening port.
      server_preferred_address_ =
          QuicSocketAddress(TestLoopback(2), server_address_.port());
      if (server_preferred_address_ == server_address_) {
        ADD_FAILURE() << "Preferred address and server address are the same "
                      << server_address_;
        return false;
      }
      // Send server preferred address and let server listen on Any.
      if (server_preferred_address_.host().IsIPv4()) {
        server_listening_address_ =
            QuicSocketAddress(QuicIpAddress::Any4(), server_address_.port());
        server_config_.SetIPv4AlternateServerAddressToSend(
            server_preferred_address_);
      } else {
        server_listening_address_ =
            QuicSocketAddress(QuicIpAddress::Any6(), server_address_.port());
        server_config_.SetIPv6AlternateServerAddressToSend(
            server_preferred_address_);
      }
      // Server restarts.
      server_writer_ = new PacketDroppingTestWriter();
      StartServer();

      if (!GetQuicFlag(quic_always_support_server_preferred_address)) {
        client_config_.SetConnectionOptionsToSend(QuicTagVector{kSPAD});
      }
    }

    if (!connect_to_server_on_initialize_) {
      initialized_ = true;
      return true;
    }

    CreateClientWithWriter();
    if (!client_) {
      ADD_FAILURE() << "Missing QuicTestClient";
      return false;
    }
    MockableQuicClient* client = client_->client();
    if (client == nullptr) {
      ADD_FAILURE() << "Missing MockableQuicClient";
      return false;
    }
    if (client_writer_ != nullptr) {
      QuicConnection* client_connection = GetClientConnection();
      if (client_connection == nullptr) {
        ADD_FAILURE() << "Missing client connection";
        return false;
      }
      client_writer_->Initialize(
          QuicConnectionPeer::GetHelper(client_connection),
          QuicConnectionPeer::GetAlarmFactory(client_connection),
          std::make_unique<ClientDelegate>(client));
    }
    initialized_ = true;
    return client->connected();
  }

  void SetUp() override {
    // The ownership of these gets transferred to the QuicPacketWriterWrapper
    // when Initialize() is executed.
    client_writer_ = new PacketDroppingTestWriter();
    server_writer_ = new PacketDroppingTestWriter();
  }

  void TearDown() override {
    EXPECT_TRUE(initialized_) << "You must call Initialize() in every test "
                              << "case. Otherwise, your test will leak memory.";
    if (connect_to_server_on_initialize_) {
      QuicConnection* client_connection = GetClientConnection();
      if (client_connection != nullptr) {
        client_connection->set_debug_visitor(nullptr);
      } else {
        ADD_FAILURE() << "Missing client connection";
      }
    }
    StopServer(/*will_restart=*/false);
    if (fd_ != kQuicInvalidSocketFd) {
      // Every test should follow StopServer(true) with StartServer(), so we
      // should never get here.
      QuicUdpSocketApi socket_api;
      socket_api.Destroy(fd_);
      fd_ = kQuicInvalidSocketFd;
    }
  }

  void StartServer() {
    if (fd_ != kQuicInvalidSocketFd) {
      // We previously called StopServer to reserve the ephemeral port. Close
      // the socket so that it's available below.
      QuicUdpSocketApi socket_api;
      socket_api.Destroy(fd_);
      fd_ = kQuicInvalidSocketFd;
    }
    auto test_server = std::make_unique<QuicTestServer>(
        crypto_test_utils::ProofSourceForTesting(), server_config_,
        server_supported_versions_, &memory_cache_backend_,
        expected_server_connection_id_length_);
    test_server->SetEventLoopFactory(GetParam().event_loop);
    const QuicSocketAddress server_listening_address =
        server_listening_address_.has_value() ? *server_listening_address_
                                              : server_address_;
    server_thread_ = std::make_unique<ServerThread>(std::move(test_server),
                                                    server_listening_address);
    if (chlo_multiplier_ != 0) {
      server_thread_->server()->SetChloMultiplier(chlo_multiplier_);
    }
    if (!pre_shared_key_server_.empty()) {
      server_thread_->server()->SetPreSharedKey(pre_shared_key_server_);
    }
    server_thread_->Initialize();
    server_address_ =
        QuicSocketAddress(server_address_.host(), server_thread_->GetPort());
    QuicDispatcher* dispatcher =
        QuicServerPeer::GetDispatcher(server_thread_->server());
    ASSERT_TRUE(dispatcher != nullptr);
    QuicDispatcherPeer::UseWriter(dispatcher, server_writer_);

    server_writer_->Initialize(QuicDispatcherPeer::GetHelper(dispatcher),
                               QuicDispatcherPeer::GetAlarmFactory(dispatcher),
                               std::make_unique<ServerDelegate>(dispatcher));
    if (stream_factory_ != nullptr) {
      static_cast<QuicTestServer*>(server_thread_->server())
          ->SetSpdyStreamFactory(stream_factory_);
    }

    server_thread_->Start();
  }

  void StopServer(bool will_restart = true) {
    if (server_thread_) {
      server_thread_->Quit();
      server_thread_->Join();
    }
    if (will_restart) {
      // server_address_ now contains the random listening port. Since many
      // tests will attempt to re-bind the socket, claim it so that the kernel
      // doesn't give away the ephemeral port.
      QuicUdpSocketApi socket_api;
      fd_ = socket_api.Create(
          server_address_.host().AddressFamilyToInt(),
          /*receive_buffer_size =*/kDefaultSocketReceiveBuffer,
          /*send_buffer_size =*/kDefaultSocketReceiveBuffer);
      if (fd_ == kQuicInvalidSocketFd) {
        QUIC_LOG(ERROR) << "CreateSocket() failed: " << strerror(errno);
        return;
      }
      int rc = socket_api.Bind(fd_, server_address_);
      if (rc < 0) {
        QUIC_LOG(ERROR) << "Bind failed: " << strerror(errno);
        return;
      }
    }
  }

  void AddToCache(absl::string_view path, int response_code,
                  absl::string_view body) {
    memory_cache_backend_.AddSimpleResponse(server_hostname_, path,
                                            response_code, body);
  }

  void SetPacketLossPercentage(int32_t loss) {
    client_writer_->set_fake_packet_loss_percentage(loss);
    server_writer_->set_fake_packet_loss_percentage(loss);
  }

  void SetPacketSendDelay(QuicTime::Delta delay) {
    client_writer_->set_fake_packet_delay(delay);
    server_writer_->set_fake_packet_delay(delay);
  }

  void SetReorderPercentage(int32_t reorder) {
    client_writer_->set_fake_reorder_percentage(reorder);
    server_writer_->set_fake_reorder_percentage(reorder);
  }

  // Verifies that the client and server connections were both free of packets
  // being discarded, based on connection stats.
  // Calls server_thread_ Pause() and Resume(), which may only be called once
  // per test.
  void VerifyCleanConnection(bool had_packet_loss) {
    QuicConnection* client_connection = GetClientConnection();
    if (client_connection == nullptr) {
      ADD_FAILURE() << "Missing client connection";
      return;
    }
    QuicConnectionStats client_stats = client_connection->GetStats();
    // TODO(ianswett): Determine why this becomes even more flaky with BBR
    // enabled.  b/62141144
    if (!had_packet_loss && !GetQuicReloadableFlag(quic_default_to_bbr)) {
      EXPECT_EQ(0u, client_stats.packets_lost);
    }
    EXPECT_EQ(0u, client_stats.packets_discarded);
    // When client starts with an unsupported version, the version negotiation
    // packet sent by server for the old connection (respond for the connection
    // close packet) will be dropped by the client.
    if (!ServerSendsVersionNegotiation()) {
      EXPECT_EQ(0u, client_stats.packets_dropped);
    }
    if (!version_.UsesTls()) {
      // Only enforce this for QUIC crypto because accounting of number of
      // packets received, processed gets complicated with packets coalescing
      // and key dropping. For example, a received undecryptable coalesced
      // packet can be processed later and each sub-packet increases
      // packets_processed.
      EXPECT_EQ(client_stats.packets_received, client_stats.packets_processed);
    }

    if (!server_thread_) {
      ADD_FAILURE() << "Missing server thread";
      return;
    }
    server_thread_->Pause();
    QuicSpdySession* server_session = GetServerSession();
    if (server_session != nullptr) {
      QuicConnection* server_connection = server_session->connection();
      if (server_connection != nullptr) {
        QuicConnectionStats server_stats = server_connection->GetStats();
        if (!had_packet_loss) {
          EXPECT_EQ(0u, server_stats.packets_lost);
        }
        EXPECT_EQ(0u, server_stats.packets_discarded);
      } else {
        ADD_FAILURE() << "Missing server connection";
      }
    } else {
      ADD_FAILURE() << "Missing server session";
    }
    // TODO(ianswett): Restore the check for packets_dropped equals 0.
    // The expect for packets received is equal to packets processed fails
    // due to version negotiation packets.
    server_thread_->Resume();
  }

  // Returns true when client starts with an unsupported version, and client
  // closes connection when version negotiation is received.
  bool ServerSendsVersionNegotiation() {
    return client_supported_versions_[0] != version_;
  }

  bool SupportsIetfQuicWithTls(ParsedQuicVersion version) {
    return version.handshake_protocol == PROTOCOL_TLS1_3;
  }

  static void ExpectFlowControlsSynced(QuicSession* client,
                                       QuicSession* server) {
    EXPECT_EQ(
        QuicFlowControllerPeer::SendWindowSize(client->flow_controller()),
        QuicFlowControllerPeer::ReceiveWindowSize(server->flow_controller()));
    EXPECT_EQ(
        QuicFlowControllerPeer::ReceiveWindowSize(client->flow_controller()),
        QuicFlowControllerPeer::SendWindowSize(server->flow_controller()));
  }

  static void ExpectFlowControlsSynced(QuicStream* client, QuicStream* server) {
    EXPECT_EQ(QuicStreamPeer::SendWindowSize(client),
              QuicStreamPeer::ReceiveWindowSize(server));
    EXPECT_EQ(QuicStreamPeer::ReceiveWindowSize(client),
              QuicStreamPeer::SendWindowSize(server));
  }

  // Must be called before Initialize to have effect.
  void SetSpdyStreamFactory(QuicTestServer::StreamFactory* factory) {
    stream_factory_ = factory;
  }

  QuicStreamId GetNthClientInitiatedBidirectionalId(int n) {
    return GetNthClientInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  QuicStreamId GetNthServerInitiatedBidirectionalId(int n) {
    return GetNthServerInitiatedBidirectionalStreamId(
        version_.transport_version, n);
  }

  bool CheckResponseHeaders(QuicTestClient* client,
                            const std::string& expected_status) {
    const quiche::HttpHeaderBlock* response_headers =
        client->response_headers();
    auto it = response_headers->find(":status");
    if (it == response_headers->end()) {
      ADD_FAILURE() << "Did not find :status header in response";
      return false;
    }
    if (it->second != expected_status) {
      ADD_FAILURE() << "Got bad :status response: \"" << it->second << "\"";
      return false;
    }
    return true;
  }

  bool CheckResponseHeaders(QuicTestClient* client) {
    return CheckResponseHeaders(client, "200");
  }

  bool CheckResponseHeaders(const std::string& expected_status) {
    return CheckResponseHeaders(client_.get(), expected_status);
  }

  bool CheckResponseHeaders() { return CheckResponseHeaders(client_.get()); }

  bool CheckResponse(QuicTestClient* client,
                     const std::string& received_response,
                     const std::string& expected_response) {
    EXPECT_THAT(client_->stream_error(), IsQuicStreamNoError());
    EXPECT_THAT(client_->connection_error(), IsQuicNoError());

    if (received_response.empty() && !expected_response.empty()) {
      ADD_FAILURE() << "Failed to get any response for request";
      return false;
    }
    if (received_response != expected_response) {
      ADD_FAILURE() << "Got wrong response: \"" << received_response << "\"";
      return false;
    }
    return CheckResponseHeaders(client);
  }

  bool SendSynchronousRequestAndCheckResponse(
      QuicTestClient* client, const std::string& request,
      const std::string& expected_response) {
    std::string received_response = client->SendSynchronousRequest(request);
    return CheckResponse(client, received_response, expected_response);
  }

  bool SendSynchronousRequestAndCheckResponse(
      const std::string& request, const std::string& expected_response) {
    return SendSynchronousRequestAndCheckResponse(client_.get(), request,
                                                  expected_response);
  }

  bool SendSynchronousFooRequestAndCheckResponse(QuicTestClient* client) {
    return SendSynchronousRequestAndCheckResponse(client, "/foo",
                                                  kFooResponseBody);
  }

  bool SendSynchronousFooRequestAndCheckResponse() {
    return SendSynchronousFooRequestAndCheckResponse(client_.get());
  }

  bool SendSynchronousBarRequestAndCheckResponse() {
    std::string received_response = client_->SendSynchronousRequest("/bar");
    return CheckResponse(client_.get(), received_response, kBarResponseBody);
  }

  bool WaitForFooResponseAndCheckIt(QuicTestClient* client) {
    client->WaitForResponse();
    std::string received_response = client->response_body();
    return CheckResponse(client_.get(), received_response, kFooResponseBody);
  }

  bool WaitForFooResponseAndCheckIt() {
    return WaitForFooResponseAndCheckIt(client_.get());
  }

  WebTransportHttp3* CreateWebTransportSession(
      const std::string& path, bool wait_for_server_response,
      std::initializer_list<std::pair<absl::string_view, absl::string_view>>
          extra_headers = {}) {
    // Wait until we receive the settings from the server indicating
    // WebTransport support.
    client_->WaitUntil(
        2000, [this]() { return GetClientSession()->SupportsWebTransport(); });
    if (!GetClientSession()->SupportsWebTransport()) {
      return nullptr;
    }

    quiche::HttpHeaderBlock headers;
    headers[":scheme"] = "https";
    headers[":authority"] = "localhost";
    headers[":path"] = path;
    headers[":method"] = "CONNECT";
    headers[":protocol"] = "webtransport";
    for (const auto& [key, value] : extra_headers) {
      headers[key] = std::string(value);
    }

    client_->SendMessage(headers, "", /*fin=*/false);
    QuicSpdyStream* stream = client_->latest_created_stream();
    if (stream->web_transport() == nullptr) {
      return nullptr;
    }
    WebTransportSessionId id = client_->latest_created_stream()->id();
    QuicSpdySession* client_session = GetClientSession();
    if (client_session->GetWebTransportSession(id) == nullptr) {
      return nullptr;
    }
    WebTransportHttp3* session = client_session->GetWebTransportSession(id);
    if (wait_for_server_response) {
      client_->WaitUntil(-1,
                         [stream]() { return stream->headers_decompressed(); });
      EXPECT_TRUE(session->ready());
    }
    return session;
  }

  NiceMock<MockWebTransportSessionVisitor>& SetupWebTransportVisitor(
      WebTransportHttp3* session) {
    auto visitor_owned =
        std::make_unique<NiceMock<MockWebTransportSessionVisitor>>();
    NiceMock<MockWebTransportSessionVisitor>& visitor = *visitor_owned;
    session->SetVisitor(std::move(visitor_owned));
    return visitor;
  }

  std::string ReadDataFromWebTransportStreamUntilFin(
      WebTransportStream* stream,
      MockWebTransportStreamVisitor* visitor = nullptr) {
    QuicStreamId id = stream->GetStreamId();
    std::string buffer;

    // Try reading data if immediately available.
    WebTransportStream::ReadResult result = stream->Read(&buffer);
    if (result.fin) {
      return buffer;
    }

    while (true) {
      bool can_read = false;
      if (visitor == nullptr) {
        auto visitor_owned = std::make_unique<MockWebTransportStreamVisitor>();
        visitor = visitor_owned.get();
        stream->SetVisitor(std::move(visitor_owned));
      }
      EXPECT_CALL(*visitor, OnCanRead())
          .WillRepeatedly(Assign(&can_read, true));
      client_->WaitUntil(5000 /*ms*/, [&can_read]() { return can_read; });
      if (!can_read) {
        ADD_FAILURE() << "Waiting for readable data on stream " << id
                      << " timed out";
        return buffer;
      }
      if (GetClientSession()->GetOrCreateSpdyDataStream(id) == nullptr) {
        ADD_FAILURE() << "Stream " << id
                      << " was deleted while waiting for incoming data";
        return buffer;
      }

      result = stream->Read(&buffer);
      if (result.fin) {
        return buffer;
      }
      if (result.bytes_read == 0) {
        ADD_FAILURE() << "No progress made while reading from stream "
                      << stream->GetStreamId();
        return buffer;
      }
    }
  }

  void ReadAllIncomingWebTransportUnidirectionalStreams(
      WebTransportSession* session) {
    while (true) {
      WebTransportStream* received_stream =
          session->AcceptIncomingUnidirectionalStream();
      if (received_stream == nullptr) {
        break;
      }
      received_webtransport_unidirectional_streams_.push_back(
          ReadDataFromWebTransportStreamUntilFin(received_stream));
    }
  }

  void WaitForNewConnectionIds() {
    // Wait until a new server CID is available for another migration.
    const auto* client_connection = GetClientConnection();
    while (!QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(
               client_connection) ||
           (!client_connection->client_connection_id().IsEmpty() &&
            !QuicConnectionPeer::HasSelfIssuedConnectionIdToConsume(
                client_connection))) {
      client_->client()->WaitForEvents();
    }
  }

  // TODO(b/154162689) Remove this method once PSK support is added for
  // QUIC+TLS.
  void InitializeAndCheckForTlsPskFailure(bool expect_client_failure = true) {
    connect_to_server_on_initialize_ = false;
    EXPECT_TRUE(Initialize());

    EXPECT_QUIC_BUG(
        CreateClientWithWriter(),
        expect_client_failure
            ? "QUIC client pre-shared keys not yet supported with TLS"
            : "QUIC server pre-shared keys not yet supported with TLS");

    // Reset the client and server state so that `TearDown()` can complete
    // successfully.
    pre_shared_key_client_ = "";
    pre_shared_key_server_ = "";

    StopServer();
    server_writer_ = new PacketDroppingTestWriter();
    StartServer();

    if (client_) {
      // If `client_` is populated it means that the `CreateClientWithWriter()`
      // call above ran in-process, in which case `client_` owns
      // `client_writer_` and we need to create a new one.
      client_writer_ = new PacketDroppingTestWriter();
    }
    CreateClientWithWriter();
  }

  void TestMultiPacketChaosProtection(int num_packets, bool drop_first_packet,
                                      bool kyber = false);

  quiche::test::ScopedEnvironmentForThreads environment_;
  bool initialized_;
  // If true, the Initialize() function will create |client_| and starts to
  // connect to the server.
  // Default is true.
  bool connect_to_server_on_initialize_;
  QuicSocketAddress server_address_;
  std::optional<QuicSocketAddress> server_listening_address_;
  std::string server_hostname_;
  QuicTestBackend memory_cache_backend_;
  std::unique_ptr<ServerThread> server_thread_;
  // This socket keeps the ephemeral port reserved so that the kernel doesn't
  // give it away while the server is shut down.
  QuicUdpSocketFd fd_;
  std::unique_ptr<QuicTestClient> client_;
  QuicConnectionDebugVisitor* connection_debug_visitor_ = nullptr;
  PacketDroppingTestWriter* client_writer_;
  PacketDroppingTestWriter* server_writer_;
  QuicConfig client_config_;
  QuicConfig server_config_;
  ParsedQuicVersion version_;
  ParsedQuicVersionVector client_supported_versions_;
  ParsedQuicVersionVector server_supported_versions_;
  QuicTagVector client_extra_copts_;
  size_t chlo_multiplier_;
  QuicTestServer::StreamFactory* stream_factory_;
  std::string pre_shared_key_client_;
  std::string pre_shared_key_server_;
  int override_server_connection_id_length_;
  int override_client_connection_id_length_ = -1;
  uint8_t expected_server_connection_id_length_;
  bool enable_web_transport_ = false;
  bool enable_mlkem_in_client_ = false;
  std::vector<std::string> received_webtransport_unidirectional_streams_;
  bool use_preferred_address_ = false;
  QuicSocketAddress server_preferred_address_;
  QuicPacketWriterParams packet_writer_params_;
};

// Run all end to end tests with all supported versions.
INSTANTIATE_TEST_SUITE_P(EndToEndTests, EndToEndTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(EndToEndTest, HandshakeSuccessful) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(server_thread_);
  server_thread_->WaitForCryptoHandshakeConfirmed();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicCryptoStream* client_crypto_stream =
      QuicSessionPeer::GetMutableCryptoStream(client_session);
  ASSERT_TRUE(client_crypto_stream);
  QuicStreamSequencer* client_sequencer =
      QuicStreamPeer::sequencer(client_crypto_stream);
  ASSERT_TRUE(client_sequencer);
  EXPECT_FALSE(
      QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(client_sequencer));

  // We've had bugs in the past where the connections could end up on the wrong
  // version. This was never diagnosed but could have been due to in-connection
  // version negotiation back when that existed. At this point in time, our test
  // setup ensures that connections here always use |version_|, but we add this
  // sanity check out of paranoia to catch a regression of this type.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(client_connection->version(), version_);

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  QuicConnection* server_connection = nullptr;
  QuicCryptoStream* server_crypto_stream = nullptr;
  QuicStreamSequencer* server_sequencer = nullptr;
  if (server_session != nullptr) {
    server_connection = server_session->connection();
    server_crypto_stream =
        QuicSessionPeer::GetMutableCryptoStream(server_session);
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  if (server_crypto_stream != nullptr) {
    server_sequencer = QuicStreamPeer::sequencer(server_crypto_stream);
  } else {
    ADD_FAILURE() << "Missing server crypto stream";
  }
  if (server_sequencer != nullptr) {
    EXPECT_FALSE(
        QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(server_sequencer));
  } else {
    ADD_FAILURE() << "Missing server sequencer";
  }
  if (server_connection != nullptr) {
    EXPECT_EQ(server_connection->version(), version_);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ExportKeyingMaterial) {
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls()) {
    return;
  }
  const char* kExportLabel = "label";
  const int kExportLen = 30;
  std::string client_keying_material_export, server_keying_material_export;

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(server_thread_);
  server_thread_->WaitForCryptoHandshakeConfirmed();

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  QuicCryptoStream* server_crypto_stream = nullptr;
  if (server_session != nullptr) {
    server_crypto_stream =
        QuicSessionPeer::GetMutableCryptoStream(server_session);
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  if (server_crypto_stream != nullptr) {
    ASSERT_TRUE(server_crypto_stream->ExportKeyingMaterial(
        kExportLabel, /*context=*/"", kExportLen,
        &server_keying_material_export));

  } else {
    ADD_FAILURE() << "Missing server crypto stream";
  }
  server_thread_->Resume();

  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicCryptoStream* client_crypto_stream =
      QuicSessionPeer::GetMutableCryptoStream(client_session);
  ASSERT_TRUE(client_crypto_stream);
  ASSERT_TRUE(client_crypto_stream->ExportKeyingMaterial(
      kExportLabel, /*context=*/"", kExportLen,
      &client_keying_material_export));
  ASSERT_EQ(client_keying_material_export.size(),
            static_cast<size_t>(kExportLen));
  EXPECT_EQ(client_keying_material_export, server_keying_material_export);
}

TEST_P(EndToEndTest, SimpleRequestResponse) {
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());
  if (version_.UsesHttp3()) {
    QuicSpdyClientSession* client_session = GetClientSession();
    ASSERT_TRUE(client_session);
    EXPECT_TRUE(QuicSpdySessionPeer::GetSendControlStream(client_session));
    EXPECT_TRUE(QuicSpdySessionPeer::GetReceiveControlStream(client_session));
    server_thread_->Pause();
    QuicSpdySession* server_session = GetServerSession();
    if (server_session != nullptr) {
      EXPECT_TRUE(QuicSpdySessionPeer::GetSendControlStream(server_session));
      EXPECT_TRUE(QuicSpdySessionPeer::GetReceiveControlStream(server_session));
    } else {
      ADD_FAILURE() << "Missing server session";
    }
    server_thread_->Resume();
  }
  QuicConnectionStats client_stats = GetClientConnection()->GetStats();
  EXPECT_TRUE(client_stats.handshake_completion_time.IsInitialized());
}

TEST_P(EndToEndTest, HandshakeConfirmed) {
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls()) {
    return;
  }
  SendSynchronousFooRequestAndCheckResponse();
  // Verify handshake state.
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_EQ(HANDSHAKE_CONFIRMED, client_session->GetHandshakeState());
  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  if (server_session != nullptr) {
    EXPECT_EQ(HANDSHAKE_CONFIRMED, server_session->GetHandshakeState());
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  server_thread_->Resume();
  client_->Disconnect();
}

TEST_P(EndToEndTest, InvalidSNI) {
  if (!version_.UsesTls()) {
    ASSERT_TRUE(Initialize());
    return;
  }

  SetQuicFlag(quic_client_allow_invalid_sni_for_test, true);
  server_hostname_ = "invalid!.example.com";
  ASSERT_FALSE(Initialize());

  QuicSpdySession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_THAT(client_session->error(),
              IsError(QUIC_HANDSHAKE_FAILED_INVALID_HOSTNAME));
  EXPECT_THAT(
      client_session->error_details(),
      HasSubstr(absl::StrCat("Invalid SNI provided: ", server_hostname_)));
}

// Two packet CHLO. The first one is buffered and acked by dispatcher, the
// second one causes session to be created.
TEST_P(EndToEndTest, TestDispatcherAckWithTwoPacketCHLO) {
  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 1);
  client_extra_copts_.push_back(kCHP1);
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  SendSynchronousFooRequestAndCheckResponse();
  if (!version_.UsesHttp3()) {
    QuicConnectionStats client_stats = GetClientConnection()->GetStats();
    EXPECT_TRUE(client_stats.handshake_completion_time.IsInitialized());
    return;
  }

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  ASSERT_NE(server_connection, nullptr);
  const QuicConnectionStats& server_stats = server_connection->GetStats();
  EXPECT_EQ(server_stats.packets_sent_by_dispatcher, 1u);

  const QuicDispatcherStats& dispatcher_stats = GetDispatcherStats();
  // The first CHLO packet is enqueued, the second causes session to be created.
  EXPECT_EQ(dispatcher_stats.packets_processed_with_unknown_cid, 2u);
  EXPECT_EQ(dispatcher_stats.packets_enqueued_early, 1u);
  EXPECT_EQ(dispatcher_stats.packets_enqueued_chlo, 0u);
  EXPECT_EQ(dispatcher_stats.packets_sent, 1u);

  server_thread_->Resume();
}

// Two packet CHLO. The first one is buffered (CHLO incomplete) and acked, the
// second one is lost and retransmitted with a new server-chosen connection ID.
TEST_P(EndToEndTest,
       TestDispatcherAckWithTwoPacketCHLO_SecondPacketRetransmitted) {
  if (!version_.HasIetfQuicFrames() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }

  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 2);
  std::string google_handshake_message(kEthernetMTU, 'a');
  client_config_.SetGoogleHandshakeMessageToSend(google_handshake_message);
  connect_to_server_on_initialize_ = false;
  override_server_connection_id_length_ = 16;
  ASSERT_TRUE(Initialize());

  // Instruct the client to drop the second CHLO packet, but not the first.
  client_writer_->set_passthrough_for_next_n_packets(1);
  client_writer_->set_fake_drop_first_n_packets(2);

  client_.reset(CreateQuicClient(client_writer_, /*connect=*/false));
  client_->client()->Initialize();

  SendSynchronousFooRequestAndCheckResponse();

  server_thread_->ScheduleAndWaitForCompletion([&] {
    const QuicDispatcherStats& dispatcher_stats = GetDispatcherStats();
    EXPECT_EQ(dispatcher_stats.sessions_created, 1u);
    EXPECT_EQ(dispatcher_stats.packets_sent, 1u);
    EXPECT_EQ(dispatcher_stats.packets_processed_with_unknown_cid, 2u);
    EXPECT_EQ(dispatcher_stats.packets_enqueued_early, 1u);
    EXPECT_EQ(dispatcher_stats.packets_enqueued_chlo, 0u);
    EXPECT_DEBUG_EQ(
        dispatcher_stats.packets_processed_with_replaced_cid_in_store, 1u);
  });
}

// Two packet CHLO. The first one is buffered (CHLO incomplete) and acked, the
// second one is buffered (session creation rate limited) but not acked.
TEST_P(EndToEndTest, TestDispatcherAckWithTwoPacketCHLO_BothBuffered) {
  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 1);
  std::string google_handshake_message(kEthernetMTU, 'a');
  client_config_.SetGoogleHandshakeMessageToSend(google_handshake_message);
  connect_to_server_on_initialize_ = false;
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    delete client_writer_;
    return;
  }

  // This will cause all CHLO packets to be buffered and no sessions created.
  server_thread_->ScheduleAndWaitForCompletion([&] {
    server_thread_->server()->set_max_sessions_to_create_per_socket_event(0);
    QuicDispatcherPeer::set_new_sessions_allowed_per_event_loop(GetDispatcher(),
                                                                0);
  });

  client_.reset(CreateQuicClient(client_writer_, /*connect=*/false));
  client_->client()->Initialize();
  client_->client()->StartConnect();
  ASSERT_TRUE(client_->connected());

  while (GetDispatcherStatsThreadSafe().packets_enqueued_chlo == 0) {
    ASSERT_TRUE(client_->connected());
    client_->client()->WaitForEvents();
  }

  server_thread_->ScheduleAndWaitForCompletion([&] {
    const QuicDispatcherStats& dispatcher_stats = GetDispatcherStats();
    EXPECT_EQ(dispatcher_stats.packets_enqueued_chlo, 1u);
    EXPECT_EQ(dispatcher_stats.packets_enqueued_early, 1u);
    EXPECT_EQ(dispatcher_stats.packets_processed_with_unknown_cid, 2u);
    // 2 CHLO packets are enqueued, but only the 1st caused a dispatcher ACK.
    EXPECT_EQ(dispatcher_stats.packets_sent, 1u);
    EXPECT_EQ(dispatcher_stats.sessions_created, 0u);

    GetDispatcher()->ProcessBufferedChlos(1);
    EXPECT_EQ(dispatcher_stats.sessions_created, 1u);
  });

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
}

// Three packet CHLO. The first two are buffered and acked by dispatcher, the
// third one causes session to be created.
TEST_P(EndToEndTest, TestDispatcherAckWithThreePacketCHLO) {
  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 2);
  client_extra_copts_.push_back(kCHP2);
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  SendSynchronousFooRequestAndCheckResponse();
  if (!version_.UsesHttp3()) {
    QuicConnectionStats client_stats = GetClientConnection()->GetStats();
    EXPECT_TRUE(client_stats.handshake_completion_time.IsInitialized());
    return;
  }

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  ASSERT_NE(server_connection, nullptr);
  const QuicConnectionStats& server_stats = server_connection->GetStats();
  EXPECT_EQ(server_stats.packets_sent_by_dispatcher, 2u);

  const QuicDispatcherStats& dispatcher_stats = GetDispatcherStats();
  // The first and second CHLO packets are enqueued, the third causes session to
  // be created.
  EXPECT_EQ(dispatcher_stats.packets_processed_with_unknown_cid, 3u);
  EXPECT_EQ(dispatcher_stats.packets_enqueued_early, 2u);
  EXPECT_EQ(dispatcher_stats.packets_enqueued_chlo, 0u);
  EXPECT_EQ(dispatcher_stats.packets_sent, 2u);
  server_thread_->Resume();
}

// Three packet CHLO. The first one is buffered and acked by dispatcher, the
// second one is buffered but not acked due to --max_ack_sent_per_connection,
// the third one causes session to be created.
TEST_P(EndToEndTest,
       TestDispatcherAckWithThreePacketCHLO_AckCountLimitedByFlag) {
  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 1);
  std::string google_handshake_message(2 * kEthernetMTU, 'a');
  client_config_.SetGoogleHandshakeMessageToSend(google_handshake_message);
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  SendSynchronousFooRequestAndCheckResponse();
  if (!version_.UsesHttp3()) {
    QuicConnectionStats client_stats = GetClientConnection()->GetStats();
    EXPECT_TRUE(client_stats.handshake_completion_time.IsInitialized());
    return;
  }

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  ASSERT_NE(server_connection, nullptr);
  const QuicConnectionStats& server_stats = server_connection->GetStats();
  EXPECT_EQ(server_stats.packets_sent_by_dispatcher, 1u);

  const QuicDispatcherStats& dispatcher_stats = GetDispatcherStats();
  // The first and second CHLO packets are enqueued, the third causes session to
  // be created.
  EXPECT_EQ(dispatcher_stats.packets_processed_with_unknown_cid, 3u);
  EXPECT_EQ(dispatcher_stats.packets_enqueued_early, 2u);
  EXPECT_EQ(dispatcher_stats.packets_enqueued_chlo, 0u);
  EXPECT_EQ(dispatcher_stats.packets_sent, 1u);
  server_thread_->Resume();
}

// Three packet CHLO. The first one is buffered (CHLO incomplete) and acked, the
// other two are lost and retransmitted with a new server-chosen connection ID.
TEST_P(EndToEndTest,
       TestDispatcherAckWithThreePacketCHLO_SecondAndThirdRetransmitted) {
  if (!version_.HasIetfQuicFrames() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }

  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 2);
  std::string google_handshake_message(2 * kEthernetMTU, 'a');
  client_config_.SetGoogleHandshakeMessageToSend(google_handshake_message);
  connect_to_server_on_initialize_ = false;
  override_server_connection_id_length_ = 16;
  ASSERT_TRUE(Initialize());

  // Instruct the client to drop the second CHLO packet, but not the first.
  client_writer_->set_passthrough_for_next_n_packets(1);
  client_writer_->set_fake_drop_first_n_packets(3);

  client_.reset(CreateQuicClient(client_writer_, /*connect=*/false));
  client_->client()->Initialize();

  SendSynchronousFooRequestAndCheckResponse();

  server_thread_->ScheduleAndWaitForCompletion([&] {
    const QuicDispatcherStats& dispatcher_stats = GetDispatcherStats();
    EXPECT_EQ(dispatcher_stats.sessions_created, 1u);

    // Packet 1 and Packet 2's retransmission caused dispatcher ACKs.
    EXPECT_EQ(dispatcher_stats.packets_sent, 2u);
    EXPECT_EQ(dispatcher_stats.packets_processed_with_unknown_cid, 3u);
    EXPECT_EQ(dispatcher_stats.packets_enqueued_early, 2u);
    EXPECT_EQ(dispatcher_stats.packets_enqueued_chlo, 0u);
    EXPECT_DEBUG_EQ(
        dispatcher_stats.packets_processed_with_replaced_cid_in_store, 2u);
  });
}

TEST_P(EndToEndTest, SendAndReceiveCoalescedPackets) {
  ASSERT_TRUE(Initialize());
  if (!version_.CanSendCoalescedPackets()) {
    return;
  }
  SendSynchronousFooRequestAndCheckResponse();
  // Verify client successfully processes coalesced packets.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionStats client_stats = client_connection->GetStats();
  EXPECT_LT(0u, client_stats.num_coalesced_packets_received);
  EXPECT_EQ(client_stats.num_coalesced_packets_processed,
            client_stats.num_coalesced_packets_received);
  // TODO(fayang): verify server successfully processes coalesced packets.
}

// Simple transaction, but set a non-default ack delay at the client
// and ensure it gets to the server.
TEST_P(EndToEndTest, SimpleRequestResponseWithAckDelayChange) {
  // Force the ACK delay to be something other than the default.
  const uint32_t kClientMaxAckDelay = GetDefaultDelayedAckTimeMs() + 100u;
  client_config_.SetMaxAckDelayToSendMs(kClientMaxAckDelay);
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  server_thread_->Pause();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();
  if (server_sent_packet_manager != nullptr) {
    EXPECT_EQ(
        kClientMaxAckDelay,
        server_sent_packet_manager->peer_max_ack_delay().ToMilliseconds());
  } else {
    ADD_FAILURE() << "Missing server sent packet manager";
  }
  server_thread_->Resume();
}

// Simple transaction, but set a non-default ack exponent at the client
// and ensure it gets to the server.
TEST_P(EndToEndTest, SimpleRequestResponseWithAckExponentChange) {
  const uint32_t kClientAckDelayExponent = 19;
  EXPECT_NE(kClientAckDelayExponent, kDefaultAckDelayExponent);
  // Force the ACK exponent to be something other than the default.
  // Note that it is sent only with QUIC+TLS.
  client_config_.SetAckDelayExponentToSend(kClientAckDelayExponent);
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();

  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    if (version_.UsesTls()) {
      // Should be only sent with QUIC+TLS.
      EXPECT_EQ(kClientAckDelayExponent,
                server_connection->framer().peer_ack_delay_exponent());
    } else {
      // No change for QUIC_CRYPTO.
      EXPECT_EQ(kDefaultAckDelayExponent,
                server_connection->framer().peer_ack_delay_exponent());
    }
    // No change, regardless of version.
    EXPECT_EQ(kDefaultAckDelayExponent,
              server_connection->framer().local_ack_delay_exponent());
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, SimpleRequestResponseForcedVersionNegotiation) {
  client_supported_versions_.insert(client_supported_versions_.begin(),
                                    QuicVersionReservedForNegotiation());
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  connection_debug_visitor_ = &visitor;
  EXPECT_CALL(visitor, OnVersionNegotiationPacket(_)).Times(1);
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(ServerSendsVersionNegotiation());

  SendSynchronousFooRequestAndCheckResponse();

  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());
}

TEST_P(EndToEndTest, ForcedVersionNegotiation) {
  client_supported_versions_.insert(client_supported_versions_.begin(),
                                    QuicVersionReservedForNegotiation());
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(ServerSendsVersionNegotiation());

  SendSynchronousFooRequestAndCheckResponse();
}

TEST_P(EndToEndTest, SimpleRequestResponseZeroConnectionID) {
  if (!version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_server_connection_id_length_ = 0;
  expected_server_connection_id_length_ = 0;
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(client_connection->connection_id(),
            QuicUtils::CreateZeroConnectionId(version_.transport_version));
}

TEST_P(EndToEndTest, ZeroConnectionID) {
  if (!version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_server_connection_id_length_ = 0;
  expected_server_connection_id_length_ = 0;
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(client_connection->connection_id(),
            QuicUtils::CreateZeroConnectionId(version_.transport_version));
}

TEST_P(EndToEndTest, BadConnectionIdLength) {
  if (!version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_server_connection_id_length_ = 9;
  ASSERT_TRUE(Initialize());
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(kQuicDefaultConnectionIdLength, client_->client()
                                                ->client_session()
                                                ->connection()
                                                ->connection_id()
                                                .length());
}

TEST_P(EndToEndTest, ClientConnectionId) {
  if (!version_.SupportsClientConnectionIds()) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_client_connection_id_length_ = kQuicDefaultConnectionIdLength;
  ASSERT_TRUE(Initialize());
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(override_client_connection_id_length_, client_->client()
                                                       ->client_session()
                                                       ->connection()
                                                       ->client_connection_id()
                                                       .length());
}

TEST_P(EndToEndTest, ForcedVersionNegotiationAndClientConnectionId) {
  if (!version_.SupportsClientConnectionIds()) {
    ASSERT_TRUE(Initialize());
    return;
  }
  client_supported_versions_.insert(client_supported_versions_.begin(),
                                    QuicVersionReservedForNegotiation());
  override_client_connection_id_length_ = kQuicDefaultConnectionIdLength;
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(ServerSendsVersionNegotiation());
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(override_client_connection_id_length_, client_->client()
                                                       ->client_session()
                                                       ->connection()
                                                       ->client_connection_id()
                                                       .length());
}

TEST_P(EndToEndTest, ForcedVersionNegotiationAndBadConnectionIdLength) {
  if (!version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }
  client_supported_versions_.insert(client_supported_versions_.begin(),
                                    QuicVersionReservedForNegotiation());
  override_server_connection_id_length_ = 9;
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(ServerSendsVersionNegotiation());
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(kQuicDefaultConnectionIdLength, client_->client()
                                                ->client_session()
                                                ->connection()
                                                ->connection_id()
                                                .length());
}

// Forced Version Negotiation with a client connection ID and a long
// connection ID.
TEST_P(EndToEndTest, ForcedVersNegoAndClientCIDAndLongCID) {
  if (!version_.SupportsClientConnectionIds() ||
      !version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ != kLongConnectionIdLength) {
    ASSERT_TRUE(Initialize());
    return;
  }
  client_supported_versions_.insert(client_supported_versions_.begin(),
                                    QuicVersionReservedForNegotiation());
  override_client_connection_id_length_ = 18;
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(ServerSendsVersionNegotiation());
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(kQuicDefaultConnectionIdLength, client_->client()
                                                ->client_session()
                                                ->connection()
                                                ->connection_id()
                                                .length());
  EXPECT_EQ(override_client_connection_id_length_, client_->client()
                                                       ->client_session()
                                                       ->connection()
                                                       ->client_connection_id()
                                                       .length());
}

TEST_P(EndToEndTest, MixGoodAndBadConnectionIdLengths) {
  if (!version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }

  // Start client_ which will use a bad connection ID length.
  override_server_connection_id_length_ = 9;
  ASSERT_TRUE(Initialize());
  override_server_connection_id_length_ = -1;

  // Start client2 which will use a good connection ID length.
  std::unique_ptr<QuicTestClient> client2(CreateQuicClient(nullptr));
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";
  client2->SendMessage(headers, "", /*fin=*/false);
  client2->SendData("eep", true);

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(kQuicDefaultConnectionIdLength, client_->client()
                                                ->client_session()
                                                ->connection()
                                                ->connection_id()
                                                .length());

  WaitForFooResponseAndCheckIt(client2.get());
  EXPECT_EQ(kQuicDefaultConnectionIdLength, client2->client()
                                                ->client_session()
                                                ->connection()
                                                ->connection_id()
                                                .length());
}

TEST_P(EndToEndTest, SimpleRequestResponseWithLargeReject) {
  chlo_multiplier_ = 1;
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  if (version_.UsesTls()) {
    // REJ messages are a QUIC crypto feature, so TLS always returns false.
    EXPECT_FALSE(client_->client()->ReceivedInchoateReject());
  } else {
    EXPECT_TRUE(client_->client()->ReceivedInchoateReject());
  }
}

TEST_P(EndToEndTest, SimpleRequestResponsev6) {
  server_address_ =
      QuicSocketAddress(QuicIpAddress::Loopback6(), server_address_.port());
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
}

TEST_P(EndToEndTest,
       ClientDoesNotAllowServerDataOnServerInitiatedBidirectionalStreams) {
  set_client_initial_max_stream_data_incoming_bidirectional(0);
  ASSERT_TRUE(Initialize());
  SendSynchronousFooRequestAndCheckResponse();
}

TEST_P(EndToEndTest,
       ServerDoesNotAllowClientDataOnServerInitiatedBidirectionalStreams) {
  set_server_initial_max_stream_data_outgoing_bidirectional(0);
  ASSERT_TRUE(Initialize());
  SendSynchronousFooRequestAndCheckResponse();
}

TEST_P(EndToEndTest,
       BothEndpointsDisallowDataOnServerInitiatedBidirectionalStreams) {
  set_client_initial_max_stream_data_incoming_bidirectional(0);
  set_server_initial_max_stream_data_outgoing_bidirectional(0);
  ASSERT_TRUE(Initialize());
  SendSynchronousFooRequestAndCheckResponse();
}

// Regression test for a bug where we would always fail to decrypt the first
// initial packet. Undecryptable packets can be seen after the handshake
// is complete due to dropping the initial keys at that point, so we only test
// for undecryptable packets before then.
TEST_P(EndToEndTest, NoUndecryptablePacketsBeforeHandshakeComplete) {
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();

  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionStats client_stats = client_connection->GetStats();
  EXPECT_EQ(
      0u,
      client_stats.undecryptable_packets_received_before_handshake_complete);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    QuicConnectionStats server_stats = server_connection->GetStats();
    EXPECT_EQ(
        0u,
        server_stats.undecryptable_packets_received_before_handshake_complete);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, SeparateFinPacket) {
  ASSERT_TRUE(Initialize());

  // Send a request in two parts: the request and then an empty packet with FIN.
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  client_->SendMessage(headers, "", /*fin=*/false);
  client_->SendData("", true);
  WaitForFooResponseAndCheckIt();

  // Now do the same thing but with a content length.
  headers["content-length"] = "3";
  client_->SendMessage(headers, "", /*fin=*/false);
  client_->SendData("foo", true);
  WaitForFooResponseAndCheckIt();
}

TEST_P(EndToEndTest, MultipleRequestResponse) {
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  SendSynchronousBarRequestAndCheckResponse();
}

TEST_P(EndToEndTest, MultipleRequestResponseZeroConnectionID) {
  if (!version_.AllowsVariableLengthConnectionIds() ||
      override_server_connection_id_length_ > -1) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_server_connection_id_length_ = 0;
  expected_server_connection_id_length_ = 0;
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  SendSynchronousBarRequestAndCheckResponse();
}

TEST_P(EndToEndTest, MultipleStreams) {
  // Verifies quic_test_client can track responses of all active streams.
  ASSERT_TRUE(Initialize());

  const int kNumRequests = 10;

  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  for (int i = 0; i < kNumRequests; ++i) {
    client_->SendMessage(headers, "bar", /*fin=*/true);
  }

  while (kNumRequests > client_->num_responses()) {
    client_->ClearPerRequestState();
    ASSERT_TRUE(WaitForFooResponseAndCheckIt());
  }
}

TEST_P(EndToEndTest, MultipleClients) {
  ASSERT_TRUE(Initialize());
  std::unique_ptr<QuicTestClient> client2(CreateQuicClient(nullptr));

  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  client_->SendMessage(headers, "", /*fin=*/false);
  client2->SendMessage(headers, "", /*fin=*/false);

  client_->SendData("bar", true);
  WaitForFooResponseAndCheckIt();

  client2->SendData("eep", true);
  WaitForFooResponseAndCheckIt(client2.get());
}

TEST_P(EndToEndTest, RequestOverMultiplePackets) {
  // Send a large enough request to guarantee fragmentation.
  std::string huge_request =
      "/some/path?query=" + std::string(kMaxOutgoingPacketSize, '.');
  AddToCache(huge_request, 200, kBarResponseBody);

  ASSERT_TRUE(Initialize());

  SendSynchronousRequestAndCheckResponse(huge_request, kBarResponseBody);
}

TEST_P(EndToEndTest, MultiplePacketsRandomOrder) {
  // Send a large enough request to guarantee fragmentation.
  std::string huge_request =
      "/some/path?query=" + std::string(kMaxOutgoingPacketSize, '.');
  AddToCache(huge_request, 200, kBarResponseBody);

  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(50);

  SendSynchronousRequestAndCheckResponse(huge_request, kBarResponseBody);
}

TEST_P(EndToEndTest, PostMissingBytes) {
  ASSERT_TRUE(Initialize());

  // Add a content length header with no body.
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  // This should be detected as stream fin without complete request,
  // triggering an error response.
  client_->SendCustomSynchronousRequest(headers, "");
  EXPECT_EQ(QuicSimpleServerStream::kErrorResponseBody,
            client_->response_body());
  CheckResponseHeaders("500");
}

TEST_P(EndToEndTest, LargePostNoPacketLoss) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // 1 MB body.
  std::string body(1024 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // TODO(ianswett): There should not be packet loss in this test, but on some
  // platforms the receive buffer overflows.
  VerifyCleanConnection(true);
}

// Marked as slow since this adds a real-clock one second of delay.
TEST_P(EndToEndTest, QUICHE_SLOW_TEST(LargePostNoPacketLoss1sRTT)) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(1000));

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // 100 KB body.
  std::string body(100 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostWithPacketLoss) {
  // Connect with lower fake packet loss than we'd like to test.
  // Until b/10126687 is fixed, losing handshake packets is pretty
  // brutal.
  // Disable blackhole detection as this test is testing loss recovery.
  client_extra_copts_.push_back(kNBHD);
  SetPacketLossPercentage(5);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  SetPacketLossPercentage(30);

  // 10 KB body.
  std::string body(1024 * 10, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  if (override_server_connection_id_length_ == -1) {
    // If the client sends a longer connection ID, we can end up with dropped
    // packets. The packets_dropped counter increments whenever a packet arrives
    // with a new server connection ID that is not INITIAL, RETRY, or 1-RTT.
    // With packet losses, we could easily lose a server INITIAL and have the
    // first observed server packet be HANDSHAKE.
    VerifyCleanConnection(true);
  }
}

// Regression test for b/80090281.
TEST_P(EndToEndTest, LargePostWithPacketLossAndAlwaysBundleWindowUpdates) {
  // Disable blackhole detection as this test is testing loss recovery.
  client_extra_copts_.push_back(kNBHD);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Normally server only bundles a retransmittable frame once every other
  // kMaxConsecutiveNonRetransmittablePackets ack-only packets. Setting the max
  // to 0 to reliably reproduce b/80090281.
  server_thread_->Schedule([this]() {
    QuicConnection* server_connection = GetServerConnection();
    if (server_connection != nullptr) {
      QuicConnectionPeer::
          SetMaxConsecutiveNumPacketsWithNoRetransmittableFrames(
              server_connection, 0);
    } else {
      ADD_FAILURE() << "Missing server connection";
    }
  });

  SetPacketLossPercentage(30);

  // 10 KB body.
  std::string body(1024 * 10, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTest, LargePostWithPacketLossAndBlockedSocket) {
  // Connect with lower fake packet loss than we'd like to test.  Until
  // b/10126687 is fixed, losing handshake packets is pretty brutal.
  // Disable blackhole detection as this test is testing loss recovery.
  client_extra_copts_.push_back(kNBHD);
  SetPacketLossPercentage(5);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  SetPacketLossPercentage(10);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // 10 KB body.
  std::string body(1024 * 10, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
}

TEST_P(EndToEndTest, LargePostNoPacketLossWithDelayAndReordering) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  // Both of these must be called when the writer is not actively used.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // 1 MB body.
  std::string body(1024 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
}

// TODO(b/214587920): make this test not rely on timeouts.
TEST_P(EndToEndTest, QUICHE_SLOW_TEST(AddressToken)) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(3));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));

  client_extra_copts_.push_back(kTRTT);
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  SendSynchronousFooRequestAndCheckResponse();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  ASSERT_TRUE(client_->client()->connected());
  SendSynchronousFooRequestAndCheckResponse();

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  QuicConnection* server_connection = GetServerConnection();
  if (server_session != nullptr && server_connection != nullptr) {
    // Verify address is validated via validating token received in INITIAL
    // packet.
    EXPECT_FALSE(
        server_connection->GetStats().address_validated_via_decrypting_packet);
    EXPECT_TRUE(server_connection->GetStats().address_validated_via_token);

    // Verify the server received a cached min_rtt from the token and used it as
    // the initial rtt.
    const CachedNetworkParameters* server_received_network_params =
        static_cast<const QuicCryptoServerStreamBase*>(
            server_session->GetCryptoStream())
            ->PreviousCachedNetworkParams();

    ASSERT_NE(server_received_network_params, nullptr);
    // QuicSentPacketManager::SetInitialRtt clamps the initial_rtt to between
    // [min_initial_rtt, max_initial_rtt].
    const QuicTime::Delta min_initial_rtt =
        QuicTime::Delta::FromMicroseconds(kMinTrustedInitialRoundTripTimeUs);
    const QuicTime::Delta max_initial_rtt =
        QuicTime::Delta::FromMicroseconds(kMaxInitialRoundTripTimeUs);
    const QuicTime::Delta expected_initial_rtt =
        std::max(min_initial_rtt,
                 std::min(max_initial_rtt,
                          QuicTime::Delta::FromMilliseconds(
                              server_received_network_params->min_rtt_ms())));
    EXPECT_EQ(
        server_connection->sent_packet_manager().GetRttStats()->initial_rtt(),
        expected_initial_rtt);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }

  server_thread_->Resume();

  client_->Disconnect();

  // Regression test for b/206087883.
  // Mock server crash.
  StopServer();

  // The handshake fails due to idle timeout.
  client_->Connect();
  ASSERT_FALSE(client_->client()->WaitForOneRttKeysAvailable());
  client_->WaitForWriteToFlush();
  client_->WaitForResponse();
  ASSERT_FALSE(client_->client()->connected());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_NETWORK_IDLE_TIMEOUT));

  // Server restarts.
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  // Client re-connect.
  client_->Connect();
  ASSERT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  client_->WaitForWriteToFlush();
  client_->WaitForResponse();
  ASSERT_TRUE(client_->client()->connected());
  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  server_thread_->Pause();
  server_session = GetServerSession();
  server_connection = GetServerConnection();
  // Verify address token is only used once.
  if (server_session != nullptr && server_connection != nullptr) {
    // Verify address is validated via decrypting packet.
    EXPECT_TRUE(
        server_connection->GetStats().address_validated_via_decrypting_packet);
    EXPECT_FALSE(server_connection->GetStats().address_validated_via_token);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();

  client_->Disconnect();
}

// Verify that client does not reuse a source address token.
// TODO(b/214587920): make this test not rely on timeouts.
TEST_P(EndToEndTest, QUICHE_SLOW_TEST(AddressTokenNotReusedByClient)) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(3));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));

  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  QuicCryptoClientConfig* client_crypto_config =
      client_->client()->crypto_config();
  QuicServerId server_id = client_->client()->server_id();

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_FALSE(GetClientSession()->EarlyDataAccepted());

  client_->Disconnect();

  QuicClientSessionCache* session_cache =
      static_cast<QuicClientSessionCache*>(client_crypto_config->session_cache());
  ASSERT_TRUE(
      !QuicClientSessionCachePeer::GetToken(session_cache, server_id).empty());

  // Pause the server thread again to blackhole packets from client.
  server_thread_->Pause();
  client_->Connect();
  EXPECT_FALSE(client_->client()->WaitForOneRttKeysAvailable());
  EXPECT_FALSE(client_->client()->connected());

  // Verify address token gets cleared.
  ASSERT_TRUE(
      QuicClientSessionCachePeer::GetToken(session_cache, server_id).empty());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, LargePostZeroRTTFailure) {
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls() &&
      GetQuicReloadableFlag(quic_require_handshake_confirmation)) {
    return;
  }

  std::string body(20480, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());
  VerifyCleanConnection(false);
}

// Regression test for b/168020146.
TEST_P(EndToEndTest, MultipleZeroRtt) {
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls() &&
      GetQuicReloadableFlag(quic_require_handshake_confirmation)) {
    return;
  }

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  client_->Disconnect();

  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  client_->Disconnect();
}

TEST_P(EndToEndTest, SynchronousRequestZeroRTTFailure) {
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls() &&
      GetQuicReloadableFlag(quic_require_handshake_confirmation)) {
    return;
  }

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, LargePostSynchronousRequest) {
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  ASSERT_TRUE(Initialize());

  std::string body(20480, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_EQ((version_.UsesTls() ||
             !GetQuicReloadableFlag(quic_require_handshake_confirmation)),
            client_session->EarlyDataAccepted());
  EXPECT_EQ((version_.UsesTls() ||
             !GetQuicReloadableFlag(quic_require_handshake_confirmation)),
            client_->client()->EarlyDataAccepted());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  VerifyCleanConnection(false);
}

TEST_P(EndToEndTest, DisableResumption) {
  client_extra_copts_.push_back(kNRES);
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls()) {
    return;
  }
  SendSynchronousFooRequestAndCheckResponse();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_EQ(client_session->GetCryptoStream()->EarlyDataReason(),
            ssl_early_data_no_session_offered);
  client_->Disconnect();

  SendSynchronousFooRequestAndCheckResponse();
  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  if (GetQuicReloadableFlag(quic_enable_disable_resumption)) {
    EXPECT_EQ(client_session->GetCryptoStream()->EarlyDataReason(),
              ssl_early_data_session_not_resumed);
  } else {
    EXPECT_EQ(client_session->GetCryptoStream()->EarlyDataReason(),
              ssl_early_data_accepted);
  }
}

// This is a regression test for b/162595387
TEST_P(EndToEndTest, PostZeroRTTRequestDuringHandshake) {
  if (!version_.UsesTls()) {
    // This test is TLS specific.
    ASSERT_TRUE(Initialize());
    return;
  }
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  connection_debug_visitor_ = &visitor;
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // The 0-RTT handshake should succeed.
  ON_CALL(visitor, OnCryptoFrame(_))
      .WillByDefault([this](const QuicCryptoFrame& frame) {
        if (frame.level != ENCRYPTION_HANDSHAKE) {
          return;
        }
        // At this point in the handshake, the client should have derived
        // ENCRYPTION_ZERO_RTT keys (thus set encryption_established). It
        // should also have set ENCRYPTION_HANDSHAKE keys after receiving
        // the server's ENCRYPTION_INITIAL flight.
        EXPECT_TRUE(
            GetClientSession()->GetCryptoStream()->encryption_established());
        EXPECT_TRUE(
            GetClientConnection()->framer().HasEncrypterOfEncryptionLevel(
                ENCRYPTION_HANDSHAKE));
        HttpHeaderBlock headers;
        headers[":method"] = "POST";
        headers[":path"] = "/foo";
        headers[":scheme"] = "https";
        headers[":authority"] = server_hostname_;
        EXPECT_GT(
            client_->SendMessage(headers, "", /*fin*/ true, /*flush*/ false),
            0);
      });
  client_->Connect();
  ASSERT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  client_->WaitForWriteToFlush();
  client_->WaitForResponse();
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->response_body());

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());
}

// Regression test for b/166836136.
TEST_P(EndToEndTest, RetransmissionAfterZeroRTTRejectBeforeOneRtt) {
  if (!version_.UsesTls()) {
    // This test is TLS specific.
    ASSERT_TRUE(Initialize());
    return;
  }
  // Send a request and then disconnect. This prepares the client to attempt
  // a 0-RTT handshake for the next request.
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  connection_debug_visitor_ = &visitor;
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  client_->Disconnect();

  // Restart the server so that the 0-RTT handshake will take 1 RTT.
  StopServer();
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();

  ON_CALL(visitor, OnZeroRttRejected(_)).WillByDefault([this]() {
    EXPECT_FALSE(GetClientSession()->IsEncryptionEstablished());
  });

  // The 0-RTT handshake should fail.
  client_->Connect();
  ASSERT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  client_->WaitForWriteToFlush();
  client_->WaitForResponse();
  ASSERT_TRUE(client_->client()->connected());

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
}

TEST_P(EndToEndTest, RejectWithPacketLoss) {
  // In this test, we intentionally drop the first packet from the
  // server, which corresponds with the initial REJ response from
  // the server.
  server_writer_->set_fake_drop_first_n_packets(1);
  ASSERT_TRUE(Initialize());
}

TEST_P(EndToEndTest, SetInitialReceivedConnectionOptions) {
  QuicTagVector initial_received_options;
  initial_received_options.push_back(kTBBR);
  initial_received_options.push_back(kIW10);
  initial_received_options.push_back(kPRST);
  EXPECT_TRUE(server_config_.SetInitialReceivedConnectionOptions(
      initial_received_options));

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  EXPECT_FALSE(server_config_.SetInitialReceivedConnectionOptions(
      initial_received_options));

  // Verify that server's configuration is correct.
  server_thread_->Pause();
  EXPECT_TRUE(server_config_.HasReceivedConnectionOptions());
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kTBBR));
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kIW10));
  EXPECT_TRUE(
      ContainsQuicTag(server_config_.ReceivedConnectionOptions(), kPRST));
}

TEST_P(EndToEndTest, LargePostSmallBandwidthLargeBuffer) {
  ASSERT_TRUE(Initialize());
  SetPacketSendDelay(QuicTime::Delta::FromMicroseconds(1));
  // 256KB per second with a 256KB buffer from server to client.  Wireless
  // clients commonly have larger buffers, but our max CWND is 200.
  server_writer_->set_max_bandwidth_and_buffer_size(
      QuicBandwidth::FromBytesPerSecond(256 * 1024), 256 * 1024);

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // 1 MB body.
  std::string body(1024 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  // This connection may drop packets, because the buffer is smaller than the
  // max CWND.
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTest, DoNotSetSendAlarmIfConnectionFlowControlBlocked) {
  // Regression test for b/14677858.
  // Test that the resume write alarm is not set in QuicConnection::OnCanWrite
  // if currently connection level flow control blocked. If set, this results in
  // an infinite loop in the EventLoop, as the alarm fires and is immediately
  // rescheduled.
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // Ensure both stream and connection level are flow control blocked by setting
  // the send window offset to 0.
  const uint64_t flow_control_window =
      server_config_.GetInitialStreamFlowControlWindowToSend();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  QuicSession* session = GetClientSession();
  ASSERT_TRUE(session);
  QuicStreamPeer::SetSendWindowOffset(stream, 0);
  QuicFlowControllerPeer::SetSendWindowOffset(session->flow_controller(), 0);
  EXPECT_TRUE(stream->IsFlowControlBlocked());
  EXPECT_TRUE(session->flow_controller()->IsBlocked());

  // Make sure that the stream has data pending so that it will be marked as
  // write blocked when it receives a stream level WINDOW_UPDATE.
  stream->WriteOrBufferBody("hello", false);

  // The stream now attempts to write, fails because it is still connection
  // level flow control blocked, and is added to the write blocked list.
  QuicWindowUpdateFrame window_update(kInvalidControlFrameId, stream->id(),
                                      2 * flow_control_window);
  stream->OnWindowUpdateFrame(window_update);

  // Prior to fixing b/14677858 this call would result in an infinite loop in
  // Chromium. As a proxy for detecting this, we now check whether the
  // send alarm is set after OnCanWrite. It should not be, as the
  // connection is still flow control blocked.
  session->connection()->OnCanWrite();

  EXPECT_FALSE(QuicConnectionPeer::GetSendAlarm(session->connection()).IsSet());
}

TEST_P(EndToEndTest, InvalidStream) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  std::string body(kMaxOutgoingPacketSize, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  // Force the client to write with a stream ID belonging to a nonexistent
  // server-side stream.
  QuicSpdySession* session = GetClientSession();
  ASSERT_TRUE(session);
  QuicSessionPeer::SetNextOutgoingBidirectionalStreamId(
      session, GetNthServerInitiatedBidirectionalId(0));

  client_->SendCustomSynchronousRequest(headers, body);
  EXPECT_THAT(client_->stream_error(),
              IsStreamError(QUIC_STREAM_CONNECTION_ERROR));
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_INVALID_STREAM_ID));
}

// Test that the server resets the stream if the client sends a request
// with overly large headers.
TEST_P(EndToEndTest, LargeHeaders) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  std::string body(kMaxOutgoingPacketSize, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["key1"] = std::string(15 * 1024, 'a');
  headers["key2"] = std::string(15 * 1024, 'a');
  headers["key3"] = std::string(15 * 1024, 'a');

  client_->SendCustomSynchronousRequest(headers, body);

  if (version_.UsesHttp3()) {
    // QuicSpdyStream::OnHeadersTooLarge() resets the stream with
    // QUIC_HEADERS_TOO_LARGE.  This is sent as H3_EXCESSIVE_LOAD, the closest
    // HTTP/3 error code, and translated back to QUIC_STREAM_EXCESSIVE_LOAD on
    // the receiving side.
    EXPECT_THAT(client_->stream_error(),
                IsStreamError(QUIC_STREAM_EXCESSIVE_LOAD));
  } else {
    EXPECT_THAT(client_->stream_error(), IsStreamError(QUIC_HEADERS_TOO_LARGE));
  }
  EXPECT_THAT(client_->connection_error(), IsQuicNoError());
}

TEST_P(EndToEndTest, EarlyResponseWithQuicStreamNoError) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  std::string large_body(1024 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  // Insert an invalid content_length field in request to trigger an early
  // response from server.
  headers["content-length"] = "-3";

  client_->SendCustomSynchronousRequest(headers, large_body);
  EXPECT_EQ("bad", client_->response_body());
  CheckResponseHeaders("500");
  EXPECT_THAT(client_->stream_error(), IsQuicStreamNoError());
  EXPECT_THAT(client_->connection_error(), IsQuicNoError());
}

// TODO(rch): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, QUIC_TEST_DISABLED_IN_CHROME(MultipleTermination)) {
  ASSERT_TRUE(Initialize());

  // Set the offset so we won't frame.  Otherwise when we pick up termination
  // before HTTP framing is complete, we send an error and close the stream,
  // and the second write is picked up as writing on a closed stream.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream != nullptr);
  QuicStreamPeer::SetStreamBytesWritten(3, stream);

  client_->SendData("bar", true);
  client_->WaitForWriteToFlush();

  // By default the stream protects itself from writes after terminte is set.
  // Override this to test the server handling buggy clients.
  QuicStreamPeer::SetWriteSideClosed(false, client_->GetOrCreateStream());

  EXPECT_QUIC_BUG(client_->SendData("eep", true), "Fin already buffered");
}

TEST_P(EndToEndTest, Timeout) {
  client_config_.SetIdleNetworkTimeout(QuicTime::Delta::FromMicroseconds(500));
  // Note: we do NOT ASSERT_TRUE: we may time out during initial handshake:
  // that's enough to validate timeout in this case.
  Initialize();
  while (client_->client()->connected()) {
    client_->client()->WaitForEvents();
  }
}

TEST_P(EndToEndTest, MaxDynamicStreamsLimitRespected) {
  // Set a limit on maximum number of incoming dynamic streams.
  // Make sure the limit is respected by the peer.
  const uint32_t kServerMaxDynamicStreams = 1;
  server_config_.SetMaxBidirectionalStreamsToSend(kServerMaxDynamicStreams);
  ASSERT_TRUE(Initialize());
  if (version_.HasIetfQuicFrames()) {
    // Do not run this test for /IETF QUIC. This test relies on the fact that
    // Google QUIC allows a small number of additional streams beyond the
    // negotiated limit, which is not supported in IETF QUIC. Note that the test
    // needs to be here, after calling Initialize(), because all tests end up
    // calling EndToEndTest::TearDown(), which asserts that Initialize has been
    // called and then proceeds to tear things down -- which fails if they are
    // not properly set up.
    return;
  }
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // Make the client misbehave after negotiation.
  const int kServerMaxStreams = kMaxStreamsMinimumIncrement + 1;
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicSessionPeer::SetMaxOpenOutgoingStreams(client_session,
                                             kServerMaxStreams + 1);

  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "3";

  // The server supports a small number of additional streams beyond the
  // negotiated limit. Open enough streams to go beyond that limit.
  for (int i = 0; i < kServerMaxStreams + 1; ++i) {
    client_->SendMessage(headers, "", /*fin=*/false);
  }
  client_->WaitForResponse();

  EXPECT_TRUE(client_->connected());
  EXPECT_THAT(client_->stream_error(), IsStreamError(QUIC_REFUSED_STREAM));
  EXPECT_THAT(client_->connection_error(), IsQuicNoError());
}

TEST_P(EndToEndTest, SetIndependentMaxDynamicStreamsLimits) {
  // Each endpoint can set max dynamic streams independently.
  const uint32_t kClientMaxDynamicStreams = 4;
  const uint32_t kServerMaxDynamicStreams = 3;
  client_config_.SetMaxBidirectionalStreamsToSend(kClientMaxDynamicStreams);
  server_config_.SetMaxBidirectionalStreamsToSend(kServerMaxDynamicStreams);
  client_config_.SetMaxUnidirectionalStreamsToSend(kClientMaxDynamicStreams);
  server_config_.SetMaxUnidirectionalStreamsToSend(kServerMaxDynamicStreams);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // The client has received the server's limit and vice versa.
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  // The value returned by max_allowed... includes the Crypto and Header
  // stream (created as a part of initialization). The config. values,
  // above, are treated as "number of requests/responses" - that is, they do
  // not include the static Crypto and Header streams. Reduce the value
  // returned by max_allowed... by 2 to remove the static streams from the
  // count.
  size_t client_max_open_outgoing_bidirectional_streams =
      version_.HasIetfQuicFrames()
          ? QuicSessionPeer::ietf_streamid_manager(client_session)
                ->max_outgoing_bidirectional_streams()
          : QuicSessionPeer::GetStreamIdManager(client_session)
                ->max_open_outgoing_streams();
  size_t client_max_open_outgoing_unidirectional_streams =
      version_.HasIetfQuicFrames()
          ? QuicSessionPeer::ietf_streamid_manager(client_session)
                    ->max_outgoing_unidirectional_streams() -
                kHttp3StaticUnidirectionalStreamCount
          : QuicSessionPeer::GetStreamIdManager(client_session)
                ->max_open_outgoing_streams();
  EXPECT_EQ(kServerMaxDynamicStreams,
            client_max_open_outgoing_bidirectional_streams);
  EXPECT_EQ(kServerMaxDynamicStreams,
            client_max_open_outgoing_unidirectional_streams);
  server_thread_->Pause();
  QuicSession* server_session = GetServerSession();
  if (server_session != nullptr) {
    size_t server_max_open_outgoing_bidirectional_streams =
        version_.HasIetfQuicFrames()
            ? QuicSessionPeer::ietf_streamid_manager(server_session)
                  ->max_outgoing_bidirectional_streams()
            : QuicSessionPeer::GetStreamIdManager(server_session)
                  ->max_open_outgoing_streams();
    size_t server_max_open_outgoing_unidirectional_streams =
        version_.HasIetfQuicFrames()
            ? QuicSessionPeer::ietf_streamid_manager(server_session)
                      ->max_outgoing_unidirectional_streams() -
                  kHttp3StaticUnidirectionalStreamCount
            : QuicSessionPeer::GetStreamIdManager(server_session)
                  ->max_open_outgoing_streams();
    EXPECT_EQ(kClientMaxDynamicStreams,
              server_max_open_outgoing_bidirectional_streams);
    EXPECT_EQ(kClientMaxDynamicStreams,
              server_max_open_outgoing_unidirectional_streams);
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, NegotiateCongestionControl) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  CongestionControlType expected_congestion_control_type = kRenoBytes;
  switch (GetParam().congestion_control_tag) {
    case kRENO:
      expected_congestion_control_type = kRenoBytes;
      break;
    case kTBBR:
      expected_congestion_control_type = kBBR;
      break;
    case kQBIC:
      expected_congestion_control_type = kCubicBytes;
      break;
    case kB2ON:
      expected_congestion_control_type = kBBRv2;
      break;
    default:
      QUIC_DLOG(FATAL) << "Unexpected congestion control tag";
  }

  server_thread_->Pause();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();
  if (server_sent_packet_manager != nullptr) {
    EXPECT_EQ(
        expected_congestion_control_type,
        QuicSentPacketManagerPeer::GetSendAlgorithm(*server_sent_packet_manager)
            ->GetCongestionControlType());
  } else {
    ADD_FAILURE() << "Missing server sent packet manager";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ClientSuggestsRTT) {
  // Client suggests initial RTT, verify it is used.
  const QuicTime::Delta kInitialRTT = QuicTime::Delta::FromMicroseconds(20000);
  client_config_.SetInitialRoundTripTimeUsToSend(kInitialRTT.ToMicroseconds());

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(server_thread_);
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  const QuicSentPacketManager* client_sent_packet_manager =
      GetSentPacketManagerFromClientSession();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();
  if (client_sent_packet_manager != nullptr &&
      server_sent_packet_manager != nullptr) {
    EXPECT_EQ(kInitialRTT,
              client_sent_packet_manager->GetRttStats()->initial_rtt());
    EXPECT_EQ(kInitialRTT,
              server_sent_packet_manager->GetRttStats()->initial_rtt());
  } else {
    ADD_FAILURE() << "Missing sent packet manager";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ClientSuggestsIgnoredRTT) {
  // Client suggests initial RTT, but also specifies NRTT, so it's not used.
  const QuicTime::Delta kInitialRTT = QuicTime::Delta::FromMicroseconds(20000);
  client_config_.SetInitialRoundTripTimeUsToSend(kInitialRTT.ToMicroseconds());
  QuicTagVector options;
  options.push_back(kNRTT);
  client_config_.SetConnectionOptionsToSend(options);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(server_thread_);
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  const QuicSentPacketManager* client_sent_packet_manager =
      GetSentPacketManagerFromClientSession();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();
  if (client_sent_packet_manager != nullptr &&
      server_sent_packet_manager != nullptr) {
    EXPECT_EQ(kInitialRTT,
              client_sent_packet_manager->GetRttStats()->initial_rtt());
    EXPECT_EQ(kInitialRTT,
              server_sent_packet_manager->GetRttStats()->initial_rtt());
  } else {
    ADD_FAILURE() << "Missing sent packet manager";
  }
  server_thread_->Resume();
}

// Regression test for b/171378845
TEST_P(EndToEndTest, ClientDisablesGQuicZeroRtt) {
  if (version_.UsesTls()) {
    // This feature is gQUIC only.
    ASSERT_TRUE(Initialize());
    return;
  }
  QuicTagVector options;
  options.push_back(kQNZ2);
  client_config_.SetClientConnectionOptions(options);

  ASSERT_TRUE(Initialize());

  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_session->ReceivedInchoateReject());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->ReceivedInchoateReject());

  client_->Disconnect();

  // Make sure that the request succeeds but 0-RTT was not used.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_FALSE(client_->client()->EarlyDataAccepted());
}

TEST_P(EndToEndTest, MaxInitialRTT) {
  // Client tries to suggest twice the server's max initial rtt and the server
  // uses the max.
  client_config_.SetInitialRoundTripTimeUsToSend(2 *
                                                 kMaxInitialRoundTripTimeUs);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(server_thread_);
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  const QuicSentPacketManager* client_sent_packet_manager =
      GetSentPacketManagerFromClientSession();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();
  if (client_sent_packet_manager != nullptr &&
      server_sent_packet_manager != nullptr) {
    // Now that acks have been exchanged, the RTT estimate has decreased on the
    // server and is not infinite on the client.
    EXPECT_FALSE(
        client_sent_packet_manager->GetRttStats()->smoothed_rtt().IsInfinite());
    const RttStats* server_rtt_stats =
        server_sent_packet_manager->GetRttStats();
    EXPECT_EQ(static_cast<int64_t>(kMaxInitialRoundTripTimeUs),
              server_rtt_stats->initial_rtt().ToMicroseconds());
    EXPECT_GE(static_cast<int64_t>(kMaxInitialRoundTripTimeUs),
              server_rtt_stats->smoothed_rtt().ToMicroseconds());
  } else {
    ADD_FAILURE() << "Missing sent packet manager";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, MinInitialRTT) {
  // Client tries to suggest 0 and the server uses the default.
  client_config_.SetInitialRoundTripTimeUsToSend(0);

  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  const QuicSentPacketManager* client_sent_packet_manager =
      GetSentPacketManagerFromClientSession();
  const QuicSentPacketManager* server_sent_packet_manager =
      GetSentPacketManagerFromFirstServerSession();
  if (client_sent_packet_manager != nullptr &&
      server_sent_packet_manager != nullptr) {
    // Now that acks have been exchanged, the RTT estimate has decreased on the
    // server and is not infinite on the client.
    EXPECT_FALSE(
        client_sent_packet_manager->GetRttStats()->smoothed_rtt().IsInfinite());
    // Expect the default rtt of 100ms.
    EXPECT_EQ(QuicTime::Delta::FromMilliseconds(100),
              server_sent_packet_manager->GetRttStats()->initial_rtt());
    // Ensure the bandwidth is valid.
    client_sent_packet_manager->BandwidthEstimate();
    server_sent_packet_manager->BandwidthEstimate();
  } else {
    ADD_FAILURE() << "Missing sent packet manager";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ResetConnection) {
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  client_->ResetConnection();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  SendSynchronousBarRequestAndCheckResponse();
}

// Regression test for b/180737158.
TEST_P(
    EndToEndTest,
    HalfRttResponseBlocksShloRetransmissionWithoutTokenBasedAddressValidation) {
  // Turn off token based address validation to make the server get constrained
  // by amplification factor during handshake.
  SetQuicFlag(quic_reject_retry_token_in_initial_packet, true);
  ASSERT_TRUE(Initialize());
  if (!version_.SupportsAntiAmplificationLimit()) {
    return;
  }
  // Perform a full 1-RTT handshake to get the new session ticket such that the
  // next connection will perform a 0-RTT handshake.
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  client_->Disconnect();

  server_thread_->Pause();
  // Drop the 1st server packet which is the coalesced INITIAL + HANDSHAKE +
  // 1RTT.
  PacketDroppingTestWriter* writer = new PacketDroppingTestWriter();
  writer->set_fake_drop_first_n_packets(1);
  QuicDispatcherPeer::UseWriter(
      QuicServerPeer::GetDispatcher(server_thread_->server()), writer);
  server_thread_->Resume();

  // Large response (100KB) for 0-RTT request.
  std::string large_body(102400, 'a');
  AddToCache("/large_response", 200, large_body);
  SendSynchronousRequestAndCheckResponse(client_.get(), "/large_response",
                                         large_body);
}

TEST_P(EndToEndTest, MaxStreamsUberTest) {
  // Connect with lower fake packet loss than we'd like to test.  Until
  // b/10126687 is fixed, losing handshake packets is pretty brutal.
  SetPacketLossPercentage(1);
  ASSERT_TRUE(Initialize());
  std::string large_body(10240, 'a');
  int max_streams = 100;

  AddToCache("/large_response", 200, large_body);

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  SetPacketLossPercentage(10);

  for (int i = 0; i < max_streams; ++i) {
    EXPECT_LT(0, client_->SendRequest("/large_response"));
  }

  // WaitForEvents waits 50ms and returns true if there are outstanding
  // requests.
  while (client_->client()->WaitForEvents()) {
    ASSERT_TRUE(client_->connected());
  }
}

TEST_P(EndToEndTest, StreamCancelErrorTest) {
  ASSERT_TRUE(Initialize());
  std::string small_body(256, 'a');

  AddToCache("/small_response", 200, small_body);

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  QuicSession* session = GetClientSession();
  ASSERT_TRUE(session);
  // Lose the request.
  SetPacketLossPercentage(100);
  EXPECT_LT(0, client_->SendRequest("/small_response"));
  client_->client()->WaitForEvents();
  // Transmit the cancel, and ensure the connection is torn down properly.
  SetPacketLossPercentage(0);
  QuicStreamId stream_id = GetNthClientInitiatedBidirectionalId(0);
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  const QuicPacketCount packets_sent_before =
      client_connection->GetStats().packets_sent;
  session->ResetStream(stream_id, QUIC_STREAM_CANCELLED);
  const QuicPacketCount packets_sent_now =
      client_connection->GetStats().packets_sent;

  if (version_.UsesHttp3()) {
    // QPACK decoder instructions and RESET_STREAM and STOP_SENDING frames are
    // sent in a single packet.
    EXPECT_EQ(packets_sent_before + 1, packets_sent_now);
  }

  // WaitForEvents waits 50ms and returns true if there are outstanding
  // requests.
  while (client_->client()->WaitForEvents()) {
    ASSERT_TRUE(client_->connected());
  }
  // It should be completely fine to RST a stream before any data has been
  // received for that stream.
  EXPECT_THAT(client_->connection_error(), IsQuicNoError());
}

TEST_P(EndToEndTest, ConnectionMigrationClientIPChanged) {
  ASSERT_TRUE(Initialize());
  if (GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress old_host =
      client_->client()->network_helper()->GetLatestClientAddress().host();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_host, new_host);
  ASSERT_TRUE(client_->client()->MigrateSocket(new_host));

  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();

  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Send another request.
  SendSynchronousBarRequestAndCheckResponse();
  // By the time the 2nd request is completed, the PATH_RESPONSE must have been
  // received by the server.
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_FALSE(server_connection->HasPendingPathValidation());
    EXPECT_EQ(1u, server_connection->GetStats().num_validated_peer_migration);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, IetfConnectionMigrationClientIPChangedMultipleTimes) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress host0 =
      client_->client()->network_helper()->GetLatestClientAddress().host();
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection != nullptr);

  // Migrate socket to a new IP address.
  QuicIpAddress host1 = TestLoopback(2);
  EXPECT_NE(host0, host1);
  ASSERT_TRUE(
      QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(client_connection));
  QuicConnectionId server_cid0 = client_connection->connection_id();
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(client_->client()->MigrateSocket(host1));
  QuicConnectionId server_cid1 = client_connection->connection_id();
  EXPECT_FALSE(server_cid1.IsEmpty());
  EXPECT_NE(server_cid0, server_cid1);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Send another request and wait for response making sure path response is
  // received at server.
  SendSynchronousBarRequestAndCheckResponse();

  // Migrate socket to a new IP address.
  WaitForNewConnectionIds();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  QuicIpAddress host2 = TestLoopback(3);
  EXPECT_NE(host0, host2);
  EXPECT_NE(host1, host2);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(client_->client()->MigrateSocket(host2));
  QuicConnectionId server_cid2 = client_connection->connection_id();
  EXPECT_FALSE(server_cid2.IsEmpty());
  EXPECT_NE(server_cid0, server_cid2);
  EXPECT_NE(server_cid1, server_cid2);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send another request using the new socket and wait for response making sure
  // path response is received at server.
  SendSynchronousBarRequestAndCheckResponse();
  EXPECT_EQ(2u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Migrate socket back to an old IP address.
  WaitForNewConnectionIds();
  EXPECT_EQ(2u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(client_->client()->MigrateSocket(host1));
  QuicConnectionId server_cid3 = client_connection->connection_id();
  EXPECT_FALSE(server_cid3.IsEmpty());
  EXPECT_NE(server_cid0, server_cid3);
  EXPECT_NE(server_cid1, server_cid3);
  EXPECT_NE(server_cid2, server_cid3);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  const auto* client_packet_creator =
      QuicConnectionPeer::GetPacketCreator(client_connection);
  EXPECT_TRUE(client_packet_creator->GetClientConnectionId().IsEmpty());
  EXPECT_EQ(server_cid3, client_packet_creator->GetServerConnectionId());

  // Send another request using the new socket and wait for response making sure
  // path response is received at server.
  SendSynchronousBarRequestAndCheckResponse();
  // Even this is an old path, server has forgotten about it and thus needs to
  // validate the path again.
  EXPECT_EQ(3u,
            client_connection->GetStats().num_connectivity_probing_received);

  WaitForNewConnectionIds();
  EXPECT_EQ(3u, client_connection->GetStats().num_retire_connection_id_sent);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  // By the time the 2nd request is completed, the PATH_RESPONSE must have been
  // received by the server.
  EXPECT_FALSE(server_connection->HasPendingPathValidation());
  EXPECT_EQ(3u, server_connection->GetStats().num_validated_peer_migration);
  EXPECT_EQ(server_cid3, server_connection->connection_id());
  const auto* server_packet_creator =
      QuicConnectionPeer::GetPacketCreator(server_connection);
  EXPECT_EQ(server_cid3, server_packet_creator->GetServerConnectionId());
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  server_connection)
                  .IsEmpty());
  EXPECT_EQ(4u, server_connection->GetStats().num_new_connection_id_sent);
  server_thread_->Resume();
}

TEST_P(EndToEndTest,
       ConnectionMigrationWithNonZeroConnectionIDClientIPChangedMultipleTimes) {
  if (!version_.HasIetfQuicFrames() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_client_connection_id_length_ = kQuicDefaultConnectionIdLength;
  ASSERT_TRUE(Initialize());
  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress host0 =
      client_->client()->network_helper()->GetLatestClientAddress().host();
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection != nullptr);

  {
    QuicConnection::ScopedPacketFlusher flusher(client_connection);
    if (client_connection->SupportsMultiplePacketNumberSpaces()) {
      if (client_connection->received_packet_manager()
              .GetEarliestAckTimeout()
              .IsInitialized()) {
        client_connection->SendAllPendingAcks();
      }
    } else {
      client_connection->SendAck();
    }
  }

  // Migrate socket to a new IP address.
  QuicIpAddress host1 = TestLoopback(2);
  EXPECT_NE(host0, host1);
  ASSERT_TRUE(
      QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(client_connection));
  QuicConnectionId server_cid0 = client_connection->connection_id();
  QuicConnectionId client_cid0 = client_connection->client_connection_id();
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(client_->client()->MigrateSocket(host1));
  QuicConnectionId server_cid1 = client_connection->connection_id();
  QuicConnectionId client_cid1 = client_connection->client_connection_id();
  EXPECT_FALSE(server_cid1.IsEmpty());
  EXPECT_FALSE(client_cid1.IsEmpty());
  EXPECT_NE(server_cid0, server_cid1);
  EXPECT_NE(client_cid0, client_cid1);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send another request to ensure that the server will have time to finish the
  // reverse path validation and send address token.
  SendSynchronousBarRequestAndCheckResponse();
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Migrate socket to a new IP address.
  WaitForNewConnectionIds();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(2u, client_connection->GetStats().num_new_connection_id_sent);
  QuicIpAddress host2 = TestLoopback(3);
  EXPECT_NE(host0, host2);
  EXPECT_NE(host1, host2);
  EXPECT_TRUE(client_->client()->MigrateSocket(host2));
  QuicConnectionId server_cid2 = client_connection->connection_id();
  QuicConnectionId client_cid2 = client_connection->client_connection_id();
  EXPECT_FALSE(server_cid2.IsEmpty());
  EXPECT_NE(server_cid0, server_cid2);
  EXPECT_NE(server_cid1, server_cid2);
  EXPECT_FALSE(client_cid2.IsEmpty());
  EXPECT_NE(client_cid0, client_cid2);
  EXPECT_NE(client_cid1, client_cid2);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_TRUE(QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send another request to ensure that the server will have time to finish the
  // reverse path validation and send address token.
  SendSynchronousBarRequestAndCheckResponse();
  EXPECT_EQ(2u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Migrate socket back to an old IP address.
  WaitForNewConnectionIds();
  EXPECT_EQ(2u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(3u, client_connection->GetStats().num_new_connection_id_sent);
  EXPECT_TRUE(client_->client()->MigrateSocket(host1));
  QuicConnectionId server_cid3 = client_connection->connection_id();
  QuicConnectionId client_cid3 = client_connection->client_connection_id();
  EXPECT_FALSE(server_cid3.IsEmpty());
  EXPECT_NE(server_cid0, server_cid3);
  EXPECT_NE(server_cid1, server_cid3);
  EXPECT_NE(server_cid2, server_cid3);
  EXPECT_FALSE(client_cid3.IsEmpty());
  EXPECT_NE(client_cid0, client_cid3);
  EXPECT_NE(client_cid1, client_cid3);
  EXPECT_NE(client_cid2, client_cid3);
  const auto* client_packet_creator =
      QuicConnectionPeer::GetPacketCreator(client_connection);
  EXPECT_EQ(client_cid3, client_packet_creator->GetClientConnectionId());
  EXPECT_EQ(server_cid3, client_packet_creator->GetServerConnectionId());
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send another request to ensure that the server will have time to finish the
  // reverse path validation and send address token.
  SendSynchronousBarRequestAndCheckResponse();
  // Even this is an old path, server has forgotten about it and thus needs to
  // validate the path again.
  EXPECT_EQ(3u,
            client_connection->GetStats().num_connectivity_probing_received);

  WaitForNewConnectionIds();
  EXPECT_EQ(3u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(4u, client_connection->GetStats().num_new_connection_id_sent);

  server_thread_->Pause();
  // By the time the 2nd request is completed, the PATH_RESPONSE must have been
  // received by the server.
  QuicConnection* server_connection = GetServerConnection();
  EXPECT_FALSE(server_connection->HasPendingPathValidation());
  EXPECT_EQ(3u, server_connection->GetStats().num_validated_peer_migration);
  EXPECT_EQ(server_cid3, server_connection->connection_id());
  EXPECT_EQ(client_cid3, server_connection->client_connection_id());
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  server_connection)
                  .IsEmpty());
  const auto* server_packet_creator =
      QuicConnectionPeer::GetPacketCreator(server_connection);
  EXPECT_EQ(client_cid3, server_packet_creator->GetClientConnectionId());
  EXPECT_EQ(server_cid3, server_packet_creator->GetServerConnectionId());
  EXPECT_EQ(3u, server_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(4u, server_connection->GetStats().num_new_connection_id_sent);
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ConnectionMigrationNewTokenForNewIp) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames() ||
      GetQuicFlag(quic_enforce_strict_amplification_factor)) {
    return;
  }
  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress old_host =
      client_->client()->network_helper()->GetLatestClientAddress().host();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_host, new_host);
  ASSERT_TRUE(client_->client()->MigrateSocket(new_host));

  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Send another request to ensure that the server will have time to finish the
  // reverse path validation and send address token.
  SendSynchronousBarRequestAndCheckResponse();

  client_->Disconnect();
  // The 0-RTT handshake should succeed.
  client_->Connect();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  ASSERT_TRUE(client_->client()->connected());
  SendSynchronousFooRequestAndCheckResponse();

  EXPECT_TRUE(GetClientSession()->EarlyDataAccepted());
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    // Verify address is validated via validating token received in INITIAL
    // packet.
    EXPECT_FALSE(
        server_connection->GetStats().address_validated_via_decrypting_packet);
    EXPECT_TRUE(server_connection->GetStats().address_validated_via_token);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
  client_->Disconnect();
}

// A writer which copies the packet and send the copy with a specified self
// address and then send the same packet with the original self address.
class DuplicatePacketWithSpoofedSelfAddressWriter
    : public QuicPacketWriterWrapper {
 public:
  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options,
                          const QuicPacketWriterParams& params) override {
    if (self_address_to_overwrite_.IsInitialized()) {
      // Send the same packet on the overwriting address before sending on the
      // actual self address.
      QuicPacketWriterWrapper::WritePacket(buffer, buf_len,
                                           self_address_to_overwrite_,
                                           peer_address, options, params);
    }
    return QuicPacketWriterWrapper::WritePacket(buffer, buf_len, self_address,
                                                peer_address, options, params);
  }

  void set_self_address_to_overwrite(const QuicIpAddress& self_address) {
    self_address_to_overwrite_ = self_address;
  }

 private:
  QuicIpAddress self_address_to_overwrite_;
};

TEST_P(EndToEndTest, ClientAddressSpoofedForSomePeriod) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  auto writer = new DuplicatePacketWithSpoofedSelfAddressWriter();
  client_.reset(CreateQuicClient(writer));

  // Make sure client has unused peer connection ID before migration.
  SendSynchronousFooRequestAndCheckResponse();
  ASSERT_TRUE(QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(
      GetClientConnection()));

  QuicIpAddress real_host =
      client_->client()->session()->connection()->self_address().host();
  ASSERT_TRUE(client_->MigrateSocket(real_host));
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(
      0u, GetClientConnection()->GetStats().num_connectivity_probing_received);
  EXPECT_EQ(
      real_host,
      client_->client()->network_helper()->GetLatestClientAddress().host());
  client_->WaitForDelayedAcks();

  std::string large_body(10240, 'a');
  AddToCache("/large_response", 200, large_body);

  QuicIpAddress spoofed_host = TestLoopback(2);
  writer->set_self_address_to_overwrite(spoofed_host);

  client_->SendRequest("/large_response");
  QuicConnection* client_connection = GetClientConnection();
  QuicPacketCount num_packets_received =
      client_connection->GetStats().packets_received;

  while (client_->client()->WaitForEvents() && client_->connected()) {
    if (client_connection->GetStats().packets_received > num_packets_received) {
      // Ideally the client won't receive any packets till the server finds out
      // the new client address is not working. But there are 2 corner cases:
      // 1) Before the server received the packet from spoofed address, it might
      // send packets to the real client address. So the client will immediately
      // switch back to use the original address;
      // 2) Between the server fails reverse path validation and the client
      // receives packets again, the client might sent some packets with the
      // spoofed address and triggers another migration.
      // In both corner cases, the attempted migration should fail and fall back
      // to the working path.
      writer->set_self_address_to_overwrite(QuicIpAddress());
    }
  }
  client_->WaitForResponse();
  EXPECT_EQ(large_body, client_->response_body());
}

TEST_P(EndToEndTest,
       AsynchronousConnectionMigrationClientIPChangedMultipleTimes) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(CreateQuicClient(nullptr));

  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress host0 =
      client_->client()->network_helper()->GetLatestClientAddress().host();
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionId server_cid0 = client_connection->connection_id();
  // Server should have one new connection ID upon handshake completion.
  ASSERT_TRUE(
      QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(client_connection));

  // Migrate socket to new IP address #1.
  QuicIpAddress host1 = TestLoopback(2);
  EXPECT_NE(host0, host1);
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host1));
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(host1, client_->client()->session()->self_address().host());
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);
  QuicConnectionId server_cid1 = client_connection->connection_id();
  EXPECT_NE(server_cid0, server_cid1);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();

  // Migrate socket to new IP address #2.
  WaitForNewConnectionIds();
  QuicIpAddress host2 = TestLoopback(3);
  EXPECT_NE(host0, host1);
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host2));

  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(host2, client_->client()->session()->self_address().host());
  EXPECT_EQ(2u,
            client_connection->GetStats().num_connectivity_probing_received);
  QuicConnectionId server_cid2 = client_connection->connection_id();
  EXPECT_NE(server_cid0, server_cid2);
  EXPECT_NE(server_cid1, server_cid2);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();

  // Migrate socket back to IP address #1.
  WaitForNewConnectionIds();
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host1));

  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(host1, client_->client()->session()->self_address().host());
  EXPECT_EQ(3u,
            client_connection->GetStats().num_connectivity_probing_received);
  QuicConnectionId server_cid3 = client_connection->connection_id();
  EXPECT_NE(server_cid0, server_cid3);
  EXPECT_NE(server_cid1, server_cid3);
  EXPECT_NE(server_cid2, server_cid3);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());

  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();
  server_thread_->Pause();
  const QuicConnection* server_connection = GetServerConnection();
  EXPECT_EQ(server_connection->connection_id(), server_cid3);
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  server_connection)
                  .IsEmpty());
  server_thread_->Resume();

  // There should be 1 new connection ID issued by the server.
  WaitForNewConnectionIds();
}

TEST_P(EndToEndTest,
       AsynchronousConnectionMigrationClientIPChangedWithNonEmptyClientCID) {
  if (!version_.HasIetfQuicFrames()) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_client_connection_id_length_ = kQuicDefaultConnectionIdLength;
  ASSERT_TRUE(Initialize());
  client_.reset(CreateQuicClient(nullptr));

  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress old_host =
      client_->client()->network_helper()->GetLatestClientAddress().host();
  auto* client_connection = GetClientConnection();
  QuicConnectionId client_cid0 = client_connection->client_connection_id();
  QuicConnectionId server_cid0 = client_connection->connection_id();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_host, new_host);
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(new_host));

  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(new_host, client_->client()->session()->self_address().host());
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);
  QuicConnectionId client_cid1 = client_connection->client_connection_id();
  QuicConnectionId server_cid1 = client_connection->connection_id();
  const auto* client_packet_creator =
      QuicConnectionPeer::GetPacketCreator(client_connection);
  EXPECT_EQ(client_cid1, client_packet_creator->GetClientConnectionId());
  EXPECT_EQ(server_cid1, client_packet_creator->GetServerConnectionId());
  // Send a request using the new socket.
  SendSynchronousBarRequestAndCheckResponse();

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  EXPECT_EQ(client_cid1, server_connection->client_connection_id());
  EXPECT_EQ(server_cid1, server_connection->connection_id());
  const auto* server_packet_creator =
      QuicConnectionPeer::GetPacketCreator(server_connection);
  EXPECT_EQ(client_cid1, server_packet_creator->GetClientConnectionId());
  EXPECT_EQ(server_cid1, server_packet_creator->GetServerConnectionId());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ConnectionMigrationClientPortChanged) {
  // Tests that the client's port can change during an established QUIC
  // connection, and that doing so does not result in the connection being
  // closed by the server.
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();

  // Store the client address which was used to send the first request.
  QuicSocketAddress old_address =
      client_->client()->network_helper()->GetLatestClientAddress();
  int old_fd = client_->client()->GetLatestFD();

  // Create a new socket before closing the old one, which will result in a new
  // ephemeral port.
  client_->client()->network_helper()->CreateUDPSocketAndBind(
      client_->client()->server_address(), client_->client()->bind_to_address(),
      client_->client()->local_port());

  // Stop listening and close the old FD.
  client_->client()->default_network_helper()->CleanUpUDPSocket(old_fd);

  // The packet writer needs to be updated to use the new FD.
  client_->client()->network_helper()->CreateQuicPacketWriter();

  // Change the internal state of the client and connection to use the new port,
  // this is done because in a real NAT rebinding the client wouldn't see any
  // port change, and so expects no change to incoming port.
  // This is kind of ugly, but needed as we are simply swapping out the client
  // FD rather than any more complex NAT rebinding simulation.
  int new_port =
      client_->client()->network_helper()->GetLatestClientAddress().port();
  client_->client()->default_network_helper()->SetClientPort(new_port);
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionPeer::SetSelfAddress(
      client_connection,
      QuicSocketAddress(client_connection->self_address().host(), new_port));

  // Send a second request, using the new FD.
  SendSynchronousBarRequestAndCheckResponse();

  // Verify that the client's ephemeral port is different.
  QuicSocketAddress new_address =
      client_->client()->network_helper()->GetLatestClientAddress();
  EXPECT_EQ(old_address.host(), new_address.host());
  EXPECT_NE(old_address.port(), new_address.port());

  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_FALSE(server_connection->HasPendingPathValidation());
    EXPECT_EQ(1u, server_connection->GetStats().num_validated_peer_migration);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, NegotiatedInitialCongestionWindow) {
  client_extra_copts_.push_back(kIW03);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    QuicPacketCount cwnd =
        server_connection->sent_packet_manager().initial_congestion_window();
    EXPECT_EQ(3u, cwnd);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, NegotiatedDoubledInitialCongestionWindow) {
  SetQuicReloadableFlag(quic_allow_client_enabled_2x_initial_cwnd, true);
  client_extra_copts_.push_back(kIW2X);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  ASSERT_NE(server_connection, nullptr);
  EXPECT_EQ(
      server_connection->sent_packet_manager().initial_congestion_window(),
      kInitialCongestionWindow * 2);
  server_thread_->Resume();

  QuicConnection* client_connection = GetClientConnection();
  ASSERT_NE(client_connection, nullptr);
  EXPECT_EQ(
      client_connection->sent_packet_manager().initial_congestion_window(),
      kInitialCongestionWindow);
}

TEST_P(EndToEndTest, DifferentFlowControlWindows) {
  // Client and server can set different initial flow control receive windows.
  // These are sent in CHLO/SHLO. Tests that these values are exchanged properly
  // in the crypto handshake.
  const uint32_t kClientStreamIFCW = 123456;
  const uint32_t kClientSessionIFCW = 234567;
  set_client_initial_stream_flow_control_receive_window(kClientStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kClientSessionIFCW);

  uint32_t kServerStreamIFCW = 32 * 1024;
  uint32_t kServerSessionIFCW = 48 * 1024;
  set_server_initial_stream_flow_control_receive_window(kServerStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kServerSessionIFCW);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Open a data stream to make sure the stream level flow control is updated.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  WriteHeadersOnStream(stream);
  stream->WriteOrBufferBody("hello", false);

  if (!version_.UsesTls()) {
    // IFWA only exists with QUIC_CRYPTO.
    // Client should have the right values for server's receive window.
    ASSERT_TRUE(client_->client()
                    ->client_session()
                    ->config()
                    ->HasReceivedInitialStreamFlowControlWindowBytes());
    EXPECT_EQ(kServerStreamIFCW,
              client_->client()
                  ->client_session()
                  ->config()
                  ->ReceivedInitialStreamFlowControlWindowBytes());
    ASSERT_TRUE(client_->client()
                    ->client_session()
                    ->config()
                    ->HasReceivedInitialSessionFlowControlWindowBytes());
    EXPECT_EQ(kServerSessionIFCW,
              client_->client()
                  ->client_session()
                  ->config()
                  ->ReceivedInitialSessionFlowControlWindowBytes());
  }
  EXPECT_EQ(kServerStreamIFCW, QuicStreamPeer::SendWindowOffset(stream));
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_EQ(kServerSessionIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                    client_session->flow_controller()));

  // Server should have the right values for client's receive window.
  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  if (server_session == nullptr) {
    ADD_FAILURE() << "Missing server session";
    server_thread_->Resume();
    return;
  }
  QuicConfig server_config = *server_session->config();
  EXPECT_EQ(kClientSessionIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                    server_session->flow_controller()));
  server_thread_->Resume();
  if (version_.UsesTls()) {
    // IFWA only exists with QUIC_CRYPTO.
    return;
  }
  ASSERT_TRUE(server_config.HasReceivedInitialStreamFlowControlWindowBytes());
  EXPECT_EQ(kClientStreamIFCW,
            server_config.ReceivedInitialStreamFlowControlWindowBytes());
  ASSERT_TRUE(server_config.HasReceivedInitialSessionFlowControlWindowBytes());
  EXPECT_EQ(kClientSessionIFCW,
            server_config.ReceivedInitialSessionFlowControlWindowBytes());
}

// Test negotiation of IFWA connection option.
TEST_P(EndToEndTest, NegotiatedServerInitialFlowControlWindow) {
  const uint32_t kClientStreamIFCW = 123456;
  const uint32_t kClientSessionIFCW = 234567;
  set_client_initial_stream_flow_control_receive_window(kClientStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kClientSessionIFCW);

  uint32_t kServerStreamIFCW = 32 * 1024;
  uint32_t kServerSessionIFCW = 48 * 1024;
  set_server_initial_stream_flow_control_receive_window(kServerStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kServerSessionIFCW);

  // Bump the window.
  const uint32_t kExpectedStreamIFCW = 1024 * 1024;
  const uint32_t kExpectedSessionIFCW = 1.5 * 1024 * 1024;
  client_extra_copts_.push_back(kIFWa);

  ASSERT_TRUE(Initialize());

  // Values are exchanged during crypto handshake, so wait for that to finish.
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  // Open a data stream to make sure the stream level flow control is updated.
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  WriteHeadersOnStream(stream);
  stream->WriteOrBufferBody("hello", false);

  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);

  if (!version_.UsesTls()) {
    // IFWA only exists with QUIC_CRYPTO.
    // Client should have the right values for server's receive window.
    ASSERT_TRUE(client_session->config()
                    ->HasReceivedInitialStreamFlowControlWindowBytes());
    EXPECT_EQ(kExpectedStreamIFCW,
              client_session->config()
                  ->ReceivedInitialStreamFlowControlWindowBytes());
    ASSERT_TRUE(client_session->config()
                    ->HasReceivedInitialSessionFlowControlWindowBytes());
    EXPECT_EQ(kExpectedSessionIFCW,
              client_session->config()
                  ->ReceivedInitialSessionFlowControlWindowBytes());
  }
  EXPECT_EQ(kExpectedStreamIFCW, QuicStreamPeer::SendWindowOffset(stream));
  EXPECT_EQ(kExpectedSessionIFCW, QuicFlowControllerPeer::SendWindowOffset(
                                      client_session->flow_controller()));
}

TEST_P(EndToEndTest, HeadersAndCryptoStreamsNoConnectionFlowControl) {
  // The special headers and crypto streams should be subject to per-stream flow
  // control limits, but should not be subject to connection level flow control
  const uint32_t kStreamIFCW = 32 * 1024;
  const uint32_t kSessionIFCW = 48 * 1024;
  set_client_initial_stream_flow_control_receive_window(kStreamIFCW);
  set_client_initial_session_flow_control_receive_window(kSessionIFCW);
  set_server_initial_stream_flow_control_receive_window(kStreamIFCW);
  set_server_initial_session_flow_control_receive_window(kSessionIFCW);

  ASSERT_TRUE(Initialize());

  // Wait for crypto handshake to finish. This should have contributed to the
  // crypto stream flow control window, but not affected the session flow
  // control window.
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicCryptoStream* crypto_stream =
      QuicSessionPeer::GetMutableCryptoStream(client_session);
  ASSERT_TRUE(crypto_stream);
  // In v47 and later, the crypto handshake (sent in CRYPTO frames) is not
  // subject to flow control.
  if (!version_.UsesCryptoFrames()) {
    EXPECT_LT(QuicStreamPeer::SendWindowSize(crypto_stream), kStreamIFCW);
  }
  // When stream type is enabled, control streams will send settings and
  // contribute to flow control windows, so this expectation is no longer valid.
  if (!version_.UsesHttp3()) {
    EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::SendWindowSize(
                                client_session->flow_controller()));
  }

  // Send a request with no body, and verify that the connection level window
  // has not been affected.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // No headers stream in IETF QUIC.
  if (version_.UsesHttp3()) {
    return;
  }

  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(client_session);
  ASSERT_TRUE(headers_stream);
  EXPECT_LT(QuicStreamPeer::SendWindowSize(headers_stream), kStreamIFCW);
  EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::SendWindowSize(
                              client_session->flow_controller()));

  // Server should be in a similar state: connection flow control window should
  // not have any bytes marked as received.
  server_thread_->Pause();
  QuicSession* server_session = GetServerSession();
  if (server_session != nullptr) {
    QuicFlowController* server_connection_flow_controller =
        server_session->flow_controller();
    EXPECT_EQ(kSessionIFCW, QuicFlowControllerPeer::ReceiveWindowSize(
                                server_connection_flow_controller));
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, FlowControlsSynced) {
  set_smaller_flow_control_receive_window();

  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  server_thread_->WaitForCryptoHandshakeConfirmed();

  QuicSpdySession* const client_session = GetClientSession();
  ASSERT_TRUE(client_session);

  if (version_.UsesHttp3()) {
    // Make sure that the client has received the initial SETTINGS frame, which
    // is sent in the first packet on the control stream.
    while (!QuicSpdySessionPeer::GetReceiveControlStream(client_session)) {
      client_->client()->WaitForEvents();
      ASSERT_TRUE(client_->connected());
    }
  }

  // Make sure that all data sent by the client has been received by the server
  // (and the ack received by the client).
  while (client_session->HasUnackedStreamData()) {
    client_->client()->WaitForEvents();
    ASSERT_TRUE(client_->connected());
  }

  server_thread_->Pause();

  QuicSpdySession* const server_session = GetServerSession();
  if (server_session == nullptr) {
    ADD_FAILURE() << "Missing server session";
    server_thread_->Resume();
    return;
  }
  ExpectFlowControlsSynced(client_session, server_session);

  // Check control streams.
  if (version_.UsesHttp3()) {
    ExpectFlowControlsSynced(
        QuicSpdySessionPeer::GetReceiveControlStream(client_session),
        QuicSpdySessionPeer::GetSendControlStream(server_session));
    ExpectFlowControlsSynced(
        QuicSpdySessionPeer::GetSendControlStream(client_session),
        QuicSpdySessionPeer::GetReceiveControlStream(server_session));
  }

  // Check crypto stream.
  if (!version_.UsesCryptoFrames()) {
    ExpectFlowControlsSynced(
        QuicSessionPeer::GetMutableCryptoStream(client_session),
        QuicSessionPeer::GetMutableCryptoStream(server_session));
  }

  // Check headers stream.
  if (!version_.UsesHttp3()) {
    SpdyFramer spdy_framer(SpdyFramer::ENABLE_COMPRESSION);
    SpdySettingsIR settings_frame;
    settings_frame.AddSetting(spdy::SETTINGS_MAX_HEADER_LIST_SIZE,
                              kDefaultMaxUncompressedHeaderSize);
    SpdySerializedFrame frame(spdy_framer.SerializeFrame(settings_frame));

    QuicHeadersStream* client_header_stream =
        QuicSpdySessionPeer::GetHeadersStream(client_session);
    QuicHeadersStream* server_header_stream =
        QuicSpdySessionPeer::GetHeadersStream(server_session);
    // Both client and server are sending this SETTINGS frame, and the send
    // window is consumed. But because of timing issue, the server may send or
    // not send the frame, and the client may send/ not send / receive / not
    // receive the frame.
    // TODO(fayang): Rewrite this part because it is hacky.
    QuicByteCount win_difference1 =
        QuicStreamPeer::ReceiveWindowSize(server_header_stream) -
        QuicStreamPeer::SendWindowSize(client_header_stream);
    if (win_difference1 != 0) {
      EXPECT_EQ(frame.size(), win_difference1);
    }

    QuicByteCount win_difference2 =
        QuicStreamPeer::ReceiveWindowSize(client_header_stream) -
        QuicStreamPeer::SendWindowSize(server_header_stream);
    if (win_difference2 != 0) {
      EXPECT_EQ(frame.size(), win_difference2);
    }

    // Client *may* have received the SETTINGs frame.
    // TODO(fayang): Rewrite this part because it is hacky.
    float ratio1 = static_cast<float>(QuicFlowControllerPeer::ReceiveWindowSize(
                       client_session->flow_controller())) /
                   QuicStreamPeer::ReceiveWindowSize(
                       QuicSpdySessionPeer::GetHeadersStream(client_session));
    float ratio2 = static_cast<float>(QuicFlowControllerPeer::ReceiveWindowSize(
                       client_session->flow_controller())) /
                   (QuicStreamPeer::ReceiveWindowSize(
                        QuicSpdySessionPeer::GetHeadersStream(client_session)) +
                    frame.size());
    EXPECT_TRUE(ratio1 == kSessionToStreamRatio ||
                ratio2 == kSessionToStreamRatio);
  }

  server_thread_->Resume();
}

TEST_P(EndToEndTest, RequestWithNoBodyWillNeverSendStreamFrameWithFIN) {
  // A stream created on receipt of a simple request with no body will never get
  // a stream frame with a FIN. Verify that we don't keep track of the stream in
  // the locally closed streams map: it will never be removed if so.
  ASSERT_TRUE(Initialize());

  // Send a simple headers only request, and receive response.
  SendSynchronousFooRequestAndCheckResponse();

  // Now verify that the server is not waiting for a final FIN or RST.
  server_thread_->Pause();
  QuicSession* server_session = GetServerSession();
  if (server_session != nullptr) {
    EXPECT_EQ(0u, QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(
                      server_session)
                      .size());
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  server_thread_->Resume();
}

// TestAckListener counts how many bytes are acked during its lifetime.
class TestAckListener : public QuicAckListenerInterface {
 public:
  TestAckListener() {}

  void OnPacketAcked(int acked_bytes,
                     QuicTime::Delta /*delta_largest_observed*/) override {
    total_bytes_acked_ += acked_bytes;
  }

  void OnPacketRetransmitted(int /*retransmitted_bytes*/) override {}

  int total_bytes_acked() const { return total_bytes_acked_; }

 protected:
  // Object is ref counted.
  ~TestAckListener() override {}

 private:
  int total_bytes_acked_ = 0;
};

class TestResponseListener : public QuicSpdyClientBase::ResponseListener {
 public:
  void OnCompleteResponse(QuicStreamId id,
                          const HttpHeaderBlock& response_headers,
                          absl::string_view response_body) override {
    QUIC_DVLOG(1) << "response for stream " << id << " "
                  << response_headers.DebugString() << "\n"
                  << response_body;
  }
};

TEST_P(EndToEndTest, AckNotifierWithPacketLossAndBlockedSocket) {
  // Verify that even in the presence of packet loss and occasionally blocked
  // socket, an AckNotifierDelegate will get informed that the data it is
  // interested in has been ACKed. This tests end-to-end ACK notification, and
  // demonstrates that retransmissions do not break this functionality.
  // Disable blackhole detection as this test is testing loss recovery.
  client_extra_copts_.push_back(kNBHD);
  SetPacketLossPercentage(5);
  ASSERT_TRUE(Initialize());
  // Wait for the server SHLO before upping the packet loss.
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  SetPacketLossPercentage(30);
  client_writer_->set_fake_blocked_socket_percentage(10);

  // Wait for SETTINGS frame from server that sets QPACK dynamic table capacity
  // to make sure request headers will be compressed using the dynamic table.
  if (version_.UsesHttp3()) {
    while (true) {
      // Waits for up to 50 ms.
      client_->client()->WaitForEvents();
      ASSERT_TRUE(client_->connected());
      QuicSpdyClientSession* client_session = GetClientSession();
      if (client_session == nullptr) {
        ADD_FAILURE() << "Missing client session";
        return;
      }
      QpackEncoder* qpack_encoder = client_session->qpack_encoder();
      if (qpack_encoder == nullptr) {
        ADD_FAILURE() << "Missing QPACK encoder";
        return;
      }
      QpackEncoderHeaderTable* header_table =
          QpackEncoderPeer::header_table(qpack_encoder);
      if (header_table == nullptr) {
        ADD_FAILURE() << "Missing header table";
        return;
      }
      if (header_table->dynamic_table_capacity() > 0) {
        break;
      }
    }
  }

  // Create a POST request and send the headers only.
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  // Here, we have to specify flush=false, otherwise we risk a race condition in
  // which the headers are sent and acknowledged before the ack notifier is
  // installed.
  client_->SendMessage(headers, "", /*fin=*/false, /*flush=*/false);

  // Size of headers on the request stream. This is zero if headers are sent on
  // the header stream.
  size_t header_size = 0;
  if (version_.UsesHttp3()) {
    // Determine size of headers after QPACK compression.
    NoopDecoderStreamErrorDelegate decoder_stream_error_delegate;
    NoopQpackStreamSenderDelegate encoder_stream_sender_delegate;
    QpackEncoder qpack_encoder(&decoder_stream_error_delegate,
                               HuffmanEncoding::kEnabled,
                               CookieCrumbling::kEnabled);
    qpack_encoder.set_qpack_stream_sender_delegate(
        &encoder_stream_sender_delegate);

    qpack_encoder.SetMaximumDynamicTableCapacity(
        kDefaultQpackMaxDynamicTableCapacity);
    qpack_encoder.SetDynamicTableCapacity(kDefaultQpackMaxDynamicTableCapacity);
    qpack_encoder.SetMaximumBlockedStreams(kDefaultMaximumBlockedStreams);

    std::string encoded_headers = qpack_encoder.EncodeHeaderList(
        /* stream_id = */ 0, headers, nullptr);
    header_size = encoded_headers.size();
  }

  // Test the AckNotifier's ability to track multiple packets by making the
  // request body exceed the size of a single packet.
  std::string request_string = "a request body bigger than one packet" +
                               std::string(kMaxOutgoingPacketSize, '.');

  const int expected_bytes_acked = header_size + request_string.length();

  // The TestAckListener will cause a failure if not notified.
  quiche::QuicheReferenceCountedPointer<TestAckListener> ack_listener(
      new TestAckListener());

  // Send the request, and register the delegate for ACKs.
  client_->SendData(request_string, true, ack_listener);
  WaitForFooResponseAndCheckIt();

  // Send another request to flush out any pending ACKs on the server.
  SendSynchronousBarRequestAndCheckResponse();

  // Make sure the delegate does get the notification it expects.
  int attempts = 0;
  constexpr int kMaxAttempts = 20;
  while (ack_listener->total_bytes_acked() < expected_bytes_acked) {
    // Waits for up to 50 ms.
    client_->client()->WaitForEvents();
    ASSERT_TRUE(client_->connected());
    if (++attempts >= kMaxAttempts) {
      break;
    }
  }
  EXPECT_EQ(ack_listener->total_bytes_acked(), expected_bytes_acked)
      << " header_size " << header_size << " request length "
      << request_string.length();
}

// Send a public reset from the server.
TEST_P(EndToEndTest, ServerSendPublicReset) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  QuicSpdySession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicConfig* config = client_session->config();
  ASSERT_TRUE(config);
  EXPECT_TRUE(config->HasReceivedStatelessResetToken());
  StatelessResetToken stateless_reset_token =
      config->ReceivedStatelessResetToken();

  // Send the public reset.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionId connection_id = client_connection->connection_id();
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_SERVER, kQuicDefaultConnectionIdLength);
  std::unique_ptr<QuicEncryptedPacket> packet =
      framer.BuildIetfStatelessResetPacket(
          connection_id, /*received_packet_length=*/100, stateless_reset_token);
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  auto client_address = client_connection->self_address();
  server_writer_->WritePacket(packet->data(), packet->length(),
                              server_address_.host(), client_address, nullptr,
                              packet_writer_params_);
  server_thread_->Resume();

  // The request should fail.
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  EXPECT_TRUE(client_->response_headers()->empty());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PUBLIC_RESET));
}

// Send a public reset from the server for a different connection ID.
// It should be ignored.
TEST_P(EndToEndTest, ServerSendPublicResetWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  QuicSpdySession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicConfig* config = client_session->config();
  ASSERT_TRUE(config);
  EXPECT_TRUE(config->HasReceivedStatelessResetToken());
  StatelessResetToken stateless_reset_token =
      config->ReceivedStatelessResetToken();
  // Send the public reset.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionId incorrect_connection_id = TestConnectionId(
      TestConnectionIdToUInt64(client_connection->connection_id()) + 1);
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_SERVER, kQuicDefaultConnectionIdLength);
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  client_connection->set_debug_visitor(&visitor);
  std::unique_ptr<QuicEncryptedPacket> packet =
      framer.BuildIetfStatelessResetPacket(incorrect_connection_id,
                                           /*received_packet_length=*/100,
                                           stateless_reset_token);
  EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
      .Times(0);
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  auto client_address = client_connection->self_address();
  server_writer_->WritePacket(packet->data(), packet->length(),
                              server_address_.host(), client_address, nullptr,
                              packet_writer_params_);
  server_thread_->Resume();

  // The request should fail. IETF stateless reset does not include connection
  // ID.
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  EXPECT_TRUE(client_->response_headers()->empty());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PUBLIC_RESET));

  client_connection->set_debug_visitor(nullptr);
}

TEST_P(EndToEndTest, InduceStatelessResetFromServer) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  SetPacketLossPercentage(100);  // Block PEER_GOING_AWAY message from server.
  StopServer(true);
  server_writer_ = new PacketDroppingTestWriter();
  StartServer();
  SetPacketLossPercentage(0);
  // The request should generate a public reset.
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  EXPECT_TRUE(client_->response_headers()->empty());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PUBLIC_RESET));
  EXPECT_FALSE(client_->connected());
}

// Send a public reset from the client for a different connection ID.
// It should be ignored.
TEST_P(EndToEndTest, ClientSendPublicResetWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  // Send the public reset.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionId incorrect_connection_id = TestConnectionId(
      TestConnectionIdToUInt64(client_connection->connection_id()) + 1);
  QuicPublicResetPacket header;
  header.connection_id = incorrect_connection_id;
  QuicFramer framer(server_supported_versions_, QuicTime::Zero(),
                    Perspective::IS_CLIENT, kQuicDefaultConnectionIdLength);
  std::unique_ptr<QuicEncryptedPacket> packet(
      framer.BuildPublicResetPacket(header));
  client_writer_->WritePacket(
      packet->data(), packet->length(),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr, packet_writer_params_);

  // The connection should be unaffected.
  SendSynchronousFooRequestAndCheckResponse();
}

// Send a version negotiation packet from the server for a different
// connection ID.  It should be ignored.
TEST_P(EndToEndTest, ServerSendVersionNegotiationWithDifferentConnectionId) {
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // Send the version negotiation packet.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionId incorrect_connection_id = TestConnectionId(
      TestConnectionIdToUInt64(client_connection->connection_id()) + 1);
  std::unique_ptr<QuicEncryptedPacket> packet(
      QuicFramer::BuildVersionNegotiationPacket(
          incorrect_connection_id, EmptyQuicConnectionId(), /*ietf_quic=*/true,
          version_.HasLengthPrefixedConnectionIds(),
          server_supported_versions_));
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  client_connection->set_debug_visitor(&visitor);
  EXPECT_CALL(visitor, OnIncorrectConnectionId(incorrect_connection_id))
      .Times(1);
  // We must pause the server's thread in order to call WritePacket without
  // race conditions.
  server_thread_->Pause();
  server_writer_->WritePacket(
      packet->data(), packet->length(), server_address_.host(),
      client_->client()->network_helper()->GetLatestClientAddress(), nullptr,
      packet_writer_params_);
  server_thread_->Resume();

  // The connection should be unaffected.
  SendSynchronousFooRequestAndCheckResponse();

  client_connection->set_debug_visitor(nullptr);
}

// DowngradePacketWriter is a client writer which will intercept all the client
// writes for |target_version| and reply to them with version negotiation
// packets to attempt a version downgrade attack. Once the client has downgraded
// to a different version, the writer stops intercepting. |server_thread| must
// start off paused, and will be resumed once interception is done.
class DowngradePacketWriter : public PacketDroppingTestWriter {
 public:
  explicit DowngradePacketWriter(
      const ParsedQuicVersion& target_version,
      const ParsedQuicVersionVector& supported_versions, QuicTestClient* client,
      QuicPacketWriter* server_writer, ServerThread* server_thread)
      : target_version_(target_version),
        supported_versions_(supported_versions),
        client_(client),
        server_writer_(server_writer),
        server_thread_(server_thread) {}
  ~DowngradePacketWriter() override {}

  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          quic::PerPacketOptions* options,
                          const quic::QuicPacketWriterParams& params) override {
    if (!intercept_enabled_) {
      return PacketDroppingTestWriter::WritePacket(
          buffer, buf_len, self_address, peer_address, options, params);
    }
    PacketHeaderFormat format;
    QuicLongHeaderType long_packet_type;
    bool version_present, has_length_prefix;
    QuicVersionLabel version_label;
    ParsedQuicVersion parsed_version = ParsedQuicVersion::Unsupported();
    absl::string_view destination_connection_id, source_connection_id;
    std::optional<absl::string_view> retry_token;
    std::string detailed_error;
    if (QuicFramer::ParsePublicHeaderDispatcher(
            QuicEncryptedPacket(buffer, buf_len),
            kQuicDefaultConnectionIdLength, &format, &long_packet_type,
            &version_present, &has_length_prefix, &version_label,
            &parsed_version, &destination_connection_id, &source_connection_id,
            &retry_token, &detailed_error) != QUIC_NO_ERROR) {
      ADD_FAILURE() << "Failed to parse our own packet: " << detailed_error;
      return WriteResult(WRITE_STATUS_ERROR, 0);
    }
    if (!version_present || parsed_version != target_version_) {
      // Client is sending with another version, the attack has succeeded so we
      // can stop intercepting.
      intercept_enabled_ = false;
      server_thread_->Resume();
      // Pass the client-sent packet through.
      return WritePacket(buffer, buf_len, self_address, peer_address, options,
                         params);
    }
    // Send a version negotiation packet.
    std::unique_ptr<QuicEncryptedPacket> packet(
        QuicFramer::BuildVersionNegotiationPacket(
            QuicConnectionId(destination_connection_id),
            QuicConnectionId(source_connection_id), /*ietf_quic=*/true,
            has_length_prefix, supported_versions_));
    QuicPacketWriterParams default_params;
    server_writer_->WritePacket(
        packet->data(), packet->length(), peer_address.host(),
        client_->client()->network_helper()->GetLatestClientAddress(), nullptr,
        default_params);
    // Drop the client-sent packet but pretend it was sent.
    return WriteResult(WRITE_STATUS_OK, buf_len);
  }

 private:
  bool intercept_enabled_ = true;
  ParsedQuicVersion target_version_;
  ParsedQuicVersionVector supported_versions_;
  QuicTestClient* client_;           // Unowned.
  QuicPacketWriter* server_writer_;  // Unowned.
  ServerThread* server_thread_;      // Unowned.
};

TEST_P(EndToEndTest, VersionNegotiationDowngradeAttackIsDetected) {
  ParsedQuicVersion target_version = server_supported_versions_.back();
  if (!version_.UsesTls() || target_version == version_) {
    ASSERT_TRUE(Initialize());
    return;
  }
  connect_to_server_on_initialize_ = false;
  client_supported_versions_.insert(client_supported_versions_.begin(),
                                    target_version);
  ParsedQuicVersionVector downgrade_versions{version_};
  ASSERT_TRUE(Initialize());
  ASSERT_TRUE(server_thread_);
  // Pause the server thread to allow our DowngradePacketWriter to write version
  // negotiation packets in a thread-safe manner. It will be resumed by the
  // DowngradePacketWriter.
  server_thread_->Pause();
  client_.reset(new QuicTestClient(server_address_, server_hostname_,
                                   client_config_, client_supported_versions_,
                                   crypto_test_utils::ProofVerifierForTesting(),
                                   std::make_unique<QuicClientSessionCache>()));
  delete client_writer_;
  client_writer_ = new DowngradePacketWriter(target_version, downgrade_versions,
                                             client_.get(), server_writer_,
                                             server_thread_.get());
  client_->UseWriter(client_writer_);
  // Have the client attempt to send a request.
  client_->Connect();
  EXPECT_TRUE(client_->SendSynchronousRequest("/foo").empty());
  // Make sure the downgrade is detected and the handshake fails.
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_HANDSHAKE_FAILED));
}

// A bad header shouldn't tear down the connection, because the receiver can't
// tell the connection ID.
TEST_P(EndToEndTest, BadPacketHeaderTruncated) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  SendSynchronousFooRequestAndCheckResponse();

  // Packet with invalid public flags.
  char packet[] = {// public flags (8 byte connection_id)
                   0x3C,
                   // truncated connection ID
                   0x11};
  client_writer_->WritePacket(
      &packet[0], sizeof(packet),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr, packet_writer_params_);
  EXPECT_TRUE(server_thread_->WaitUntil(
      [&] {
        return QuicDispatcherPeer::GetAndClearLastError(
                   QuicServerPeer::GetDispatcher(server_thread_->server())) ==
               QUIC_INVALID_PACKET_HEADER;
      },
      QuicTime::Delta::FromSeconds(5)));

  // The connection should not be terminated.
  SendSynchronousFooRequestAndCheckResponse();
}

// A bad header shouldn't tear down the connection, because the receiver can't
// tell the connection ID.
TEST_P(EndToEndTest, BadPacketHeaderFlags) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  SendSynchronousFooRequestAndCheckResponse();

  // Packet with invalid public flags.
  uint8_t packet[] = {
      // invalid public flags
      0xFF,
      // connection_id
      0x10,
      0x32,
      0x54,
      0x76,
      0x98,
      0xBA,
      0xDC,
      0xFE,
      // packet sequence number
      0xBC,
      0x9A,
      0x78,
      0x56,
      0x34,
      0x12,
      // private flags
      0x00,
  };
  client_writer_->WritePacket(
      reinterpret_cast<const char*>(packet), sizeof(packet),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr, packet_writer_params_);

  EXPECT_TRUE(server_thread_->WaitUntil(
      [&] {
        return QuicDispatcherPeer::GetAndClearLastError(
                   QuicServerPeer::GetDispatcher(server_thread_->server())) ==
               QUIC_INVALID_PACKET_HEADER;
      },
      QuicTime::Delta::FromSeconds(5)));

  // The connection should not be terminated.
  SendSynchronousFooRequestAndCheckResponse();
}

// Send a packet from the client with bad encrypted data.  The server should not
// tear down the connection.
// Marked as slow since it calls absl::SleepFor().
TEST_P(EndToEndTest, QUICHE_SLOW_TEST(BadEncryptedData)) {
  ASSERT_TRUE(Initialize());

  // Start the connection.
  SendSynchronousFooRequestAndCheckResponse();

  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  std::unique_ptr<QuicEncryptedPacket> packet(ConstructEncryptedPacket(
      client_connection->connection_id(), EmptyQuicConnectionId(), false, false,
      1, "At least 20 characters.", CONNECTION_ID_PRESENT, CONNECTION_ID_ABSENT,
      PACKET_4BYTE_PACKET_NUMBER));
  // Damage the encrypted data.
  std::string damaged_packet(packet->data(), packet->length());
  damaged_packet[30] ^= 0x01;
  QUIC_DLOG(INFO) << "Sending bad packet.";
  client_writer_->WritePacket(
      damaged_packet.data(), damaged_packet.length(),
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr, packet_writer_params_);
  // Give the server time to process the packet.
  absl::SleepFor(absl::Seconds(1));
  // This error is sent to the connection's OnError (which ignores it), so the
  // dispatcher doesn't see it.
  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  if (dispatcher != nullptr) {
    EXPECT_THAT(QuicDispatcherPeer::GetAndClearLastError(dispatcher),
                IsQuicNoError());
  } else {
    ADD_FAILURE() << "Missing dispatcher";
  }
  server_thread_->Resume();

  // The connection should not be terminated.
  SendSynchronousFooRequestAndCheckResponse();
}

TEST_P(EndToEndTest, CanceledStreamDoesNotBecomeZombie) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  // Lose the request.
  SetPacketLossPercentage(100);
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  client_->SendMessage(headers, "test_body", /*fin=*/false);
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();

  // Cancel the stream.
  stream->Reset(QUIC_STREAM_CANCELLED);
  QuicSession* session = GetClientSession();
  ASSERT_TRUE(session);
  // Verify canceled stream does not become zombie.
  EXPECT_EQ(1u, QuicSessionPeer::closed_streams(session).size());
}

// A test stream that gives |response_body_| as an error response body.
class ServerStreamWithErrorResponseBody : public QuicSimpleServerStream {
 public:
  ServerStreamWithErrorResponseBody(
      QuicStreamId id, QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend,
      std::string response_body)
      : QuicSimpleServerStream(id, session, BIDIRECTIONAL,
                               quic_simple_server_backend),
        response_body_(std::move(response_body)) {}

  ~ServerStreamWithErrorResponseBody() override = default;

 protected:
  void SendErrorResponse() override {
    QUIC_DLOG(INFO) << "Sending error response for stream " << id();
    HttpHeaderBlock headers;
    headers[":status"] = "500";
    headers["content-length"] = absl::StrCat(response_body_.size());
    // This method must call CloseReadSide to cause the test case, StopReading
    // is not sufficient.
    QuicStreamPeer::CloseReadSide(this);
    SendHeadersAndBody(std::move(headers), response_body_);
  }

  std::string response_body_;
};

class StreamWithErrorFactory : public QuicTestServer::StreamFactory {
 public:
  explicit StreamWithErrorFactory(std::string response_body)
      : response_body_(std::move(response_body)) {}

  ~StreamWithErrorFactory() override = default;

  QuicSimpleServerStream* CreateStream(
      QuicStreamId id, QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend) override {
    return new ServerStreamWithErrorResponseBody(
        id, session, quic_simple_server_backend, response_body_);
  }

  QuicSimpleServerStream* CreateStream(
      PendingStream* /*pending*/, QuicSpdySession* /*session*/,
      QuicSimpleServerBackend* /*response_cache*/) override {
    return nullptr;
  }

 private:
  std::string response_body_;
};

// A test server stream that drops all received body.
class ServerStreamThatDropsBody : public QuicSimpleServerStream {
 public:
  ServerStreamThatDropsBody(QuicStreamId id, QuicSpdySession* session,
                            QuicSimpleServerBackend* quic_simple_server_backend)
      : QuicSimpleServerStream(id, session, BIDIRECTIONAL,
                               quic_simple_server_backend) {}

  ~ServerStreamThatDropsBody() override = default;

 protected:
  void OnBodyAvailable() override {
    while (HasBytesToRead()) {
      struct iovec iov;
      if (GetReadableRegions(&iov, 1) == 0) {
        // No more data to read.
        break;
      }
      QUIC_DVLOG(1) << "Processed " << iov.iov_len << " bytes for stream "
                    << id();
      MarkConsumed(iov.iov_len);
    }

    if (!sequencer()->IsClosed()) {
      sequencer()->SetUnblocked();
      return;
    }

    // If the sequencer is closed, then all the body, including the fin, has
    // been consumed.
    OnFinRead();

    if (write_side_closed() || fin_buffered()) {
      return;
    }

    SendResponse();
  }
};

class ServerStreamThatDropsBodyFactory : public QuicTestServer::StreamFactory {
 public:
  ServerStreamThatDropsBodyFactory() = default;

  ~ServerStreamThatDropsBodyFactory() override = default;

  QuicSimpleServerStream* CreateStream(
      QuicStreamId id, QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend) override {
    return new ServerStreamThatDropsBody(id, session,
                                         quic_simple_server_backend);
  }

  QuicSimpleServerStream* CreateStream(
      PendingStream* /*pending*/, QuicSpdySession* /*session*/,
      QuicSimpleServerBackend* /*response_cache*/) override {
    return nullptr;
  }
};

// A test server stream that sends response with body size greater than 4GB.
class ServerStreamThatSendsHugeResponse : public QuicSimpleServerStream {
 public:
  ServerStreamThatSendsHugeResponse(
      QuicStreamId id, QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend, int64_t body_bytes)
      : QuicSimpleServerStream(id, session, BIDIRECTIONAL,
                               quic_simple_server_backend),
        body_bytes_(body_bytes) {}

  ~ServerStreamThatSendsHugeResponse() override = default;

 protected:
  void SendResponse() override {
    QuicBackendResponse response;
    std::string body(body_bytes_, 'a');
    response.set_body(body);
    SendHeadersAndBodyAndTrailers(response.headers().Clone(), response.body(),
                                  response.trailers().Clone());
  }

 private:
  // Use a explicit int64_t rather than size_t to simulate a 64-bit server
  // talking to a 32-bit client.
  int64_t body_bytes_;
};

class ServerStreamThatSendsHugeResponseFactory
    : public QuicTestServer::StreamFactory {
 public:
  explicit ServerStreamThatSendsHugeResponseFactory(int64_t body_bytes)
      : body_bytes_(body_bytes) {}

  ~ServerStreamThatSendsHugeResponseFactory() override = default;

  QuicSimpleServerStream* CreateStream(
      QuicStreamId id, QuicSpdySession* session,
      QuicSimpleServerBackend* quic_simple_server_backend) override {
    return new ServerStreamThatSendsHugeResponse(
        id, session, quic_simple_server_backend, body_bytes_);
  }

  QuicSimpleServerStream* CreateStream(
      PendingStream* /*pending*/, QuicSpdySession* /*session*/,
      QuicSimpleServerBackend* /*response_cache*/) override {
    return nullptr;
  }

  int64_t body_bytes_;
};

class BlockedFrameObserver : public QuicConnectionDebugVisitor {
 public:
  std::vector<QuicBlockedFrame> blocked_frames() const {
    return blocked_frames_;
  }

  void OnBlockedFrame(const QuicBlockedFrame& frame) override {
    blocked_frames_.push_back(frame);
  }

 private:
  std::vector<QuicBlockedFrame> blocked_frames_;
};

TEST_P(EndToEndTest, BlockedFrameIncludesOffset) {
  if (!version_.HasIetfQuicFrames()) {
    // For Google QUIC, the BLOCKED frame offset is ignored.
    Initialize();
    return;
  }

  set_smaller_flow_control_receive_window();
  ASSERT_TRUE(Initialize());

  // Observe the connection for BLOCKED frames.
  BlockedFrameObserver observer;
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  client_connection->set_debug_visitor(&observer);

  // Set the response body larger than the flow control window so the server
  // must receive a window update from the client before it can finish sending
  // it (hence, causing the server to send a BLOCKED frame)
  uint32_t response_body_size =
      client_config_.GetInitialSessionFlowControlWindowToSend() + 10;
  std::string response_body(response_body_size, 'a');
  AddToCache("/blocked", 200, response_body);
  SendSynchronousRequestAndCheckResponse("/blocked", response_body);
  client_->Disconnect();

  ASSERT_GE(observer.blocked_frames().size(), static_cast<uint64_t>(0));
  for (const QuicBlockedFrame& frame : observer.blocked_frames()) {
    if (frame.stream_id ==
        QuicUtils::GetInvalidStreamId(version_.transport_version)) {
      // connection-level BLOCKED frame
      ASSERT_EQ(frame.offset,
                client_config_.GetInitialSessionFlowControlWindowToSend());
    } else {
      // stream-level BLOCKED frame
      ASSERT_EQ(frame.offset,
                client_config_.GetInitialStreamFlowControlWindowToSend());
    }
  }

  client_connection->set_debug_visitor(nullptr);
}

TEST_P(EndToEndTest, EarlyResponseFinRecording) {
  set_smaller_flow_control_receive_window();

  // Verify that an incoming FIN is recorded in a stream object even if the read
  // side has been closed.  This prevents an entry from being made in
  // locally_close_streams_highest_offset_ (which will never be deleted).
  // To set up the test condition, the server must do the following in order:
  // start sending the response and call CloseReadSide
  // receive the FIN of the request
  // send the FIN of the response

  // The response body must be larger than the flow control window so the server
  // must receive a window update from the client before it can finish sending
  // it.
  uint32_t response_body_size =
      2 * client_config_.GetInitialStreamFlowControlWindowToSend();
  std::string response_body(response_body_size, 'a');

  StreamWithErrorFactory stream_factory(response_body);
  SetSpdyStreamFactory(&stream_factory);

  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // A POST that gets an early error response, after the headers are received
  // and before the body is received, due to invalid content-length.
  // Set an invalid content-length, so the request will receive an early 500
  // response.
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/garbage";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = "-1";

  // The body must be large enough that the FIN will be in a different packet
  // than the end of the headers, but short enough to not require a flow control
  // update.  This allows headers processing to trigger the error response
  // before the request FIN is processed but receive the request FIN before the
  // response is sent completely.
  const uint32_t kRequestBodySize = kMaxOutgoingPacketSize + 10;
  std::string request_body(kRequestBodySize, 'a');

  // Send the request.
  client_->SendMessage(headers, request_body);
  client_->WaitForResponse();
  CheckResponseHeaders("500");

  // Pause the server so we can access the server's internals without races.
  server_thread_->Pause();

  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  QuicSession* server_session =
      QuicDispatcherPeer::GetFirstSessionIfAny(dispatcher);
  EXPECT_TRUE(server_session != nullptr);

  // The stream is not waiting for the arrival of the peer's final offset.
  EXPECT_EQ(
      0u, QuicSessionPeer::GetLocallyClosedStreamsHighestOffset(server_session)
              .size());

  server_thread_->Resume();
}

TEST_P(EndToEndTest, Trailers) {
  // Test sending and receiving HTTP/2 Trailers (trailing HEADERS frames).
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  // Set reordering to ensure that Trailers arriving before body is ok.
  SetPacketSendDelay(QuicTime::Delta::FromMilliseconds(2));
  SetReorderPercentage(30);

  // Add a response with headers, body, and trailers.
  const std::string kBody = "body content";

  HttpHeaderBlock headers;
  headers[":status"] = "200";
  headers["content-length"] = absl::StrCat(kBody.size());

  HttpHeaderBlock trailers;
  trailers["some-trailing-header"] = "trailing-header-value";

  memory_cache_backend_.AddResponse(server_hostname_, "/trailer_url",
                                    std::move(headers), kBody,
                                    trailers.Clone());

  SendSynchronousRequestAndCheckResponse("/trailer_url", kBody);
  EXPECT_EQ(trailers, client_->response_trailers());
}

// TODO(fayang): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, DISABLED_TestHugePostWithPacketLoss) {
  // This test tests a huge post with introduced packet loss from client to
  // server and body size greater than 4GB, making sure QUIC code does not break
  // for 32-bit builds.
  ServerStreamThatDropsBodyFactory stream_factory;
  SetSpdyStreamFactory(&stream_factory);
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  SetPacketLossPercentage(1);
  // To avoid storing the whole request body in memory, use a loop to repeatedly
  // send body size of kSizeBytes until the whole request body size is reached.
  const int kSizeBytes = 128 * 1024;
  // Request body size is 4G plus one more kSizeBytes.
  int64_t request_body_size_bytes = pow(2, 32) + kSizeBytes;
  ASSERT_LT(INT64_C(4294967296), request_body_size_bytes);
  std::string body(kSizeBytes, 'a');

  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["content-length"] = absl::StrCat(request_body_size_bytes);

  client_->SendMessage(headers, "", /*fin=*/false);

  for (int i = 0; i < request_body_size_bytes / kSizeBytes; ++i) {
    bool fin = (i == request_body_size_bytes - 1);
    client_->SendData(std::string(body.data(), kSizeBytes), fin);
    client_->client()->WaitForEvents();
  }
  VerifyCleanConnection(true);
}

// TODO(fayang): this test seems to cause net_unittests timeouts :|
TEST_P(EndToEndTest, DISABLED_TestHugeResponseWithPacketLoss) {
  // This test tests a huge response with introduced loss from server to client
  // and body size greater than 4GB, making sure QUIC code does not break for
  // 32-bit builds.
  const int kSizeBytes = 128 * 1024;
  int64_t response_body_size_bytes = pow(2, 32) + kSizeBytes;
  ASSERT_LT(4294967296, response_body_size_bytes);
  ServerStreamThatSendsHugeResponseFactory stream_factory(
      response_body_size_bytes);
  SetSpdyStreamFactory(&stream_factory);

  StartServer();

  // Use a quic client that drops received body.
  QuicTestClient* client =
      new QuicTestClient(server_address_, server_hostname_, client_config_,
                         client_supported_versions_);
  client->client()->set_drop_response_body(true);
  client->UseWriter(client_writer_);
  client->Connect();
  client_.reset(client);
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  client_writer_->Initialize(
      QuicConnectionPeer::GetHelper(client_connection),
      QuicConnectionPeer::GetAlarmFactory(client_connection),
      std::make_unique<ClientDelegate>(client_->client()));
  initialized_ = true;
  ASSERT_TRUE(client_->client()->connected());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  SetPacketLossPercentage(1);
  client_->SendRequest("/huge_response");
  client_->WaitForResponse();
  VerifyCleanConnection(true);
}

TEST_P(EndToEndTest, ReleaseHeadersStreamBufferWhenIdle) {
  // Tests that when client side has no active request,
  // its headers stream's sequencer buffer should be released.
  ASSERT_TRUE(Initialize());
  client_->SendSynchronousRequest("/foo");
  if (version_.UsesHttp3()) {
    return;
  }
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicHeadersStream* headers_stream =
      QuicSpdySessionPeer::GetHeadersStream(client_session);
  ASSERT_TRUE(headers_stream);
  QuicStreamSequencer* sequencer = QuicStreamPeer::sequencer(headers_stream);
  ASSERT_TRUE(sequencer);
  EXPECT_FALSE(QuicStreamSequencerPeer::IsUnderlyingBufferAllocated(sequencer));
}

// A single large header value causes a different error than the total size of
// headers exceeding a smaller limit, tested at EndToEndTest.LargeHeaders.
TEST_P(EndToEndTest, WayTooLongRequestHeaders) {
  ASSERT_TRUE(Initialize());

  HttpHeaderBlock headers;
  headers[":method"] = "GET";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers["key"] = std::string(2 * 1024 * 1024, 'a');

  client_->SendMessage(headers, "");
  client_->WaitForResponse();
  if (version_.UsesHttp3()) {
    EXPECT_THAT(client_->connection_error(),
                IsError(QUIC_QPACK_DECOMPRESSION_FAILED));
  } else {
    EXPECT_THAT(client_->connection_error(),
                IsError(QUIC_HPACK_VALUE_TOO_LONG));
  }
}

class WindowUpdateObserver : public QuicConnectionDebugVisitor {
 public:
  WindowUpdateObserver() : num_window_update_frames_(0), num_ping_frames_(0) {}

  size_t num_window_update_frames() const { return num_window_update_frames_; }

  size_t num_ping_frames() const { return num_ping_frames_; }

  void OnWindowUpdateFrame(const QuicWindowUpdateFrame& /*frame*/,
                           const QuicTime& /*receive_time*/) override {
    ++num_window_update_frames_;
  }

  void OnPingFrame(const QuicPingFrame& /*frame*/,
                   const QuicTime::Delta /*ping_received_delay*/) override {
    ++num_ping_frames_;
  }

 private:
  size_t num_window_update_frames_;
  size_t num_ping_frames_;
};

TEST_P(EndToEndTest, WindowUpdateInAck) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  WindowUpdateObserver observer;
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  client_connection->set_debug_visitor(&observer);
  // 100KB body.
  std::string body(100 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  EXPECT_EQ(kFooResponseBody,
            client_->SendCustomSynchronousRequest(headers, body));
  client_->Disconnect();
  EXPECT_LT(0u, observer.num_window_update_frames());
  EXPECT_EQ(0u, observer.num_ping_frames());
  client_connection->set_debug_visitor(nullptr);
}

TEST_P(EndToEndTest, SendStatelessResetTokenInShlo) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicConfig* config = client_session->config();
  ASSERT_TRUE(config);
  EXPECT_TRUE(config->HasReceivedStatelessResetToken());
  QuicConnection* client_connection = client_session->connection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(QuicUtils::GenerateStatelessResetToken(
                client_connection->connection_id()),
            config->ReceivedStatelessResetToken());
  client_->Disconnect();
}

// Regression test for b/116200989.
TEST_P(EndToEndTest,
       SendStatelessResetIfServerConnectionClosedLocallyDuringHandshake) {
  SetQuicFlag(quic_allow_chlo_buffering, true);
  SetQuicFlag(quic_dispatcher_max_ack_sent_per_connection, 1);
  // Make the client hello to span 2 packets.
  client_extra_copts_.push_back(kCHP1);
  connect_to_server_on_initialize_ = false;
  ASSERT_TRUE(Initialize());

  ASSERT_TRUE(server_thread_);
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  if (dispatcher == nullptr) {
    ADD_FAILURE() << "Missing dispatcher";
    server_thread_->Resume();
    return;
  }
  if (dispatcher->NumSessions() > 0) {
    ADD_FAILURE() << "Dispatcher session map not empty";
    server_thread_->Resume();
    return;
  }
  // Note: this writer will only used by the server connection, not the time
  // wait list. We start failing the write after the first packet, which is the
  // ACK of the first CHLO packet sent by the dispatcher.
  QuicDispatcherPeer::UseWriter(
      dispatcher,
      // This cause the all server-sent packets to fail except the first one.
      new BadPacketWriter(/*packet_causing_write_error=*/1, EPERM));
  server_thread_->Resume();

  client_.reset(CreateQuicClient(client_writer_));
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  EXPECT_THAT(client_->connection_error(),
              IsError(QUIC_HANDSHAKE_FAILED_SYNTHETIC_CONNECTION_CLOSE));
}

// Regression test for b/116200989.
TEST_P(EndToEndTest,
       SendStatelessResetIfServerConnectionClosedLocallyAfterHandshake) {
  // Prevent the connection from expiring in the time wait list.
  SetQuicFlag(quic_time_wait_list_seconds, 10000);
  connect_to_server_on_initialize_ = false;
  ASSERT_TRUE(Initialize());

  // big_response_body is 64K, which is about 48 full-sized packets.
  const size_t kBigResponseBodySize = 65536;
  QuicData big_response_body(new char[kBigResponseBodySize](),
                             kBigResponseBodySize, /*owns_buffer=*/true);
  AddToCache("/big_response", 200, big_response_body.AsStringPiece());

  ASSERT_TRUE(server_thread_);
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  if (dispatcher == nullptr) {
    ADD_FAILURE() << "Missing dispatcher";
    server_thread_->Resume();
    return;
  }
  if (dispatcher->NumSessions() > 0) {
    ADD_FAILURE() << "Dispatcher session map not empty";
    server_thread_->Resume();
    return;
  }
  QuicDispatcherPeer::UseWriter(
      dispatcher,
      // This will cause an server write error with EPERM, while sending the
      // response for /big_response.
      new BadPacketWriter(/*packet_causing_write_error=*/20, EPERM));
  server_thread_->Resume();

  client_.reset(CreateQuicClient(client_writer_));

  // First, a /foo request with small response should succeed.
  SendSynchronousFooRequestAndCheckResponse();

  // Second, a /big_response request with big response should fail.
  EXPECT_LT(client_->SendSynchronousRequest("/big_response").length(),
            kBigResponseBodySize);
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PUBLIC_RESET));
}

// Regression test of b/70782529.
TEST_P(EndToEndTest, DoNotCrashOnPacketWriteError) {
  ASSERT_TRUE(Initialize());
  BadPacketWriter* bad_writer =
      new BadPacketWriter(/*packet_causing_write_error=*/5,
                          /*error_code=*/90);
  std::unique_ptr<QuicTestClient> client(CreateQuicClient(bad_writer));

  // 1 MB body.
  std::string body(1024 * 1024, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  client->SendCustomSynchronousRequest(headers, body);
}

// Regression test for b/71711996. This test sends a connectivity probing packet
// as its last sent packet, and makes sure the server's ACK of that packet does
// not cause the client to fail.
TEST_P(EndToEndTest, LastPacketSentIsConnectivityProbing) {
  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();

  // Wait for the client's ACK (of the response) to be received by the server.
  client_->WaitForDelayedAcks();

  // We are sending a connectivity probing packet from an unchanged client
  // address, so the server will not respond to us with a connectivity probing
  // packet, however the server should send an ack-only packet to us.
  client_->SendConnectivityProbing();

  // Wait for the server's last ACK to be received by the client.
  client_->WaitForDelayedAcks();
}

TEST_P(EndToEndTest, PreSharedKey) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(5));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(5));
  pre_shared_key_client_ = "foobar";
  pre_shared_key_server_ = "foobar";

  if (version_.UsesTls()) {
    // TODO(b/154162689) add PSK support to QUIC+TLS.
    InitializeAndCheckForTlsPskFailure();
    return;
  }

  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
}

// TODO: reenable once we have a way to make this run faster.
TEST_P(EndToEndTest, QUIC_TEST_DISABLED_IN_CHROME(PreSharedKeyMismatch)) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_client_ = "foo";
  pre_shared_key_server_ = "bar";

  if (version_.UsesTls()) {
    // TODO(b/154162689) add PSK support to QUIC+TLS.
    InitializeAndCheckForTlsPskFailure();
    return;
  }

  // One of two things happens when Initialize() returns:
  // 1. Crypto handshake has completed, and it is unsuccessful. Initialize()
  //    returns false.
  // 2. Crypto handshake has not completed, Initialize() returns true. The call
  //    to WaitForCryptoHandshakeConfirmed() will wait for the handshake and
  //    return whether it is successful.
  ASSERT_FALSE(Initialize() && client_->client()->WaitForOneRttKeysAvailable());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_HANDSHAKE_TIMEOUT));
}

// TODO: reenable once we have a way to make this run faster.
TEST_P(EndToEndTest, QUIC_TEST_DISABLED_IN_CHROME(PreSharedKeyNoClient)) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_server_ = "foobar";

  if (version_.UsesTls()) {
    // TODO(b/154162689) add PSK support to QUIC+TLS.
    InitializeAndCheckForTlsPskFailure(/*expect_client_failure=*/false);
    return;
  }

  ASSERT_FALSE(Initialize() && client_->client()->WaitForOneRttKeysAvailable());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_HANDSHAKE_TIMEOUT));
}

// TODO: reenable once we have a way to make this run faster.
TEST_P(EndToEndTest, QUIC_TEST_DISABLED_IN_CHROME(PreSharedKeyNoServer)) {
  client_config_.set_max_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  client_config_.set_max_idle_time_before_crypto_handshake(
      QuicTime::Delta::FromSeconds(1));
  pre_shared_key_client_ = "foobar";

  if (version_.UsesTls()) {
    // TODO(b/154162689) add PSK support to QUIC+TLS.
    InitializeAndCheckForTlsPskFailure();
    return;
  }

  ASSERT_FALSE(Initialize() && client_->client()->WaitForOneRttKeysAvailable());
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_HANDSHAKE_TIMEOUT));
}

TEST_P(EndToEndTest, RequestAndStreamRstInOnePacket) {
  // Regression test for b/80234898.
  ASSERT_TRUE(Initialize());

  // INCOMPLETE_RESPONSE will cause the server to not to send the trailer
  // (and the FIN) after the response body.
  std::string response_body(1305, 'a');
  HttpHeaderBlock response_headers;
  response_headers[":status"] = absl::StrCat(200);
  response_headers["content-length"] = absl::StrCat(response_body.length());
  memory_cache_backend_.AddSpecialResponse(
      server_hostname_, "/test_url", std::move(response_headers), response_body,
      QuicBackendResponse::INCOMPLETE_RESPONSE);

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  client_->WaitForDelayedAcks();

  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  const QuicPacketCount packets_sent_before =
      client_connection->GetStats().packets_sent;

  client_->SendRequestAndRstTogether("/test_url");

  // Expect exactly one packet is sent from the block above.
  ASSERT_EQ(packets_sent_before + 1,
            client_connection->GetStats().packets_sent);

  // Wait for the connection to become idle.
  client_->WaitForDelayedAcks();

  // The real expectation is the test does not crash or timeout.
  EXPECT_THAT(client_->connection_error(), IsQuicNoError());
}

TEST_P(EndToEndTest, ResetStreamOnTtlExpires) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  SetPacketLossPercentage(30);

  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  // Set a TTL which expires immediately.
  stream->MaybeSetTtl(QuicTime::Delta::FromMicroseconds(1));

  WriteHeadersOnStream(stream);
  // 1 MB body.
  std::string body(1024 * 1024, 'a');
  stream->WriteOrBufferBody(body, true);
  client_->WaitForResponse();
  EXPECT_THAT(client_->stream_error(), IsStreamError(QUIC_STREAM_TTL_EXPIRED));
}

TEST_P(EndToEndTest, SendDatagrams) {
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  QuicSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicConnection* client_connection = client_session->connection();
  ASSERT_TRUE(client_connection);

  SetPacketLossPercentage(30);
  ASSERT_GT(kMaxOutgoingPacketSize,
            client_session->GetCurrentLargestDatagramPayload());
  ASSERT_LT(0, client_session->GetCurrentLargestDatagramPayload());

  std::string datagram_string(kMaxOutgoingPacketSize, 'a');
  QuicRandom* random =
      QuicConnectionPeer::GetHelper(client_connection)->GetRandomGenerator();
  {
    QuicConnection::ScopedPacketFlusher flusher(client_session->connection());
    // Verify the largest datagram gets successfully sent.
    EXPECT_EQ(DatagramResult(DATAGRAM_STATUS_SUCCESS, 1),
              client_session->SendDatagram(MemSliceFromString(absl::string_view(
                  datagram_string.data(),
                  client_session->GetCurrentLargestDatagramPayload()))));
    // Send more datagrams with size (0, largest_payload] until connection is
    // write blocked.
    const int kTestMaxNumberOfDatagrams = 100;
    for (size_t i = 2; i <= kTestMaxNumberOfDatagrams; ++i) {
      size_t datagram_length =
          random->RandUint64() %
              client_session->GetGuaranteedLargestDatagramPayload() +
          1;
      DatagramResult result = client_session->SendDatagram(MemSliceFromString(
          absl::string_view(datagram_string.data(), datagram_length)));
      if (result.status == DATAGRAM_STATUS_BLOCKED) {
        // Connection is write blocked.
        break;
      }
      EXPECT_EQ(DatagramResult(DATAGRAM_STATUS_SUCCESS, i), result);
    }
  }

  client_->WaitForDelayedAcks();
  EXPECT_EQ(DATAGRAM_STATUS_TOO_LARGE,
            client_session
                ->SendDatagram(MemSliceFromString(absl::string_view(
                    datagram_string.data(),
                    client_session->GetCurrentLargestDatagramPayload() + 1)))
                .status);
  EXPECT_THAT(client_->connection_error(), IsQuicNoError());
}

class EndToEndPacketReorderingTest : public EndToEndTest {
 public:
  void CreateClientWithWriter() override {
    QUIC_LOG(ERROR) << "create client with reorder_writer_";
    reorder_writer_ = new PacketReorderingWriter();
    client_.reset(EndToEndTest::CreateQuicClient(reorder_writer_));
  }

  void SetUp() override {
    // Don't initialize client writer in base class.
    server_writer_ = new PacketDroppingTestWriter();
  }

 protected:
  PacketReorderingWriter* reorder_writer_;
};

INSTANTIATE_TEST_SUITE_P(EndToEndPacketReorderingTests,
                         EndToEndPacketReorderingTest,
                         ::testing::ValuesIn(GetTestParams()),
                         ::testing::PrintToStringParamName());

TEST_P(EndToEndPacketReorderingTest, ReorderedConnectivityProbing) {
  ASSERT_TRUE(Initialize());
  if (version_.HasIetfQuicFrames() ||
      GetQuicReloadableFlag(quic_ignore_gquic_probing)) {
    return;
  }

  // Finish one request to make sure handshake established.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Wait for the connection to become idle, to make sure the packet gets
  // delayed is the connectivity probing packet.
  client_->WaitForDelayedAcks();

  QuicSocketAddress old_addr =
      client_->client()->network_helper()->GetLatestClientAddress();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_addr.host(), new_host);
  ASSERT_TRUE(client_->client()->MigrateSocket(new_host));

  // Write a connectivity probing after the next /foo request.
  reorder_writer_->SetDelay(1);
  client_->SendConnectivityProbing();

  ASSERT_TRUE(client_->MigrateSocketWithSpecifiedPort(old_addr.host(),
                                                      old_addr.port()));

  // The (delayed) connectivity probing will be sent after this request.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Send yet another request after the connectivity probing, when this request
  // returns, the probing is guaranteed to have been received by the server, and
  // the server's response to probing is guaranteed to have been received by the
  // client.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_EQ(1u,
              server_connection->GetStats().num_connectivity_probing_received);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();

  // Server definitely responded to the connectivity probing. Sometime it also
  // sends a padded ping that is not a connectivity probing, which is recognized
  // as connectivity probing because client's self address is ANY.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_LE(1u,
            client_connection->GetStats().num_connectivity_probing_received);
}

// A writer which holds the next packet to be sent till ReleasePacket() is
// called.
class PacketHoldingWriter : public QuicPacketWriterWrapper {
 public:
  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options,
                          const QuicPacketWriterParams& params) override {
    if (!hold_next_packet_) {
      return QuicPacketWriterWrapper::WritePacket(
          buffer, buf_len, self_address, peer_address, options, params);
    }
    QUIC_DLOG(INFO) << "Packet is held by the writer";
    packet_content_ = std::string(buffer, buf_len);
    self_address_ = self_address;
    peer_address_ = peer_address;
    options_ = (options == nullptr ? nullptr : options->Clone());
    hold_next_packet_ = false;
    return WriteResult(WRITE_STATUS_OK, buf_len);
  }

  void HoldNextPacket() {
    QUICHE_DCHECK(packet_content_.empty())
        << "There is already one packet on hold.";
    hold_next_packet_ = true;
  }

  void ReleasePacket() {
    QUIC_DLOG(INFO) << "Release packet";
    ASSERT_EQ(WRITE_STATUS_OK,
              QuicPacketWriterWrapper::WritePacket(
                  packet_content_.data(), packet_content_.length(),
                  self_address_, peer_address_, options_.release(), params_)
                  .status);
    packet_content_.clear();
  }

 private:
  bool hold_next_packet_{false};
  std::string packet_content_;
  QuicIpAddress self_address_;
  QuicSocketAddress peer_address_;
  std::unique_ptr<PerPacketOptions> options_;
  QuicPacketWriterParams params_;
};

TEST_P(EndToEndTest, ClientValidateNewNetwork) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  SendSynchronousFooRequestAndCheckResponse();

  // Store the client IP address which was used to send the first request.
  QuicIpAddress old_host =
      client_->client()->network_helper()->GetLatestClientAddress().host();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_host, new_host);

  client_->client()->ValidateNewNetwork(new_host);
  // Send a request using the old socket.
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  // Client should have received a PATH_CHALLENGE.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);

  // Send another request to make sure THE server will receive PATH_RESPONSE.
  client_->SendSynchronousRequest("/eep");

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_EQ(1u,
              server_connection->GetStats().num_connectivity_probing_received);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ClientMultiPortConnection) {
  client_config_.SetClientConnectionOptions(QuicTagVector{kMPQC, kMPQM});
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream);
  // Increase the probing frequency to speed up this test.
  client_connection->SetMultiPortProbingInterval(
      QuicTime::Delta::FromMilliseconds(100));
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 1u == client_connection->GetStats().num_path_response_received;
  }));
  // Verify that the alternative path keeps sending probes periodically.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 2u == client_connection->GetStats().num_path_response_received;
  }));
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  // Verify that no migration has happened.
  if (server_connection != nullptr) {
    EXPECT_EQ(0u, server_connection->GetStats()
                      .num_peer_migration_to_proactively_validated_address);
  }
  server_thread_->Resume();

  // This will cause the next periodic probing to fail.
  server_writer_->set_fake_packet_loss_percentage(100);
  EXPECT_TRUE(client_->WaitUntil(
      1000, [&]() { return client_->client()->HasPendingPathValidation(); }));
  // Now wait for path validation to timeout.
  EXPECT_TRUE(client_->WaitUntil(
      2000, [&]() { return !client_->client()->HasPendingPathValidation(); }));
  server_writer_->set_fake_packet_loss_percentage(0);
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 3u == client_connection->GetStats().num_path_response_received;
  }));
  // Verify that the previous path was retired.
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  stream->Reset(QuicRstStreamErrorCode::QUIC_STREAM_NO_ERROR);
}

TEST_P(EndToEndTest, ClientMultiPortProbeOnRto) {
  client_config_.SetClientConnectionOptions(QuicTagVector{kMPQC, kMPR1});
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  ASSERT_TRUE(client_->client()->WaitForHandshakeConfirmed());

  QuicConnection* client_connection = GetClientConnection();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream);

  // Increase the probing frequency to speed up this test.
  client_connection->SetMultiPortProbingInterval(
      QuicTime::Delta::FromMilliseconds(100));

  SendSynchronousFooRequestAndCheckResponse();

  // Verify that no multiport connection is established before RTO.
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty() ||
              client_connection->GetStats().pto_count > 0);

  // If no multiport connection is established, simulate a RTO and verify that
  // the probing on RTO is triggered.
  if (client_connection->multi_port_stats()->num_multi_port_paths_created ==
      0) {
    server_writer_->set_fake_packet_loss_percentage(100);
    EXPECT_TRUE(client_->WaitUntil(
        1000, [&]() { return client_->client()->HasPendingPathValidation(); }));
    server_writer_->set_fake_packet_loss_percentage(0);
    // Now wait for path validation to complete.
    EXPECT_TRUE(client_->WaitUntil(2000, [&]() {
      return !client_->client()->HasPendingPathValidation();
    }));
  }

  // Verify that a multiport connection is established.
  EXPECT_EQ(client_connection->multi_port_stats()->num_multi_port_paths_created,
            1);

  // Verify that the probing is triggered after multiport connection is
  // established.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 1u == client_connection->GetStats().num_path_response_received;
  }));

  // Verify that the alternative path keeps sending probes periodically.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 2u == client_connection->GetStats().num_path_response_received;
  }));

  // This will cause the next periodic probing to fail.
  server_writer_->set_fake_packet_loss_percentage(100);
  EXPECT_TRUE(client_->WaitUntil(
      1000, [&]() { return client_->client()->HasPendingPathValidation(); }));
  // Now wait for path validation to timeout.
  EXPECT_TRUE(client_->WaitUntil(
      2000, [&]() { return !client_->client()->HasPendingPathValidation(); }));
  server_writer_->set_fake_packet_loss_percentage(0);
  // Verify no new path response received on alternate path
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 2u == client_connection->GetStats().num_path_response_received;
  }));

  // Verify that the previous path is retired after path validation times out.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 1u == client_connection->GetStats().num_retire_connection_id_sent;
  }));

  // Wait for new connection id to be received before new multiport connection
  // is established.
  WaitForNewConnectionIds();

  // Send another request to make sure the server will have a chance to
  // establish new multiport connection on RTO.
  SendSynchronousFooRequestAndCheckResponse();

  // Simulate another RTO and verify that the probing on RTO is triggered again.
  server_writer_->set_fake_packet_loss_percentage(100);

  // Verify that a new multiport connection is established on RTO.
  EXPECT_TRUE(client_->WaitUntil(2000, [&]() {
    return client_connection->multi_port_stats()
               ->num_multi_port_paths_created == 2;
  }));
  EXPECT_TRUE(client_->WaitUntil(
      2000, [&]() { return client_->client()->HasPendingPathValidation(); }));
  server_writer_->set_fake_packet_loss_percentage(0);
  // Now wait for path validation to complete.
  EXPECT_TRUE(client_->WaitUntil(
      1000, [&]() { return !client_->client()->HasPendingPathValidation(); }));

  // Verify new path is validated after establishing a new multiport connection.
  // Sometimes the path validation is trigerred more than 3 times.
  EXPECT_TRUE(client_->WaitUntil(2000, [&]() {
    return 3u <= client_connection->GetStats().num_path_response_received;
  }));

  stream->Reset(QuicRstStreamErrorCode::QUIC_STREAM_NO_ERROR);
}

TEST_P(EndToEndTest, ClientPortMigrationOnPathDegrading) {
  connect_to_server_on_initialize_ = false;
  Initialize();
  if (!version_.HasIetfQuicFrames()) {
    CreateClientWithWriter();
    return;
  }

  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  if (dispatcher == nullptr) {
    ADD_FAILURE() << "Missing dispatcher";
    server_thread_->Resume();
    return;
  }
  if (dispatcher->NumSessions() > 0) {
    ADD_FAILURE() << "Dispatcher session map not empty";
    server_thread_->Resume();
    return;
  }
  auto* new_writer = new DroppingPacketsWithSpecificDestinationWriter();
  // Note: this writer will only used by the server connection, not the time
  // wait list.
  QuicDispatcherPeer::UseWriter(dispatcher, new_writer);
  server_thread_->Resume();

  delete client_writer_;
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  client_->client()->EnablePortMigrationUponPathDegrading(std::nullopt);
  ASSERT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  QuicConnection* client_connection = GetClientConnection();
  QuicSocketAddress original_self_addr = client_connection->self_address();
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/bar";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  client_->SendMessage(headers, "aaaa", false);

  // This causes the all server sent packets to the client's current address to
  // be dropped.
  new_writer->set_peer_address_to_drop(original_self_addr);
  client_->SendData("bbbb", true);
  // The response will be dropped till client migrates to a different port.
  client_->WaitForResponse();
  QuicSocketAddress new_self_addr1 = client_connection->self_address();
  EXPECT_NE(original_self_addr, new_self_addr1);
  EXPECT_EQ(1u, GetClientConnection()->GetStats().num_path_degrading);
  EXPECT_EQ(1u, GetClientConnection()
                    ->GetStats()
                    .num_forward_progress_after_path_degrading);
  EXPECT_EQ(1u, GetClientConnection()->GetStats().num_path_response_received);
  size_t pto_count = GetClientConnection()->GetStats().pto_count;

  // Wait for new connection id to be received.
  WaitForNewConnectionIds();
  // Use 1 PTO to detect path degrading more aggressively.
  client_->client()->EnablePortMigrationUponPathDegrading({1});
  new_writer->set_peer_address_to_drop(new_self_addr1);
  client_->SendSynchronousRequest("/eep");
  QuicSocketAddress new_self_addr2 = client_connection->self_address();
  EXPECT_NE(new_self_addr1, new_self_addr2);
  EXPECT_EQ(2u, GetClientConnection()->GetStats().num_path_degrading);
  EXPECT_EQ(2u, GetClientConnection()
                    ->GetStats()
                    .num_forward_progress_after_path_degrading);
  EXPECT_EQ(2u, GetClientConnection()->GetStats().num_path_response_received);
  // It should take fewer PTOs to trigger port migration than the default(4).
  EXPECT_GT(pto_count + 4, GetClientConnection()->GetStats().pto_count);
}

TEST_P(EndToEndTest, ClientLimitPortMigrationOnPathDegrading) {
  connect_to_server_on_initialize_ = false;
  Initialize();
  if (!version_.HasIetfQuicFrames()) {
    CreateClientWithWriter();
    return;
  }
  const uint32_t max_num_path_degrading_to_mitigate =
      GetQuicFlag(quic_max_num_path_degrading_to_mitigate);

  delete client_writer_;
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  client_->client()->EnablePortMigrationUponPathDegrading(std::nullopt);
  ASSERT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  QuicConnection* client_connection = GetClientConnection();
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/bar";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  // Manually trigger path degrading 5 times and expect they should all trigger
  // port migration.
  for (uint32_t i = 0; i < max_num_path_degrading_to_mitigate; ++i) {
    client_->SendMessage(headers, "aaaa", false);
    QuicSocketAddress original_self_addr = client_connection->self_address();
    WaitForNewConnectionIds();
    client_connection->OnPathDegradingDetected();
    client_->SendData("bbbb", true);
    client_->WaitForResponse();
    while (client_->client()->HasPendingPathValidation()) {
      client_->client()->WaitForEvents();
    }
    QuicSocketAddress new_self_addr = client_connection->self_address();
    EXPECT_NE(original_self_addr, new_self_addr);
  }

  EXPECT_EQ(max_num_path_degrading_to_mitigate,
            GetClientConnection()->GetStats().num_path_degrading);
  EXPECT_EQ(max_num_path_degrading_to_mitigate,
            GetClientConnection()->GetStats().num_path_response_received);

  // The next path degrading shouldn't trigger port migration.
  WaitForNewConnectionIds();
  QuicSocketAddress original_self_addr = client_connection->self_address();
  client_connection->OnPathDegradingDetected();
  EXPECT_FALSE(client_->client()->HasPendingPathValidation());
  client_->SendSynchronousRequest("/eep");
  EXPECT_EQ(original_self_addr, client_connection->self_address());
  EXPECT_EQ(max_num_path_degrading_to_mitigate + 1,
            GetClientConnection()->GetStats().num_path_degrading);
  EXPECT_EQ(max_num_path_degrading_to_mitigate,
            GetClientConnection()->GetStats().num_path_response_received);
}

TEST_P(EndToEndTest, ClientMultiPortMigrationOnPathDegrading) {
  client_config_.SetClientConnectionOptions(QuicTagVector{kMPQC, kMPQM});
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream);
  // Increase the probing frequency to speed up this test.
  client_connection->SetMultiPortProbingInterval(
      QuicTime::Delta::FromMilliseconds(100));
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 1u == client_connection->GetStats().num_path_response_received;
  }));
  // Verify that the alternative path keeps sending probes periodically.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 2u == client_connection->GetStats().num_path_response_received;
  }));
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  // Verify that no migration has happened.
  if (server_connection != nullptr) {
    EXPECT_EQ(0u, server_connection->GetStats()
                      .num_peer_migration_to_proactively_validated_address);
  }
  server_thread_->Resume();

  auto original_self_addr = client_connection->self_address();
  // Trigger client side path degrading
  client_connection->OnPathDegradingDetected();
  EXPECT_NE(original_self_addr, client_connection->self_address());

  // Send another request to trigger connection id retirement.
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  auto new_alt_path = QuicConnectionPeer::GetAlternativePath(client_connection);
  EXPECT_NE(client_connection->self_address(), new_alt_path->self_address);

  stream->Reset(QuicRstStreamErrorCode::QUIC_STREAM_NO_ERROR);
}

TEST_P(EndToEndTest, ClientMultiPortMigrationOnPathDegradingOnRTO) {
  client_config_.SetClientConnectionOptions(QuicTagVector{kMPQC, kMPR1, kMPQM});
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  ASSERT_TRUE(client_->client()->WaitForHandshakeConfirmed());

  QuicConnection* client_connection = GetClientConnection();
  QuicSpdyClientStream* stream = client_->GetOrCreateStream();
  ASSERT_TRUE(stream);

  // Increase the probing frequency to speed up this test.
  client_connection->SetMultiPortProbingInterval(
      QuicTime::Delta::FromMilliseconds(100));

  SendSynchronousFooRequestAndCheckResponse();

  // If no multiport connection is established, induce the client to validate an
  // alternative path.
  server_writer_->set_fake_packet_loss_percentage(100);
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return client_connection->multi_port_stats()
               ->num_multi_port_paths_created == 1;
  }));
  server_writer_->set_fake_packet_loss_percentage(0);

  // Verify that the probing is triggered after multiport connection is
  // established.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return 1u == client_connection->GetStats().num_path_response_received;
  }));

  auto original_self_addr = client_connection->self_address();
  // Trigger client side path degrading
  client_connection->OnPathDegradingDetected();
  // Verify that the client address has changed due to migration.
  EXPECT_NE(original_self_addr, client_connection->self_address());

  // Send another request to trigger connection id retirement.
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);

  // Verify new alternate path is created.
  WaitForNewConnectionIds();
  // Send another request to make sure the server will have a chance to
  // establish new multiport connection on RTO.
  SendSynchronousFooRequestAndCheckResponse();
  // Simulate another RTO and verify that the probing on RTO is triggered again.
  server_writer_->set_fake_packet_loss_percentage(100);
  // Verify that a new multiport connection is established on RTO.
  EXPECT_TRUE(client_->WaitUntil(2000, [&]() {
    return client_connection->multi_port_stats()
               ->num_multi_port_paths_created == 2;
  }));
  server_writer_->set_fake_packet_loss_percentage(0);
  auto new_alt_path = QuicConnectionPeer::GetAlternativePath(client_connection);
  EXPECT_NE(client_connection->self_address(), new_alt_path->self_address);

  stream->Reset(QuicRstStreamErrorCode::QUIC_STREAM_NO_ERROR);
}

TEST_P(EndToEndTest, SimpleServerPreferredAddressTest) {
  use_preferred_address_ = true;
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  EXPECT_EQ(server_address_, client_connection->effective_peer_address());
  EXPECT_EQ(server_address_, client_connection->peer_address());
  EXPECT_TRUE(client_->client()->HasPendingPathValidation());
  QuicConnectionId server_cid1 = client_connection->connection_id();

  SendSynchronousFooRequestAndCheckResponse();
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(server_preferred_address_,
            client_connection->effective_peer_address());
  EXPECT_EQ(server_preferred_address_, client_connection->peer_address());
  EXPECT_NE(server_cid1, client_connection->connection_id());

  const auto client_stats = GetClientConnection()->GetStats();
  EXPECT_TRUE(client_stats.server_preferred_address_validated);
  EXPECT_FALSE(client_stats.failed_to_validate_server_preferred_address);
}

TEST_P(EndToEndTest, SimpleServerPreferredAddressTestNoSPAD) {
  SetQuicFlag(quic_always_support_server_preferred_address, true);
  use_preferred_address_ = true;
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  EXPECT_EQ(server_address_, client_connection->effective_peer_address());
  EXPECT_EQ(server_address_, client_connection->peer_address());
  EXPECT_TRUE(client_->client()->HasPendingPathValidation());
  QuicConnectionId server_cid1 = client_connection->connection_id();

  SendSynchronousFooRequestAndCheckResponse();
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(server_preferred_address_,
            client_connection->effective_peer_address());
  EXPECT_EQ(server_preferred_address_, client_connection->peer_address());
  EXPECT_NE(server_cid1, client_connection->connection_id());

  const auto client_stats = GetClientConnection()->GetStats();
  EXPECT_TRUE(client_stats.server_preferred_address_validated);
  EXPECT_FALSE(client_stats.failed_to_validate_server_preferred_address);
}

TEST_P(EndToEndTest, OptimizedServerPreferredAddress) {
  use_preferred_address_ = true;
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_config_.SetClientConnectionOptions(QuicTagVector{kSPA2});
  client_.reset(CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());
  EXPECT_EQ(server_address_, client_connection->effective_peer_address());
  EXPECT_EQ(server_address_, client_connection->peer_address());
  EXPECT_TRUE(client_->client()->HasPendingPathValidation());
  SendSynchronousFooRequestAndCheckResponse();
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }

  const auto client_stats = GetClientConnection()->GetStats();
  EXPECT_TRUE(client_stats.server_preferred_address_validated);
  EXPECT_FALSE(client_stats.failed_to_validate_server_preferred_address);
}

TEST_P(EndToEndPacketReorderingTest, ReorderedPathChallenge) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));

  // Finish one request to make sure handshake established.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Wait for the connection to become idle, to make sure the packet gets
  // delayed is the connectivity probing packet.
  client_->WaitForDelayedAcks();

  QuicSocketAddress old_addr =
      client_->client()->network_helper()->GetLatestClientAddress();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_addr.host(), new_host);

  // Setup writer wrapper to hold the probing packet.
  auto holding_writer = new PacketHoldingWriter();
  client_->UseWriter(holding_writer);
  // Write a connectivity probing after the next /foo request.
  holding_writer->HoldNextPacket();

  // A packet with PATH_CHALLENGE will be held in the writer.
  client_->client()->ValidateNewNetwork(new_host);

  // Send (on-hold) PATH_CHALLENGE after this request.
  client_->SendRequest("/foo");
  holding_writer->ReleasePacket();

  client_->WaitForResponse();

  EXPECT_EQ(kFooResponseBody, client_->response_body());
  // Send yet another request after the PATH_CHALLENGE, when this request
  // returns, the probing is guaranteed to have been received by the server, and
  // the server's response to probing is guaranteed to have been received by the
  // client.
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));

  // Client should have received a PATH_CHALLENGE.
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(1u,
            client_connection->GetStats().num_connectivity_probing_received);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_EQ(1u,
              server_connection->GetStats().num_connectivity_probing_received);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndPacketReorderingTest, PathValidationFailure) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  client_.reset(CreateQuicClient(nullptr));
  // Finish one request to make sure handshake established.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Wait for the connection to become idle, to make sure the packet gets
  // delayed is the connectivity probing packet.
  client_->WaitForDelayedAcks();

  QuicSocketAddress old_addr = client_->client()->session()->self_address();

  // Migrate socket to the new IP address.
  QuicIpAddress new_host = TestLoopback(2);
  EXPECT_NE(old_addr.host(), new_host);

  // Drop PATH_RESPONSE packets to timeout the path validation.
  server_writer_->set_fake_packet_loss_percentage(100);
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(new_host));
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(old_addr, client_->client()->session()->self_address());
  server_writer_->set_fake_packet_loss_percentage(0);
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_EQ(3u,
              server_connection->GetStats().num_connectivity_probing_received);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndPacketReorderingTest, MigrateAgainAfterPathValidationFailure) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  client_.reset(CreateQuicClient(nullptr));
  // Finish one request to make sure handshake established.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Wait for the connection to become idle, to make sure the packet gets
  // delayed is the connectivity probing packet.
  client_->WaitForDelayedAcks();

  QuicSocketAddress addr1 = client_->client()->session()->self_address();
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionId server_cid1 = client_connection->connection_id();

  // Migrate socket to the new IP address.
  QuicIpAddress host2 = TestLoopback(2);
  EXPECT_NE(addr1.host(), host2);

  // Drop PATH_RESPONSE packets to timeout the path validation.
  server_writer_->set_fake_packet_loss_percentage(100);
  ASSERT_TRUE(
      QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(client_connection));

  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host2));

  QuicConnectionId server_cid2 =
      QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_FALSE(server_cid2.IsEmpty());
  EXPECT_NE(server_cid2, server_cid1);
  // Wait until path validation fails at the client.
  while (client_->client()->HasPendingPathValidation()) {
    EXPECT_EQ(server_cid2,
              QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection));
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(addr1, client_->client()->session()->self_address());
  EXPECT_EQ(server_cid1, GetClientConnection()->connection_id());

  server_writer_->set_fake_packet_loss_percentage(0);
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));

  WaitForNewConnectionIds();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(0u, client_connection->GetStats().num_new_connection_id_sent);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  // Server has received 3 path challenges.
  EXPECT_EQ(3u,
            server_connection->GetStats().num_connectivity_probing_received);
  EXPECT_EQ(server_cid1, server_connection->connection_id());
  EXPECT_EQ(0u, server_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(2u, server_connection->GetStats().num_new_connection_id_sent);
  server_thread_->Resume();

  // Migrate socket to a new IP address again.
  QuicIpAddress host3 = TestLoopback(3);
  EXPECT_NE(addr1.host(), host3);
  EXPECT_NE(host2, host3);

  WaitForNewConnectionIds();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(0u, client_connection->GetStats().num_new_connection_id_sent);

  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host3));
  QuicConnectionId server_cid3 =
      QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_FALSE(server_cid3.IsEmpty());
  EXPECT_NE(server_cid1, server_cid3);
  EXPECT_NE(server_cid2, server_cid3);
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(host3, client_->client()->session()->self_address().host());
  EXPECT_EQ(server_cid3, GetClientConnection()->connection_id());
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));

  // Server should send a new connection ID to client.
  WaitForNewConnectionIds();
  EXPECT_EQ(2u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(0u, client_connection->GetStats().num_new_connection_id_sent);
}

TEST_P(EndToEndPacketReorderingTest,
       MigrateAgainAfterPathValidationFailureWithNonZeroClientCid) {
  if (!version_.HasIetfQuicFrames()) {
    ASSERT_TRUE(Initialize());
    return;
  }
  override_client_connection_id_length_ = kQuicDefaultConnectionIdLength;
  ASSERT_TRUE(Initialize());

  client_.reset(CreateQuicClient(nullptr));
  // Finish one request to make sure handshake established.
  EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));

  // Wait for the connection to become idle, to make sure the packet gets
  // delayed is the connectivity probing packet.
  client_->WaitForDelayedAcks();

  QuicSocketAddress addr1 = client_->client()->session()->self_address();
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionId server_cid1 = client_connection->connection_id();
  QuicConnectionId client_cid1 = client_connection->client_connection_id();

  // Migrate socket to the new IP address.
  QuicIpAddress host2 = TestLoopback(2);
  EXPECT_NE(addr1.host(), host2);

  // Drop PATH_RESPONSE packets to timeout the path validation.
  server_writer_->set_fake_packet_loss_percentage(100);
  ASSERT_TRUE(
      QuicConnectionPeer::HasUnusedPeerIssuedConnectionId(client_connection));
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host2));
  QuicConnectionId server_cid2 =
      QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_FALSE(server_cid2.IsEmpty());
  EXPECT_NE(server_cid2, server_cid1);
  QuicConnectionId client_cid2 =
      QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_FALSE(client_cid2.IsEmpty());
  EXPECT_NE(client_cid2, client_cid1);
  while (client_->client()->HasPendingPathValidation()) {
    EXPECT_EQ(server_cid2,
              QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection));
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(addr1, client_->client()->session()->self_address());
  EXPECT_EQ(server_cid1, GetClientConnection()->connection_id());
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  server_writer_->set_fake_packet_loss_percentage(0);
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));
  WaitForNewConnectionIds();
  EXPECT_EQ(1u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(2u, client_connection->GetStats().num_new_connection_id_sent);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    EXPECT_EQ(3u,
              server_connection->GetStats().num_connectivity_probing_received);
    EXPECT_EQ(server_cid1, server_connection->connection_id());
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  EXPECT_EQ(1u, server_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(2u, server_connection->GetStats().num_new_connection_id_sent);
  server_thread_->Resume();

  // Migrate socket to a new IP address again.
  QuicIpAddress host3 = TestLoopback(3);
  EXPECT_NE(addr1.host(), host3);
  EXPECT_NE(host2, host3);
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host3));

  QuicConnectionId server_cid3 =
      QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_FALSE(server_cid3.IsEmpty());
  EXPECT_NE(server_cid1, server_cid3);
  EXPECT_NE(server_cid2, server_cid3);
  QuicConnectionId client_cid3 =
      QuicConnectionPeer::GetClientConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_NE(client_cid1, client_cid3);
  EXPECT_NE(client_cid2, client_cid3);
  while (client_->client()->HasPendingPathValidation()) {
    client_->client()->WaitForEvents();
  }
  EXPECT_EQ(host3, client_->client()->session()->self_address().host());
  EXPECT_EQ(server_cid3, GetClientConnection()->connection_id());
  EXPECT_TRUE(QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
                  client_connection)
                  .IsEmpty());
  EXPECT_EQ(kBarResponseBody, client_->SendSynchronousRequest("/bar"));

  // Server should send new server connection ID to client and retires old
  // client connection ID.
  WaitForNewConnectionIds();
  EXPECT_EQ(2u, client_connection->GetStats().num_retire_connection_id_sent);
  EXPECT_EQ(3u, client_connection->GetStats().num_new_connection_id_sent);
}

TEST_P(EndToEndPacketReorderingTest, Buffer0RttRequest) {
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls() &&
      GetQuicReloadableFlag(quic_require_handshake_confirmation)) {
    return;
  }
  // Finish one request to make sure handshake established.
  client_->SendSynchronousRequest("/foo");
  // Disconnect for next 0-rtt request.
  client_->Disconnect();

  // Client has valid Session Ticket now. Do a 0-RTT request.
  // Buffer a CHLO till the request is sent out. HTTP/3 sends two packets: a
  // SETTINGS frame and a request.
  reorder_writer_->SetDelay(version_.UsesHttp3() ? 2 : 1);
  // Only send out a CHLO.
  client_->client()->Initialize();

  // Send a request before handshake finishes.
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/bar";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  client_->SendMessage(headers, "");
  client_->WaitForResponse();
  EXPECT_EQ(kBarResponseBody, client_->response_body());
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  QuicConnectionStats client_stats = client_connection->GetStats();
  EXPECT_EQ(0u, client_stats.packets_lost);
  EXPECT_TRUE(client_->client()->EarlyDataAccepted());
}

TEST_P(EndToEndTest, SimpleStopSendingRstStreamTest) {
  ASSERT_TRUE(Initialize());

  // Send a request without a fin, to keep the stream open
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  client_->SendMessage(headers, "", /*fin=*/false);
  // Stream should be open
  ASSERT_NE(nullptr, client_->latest_created_stream());
  EXPECT_FALSE(client_->latest_created_stream()->write_side_closed());
  EXPECT_FALSE(
      QuicStreamPeer::read_side_closed(client_->latest_created_stream()));

  // Send a RST_STREAM+STOP_SENDING on the stream
  // Code is not important.
  client_->latest_created_stream()->Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  client_->WaitForResponse();

  // Stream should be gone.
  ASSERT_EQ(nullptr, client_->latest_created_stream());
}

class BadShloPacketWriter : public QuicPacketWriterWrapper {
 public:
  BadShloPacketWriter(ParsedQuicVersion version)
      : error_returned_(false), version_(version) {}
  ~BadShloPacketWriter() override {}

  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          quic::PerPacketOptions* options,
                          const quic::QuicPacketWriterParams& params) override {
    const WriteResult result = QuicPacketWriterWrapper::WritePacket(
        buffer, buf_len, self_address, peer_address, options, params);
    const uint8_t type_byte = buffer[0];
    if (!error_returned_ && (type_byte & FLAGS_LONG_HEADER) &&
        TypeByteIsServerHello(type_byte)) {
      QUIC_DVLOG(1) << "Return write error for packet containing ServerHello";
      error_returned_ = true;
      return WriteResult(WRITE_STATUS_ERROR, *MessageTooBigErrorCode());
    }
    return result;
  }

  bool TypeByteIsServerHello(uint8_t type_byte) {
    if (version_.UsesV2PacketTypes()) {
      return ((type_byte & 0x30) >> 4) == 3;
    }
    if (version_.UsesQuicCrypto()) {
      // ENCRYPTION_ZERO_RTT packet.
      return ((type_byte & 0x30) >> 4) == 1;
    }
    // ENCRYPTION_HANDSHAKE packet.
    return ((type_byte & 0x30) >> 4) == 2;
  }

 private:
  bool error_returned_;
  ParsedQuicVersion version_;
};

TEST_P(EndToEndTest, ConnectionCloseBeforeHandshakeComplete) {
  // This test ensures ZERO_RTT_PROTECTED connection close could close a client
  // which has switched to forward secure.
  connect_to_server_on_initialize_ = false;
  ASSERT_TRUE(Initialize());
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  if (dispatcher == nullptr) {
    ADD_FAILURE() << "Missing dispatcher";
    server_thread_->Resume();
    return;
  }
  if (dispatcher->NumSessions() > 0) {
    ADD_FAILURE() << "Dispatcher session map not empty";
    server_thread_->Resume();
    return;
  }
  // Note: this writer will only used by the server connection, not the time
  // wait list.
  QuicDispatcherPeer::UseWriter(
      dispatcher,
      // This causes the first server sent ZERO_RTT_PROTECTED packet (i.e.,
      // SHLO) to be sent, but WRITE_ERROR is returned. Such that a
      // ZERO_RTT_PROTECTED connection close would be sent to a client with
      // encryption level FORWARD_SECURE.
      new BadShloPacketWriter(version_));
  server_thread_->Resume();

  client_.reset(CreateQuicClient(client_writer_));
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  // Verify ZERO_RTT_PROTECTED connection close is successfully processed by
  // client which switches to FORWARD_SECURE.
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PACKET_WRITE_ERROR));
}

class BadShloPacketWriter2 : public QuicPacketWriterWrapper {
 public:
  BadShloPacketWriter2(ParsedQuicVersion version)
      : error_returned_(false), version_(version) {}
  ~BadShloPacketWriter2() override {}

  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          quic::PerPacketOptions* options,
                          const quic::QuicPacketWriterParams& params) override {
    const uint8_t type_byte = buffer[0];

    if (type_byte & FLAGS_LONG_HEADER) {
      if (((type_byte & 0x30 >> 4) == (version_.UsesV2PacketTypes() ? 2 : 1)) ||
          ((type_byte & 0x7F) == 0x7C)) {
        QUIC_DVLOG(1) << "Dropping ZERO_RTT_PACKET packet";
        return WriteResult(WRITE_STATUS_OK, buf_len);
      }
    } else if (!error_returned_) {
      QUIC_DVLOG(1) << "Return write error for short header packet";
      error_returned_ = true;
      return WriteResult(WRITE_STATUS_ERROR, *MessageTooBigErrorCode());
    }
    return QuicPacketWriterWrapper::WritePacket(buffer, buf_len, self_address,
                                                peer_address, options, params);
  }

 private:
  bool error_returned_;
  ParsedQuicVersion version_;
};

TEST_P(EndToEndTest, ForwardSecureConnectionClose) {
  // This test ensures ZERO_RTT_PROTECTED connection close is sent to a client
  // which has ZERO_RTT_PROTECTED encryption level.
  connect_to_server_on_initialize_ = false;
  ASSERT_TRUE(Initialize());
  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  if (dispatcher == nullptr) {
    ADD_FAILURE() << "Missing dispatcher";
    server_thread_->Resume();
    return;
  }
  if (dispatcher->NumSessions() > 0) {
    ADD_FAILURE() << "Dispatcher session map not empty";
    server_thread_->Resume();
    return;
  }
  // Note: this writer will only used by the server connection, not the time
  // wait list.
  QuicDispatcherPeer::UseWriter(
      dispatcher,
      // This causes the all server sent ZERO_RTT_PROTECTED packets to be
      // dropped, and first short header packet causes write error.
      new BadShloPacketWriter2(version_));
  server_thread_->Resume();
  client_.reset(CreateQuicClient(client_writer_));
  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  // Verify ZERO_RTT_PROTECTED connection close is successfully processed by
  // client.
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PACKET_WRITE_ERROR));
}

// Test that the stream id manager closes the connection if a stream
// in excess of the allowed maximum.
TEST_P(EndToEndTest, TooBigStreamIdClosesConnection) {
  // Has to be before version test, see EndToEndTest::TearDown()
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    // Only runs for IETF QUIC.
    return;
  }
  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  std::string body(kMaxOutgoingPacketSize, 'a');
  HttpHeaderBlock headers;
  headers[":method"] = "POST";
  headers[":path"] = "/foo";
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;

  // Force the client to write with a stream ID that exceeds the limit.
  QuicSpdySession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  QuicStreamIdManager* stream_id_manager =
      QuicSessionPeer::ietf_bidirectional_stream_id_manager(client_session);
  ASSERT_TRUE(stream_id_manager);
  QuicStreamCount max_number_of_streams =
      stream_id_manager->outgoing_max_streams();
  QuicSessionPeer::SetNextOutgoingBidirectionalStreamId(
      client_session,
      GetNthClientInitiatedBidirectionalId(max_number_of_streams + 1));
  client_->SendCustomSynchronousRequest(headers, body);
  EXPECT_THAT(client_->stream_error(),
              IsStreamError(QUIC_STREAM_CONNECTION_ERROR));
  EXPECT_THAT(client_session->error(), IsError(QUIC_INVALID_STREAM_ID));
  EXPECT_EQ(IETF_QUIC_TRANSPORT_CONNECTION_CLOSE, client_session->close_type());
  EXPECT_TRUE(
      IS_IETF_STREAM_FRAME(client_session->transport_close_frame_type()));
}

TEST_P(EndToEndTest, CustomTransportParameters) {
  if (!version_.UsesTls()) {
    // Custom transport parameters are only supported with TLS.
    ASSERT_TRUE(Initialize());
    return;
  }
  constexpr auto kCustomParameter =
      static_cast<TransportParameters::TransportParameterId>(0xff34);
  client_config_.custom_transport_parameters_to_send()[kCustomParameter] =
      "test";
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  connection_debug_visitor_ = &visitor;
  EXPECT_CALL(visitor, OnTransportParametersSent(_))
      .WillOnce([kCustomParameter](
                    const TransportParameters& transport_parameters) {
        auto it = transport_parameters.custom_parameters.find(kCustomParameter);
        ASSERT_NE(it, transport_parameters.custom_parameters.end());
        EXPECT_EQ(it->second, "test");
      });
  EXPECT_CALL(visitor, OnTransportParametersReceived(_)).Times(1);
  ASSERT_TRUE(Initialize());

  EXPECT_TRUE(client_->client()->WaitForOneRttKeysAvailable());

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  QuicConfig* server_config = nullptr;
  if (server_session != nullptr) {
    server_config = server_session->config();
  } else {
    ADD_FAILURE() << "Missing server session";
  }
  if (server_config != nullptr) {
    if (auto it = server_config->received_custom_transport_parameters().find(
            kCustomParameter);
        it != server_config->received_custom_transport_parameters().end()) {
      EXPECT_EQ(it->second, "test");
    } else {
      ADD_FAILURE() << "Did not find custom parameter";
    }
  } else {
    ADD_FAILURE() << "Missing server config";
  }
  server_thread_->Resume();
}

// Testing packet writer that parses initial packets and saves information
// relevant to chaos protection.
class ChaosPacketWriter : public PacketDroppingTestWriter {
 public:
  explicit ChaosPacketWriter(const ParsedQuicVersion& version,
                             bool drop_first_initial_packet)
      : framer_({version}),
        drop_next_initial_packet_(drop_first_initial_packet) {
    framer_.framer()->SetInitialObfuscators(TestConnectionId());
  }

  WriteResult WritePacket(const char* buffer, size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options,
                          const QuicPacketWriterParams& params) override {
    bool drop_packet = false;
    QuicEncryptedPacket packet(buffer, buf_len);
    if (framer_.ProcessPacket(packet)) {
      if (framer_.header().form == IETF_QUIC_LONG_HEADER_PACKET &&
          framer_.header().long_packet_type == INITIAL) {
        auto initial_packet = std::make_unique<InitialPacketContents>();
        for (const auto& frame : framer_.crypto_frames()) {
          QuicInterval<QuicStreamOffset> interval(
              frame->offset, frame->offset + frame->data_length);
          initial_packet->crypto_data_intervals.Add(interval);
          initial_packet->total_crypto_data_length += frame->data_length;
        }
        initial_packet->packet_number =
            framer_.header().packet_number.ToUint64();
        initial_packet->num_crypto_frames = framer_.crypto_frames().size();
        initial_packet->num_padding_frames = framer_.padding_frames().size();
        initial_packet->num_ping_frames = framer_.ping_frames().size();
        if (drop_next_initial_packet_) {
          drop_packet = true;
          drop_next_initial_packet_ = false;
          initial_packet->was_dropped = true;
        }
        initial_packets_.push_back(std::move(initial_packet));
      }
    }
    if (drop_packet) {
      return WriteResult(WRITE_STATUS_OK, buf_len);
    }
    return PacketDroppingTestWriter::WritePacket(buffer, buf_len, self_address,
                                                 peer_address, options, params);
  }

  struct InitialPacketContents {
    uint64_t packet_number = std::numeric_limits<uint64_t>::max();
    int num_crypto_frames = 0;
    int num_padding_frames = 0;
    int num_ping_frames = 0;
    bool was_dropped = false;
    QuicByteCount total_crypto_data_length = 0;
    QuicIntervalSet<QuicStreamOffset> crypto_data_intervals;
    QuicByteCount min_crypto_offset() const {
      return crypto_data_intervals.SpanningInterval().min();
    }
    QuicByteCount max_crypto_data() const {
      return crypto_data_intervals.SpanningInterval().max();
    }
  };

  const std::vector<std::unique_ptr<InitialPacketContents>>& initial_packets() {
    return initial_packets_;
  }

 private:
  SimpleQuicFramer framer_;
  std::vector<std::unique_ptr<InitialPacketContents>> initial_packets_;
  bool drop_next_initial_packet_;
};

TEST_P(EndToEndTest, KyberChaosProtection) {
  TestMultiPacketChaosProtection(/*num_packets=*/2,
                                 /*drop_first_packet=*/false,
                                 /*kyber=*/true);
}

TEST_P(EndToEndTest, KyberChaosProtectionWithRetransmission) {
  TestMultiPacketChaosProtection(/*num_packets=*/2,
                                 /*drop_first_packet=*/true,
                                 /*kyber=*/true);
}

TEST_P(EndToEndTest, TwoPacketChaosProtection) {
  TestMultiPacketChaosProtection(/*num_packets=*/2,
                                 /*drop_first_packet=*/false);
}

TEST_P(EndToEndTest, TwoPacketChaosProtectionWithRetransmission) {
  TestMultiPacketChaosProtection(/*num_packets=*/2,
                                 /*drop_first_packet=*/true);
}

TEST_P(EndToEndTest, ThreePacketChaosProtection) {
  TestMultiPacketChaosProtection(/*num_packets=*/3,
                                 /*drop_first_packet=*/false);
}

TEST_P(EndToEndTest, ThreePacketChaosProtectionWithRetransmission) {
  TestMultiPacketChaosProtection(/*num_packets=*/3,
                                 /*drop_first_packet=*/true);
}

TEST_P(EndToEndTest, FourPacketChaosProtection) {
  TestMultiPacketChaosProtection(/*num_packets=*/4,
                                 /*drop_first_packet=*/false);
}

TEST_P(EndToEndTest, FivePacketChaosProtection) {
  // Regression test for b/387486449.
  TestMultiPacketChaosProtection(/*num_packets=*/5,
                                 /*drop_first_packet=*/false);
}

void EndToEndTest::TestMultiPacketChaosProtection(int num_packets,
                                                  bool drop_first_packet,
                                                  bool kyber) {
  if (!version_.HasIetfQuicFrames()) {
    ASSERT_TRUE(Initialize());
    return;
  }
  // Setup test harness with a custom client writer.
  connect_to_server_on_initialize_ = false;
  int discard_length;
  if (kyber) {
    discard_length = 1216;
    enable_mlkem_in_client_ = true;
  } else {
    discard_length = 1000 * num_packets;
    client_config_.SetDiscardLengthToSend(discard_length);
  }
  ASSERT_TRUE(Initialize());
  auto copying_writer = new ChaosPacketWriter(version_, drop_first_packet);
  delete client_writer_;
  client_writer_ = copying_writer;
  client_.reset(CreateQuicClient(client_writer_, /*connect=*/false));
  client_->UseConnectionId(TestConnectionId());
  client_->Connect();
  MockableQuicClient* client = client_->client();
  QuicConnection* client_connection = GetClientConnection();
  client_writer_->Initialize(
      QuicConnectionPeer::GetHelper(client_connection),
      QuicConnectionPeer::GetAlarmFactory(client_connection),
      std::make_unique<ClientDelegate>(client));
  EXPECT_TRUE(client->connected());
  // Make sure application data can be sent.
  EXPECT_TRUE(SendSynchronousFooRequestAndCheckResponse());

  // Make sure the first flight contains the entire client hello.
  QuicIntervalSet<QuicStreamOffset> crypto_data_intervals;
  int num_first_flight_packets = 0;
  for (size_t i = 0; i < copying_writer->initial_packets().size(); ++i) {
    if (copying_writer->initial_packets()[i]->crypto_data_intervals.Empty()) {
      continue;
    }
    bool found = false;
    for (const auto& interval :
         copying_writer->initial_packets()[i]->crypto_data_intervals) {
      if (!crypto_data_intervals.IsDisjoint(interval)) {
        found = true;
      }
      crypto_data_intervals.Add(interval);
    }
    if (found) {
      break;
    }
    num_first_flight_packets++;
  }
  EXPECT_EQ(num_first_flight_packets, num_packets);
  EXPECT_EQ(crypto_data_intervals.Size(), 1u);
  EXPECT_EQ(crypto_data_intervals.SpanningInterval().min(), 0u);
  EXPECT_GT(crypto_data_intervals.SpanningInterval().max(), discard_length);

  for (int i = 1; i <= num_packets; ++i) {
    ASSERT_GE(copying_writer->initial_packets().size(), i);
    auto& packet = copying_writer->initial_packets()[i - 1];
    EXPECT_EQ(packet->was_dropped, drop_first_packet && i == 1);
    EXPECT_EQ(packet->packet_number, i);
    if (i == 1 || i == num_packets) {
      // Ensure first and last packets are properly chaos protected.
      EXPECT_TRUE(packet->num_crypto_frames > 2 ||
                  packet->num_ping_frames > 0 || packet->num_padding_frames > 1)
          << "crypto=" << packet->num_crypto_frames
          << ", ping=" << packet->num_ping_frames
          << ", pad=" << packet->num_padding_frames;
    } else {
      // Middle packets do not have single-packet chaos protection.
      EXPECT_GE(packet->num_crypto_frames, 1u);
    }
    if (i == 1) {
      EXPECT_EQ(packet->min_crypto_offset(), 0u);
      EXPECT_GE(packet->max_crypto_data(), discard_length);
    } else {
      EXPECT_GT(packet->min_crypto_offset(), 0u);
      EXPECT_LT(packet->max_crypto_data(), discard_length);
    }
    EXPECT_GE(packet->total_crypto_data_length, 500u);
  }

  if (!drop_first_packet) {
    return;
  }
  // Retransmission of the first packet contains the start and end of the client
  // hello. This validates that the multiple crypto frames are retransmitted in
  // the same packet, without the packet creator flushing between them.
  bool found_retransmission = false;
  for (size_t i = num_packets; i < copying_writer->initial_packets().size();
       ++i) {
    // Iterate on subsequent packets until we find the one that contains the
    // retransmission of the crypto frame that contains the start of the client
    // hello.
    auto& packet = copying_writer->initial_packets()[i];
    if (packet->num_crypto_frames == 0 || packet->min_crypto_offset() != 0) {
      continue;
    }
    found_retransmission = true;
    EXPECT_FALSE(packet->was_dropped);
    EXPECT_GE(packet->num_crypto_frames, 2u);
    EXPECT_GE(packet->max_crypto_data(), discard_length);
    EXPECT_GE(packet->total_crypto_data_length, 500u);
  }
  EXPECT_TRUE(found_retransmission);
}

TEST_P(EndToEndTest, KeyUpdateInitiatedByClient) {
  if (!version_.UsesTls()) {
    // Key Update is only supported in TLS handshake.
    ASSERT_TRUE(Initialize());
    return;
  }

  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(0u, client_connection->GetStats().key_update_count);

  EXPECT_TRUE(
      client_connection->InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().key_update_count);

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().key_update_count);

  EXPECT_TRUE(
      client_connection->InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));
  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(2u, client_connection->GetStats().key_update_count);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection) {
    QuicConnectionStats server_stats = server_connection->GetStats();
    EXPECT_EQ(2u, server_stats.key_update_count);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, KeyUpdateInitiatedByServer) {
  if (!version_.UsesTls()) {
    // Key Update is only supported in TLS handshake.
    ASSERT_TRUE(Initialize());
    return;
  }

  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(0u, client_connection->GetStats().key_update_count);

  // Use WaitUntil to ensure the server had executed the key update predicate
  // before sending the Foo request, otherwise the test can be flaky if it
  // receives the Foo request before executing the key update.
  server_thread_->WaitUntil(
      [this]() {
        QuicConnection* server_connection = GetServerConnection();
        if (server_connection != nullptr) {
          if (!server_connection->IsKeyUpdateAllowed()) {
            // Server may not have received ack from client yet for the current
            // key phase, wait a bit and try again.
            return false;
          }
          EXPECT_TRUE(server_connection->InitiateKeyUpdate(
              KeyUpdateReason::kLocalForTests));
        } else {
          ADD_FAILURE() << "Missing server connection";
        }
        return true;
      },
      QuicTime::Delta::FromSeconds(5));

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().key_update_count);

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().key_update_count);

  server_thread_->WaitUntil(
      [this]() {
        QuicConnection* server_connection = GetServerConnection();
        if (server_connection != nullptr) {
          if (!server_connection->IsKeyUpdateAllowed()) {
            return false;
          }
          EXPECT_TRUE(server_connection->InitiateKeyUpdate(
              KeyUpdateReason::kLocalForTests));
        } else {
          ADD_FAILURE() << "Missing server connection";
        }
        return true;
      },
      QuicTime::Delta::FromSeconds(5));

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(2u, client_connection->GetStats().key_update_count);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection) {
    QuicConnectionStats server_stats = server_connection->GetStats();
    EXPECT_EQ(2u, server_stats.key_update_count);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, KeyUpdateInitiatedByBoth) {
  if (!version_.UsesTls()) {
    // Key Update is only supported in TLS handshake.
    ASSERT_TRUE(Initialize());
    return;
  }

  ASSERT_TRUE(Initialize());

  SendSynchronousFooRequestAndCheckResponse();

  // Use WaitUntil to ensure the server had executed the key update predicate
  // before the client sends the Foo request, otherwise the Foo request from
  // the client could trigger the server key update before the server can
  // initiate the key update locally. That would mean the test is no longer
  // hitting the intended test state of both sides locally initiating a key
  // update before receiving a packet in the new key phase from the other side.
  // Additionally the test would fail since InitiateKeyUpdate() would not allow
  // to do another key update yet and return false.
  server_thread_->WaitUntil(
      [this]() {
        QuicConnection* server_connection = GetServerConnection();
        if (server_connection != nullptr) {
          if (!server_connection->IsKeyUpdateAllowed()) {
            // Server may not have received ack from client yet for the current
            // key phase, wait a bit and try again.
            return false;
          }
          EXPECT_TRUE(server_connection->InitiateKeyUpdate(
              KeyUpdateReason::kLocalForTests));
        } else {
          ADD_FAILURE() << "Missing server connection";
        }
        return true;
      },
      QuicTime::Delta::FromSeconds(5));
  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_TRUE(
      client_connection->InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().key_update_count);

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(1u, client_connection->GetStats().key_update_count);

  server_thread_->WaitUntil(
      [this]() {
        QuicConnection* server_connection = GetServerConnection();
        if (server_connection != nullptr) {
          if (!server_connection->IsKeyUpdateAllowed()) {
            return false;
          }
          EXPECT_TRUE(server_connection->InitiateKeyUpdate(
              KeyUpdateReason::kLocalForTests));
        } else {
          ADD_FAILURE() << "Missing server connection";
        }
        return true;
      },
      QuicTime::Delta::FromSeconds(5));
  EXPECT_TRUE(
      client_connection->InitiateKeyUpdate(KeyUpdateReason::kLocalForTests));

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_EQ(2u, client_connection->GetStats().key_update_count);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection) {
    QuicConnectionStats server_stats = server_connection->GetStats();
    EXPECT_EQ(2u, server_stats.key_update_count);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, KeyUpdateInitiatedByConfidentialityLimit) {
  SetQuicFlag(quic_key_update_confidentiality_limit, 16U);

  if (!version_.UsesTls()) {
    // Key Update is only supported in TLS handshake.
    ASSERT_TRUE(Initialize());
    return;
  }

  ASSERT_TRUE(Initialize());

  QuicConnection* client_connection = GetClientConnection();
  ASSERT_TRUE(client_connection);
  EXPECT_EQ(0u, client_connection->GetStats().key_update_count);

  server_thread_->WaitUntil(
      [this]() {
        QuicConnection* server_connection = GetServerConnection();
        if (server_connection != nullptr) {
          EXPECT_EQ(0u, server_connection->GetStats().key_update_count);
        } else {
          ADD_FAILURE() << "Missing server connection";
        }
        return true;
      },
      QuicTime::Delta::FromSeconds(5));

  for (uint64_t i = 0; i < GetQuicFlag(quic_key_update_confidentiality_limit);
       ++i) {
    SendSynchronousFooRequestAndCheckResponse();
  }

  // Don't know exactly how many packets will be sent in each request/response,
  // so just test that at least one key update occurred.
  EXPECT_LE(1u, client_connection->GetStats().key_update_count);

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection) {
    QuicConnectionStats server_stats = server_connection->GetStats();
    EXPECT_LE(1u, server_stats.key_update_count);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, TlsResumptionEnabledOnTheFly) {
  SetQuicFlag(quic_disable_server_tls_resumption, true);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesTls()) {
    // This test is TLS specific.
    return;
  }

  // Send the first request. Client should not have a resumption ticket.
  SendSynchronousFooRequestAndCheckResponse();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_EQ(client_session->GetCryptoStream()->EarlyDataReason(),
            ssl_early_data_no_session_offered);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  client_->Disconnect();

  SetQuicFlag(quic_disable_server_tls_resumption, false);

  // Send the second request. Client should still have no resumption ticket, but
  // it will receive one which can be used by the next request.
  client_->Connect();
  SendSynchronousFooRequestAndCheckResponse();

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_EQ(client_session->GetCryptoStream()->EarlyDataReason(),
            ssl_early_data_no_session_offered);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  client_->Disconnect();

  // Send the third request in 0RTT.
  client_->Connect();
  SendSynchronousFooRequestAndCheckResponse();

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  client_->Disconnect();
}

TEST_P(EndToEndTest, TlsResumptionDisabledOnTheFly) {
  SetQuicFlag(quic_disable_server_tls_resumption, false);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesTls()) {
    // This test is TLS specific.
    return;
  }

  // Send the first request and then disconnect.
  SendSynchronousFooRequestAndCheckResponse();
  QuicSpdyClientSession* client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  client_->Disconnect();

  // Send the second request in 0RTT.
  client_->Connect();
  SendSynchronousFooRequestAndCheckResponse();

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_TRUE(client_session->EarlyDataAccepted());
  client_->Disconnect();

  SetQuicFlag(quic_disable_server_tls_resumption, true);

  // Send the third request. The client should try resumption but server should
  // decline it.
  client_->Connect();
  SendSynchronousFooRequestAndCheckResponse();

  client_session = GetClientSession();
  ASSERT_TRUE(client_session);
  EXPECT_FALSE(client_session->EarlyDataAccepted());
  EXPECT_EQ(client_session->GetCryptoStream()->EarlyDataReason(),
            ssl_early_data_session_not_resumed);
  client_->Disconnect();

  // Keep sending until the client runs out of resumption tickets.
  for (int i = 0; i < 10; ++i) {
    client_->Connect();
    SendSynchronousFooRequestAndCheckResponse();

    client_session = GetClientSession();
    ASSERT_TRUE(client_session);
    EXPECT_FALSE(client_session->EarlyDataAccepted());
    const auto early_data_reason =
        client_session->GetCryptoStream()->EarlyDataReason();
    client_->Disconnect();

    if (early_data_reason != ssl_early_data_session_not_resumed) {
      EXPECT_EQ(early_data_reason, ssl_early_data_unsupported_for_session);
      return;
    }
  }

  ADD_FAILURE() << "Client should not have 10 resumption tickets.";
}

TEST_P(EndToEndTest, BlockServerUntilSettingsReceived) {
  SetQuicReloadableFlag(quic_block_until_settings_received_copt, true);
  // Force loss to test data stream being blocked when SETTINGS are missing.
  SetPacketLossPercentage(30);
  client_extra_copts_.push_back(kBSUS);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  SendSynchronousFooRequestAndCheckResponse();

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  server_thread_->Resume();
  EXPECT_FALSE(GetClientSession()->ShouldBufferRequestsUntilSettings());
  server_thread_->ScheduleAndWaitForCompletion([server_session] {
    EXPECT_TRUE(server_session->ShouldBufferRequestsUntilSettings());
  });
}

TEST_P(EndToEndTest, WebTransportSessionSetup) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* web_transport =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_NE(web_transport, nullptr);

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  EXPECT_TRUE(server_session->GetWebTransportSession(web_transport->id()) !=
              nullptr);
  server_thread_->Resume();
}

TEST_P(EndToEndTest, WebTransportSessionProtocolNegotiation) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session = CreateWebTransportSession(
      "/selected-subprotocol", /*wait_for_server_response=*/true,
      {{webtransport::kSubprotocolRequestHeader, R"("a", "b", "c", "d")"},
       {"subprotocol-index", "1"}});
  ASSERT_NE(session, nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);
  EXPECT_EQ(session->GetNegotiatedSubprotocol(), "b");

  WebTransportStream* received_stream =
      session->AcceptIncomingUnidirectionalStream();
  if (received_stream == nullptr) {
    // Retry if reordering happens.
    bool stream_received = false;
    EXPECT_CALL(visitor, OnIncomingUnidirectionalStreamAvailable())
        .WillOnce(Assign(&stream_received, true));
    client_->WaitUntil(2000, [&stream_received]() { return stream_received; });
    received_stream = session->AcceptIncomingUnidirectionalStream();
  }
  ASSERT_TRUE(received_stream != nullptr);
  std::string received_data;
  WebTransportStream::ReadResult result = received_stream->Read(&received_data);
  EXPECT_EQ(received_data, "b");
  EXPECT_TRUE(result.fin);
}

TEST_P(EndToEndTest, WebTransportSessionSetupWithEchoWithSuffix) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  // "/echoFoo" should be accepted as "echo" with "set-header" query.
  WebTransportHttp3* web_transport = CreateWebTransportSession(
      "/echoFoo?set-header=bar:baz", /*wait_for_server_response=*/true);
  ASSERT_NE(web_transport, nullptr);

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  EXPECT_TRUE(server_session->GetWebTransportSession(web_transport->id()) !=
              nullptr);
  server_thread_->Resume();
  const quiche::HttpHeaderBlock* response_headers = client_->response_headers();
  auto it = response_headers->find("bar");
  EXPECT_NE(it, response_headers->end());
  EXPECT_EQ(it->second, "baz");
}

TEST_P(EndToEndTest, WebTransportSessionWithLoss) {
  enable_web_transport_ = true;
  // Enable loss to verify all permutations of receiving SETTINGS and
  // request/response data.
  SetPacketLossPercentage(30);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* web_transport =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_NE(web_transport, nullptr);

  server_thread_->Pause();
  QuicSpdySession* server_session = GetServerSession();
  EXPECT_TRUE(server_session->GetWebTransportSession(web_transport->id()) !=
              nullptr);
  server_thread_->Resume();
}

TEST_P(EndToEndTest, WebTransportSessionUnidirectionalStream) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  WebTransportStream* outgoing_stream =
      session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(outgoing_stream != nullptr);
  EXPECT_EQ(outgoing_stream,
            session->GetStreamById(outgoing_stream->GetStreamId()));

  auto stream_visitor =
      std::make_unique<NiceMock<MockWebTransportStreamVisitor>>();
  bool data_acknowledged = false;
  EXPECT_CALL(*stream_visitor, OnWriteSideInDataRecvdState())
      .WillOnce(Assign(&data_acknowledged, true));
  outgoing_stream->SetVisitor(std::move(stream_visitor));

  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*outgoing_stream, "test"));
  EXPECT_TRUE(outgoing_stream->SendFin());

  bool stream_received = false;
  EXPECT_CALL(visitor, OnIncomingUnidirectionalStreamAvailable())
      .WillOnce(Assign(&stream_received, true));
  client_->WaitUntil(2000, [&stream_received]() { return stream_received; });
  EXPECT_TRUE(stream_received);
  WebTransportStream* received_stream =
      session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(received_stream != nullptr);
  EXPECT_EQ(received_stream,
            session->GetStreamById(received_stream->GetStreamId()));
  std::string received_data;
  WebTransportStream::ReadResult result = received_stream->Read(&received_data);
  EXPECT_EQ(received_data, "test");
  EXPECT_TRUE(result.fin);

  client_->WaitUntil(2000,
                     [&data_acknowledged]() { return data_acknowledged; });
  EXPECT_TRUE(data_acknowledged);
}

TEST_P(EndToEndTest, WebTransportSessionUnidirectionalStreamSentEarly) {
  enable_web_transport_ = true;
  SetPacketLossPercentage(30);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/false);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  WebTransportStream* outgoing_stream =
      session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(outgoing_stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*outgoing_stream, "test"));
  EXPECT_TRUE(outgoing_stream->SendFin());

  bool stream_received = false;
  EXPECT_CALL(visitor, OnIncomingUnidirectionalStreamAvailable())
      .WillOnce(Assign(&stream_received, true));
  client_->WaitUntil(5000, [&stream_received]() { return stream_received; });
  EXPECT_TRUE(stream_received);
  WebTransportStream* received_stream =
      session->AcceptIncomingUnidirectionalStream();
  ASSERT_TRUE(received_stream != nullptr);
  std::string received_data;
  WebTransportStream::ReadResult result = received_stream->Read(&received_data);
  EXPECT_EQ(received_data, "test");
  EXPECT_TRUE(result.fin);
}

TEST_P(EndToEndTest, WebTransportSessionBidirectionalStream) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);

  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  EXPECT_EQ(stream, session->GetStreamById(stream->GetStreamId()));

  auto stream_visitor_owned =
      std::make_unique<NiceMock<MockWebTransportStreamVisitor>>();
  MockWebTransportStreamVisitor* stream_visitor = stream_visitor_owned.get();
  bool data_acknowledged = false;
  EXPECT_CALL(*stream_visitor, OnWriteSideInDataRecvdState())
      .WillOnce(Assign(&data_acknowledged, true));
  stream->SetVisitor(std::move(stream_visitor_owned));

  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  EXPECT_TRUE(stream->SendFin());

  std::string received_data =
      ReadDataFromWebTransportStreamUntilFin(stream, stream_visitor);
  EXPECT_EQ(received_data, "test");

  client_->WaitUntil(2000,
                     [&data_acknowledged]() { return data_acknowledged; });
  EXPECT_TRUE(data_acknowledged);
}

TEST_P(EndToEndTest, WebTransportSessionBidirectionalStreamWithBuffering) {
  enable_web_transport_ = true;
  SetPacketLossPercentage(30);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/false);
  ASSERT_TRUE(session != nullptr);

  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  EXPECT_TRUE(stream->SendFin());

  std::string received_data = ReadDataFromWebTransportStreamUntilFin(stream);
  EXPECT_EQ(received_data, "test");
}

TEST_P(EndToEndTest, WebTransportSessionServerBidirectionalStream) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/false);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  bool stream_received = false;
  EXPECT_CALL(visitor, OnIncomingBidirectionalStreamAvailable())
      .WillOnce(Assign(&stream_received, true));
  client_->WaitUntil(5000, [&stream_received]() { return stream_received; });
  EXPECT_TRUE(stream_received);

  WebTransportStream* stream = session->AcceptIncomingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  // Test the full Writev() API.
  const std::string kLongString = std::string(16 * 1024, 'a');
  std::array write_vector = {quiche::QuicheMemSlice::Copy("foo"),
                             quiche::QuicheMemSlice::Copy("bar"),
                             quiche::QuicheMemSlice::Copy("test"),
                             quiche::QuicheMemSlice::Copy(kLongString)};
  quiche::StreamWriteOptions options;
  options.set_send_fin(true);
  QUICHE_EXPECT_OK(stream->Writev(absl::MakeSpan(write_vector), options));

  std::string received_data = ReadDataFromWebTransportStreamUntilFin(stream);
  EXPECT_EQ(received_data, absl::StrCat("foobartest", kLongString));
}

TEST_P(EndToEndTest, WebTransportDatagrams) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  quiche::SimpleBufferAllocator allocator;
  for (int i = 0; i < 10; i++) {
    session->SendOrQueueDatagram("test");
  }

  int received = 0;
  EXPECT_CALL(visitor, OnDatagramReceived(_)).WillRepeatedly([&received]() {
    received++;
  });
  client_->WaitUntil(5000, [&received]() { return received > 0; });
  EXPECT_GT(received, 0);
}

TEST_P(EndToEndTest, WebTransportSessionClose) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  QuicStreamId stream_id = stream->GetStreamId();
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  // Keep stream open.

  bool close_received = false;
  EXPECT_CALL(visitor, OnSessionClosed(42, "test error"))
      .WillOnce(Assign(&close_received, true));
  session->CloseSession(42, "test error");
  client_->WaitUntil(2000, [&]() { return close_received; });
  EXPECT_TRUE(close_received);

  QuicSpdyStream* spdy_stream =
      GetClientSession()->GetOrCreateSpdyDataStream(stream_id);
  EXPECT_TRUE(spdy_stream == nullptr);
}

TEST_P(EndToEndTest, WebTransportSessionCloseWithoutCapsule) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  QuicStreamId stream_id = stream->GetStreamId();
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  // Keep stream open.

  bool close_received = false;
  EXPECT_CALL(visitor, OnSessionClosed(0, ""))
      .WillOnce(Assign(&close_received, true));
  session->CloseSessionWithFinOnlyForTests();
  client_->WaitUntil(2000, [&]() { return close_received; });
  EXPECT_TRUE(close_received);

  QuicSpdyStream* spdy_stream =
      GetClientSession()->GetOrCreateSpdyDataStream(stream_id);
  EXPECT_TRUE(spdy_stream == nullptr);
}

TEST_P(EndToEndTest, WebTransportSessionReceiveClose) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session = CreateWebTransportSession(
      "/session-close", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);
  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);

  WebTransportStream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  QuicStreamId stream_id = stream->GetStreamId();
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "42 test error"));
  EXPECT_TRUE(stream->SendFin());

  // Have some other streams open pending, to ensure they are closed properly.
  stream = session->OpenOutgoingUnidirectionalStream();
  stream = session->OpenOutgoingBidirectionalStream();

  bool close_received = false;
  EXPECT_CALL(visitor, OnSessionClosed(42, "test error"))
      .WillOnce(Assign(&close_received, true));
  client_->WaitUntil(2000, [&]() { return close_received; });
  EXPECT_TRUE(close_received);

  QuicSpdyStream* spdy_stream =
      GetClientSession()->GetOrCreateSpdyDataStream(stream_id);
  EXPECT_TRUE(spdy_stream == nullptr);
}

TEST_P(EndToEndTest, WebTransportSessionReceiveDrain) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session = CreateWebTransportSession(
      "/session-close", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);

  WebTransportStream* stream = session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "DRAIN"));
  EXPECT_TRUE(stream->SendFin());

  bool drain_received = false;
  session->SetOnDraining([&drain_received] { drain_received = true; });
  client_->WaitUntil(2000, [&]() { return drain_received; });
  EXPECT_TRUE(drain_received);
}

TEST_P(EndToEndTest, WebTransportSessionStreamTermination) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/resets", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);

  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);
  EXPECT_CALL(visitor, OnIncomingUnidirectionalStreamAvailable())
      .WillRepeatedly([this, session]() {
        ReadAllIncomingWebTransportUnidirectionalStreams(session);
      });

  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  QuicStreamId id1 = stream->GetStreamId();
  ASSERT_TRUE(stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  stream->ResetWithUserCode(42);

  // This read fails if the stream is closed in both directions, since that
  // results in stream object being deleted.
  std::string received_data = ReadDataFromWebTransportStreamUntilFin(stream);
  EXPECT_LE(received_data.size(), 4u);

  stream = session->OpenOutgoingBidirectionalStream();
  QuicStreamId id2 = stream->GetStreamId();
  ASSERT_TRUE(stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  stream->SendStopSending(100024);

  std::array<std::string, 2> expected_log = {
      absl::StrCat("Received reset for stream ", id1, " with error code 42"),
      absl::StrCat("Received stop sending for stream ", id2,
                   " with error code 100024"),
  };
  client_->WaitUntil(2000, [this, &expected_log]() {
    return received_webtransport_unidirectional_streams_.size() >=
           expected_log.size();
  });
  EXPECT_THAT(received_webtransport_unidirectional_streams_,
              UnorderedElementsAreArray(expected_log));

  // Since we closed the read side, cleanly closing the write side should result
  // in the stream getting deleted.
  ASSERT_TRUE(GetClientSession()->GetOrCreateSpdyDataStream(id2) != nullptr);
  EXPECT_TRUE(stream->SendFin());
  EXPECT_TRUE(client_->WaitUntil(2000, [this, id2]() {
    return GetClientSession()->GetOrCreateSpdyDataStream(id2) == nullptr;
  }));
}

// This test currently does not pass; we need support for
// https://datatracker.ietf.org/doc/draft-seemann-quic-reliable-stream-reset/ in
// order to make this work.
TEST_P(EndToEndTest, DISABLED_WebTransportSessionResetReliability) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  SetPacketLossPercentage(30);

  WebTransportHttp3* session =
      CreateWebTransportSession("/resets", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);

  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);
  EXPECT_CALL(visitor, OnIncomingUnidirectionalStreamAvailable())
      .WillRepeatedly([this, session]() {
        ReadAllIncomingWebTransportUnidirectionalStreams(session);
      });

  std::vector<std::string> expected_log;
  constexpr int kStreamsToCreate = 10;
  for (int i = 0; i < kStreamsToCreate; i++) {
    WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
    QuicStreamId id = stream->GetStreamId();
    ASSERT_TRUE(stream != nullptr);
    stream->ResetWithUserCode(42);

    expected_log.push_back(
        absl::StrCat("Received reset for stream ", id, " with error code 42"));
  }
  client_->WaitUntil(2000, [this, &expected_log]() {
    return received_webtransport_unidirectional_streams_.size() >=
           expected_log.size();
  });
  EXPECT_THAT(received_webtransport_unidirectional_streams_,
              UnorderedElementsAreArray(expected_log));
}

TEST_P(EndToEndTest, WebTransportSession404) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session = CreateWebTransportSession(
      "/does-not-exist", /*wait_for_server_response=*/false);
  ASSERT_TRUE(session != nullptr);
  QuicSpdyStream* connect_stream = client_->latest_created_stream();
  QuicStreamId connect_stream_id = connect_stream->id();

  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  EXPECT_TRUE(stream->SendFin());

  EXPECT_TRUE(client_->WaitUntil(-1, [this, connect_stream_id]() {
    return GetClientSession()->GetOrCreateSpdyDataStream(connect_stream_id) ==
           nullptr;
  }));
}
TEST_P(EndToEndTest, WebTransportSessionGoaway) {
  enable_web_transport_ = true;
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }

  WebTransportHttp3* session =
      CreateWebTransportSession("/echo", /*wait_for_server_response=*/true);
  ASSERT_TRUE(session != nullptr);

  NiceMock<MockWebTransportSessionVisitor>& visitor =
      SetupWebTransportVisitor(session);
  bool goaway_received = false;
  session->SetOnDraining([&goaway_received] { goaway_received = true; });
  server_thread_->Schedule([server_session = GetServerSession()]() {
    server_session->SendHttp3GoAway(QUIC_PEER_GOING_AWAY,
                                    "server shutting down");
  });
  client_->WaitUntil(2000, [&]() { return goaway_received; });
  EXPECT_TRUE(goaway_received);

  // Ensure that we can still send and receive unidirectional streams after
  // GOAWAY has been processed.
  WebTransportStream* outgoing_stream =
      session->OpenOutgoingUnidirectionalStream();
  ASSERT_TRUE(outgoing_stream != nullptr);
  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*outgoing_stream, "test"));
  EXPECT_TRUE(outgoing_stream->SendFin());

  EXPECT_CALL(visitor, OnIncomingUnidirectionalStreamAvailable())
      .WillRepeatedly([this, session]() {
        ReadAllIncomingWebTransportUnidirectionalStreams(session);
      });
  client_->WaitUntil(2000, [this]() {
    return !received_webtransport_unidirectional_streams_.empty();
  });
  EXPECT_THAT(received_webtransport_unidirectional_streams_,
              testing::ElementsAre("test"));

// TODO(b/283160645): fix this and re-enable the test.
#if 0
  // Ensure that we can still send and receive bidirectional data streams after
  // GOAWAY has been processed.
  WebTransportStream* stream = session->OpenOutgoingBidirectionalStream();
  ASSERT_TRUE(stream != nullptr);

  auto stream_visitor_owned =
      std::make_unique<NiceMock<MockWebTransportStreamVisitor>>();
  MockWebTransportStreamVisitor* stream_visitor = stream_visitor_owned.get();
  stream->SetVisitor(std::move(stream_visitor_owned));

  QUICHE_EXPECT_OK(quiche::WriteIntoStream(*stream, "test"));
  EXPECT_TRUE(stream->SendFin());

  std::string received_data =
      ReadDataFromWebTransportStreamUntilFin(stream, stream_visitor);
  EXPECT_EQ(received_data, "test");
#endif
}

TEST_P(EndToEndTest, InvalidExtendedConnect) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }
  // Missing :path header.
  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":authority"] = "localhost";
  headers[":method"] = "CONNECT";
  headers[":protocol"] = "webtransport";

  client_->SendMessage(headers, "", /*fin=*/false);
  client_->WaitForResponse();
  // An early response should be received.
  CheckResponseHeaders("400");
}

TEST_P(EndToEndTest, RejectExtendedConnect) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  // Disable extended CONNECT.
  memory_cache_backend_.set_enable_extended_connect(false);
  ASSERT_TRUE(Initialize());

  if (!version_.UsesHttp3()) {
    return;
  }
  // This extended CONNECT should be rejected.
  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":authority"] = "localhost";
  headers[":method"] = "CONNECT";
  headers[":path"] = "/echo";
  headers[":protocol"] = "webtransport";

  client_->SendMessage(headers, "", /*fin=*/false);
  client_->WaitForResponse();
  CheckResponseHeaders("400");

  // Vanilla CONNECT should be sent to backend.
  quiche::HttpHeaderBlock headers2;
  headers2[":authority"] = "localhost";
  headers2[":method"] = "CONNECT";

  // Backend not configured/implemented to fully handle CONNECT requests, so
  // expect it to send a 405.
  client_->SendMessage(headers2, "body", /*fin=*/true);
  client_->WaitForResponse();
  CheckResponseHeaders("405");
}

TEST_P(EndToEndTest, RejectInvalidRequestHeader) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  ASSERT_TRUE(Initialize());

  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":authority"] = "localhost";
  headers[":method"] = "GET";
  headers[":path"] = "/echo";
  // transfer-encoding header is not allowed.
  headers["transfer-encoding"] = "chunk";

  client_->SendMessage(headers, "", /*fin=*/false);
  client_->WaitForResponse();
  CheckResponseHeaders("400");
}

TEST_P(EndToEndTest, RejectTransferEncodingResponse) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  ASSERT_TRUE(Initialize());

  // Add a response with transfer-encoding headers.
  HttpHeaderBlock headers;
  headers[":status"] = "200";
  headers["transfer-encoding"] = "gzip";

  HttpHeaderBlock trailers;
  trailers["some-trailing-header"] = "trailing-header-value";

  memory_cache_backend_.AddResponse(server_hostname_, "/eep",
                                    std::move(headers), "", trailers.Clone());

  std::string received_response = client_->SendSynchronousRequest("/eep");
  EXPECT_THAT(client_->stream_error(),
              IsStreamError(QUIC_BAD_APPLICATION_PAYLOAD));
}

TEST_P(EndToEndTest, RejectUpperCaseRequest) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  ASSERT_TRUE(Initialize());

  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":authority"] = "localhost";
  headers[":method"] = "GET";
  headers[":path"] = "/echo";
  headers["UpperCaseHeader"] = "foo";

  client_->SendMessage(headers, "", /*fin=*/false);
  client_->WaitForResponse();
  CheckResponseHeaders("400");
}

TEST_P(EndToEndTest, RejectRequestWithInvalidToken) {
  SetQuicReloadableFlag(quic_act_upon_invalid_header, true);
  ASSERT_TRUE(Initialize());

  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":authority"] = "localhost";
  headers[":method"] = "GET";
  headers[":path"] = "/echo";
  headers["invalid,header"] = "foo";

  client_->SendMessage(headers, "", /*fin=*/false);
  client_->WaitForResponse();
  CheckResponseHeaders("400");
}

TEST_P(EndToEndTest, OriginalConnectionIdClearedFromMap) {
  connect_to_server_on_initialize_ = false;
  ASSERT_TRUE(Initialize());
  if (override_client_connection_id_length_ != kLongConnectionIdLength) {
    // There might not be an original connection ID.
    CreateClientWithWriter();
    return;
  }

  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(QuicDispatcherPeer::GetFirstSessionIfAny(dispatcher), nullptr);
  server_thread_->Resume();

  CreateClientWithWriter();  // Also connects.
  EXPECT_NE(client_, nullptr);

  server_thread_->Pause();
  EXPECT_NE(QuicDispatcherPeer::GetFirstSessionIfAny(dispatcher), nullptr);
  EXPECT_EQ(dispatcher->NumSessions(), 1);
  auto ids = GetServerConnection()->GetActiveServerConnectionIds();
  ASSERT_EQ(ids.size(), 2);
  for (QuicConnectionId id : ids) {
    EXPECT_NE(QuicDispatcherPeer::FindSession(dispatcher, id), nullptr);
  }
  QuicConnectionId original = ids[1];
  server_thread_->Resume();

  client_->SendSynchronousRequest("/foo");
  client_->Disconnect();

  server_thread_->Pause();
  EXPECT_EQ(QuicDispatcherPeer::GetFirstSessionIfAny(dispatcher), nullptr);
  EXPECT_EQ(QuicDispatcherPeer::FindSession(dispatcher, original), nullptr);
  server_thread_->Resume();
}

TEST_P(EndToEndTest, FlowLabelSend) {
  ASSERT_TRUE(Initialize());

  const uint32_t server_flow_label = 2;
  absl::Notification set;
  server_thread_->Schedule([this, &set]() {
    QuicConnection* server_connection = GetServerConnection();
    if (server_connection != nullptr) {
      server_connection->set_outgoing_flow_label(server_flow_label);
    } else {
      ADD_FAILURE() << "Missing server connection";
    }
    set.Notify();
  });
  set.WaitForNotification();

  const uint32_t client_flow_label = 1;
  QuicConnection* client_connection = GetClientConnection();
  client_connection->set_outgoing_flow_label(client_flow_label);

  client_->SendSynchronousRequest("/foo");

  if (server_address_.host().IsIPv6()) {
    EXPECT_EQ(client_flow_label, client_connection->outgoing_flow_label());
    EXPECT_EQ(server_flow_label, client_connection->last_received_flow_label());

    server_thread_->Pause();
    QuicConnection* server_connection = GetServerConnection();
    EXPECT_EQ(server_flow_label, server_connection->outgoing_flow_label());
    EXPECT_EQ(client_flow_label, server_connection->last_received_flow_label());
  }
}

TEST_P(EndToEndTest, ServerReportsNotEct) {
  // Client connects using not-ECT.
  ASSERT_TRUE(Initialize());
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionPeer::DisableEcnCodepointValidation(client_connection);
  QuicEcnCounts* ecn = QuicSentPacketManagerPeer::GetPeerEcnCounts(
      QuicConnectionPeer::GetSentPacketManager(client_connection),
      APPLICATION_DATA);
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ect1, 0);
  EXPECT_EQ(ecn->ce, 0);
  EXPECT_TRUE(client_connection->set_ecn_codepoint(ECN_NOT_ECT));
  client_->SendSynchronousRequest("/foo");
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ect1, 0);
  EXPECT_EQ(ecn->ce, 0);
  client_->Disconnect();
}

TEST_P(EndToEndTest, ServerReportsEct0) {
  // Client connects using not-ECT.
  ASSERT_TRUE(Initialize());
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionPeer::DisableEcnCodepointValidation(client_connection);
  QuicEcnCounts* ecn = QuicSentPacketManagerPeer::GetPeerEcnCounts(
      QuicConnectionPeer::GetSentPacketManager(client_connection),
      APPLICATION_DATA);
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ect1, 0);
  EXPECT_EQ(ecn->ce, 0);
  EXPECT_TRUE(client_connection->set_ecn_codepoint(ECN_ECT0));
  client_->SendSynchronousRequest("/foo");
  if (!VersionHasIetfQuicFrames(version_.transport_version)) {
    EXPECT_EQ(ecn->ect0, 0);
  } else {
    EXPECT_GT(ecn->ect0, 0);
  }
  EXPECT_EQ(ecn->ect1, 0);
  EXPECT_EQ(ecn->ce, 0);
  client_->Disconnect();
}

TEST_P(EndToEndTest, ServerReportsEct1) {
  // Client connects using not-ECT.
  ASSERT_TRUE(Initialize());
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionPeer::DisableEcnCodepointValidation(client_connection);
  QuicEcnCounts* ecn = QuicSentPacketManagerPeer::GetPeerEcnCounts(
      QuicConnectionPeer::GetSentPacketManager(client_connection),
      APPLICATION_DATA);
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ect1, 0);
  EXPECT_EQ(ecn->ce, 0);
  EXPECT_TRUE(client_connection->set_ecn_codepoint(ECN_ECT1));
  client_->SendSynchronousRequest("/foo");
  if (!VersionHasIetfQuicFrames(version_.transport_version)) {
    EXPECT_EQ(ecn->ect1, 0);
  } else {
    EXPECT_GT(ecn->ect1, 0);
  }
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ce, 0);
  client_->Disconnect();
}

TEST_P(EndToEndTest, ServerReportsCe) {
  // Client connects using not-ECT.
  ASSERT_TRUE(Initialize());
  QuicConnection* client_connection = GetClientConnection();
  QuicConnectionPeer::DisableEcnCodepointValidation(client_connection);
  QuicEcnCounts* ecn = QuicSentPacketManagerPeer::GetPeerEcnCounts(
      QuicConnectionPeer::GetSentPacketManager(client_connection),
      APPLICATION_DATA);
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ect1, 0);
  EXPECT_EQ(ecn->ce, 0);
  EXPECT_TRUE(client_connection->set_ecn_codepoint(ECN_CE));
  client_->SendSynchronousRequest("/foo");
  if (!VersionHasIetfQuicFrames(version_.transport_version)) {
    EXPECT_EQ(ecn->ce, 0);
  } else {
    EXPECT_GT(ecn->ce, 0);
  }
  EXPECT_EQ(ecn->ect0, 0);
  EXPECT_EQ(ecn->ect1, 0);
  client_->Disconnect();
}

TEST_P(EndToEndTest, ClientReportsEct1) {
  ASSERT_TRUE(Initialize());
  // Wait for handshake to complete, so that we can manipulate the server
  // connection without race conditions.
  server_thread_->WaitForCryptoHandshakeConfirmed();
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  QuicConnectionPeer::DisableEcnCodepointValidation(server_connection);
  QuicEcnCounts* ecn = QuicSentPacketManagerPeer::GetPeerEcnCounts(
      QuicConnectionPeer::GetSentPacketManager(server_connection),
      APPLICATION_DATA);
  EXPECT_TRUE(server_connection->set_ecn_codepoint(ECN_ECT1));
  server_thread_->Resume();
  client_->SendSynchronousRequest("/foo");
  // A second request provides a packet for the client ACKs to go with.
  client_->SendSynchronousRequest("/foo");

  server_thread_->ScheduleAndWaitForCompletion([&] {
    EXPECT_EQ(ecn->ce, 0);
    if (!VersionHasIetfQuicFrames(version_.transport_version)) {
      EXPECT_EQ(ecn->ect1, 0);
    } else {
      EXPECT_GT(ecn->ect1, 0);
    }
  });

  client_->Disconnect();
}

TEST_P(EndToEndTest, FixTimeouts) {
  client_extra_copts_.push_back(kFTOE);
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls()) {
    return;
  }
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  // Verify handshake timeout has been removed on both endpoints.
  QuicConnection* client_connection = GetClientConnection();
  EXPECT_EQ(QuicConnectionPeer::GetIdleNetworkDetector(client_connection)
                .handshake_timeout(),
            QuicTime::Delta::Infinite());
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  EXPECT_EQ(QuicConnectionPeer::GetIdleNetworkDetector(server_connection)
                .handshake_timeout(),
            QuicTime::Delta::Infinite());
  server_thread_->Resume();
}

TEST_P(EndToEndTest, ClientMigrationAfterHalfwayServerMigration) {
  use_preferred_address_ = true;
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  EXPECT_EQ(server_address_, client_connection->effective_peer_address());
  EXPECT_EQ(server_address_, client_connection->peer_address());
  EXPECT_TRUE(client_->client()->HasPendingPathValidation());
  QuicConnectionId server_cid1 = client_connection->connection_id();

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_TRUE(client_->WaitUntil(
      1000, [&]() { return !client_->client()->HasPendingPathValidation(); }));
  EXPECT_EQ(server_preferred_address_,
            client_connection->effective_peer_address());
  EXPECT_EQ(server_preferred_address_, client_connection->peer_address());
  EXPECT_NE(server_cid1, client_connection->connection_id());
  EXPECT_EQ(0u,
            client_connection->GetStats().num_connectivity_probing_received);
  const auto client_stats = GetClientConnection()->GetStats();
  EXPECT_TRUE(client_stats.server_preferred_address_validated);
  EXPECT_FALSE(client_stats.failed_to_validate_server_preferred_address);

  WaitForNewConnectionIds();
  // Migrate socket to a new IP address.
  QuicIpAddress host = TestLoopback(2);
  ASSERT_NE(
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      host);
  ASSERT_TRUE(client_->client()->ValidateAndMigrateSocket(host));
  EXPECT_TRUE(client_->WaitUntil(
      1000, [&]() { return !client_->client()->HasPendingPathValidation(); }));
  EXPECT_EQ(host, client_->client()->session()->self_address().host());

  SendSynchronousBarRequestAndCheckResponse();

  // Wait for the PATH_CHALLENGE.
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return client_connection->GetStats().num_connectivity_probing_received >= 1;
  }));

  // Send another request to ensure that the server will have time to finish the
  // reverse path validation and send address token.
  SendSynchronousBarRequestAndCheckResponse();
  // By the time the above request is completed, the PATH_RESPONSE must have
  // been received by the server. Check server stats.
  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  EXPECT_FALSE(server_connection->HasPendingPathValidation());
  EXPECT_EQ(2u, server_connection->GetStats().num_validated_peer_migration);
  EXPECT_EQ(2u, server_connection->GetStats().num_new_connection_id_sent);
  server_thread_->Resume();
}

TEST_P(EndToEndTest, MultiPortCreationFollowingServerMigration) {
  use_preferred_address_ = true;
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  client_config_.SetClientConnectionOptions(QuicTagVector{kMPQC});
  client_.reset(EndToEndTest::CreateQuicClient(nullptr));
  QuicConnection* client_connection = GetClientConnection();
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  EXPECT_EQ(server_address_, client_connection->effective_peer_address());
  EXPECT_EQ(server_address_, client_connection->peer_address());
  QuicConnectionId server_cid1 = client_connection->connection_id();
  EXPECT_TRUE(client_connection->IsValidatingServerPreferredAddress());

  SendSynchronousFooRequestAndCheckResponse();
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return !client_connection->IsValidatingServerPreferredAddress();
  }));
  EXPECT_EQ(server_preferred_address_,
            client_connection->effective_peer_address());
  EXPECT_EQ(server_preferred_address_, client_connection->peer_address());
  const auto client_stats = GetClientConnection()->GetStats();
  EXPECT_TRUE(client_stats.server_preferred_address_validated);
  EXPECT_FALSE(client_stats.failed_to_validate_server_preferred_address);

  QuicConnectionId server_cid2 = client_connection->connection_id();
  EXPECT_NE(server_cid1, server_cid2);
  EXPECT_TRUE(client_->WaitUntil(1000, [&]() {
    return client_connection->GetStats().num_path_response_received == 2;
  }));
  EXPECT_TRUE(
      QuicConnectionPeer::IsAlternativePathValidated(client_connection));
  QuicConnectionId server_cid3 =
      QuicConnectionPeer::GetServerConnectionIdOnAlternativePath(
          client_connection);
  EXPECT_NE(server_cid2, server_cid3);
  EXPECT_NE(server_cid1, server_cid3);
}

TEST_P(EndToEndTest, DoNotAdvertisePreferredAddressWithoutSPAD) {
  if (!version_.HasIetfQuicFrames()) {
    ASSERT_TRUE(Initialize());
    return;
  }
  server_config_.SetIPv4AlternateServerAddressToSend(
      QuicSocketAddress(QuicIpAddress::Any4(), 12345));
  server_config_.SetIPv6AlternateServerAddressToSend(
      QuicSocketAddress(QuicIpAddress::Any6(), 12345));
  NiceMock<MockQuicConnectionDebugVisitor> visitor;
  connection_debug_visitor_ = &visitor;
  EXPECT_CALL(visitor, OnTransportParametersReceived(_))
      .WillOnce([](const TransportParameters& transport_parameters) {
        EXPECT_EQ(nullptr, transport_parameters.preferred_address);
      });
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
}

TEST_P(EndToEndTest, MaxPacingRate) {
  const std::string huge_response(10 * 1024 * 1024, 'a');  // 10 MB
  ASSERT_TRUE(Initialize());

  if (!GetQuicReloadableFlag(quic_pacing_remove_non_initial_burst)) {
    return;
  }

  AddToCache("/10MB_response", 200, huge_response);

  ASSERT_TRUE(client_->client()->WaitForHandshakeConfirmed());

  auto set_server_max_pacing_rate = [&](QuicBandwidth max_pacing_rate) {
    QuicSpdySession* server_session = GetServerSession();
    ASSERT_NE(server_session, nullptr);
    server_session->connection()->SetMaxPacingRate(max_pacing_rate);
  };

  // Set up the first response to be paced at 2 MB/s.
  server_thread_->ScheduleAndWaitForCompletion([&]() {
    set_server_max_pacing_rate(
        QuicBandwidth::FromBytesPerSecond(2 * 1024 * 1024));
  });

  QuicTime start = QuicDefaultClock::Get()->Now();
  SendSynchronousRequestAndCheckResponse(client_.get(), "/10MB_response",
                                         huge_response);
  QuicTime::Delta duration = QuicDefaultClock::Get()->Now() - start;
  QUIC_LOG(INFO) << "Response 1 duration: " << duration;
  EXPECT_GE(duration, QuicTime::Delta::FromMilliseconds(5000));
  EXPECT_LE(duration, QuicTime::Delta::FromMilliseconds(7500));

  // Set up the second response to be paced at 512 KB/s.
  server_thread_->ScheduleAndWaitForCompletion([&]() {
    set_server_max_pacing_rate(QuicBandwidth::FromBytesPerSecond(512 * 1024));
  });

  start = QuicDefaultClock::Get()->Now();
  SendSynchronousRequestAndCheckResponse(client_.get(), "/10MB_response",
                                         huge_response);
  duration = QuicDefaultClock::Get()->Now() - start;
  QUIC_LOG(INFO) << "Response 2 duration: " << duration;
  EXPECT_GE(duration, QuicTime::Delta::FromSeconds(20));
  EXPECT_LE(duration, QuicTime::Delta::FromSeconds(25));
}

TEST_P(EndToEndTest, RequestsBurstMitigation) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  // Send 50 requests simutanuously and wait for their responses. Hopefully at
  // least more than 5 of these requests will arrive at the server in the same
  // event loop and cause some of them to be pending till the next loop.
  for (int i = 0; i < 50; ++i) {
    EXPECT_LT(0, client_->SendRequest("/foo"));
  }

  while (50 > client_->num_responses()) {
    client_->ClearPerRequestState();
    client_->WaitForResponse();
    CheckResponseHeaders(client_.get());
  }
  EXPECT_TRUE(client_->connected());

  server_thread_->Pause();
  QuicConnection* server_connection = GetServerConnection();
  if (server_connection != nullptr) {
    const QuicConnectionStats& server_stats = server_connection->GetStats();
    EXPECT_LT(0u, server_stats.num_total_pending_streams);
  } else {
    ADD_FAILURE() << "Missing server connection";
  }
  server_thread_->Resume();
}

TEST_P(EndToEndTest, SerializeConnectionClosePacketWithLargestPacketNumber) {
  ASSERT_TRUE(Initialize());
  if (!version_.UsesTls()) {
    return;
  }
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());

  std::unique_ptr<SerializedPacket> connection_close_packet =
      GetClientConnection()->SerializeLargePacketNumberConnectionClosePacket(
          QUIC_CLIENT_LOST_NETWORK_ACCESS, "EndToEndTest");
  ASSERT_NE(connection_close_packet, nullptr);

  // Send 50 requests to increase the packet number.
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(kFooResponseBody, client_->SendSynchronousRequest("/foo"));
  }

  server_thread_->Pause();
  QuicDispatcher* dispatcher =
      QuicServerPeer::GetDispatcher(server_thread_->server());
  EXPECT_EQ(dispatcher->NumSessions(), 1);
  server_thread_->Resume();

  // Send the connection close packet to the server.
  QUIC_LOG(INFO) << "Sending close connection packet";
  client_writer_->WritePacket(
      connection_close_packet->encrypted_buffer,
      connection_close_packet->encrypted_length,
      client_->client()->network_helper()->GetLatestClientAddress().host(),
      server_address_, nullptr, packet_writer_params_);

  // Wait for the server to close the connection.
  EXPECT_TRUE(
      server_thread_->WaitUntil([&] { return dispatcher->NumSessions() == 0; },
                                QuicTime::Delta::FromSeconds(5)));

  EXPECT_EQ("", client_->SendSynchronousRequest("/foo"));
  EXPECT_THAT(client_->connection_error(), IsError(QUIC_PUBLIC_RESET));
}

TEST_P(EndToEndTest, EmptyResponseWithFin) {
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }
  memory_cache_backend_.AddSpecialResponse(
      server_hostname_, "/empty_response_with_fin",
      QuicBackendResponse::EMPTY_PAYLOAD_WITH_FIN);

  quiche::HttpHeaderBlock headers;
  headers[":scheme"] = "https";
  headers[":authority"] = server_hostname_;
  headers[":method"] = "GET";
  headers[":path"] = "/empty_response_with_fin";
  client_->SendMessage(headers, "", /*fin=*/true);
  client_->WaitForResponseForMs(100);
  if (GetQuicReloadableFlag(quic_fin_before_completed_http_headers)) {
    EXPECT_THAT(client_->connection_error(),
                IsError(QUIC_HTTP_INVALID_FRAME_SEQUENCE_ON_SPDY_STREAM));
  } else {
    EXPECT_FALSE(client_->response_headers_complete());
    EXPECT_FALSE(client_->response_complete());
  }
}

TEST_P(EndToEndTest, PragueConnectionOptionSent) {
  client_extra_copts_.push_back(kPRGC);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  server_thread_->Pause();
  QuicSession* session = GetServerSession();
  // Check the server received the copt.
  ASSERT_TRUE(session->config()->HasReceivedConnectionOptions());
  bool found_prgc = false;
  for (auto it : session->config()->ReceivedConnectionOptions()) {
    if (it == kPRGC) {
      found_prgc = true;
      break;
    }
  }
  server_thread_->Resume();
  EXPECT_TRUE(found_prgc);
  // Sent connection option does not select the congestion control.
  EXPECT_EQ(GetClientConnection()->ecn_codepoint(), ECN_NOT_ECT);
}

TEST_P(EndToEndTest, CubicConnectionOptionSent) {
  client_extra_copts_.push_back(kCQBC);
  ASSERT_TRUE(Initialize());
  EXPECT_TRUE(client_->client()->WaitForHandshakeConfirmed());
  server_thread_->Pause();
  QuicSession* session = GetServerSession();
  // Check the server received the copt.
  ASSERT_TRUE(session->config()->HasReceivedConnectionOptions());
  bool found_cqbc = false;
  for (auto it : session->config()->ReceivedConnectionOptions()) {
    if (it == kCQBC) {
      found_cqbc = true;
      break;
    }
  }
  server_thread_->Resume();
  EXPECT_TRUE(found_cqbc);
  // Sent connection option does not select the congestion control.
  EXPECT_EQ(GetClientConnection()->ecn_codepoint(), ECN_NOT_ECT);
}

TEST_P(EndToEndTest, ChangeFlowLabelOnRTO) {
  SetQuicReloadableFlag(quic_allow_flow_label_blackhole_avoidance_on_server,
                        true);
  client_extra_copts_.push_back(kCFLS);
  server_address_ =
      QuicSocketAddress(QuicIpAddress::Loopback6(), server_address_.port());
  ASSERT_TRUE(Initialize());
  if (!version_.HasIetfQuicFrames()) {
    return;
  }

  // Block the client until the server changes its flow label on RTO.
  EXPECT_TRUE(server_thread_->WaitUntil(
      [&]() {
        QuicConnection* server_connection = GetServerConnection();
        if (server_connection == nullptr) {
          return false;
        }
        QuicConnectionStats server_stats = server_connection->GetStats();
        EXPECT_TRUE(
            server_connection->enable_black_hole_avoidance_via_flow_label());
        EXPECT_TRUE(server_stats.num_flow_label_changes == 0 ||
                    server_stats.pto_count > 0);
        return server_connection->GetStats().num_flow_label_changes > 0;
      },
      QuicTime::Delta::FromSeconds(5)));

  client_->Disconnect();
}

}  // namespace
}  // namespace test
}  // namespace quic
