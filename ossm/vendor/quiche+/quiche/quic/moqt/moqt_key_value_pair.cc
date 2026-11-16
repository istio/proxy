// Copyright (c) 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_key_value_pair.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/common/platform/api/quiche_bug_tracker.h"
#include "quiche/common/platform/api/quiche_logging.h"

namespace moqt {

void KeyValuePairList::insert(uint64_t key,
                              std::variant<uint64_t, absl::string_view> value) {
  QUICHE_DCHECK(
      (key % 2 == 1 && std::holds_alternative<absl::string_view>(value)) ||
      (key % 2 == 0 && std::holds_alternative<uint64_t>(value)));
  if (std::holds_alternative<absl::string_view>(value)) {
    map_.insert({key, std::string(std::get<absl::string_view>(value))});
  } else {
    map_.insert({key, std::get<uint64_t>(value)});
  }
}

bool KeyValuePairList::ForEach(ValueCallback callback) const {
  for (const auto& [key, value] : map_) {
    if (!std::visit([&](const auto& val) { return callback(key, val); },
                    value)) {
      return false;
    }
  }
  return true;
}

KeyValuePairList::ValueVector KeyValuePairList::Get(uint64_t key) const {
  std::vector<std::variant<uint64_t, absl::string_view>> values;
  auto entries = map_.equal_range(key);
  for (auto it = entries.first; it != entries.second; ++it) {
    std::visit([&](const auto& value) { values.push_back(value); }, it->second);
  }
  return values;
}

void LocationFilter::ConvertRelativeToAbsolute(
    const std::optional<Location>& largest_object) {
  switch (type_) {
    case MoqtLocationFilterType::kRelativeStart:
      type_ = MoqtLocationFilterType::kAbsoluteStart;
      if (!largest_object.has_value() ||
          largest_object->group + 1 < start_.group) {
        start_ = Location(0, 0);
        break;
      }
      start_ = Location(largest_object->group + 1 - start_.group, 0);
      break;
    case MoqtLocationFilterType::kNextObject:
      type_ = MoqtLocationFilterType::kAbsoluteStart;
      if (!largest_object.has_value()) {
        break;
      }
      start_ = largest_object->Next();
      break;
    default:
      return;
  }
  type_ = MoqtLocationFilterType::kAbsoluteStart;
}

absl::Status LocationFilter::TrimEndLocation(const Location& end_location) {
  if (end_location < start()) {
    return absl::OutOfRangeError("End location is before start location");
  }
  end_ = std::min(end_, end_location);
  return absl::OkStatus();
}

void MessageParameters::Update(const MessageParameters& other) {
  // In order of MessageParameter enum.
  if (other.object_delivery_timeout.has_value()) {
    object_delivery_timeout = other.object_delivery_timeout;
  }
  if (!other.authorization_tokens.empty()) {
    authorization_tokens = other.authorization_tokens;
  }
  // RENDEZVOUS_TIMEOUT cannot be updated.
  if (other.subgroup_delivery_timeout.has_value()) {
    subgroup_delivery_timeout = other.subgroup_delivery_timeout;
  }
  if (other.expires.has_value()) {
    expires = other.expires;
  }
  if (other.largest_object.has_value()) {
    largest_object = other.largest_object;
  }
  if (other.forward_.has_value()) {
    forward_ = other.forward_;
  }
  if (other.subscriber_priority.has_value()) {
    subscriber_priority = other.subscriber_priority;
  }
  if (other.location_filter.has_value()) {
    location_filter = other.location_filter;
  }
  // GROUP_ORDER cannot be updated.
  if (other.new_group_request.has_value()) {
    new_group_request = other.new_group_request;
  }
  if (other.track_namespace_prefix.has_value()) {
    track_namespace_prefix = other.track_namespace_prefix;
  }
  if (other.oack_window_size.has_value()) {
    oack_window_size = other.oack_window_size;
  }
  if (location_filter.has_value() && !location_filter->WindowKnown()) {
    location_filter->ConvertRelativeToAbsolute(largest_object);
  }
}

TrackProperties::TrackProperties(
    std::optional<quic::QuicTimeDelta> object_delivery_timeout,
    std::optional<quic::QuicTimeDelta> max_cache_duration,
    std::optional<quic::QuicTimeDelta> subgroup_delivery_timeout,
    std::optional<MoqtPriority> publisher_priority,
    std::optional<MoqtGroupOrder> group_order,
    std::optional<bool> dynamic_groups,
    std::optional<absl::string_view> immutable_properties) {
  if (object_delivery_timeout.has_value() &&
      *object_delivery_timeout != kDefaultDeliveryTimeout &&
      *object_delivery_timeout != quic::QuicTimeDelta::Zero()) {
    insert(static_cast<uint64_t>(PropertyType::kObjectDeliveryTimeout),
           static_cast<uint64_t>(object_delivery_timeout->ToMilliseconds()));
  }
  if (max_cache_duration.has_value() &&
      *max_cache_duration != kDefaultMaxCacheDuration) {
    insert(static_cast<uint64_t>(PropertyType::kMaxCacheDuration),
           static_cast<uint64_t>(max_cache_duration->ToMilliseconds()));
  }
  if (subgroup_delivery_timeout.has_value() &&
      *subgroup_delivery_timeout != kDefaultDeliveryTimeout &&
      *subgroup_delivery_timeout != quic::QuicTimeDelta::Zero()) {
    insert(static_cast<uint64_t>(PropertyType::kSubgroupDeliveryTimeout),
           static_cast<uint64_t>(subgroup_delivery_timeout->ToMilliseconds()));
  }
  if (immutable_properties.has_value() && !immutable_properties->empty()) {
    insert(static_cast<uint64_t>(PropertyType::kImmutableProperties),
           *immutable_properties);
  }
  if (publisher_priority.has_value() &&
      *publisher_priority != kDefaultPublisherPriority) {
    insert(static_cast<uint64_t>(PropertyType::kDefaultPublisherPriority),
           static_cast<uint64_t>(*publisher_priority));
  }
  if (group_order.has_value() && *group_order != kDefaultGroupOrder) {
    insert(static_cast<uint64_t>(PropertyType::kDefaultPublisherGroupOrder),
           static_cast<uint64_t>(*group_order));
  }
  if (dynamic_groups.has_value() && *dynamic_groups != kDefaultDynamicGroups) {
    insert(static_cast<uint64_t>(PropertyType::kDynamicGroups),
           *dynamic_groups ? 1ULL : 0ULL);
  }
}

quic::QuicTimeDelta TrackProperties::object_delivery_timeout() const {
  std::optional<uint64_t> value =
      GetValueIfExactlyOne(PropertyType::kObjectDeliveryTimeout);
  return (value.has_value() && *value > 0)
             ? quic::QuicTimeDelta::FromMilliseconds(*value)
             : kDefaultDeliveryTimeout;
}
quic::QuicTimeDelta TrackProperties::max_cache_duration() const {
  std::optional<uint64_t> value =
      GetValueIfExactlyOne(PropertyType::kMaxCacheDuration);
  return value.has_value() ? quic::QuicTimeDelta::FromMilliseconds(*value)
                           : kDefaultMaxCacheDuration;
}
quic::QuicTimeDelta TrackProperties::subgroup_delivery_timeout() const {
  std::optional<uint64_t> value =
      GetValueIfExactlyOne(PropertyType::kSubgroupDeliveryTimeout);
  return (value.has_value() && *value > 0)
             ? quic::QuicTimeDelta::FromMilliseconds(*value)
             : kDefaultDeliveryTimeout;
}
absl::string_view TrackProperties::immutable_properties() const {
  ValueVector values =
      Get(static_cast<uint64_t>(PropertyType::kImmutableProperties));
  return (values.size() != 1 ||
          !std::holds_alternative<absl::string_view>(values[0]))
             ? ""
             : std::get<absl::string_view>(values[0]);
}
MoqtPriority TrackProperties::default_publisher_priority() const {
  std::optional<uint64_t> value =
      GetValueIfExactlyOne(PropertyType::kDefaultPublisherPriority);
  return (!value.has_value() || *value > kMaxPriority)
             ? kDefaultPublisherPriority
             : static_cast<MoqtPriority>(*value);
}
MoqtGroupOrder TrackProperties::default_publisher_group_order() const {
  std::optional<uint64_t> value =
      GetValueIfExactlyOne(PropertyType::kDefaultPublisherGroupOrder);
  return (!value.has_value() || *value > kMaxMoqtGroupOrder ||
          *value < kMinMoqtGroupOrder)
             ? kDefaultGroupOrder
             : static_cast<MoqtGroupOrder>(*value);
}
bool TrackProperties::dynamic_groups() const {
  std::optional<uint64_t> value =
      GetValueIfExactlyOne(PropertyType::kDynamicGroups);
  return (!value.has_value() || *value > 1) ? kDefaultDynamicGroups
                                            : (*value == 1);
}

bool TrackProperties::Validate() const {
  // TODO(martinduke): If immutable properties include an immutable properties
  // property, the track is malformed.
  return (ValidateInner(PropertyType::kObjectDeliveryTimeout, std::nullopt,
                        std::nullopt) &&
          ValidateInner(PropertyType::kMaxCacheDuration, std::nullopt,
                        std::nullopt) &&
          ValidateInner(PropertyType::kSubgroupDeliveryTimeout, std::nullopt,
                        std::nullopt) &&
          ValidateInner(PropertyType::kDefaultPublisherPriority, std::nullopt,
                        kMaxPriority) &&
          ValidateInner(PropertyType::kDefaultPublisherGroupOrder,
                        kMinMoqtGroupOrder, kMaxMoqtGroupOrder) &&
          ValidateInner(PropertyType::kDynamicGroups, 0, 1) &&
          count(static_cast<uint64_t>(PropertyType::kImmutableProperties)) <=
              1);
}

// private
std::optional<uint64_t> TrackProperties::GetValueIfExactlyOne(
    PropertyType header) const {
  QUICHE_BUG_IF(moqt_bug_must_be_even, (static_cast<uint64_t>(header) % 2) == 1)
      << "MoqtKeyValuePair extracting integer from odd key.";
  ValueVector values = Get(static_cast<uint64_t>(header));
  if (values.size() != 1 || !std::holds_alternative<uint64_t>(values[0])) {
    return std::nullopt;
  }
  return std::get<uint64_t>(values[0]);
}

bool TrackProperties::ValidateInner(PropertyType header,
                                    std::optional<uint64_t> min,
                                    std::optional<uint64_t> max) const {
  ValueVector values = Get(static_cast<uint64_t>(header));
  switch (values.size()) {
    case 0:
      return true;
    case 1:
      if (!std::holds_alternative<uint64_t>(values[0])) {
        return false;
      }
      return (!min.has_value() || std::get<uint64_t>(values[0]) >= *min) &&
             (!max.has_value() || std::get<uint64_t>(values[0]) <= *max);
    default:
      return false;
  }
}

}  // namespace moqt
