// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/test_tools/test_ip_packets.h"

#include <string>

#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_socket_address.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/quiche_ip_address.h"

namespace quic::test {
namespace {

TEST(TestIpPacketsTest, CreateIpv4Packet) {
  quiche::QuicheIpAddress source_ip;
  ASSERT_TRUE(source_ip.FromString("192.0.2.45"));
  ASSERT_TRUE(source_ip.IsIPv4());
  QuicSocketAddress source_address{source_ip, /*port=*/54131};

  quiche::QuicheIpAddress destination_ip;
  ASSERT_TRUE(destination_ip.FromString("192.0.2.67"));
  ASSERT_TRUE(destination_ip.IsIPv4());
  QuicSocketAddress destination_address(destination_ip, /*port=*/57542);

  std::string packet =
      CreateIpPacket(source_ip, destination_ip,
                     CreateUdpPacket(source_address, destination_address,
                                     /*payload=*/"foo"),
                     IpPacketPayloadType::kUdp);

  constexpr static char kExpected[] =
      "\x45"              // Version: 4, Header length: 5 words
      "\x00"              // DSCP: 0, ECN: 0
      "\x00\x1F"          // Total length: 31
      "\x00\x00"          // Id: 0
      "\x00\x00"          // Flags: 0, Fragment offset: 0
      "\x40"              // TTL: 64 hops
      "\x11"              // Protocol: 17 (UDP)
      "\x00\x00"          // Header checksum: 0
      "\xC0\x00\x02\x2D"  // Source IP
      "\xC0\x00\x02\x43"  // Destination IP
      "\xD3\x73"          // Source port
      "\xE0\xC6"          // Destination port
      "\x00\x0B"          // Length: 11
      "\xF1\xBC"          // Checksum: 0xF1BC
      "foo";              // Payload
  EXPECT_EQ(absl::string_view(packet),
            absl::string_view(kExpected, sizeof(kExpected) - 1));
}

TEST(TestIpPacketsTest, CreateIpv6Packet) {
  quiche::QuicheIpAddress source_ip;
  ASSERT_TRUE(source_ip.FromString("2001:db8::45"));
  ASSERT_TRUE(source_ip.IsIPv6());
  QuicSocketAddress source_address{source_ip, /*port=*/51941};

  quiche::QuicheIpAddress destination_ip;
  ASSERT_TRUE(destination_ip.FromString("2001:db8::67"));
  ASSERT_TRUE(destination_ip.IsIPv6());
  QuicSocketAddress destination_address(destination_ip, /*port=*/55341);

  std::string packet =
      CreateIpPacket(source_ip, destination_ip,
                     CreateUdpPacket(source_address, destination_address,
                                     /*payload=*/"foo"),
                     IpPacketPayloadType::kUdp);

  constexpr static char kExpected[] =
      "\x60\x00\x00\x00"  // Version: 6, Traffic class: 0, Flow label: 0
      "\x00\x0b"          // Payload length: 11
      "\x11"              // Next header: 17 (UDP)
      "\x40"              // Hop limit: 64
      // Source IP
      "\x20\x01\x0D\xB8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x45"
      // Destination IP
      "\x20\x01\x0D\xB8\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x67"
      "\xCA\xE5"  // Source port
      "\xD8\x2D"  // Destination port
      "\x00\x0B"  // Length: 11
      "\x2B\x37"  // Checksum: 0x2B37
      "foo";      // Payload
  EXPECT_EQ(absl::string_view(packet),
            absl::string_view(kExpected, sizeof(kExpected) - 1));
}

}  // namespace
}  // namespace quic::test
