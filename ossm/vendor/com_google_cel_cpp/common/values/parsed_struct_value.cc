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

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/allocator.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"

namespace cel {

absl::Status ParsedStructValueInterface::Equal(ValueManager& value_manager,
                                               const Value& other,
                                               Value& result) const {
  if (auto parsed_struct_value = As<ParsedStructValue>(other);
      parsed_struct_value.has_value() &&
      NativeTypeId::Of(*this) == NativeTypeId::Of(*parsed_struct_value)) {
    return EqualImpl(value_manager, *parsed_struct_value, result);
  }
  if (auto struct_value = As<StructValue>(other); struct_value.has_value()) {
    return common_internal::StructValueEqual(value_manager, *this,
                                             *struct_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

absl::Status ParsedStructValueInterface::EqualImpl(
    ValueManager& value_manager, const ParsedStructValue& other,
    Value& result) const {
  return common_internal::StructValueEqual(value_manager, *this, other, result);
}

ParsedStructValue ParsedStructValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(!interface_)) {
    return ParsedStructValue();
  }
  if (absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      arena != nullptr &&
      common_internal::GetReferenceCount(interface_) != nullptr) {
    return interface_->Clone(arena);
  }
  return *this;
}

absl::StatusOr<int> ParsedStructValueInterface::Qualify(
    ValueManager&, absl::Span<const SelectQualifier>, bool, Value&) const {
  return absl::UnimplementedError("Qualify not supported.");
}

}  // namespace cel
