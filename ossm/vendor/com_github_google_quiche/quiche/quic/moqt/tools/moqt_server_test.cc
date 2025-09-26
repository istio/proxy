// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/tools/moqt_server.h"

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
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
#include "quiche/web_transport/test_tools/mock_web_transport.h"

namespace moqt::test {

class MoqtServerPeer {
 public:
  static quic::WebTransportOnlyBackend* backend(MoqtServer& server) {
    return &server.backend_;
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
    EXPECT_TRUE(server_.quic_server().CreateUDPSocketAndListen(
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
  quic::WebTransportOnlyBackend::WebTransportResponse response =
      MoqtServerPeer::backend(server_)->ProcessWebTransportRequest(
          headers, &mock_session_);
  ASSERT_NE(session_, nullptr);
  ASSERT_NE(MoqtSessionPeer::GetAlarmFactory(session_), nullptr);
  auto delegate = new MockAlarmDelegate();
  auto alarm = absl::WrapUnique(
      MoqtSessionPeer::GetAlarmFactory(session_)->CreateAlarm(delegate));
  alarm->Set(quic::QuicTime::Infinite());
  EXPECT_TRUE(alarm->IsSet());
}

}  // namespace moqt::test
