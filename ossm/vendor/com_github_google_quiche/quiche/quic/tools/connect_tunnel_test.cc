// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/tools/connect_tunnel.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/connecting_client_socket.h"
#include "quiche/quic/core/quic_connection_id.h"
#include "quiche/quic/core/quic_error_codes.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/core/socket_factory.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/platform/api/quic_test_loopback.h"
#include "quiche/quic/test_tools/quic_test_utils.h"
#include "quiche/quic/tools/quic_backend_response.h"
#include "quiche/quic/tools/quic_simple_server_backend.h"
#include "quiche/common/http/http_header_block.h"
#include "quiche/common/platform/api/quiche_mem_slice.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quic::test {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;

class MockRequestHandler : public QuicSimpleServerBackend::RequestHandler {
 public:
  QuicConnectionId connection_id() const override {
    return TestConnectionId(41212);
  }
  QuicStreamId stream_id() const override { return 100; }
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
              (const quic::QuicSocketAddress& peer_address,
               QuicByteCount receive_buffer_size,
               QuicByteCount send_buffer_size,
               ConnectingClientSocket::AsyncVisitor* async_visitor),
              (override));
  MOCK_METHOD(std::unique_ptr<ConnectingClientSocket>,
              CreateConnectingUdpClientSocket,
              (const quic::QuicSocketAddress& peer_address,
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

class ConnectTunnelTest : public quiche::test::QuicheTest {
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
            CreateTcpClientSocket(
                AnyOf(QuicSocketAddress(TestLoopback4(), kAcceptablePort),
                      QuicSocketAddress(TestLoopback6(), kAcceptablePort)),
                _, _, &tunnel_))
        .WillByDefault(Return(ByMove(std::move(socket))));
  }

 protected:
  static constexpr absl::string_view kAcceptableDestination = "localhost";
  static constexpr uint16_t kAcceptablePort = 977;

  StrictMock<MockRequestHandler> request_handler_;
  NiceMock<MockSocketFactory> socket_factory_;
  StrictMock<MockSocket>* socket_;

  ConnectTunnel tunnel_{
      &request_handler_,
      &socket_factory_,
      /*acceptable_destinations=*/
      {{std::string(kAcceptableDestination), kAcceptablePort},
       {TestLoopback4().ToString(), kAcceptablePort},
       {absl::StrCat("[", TestLoopback6().ToString(), "]"), kAcceptablePort}}};
};

TEST_F(ConnectTunnelTest, OpenTunnel) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  quiche::HttpHeaderBlock expected_response_headers;
  expected_response_headers[":status"] = "200";
  QuicBackendResponse expected_response;
  expected_response.set_headers(std::move(expected_response_headers));
  expected_response.set_response_type(QuicBackendResponse::INCOMPLETE_RESPONSE);
  EXPECT_CALL(request_handler_,
              OnResponseBackendComplete(
                  AllOf(Property(&QuicBackendResponse::response_type,
                                 QuicBackendResponse::INCOMPLETE_RESPONSE),
                        Property(&QuicBackendResponse::headers,
                                 ElementsAre(Pair(":status", "200"))),
                        Property(&QuicBackendResponse::trailers, IsEmpty()),
                        Property(&QuicBackendResponse::body, IsEmpty()))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] =
      absl::StrCat(kAcceptableDestination, ":", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);
  EXPECT_TRUE(tunnel_.IsConnectedToDestination());
  tunnel_.OnClientStreamClose();
  EXPECT_FALSE(tunnel_.IsConnectedToDestination());
}

TEST_F(ConnectTunnelTest, OpenTunnelToIpv4LiteralDestination) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  quiche::HttpHeaderBlock expected_response_headers;
  expected_response_headers[":status"] = "200";
  QuicBackendResponse expected_response;
  expected_response.set_headers(std::move(expected_response_headers));
  expected_response.set_response_type(QuicBackendResponse::INCOMPLETE_RESPONSE);
  EXPECT_CALL(request_handler_,
              OnResponseBackendComplete(
                  AllOf(Property(&QuicBackendResponse::response_type,
                                 QuicBackendResponse::INCOMPLETE_RESPONSE),
                        Property(&QuicBackendResponse::headers,
                                 ElementsAre(Pair(":status", "200"))),
                        Property(&QuicBackendResponse::trailers, IsEmpty()),
                        Property(&QuicBackendResponse::body, IsEmpty()))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] =
      absl::StrCat(TestLoopback4().ToString(), ":", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);
  EXPECT_TRUE(tunnel_.IsConnectedToDestination());
  tunnel_.OnClientStreamClose();
  EXPECT_FALSE(tunnel_.IsConnectedToDestination());
}

TEST_F(ConnectTunnelTest, OpenTunnelToIpv6LiteralDestination) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  quiche::HttpHeaderBlock expected_response_headers;
  expected_response_headers[":status"] = "200";
  QuicBackendResponse expected_response;
  expected_response.set_headers(std::move(expected_response_headers));
  expected_response.set_response_type(QuicBackendResponse::INCOMPLETE_RESPONSE);
  EXPECT_CALL(request_handler_,
              OnResponseBackendComplete(
                  AllOf(Property(&QuicBackendResponse::response_type,
                                 QuicBackendResponse::INCOMPLETE_RESPONSE),
                        Property(&QuicBackendResponse::headers,
                                 ElementsAre(Pair(":status", "200"))),
                        Property(&QuicBackendResponse::trailers, IsEmpty()),
                        Property(&QuicBackendResponse::body, IsEmpty()))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] =
      absl::StrCat("[", TestLoopback6().ToString(), "]:", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);
  EXPECT_TRUE(tunnel_.IsConnectedToDestination());
  tunnel_.OnClientStreamClose();
  EXPECT_FALSE(tunnel_.IsConnectedToDestination());
}

TEST_F(ConnectTunnelTest, OpenTunnelWithMalformedRequest) {
  EXPECT_CALL(request_handler_,
              TerminateStreamWithError(Property(
                  &QuicResetStreamError::ietf_application_code,
                  static_cast<uint64_t>(QuicHttp3ErrorCode::MESSAGE_ERROR))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  // No ":authority" header.

  tunnel_.OpenTunnel(request_headers);
  EXPECT_FALSE(tunnel_.IsConnectedToDestination());
  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectTunnelTest, OpenTunnelWithUnacceptableDestination) {
  EXPECT_CALL(
      request_handler_,
      TerminateStreamWithError(Property(
          &QuicResetStreamError::ietf_application_code,
          static_cast<uint64_t>(QuicHttp3ErrorCode::REQUEST_REJECTED))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] = "unacceptable.test:100";

  tunnel_.OpenTunnel(request_headers);
  EXPECT_FALSE(tunnel_.IsConnectedToDestination());
  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectTunnelTest, ReceiveFromDestination) {
  static constexpr absl::string_view kData = "\x11\x22\x33\x44\x55";

  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Ge(kData.size()))).Times(2);
  EXPECT_CALL(*socket_, Disconnect()).WillOnce(InvokeWithoutArgs([this]() {
    tunnel_.ReceiveComplete(absl::CancelledError());
  }));

  EXPECT_CALL(request_handler_, OnResponseBackendComplete(_));

  EXPECT_CALL(request_handler_, SendStreamData(kData, /*close_stream=*/false));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] =
      absl::StrCat(kAcceptableDestination, ":", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);

  // Simulate receiving `kData`.
  tunnel_.ReceiveComplete(MemSliceFromString(kData));

  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectTunnelTest, SendToDestination) {
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
  request_headers[":authority"] =
      absl::StrCat(kAcceptableDestination, ":", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);
  tunnel_.SendDataToDestination(kData);
  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectTunnelTest, DestinationDisconnect) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect());

  EXPECT_CALL(request_handler_, OnResponseBackendComplete(_));
  EXPECT_CALL(request_handler_, SendStreamData("", /*close_stream=*/true));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] =
      absl::StrCat(kAcceptableDestination, ":", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);

  // Simulate receiving empty data.
  tunnel_.ReceiveComplete(quiche::QuicheMemSlice());

  EXPECT_FALSE(tunnel_.IsConnectedToDestination());

  tunnel_.OnClientStreamClose();
}

TEST_F(ConnectTunnelTest, DestinationTcpConnectionError) {
  EXPECT_CALL(*socket_, ConnectBlocking()).WillOnce(Return(absl::OkStatus()));
  EXPECT_CALL(*socket_, ReceiveAsync(Gt(0)));
  EXPECT_CALL(*socket_, Disconnect());

  EXPECT_CALL(request_handler_, OnResponseBackendComplete(_));
  EXPECT_CALL(request_handler_,
              TerminateStreamWithError(Property(
                  &QuicResetStreamError::ietf_application_code,
                  static_cast<uint64_t>(QuicHttp3ErrorCode::CONNECT_ERROR))));

  quiche::HttpHeaderBlock request_headers;
  request_headers[":method"] = "CONNECT";
  request_headers[":authority"] =
      absl::StrCat(kAcceptableDestination, ":", kAcceptablePort);

  tunnel_.OpenTunnel(request_headers);

  // Simulate receving error.
  tunnel_.ReceiveComplete(absl::UnknownError("error"));

  tunnel_.OnClientStreamClose();
}

}  // namespace
}  // namespace quic::test
