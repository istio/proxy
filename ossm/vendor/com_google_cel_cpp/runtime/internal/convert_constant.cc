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

#include "runtime/internal/convert_constant.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/ast/expr.h"
#include "common/constant.h"
#include "common/value.h"
#include "eval/internal/errors.h"

namespace cel::runtime_internal {
namespace {
using ::cel::Constant;

struct ConvertVisitor {
  Allocator<> allocator;

  absl::StatusOr<cel::Value> operator()(absl::monostate) {
    return absl::InvalidArgumentError("unspecified constant");
  }
  absl::StatusOr<cel::Value> operator()(
      const cel::ast_internal::NullValue& value) {
    return NullValue();
  }
  absl::StatusOr<cel::Value> operator()(bool value) { return BoolValue(value); }
  absl::StatusOr<cel::Value> operator()(int64_t value) {
    return IntValue(value);
  }
  absl::StatusOr<cel::Value> operator()(uint64_t value) {
    return UintValue(value);
  }
  absl::StatusOr<cel::Value> operator()(double value) {
    return DoubleValue(value);
  }
  absl::StatusOr<cel::Value> operator()(const cel::StringConstant& value) {
    return StringValue(allocator, value);
  }
  absl::StatusOr<cel::Value> operator()(const cel::BytesConstant& value) {
    return BytesValue(allocator, value);
  }
  absl::StatusOr<cel::Value> operator()(const absl::Duration duration) {
    if (duration >= kDurationHigh || duration <= kDurationLow) {
      return ErrorValue(*DurationOverflowError());
    }
    return UnsafeDurationValue(duration);
  }
  absl::StatusOr<cel::Value> operator()(const absl::Time timestamp) {
    return UnsafeTimestampValue(timestamp);
  }
};

}  // namespace

// Converts an Ast constant into a runtime value, managed according to the
// given value factory.
//
// A status maybe returned if value creation fails.
absl::StatusOr<Value> ConvertConstant(const Constant& constant,
                                      Allocator<> allocator) {
  return absl::visit(ConvertVisitor{allocator}, constant.constant_kind());
}

}  // namespace cel::runtime_internal
