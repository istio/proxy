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

#include "common/values/legacy_map_value.h"

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/values/map_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::common_internal {

absl::Status LegacyMapValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (auto map_value = other.AsMap(); map_value.has_value()) {
    return MapValueEqual(*this, *map_value, descriptor_pool, message_factory,
                         arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

bool IsLegacyMapValue(const Value& value) {
  return value.variant_.Is<LegacyMapValue>();
}

LegacyMapValue GetLegacyMapValue(const Value& value) {
  ABSL_DCHECK(IsLegacyMapValue(value));
  return value.variant_.Get<LegacyMapValue>();
}

absl::optional<LegacyMapValue> AsLegacyMapValue(const Value& value) {
  if (IsLegacyMapValue(value)) {
    return GetLegacyMapValue(value);
  }
  if (auto custom_map_value = value.AsCustomMap(); custom_map_value) {
    NativeTypeId native_type_id = NativeTypeId::Of(*custom_map_value);
    if (native_type_id == NativeTypeId::For<CompatMapValue>()) {
      return LegacyMapValue(
          static_cast<const google::api::expr::runtime::CelMap*>(
              cel::internal::down_cast<const CompatMapValue*>(
                  custom_map_value->interface())));
    } else if (native_type_id == NativeTypeId::For<MutableCompatMapValue>()) {
      return LegacyMapValue(
          static_cast<const google::api::expr::runtime::CelMap*>(
              cel::internal::down_cast<const MutableCompatMapValue*>(
                  custom_map_value->interface())));
    }
  }
  return absl::nullopt;
}

}  // namespace cel::common_internal
