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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_SERIALIZE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_SERIALIZE_H_

#include <cstddef>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/json.h"

namespace cel::internal {

absl::Status SerializeDuration(absl::Duration value,
                               absl::Cord& serialized_value);

absl::Status SerializeTimestamp(absl::Time value, absl::Cord& serialized_value);

absl::Status SerializeBytesValue(const absl::Cord& value,
                                 absl::Cord& serialized_value);

absl::Status SerializeBytesValue(absl::string_view value,
                                 absl::Cord& serialized_value);

absl::Status SerializeStringValue(const absl::Cord& value,
                                  absl::Cord& serialized_value);

absl::Status SerializeStringValue(absl::string_view value,
                                  absl::Cord& serialized_value);

absl::Status SerializeBoolValue(bool value, absl::Cord& serialized_value);

absl::Status SerializeInt32Value(int32_t value, absl::Cord& serialized_value);

absl::Status SerializeInt64Value(int64_t value, absl::Cord& serialized_value);

absl::Status SerializeUInt32Value(uint32_t value, absl::Cord& serialized_value);

absl::Status SerializeUInt64Value(uint64_t value, absl::Cord& serialized_value);

absl::Status SerializeFloatValue(float value, absl::Cord& serialized_value);

absl::Status SerializeDoubleValue(double value, absl::Cord& serialized_value);

absl::Status SerializeValue(const Json& value, absl::Cord& serialized_value);

absl::Status SerializeListValue(const JsonArray& value,
                                absl::Cord& serialized_value);

absl::Status SerializeStruct(const JsonObject& value,
                             absl::Cord& serialized_value);

size_t SerializedDurationSize(absl::Duration value);

size_t SerializedTimestampSize(absl::Time value);

size_t SerializedBytesValueSize(const absl::Cord& value);

size_t SerializedBytesValueSize(absl::string_view value);

size_t SerializedStringValueSize(const absl::Cord& value);

size_t SerializedStringValueSize(absl::string_view value);

size_t SerializedBoolValueSize(bool value);

size_t SerializedInt32ValueSize(int32_t value);

size_t SerializedInt64ValueSize(int64_t value);

size_t SerializedUInt32ValueSize(uint32_t value);

size_t SerializedUInt64ValueSize(uint64_t value);

size_t SerializedFloatValueSize(float value);

size_t SerializedDoubleValueSize(double value);

size_t SerializedValueSize(const Json& value);

size_t SerializedListValueSize(const JsonArray& value);

size_t SerializedStructSize(const JsonObject& value);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_SERIALIZE_H_
