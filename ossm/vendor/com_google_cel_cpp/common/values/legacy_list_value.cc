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

#include "common/values/legacy_list_value.h"

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/values/list_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::common_internal {

absl::Status LegacyListValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (auto list_value = other.AsList(); list_value.has_value()) {
    return ListValueEqual(*this, *list_value, descriptor_pool, message_factory,
                          arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

bool IsLegacyListValue(const Value& value) {
  return value.variant_.Is<LegacyListValue>();
}

LegacyListValue GetLegacyListValue(const Value& value) {
  ABSL_DCHECK(IsLegacyListValue(value));
  return value.variant_.Get<LegacyListValue>();
}

absl::optional<LegacyListValue> AsLegacyListValue(const Value& value) {
  if (IsLegacyListValue(value)) {
    return GetLegacyListValue(value);
  }
  if (auto custom_list_value = value.AsCustomList(); custom_list_value) {
    NativeTypeId native_type_id = custom_list_value->GetTypeId();
    if (native_type_id == NativeTypeId::For<CompatListValue>()) {
      return LegacyListValue(
          static_cast<const google::api::expr::runtime::CelList*>(
              cel::internal::down_cast<const CompatListValue*>(
                  custom_list_value->interface())));
    } else if (native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
      return LegacyListValue(
          static_cast<const google::api::expr::runtime::CelList*>(
              cel::internal::down_cast<const MutableCompatListValue*>(
                  custom_list_value->interface())));
    }
  }
  return absl::nullopt;
}

}  // namespace cel::common_internal
