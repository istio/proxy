// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_key_value_pair.h"

#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace moqt::test {

using ::quiche::test::StatusIs;

class LocationTest : public quic::test::QuicTest {};

TEST_F(LocationTest, LocationTests) {
  Location location;
  EXPECT_EQ(location, Location(0, 0));
  EXPECT_EQ(location.Next(), Location(0, 1));
  EXPECT_LT(Location(4, 20), Location(5, 0));
  EXPECT_LT(Location(4, 0), Location(4, 1));
}

class AuthTokenTest : public quic::test::QuicTest {};

TEST_F(AuthTokenTest, Delete) {
  AuthToken token(1, AuthTokenAliasType::kDelete);
  EXPECT_EQ(token.alias_type, AuthTokenAliasType::kDelete);
  EXPECT_EQ(token.alias, 1);
  EXPECT_FALSE(token.type.has_value());
  EXPECT_FALSE(token.value.has_value());
}

TEST_F(AuthTokenTest, Register) {
  AuthToken token(1, AuthTokenType::kOutOfBand, "token");
  EXPECT_EQ(token.alias_type, AuthTokenAliasType::kRegister);
  EXPECT_EQ(token.alias, 1);
  EXPECT_EQ(token.type, AuthTokenType::kOutOfBand);
  EXPECT_EQ(token.value, "token");
}

TEST_F(AuthTokenTest, UseAlias) {
  AuthToken token(1, AuthTokenAliasType::kUseAlias);
  EXPECT_EQ(token.alias_type, AuthTokenAliasType::kUseAlias);
  EXPECT_EQ(token.alias, 1);
  EXPECT_FALSE(token.type.has_value());
  EXPECT_FALSE(token.value.has_value());
}

TEST_F(AuthTokenTest, UseValue) {
  AuthToken token(AuthTokenType::kOutOfBand, "token");
  EXPECT_EQ(token.alias_type, AuthTokenAliasType::kUseValue);
  EXPECT_EQ(token.type, AuthTokenType::kOutOfBand);
  EXPECT_EQ(token.value, "token");
}

class SubscriptionFilterTest : public quic::test::QuicTest {};

TEST_F(SubscriptionFilterTest, NextGroupStart) {
  SubscriptionFilter filter(MoqtFilterType::kNextGroupStart);
  EXPECT_EQ(filter.type(), (MoqtFilterType::kNextGroupStart));
  EXPECT_FALSE(filter.WindowKnown());
  filter.OnLargestObject(Location(3, 6));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.type(), (MoqtFilterType::kAbsoluteStart));
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_FALSE(filter.InWindow(Location(3, 7)));
  EXPECT_TRUE(filter.InWindow(4));
  EXPECT_FALSE(filter.InWindow(3));
}

TEST_F(SubscriptionFilterTest, LargestObject) {
  SubscriptionFilter filter(MoqtFilterType::kLargestObject);
  EXPECT_EQ(filter.type(), (MoqtFilterType::kLargestObject));
  EXPECT_FALSE(filter.WindowKnown());
  filter.OnLargestObject(Location(3, 6));
  EXPECT_EQ(filter.type(), (MoqtFilterType::kAbsoluteStart));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_TRUE(filter.InWindow(Location(3, 7)));
  EXPECT_FALSE(filter.InWindow(Location(3, 6)));
  EXPECT_TRUE(filter.InWindow(4));
  EXPECT_TRUE(filter.InWindow(3));
  EXPECT_FALSE(filter.InWindow(2));
}

TEST_F(SubscriptionFilterTest, LargestObjectNoObjectsYet) {
  SubscriptionFilter filter(MoqtFilterType::kLargestObject);
  EXPECT_EQ(filter.type(), (MoqtFilterType::kLargestObject));
  EXPECT_FALSE(filter.WindowKnown());
  filter.OnLargestObject(std::nullopt);
  EXPECT_EQ(filter.type(), (MoqtFilterType::kAbsoluteStart));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_TRUE(filter.InWindow(Location(0, 0)));
  EXPECT_TRUE(filter.InWindow(0));
}

TEST_F(SubscriptionFilterTest, AbsoluteStart) {
  SubscriptionFilter filter(Location(3, 6));
  EXPECT_EQ(filter.type(), (MoqtFilterType::kAbsoluteStart));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_TRUE(filter.InWindow(Location(3, 6)));
  EXPECT_FALSE(filter.InWindow(Location(3, 5)));
  EXPECT_TRUE(filter.InWindow(4));
  EXPECT_TRUE(filter.InWindow(3));
  EXPECT_FALSE(filter.InWindow(2));
}

TEST_F(SubscriptionFilterTest, AbsoluteRange) {
  SubscriptionFilter filter(Location(3, 6), 5);
  EXPECT_EQ(filter.type(), (MoqtFilterType::kAbsoluteRange));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.start(), Location(3, 6));
  EXPECT_EQ(filter.end_group(), 5);
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_TRUE(filter.InWindow(Location(3, 6)));
  EXPECT_TRUE(filter.InWindow(Location(5, kMaxObjectId)));
  EXPECT_FALSE(filter.InWindow(Location(3, 5)));
  EXPECT_FALSE(filter.InWindow(Location(6, 0)));
  EXPECT_TRUE(filter.InWindow(5));
  EXPECT_TRUE(filter.InWindow(3));
  EXPECT_FALSE(filter.InWindow(2));
  EXPECT_FALSE(filter.InWindow(6));
}

class MessageParametersTest : public quic::test::QuicTest {};

TEST_F(MessageParametersTest, FromKeyValuePairList) {
  KeyValuePairList list;
  list.insert(static_cast<uint64_t>(MessageParameter::kDeliveryTimeout), 1ULL);
  list.insert(static_cast<uint64_t>(MessageParameter::kForward), 0ULL);
  list.insert(static_cast<uint64_t>(MessageParameter::kOackWindowSize),
              12345678ULL);
  MessageParameters parameters;
  QUICHE_EXPECT_OK(parameters.FromKeyValuePairList(list));
  EXPECT_EQ(parameters.delivery_timeout,
            quic::QuicTimeDelta::FromMilliseconds(1));
  EXPECT_FALSE(parameters.forward());
  EXPECT_EQ(parameters.oack_window_size,
            quic::QuicTimeDelta::FromMicroseconds(12345678));
}

TEST_F(MessageParametersTest, IllegalKeyValuePairs) {
  KeyValuePairList list;
  MessageParameters parameters;
  list.insert(static_cast<uint64_t>(MessageParameter::kDeliveryTimeout), 0ULL);
  EXPECT_THAT(parameters.FromKeyValuePairList(list),
              StatusIs(absl::StatusCode::kInvalidArgument));
  list.clear();
  list.insert(static_cast<uint64_t>(MessageParameter::kForward), 2ULL);
  EXPECT_THAT(parameters.FromKeyValuePairList(list),
              StatusIs(absl::StatusCode::kInvalidArgument));
  list.clear();
  list.insert(static_cast<uint64_t>(MessageParameter::kSubscriberPriority),
              256ULL);
  EXPECT_THAT(parameters.FromKeyValuePairList(list),
              StatusIs(absl::StatusCode::kInvalidArgument));
  list.clear();
  list.insert(static_cast<uint64_t>(MessageParameter::kGroupOrder), 0ULL);
  EXPECT_THAT(parameters.FromKeyValuePairList(list),
              StatusIs(absl::StatusCode::kInvalidArgument));
  list.clear();
  list.insert(static_cast<uint64_t>(MessageParameter::kGroupOrder), 3ULL);
  EXPECT_THAT(parameters.FromKeyValuePairList(list),
              StatusIs(absl::StatusCode::kInvalidArgument));
  // Unknown MessageParameter.
  list.clear();
  list.insert(0x12345678, 12345678ULL);
  EXPECT_THAT(parameters.FromKeyValuePairList(list),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(MessageParametersTest, DuplicateParameters) {
  for (MessageParameter param :
       {MessageParameter::kDeliveryTimeout,
        // Auth token can be repeated.
        MessageParameter::kExpires, MessageParameter::kLargestObject,
        MessageParameter::kForward, MessageParameter::kSubscriberPriority,
        MessageParameter::kSubscriptionFilter, MessageParameter::kGroupOrder,
        MessageParameter::kNewGroupRequest,
        MessageParameter::kOackWindowSize}) {
    KeyValuePairList list;
    MessageParameters parameters;
    switch (param) {
      case MessageParameter::kLargestObject: {
        char largest_object[] = {0x00, 0x01};
        list.insert(static_cast<uint64_t>(param),
                    absl::string_view(largest_object, 2));
        largest_object[1] = 0x02;
        list.insert(static_cast<uint64_t>(param),
                    absl::string_view(largest_object, 2));
        break;
      }
      case MessageParameter::kForward: {
        list.insert(static_cast<uint64_t>(param), 0ULL);
        list.insert(static_cast<uint64_t>(param), 1ULL);
        break;
      }
      case MessageParameter::kSubscriberPriority: {
        list.insert(static_cast<uint64_t>(param), 127ULL);
        list.insert(static_cast<uint64_t>(param), 128ULL);
        break;
      }
      case MessageParameter::kSubscriptionFilter: {
        char filter[] = {0x01};  // kNextGroupStart
        list.insert(static_cast<uint64_t>(param), absl::string_view(filter, 1));
        filter[0] = 0x02;  // kLargestObject
        list.insert(static_cast<uint64_t>(param), absl::string_view(filter, 1));
        break;
      }
      case MessageParameter::kGroupOrder: {
        list.insert(static_cast<uint64_t>(param), 1ULL);
        list.insert(static_cast<uint64_t>(param), 2ULL);
        break;
      }
      default: {
        list.insert(static_cast<uint64_t>(param), 1024ULL);
        list.insert(static_cast<uint64_t>(param), 2048ULL);
        break;
      }
    }
    EXPECT_THAT(parameters.FromKeyValuePairList(list),
                StatusIs(absl::StatusCode::kInvalidArgument));
  }
}

TEST_F(MessageParametersTest, Update) {
  MessageParameters p1;
  p1.delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(10);
  p1.expires = quic::QuicTimeDelta::FromMilliseconds(100);
  p1.set_forward(false);
  p1.subscriber_priority = 100;
  p1.new_group_request = 1;
  MessageParameters p2;
  p2.delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(20);
  p2.authorization_tokens.push_back(
      AuthToken(AuthTokenType::kOutOfBand, "token"));
  p2.set_forward(true);
  p2.group_order = MoqtDeliveryOrder::kDescending;
  p1.Update(p2);
  EXPECT_EQ(p1.delivery_timeout, quic::QuicTimeDelta::FromMilliseconds(20));
  EXPECT_EQ(p1.expires, quic::QuicTimeDelta::FromMilliseconds(100));
  ASSERT_EQ(p1.authorization_tokens.size(), 1);
  EXPECT_EQ(p1.authorization_tokens[0],
            AuthToken(AuthTokenType::kOutOfBand, "token"));
  EXPECT_TRUE(p1.forward());
  EXPECT_EQ(p1.subscriber_priority, 100);
  EXPECT_EQ(p1.group_order, MoqtDeliveryOrder::kDescending);
  EXPECT_EQ(p1.new_group_request, 1);
}

class TrackExtensionsTest : public quic::test::QuicTest {};

TEST_F(TrackExtensionsTest, DefaultConstructor) {
  TrackExtensions extensions;
  EXPECT_TRUE(extensions.Validate());
  EXPECT_EQ(extensions.delivery_timeout(), kDefaultDeliveryTimeout);
  EXPECT_EQ(extensions.max_cache_duration(), kDefaultMaxCacheDuration);
  EXPECT_EQ(extensions.default_publisher_priority(), kDefaultPublisherPriority);
  EXPECT_EQ(extensions.default_publisher_group_order(), kDefaultGroupOrder);
  EXPECT_EQ(extensions.dynamic_groups(), kDefaultDynamicGroups);
  EXPECT_TRUE(extensions.immutable_extensions().empty());
}

TEST_F(TrackExtensionsTest, AllExtensions) {
  TrackExtensions extensions(quic::QuicTimeDelta::FromMilliseconds(1),
                             quic::QuicTimeDelta::FromMilliseconds(2),
                             MoqtPriority(10), MoqtDeliveryOrder::kDescending,
                             true, "extensions");
  EXPECT_TRUE(extensions.Validate());
  EXPECT_EQ(extensions.delivery_timeout(),
            quic::QuicTimeDelta::FromMilliseconds(1));
  EXPECT_EQ(extensions.max_cache_duration(),
            quic::QuicTimeDelta::FromMilliseconds(2));
  EXPECT_EQ(extensions.default_publisher_priority(), MoqtPriority(10));
  EXPECT_EQ(extensions.default_publisher_group_order(),
            MoqtDeliveryOrder::kDescending);
  EXPECT_TRUE(extensions.dynamic_groups());
  EXPECT_EQ(extensions.immutable_extensions(), "extensions");
}

TEST_F(TrackExtensionsTest, ExplicitDefaults) {
  TrackExtensions extensions(kDefaultDeliveryTimeout, kDefaultMaxCacheDuration,
                             kDefaultPublisherPriority, kDefaultGroupOrder,
                             kDefaultDynamicGroups, "");
  EXPECT_TRUE(extensions.Validate());
  EXPECT_EQ(extensions.size(), 0);
  EXPECT_EQ(extensions.delivery_timeout(), kDefaultDeliveryTimeout);
  EXPECT_EQ(extensions.max_cache_duration(), kDefaultMaxCacheDuration);
  EXPECT_EQ(extensions.default_publisher_priority(), kDefaultPublisherPriority);
  EXPECT_EQ(extensions.default_publisher_group_order(), kDefaultGroupOrder);
  EXPECT_EQ(extensions.dynamic_groups(), kDefaultDynamicGroups);
  EXPECT_TRUE(extensions.immutable_extensions().empty());
}

TEST_F(TrackExtensionsTest, Validate) {
  TrackExtensions extensions;
  // Unknown extension.
  extensions.insert(0x42, 15ULL);
  extensions.insert(0x42, 25ULL);
  EXPECT_TRUE(extensions.Validate());

  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDeliveryTimeout),
                    5ULL);
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDeliveryTimeout),
                    6ULL);
  EXPECT_FALSE(extensions.Validate());

  extensions.clear();
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kMaxCacheDuration),
                    5ULL);
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kMaxCacheDuration),
                    6ULL);
  EXPECT_FALSE(extensions.Validate());

  extensions.clear();
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kDefaultPublisherPriority),
      256ULL);
  EXPECT_FALSE(extensions.Validate());
  extensions.clear();
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kDefaultPublisherPriority), 0ULL);
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kDefaultPublisherPriority), 1ULL);
  EXPECT_FALSE(extensions.Validate());

  extensions.clear();
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kDefaultPublisherGroupOrder),
      0ULL);
  EXPECT_FALSE(extensions.Validate());
  extensions.clear();
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kDefaultPublisherGroupOrder),
      3ULL);
  EXPECT_FALSE(extensions.Validate());
  extensions.clear();
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDynamicGroups),
                    2ULL);
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDynamicGroups),
                    1ULL);
  EXPECT_FALSE(extensions.Validate());

  extensions.clear();
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDynamicGroups),
                    2ULL);
  EXPECT_FALSE(extensions.Validate());
  extensions.clear();
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDynamicGroups),
                    0ULL);
  extensions.insert(static_cast<uint64_t>(ExtensionHeader::kDynamicGroups),
                    1ULL);
  EXPECT_FALSE(extensions.Validate());

  extensions.clear();
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kImmutableExtensions), "foo");
  extensions.insert(
      static_cast<uint64_t>(ExtensionHeader::kImmutableExtensions), "bar");
  EXPECT_FALSE(extensions.Validate());
}

}  // namespace moqt::test
