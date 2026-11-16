// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end test for MoqtClient/MoqtServer.
//
// IMPORTANT NOTE:
// This test mostly exists to test the two classes mentioned above. When
// possible, moqt_integration_test should be used instead, as it does not use
// real clocks or I/O and thus has less overhead.

#include <memory>
#include <string>
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/io/quic_event_loop.h"
#include "quiche/quic/core/quic_server_id.h"
#include "quiche/quic/moqt/moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/moqt/tools/moqt_client.h"
#include "quiche/quic/moqt/tools/moqt_server.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/tools/quic_event_loop_tools.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_callbacks.h"

namespace moqt::test {
namespace {

constexpr absl::string_view kNotFoundPath = "/not-found";

void UnexpectedClose(absl::string_view reason) {
  ADD_FAILURE() << "Unexpected close of MoQT session with reason: " << reason;
}

class MoqtEndToEndTest : public quiche::test::QuicheTest {
 public:
  MoqtEndToEndTest()
      : server_(quic::test::crypto_test_utils::ProofSourceForTesting(),
                absl::bind_front(&MoqtEndToEndTest::ServerBackend, this)) {
    quic::QuicIpAddress host = quic::TestLoopback();
    absl::Status socket_status = server_.CreateUDPSocketAndListen(
        quic::QuicSocketAddress(host, /*port=*/0));
    QUICHE_CHECK_OK(socket_status);
    server_address_ = quic::QuicSocketAddress(host, server_.port());
    event_loop_ = server_.event_loop();
  }

  absl::StatusOr<MoqtConfigureSessionCallback> ServerBackend(
      absl::string_view path) {
    QUICHE_LOG(INFO) << "Server: Received a request for path " << path;
    if (path == kNotFoundPath) {
      return absl::NotFoundError("404 test endpoint");
    }
    return [](MoqtSession* session) {
      session->callbacks().session_established_callback = []() {
        QUICHE_LOG(INFO) << "Server: session established";
      };
      session->callbacks().session_terminated_callback =
          [](absl::string_view reason) {
            QUICHE_LOG(INFO)
                << "Server: session terminated with reason: " << reason;
          };
    };
  }

  std::unique_ptr<MoqtClient> CreateClient() {
    return std::make_unique<MoqtClient>(
        server_address_, quic::QuicServerId("test.example.com", 443),
        quic::test::crypto_test_utils::ProofVerifierForTesting(), event_loop_);
  }

  bool RunEventsUntil(quiche::UnretainedCallback<bool()> callback) {
    return quic::ProcessEventsUntil(event_loop_, callback);
  }

 private:
  MoqtServer server_;
  quic::QuicEventLoop* event_loop_;
  quic::QuicSocketAddress server_address_;
};

TEST_F(MoqtEndToEndTest, SuccessfulHandshake) {
  MoqtSessionCallbacks callbacks;
  bool established = false;
  bool deleted = false;
  callbacks.session_established_callback = [&] { established = true; };
  callbacks.session_terminated_callback = UnexpectedClose;
  callbacks.session_deleted_callback = [&] { deleted = true; };
  std::unique_ptr<MoqtClient> client = CreateClient();
  client->Connect("/test", std::move(callbacks));
  EXPECT_GT(client->quic_session()
                ->config()
                ->GetInitialMaxStreamDataBytesUnidirectionalToSend(),
            64 * 1024);
  EXPECT_GT(client->quic_session()
                ->config()
                ->ReceivedInitialMaxStreamDataBytesUnidirectional(),
            64 * 1024);
  bool success = RunEventsUntil([&] { return established; });
  EXPECT_TRUE(success);
  EXPECT_FALSE(deleted);
  client.reset();
  EXPECT_TRUE(deleted);
}

TEST_F(MoqtEndToEndTest, HandshakeFailed404) {
  MoqtSessionCallbacks callbacks;
  bool resolved = false;
  callbacks.session_established_callback = [&] {
    ADD_FAILURE() << "Established session when 404 expected";
    resolved = true;
  };
  callbacks.session_terminated_callback = [&](absl::string_view error) {
    resolved = true;
  };
  std::unique_ptr<MoqtClient> client = CreateClient();
  client->Connect(std::string(kNotFoundPath), std::move(callbacks));
  bool success = RunEventsUntil([&] { return resolved; });
  EXPECT_TRUE(success);
}

TEST_F(MoqtEndToEndTest, CustomParametersHandshake) {
  MoqtSessionParameters server_parameters;
  server_parameters.max_undelivered_object = 0;
  MoqtSession* server_session = nullptr;
  auto server_backend = [&](absl::string_view /*path*/)
      -> absl::StatusOr<MoqtConfigureSessionCallback> {
    return [&](MoqtSession* session) {
      server_session = session;
      session->callbacks().session_terminated_callback = [](absl::string_view) {
      };
    };
  };
  MoqtServer custom_server(
      quic::test::crypto_test_utils::ProofSourceForTesting(),
      std::move(server_backend), server_parameters);
  quic::QuicIpAddress host = quic::TestLoopback();
  QUICHE_CHECK_OK(custom_server.CreateUDPSocketAndListen(
      quic::QuicSocketAddress(host, /*port=*/0)));
  quic::QuicSocketAddress custom_server_address(host, custom_server.port());

  MoqtSessionParameters client_parameters;
  client_parameters.max_auth_token_cache_size = 200;
  MoqtClient client(custom_server_address,
                    quic::QuicServerId("test.example.com", 443),
                    quic::test::crypto_test_utils::ProofVerifierForTesting(),
                    custom_server.event_loop(), client_parameters);

  MoqtSessionCallbacks callbacks;
  bool established = false;
  callbacks.session_established_callback = [&] { established = true; };
  callbacks.session_terminated_callback = UnexpectedClose;
  client.Connect("/test", std::move(callbacks));

  bool success = quic::ProcessEventsUntil(custom_server.event_loop(), [&] {
    return established && server_session != nullptr;
  });
  EXPECT_TRUE(success);
  ASSERT_NE(client.session(), nullptr);
  EXPECT_EQ(MoqtSessionPeer::GetParameters(client.session())
                .max_auth_token_cache_size,
            200);
  ASSERT_NE(server_session, nullptr);
  EXPECT_EQ(
      MoqtSessionPeer::GetParameters(server_session).max_undelivered_object, 0);
}

}  // namespace
}  // namespace moqt::test
