// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/tools/connect_udp_tunnel.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/connecting_client_socket.h"
#include "quiche/quic/core/http/quic_spdy_stream.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/socket_factory.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/tools/quic_simple_server_backend.h"
#include "quiche/common/masque/connect_udp_datagram_payload.h"
#include "quiche/common/platform/api/quiche_googleurl.h"
#include "quiche/common/platform/api/quiche_mem_slice.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/platform/api/quiche_url_utils.h"

namespace quic::test {
namespace {

using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAre;

constexpr QuicStreamId kStreamId = 100;

class MockStream : public QuicSpdyStream {
 public:
  explicit MockStream(QuicSpdySession* spdy_session)
      : QuicSpdyStream(kStreamId, spdy_session, BIDIRECTIONAL) {}

  void OnBodyAvailable() override {}

  MOCK_METHOD(MessageStatus, SendHttp3Datagram, (absl::string_view data),
              (override));
};

class MockRequestHandler : public QuicSimpleServerBackend::RequestHandler {
 public:
  QuicConnectionId connection_id() const override {
    return TestConnectionId(41212);
  }
  QuicStreamId stream_id() const override { return kStreamId; }
  std::string peer_host() const override { return "127.0.0.1"; }

  MOCK_METHOD(QuicSpdyStream*, GetStream, (), (override));
  MOCK_METHOD(void, OnResponseBackendComplete,
              (const QuicBackendResponse* response), (override));
  MOCK_METHOD(void, SendStreamData, (absl::string_view data, bool close_stream),
              (override));
  MOCK_METHOD(void, TerminateStreamWithError, (QuicResetStreamError error),
              (override));
};

class MockSocketFactory : public SocketFactory {
 public:
  MOCK_METHOD(std::unique_ptr<ConnectingClientSocket>, CreateTcpClientSocket,
              (const QuicSocketAddress& peer_address,
               QuicByteCount receive_buffer_size,
               QuicByteCount send_buffer_size,
               ConnectingClientSocket::AsyncVisitor* async_visitor),
              (override));
  MOCK_METHOD(std::unique_ptr<ConnectingClientSocket>,
              CreateConnectingUdpClientSocket,
              (const QuicSocketAddress& peer_address,
               QuicByteCount receive_buffer_size,
               QuicByteCount send_buffer_size,
               ConnectingClientSocket::AsyncVisitor* async_visitor),
              (override));
};

class MockSocket : public ConnectingClientSocket {
 public:
  MOCK_METHOD(absl::Status, ConnectBlocking, (), (override));
  MOCK_METHOD(void, ConnectAsync, (), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(absl::StatusOr<QuicSocketAddress>, GetLocalAddress, (),
              (override));
  MOCK_METHOD(absl::StatusOr<quiche::QuicheMemSlice>, ReceiveBlocking,
              (QuicByteCount max_size), (override));
  MOCK_METHOD(void, ReceiveAsync, (QuicByteCount max_size), (override));
  MOCK_METHOD(absl::Status, SendBlocking, (std::string data), (override));
  MOCK_METHOD(absl::Status, SendBlocking, (quiche::QuicheMemSlice data),
              (override));
  MOCK_METHOD(void, SendAsync, (std::string data), (override));
  MOCK_METHOD(void, SendAsync, (quiche::QuicheMemSlice data), (override));
};

class ConnectUdpTunnelTest : public quiche::test::QuicheTest {
 public:
  void SetUp() override {
#if defined(_WIN32)
    WSADATA wsa_data;
    const WORD version_required = MAKEWORD(2, 2);
    ASSERT_EQ(WSAStartup(version_required, &wsa_data), 0);
#endif
    auto socket = std::make_unique<StrictMock<MockSocket>>();
    socket_ = socket.get();
    ON_CALL(socket_factory_,
            CreateConnectingUdpClientSocket(
                AnyOf(QuicSocketAddress(TestLoopback4(), kAcceptablePort),
                      QuicSocketAddress(TestLoopback6(), kAcceptablePort)),
                _, _, &tunnel_))
        .WillByDefault(Return(ByMove(std::move(socket))));

    EXPECT_CALL(request_handler_, GetStream()).WillRepeatedly(Return(&stream_));
  }

 protected:
  static constexpr absl::string_view kAcceptableTarget = "localhost";
  static constexpr uint16_t kAcceptablePort = 977;

  NiceMock<MockQuicConnectionHelper> connection_helper_;
  NiceMock<MockAlarmFactory> alarm_factory_;
  NiceMock<MockQuicSpdySession> session_{new NiceMock<MockQuicConnection>(
      &connection_helper_, &alarm_factory_, Perspective::IS_SERVER)};
  StrictMock<MockStream> stream_{&session_};

  StrictMock<MockRequestHandler> request_handler_;
  NiceMock<MockSocketFactory> socket_factory_;
  StrictMock<MockSocket>* socket_;

  ConnectUdpTunnel tunnel_{
      &request_handler_,
      &socket_factory_,
      "server_label",
      /*acceptable_targets=*/
      {{std::string(kAcceptableTarget), kAcceptablePort},
       {TestLoopback4().ToString(), kAcceptablePort},
       {absl::StrCat("[", TestLoopback6().ToString(), "]"), kAcceptablePort}}};
};

TEST_F(ConnectUdpTunnelTest, OpenTunnel) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  EXPECT_CALL(
      request_handler_,
      OnResponseBackendComplete(
          AllOf(Property(&QuicBackendResponse::response_type,
                         QuicBackendResponse::INCOMPLETE_RESPONSE),
                Property(&QuicBackendResponse::headers,
                         UnorderedElementsAre(Pair(":status", "200"),
                                              Pair("Capsule-Protocol", "?1"))),
                Property(&QuicBackendResponse::trailers, IsEmpty()),
                Property(&QuicBackendResponse::body, IsEmpty()))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  request_headers[":path"] = absl::StrCat(
      "/.well-known/masque/udp/", kAcceptableTarget, "/", kAcceptablePort, "/");

  tunnel_.OpenTunnel(request_headers);
  EXPECT_TRUE(tunnel_.IsTunnelOpenToTarget());
  tunnel_.OnClientStreamClose();
  EXPECT_FALSE(tunnel_.IsTunnelOpenToTarget());
}

TEST_F(ConnectUdpTunnelTest, OpenTunnelToIpv4LiteralTarget) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  EXPECT_CALL(
      request_handler_,
      OnResponseBackendComplete(
          AllOf(Property(&QuicBackendResponse::response_type,
                         QuicBackendResponse::INCOMPLETE_RESPONSE),
                Property(&QuicBackendResponse::headers,
                         UnorderedElementsAre(Pair(":status", "200"),
                                              Pair("Capsule-Protocol", "?1"))),
                Property(&QuicBackendResponse::trailers, IsEmpty()),
                Property(&QuicBackendResponse::body, IsEmpty()))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  request_headers[":path"] =
      absl::StrCat("/.well-known/masque/udp/", TestLoopback4().ToString(), "/",
                   kAcceptablePort, "/");

  tunnel_.OpenTunnel(request_headers);
  EXPECT_TRUE(tunnel_.IsTunnelOpenToTarget());
  tunnel_.OnClientStreamClose();
  EXPECT_FALSE(tunnel_.IsTunnelOpenToTarget());
}

TEST_F(ConnectUdpTunnelTest, OpenTunnelToIpv6LiteralTarget) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  EXPECT_CALL(
      request_handler_,
      OnResponseBackendComplete(
          AllOf(Property(&QuicBackendResponse::response_type,
                         QuicBackendResponse::INCOMPLETE_RESPONSE),
                Property(&QuicBackendResponse::headers,
                         UnorderedElementsAre(Pair(":status", "200"),
                                              Pair("Capsule-Protocol", "?1"))),
                Property(&QuicBackendResponse::trailers, IsEmpty()),
                Property(&QuicBackendResponse::body, IsEmpty()))));

  std::string path;
  ASSERT_TRUE(quiche::ExpandURITemplate(
      "/.well-known/masque/udp/{target_host}/{target_port}/",
      {{"target_host", absl::StrCat("[", TestLoopback6().ToString(), "]")},
       {"target_port", absl::StrCat(kAcceptablePort)}},
      &path));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  request_headers[":path"] = path;

  tunnel_.OpenTunnel(request_headers);
  EXPECT_TRUE(tunnel_.IsTunnelOpenToTarget());
  tunnel_.OnClientStreamClose();
  EXPECT_FALSE(tunnel_.IsTunnelOpenToTarget());
}

TEST_F(ConnectUdpTunnelTest, OpenTunnelWithMalformedRequest) {
  EXPECT_CALL(request_handler_,
              TerminateStreamWithError(Property(
                  &QuicResetStreamError::ietf_application_code,
                  static_cast<uint64_t>(QuicHttp3ErrorCode::MESSAGE_ERROR))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  // No ":path" header.

  tunnel_.OpenTunnel(request_headers);
  EXPECT_FALSE(tunnel_.IsTunnelOpenToTarget());
  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectUdpTunnelTest, OpenTunnelWithUnacceptableTarget) {
  EXPECT_CALL(request_handler_,
              OnResponseBackendComplete(AllOf(
                  Property(&QuicBackendResponse::response_type,
                           QuicBackendResponse::REGULAR_RESPONSE),
                  Property(&QuicBackendResponse::headers,
                           UnorderedElementsAre(
                               Pair(":status", "403"),
                               Pair("Proxy-Status",
                                    HasSubstr("destination_ip_prohibited")))),
                  Property(&QuicBackendResponse::trailers, IsEmpty()))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  request_headers[":path"] = "/.well-known/masque/udp/unacceptable.test/100/";

  tunnel_.OpenTunnel(request_headers);
  EXPECT_FALSE(tunnel_.IsTunnelOpenToTarget());
  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectUdpTunnelTest, ReceiveFromTarget) {
  static constexpr absl::string_view kData = "\x11\x22\x33\x44\x55";

  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Ge(kData.size()))).Times(2);
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  EXPECT_CALL(request_handler_, OnResponseBackendComplete(_));

  EXPECT_CALL(
      stream_,
      SendHttp3Datagram(
          quiche::ConnectUdpDatagramUdpPacketPayload(kData).Serialize()))
      .WillOnce(Return(MESSAGE_STATUS_SUCCESS));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  request_headers[":path"] = absl::StrCat(
      "/.well-known/masque/udp/", kAcceptableTarget, "/", kAcceptablePort, "/");

  tunnel_.OpenTunnel(request_headers);

  // Simulate receiving `kData`.
  tunnel_.ReceiveComplete(MemSliceFromString(kData));

  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectUdpTunnelTest, SendToTarget) {
  static constexpr absl::string_view kData = "\x11\x22\x33\x44\x55";

  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, SendBlocking(Matcher<std::string>(Eq(kData))))
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  EXPECT_CALL(request_handler_, OnResponseBackendComplete(_));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":protocol"] = "connect-udp";
  request_headers[":authority"] = "proxy.test";
  request_headers[":scheme"] = "https";
  request_headers[":path"] = absl::StrCat(
      "/.well-known/masque/udp/", kAcceptableTarget, "/", kAcceptablePort, "/");

  tunnel_.OpenTunnel(request_headers);
  tunnel_.OnHttp3Datagram(
      kStreamId, quiche::ConnectUdpDatagramUdpPacketPayload(kData).Serialize());
  tunnel_.OnClientStreamClose();
}

}  // namespace
}  // namespace quic::test
