// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/serialize.h"

#include <cstddef>
#include <cstdint>

#include "absl/base/casts.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel::internal {

namespace {

size_t SerializedDurationSizeOrTimestampSize(absl::Duration value) {
  size_t serialized_size = 0;
  if (value != absl::ZeroDuration()) {
    auto seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
    auto nanos = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    if (seconds != 0) {
      serialized_size +=
          VarintSize(MakeProtoWireTag(1, ProtoWireType::kVarint)) +
          VarintSize(seconds);
    }
    if (nanos != 0) {
      serialized_size +=
          VarintSize(MakeProtoWireTag(2, ProtoWireType::kVarint)) +
          VarintSize(nanos);
    }
  }
  return serialized_size;
}

}  // namespace

size_t SerializedDurationSize(absl::Duration value) {
  return SerializedDurationSizeOrTimestampSize(value);
}

size_t SerializedTimestampSize(absl::Time value) {
  return SerializedDurationSizeOrTimestampSize(value - absl::UnixEpoch());
}

namespace {

template <typename Value>
size_t SerializedBytesValueSizeOrStringValueSize(Value&& value) {
  return !value.empty() ? VarintSize(MakeProtoWireTag(
                              1, ProtoWireType::kLengthDelimited)) +
                              VarintSize(value.size()) + value.size()
                        : 0;
}

}  // namespace

size_t SerializedBytesValueSize(const absl::Cord& value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

size_t SerializedBytesValueSize(absl::string_view value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

size_t SerializedStringValueSize(const absl::Cord& value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

size_t SerializedStringValueSize(absl::string_view value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

namespace {

template <typename Value>
size_t SerializedVarintValueSize(Value value) {
  return value ? VarintSize(MakeProtoWireTag(1, ProtoWireType::kVarint)) +
                     VarintSize(value)
               : 0;
}

}  // namespace

size_t SerializedBoolValueSize(bool value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedInt32ValueSize(int32_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedInt64ValueSize(int64_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedUInt32ValueSize(uint32_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedUInt64ValueSize(uint64_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedFloatValueSize(float value) {
  return absl::bit_cast<uint32_t>(value) != 0
             ? VarintSize(MakeProtoWireTag(1, ProtoWireType::kFixed32)) + 4
             : 0;
}

size_t SerializedDoubleValueSize(double value) {
  return absl::bit_cast<uint64_t>(value) != 0
             ? VarintSize(MakeProtoWireTag(1, ProtoWireType::kFixed64)) + 8
             : 0;
}

size_t SerializedValueSize(const Json& value) {
  return absl::visit(
      absl::Overload(
          [](JsonNull) -> size_t {
            return VarintSize(MakeProtoWireTag(1, ProtoWireType::kVarint)) +
                   VarintSize(0);
          },
          [](JsonBool value) -> size_t {
            return VarintSize(MakeProtoWireTag(4, ProtoWireType::kVarint)) +
                   VarintSize(value);
          },
          [](JsonNumber value) -> size_t {
            return VarintSize(MakeProtoWireTag(2, ProtoWireType::kFixed64)) + 8;
          },
          [](const JsonString& value) -> size_t {
            return VarintSize(
                       MakeProtoWireTag(3, ProtoWireType::kLengthDelimited)) +
                   VarintSize(value.size()) + value.size();
          },
          [](const JsonArray& value) -> size_t {
            size_t value_size = SerializedListValueSize(value);
            return VarintSize(
                       MakeProtoWireTag(6, ProtoWireType::kLengthDelimited)) +
                   VarintSize(value_size) + value_size;
          },
          [](const JsonObject& value) -> size_t {
            size_t value_size = SerializedStructSize(value);
            return VarintSize(
                       MakeProtoWireTag(5, ProtoWireType::kLengthDelimited)) +
                   VarintSize(value_size) + value_size;
          }),
      value);
}

size_t SerializedListValueSize(const JsonArray& value) {
  size_t serialized_size = 0;
  if (!value.empty()) {
    size_t tag_size =
        VarintSize(MakeProtoWireTag(1, ProtoWireType::kLengthDelimited));
    for (const auto& element : value) {
      size_t value_size = SerializedValueSize(element);
      serialized_size += tag_size + VarintSize(value_size) + value_size;
    }
  }
  return serialized_size;
}

namespace {

size_t SerializedStructFieldSize(const JsonString& name, const Json& value) {
  size_t name_size =
      VarintSize(MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) +
      VarintSize(name.size()) + name.size();
  size_t value_size = SerializedValueSize(value);
  value_size =
      VarintSize(MakeProtoWireTag(2, ProtoWireType::kLengthDelimited)) +
      VarintSize(value_size) + value_size;
  return name_size + value_size;
}

}  // namespace

size_t SerializedStructSize(const JsonObject& value) {
  size_t serialized_size = 0;
  if (!value.empty()) {
    size_t tag_size =
        VarintSize(MakeProtoWireTag(1, ProtoWireType::kLengthDelimited));
    for (const auto& entry : value) {
      size_t value_size = SerializedStructFieldSize(entry.first, entry.second);
      serialized_size += tag_size + VarintSize(value_size) + value_size;
    }
  }
  return serialized_size;
}

// NOTE: We use ABSL_DCHECK below to assert that the resulting size of
// serializing is the same as the preflighting size calculation functions. They
// must be the same, and ABSL_DCHECK is the cheapest way of ensuring this
// without having to duplicate tests.

namespace {

absl::Status SerializeDurationOrTimestamp(absl::string_view name,
                                          absl::Duration value,
                                          absl::Cord& serialized_value) {
  if (value != absl::ZeroDuration()) {
    auto original_value = value;
    auto seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
    auto nanos = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    ProtoWireEncoder encoder(name, serialized_value);
    if (seconds != 0) {
      CEL_RETURN_IF_ERROR(
          encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
      CEL_RETURN_IF_ERROR(encoder.WriteVarint(seconds));
    }
    if (nanos != 0) {
      CEL_RETURN_IF_ERROR(
          encoder.WriteTag(ProtoWireTag(2, ProtoWireType::kVarint)));
      CEL_RETURN_IF_ERROR(encoder.WriteVarint(nanos));
    }
    encoder.EnsureFullyEncoded();
    ABSL_DCHECK_EQ(encoder.size(),
                   SerializedDurationSizeOrTimestampSize(original_value));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SerializeDuration(absl::Duration value,
                               absl::Cord& serialized_value) {
  return SerializeDurationOrTimestamp("google.protobuf.Duration", value,
                                      serialized_value);
}

absl::Status SerializeTimestamp(absl::Time value,
                                absl::Cord& serialized_value) {
  return SerializeDurationOrTimestamp(
      "google.protobuf.Timestamp", value - absl::UnixEpoch(), serialized_value);
}

namespace {

template <typename Value>
absl::Status SerializeBytesValueOrStringValue(absl::string_view name,
                                              Value&& value,
                                              absl::Cord& serialized_value) {
  if (!value.empty()) {
    ProtoWireEncoder encoder(name, serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kLengthDelimited)));
    CEL_RETURN_IF_ERROR(
        encoder.WriteLengthDelimited(std::forward<Value>(value)));
    encoder.EnsureFullyEncoded();
    ABSL_DCHECK_EQ(encoder.size(),
                   SerializedBytesValueSizeOrStringValueSize(value));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SerializeBytesValue(const absl::Cord& value,
                                 absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.BytesValue", value,
                                          serialized_value);
}

absl::Status SerializeBytesValue(absl::string_view value,
                                 absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.BytesValue", value,
                                          serialized_value);
}

absl::Status SerializeStringValue(const absl::Cord& value,
                                  absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.StringValue", value,
                                          serialized_value);
}

absl::Status SerializeStringValue(absl::string_view value,
                                  absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.StringValue", value,
                                          serialized_value);
}

namespace {

template <typename Value>
absl::Status SerializeVarintValue(absl::string_view name, Value value,
                                  absl::Cord& serialized_value) {
  if (value) {
    ProtoWireEncoder encoder(name, serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(value));
    encoder.EnsureFullyEncoded();
    ABSL_DCHECK_EQ(encoder.size(), SerializedVarintValueSize(value));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SerializeBoolValue(bool value, absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.BoolValue", value,
                              serialized_value);
}

absl::Status SerializeInt32Value(int32_t value, absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.Int32Value", value,
                              serialized_value);
}

absl::Status SerializeInt64Value(int64_t value, absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.Int64Value", value,
                              serialized_value);
}

absl::Status SerializeUInt32Value(uint32_t value,
                                  absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.UInt32Value", value,
                              serialized_value);
}

absl::Status SerializeUInt64Value(uint64_t value,
                                  absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.UInt64Value", value,
                              serialized_value);
}

absl::Status SerializeFloatValue(float value, absl::Cord& serialized_value) {
  if (absl::bit_cast<uint32_t>(value) != 0) {
    ProtoWireEncoder encoder("google.protobuf.FloatValue", serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kFixed32)));
    CEL_RETURN_IF_ERROR(encoder.WriteFixed32(value));
    encoder.EnsureFullyEncoded();
    ABSL_DCHECK_EQ(encoder.size(), SerializedFloatValueSize(value));
  }
  return absl::OkStatus();
}

absl::Status SerializeDoubleValue(double value, absl::Cord& serialized_value) {
  if (absl::bit_cast<uint64_t>(value) != 0) {
    ProtoWireEncoder encoder("google.protobuf.FloatValue", serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kFixed64)));
    CEL_RETURN_IF_ERROR(encoder.WriteFixed64(value));
    encoder.EnsureFullyEncoded();
    ABSL_DCHECK_EQ(encoder.size(), SerializedDoubleValueSize(value));
  }
  return absl::OkStatus();
}

absl::Status SerializeValue(const Json& value, absl::Cord& serialized_value) {
  size_t original_size = serialized_value.size();
  CEL_RETURN_IF_ERROR(JsonToAnyValue(value, serialized_value));
  ABSL_DCHECK_EQ(serialized_value.size() - original_size,
                 SerializedValueSize(value));
  return absl::OkStatus();
}

absl::Status SerializeListValue(const JsonArray& value,
                                absl::Cord& serialized_value) {
  size_t original_size = serialized_value.size();
  CEL_RETURN_IF_ERROR(JsonArrayToAnyValue(value, serialized_value));
  ABSL_DCHECK_EQ(serialized_value.size() - original_size,
                 SerializedListValueSize(value));
  return absl::OkStatus();
}

absl::Status SerializeStruct(const JsonObject& value,
                             absl::Cord& serialized_value) {
  size_t original_size = serialized_value.size();
  CEL_RETURN_IF_ERROR(JsonObjectToAnyValue(value, serialized_value));
  ABSL_DCHECK_EQ(serialized_value.size() - original_size,
                 SerializedStructSize(value));
  return absl::OkStatus();
}

}  // namespace cel::internal
