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

#include <cstddef>
#include <cstdint>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/list_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/casts.h"

namespace cel::common_internal {

absl::Status LegacyListValue::ForEach(ValueManager& value_manager,
                                      ForEachCallback callback) const {
  return ForEach(
      value_manager,
      [callback](size_t, const Value& value) -> absl::StatusOr<bool> {
        return callback(value);
      });
}

absl::Status LegacyListValue::Equal(ValueManager& value_manager,
                                    const Value& other, Value& result) const {
  if (auto list_value = As<ListValue>(other); list_value.has_value()) {
    return ListValueEqual(value_manager, *this, *list_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

bool IsLegacyListValue(const Value& value) {
  return absl::holds_alternative<LegacyListValue>(value.variant_);
}

LegacyListValue GetLegacyListValue(const Value& value) {
  ABSL_DCHECK(IsLegacyListValue(value));
  return absl::get<LegacyListValue>(value.variant_);
}

absl::optional<LegacyListValue> AsLegacyListValue(const Value& value) {
  if (IsLegacyListValue(value)) {
    return GetLegacyListValue(value);
  }
  if (auto parsed_list_value = value.AsParsedList(); parsed_list_value) {
    NativeTypeId native_type_id = NativeTypeId::Of(*parsed_list_value);
    if (native_type_id == NativeTypeId::For<CompatListValue>()) {
      return LegacyListValue(reinterpret_cast<uintptr_t>(
          static_cast<const google::api::expr::runtime::CelList*>(
              cel::internal::down_cast<const CompatListValue*>(
                  (*parsed_list_value).operator->()))));
    } else if (native_type_id == NativeTypeId::For<MutableCompatListValue>()) {
      return LegacyListValue(reinterpret_cast<uintptr_t>(
          static_cast<const google::api::expr::runtime::CelList*>(
              cel::internal::down_cast<const MutableCompatListValue*>(
                  (*parsed_list_value).operator->()))));
    }
  }
  return absl::nullopt;
}

}  // namespace cel::common_internal
