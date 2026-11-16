// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_key_value_pair.h"

#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/common/platform/api/quiche_expect_bug.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace moqt::test {

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

class LocationFilterTest : public quic::test::QuicTest {};

TEST_F(LocationFilterTest, NoFilter) {
  LocationFilter filter = LocationFilter::NoLocationFilter();
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_TRUE(filter.InWindow(Location(0, 0)));
  EXPECT_TRUE(filter.InWindow(0));
}

TEST_F(LocationFilterTest, RelativeStart) {
  LocationFilter filter = LocationFilter::RelativeStartFilter(0);
  EXPECT_FALSE(filter.WindowKnown());
  EXPECT_FALSE(filter.is_next_object());
  EXPECT_EQ(filter.relative_start_offset(), 0);
  filter.ConvertRelativeToAbsolute(Location(3, 6));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.relative_start_offset(), std::nullopt);
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_FALSE(filter.InWindow(Location(3, 7)));
  EXPECT_TRUE(filter.InWindow(4));
  EXPECT_FALSE(filter.InWindow(3));
}

TEST_F(LocationFilterTest, RelativeStartWithOffset) {
  LocationFilter filter = LocationFilter::RelativeStartFilter(2);
  EXPECT_FALSE(filter.WindowKnown());
  EXPECT_FALSE(filter.is_next_object());
  EXPECT_EQ(filter.relative_start_offset(), 2);
  filter.ConvertRelativeToAbsolute(Location(3, 6));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.start(), Location(2, 0));
  EXPECT_TRUE(filter.InWindow(Location(2, 0)));
  EXPECT_FALSE(filter.InWindow(Location(1, 7)));
}

TEST_F(LocationFilterTest, NextObject) {
  LocationFilter filter = LocationFilter::NextObjectFilter();
  EXPECT_FALSE(filter.WindowKnown());
  EXPECT_TRUE(filter.is_next_object());
  EXPECT_EQ(filter.relative_start_offset(), std::nullopt);
  filter.ConvertRelativeToAbsolute(Location(3, 6));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_FALSE(filter.is_next_object());
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_TRUE(filter.InWindow(Location(3, 7)));
  EXPECT_FALSE(filter.InWindow(Location(3, 6)));
  EXPECT_TRUE(filter.InWindow(4));
  EXPECT_TRUE(filter.InWindow(3));
  EXPECT_FALSE(filter.InWindow(2));
}

TEST_F(LocationFilterTest, NextObjectNoObjectsYet) {
  LocationFilter filter = LocationFilter::NextObjectFilter();
  EXPECT_FALSE(filter.WindowKnown());
  filter.ConvertRelativeToAbsolute(std::nullopt);
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_TRUE(filter.InWindow(Location(0, 0)));
  EXPECT_TRUE(filter.InWindow(0));
}

TEST_F(LocationFilterTest, AbsoluteStart) {
  LocationFilter filter = LocationFilter::AbsoluteStartFilter(Location(3, 6));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_TRUE(filter.InWindow(Location(3, 6)));
  EXPECT_FALSE(filter.InWindow(Location(3, 5)));
  EXPECT_TRUE(filter.InWindow(4));
  EXPECT_TRUE(filter.InWindow(3));
  EXPECT_FALSE(filter.InWindow(2));
}

TEST_F(LocationFilterTest, AbsoluteStartGroupEnd) {
  LocationFilter filter =
      LocationFilter::AbsoluteStartGroupEndFilter(Location(3, 6), 5);
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.start(), Location(3, 6));
  EXPECT_EQ(filter.end_group(), 5);
  EXPECT_EQ(filter.end(), Location(5, kMaxObjectId));
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

TEST_F(LocationFilterTest, AbsoluteRange) {
  LocationFilter filter =
      LocationFilter::AbsoluteRangeFilter(Location(3, 6), Location(5, 10));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.start(), Location(3, 6));
  EXPECT_EQ(filter.end_group(), 5);
  EXPECT_EQ(filter.end(), Location(5, 10));
  EXPECT_TRUE(filter.InWindow(Location(4, 0)));
  EXPECT_TRUE(filter.InWindow(Location(3, 6)));
  EXPECT_TRUE(filter.InWindow(Location(5, 10)));
  EXPECT_FALSE(filter.InWindow(Location(5, 11)));
  EXPECT_FALSE(filter.InWindow(Location(3, 5)));
  EXPECT_FALSE(filter.InWindow(Location(6, 0)));
  EXPECT_TRUE(filter.InWindow(5));
  EXPECT_TRUE(filter.InWindow(3));
  EXPECT_FALSE(filter.InWindow(2));
  EXPECT_FALSE(filter.InWindow(6));

  LocationFilter single_object =
      LocationFilter::AbsoluteRangeFilter(Location(3, 6), Location(3, 6));
  EXPECT_TRUE(single_object.WindowKnown());
  EXPECT_EQ(single_object.start(), Location(3, 6));
  EXPECT_EQ(single_object.end(), Location(3, 6));
  EXPECT_TRUE(single_object.InWindow(Location(3, 6)));
  EXPECT_FALSE(single_object.InWindow(Location(3, 5)));
  EXPECT_FALSE(single_object.InWindow(Location(3, 7)));
}

TEST_F(LocationFilterTest, TrimEndLocation) {
  LocationFilter filter = LocationFilter::RelativeStartFilter(2);
  filter.ConvertRelativeToAbsolute(Location(3, 6));
  EXPECT_TRUE(filter.WindowKnown());
  EXPECT_EQ(filter.start(), Location(2, 0));
  EXPECT_EQ(filter.end(), Location(kMaxGroupId, kMaxObjectId));
  EXPECT_THAT(filter.TrimEndLocation(Location(1, 9)),
              quiche::test::StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_EQ(filter.end(), Location(kMaxGroupId, kMaxObjectId));
  QUICHE_EXPECT_OK(filter.TrimEndLocation(Location(3, 6)));
  EXPECT_EQ(filter.end(), Location(3, 6));
  EXPECT_TRUE(filter.InWindow(Location(3, 6)));
  EXPECT_FALSE(filter.InWindow(Location(3, 7)));
  QUICHE_EXPECT_OK(filter.TrimEndLocation(Location(4, 0)));
  EXPECT_EQ(filter.end(), Location(3, 6));
}

TEST_F(LocationFilterTest, InvalidConstructors) {
  LocationFilter filter = LocationFilter::NoLocationFilter();
  EXPECT_QUICHE_BUG(
      filter = LocationFilter::AbsoluteStartGroupEndFilter(Location(3, 6), 2),
      "End group is before start location");
  EXPECT_EQ(filter, LocationFilter::NoLocationFilter());
  EXPECT_QUICHE_BUG(filter = LocationFilter::AbsoluteRangeFilter(
                        Location(3, 6), Location(3, 5)),
                    "End location is before start location");
  EXPECT_EQ(filter, LocationFilter::NoLocationFilter());
  EXPECT_QUICHE_BUG(filter = LocationFilter::AbsoluteRangeFilter(
                        Location(3, 6), Location(2, 10)),
                    "End location is before start location");
  EXPECT_EQ(filter, LocationFilter::NoLocationFilter());
}

class MessageParametersTest : public quic::test::QuicTest {};

TEST_F(MessageParametersTest, Update) {
  MessageParameters p1;
  p1.object_delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(10);
  p1.expires = quic::QuicTimeDelta::FromMilliseconds(100);
  p1.set_forward(false);
  p1.subscriber_priority = 100;
  p1.new_group_request = 1;
  p1.track_namespace_prefix = TrackNamespace({"foo"});
  MessageParameters p2;
  p2.object_delivery_timeout = quic::QuicTimeDelta::FromMilliseconds(20);
  p2.authorization_tokens.push_back(
      AuthToken(AuthTokenType::kOutOfBand, "token"));
  p2.set_forward(true);
  p2.group_order = MoqtGroupOrder::kDescending;
  p2.track_namespace_prefix = TrackNamespace({"bar"});
  p1.Update(p2);
  EXPECT_EQ(p1.object_delivery_timeout,
            quic::QuicTimeDelta::FromMilliseconds(20));
  EXPECT_EQ(p1.expires, quic::QuicTimeDelta::FromMilliseconds(100));
  ASSERT_EQ(p1.authorization_tokens.size(), 1);
  EXPECT_EQ(p1.authorization_tokens[0],
            AuthToken(AuthTokenType::kOutOfBand, "token"));
  EXPECT_TRUE(p1.forward());
  EXPECT_EQ(p1.subscriber_priority, 100);
  EXPECT_EQ(p1.group_order, std::nullopt);
  EXPECT_EQ(p1.new_group_request, 1);
  EXPECT_EQ(p1.track_namespace_prefix, TrackNamespace({"bar"}));
}

TEST_F(MessageParametersTest, UpdateLocationFilter) {
  // 1. Uninitialized existing filter gets initialized on Update even if
  // largest_object is not set.
  MessageParameters params_no_largest(LocationFilter::NextObjectFilter());
  EXPECT_FALSE(params_no_largest.location_filter->WindowKnown());
  params_no_largest.Update(MessageParameters());
  EXPECT_TRUE(params_no_largest.location_filter->WindowKnown());
  EXPECT_EQ(params_no_largest.location_filter->start(), Location(0, 0));

  // 2. Uninitialized existing filter gets initialized with largest_object on
  // Update.
  MessageParameters params(LocationFilter::NextObjectFilter());
  params.set_forward(false);
  EXPECT_FALSE(params.location_filter->WindowKnown());
  MessageParameters update1;
  update1.largest_object = Location(1, 2);
  params.Update(update1);
  EXPECT_TRUE(params.location_filter->WindowKnown());
  EXPECT_EQ(params.location_filter->start(), Location(1, 3));

  // 3. Once the filter window is known, subsequent Updates do not re-initialize
  // it even if largest_object or forward changes.
  MessageParameters update2;
  update2.largest_object = Location(2, 4);
  update2.set_forward(true);
  params.Update(update2);
  EXPECT_TRUE(params.forward());
  EXPECT_EQ(params.location_filter->start(), Location(1, 3));

  // 4. New filter in Update is initialized with largest_object.
  MessageParameters update3(LocationFilter::RelativeStartFilter(0));
  update3.largest_object = Location(3, 1);
  params.Update(update3);
  EXPECT_TRUE(params.location_filter->WindowKnown());
  EXPECT_EQ(params.location_filter->start(), Location(4, 0));
}

class TrackPropertiesTest : public quic::test::QuicTest {};

TEST_F(TrackPropertiesTest, DefaultConstructor) {
  TrackProperties properties;
  EXPECT_TRUE(properties.Validate());
  EXPECT_EQ(properties.object_delivery_timeout(), kDefaultDeliveryTimeout);
  EXPECT_EQ(properties.max_cache_duration(), kDefaultMaxCacheDuration);
  EXPECT_EQ(properties.default_publisher_priority(), kDefaultPublisherPriority);
  EXPECT_EQ(properties.default_publisher_group_order(), kDefaultGroupOrder);
  EXPECT_EQ(properties.dynamic_groups(), kDefaultDynamicGroups);
  EXPECT_TRUE(properties.immutable_properties().empty());
}

TEST_F(TrackPropertiesTest, AllProperties) {
  TrackProperties properties(quic::QuicTimeDelta::FromMilliseconds(1),
                             quic::QuicTimeDelta::FromMilliseconds(2),
                             quic::QuicTimeDelta::FromMilliseconds(3),
                             MoqtPriority(10), MoqtGroupOrder::kDescending,
                             true, "properties");
  EXPECT_TRUE(properties.Validate());
  EXPECT_EQ(properties.object_delivery_timeout(),
            quic::QuicTimeDelta::FromMilliseconds(1));
  EXPECT_EQ(properties.max_cache_duration(),
            quic::QuicTimeDelta::FromMilliseconds(2));
  EXPECT_EQ(properties.subgroup_delivery_timeout(),
            quic::QuicTimeDelta::FromMilliseconds(3));
  EXPECT_EQ(properties.default_publisher_priority(), MoqtPriority(10));
  EXPECT_EQ(properties.default_publisher_group_order(),
            MoqtGroupOrder::kDescending);
  EXPECT_TRUE(properties.dynamic_groups());
  EXPECT_EQ(properties.immutable_properties(), "properties");
}

TEST_F(TrackPropertiesTest, ExplicitDefaults) {
  TrackProperties properties(kDefaultDeliveryTimeout, kDefaultMaxCacheDuration,
                             kDefaultDeliveryTimeout, kDefaultPublisherPriority,
                             kDefaultGroupOrder, kDefaultDynamicGroups, "");
  EXPECT_TRUE(properties.Validate());
  EXPECT_EQ(properties.size(), 0);
  EXPECT_EQ(properties.object_delivery_timeout(), kDefaultDeliveryTimeout);
  EXPECT_EQ(properties.max_cache_duration(), kDefaultMaxCacheDuration);
  EXPECT_EQ(properties.subgroup_delivery_timeout(), kDefaultDeliveryTimeout);
  EXPECT_EQ(properties.default_publisher_priority(), kDefaultPublisherPriority);
  EXPECT_EQ(properties.default_publisher_group_order(), kDefaultGroupOrder);
  EXPECT_EQ(properties.dynamic_groups(), kDefaultDynamicGroups);
  EXPECT_TRUE(properties.immutable_properties().empty());
}

TEST_F(TrackPropertiesTest, Validate) {
  TrackProperties properties;
  // Unknown property.
  properties.insert(0x42, 15ULL);
  properties.insert(0x42, 25ULL);
  EXPECT_TRUE(properties.Validate());

  properties.insert(static_cast<uint64_t>(PropertyType::kObjectDeliveryTimeout),
                    5ULL);
  properties.insert(static_cast<uint64_t>(PropertyType::kObjectDeliveryTimeout),
                    6ULL);
  EXPECT_FALSE(properties.Validate());

  properties.clear();
  properties.insert(static_cast<uint64_t>(PropertyType::kMaxCacheDuration),
                    5ULL);
  properties.insert(static_cast<uint64_t>(PropertyType::kMaxCacheDuration),
                    6ULL);
  EXPECT_FALSE(properties.Validate());

  properties.clear();
  properties.insert(
      static_cast<uint64_t>(PropertyType::kSubgroupDeliveryTimeout), 5ULL);
  properties.insert(
      static_cast<uint64_t>(PropertyType::kSubgroupDeliveryTimeout), 6ULL);
  EXPECT_FALSE(properties.Validate());

  properties.clear();
  properties.insert(
      static_cast<uint64_t>(PropertyType::kDefaultPublisherPriority), 256ULL);
  EXPECT_FALSE(properties.Validate());
  properties.clear();
  properties.insert(
      static_cast<uint64_t>(PropertyType::kDefaultPublisherPriority), 0ULL);
  properties.insert(
      static_cast<uint64_t>(PropertyType::kDefaultPublisherPriority), 1ULL);
  EXPECT_FALSE(properties.Validate());

  properties.clear();
  properties.insert(
      static_cast<uint64_t>(PropertyType::kDefaultPublisherGroupOrder), 0ULL);
  EXPECT_FALSE(properties.Validate());
  properties.clear();
  properties.insert(
      static_cast<uint64_t>(PropertyType::kDefaultPublisherGroupOrder), 3ULL);
  EXPECT_FALSE(properties.Validate());
  properties.clear();
  properties.insert(static_cast<uint64_t>(PropertyType::kDynamicGroups), 2ULL);
  properties.insert(static_cast<uint64_t>(PropertyType::kDynamicGroups), 1ULL);
  EXPECT_FALSE(properties.Validate());

  properties.clear();
  properties.insert(static_cast<uint64_t>(PropertyType::kDynamicGroups), 2ULL);
  EXPECT_FALSE(properties.Validate());
  properties.clear();
  properties.insert(static_cast<uint64_t>(PropertyType::kDynamicGroups), 0ULL);
  properties.insert(static_cast<uint64_t>(PropertyType::kDynamicGroups), 1ULL);
  EXPECT_FALSE(properties.Validate());

  properties.clear();
  properties.insert(static_cast<uint64_t>(PropertyType::kImmutableProperties),
                    "foo");
  properties.insert(static_cast<uint64_t>(PropertyType::kImmutableProperties),
                    "bar");
  EXPECT_FALSE(properties.Validate());
}

TEST_F(TrackPropertiesTest, CheckForUnknownMandatoryProperty) {
  TrackProperties properties;
  QUICHE_EXPECT_OK(properties.CheckForUnknownMandatoryProperty());

  // Non-mandatory property types (< 0x4000).
  properties.insert(static_cast<uint64_t>(PropertyType::kObjectDeliveryTimeout),
                    5ULL);
  properties.insert(0x3FFE, 1ULL);
  properties.insert(0x3FFF, "odd_optional");
  QUICHE_EXPECT_OK(properties.CheckForUnknownMandatoryProperty());

  // Non-mandatory property types (> 0x7FFF).
  properties.insert(0x8000, 2ULL);
  properties.insert(0x8001, "above_range");
  properties.insert(0x10000, 3ULL);
  QUICHE_EXPECT_OK(properties.CheckForUnknownMandatoryProperty());

  // Min boundary: 0x4000.
  TrackProperties min_mandatory;
  min_mandatory.insert(kMinMandatoryTrackProperty, 0ULL);
  EXPECT_TRUE(
      IsFailedPrecondition(min_mandatory.CheckForUnknownMandatoryProperty()));
  EXPECT_EQ(min_mandatory.CheckForUnknownMandatoryProperty().message(),
            "Unknown mandatory property: 0x4000");

  // Max boundary: 0x7FFF.
  TrackProperties max_mandatory;
  max_mandatory.insert(kMaxMandatoryTrackProperty, "mandatory_string");
  EXPECT_TRUE(
      IsFailedPrecondition(max_mandatory.CheckForUnknownMandatoryProperty()));
  EXPECT_EQ(max_mandatory.CheckForUnknownMandatoryProperty().message(),
            "Unknown mandatory property: 0x7fff");

  // In between.
  TrackProperties mid_mandatory;
  mid_mandatory.insert(0x5000, 42ULL);
  EXPECT_TRUE(
      IsFailedPrecondition(mid_mandatory.CheckForUnknownMandatoryProperty()));
  EXPECT_EQ(mid_mandatory.CheckForUnknownMandatoryProperty().message(),
            "Unknown mandatory property: 0x5000");
}

}  // namespace moqt::test
