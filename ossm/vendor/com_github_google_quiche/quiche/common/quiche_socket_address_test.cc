// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_socket_address.h"

#include <sstream>

#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_ip_address.h"

namespace quiche {
namespace {

TEST(QuicheSocketAddress, Uninitialized) {
  QuicheSocketAddress uninitialized;
  EXPECT_FALSE(uninitialized.IsInitialized());
}

TEST(QuicheSocketAddress, ExplicitConstruction) {
  QuicheSocketAddress ipv4_address(QuicheIpAddress::Loopback4(), 443);
  QuicheSocketAddress ipv6_address(QuicheIpAddress::Loopback6(), 443);
  EXPECT_TRUE(ipv4_address.IsInitialized());
  EXPECT_EQ("127.0.0.1:443", ipv4_address.ToString());
  EXPECT_EQ("[::1]:443", ipv6_address.ToString());
  EXPECT_EQ(QuicheIpAddress::Loopback4(), ipv4_address.host());
  EXPECT_EQ(QuicheIpAddress::Loopback6(), ipv6_address.host());
  EXPECT_EQ(443, ipv4_address.port());
}

TEST(QuicheSocketAddress, OutputToStream) {
  QuicheSocketAddress ipv4_address(QuicheIpAddress::Loopback4(), 443);
  std::stringstream stream;
  stream << ipv4_address;
  EXPECT_EQ("127.0.0.1:443", stream.str());
}

TEST(QuicheSocketAddress, FromSockaddrIPv4) {
  union {
    sockaddr_storage storage;
    sockaddr addr;
    sockaddr_in v4;
  } address;

  memset(&address, 0, sizeof(address));
  address.v4.sin_family = AF_INET;
  address.v4.sin_addr = QuicheIpAddress::Loopback4().GetIPv4();
  address.v4.sin_port = htons(443);
  EXPECT_EQ("127.0.0.1:443",
            QuicheSocketAddress(&address.addr, sizeof(address.v4)).ToString());
  EXPECT_EQ("127.0.0.1:443", QuicheSocketAddress(address.storage).ToString());
}

TEST(QuicheSocketAddress, FromSockaddrIPv6) {
  union {
    sockaddr_storage storage;
    sockaddr addr;
    sockaddr_in6 v6;
  } address;

  memset(&address, 0, sizeof(address));
  address.v6.sin6_family = AF_INET6;
  address.v6.sin6_addr = QuicheIpAddress::Loopback6().GetIPv6();
  address.v6.sin6_port = htons(443);
  EXPECT_EQ("[::1]:443",
            QuicheSocketAddress(&address.addr, sizeof(address.v6)).ToString());
  EXPECT_EQ("[::1]:443", QuicheSocketAddress(address.storage).ToString());
}

TEST(QuicSocketAddres, ToSockaddrIPv4) {
  union {
    sockaddr_storage storage;
    sockaddr_in v4;
  } address;

  address.storage =
      QuicheSocketAddress(QuicheIpAddress::Loopback4(), 443).generic_address();
  ASSERT_EQ(AF_INET, address.v4.sin_family);
  EXPECT_EQ(QuicheIpAddress::Loopback4(), QuicheIpAddress(address.v4.sin_addr));
  EXPECT_EQ(htons(443), address.v4.sin_port);
}

TEST(QuicheSocketAddress, Normalize) {
  QuicheIpAddress dual_stacked;
  ASSERT_TRUE(dual_stacked.FromString("::ffff:127.0.0.1"));
  ASSERT_TRUE(dual_stacked.IsIPv6());
  QuicheSocketAddress not_normalized(dual_stacked, 443);
  QuicheSocketAddress normalized = not_normalized.Normalized();
  EXPECT_EQ("[::ffff:127.0.0.1]:443", not_normalized.ToString());
  EXPECT_EQ("127.0.0.1:443", normalized.ToString());
}

// TODO(vasilvv): either ensure this works on all platforms, or deprecate and
// remove this API.
#if defined(__linux__) && !defined(ANDROID)
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

TEST(QuicheSocketAddress, FromSocket) {
  int fd;
  QuicheSocketAddress address;
  bool bound = false;
  for (int port = 50000; port < 50400; port++) {
    fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_GT(fd, 0);

    address = QuicheSocketAddress(QuicheIpAddress::Loopback6(), port);
    sockaddr_storage raw_address = address.generic_address();
    int bind_result = bind(fd, reinterpret_cast<const sockaddr*>(&raw_address),
                           sizeof(sockaddr_in6));

    if (bind_result < 0 && errno == EADDRINUSE) {
      close(fd);
      continue;
    }

    ASSERT_EQ(0, bind_result);
    bound = true;
    break;
  }
  ASSERT_TRUE(bound);

  QuicheSocketAddress real_address;
  ASSERT_EQ(0, real_address.FromSocket(fd));
  ASSERT_TRUE(real_address.IsInitialized());
  EXPECT_EQ(real_address, address);
  close(fd);
}
#endif

}  // namespace
}  // namespace quiche
