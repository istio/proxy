// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/masque/connect_udp_datagram_payload.h"

#include <memory>

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::test {
namespace {

TEST(ConnectUdpDatagramPayloadTest, ParseUdpPacket) {
  static constexpr char kDatagramPayload[] = "\x00packet";

  std::unique_ptr<ConnectUdpDatagramPayload> parsed =
      ConnectUdpDatagramPayload::Parse(
          absl::string_view(kDatagramPayload, sizeof(kDatagramPayload) - 1));
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->GetContextId(),
            ConnectUdpDatagramUdpPacketPayload::kContextId);
  EXPECT_EQ(parsed->GetType(), ConnectUdpDatagramPayload::Type::kUdpPacket);
  EXPECT_EQ(parsed->GetUdpProxyingPayload(), "packet");
}

TEST(ConnectUdpDatagramPayloadTest, SerializeUdpPacket) {
  static constexpr absl::string_view kUdpPacket = "packet";

  ConnectUdpDatagramUdpPacketPayload payload(kUdpPacket);
  EXPECT_EQ(payload.GetUdpProxyingPayload(), kUdpPacket);

  EXPECT_EQ(payload.Serialize(), std::string("\x00packet", 7));
}

TEST(ConnectUdpDatagramPayloadTest, ParseUnknownPacket) {
  static constexpr char kDatagramPayload[] = "\x05packet";

  std::unique_ptr<ConnectUdpDatagramPayload> parsed =
      ConnectUdpDatagramPayload::Parse(
          absl::string_view(kDatagramPayload, sizeof(kDatagramPayload) - 1));
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->GetContextId(), 5);
  EXPECT_EQ(parsed->GetType(), ConnectUdpDatagramPayload::Type::kUnknown);
  EXPECT_EQ(parsed->GetUdpProxyingPayload(), "packet");
}

TEST(ConnectUdpDatagramPayloadTest, SerializeUnknownPacket) {
  static constexpr absl::string_view kInnerUdpProxyingPayload = "packet";

  ConnectUdpDatagramUnknownPayload payload(4u, kInnerUdpProxyingPayload);
  EXPECT_EQ(payload.GetUdpProxyingPayload(), kInnerUdpProxyingPayload);

  EXPECT_EQ(payload.Serialize(), std::string("\x04packet", 7));
}

}  // namespace
}  // namespace quiche::test
