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
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "base/ast_internal/expr.h"
#include "common/constant.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/internal/errors.h"

namespace cel::runtime_internal {
namespace {
using ::cel::ast_internal::Constant;

struct ConvertVisitor {
  cel::ValueManager& value_factory;

  absl::StatusOr<cel::Value> operator()(absl::monostate) {
    return absl::InvalidArgumentError("unspecified constant");
  }
  absl::StatusOr<cel::Value> operator()(
      const cel::ast_internal::NullValue& value) {
    return value_factory.GetNullValue();
  }
  absl::StatusOr<cel::Value> operator()(bool value) {
    return value_factory.CreateBoolValue(value);
  }
  absl::StatusOr<cel::Value> operator()(int64_t value) {
    return value_factory.CreateIntValue(value);
  }
  absl::StatusOr<cel::Value> operator()(uint64_t value) {
    return value_factory.CreateUintValue(value);
  }
  absl::StatusOr<cel::Value> operator()(double value) {
    return value_factory.CreateDoubleValue(value);
  }
  absl::StatusOr<cel::Value> operator()(const cel::StringConstant& value) {
    return value_factory.CreateUncheckedStringValue(value);
  }
  absl::StatusOr<cel::Value> operator()(const cel::BytesConstant& value) {
    return value_factory.CreateBytesValue(value);
  }
  absl::StatusOr<cel::Value> operator()(const absl::Duration duration) {
    if (duration >= kDurationHigh || duration <= kDurationLow) {
      return value_factory.CreateErrorValue(*DurationOverflowError());
    }
    return value_factory.CreateUncheckedDurationValue(duration);
  }
  absl::StatusOr<cel::Value> operator()(const absl::Time timestamp) {
    return value_factory.CreateUncheckedTimestampValue(timestamp);
  }
};

}  // namespace
// Converts an Ast constant into a runtime value, managed according to the
// given value factory.
//
// A status maybe returned if value creation fails.
absl::StatusOr<Value> ConvertConstant(const Constant& constant,
                                      ValueManager& value_factory) {
  return absl::visit(ConvertVisitor{value_factory}, constant.constant_kind());
}

}  // namespace cel::runtime_internal
