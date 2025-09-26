// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/io/socket.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/platform/api/quic_ip_address.h"
#include "quiche/quic/platform/api/quic_ip_address_family.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/quic/test_tools/test_ip_packets.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/platform/api/quiche_test_loopback.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quic::test {
namespace {

using quiche::test::QuicheTest;
using quiche::test::StatusIs;
using testing::Lt;
using testing::SizeIs;

SocketFd CreateTestSocket(socket_api::SocketProtocol protocol,
                          bool blocking = true) {
  absl::StatusOr<SocketFd> socket = socket_api::CreateSocket(
      quiche::TestLoopback().address_family(), protocol, blocking);

  if (socket.ok()) {
    return socket.value();
  } else {
    QUICHE_CHECK(false);
    return kInvalidSocketFd;
  }
}

SocketFd CreateTestRawSocket(
    bool blocking = true,
    IpAddressFamily address_family = IpAddressFamily::IP_UNSPEC) {
  absl::StatusOr<SocketFd> socket;
  switch (address_family) {
    case IpAddressFamily::IP_V4:
      socket = socket_api::CreateSocket(
          quiche::TestLoopback4().address_family(),
          socket_api::SocketProtocol::kRawIp, blocking);
      break;
    case IpAddressFamily::IP_V6:
      socket = socket_api::CreateSocket(
          quiche::TestLoopback6().address_family(),
          socket_api::SocketProtocol::kRawIp, blocking);
      break;
    case IpAddressFamily::IP_UNSPEC:
      socket = socket_api::CreateSocket(quiche::TestLoopback().address_family(),
                                        socket_api::SocketProtocol::kRawIp,
                                        blocking);
      break;
  }

  if (socket.ok()) {
    return socket.value();
  } else {
    // This is expected if test not run with relevant admin privileges or if
    // address family is unsupported.
    QUICHE_CHECK(absl::IsPermissionDenied(socket.status()) ||
                 absl::IsNotFound(socket.status()));
    return kInvalidSocketFd;
  }
}

TEST(SocketTest, CreateAndCloseSocket) {
  QuicIpAddress localhost_address = quiche::TestLoopback();
  absl::StatusOr<SocketFd> created_socket = socket_api::CreateSocket(
      localhost_address.address_family(), socket_api::SocketProtocol::kUdp);

  QUICHE_EXPECT_OK(created_socket.status());

  QUICHE_EXPECT_OK(socket_api::Close(created_socket.value()));
}

TEST(SocketTest, CreateAndCloseRawSocket) {
  QuicIpAddress localhost_address = quiche::TestLoopback();
  absl::StatusOr<SocketFd> created_socket = socket_api::CreateSocket(
      localhost_address.address_family(), socket_api::SocketProtocol::kRawIp);

  // Raw IP socket creation will typically fail if not run with relevant admin
  // privileges.
  if (!created_socket.ok()) {
    EXPECT_THAT(created_socket.status(),
                StatusIs(absl::StatusCode::kPermissionDenied));
    return;
  }

  QUICHE_EXPECT_OK(socket_api::Close(created_socket.value()));
}

TEST(SocketTest, SetSocketBlocking) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/true);

  QUICHE_EXPECT_OK(socket_api::SetSocketBlocking(socket, /*blocking=*/false));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SetReceiveBufferSize) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/true);

  QUICHE_EXPECT_OK(socket_api::SetReceiveBufferSize(socket, /*size=*/100));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SetSendBufferSize) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/true);

  QUICHE_EXPECT_OK(socket_api::SetSendBufferSize(socket, /*size=*/100));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SetIpHeaderIncludedForRaw) {
  SocketFd socket =
      CreateTestRawSocket(/*blocking=*/true, IpAddressFamily::IP_V4);
  if (socket == kInvalidSocketFd) {
    GTEST_SKIP();
  }

  QUICHE_EXPECT_OK(socket_api::SetIpHeaderIncluded(
      socket, IpAddressFamily::IP_V4, /*ip_header_included=*/true));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SetIpHeaderIncludedForRawV6) {
  SocketFd socket =
      CreateTestRawSocket(/*blocking=*/true, IpAddressFamily::IP_V6);
  if (socket == kInvalidSocketFd) {
    GTEST_SKIP();
  }

  QUICHE_EXPECT_OK(socket_api::SetIpHeaderIncluded(
      socket, IpAddressFamily::IP_V6, /*ip_header_included=*/true));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SetIpHeaderIncludedForUdp) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/true);

  // Expect option only allowed for raw IP sockets.
  EXPECT_THAT(socket_api::SetIpHeaderIncluded(socket, IpAddressFamily::IP_V4,
                                              /*ip_header_included=*/true),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(socket_api::SetIpHeaderIncluded(socket, IpAddressFamily::IP_V6,
                                              /*ip_header_included=*/true),
              StatusIs(absl::StatusCode::kInvalidArgument));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Connect) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp);

  // UDP, so "connecting" should succeed without any listening sockets.
  QUICHE_EXPECT_OK(socket_api::Connect(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, GetSocketError) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/true);

  absl::Status error = socket_api::GetSocketError(socket);
  QUICHE_EXPECT_OK(error);

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Bind) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp);

  QUICHE_EXPECT_OK(socket_api::Bind(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, GetSocketAddress) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp);
  QUICHE_ASSERT_OK(socket_api::Bind(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  absl::StatusOr<QuicSocketAddress> address =
      socket_api::GetSocketAddress(socket);
  QUICHE_EXPECT_OK(address);
  EXPECT_TRUE(address.value().IsInitialized());
  EXPECT_EQ(address.value().host(), quiche::TestLoopback());

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Listen) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kTcp);
  QUICHE_ASSERT_OK(socket_api::Bind(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  QUICHE_EXPECT_OK(socket_api::Listen(socket, /*backlog=*/5));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Accept) {
  // Need a non-blocking socket to avoid waiting when no connection comes.
  SocketFd socket =
      CreateTestSocket(socket_api::SocketProtocol::kTcp, /*blocking=*/false);
  QUICHE_ASSERT_OK(socket_api::Bind(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));
  QUICHE_ASSERT_OK(socket_api::Listen(socket, /*backlog=*/5));

  // Nothing set up to connect, so expect kUnavailable.
  absl::StatusOr<socket_api::AcceptResult> result = socket_api::Accept(socket);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnavailable));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Receive) {
  // Non-blocking to avoid waiting when no data to receive.
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/false);

  // On Windows, recv() fails on a socket that is connectionless and not bound.
  QUICHE_ASSERT_OK(socket_api::Bind(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  std::string buffer(100, 0);
  absl::StatusOr<absl::Span<char>> result =
      socket_api::Receive(socket, absl::MakeSpan(buffer));
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnavailable));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Peek) {
  // Non-blocking to avoid waiting when no data to receive.
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp,
                                     /*blocking=*/false);

  // On Windows, recv() fails on a socket that is connectionless and not bound.
  QUICHE_ASSERT_OK(socket_api::Bind(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  std::string buffer(100, 0);
  absl::StatusOr<absl::Span<char>> result =
      socket_api::Receive(socket, absl::MakeSpan(buffer), /*peek=*/true);
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnavailable));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, Send) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp);
  // UDP, so "connecting" should succeed without any listening sockets.
  QUICHE_ASSERT_OK(socket_api::Connect(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  char buffer[] = {12, 34, 56, 78};
  // Expect at least some data to be sent successfully.
  absl::StatusOr<absl::string_view> result =
      socket_api::Send(socket, absl::string_view(buffer, sizeof(buffer)));
  QUICHE_ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), SizeIs(Lt(4)));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SendTo) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp);

  // Send data to an arbitrarily-chosen ephemeral port.
  char buffer[] = {12, 34, 56, 78};
  absl::StatusOr<absl::string_view> result = socket_api::SendTo(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/57290),
      absl::string_view(buffer, sizeof(buffer)));

  // Expect at least some data to be sent successfully.
  QUICHE_ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), SizeIs(Lt(4)));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SendToWithConnection) {
  SocketFd socket = CreateTestSocket(socket_api::SocketProtocol::kUdp);
  // UDP, so "connecting" should succeed without any listening sockets.
  QUICHE_ASSERT_OK(socket_api::Connect(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/0)));

  // Send data to an arbitrarily-chosen ephemeral port.
  char buffer[] = {12, 34, 56, 78};
  absl::StatusOr<absl::string_view> result = socket_api::SendTo(
      socket, QuicSocketAddress(quiche::TestLoopback(), /*port=*/50495),
      absl::string_view(buffer, sizeof(buffer)));
  // Expect at least some data to be sent successfully.
  QUICHE_ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), SizeIs(Lt(4)));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SendToForRaw) {
  SocketFd socket = CreateTestRawSocket(/*blocking=*/true);
  if (socket == kInvalidSocketFd) {
    GTEST_SKIP();
  }

  QuicIpAddress localhost_address = quiche::TestLoopback();
  QUICHE_EXPECT_OK(socket_api::SetIpHeaderIncluded(
      socket, localhost_address.address_family(),
      /*ip_header_included=*/false));

  // Arbitrarily-chosen ephemeral ports.
  QuicSocketAddress client_address(localhost_address, /*port=*/53368);
  QuicSocketAddress server_address(localhost_address, /*port=*/56362);
  std::string packet = CreateUdpPacket(client_address, server_address, "foo");
  absl::StatusOr<absl::string_view> result = socket_api::SendTo(
      socket, QuicSocketAddress(localhost_address, /*port=*/56362), packet);

  // Expect at least some data to be sent successfully.
  QUICHE_ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), SizeIs(Lt(packet.size())));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

TEST(SocketTest, SendToForRawWithIpHeader) {
  SocketFd socket = CreateTestRawSocket(/*blocking=*/true);
  if (socket == kInvalidSocketFd) {
    GTEST_SKIP();
  }

  QuicIpAddress localhost_address = quiche::TestLoopback();
  QUICHE_EXPECT_OK(socket_api::SetIpHeaderIncluded(
      socket, localhost_address.address_family(), /*ip_header_included=*/true));

  // Arbitrarily-chosen ephemeral ports.
  QuicSocketAddress client_address(localhost_address, /*port=*/53368);
  QuicSocketAddress server_address(localhost_address, /*port=*/56362);
  std::string packet =
      CreateIpPacket(client_address.host(), server_address.host(),
                     CreateUdpPacket(client_address, server_address, "foo"));
  absl::StatusOr<absl::string_view> result = socket_api::SendTo(
      socket, QuicSocketAddress(localhost_address, /*port=*/56362), packet);

  // Expect at least some data to be sent successfully.
  QUICHE_ASSERT_OK(result.status());
  EXPECT_THAT(result.value(), SizeIs(Lt(packet.size())));

  QUICHE_EXPECT_OK(socket_api::Close(socket));
}

}  // namespace
}  // namespace quic::test
