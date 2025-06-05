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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/struct_value_interface.h"
#include "runtime/runtime_options.h"

namespace cel {

class Value;
class ValueManager;
class TypeManager;

namespace common_internal {

class LegacyStructValue;

// `LegacyStructValue` is a wrapper around the old representation of protocol
// buffer messages in `google::api::expr::runtime::CelValue`. It only supports
// arena allocation.
class LegacyStructValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kStruct;

  LegacyStructValue(uintptr_t message_ptr, uintptr_t type_info)
      : message_ptr_(message_ptr), type_info_(type_info) {}

  LegacyStructValue(const LegacyStructValue&) = default;
  LegacyStructValue& operator=(const LegacyStructValue&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& value_manager,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& value_manager) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;

  bool IsZeroValue() const;

  void swap(LegacyStructValue& other) noexcept {
    using std::swap;
    swap(message_ptr_, other.message_ptr_);
    swap(type_info_, other.type_info_);
  }

  absl::Status GetFieldByName(ValueManager& value_manager,
                              absl::string_view name, Value& result,
                              ProtoWrapperTypeOptions unboxing_options =
                                  ProtoWrapperTypeOptions::kUnsetNull) const;

  absl::Status GetFieldByNumber(ValueManager& value_manager, int64_t number,
                                Value& result,
                                ProtoWrapperTypeOptions unboxing_options =
                                    ProtoWrapperTypeOptions::kUnsetNull) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<int> Qualify(ValueManager& value_manager,
                              absl::Span<const SelectQualifier> qualifiers,
                              bool presence_test, Value& result) const;

  uintptr_t message_ptr() const { return message_ptr_; }

  uintptr_t legacy_type_info() const { return type_info_; }

 private:
  uintptr_t message_ptr_;
  uintptr_t type_info_;
};

inline void swap(LegacyStructValue& lhs, LegacyStructValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const LegacyStructValue& value) {
  return out << value.DebugString();
}

bool IsLegacyStructValue(const Value& value);

LegacyStructValue GetLegacyStructValue(const Value& value);

absl::optional<LegacyStructValue> AsLegacyStructValue(const Value& value);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_STRUCT_VALUE_H_
