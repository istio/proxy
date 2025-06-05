// Copyright 2024 Google LLC
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

#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "google/protobuf/arena.h"

namespace cel {

// Code below assumes OptionalValue has the same layout as OpaqueValue.
static_assert(std::is_base_of_v<OpaqueValue, OptionalValue>);
static_assert(sizeof(OpaqueValue) == sizeof(OptionalValue));
static_assert(alignof(OpaqueValue) == alignof(OptionalValue));

OpaqueValue OpaqueValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(!interface_)) {
    return OpaqueValue();
  }
  // Shared does not keep track of the allocating arena. We need to upgrade it
  // to Owned. For now we only copy if this is reference counted and the target
  // is an arena allocator.
  if (absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      arena != nullptr &&
      common_internal::GetReferenceCount(interface_) != nullptr) {
    return interface_->Clone(arena);
  }
  return *this;
}

bool OpaqueValue::IsOptional() const {
  return NativeTypeId::Of(*interface_) ==
         NativeTypeId::For<OptionalValueInterface>();
}

optional_ref<const OptionalValue> OpaqueValue::AsOptional() const& {
  if (IsOptional()) {
    return *reinterpret_cast<const OptionalValue*>(this);
  }
  return absl::nullopt;
}

absl::optional<OptionalValue> OpaqueValue::AsOptional() && {
  if (IsOptional()) {
    return std::move(*reinterpret_cast<OptionalValue*>(this));
  }
  return absl::nullopt;
}

const OptionalValue& OpaqueValue::GetOptional() const& {
  ABSL_DCHECK(IsOptional()) << *this;
  return *reinterpret_cast<const OptionalValue*>(this);
}

OptionalValue OpaqueValue::GetOptional() && {
  ABSL_DCHECK(IsOptional()) << *this;
  return std::move(*reinterpret_cast<OptionalValue*>(this));
}

}  // namespace cel
