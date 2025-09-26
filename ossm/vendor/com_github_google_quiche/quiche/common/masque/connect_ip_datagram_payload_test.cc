// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/masque/connect_ip_datagram_payload.h"

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::test {
namespace {

TEST(ConnectIpDatagramPayloadTest, ParseIpPacket) {
  static constexpr char kDatagramPayload[] = "\x00packet";

  std::unique_ptr<ConnectIpDatagramPayload> parsed =
      ConnectIpDatagramPayload::Parse(
          absl::string_view(kDatagramPayload, sizeof(kDatagramPayload) - 1));
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->GetContextId(),
            ConnectIpDatagramIpPacketPayload::kContextId);
  EXPECT_EQ(parsed->GetType(), ConnectIpDatagramPayload::Type::kIpPacket);
  EXPECT_EQ(parsed->GetIpProxyingPayload(), "packet");
}

TEST(ConnectIpDatagramPayloadTest, SerializeIpPacket) {
  static constexpr absl::string_view kIpPacket = "packet";

  ConnectIpDatagramIpPacketPayload payload(kIpPacket);
  EXPECT_EQ(payload.GetIpProxyingPayload(), kIpPacket);

  EXPECT_EQ(payload.Serialize(), std::string("\x00packet", 7));
}

TEST(ConnectIpDatagramPayloadTest, ParseUnknownPacket) {
  static constexpr char kDatagramPayload[] = "\x05packet";

  std::unique_ptr<ConnectIpDatagramPayload> parsed =
      ConnectIpDatagramPayload::Parse(
          absl::string_view(kDatagramPayload, sizeof(kDatagramPayload) - 1));
  ASSERT_TRUE(parsed);

  EXPECT_EQ(parsed->GetContextId(), 5);
  EXPECT_EQ(parsed->GetType(), ConnectIpDatagramPayload::Type::kUnknown);
  EXPECT_EQ(parsed->GetIpProxyingPayload(), "packet");
}

TEST(ConnectIpDatagramPayloadTest, SerializeUnknownPacket) {
  static constexpr absl::string_view kInnerIpProxyingPayload = "packet";

  ConnectIpDatagramUnknownPayload payload(4u, kInnerIpProxyingPayload);
  EXPECT_EQ(payload.GetIpProxyingPayload(), kInnerIpProxyingPayload);

  EXPECT_EQ(payload.Serialize(), std::string("\x04packet", 7));
}

}  // namespace
}  // namespace quiche::test
