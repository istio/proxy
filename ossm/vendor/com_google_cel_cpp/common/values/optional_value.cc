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

#include <string>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/allocator.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_kind.h"

namespace cel {

namespace {

class EmptyOptionalValue final : public OptionalValueInterface {
 public:
  EmptyOptionalValue() = default;

  OpaqueValue Clone(ArenaAllocator<>) const override { return OptionalValue(); }

  bool HasValue() const override { return false; }

  void Value(cel::Value& result) const override {
    result = ErrorValue(
        absl::FailedPreconditionError("optional.none() dereference"));
  }
};

class FullOptionalValue final : public OptionalValueInterface {
 public:
  explicit FullOptionalValue(cel::Value value) : value_(std::move(value)) {}

  OpaqueValue Clone(ArenaAllocator<> allocator) const override {
    return MemoryManager(allocator).MakeShared<FullOptionalValue>(
        value_.Clone(allocator));
  }

  bool HasValue() const override { return true; }

  void Value(cel::Value& result) const override { result = value_; }

 private:
  friend struct NativeTypeTraits<FullOptionalValue>;

  const cel::Value value_;
};

}  // namespace

template <>
struct NativeTypeTraits<FullOptionalValue> {
  static bool SkipDestructor(const FullOptionalValue& value) {
    return NativeType::SkipDestructor(value.value_);
  }
};

std::string OptionalValueInterface::DebugString() const {
  if (HasValue()) {
    return absl::StrCat("optional(", Value().DebugString(), ")");
  }
  return "optional.none()";
}

OptionalValue OptionalValue::Of(MemoryManagerRef memory_manager,
                                cel::Value value) {
  ABSL_DCHECK(value.kind() != ValueKind::kError &&
              value.kind() != ValueKind::kUnknown);
  return OptionalValue(
      memory_manager.MakeShared<FullOptionalValue>(std::move(value)));
}

OptionalValue OptionalValue::None() {
  static const absl::NoDestructor<EmptyOptionalValue> empty;
  return OptionalValue(common_internal::MakeShared(&*empty, nullptr));
}

absl::Status OptionalValueInterface::Equal(ValueManager& value_manager,
                                           const cel::Value& other,
                                           cel::Value& result) const {
  if (auto other_value = As<OptionalValue>(other); other_value.has_value()) {
    if (HasValue() != other_value->HasValue()) {
      result = BoolValue{false};
      return absl::OkStatus();
    }
    if (!HasValue()) {
      result = BoolValue{true};
      return absl::OkStatus();
    }
    return Value().Equal(value_manager, other_value->Value(), result);
    return absl::OkStatus();
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

}  // namespace cel
