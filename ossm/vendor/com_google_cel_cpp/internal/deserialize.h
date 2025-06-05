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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_DESERIALIZE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_DESERIALIZE_H_

#include <cstdint>

#include "google/protobuf/any.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "common/json.h"

namespace cel::internal {

absl::StatusOr<absl::Duration> DeserializeDuration(const absl::Cord& data);

absl::StatusOr<absl::Time> DeserializeTimestamp(const absl::Cord& data);

absl::StatusOr<absl::Cord> DeserializeBytesValue(const absl::Cord& data);

absl::StatusOr<absl::Cord> DeserializeStringValue(const absl::Cord& data);

absl::StatusOr<bool> DeserializeBoolValue(const absl::Cord& data);

absl::StatusOr<int32_t> DeserializeInt32Value(const absl::Cord& data);

absl::StatusOr<int64_t> DeserializeInt64Value(const absl::Cord& data);

absl::StatusOr<uint32_t> DeserializeUInt32Value(const absl::Cord& data);

absl::StatusOr<uint64_t> DeserializeUInt64Value(const absl::Cord& data);

absl::StatusOr<float> DeserializeFloatValue(const absl::Cord& data);

absl::StatusOr<double> DeserializeDoubleValue(const absl::Cord& data);

absl::StatusOr<double> DeserializeFloatValueOrDoubleValue(
    const absl::Cord& data);

absl::StatusOr<Json> DeserializeValue(const absl::Cord& data);

absl::StatusOr<JsonArray> DeserializeListValue(const absl::Cord& data);

absl::StatusOr<JsonObject> DeserializeStruct(const absl::Cord& data);

absl::StatusOr<google::protobuf::Any> DeserializeAny(const absl::Cord& data);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_DESERIALIZE_H_
