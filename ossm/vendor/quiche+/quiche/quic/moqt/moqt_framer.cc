// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/moqt/moqt_framer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "absl/container/fixed_array.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "quiche/quic/core/quic_time.h"
#include "quiche/quic/core/quic_types.h"
#include "quiche/quic/moqt/moqt_error.h"
#include "quiche/quic/moqt/moqt_key_value_pair.h"
#include "quiche/quic/moqt/moqt_messages.h"
#include "quiche/quic/moqt/moqt_names.h"
#include "quiche/quic/moqt/moqt_object.h"
#include "quiche/quic/moqt/moqt_priority.h"
#include "quiche/quic/moqt/moqt_types.h"
#include "quiche/common/platform/api/quiche_bug_tracker.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/quiche_buffer_allocator.h"
#include "quiche/common/quiche_data_writer.h"
#include "quiche/common/quiche_status_utils.h"
#include "quiche/common/simple_buffer_allocator.h"
#include "quiche/common/wire_serialization.h"

namespace moqt {

namespace {

using ::quiche::QuicheBuffer;
using ::quiche::WireBytes;
using ::quiche::WireMoqVarInt;
using ::quiche::WireOptional;
using ::quiche::WireSpan;
using ::quiche::WireStringWithMoqVarIntLength;
using ::quiche::WireUint8;

class WireKeyVarIntPair {
 public:
  explicit WireKeyVarIntPair(uint64_t key, uint64_t value)
      : key_(key), value_(value) {}

  size_t GetLengthOnWire() {
    return quiche::ComputeLengthOnWire(WireMoqVarInt(key_),
                                       WireMoqVarInt(value_));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    return quiche::SerializeIntoWriter(writer, WireMoqVarInt(key_),
                                       WireMoqVarInt(value_));
  }

 private:
  const uint64_t key_;
  const uint64_t value_;
};

class WireKeyStringPair {
 public:
  explicit WireKeyStringPair(uint64_t key, absl::string_view value)
      : key_(key), value_(value) {}
  size_t GetLengthOnWire() {
    return quiche::ComputeLengthOnWire(WireMoqVarInt(key_),
                                       WireStringWithMoqVarIntLength(value_));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    return quiche::SerializeIntoWriter(writer, WireMoqVarInt(key_),
                                       WireStringWithMoqVarIntLength(value_));
  }

 private:
  const uint64_t key_;
  const absl::string_view value_;
};

class WireKeyValuePairList {
 public:
  explicit WireKeyValuePairList(const KeyValuePairList& list) : list_(list) {}

  size_t GetLengthOnWire() {
    size_t total = 0;
    uint64_t last_key = 0;
    list_.ForEach([&](uint64_t key,
                      std::variant<uint64_t, absl::string_view> value) {
      total += std::visit(
          absl::Overload{
              [&](uint64_t val) {
                return WireKeyVarIntPair(key - last_key, val).GetLengthOnWire();
              },
              [&](absl::string_view val) {
                return WireKeyStringPair(key - last_key, val).GetLengthOnWire();
              }},
          value);
      last_key = key;
      return true;
    });
    return total;
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    uint64_t last_key = 0;
    list_.ForEach(
        [&](uint64_t key, std::variant<uint64_t, absl::string_view> value) {
          absl::Status status = std::visit(
              absl::Overload{[&](uint64_t val) {
                               return WireKeyVarIntPair(key - last_key, val)
                                   .SerializeIntoWriter(writer);
                             },
                             [&](absl::string_view val) {
                               return WireKeyStringPair(key - last_key, val)
                                   .SerializeIntoWriter(writer);
                             }},
              value);
          last_key = key;
          return quiche::IsWriterStatusOk(status);
        });
    return absl::OkStatus();
  }

 private:
  const KeyValuePairList& list_;
};

class WireLocation {
 public:
  explicit WireLocation(const Location& location) : location_(location) {}
  size_t GetLengthOnWire() {
    return quiche::ComputeLengthOnWire(WireMoqVarInt(location_.group),
                                       WireMoqVarInt(location_.object));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    return quiche::SerializeIntoWriter(writer, WireMoqVarInt(location_.group),
                                       WireMoqVarInt(location_.object));
  }

 private:
  const Location& location_;
};

class WireAuthToken {
 public:
  explicit WireAuthToken(const AuthToken& token) : token_(token) {}
  size_t GetLengthOnWire() {
    return quiche::ComputeLengthOnWire(
        WireMoqVarInt(token_.alias_type),
        WireOptional<WireMoqVarInt>(token_.alias),
        WireOptional<WireMoqVarInt>(token_.type),
        WireOptional<WireBytes>(token_.value));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    return quiche::SerializeIntoWriter(
        writer, WireMoqVarInt(token_.alias_type),
        WireOptional<WireMoqVarInt>(token_.alias),
        WireOptional<WireMoqVarInt>(token_.type),
        WireOptional<WireBytes>(token_.value));
  }

 private:
  const AuthToken& token_;
};

class WireLocationFilter {
 public:
  explicit WireLocationFilter(const LocationFilter& filter) : filter_(filter) {}
  size_t GetLengthOnWire() {
    if (filter_.is_next_object()) {
      return quiche::ComputeLengthOnWire(
          WireMoqVarInt(MoqtLocationFilterType::kNextObject));
    }
    if (filter_.relative_start_offset().has_value()) {
      return quiche::ComputeLengthOnWire(
          WireMoqVarInt(MoqtLocationFilterType::kRelativeStart),
          WireMoqVarInt(*filter_.relative_start_offset()));
    }
    if (filter_.end() == Location(kMaxGroupId, kMaxObjectId)) {
      if (filter_.start() == Location(0, 0)) {
        return quiche::ComputeLengthOnWire(
            WireMoqVarInt(MoqtLocationFilterType::kNoFilter));
      }
      return quiche::ComputeLengthOnWire(
          WireMoqVarInt(MoqtLocationFilterType::kAbsoluteStart),
          WireLocation(filter_.start()));
    }
    if (filter_.end().object == kMaxObjectId) {
      return quiche::ComputeLengthOnWire(
          WireMoqVarInt(MoqtLocationFilterType::kAbsoluteStartGroupEnd),
          WireLocation(filter_.start()),
          WireMoqVarInt(filter_.end_group() - filter_.start().group));
    }
    return quiche::ComputeLengthOnWire(
        WireMoqVarInt(MoqtLocationFilterType::kAbsoluteRange),
        WireLocation(filter_.start()),
        WireMoqVarInt(filter_.end_group() - filter_.start().group),
        WireMoqVarInt(filter_.end().object));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    if (filter_.is_next_object()) {
      return quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(MoqtLocationFilterType::kNextObject));
    }
    if (filter_.relative_start_offset().has_value()) {
      return quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(MoqtLocationFilterType::kRelativeStart),
          WireMoqVarInt(*filter_.relative_start_offset()));
    }
    if (filter_.end() == Location(kMaxGroupId, kMaxObjectId)) {
      if (filter_.start() == Location(0, 0)) {
        return quiche::SerializeIntoWriter(
            writer, WireMoqVarInt(MoqtLocationFilterType::kNoFilter));
      }
      return quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(MoqtLocationFilterType::kAbsoluteStart),
          WireLocation(filter_.start()));
    }
    if (filter_.end().object == kMaxObjectId) {
      return quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(MoqtLocationFilterType::kAbsoluteStartGroupEnd),
          WireLocation(filter_.start()),
          WireMoqVarInt(filter_.end_group() - filter_.start().group));
    }
    return quiche::SerializeIntoWriter(
        writer, WireMoqVarInt(MoqtLocationFilterType::kAbsoluteRange),
        WireLocation(filter_.start()),
        WireMoqVarInt(filter_.end_group() - filter_.start().group),
        WireMoqVarInt(filter_.end().object));
  }

 private:
  const LocationFilter& filter_;
};

class WireTrackNamespace {
 public:
  WireTrackNamespace(const TrackNamespace& name) : namespace_(name) {}

  size_t GetLengthOnWire() {
    absl::FixedArray<absl::string_view> tuple(namespace_.tuple().begin(),
                                              namespace_.tuple().end());
    return quiche::ComputeLengthOnWire(
        WireMoqVarInt(namespace_.number_of_elements()),
        WireSpan<WireStringWithMoqVarIntLength, absl::string_view>(
            absl::MakeSpan(tuple)));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    absl::FixedArray<absl::string_view> tuple(namespace_.tuple().begin(),
                                              namespace_.tuple().end());
    return quiche::SerializeIntoWriter(
        writer, WireMoqVarInt(namespace_.number_of_elements()),
        WireSpan<WireStringWithMoqVarIntLength, absl::string_view>(
            absl::MakeSpan(tuple)));
  }

 private:
  const TrackNamespace& namespace_;
};

uint64_t TimeDeltaToMilliseconds(const quic::QuicTimeDelta& time_delta) {
  if (time_delta == quic::QuicTimeDelta::Infinite()) {
    return 0ULL;
  }
  return std::max(time_delta.ToMilliseconds(), int64_t{1});
}

class WireMessageParameters {
 public:
  explicit WireMessageParameters(const MessageParameters& parameters)
      : parameters_(parameters), num_parameters_(0) {
    if (parameters_.object_delivery_timeout.has_value()) {
      ++num_parameters_;
    }
    num_parameters_ += parameters_.authorization_tokens.size();
    if (parameters_.rendezvous_timeout.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.subgroup_delivery_timeout.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.expires.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.largest_object.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.fill_timeout.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.forward_has_value()) {
      ++num_parameters_;
    }
    if (parameters_.subscriber_priority.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.location_filter.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.group_order.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.new_group_request.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.track_namespace_prefix.has_value()) {
      ++num_parameters_;
    }
    if (parameters_.oack_window_size.has_value()) {
      ++num_parameters_;
    }
  }

  size_t GetLengthOnWire() {
    size_t length = WireMoqVarInt(num_parameters_).GetLengthOnWire();
    uint64_t last_key = 0;
    auto key_delta = [&](MessageParameter key) {
      uint64_t delta = static_cast<uint64_t>(key) - last_key;
      last_key = static_cast<uint64_t>(key);
      return delta;
    };
    if (parameters_.object_delivery_timeout.has_value()) {
      length += quiche::ComputeLengthOnWire(WireKeyVarIntPair(
          key_delta(MessageParameter::kObjectDeliveryTimeout),
          TimeDeltaToMilliseconds(*parameters_.object_delivery_timeout)));
    }
    for (const AuthToken& token : parameters_.authorization_tokens) {
      WireAuthToken wire_token(token);
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kAuthorizationToken)),
          WireMoqVarInt(wire_token.GetLengthOnWire()), wire_token);
    }
    if (parameters_.rendezvous_timeout.has_value()) {
      length +=
          WireKeyVarIntPair(key_delta(MessageParameter::kRendezvousTimeout),
                            parameters_.rendezvous_timeout->ToMilliseconds())
              .GetLengthOnWire();
    }
    if (parameters_.subgroup_delivery_timeout.has_value()) {
      length +=
          WireKeyVarIntPair(
              key_delta(MessageParameter::kSubgroupDeliveryTimeout),
              TimeDeltaToMilliseconds(*parameters_.subgroup_delivery_timeout))
              .GetLengthOnWire();
    }
    if (parameters_.expires.has_value()) {
      length += WireKeyVarIntPair(key_delta(MessageParameter::kExpires),
                                  TimeDeltaToMilliseconds(*parameters_.expires))
                    .GetLengthOnWire();
    }
    if (parameters_.largest_object.has_value()) {
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kLargestObject)),
          WireLocation(*parameters_.largest_object));
    }
    if (parameters_.fill_timeout.has_value()) {
      length += WireKeyVarIntPair(key_delta(MessageParameter::kFillTimeout),
                                  parameters_.fill_timeout->ToMilliseconds())
                    .GetLengthOnWire();
    }
    if (parameters_.forward_has_value()) {
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kForward)),
          WireUint8(parameters_.forward() ? 1ULL : 0ULL));
    }
    if (parameters_.subscriber_priority.has_value()) {
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kSubscriberPriority)),
          WireUint8(*parameters_.subscriber_priority));
    }
    if (parameters_.location_filter.has_value()) {
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kLocationFilter)),
          WireLocationFilter(*parameters_.location_filter));
    }
    if (parameters_.group_order.has_value()) {
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kGroupOrder)),
          WireUint8(static_cast<uint8_t>(*parameters_.group_order)));
    }
    if (parameters_.new_group_request.has_value()) {
      length += WireKeyVarIntPair(key_delta(MessageParameter::kNewGroupRequest),
                                  *parameters_.new_group_request)
                    .GetLengthOnWire();
    }
    if (parameters_.track_namespace_prefix.has_value()) {
      length += quiche::ComputeLengthOnWire(
          WireMoqVarInt(key_delta(MessageParameter::kTrackNamespacePrefix)),
          WireTrackNamespace(*parameters_.track_namespace_prefix));
    }
    if (parameters_.oack_window_size.has_value()) {
      length +=
          WireKeyVarIntPair(key_delta(MessageParameter::kOackWindowSize),
                            parameters_.oack_window_size->ToMicroseconds())
              .GetLengthOnWire();
    }
    return length;
  }

  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    QUICHE_RETURN_IF_ERROR(
        quiche::SerializeIntoWriter(writer, WireMoqVarInt(num_parameters_)));
    uint64_t last_key = 0;
    auto key_delta = [&](MessageParameter key) {
      uint64_t delta = static_cast<uint64_t>(key) - last_key;
      last_key = static_cast<uint64_t>(key);
      return delta;
    };
    if (parameters_.object_delivery_timeout.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireKeyVarIntPair(
              key_delta(MessageParameter::kObjectDeliveryTimeout),
              TimeDeltaToMilliseconds(*parameters_.object_delivery_timeout))));
    }
    for (const AuthToken& token : parameters_.authorization_tokens) {
      WireAuthToken wire_token(token);
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireMoqVarInt(key_delta(MessageParameter::kAuthorizationToken)),
          WireMoqVarInt(wire_token.GetLengthOnWire()), wire_token));
    }
    if (parameters_.rendezvous_timeout.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireKeyVarIntPair(key_delta(MessageParameter::kRendezvousTimeout),
                            parameters_.rendezvous_timeout->ToMilliseconds())));
    }
    if (parameters_.subgroup_delivery_timeout.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer, WireKeyVarIntPair(
                      key_delta(MessageParameter::kSubgroupDeliveryTimeout),
                      TimeDeltaToMilliseconds(
                          *parameters_.subgroup_delivery_timeout))));
    }
    if (parameters_.expires.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireKeyVarIntPair(key_delta(MessageParameter::kExpires),
                            TimeDeltaToMilliseconds(*parameters_.expires))));
    }
    if (parameters_.largest_object.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(key_delta(MessageParameter::kLargestObject)),
          WireLocation(*parameters_.largest_object)));
    }
    if (parameters_.fill_timeout.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireKeyVarIntPair(key_delta(MessageParameter::kFillTimeout),
                            parameters_.fill_timeout->ToMilliseconds())));
    }
    if (parameters_.forward_has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(key_delta(MessageParameter::kForward)),
          WireUint8(parameters_.forward() ? 1ULL : 0ULL)));
    }
    if (parameters_.subscriber_priority.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireMoqVarInt(key_delta(MessageParameter::kSubscriberPriority)),
          WireUint8(*parameters_.subscriber_priority)));
    }
    if (parameters_.location_filter.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(key_delta(MessageParameter::kLocationFilter)),
          WireLocationFilter(*parameters_.location_filter)));
    }
    if (parameters_.group_order.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer, WireMoqVarInt(key_delta(MessageParameter::kGroupOrder)),
          WireUint8(static_cast<uint8_t>(*parameters_.group_order))));
    }
    if (parameters_.new_group_request.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireKeyVarIntPair(key_delta(MessageParameter::kNewGroupRequest),
                            *parameters_.new_group_request)));
    }
    if (parameters_.track_namespace_prefix.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireMoqVarInt(key_delta(MessageParameter::kTrackNamespacePrefix)),
          WireTrackNamespace(*parameters_.track_namespace_prefix)));
    }
    if (parameters_.oack_window_size.has_value()) {
      QUICHE_RETURN_IF_ERROR(quiche::SerializeIntoWriter(
          writer,
          WireKeyVarIntPair(key_delta(MessageParameter::kOackWindowSize),
                            parameters_.oack_window_size->ToMicroseconds())));
    }
    return absl::OkStatus();
  }

 private:
  const MessageParameters& parameters_;
  uint64_t num_parameters_;
};

class WireFullTrackName {
 public:
  WireFullTrackName(const FullTrackName& name) : name_(name) {}

  size_t GetLengthOnWire() {
    return quiche::ComputeLengthOnWire(
        WireTrackNamespace(name_.track_namespace()),
        WireStringWithMoqVarIntLength(name_.name()));
  }
  absl::Status SerializeIntoWriter(quiche::QuicheDataWriter& writer) {
    return quiche::SerializeIntoWriter(
        writer, WireTrackNamespace(name_.track_namespace()),
        WireStringWithMoqVarIntLength(name_.name()));
  }

 private:
  const FullTrackName& name_;
};

// Serializes data into buffer using the default allocator.  Invokes QUICHE_BUG
// on failure.
template <typename... Ts>
QuicheBuffer Serialize(Ts... data) {
  absl::StatusOr<QuicheBuffer> buffer = quiche::SerializeIntoBuffer(
      quiche::SimpleBufferAllocator::Get(), data...);
  if (!buffer.ok()) {
    QUICHE_BUG(moqt_failed_serialization)
        << "Failed to serialize MoQT frame: " << buffer.status();
    return QuicheBuffer();
  }
  return *std::move(buffer);
}

// Serializes data into buffer using the default allocator.  Invokes QUICHE_BUG
// on failure.
template <typename... Ts>
QuicheBuffer SerializeControlMessage(MoqtMessageType type, Ts... data) {
  uint64_t message_type = static_cast<uint64_t>(type);
  size_t payload_size = quiche::ComputeLengthOnWire(data...);
  size_t buffer_size = sizeof(uint16_t) + payload_size +
                       quiche::ComputeLengthOnWire(WireMoqVarInt(message_type));
  if (buffer_size == 0) {
    return QuicheBuffer();
  }

  QuicheBuffer buffer(quiche::SimpleBufferAllocator::Get(), buffer_size);
  quiche::QuicheDataWriter writer(buffer.size(), buffer.data());
  absl::Status status =
      SerializeIntoWriter(writer, WireMoqVarInt(message_type),
                          quiche::WireUint16(payload_size), data...);
  if (!status.ok() || writer.remaining() != 0) {
    QUICHE_BUG(moqt_failed_serialization)
        << "Failed to serialize MoQT frame: " << status;
    return QuicheBuffer();
  }
  return buffer;
}

[[maybe_unused]] WireUint8 WireDeliveryOrder(
    std::optional<MoqtGroupOrder> group_order) {
  if (!group_order.has_value()) {
    return WireUint8(0x00);
  }
  switch (*group_order) {
    case MoqtGroupOrder::kAscending:
      return WireUint8(0x01);
    case MoqtGroupOrder::kDescending:
      return WireUint8(0x02);
  }
  QUICHE_NOTREACHED();
  return WireUint8(0xff);
}

WireUint8 WireBoolean(bool value) { return WireUint8(value ? 0x01 : 0x00); }

uint64_t SignedVarintSerializedForm(int64_t value) {
  if (value < 0) {
    return ((-value) << 1) | 0x01;
  }
  return value << 1;
}

quiche::QuicheBuffer SerializeAuthToken(const AuthToken& token) {
  return Serialize(WireMoqVarInt(token.alias_type),
                   WireOptional<WireMoqVarInt>(token.alias),
                   WireOptional<WireMoqVarInt>(token.type),
                   WireOptional<WireBytes>(token.value));
}

}  // namespace

KeyValuePairList SetupOptions::ToKeyValuePairList() const {
  KeyValuePairList out;
  if (max_auth_token_cache_size.has_value()) {
    out.insert(static_cast<uint64_t>(SetupOption::kMaxAuthTokenCacheSize),
               *max_auth_token_cache_size);
  }
  if (path.has_value()) {
    out.insert(static_cast<uint64_t>(SetupOption::kPath), *path);
  }
  for (const AuthToken& token : authorization_tokens) {
    out.insert(static_cast<uint64_t>(SetupOption::kAuthorizationToken),
               SerializeAuthToken(token).AsStringView());
  }
  if (authority.has_value()) {
    out.insert(static_cast<uint64_t>(SetupOption::kAuthority), *authority);
  }
  if (moqt_implementation.has_value()) {
    out.insert(static_cast<uint64_t>(SetupOption::kMoqtImplementation),
               *moqt_implementation);
  }
  if (support_object_acks.has_value()) {
    out.insert(static_cast<uint64_t>(SetupOption::kSupportObjectAcks),
               *support_object_acks ? 1ULL : 0ULL);
  }
  return out;
}

quiche::QuicheBuffer MoqtFramer::SerializeObjectHeader(
    const MoqtObject& message, MoqtDataStreamType message_type,
    std::optional<PublishedObjectMetadata>& previous_object_in_stream) {
  if (!ValidateObjectMetadata(message, message_type.EndOfGroupInStream(),
                              message_type.IsFetch())) {
    QUICHE_BUG(QUICHE_BUG_serialize_object_header_01)
        << "Object metadata is invalid";
    return quiche::QuicheBuffer();
  }
  // Many fields are optional because the stream type or Fetch serialization
  // omits them.
  std::optional<uint64_t> stream_type;
  std::optional<uint64_t> track_id;  // Track alias or FETCH ID.
  std::optional<uint64_t> group_id;
  std::optional<uint64_t> subgroup_id;
  std::optional<uint64_t> object_id;
  std::optional<uint8_t> publisher_priority;
  std::optional<absl::string_view> properties;
  bool is_first_in_stream = !previous_object_in_stream.has_value();
  if (is_first_in_stream) {
    stream_type = message_type.value();
    track_id = message.track_alias;
  }
  if (message_type.IsFetch()) {
    MoqtFetchSerialization serialization;
    std::optional<uint64_t> payload_length;
    if (is_first_in_stream) {
      serialization = MoqtFetchSerialization(message);
    } else {
      serialization =
          MoqtFetchSerialization(message, *previous_object_in_stream);
    }
    if (serialization.has_group_id()) {
      if (is_first_in_stream) {
        group_id = message.group_id;
      } else {
        group_id =
            (message.group_id > previous_object_in_stream->location.group)
                ? (message.group_id -
                   previous_object_in_stream->location.group - 1)
                : (previous_object_in_stream->location.group -
                   message.group_id - 1);
      }
    }
    if (serialization.has_subgroup_id()) {
      subgroup_id = message.subgroup_id;
    }
    if (serialization.has_object_id()) {
      if (is_first_in_stream || serialization.has_group_id()) {
        object_id = message.object_id;
      } else {
        object_id =
            message.object_id - previous_object_in_stream->location.object;
      }
    }
    if (serialization.has_priority()) {
      publisher_priority = message.publisher_priority;
    }
    if (serialization.has_properties()) {
      properties = message.properties;
    }
    if (!serialization.end_of_non_existent_range() &&
        !serialization.end_of_unknown_range()) {
      payload_length = message.payload_length;
    }
    return Serialize(WireOptional<WireMoqVarInt>(stream_type),
                     WireOptional<WireMoqVarInt>(track_id),
                     WireMoqVarInt(serialization.value()),
                     WireOptional<WireMoqVarInt>(group_id),
                     WireOptional<WireMoqVarInt>(subgroup_id),
                     WireOptional<WireMoqVarInt>(object_id),
                     WireOptional<WireUint8>(publisher_priority),
                     WireOptional<WireStringWithMoqVarIntLength>(properties),
                     WireOptional<WireMoqVarInt>(payload_length));
  }
  // Subgroup stream.
  if (!message.subgroup_id.has_value()) {
    QUICHE_BUG(QUICHE_BUG_serialize_object_header_02)
        << "Subgroup ID is missing";
    return quiche::QuicheBuffer();
  }
  if (is_first_in_stream) {
    group_id = message.group_id;
    if (message_type.IsSubgroupPresent()) {
      subgroup_id = message.subgroup_id;
    }
    if (!message_type.HasDefaultPriority()) {
      publisher_priority = message.publisher_priority;
    }
  }
  object_id = message.object_id;
  if (!is_first_in_stream) {
    *object_id -= (previous_object_in_stream->location.object + 1);
  }
  if (message_type.ArePropertiesPresent()) {
    properties = message.properties;
  }
  std::optional<uint64_t> object_status;
  if (message.payload_length == 0) {
    object_status = static_cast<uint64_t>(message.object_status);
  }
  return Serialize(WireOptional<WireMoqVarInt>(stream_type),
                   WireOptional<WireMoqVarInt>(track_id),
                   WireOptional<WireMoqVarInt>(group_id),
                   WireOptional<WireMoqVarInt>(subgroup_id),
                   WireOptional<WireUint8>(publisher_priority),
                   WireMoqVarInt(*object_id),
                   WireOptional<WireStringWithMoqVarIntLength>(properties),
                   WireMoqVarInt(message.payload_length),
                   WireOptional<WireMoqVarInt>(object_status));
}

quiche::QuicheBuffer MoqtFramer::SerializeObjectDatagram(
    const MoqtObject& message, absl::string_view payload,
    MoqtPriority default_priority) {
  // Any datagram can support an alternate end-of-group encoding, and it is
  // never a FETCH.
  if (!ValidateObjectMetadata(message, true, false) ||
      message.subgroup_id.has_value()) {
    QUICHE_BUG(QUICHE_BUG_serialize_object_datagram_01)
        << "Object metadata is invalid";
    return quiche::QuicheBuffer();
  }
  if (message.payload_length != payload.length()) {
    QUICHE_BUG(QUICHE_BUG_serialize_object_datagram_03)
        << "Payload length does not match payload";
    return quiche::QuicheBuffer();
  }
  MoqtDatagramType datagram_type(
      !payload.empty(), !message.properties.empty(),
      message.object_status == MoqtObjectStatus::kEndOfGroup,
      message.publisher_priority == default_priority, message.object_id == 0);
  std::optional<uint64_t> object_id =
      datagram_type.has_object_id() ? std::optional<uint64_t>(message.object_id)
                                    : std::nullopt;
  std::optional<uint8_t> publisher_priority =
      datagram_type.has_default_priority()
          ? std::nullopt
          : std::optional<uint8_t>(message.publisher_priority);
  std::optional<absl::string_view> properties =
      datagram_type.has_properties()
          ? std::optional<absl::string_view>(message.properties)
          : std::nullopt;
  std::optional<uint64_t> object_status =
      datagram_type.has_status()
          ? std::optional<uint64_t>(
                static_cast<uint64_t>(message.object_status))
          : std::nullopt;
  std::optional<absl::string_view> raw_payload =
      payload.empty() ? std::nullopt
                      : std::optional<absl::string_view>(payload);
  return Serialize(
      WireMoqVarInt(datagram_type.value()), WireMoqVarInt(message.track_alias),
      WireMoqVarInt(message.group_id), WireOptional<WireMoqVarInt>(object_id),
      WireOptional<WireUint8>(publisher_priority),
      WireOptional<WireStringWithMoqVarIntLength>(properties),
      WireOptional<WireMoqVarInt>(object_status),
      WireOptional<WireBytes>(raw_payload));
}

quiche::QuicheBuffer MoqtFramer::SerializeSetup(const MoqtSetup& message) {
  KeyValuePairList options;
  if (!FillAndValidateSetupOptions(message.options, options)) {
    return quiche::QuicheBuffer();
  }
  return SerializeControlMessage(MoqtMessageType::kSetup,
                                 WireKeyValuePairList(options));
}

quiche::QuicheBuffer MoqtFramer::SerializeRequestOk(
    const MoqtRequestOk& message) {
  return SerializeControlMessage(MoqtMessageType::kRequestOk,
                                 WireMessageParameters(message.parameters),
                                 WireKeyValuePairList(message.properties));
}

quiche::QuicheBuffer MoqtFramer::SerializeSubscribe(
    const MoqtSubscribe& message) {
  return SerializeControlMessage(MoqtMessageType::kSubscribe,
                                 WireMoqVarInt(message.request_id),
                                 WireFullTrackName(message.full_track_name),
                                 WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeSubscribeOk(
    const MoqtSubscribeOk& message) {
  if (!message.properties.Validate()) {
    QUICHE_BUG(QUICHE_BUG_serialize_subscribe_ok_01)
        << "Subscribe OK properties are ill-formed";
    return quiche::QuicheBuffer();
  }
  return SerializeControlMessage(MoqtMessageType::kSubscribeOk,
                                 WireMoqVarInt(message.track_alias),
                                 WireMessageParameters(message.parameters),
                                 WireKeyValuePairList(message.properties));
}

quiche::QuicheBuffer MoqtFramer::SerializeRequestError(
    const MoqtRequestError& message) {
  if ((message.error_code == RequestErrorCode::kRedirect) !=
      message.redirect.has_value()) {
    QUICHE_BUG(QUICHE_BUG_serialize_request_error_01)
        << "Redirect presence must match kRedirect error code";
    return quiche::QuicheBuffer();
  }
  if (message.redirect.has_value()) {
    if (perspective_ == quic::Perspective::IS_CLIENT &&
        !message.redirect->connect_uri.empty()) {
      QUICHE_BUG(QUICHE_BUG_serialize_request_error_02)
          << "Connect URI must be empty from client";
      return quiche::QuicheBuffer();
    }
    return SerializeControlMessage(
        MoqtMessageType::kRequestError, WireMoqVarInt(message.error_code),
        WireMoqVarInt(message.retry_interval.has_value()
                          ? message.retry_interval->ToMilliseconds() + 1
                          : 0),
        WireStringWithMoqVarIntLength(message.reason_phrase),
        WireStringWithMoqVarIntLength(message.redirect->connect_uri),
        WireFullTrackName(message.redirect->full_track_name));
  }
  return SerializeControlMessage(
      MoqtMessageType::kRequestError, WireMoqVarInt(message.error_code),
      WireMoqVarInt(message.retry_interval.has_value()
                        ? message.retry_interval->ToMilliseconds() + 1
                        : 0),
      WireStringWithMoqVarIntLength(message.reason_phrase));
}

quiche::QuicheBuffer MoqtFramer::SerializePublishDone(
    const MoqtPublishDone& message) {
  return SerializeControlMessage(
      MoqtMessageType::kPublishDone, WireMoqVarInt(message.status_code),
      WireMoqVarInt(message.stream_count),
      WireStringWithMoqVarIntLength(message.error_reason));
}

quiche::QuicheBuffer MoqtFramer::SerializeRequestUpdate(
    const MoqtRequestUpdate& message) {
  return SerializeControlMessage(MoqtMessageType::kRequestUpdate,
                                 WireMoqVarInt(message.request_id),
                                 WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializePublishNamespace(
    const MoqtPublishNamespace& message) {
  return SerializeControlMessage(MoqtMessageType::kPublishNamespace,
                                 WireMoqVarInt(message.request_id),
                                 WireTrackNamespace(message.track_namespace),
                                 WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeNamespace(
    const MoqtNamespace& message) {
  return SerializeControlMessage(
      MoqtMessageType::kNamespace,
      WireTrackNamespace(message.track_namespace_suffix));
}

quiche::QuicheBuffer MoqtFramer::SerializeNamespaceDone(
    const MoqtNamespaceDone& message) {
  return SerializeControlMessage(
      MoqtMessageType::kNamespaceDone,
      WireTrackNamespace(message.track_namespace_suffix));
}

quiche::QuicheBuffer MoqtFramer::SerializePublishSkipped(
    const MoqtPublishSkipped& message) {
  return SerializeControlMessage(MoqtMessageType::kPublishSkipped,
                                 WireFullTrackName(message.name));
}

quiche::QuicheBuffer MoqtFramer::SerializeTrackStatus(
    const MoqtTrackStatus& message) {
  return SerializeControlMessage(MoqtMessageType::kTrackStatus,
                                 WireMoqVarInt(message.request_id),
                                 WireFullTrackName(message.full_track_name),
                                 WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeGoAway(const MoqtGoAway& message) {
  if (perspective_ == quic::Perspective::IS_CLIENT &&
      !message.new_session_uri.empty()) {
    QUICHE_BUG(QUICHE_BUG_serialize_go_away_01)
        << "New session URI must be empty from client";
    return quiche::QuicheBuffer();
  }
  if (message.new_session_uri.length() > kMaxNewSessionUriLength) {
    QUICHE_BUG(QUICHE_BUG_serialize_go_away_02)
        << "New session URI is too long";
    return quiche::QuicheBuffer();
  }
  return SerializeControlMessage(
      MoqtMessageType::kGoAway,
      WireStringWithMoqVarIntLength(message.new_session_uri),
      WireMoqVarInt(message.timeout.ToMilliseconds()));
}

quiche::QuicheBuffer MoqtFramer::SerializeSubscribeNamespace(
    const MoqtSubscribeNamespace& message) {
  return SerializeControlMessage(
      MoqtMessageType::kSubscribeNamespace, WireMoqVarInt(message.request_id),
      WireTrackNamespace(message.track_namespace_prefix),
      WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeSubscribeTracks(
    const MoqtSubscribeTracks& message) {
  return SerializeControlMessage(
      MoqtMessageType::kSubscribeTracks, WireMoqVarInt(message.request_id),
      WireTrackNamespace(message.track_namespace_prefix),
      WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeFetch(const MoqtFetch& message) {
  if (std::holds_alternative<StandaloneFetch>(message.fetch)) {
    const StandaloneFetch& standalone_fetch =
        std::get<StandaloneFetch>(message.fetch);
    if (standalone_fetch.end_location < standalone_fetch.start_location) {
      QUICHE_BUG(MoqtFramer_invalid_fetch) << "Invalid FETCH object range";
      return quiche::QuicheBuffer();
    }
  }
  if (std::holds_alternative<StandaloneFetch>(message.fetch)) {
    const StandaloneFetch& standalone_fetch =
        std::get<StandaloneFetch>(message.fetch);
    return SerializeControlMessage(
        MoqtMessageType::kFetch, WireMoqVarInt(message.request_id),
        WireMoqVarInt(FetchType::kStandalone),
        WireFullTrackName(standalone_fetch.full_track_name),
        WireMoqVarInt(standalone_fetch.start_location.group),
        WireMoqVarInt(standalone_fetch.start_location.object),
        WireMoqVarInt(standalone_fetch.end_location.group),
        WireMoqVarInt(standalone_fetch.end_location.object == kMaxObjectId
                          ? 0
                          : standalone_fetch.end_location.object + 1),
        WireMessageParameters(message.parameters));
  }
  uint64_t request_id, joining_start;
  if (std::holds_alternative<JoiningFetchRelative>(message.fetch)) {
    const JoiningFetchRelative& joining_fetch =
        std::get<JoiningFetchRelative>(message.fetch);
    request_id = joining_fetch.joining_request_id;
    joining_start = joining_fetch.joining_start;
  } else {
    const JoiningFetchAbsolute& joining_fetch =
        std::get<JoiningFetchAbsolute>(message.fetch);
    request_id = joining_fetch.joining_request_id;
    joining_start = joining_fetch.joining_start;
  }
  return SerializeControlMessage(
      MoqtMessageType::kFetch, WireMoqVarInt(message.request_id),
      WireMoqVarInt(message.fetch.index() + 1), WireMoqVarInt(request_id),
      WireMoqVarInt(joining_start), WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeFetchOk(const MoqtFetchOk& message) {
  return SerializeControlMessage(
      MoqtMessageType::kFetchOk, WireBoolean(message.end_of_track),
      WireMoqVarInt(message.end_location.group),
      WireMoqVarInt(message.end_location.object == kMaxObjectId
                        ? 0
                        : (message.end_location.object + 1)),
      WireMessageParameters(message.parameters),
      WireKeyValuePairList(message.properties));
}

quiche::QuicheBuffer MoqtFramer::SerializePublish(const MoqtPublish& message) {
  return SerializeControlMessage(MoqtMessageType::kPublish,
                                 WireMoqVarInt(message.request_id),
                                 WireFullTrackName(message.full_track_name),
                                 WireMoqVarInt(message.track_alias),
                                 WireMessageParameters(message.parameters),
                                 WireKeyValuePairList(message.properties));
}

quiche::QuicheBuffer MoqtFramer::SerializePublishStateNotify(
    const MoqtPublishStateNotify& message) {
  return SerializeControlMessage(MoqtMessageType::kPublishStateNotify,
                                 WireMessageParameters(message.parameters));
}

quiche::QuicheBuffer MoqtFramer::SerializeObjectAck(
    const MoqtObjectAck& message) {
  return SerializeControlMessage(
      MoqtMessageType::kObjectAck, WireMoqVarInt(message.group_id),
      WireMoqVarInt(message.object_id),
      WireMoqVarInt(SignedVarintSerializedForm(
          message.delta_from_deadline.ToMicroseconds())));
}

bool MoqtFramer::FillAndValidateSetupOptions(const SetupOptions& options,
                                             KeyValuePairList& out) {
  if (SetupOptionsAllowedByMessage(options, perspective_, using_webtrans_) !=
      MoqtError::kNoError) {
    QUICHE_BUG(QUICHE_BUG_invalid_setup_options)
        << "Invalid setup options for "
        << MoqtMessageTypeToString(MoqtMessageType::kSetup);
    return false;
  }
  out = options.ToKeyValuePairList();
  return true;
}

// static
bool MoqtFramer::ValidateObjectMetadata(const MoqtObject& object,
                                        bool alternate_end_of_group_encoding,
                                        bool is_fetch) {
  switch (object.object_status) {
    case MoqtObjectStatus::kNormal:
      return true;
    case MoqtObjectStatus::kEndOfGroup:
      return alternate_end_of_group_encoding ||
             (object.payload_length == 0 && object.properties.empty());
    case MoqtObjectStatus::kEndOfTrack:
      return object.payload_length == 0 && object.properties.empty();
    case MoqtObjectStatus::kEndOfNonExistentRange:
    case MoqtObjectStatus::kEndOfUnknownRange:
      return is_fetch && object.payload_length == 0 &&
             object.properties.empty();
    default:
      QUICHE_NOTREACHED();
      return false;
  }
}

}  // namespace moqt
