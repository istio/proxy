// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/tools/moqt_server.h"

#include <utility>

#include "absl/base/nullability.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/http/web_transport_only_server_session.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_session.h"
#include "quiche/quic/moqt/test_tools/moqt_session_peer.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/crypto_test_utils.h"
#include "quiche/quic/tools/web_transport_only_backend.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/quiche_ip_address.h"
#include "quiche/common/test_tools/quiche_test_utils.h"
#include "quiche/web_transport/test_tools/mock_web_transport.h"
#include "quiche/web_transport/web_transport.h"

namespace moqt::test {

class MoqtServerPeer {
 public:
  static absl::StatusOr<quic::WebTransportConnectResponse> CallHandlerFactory(
      MoqtServer& server, webtransport::Session* session,
      const quic::WebTransportIncomingRequestDetails& details) {
    return server.dispatcher_.parameters().handler_factory(session, details);
  }
};

class MockAlarmDelegate : public quic::QuicAlarm::DelegateWithoutContext {
 public:
  MOCK_METHOD(void, OnAlarm, (), (override));
};

class MoqtServerTest : public quic::test::QuicTest {
 public:
  MoqtServerTest()
      : server_(quic::test::crypto_test_utils::ProofSourceForTesting(),
                [&](absl::string_view /*path*/) {
                  return [&](MoqtSession* session) { session_ = session; };
                }) {
    quiche::QuicheIpAddress bind_address;
    bind_address.FromString("127.0.0.1");
    // This will create an event loop that makes alarm factories.
    QUICHE_EXPECT_OK(server_.CreateUDPSocketAndListen(
        quic::QuicSocketAddress(bind_address, 0)));
  }

  MoqtServer server_;
  MoqtSession* session_ = nullptr;
  webtransport::test::MockSession mock_session_;
};

// Test that new sessions are correctly populated with an alarm factory.
TEST_F(MoqtServerTest, NewSessionHasAlarmFactory) {
  quiche::HttpHeaderBlock headers;
  headers.AppendValueOrAddHeader(":path", "/foo");
  absl::StatusOr<quic::WebTransportConnectResponse> response =
      MoqtServerPeer::CallHandlerFactory(
          server_, &mock_session_,
          quic::WebTransportIncomingRequestDetails{.headers =
                                                       std::move(headers)});
  QUICHE_EXPECT_OK(response.status());
  ASSERT_NE(session_, nullptr);
  ASSERT_NE(MoqtSessionPeer::GetAlarmFactory(session_), nullptr);
  auto delegate = new MockAlarmDelegate();
  auto alarm = absl::WrapUnique(
      MoqtSessionPeer::GetAlarmFactory(session_)->CreateAlarm(delegate));
  alarm->Set(quic::QuicTime::Infinite());
  EXPECT_TRUE(alarm->IsSet());
}

}  // namespace moqt::test
