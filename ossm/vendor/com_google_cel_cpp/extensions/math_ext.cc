// Copyright 2022 Google LLC
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

#include "extensions/math_ext.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/base/casts.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

using ::google::api::expr::runtime::CelFunctionRegistry;
using ::google::api::expr::runtime::CelNumber;
using ::google::api::expr::runtime::InterpreterOptions;

static constexpr char kMathMin[] = "math.@min";
static constexpr char kMathMax[] = "math.@max";

struct ToValueVisitor {
  Value operator()(uint64_t v) const { return UintValue{v}; }
  Value operator()(int64_t v) const { return IntValue{v}; }
  Value operator()(double v) const { return DoubleValue{v}; }
};

Value NumberToValue(CelNumber number) {
  return number.visit<Value>(ToValueVisitor{});
}

absl::StatusOr<CelNumber> ValueToNumber(const Value& value,
                                        absl::string_view function) {
  if (auto int_value = As<IntValue>(value); int_value) {
    return CelNumber::FromInt64(int_value->NativeValue());
  }
  if (auto uint_value = As<UintValue>(value); uint_value) {
    return CelNumber::FromUint64(uint_value->NativeValue());
  }
  if (auto double_value = As<DoubleValue>(value); double_value) {
    return CelNumber::FromDouble(double_value->NativeValue());
  }
  return absl::InvalidArgumentError(
      absl::StrCat(function, " arguments must be numeric"));
}

CelNumber MinNumber(CelNumber v1, CelNumber v2) {
  if (v2 < v1) {
    return v2;
  }
  return v1;
}

Value MinValue(CelNumber v1, CelNumber v2) {
  return NumberToValue(MinNumber(v1, v2));
}

template <typename T>
Value Identity(ValueManager&, T v1) {
  return NumberToValue(CelNumber(v1));
}

template <typename T, typename U>
Value Min(ValueManager&, T v1, U v2) {
  return MinValue(CelNumber(v1), CelNumber(v2));
}

absl::StatusOr<Value> MinList(ValueManager& value_manager,
                              const ListValue& values) {
  CEL_ASSIGN_OR_RETURN(auto iterator, values.NewIterator(value_manager));
  if (!iterator->HasNext()) {
    return ErrorValue(
        absl::InvalidArgumentError("math.@min argument must not be empty"));
  }
  Value value;
  CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
  absl::StatusOr<CelNumber> current = ValueToNumber(value, kMathMin);
  if (!current.ok()) {
    return ErrorValue{current.status()};
  }
  CelNumber min = *current;
  while (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
    absl::StatusOr<CelNumber> other = ValueToNumber(value, kMathMin);
    if (!other.ok()) {
      return ErrorValue{other.status()};
    }
    min = MinNumber(min, *other);
  }
  return NumberToValue(min);
}

CelNumber MaxNumber(CelNumber v1, CelNumber v2) {
  if (v2 > v1) {
    return v2;
  }
  return v1;
}

Value MaxValue(CelNumber v1, CelNumber v2) {
  return NumberToValue(MaxNumber(v1, v2));
}

template <typename T, typename U>
Value Max(ValueManager&, T v1, U v2) {
  return MaxValue(CelNumber(v1), CelNumber(v2));
}

absl::StatusOr<Value> MaxList(ValueManager& value_manager,
                              const ListValue& values) {
  CEL_ASSIGN_OR_RETURN(auto iterator, values.NewIterator(value_manager));
  if (!iterator->HasNext()) {
    return ErrorValue(
        absl::InvalidArgumentError("math.@max argument must not be empty"));
  }
  Value value;
  CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
  absl::StatusOr<CelNumber> current = ValueToNumber(value, kMathMax);
  if (!current.ok()) {
    return ErrorValue{current.status()};
  }
  CelNumber min = *current;
  while (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
    absl::StatusOr<CelNumber> other = ValueToNumber(value, kMathMax);
    if (!other.ok()) {
      return ErrorValue{other.status()};
    }
    min = MaxNumber(min, *other);
  }
  return NumberToValue(min);
}

template <typename T, typename U>
absl::Status RegisterCrossNumericMin(FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, T, U>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, T, U>::WrapFunction(Min<T, U>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, U, T>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, U, T>::WrapFunction(Min<U, T>)));

  return absl::OkStatus();
}

template <typename T, typename U>
absl::Status RegisterCrossNumericMax(FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, T, U>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, T, U>::WrapFunction(Max<T, U>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, U, T>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, U, T>::WrapFunction(Max<U, T>)));

  return absl::OkStatus();
}

double CeilDouble(ValueManager&, double value) { return std::ceil(value); }

double FloorDouble(ValueManager&, double value) { return std::floor(value); }

double RoundDouble(ValueManager&, double value) { return std::round(value); }

double TruncDouble(ValueManager&, double value) { return std::trunc(value); }

bool IsInfDouble(ValueManager&, double value) { return std::isinf(value); }

bool IsNaNDouble(ValueManager&, double value) { return std::isnan(value); }

bool IsFiniteDouble(ValueManager&, double value) {
  return std::isfinite(value);
}

double AbsDouble(ValueManager&, double value) { return std::fabs(value); }

Value AbsInt(ValueManager& value_manager, int64_t value) {
  if (ABSL_PREDICT_FALSE(value == std::numeric_limits<int64_t>::min())) {
    return ErrorValue(absl::InvalidArgumentError("integer overflow"));
  }
  return IntValue(value < 0 ? -value : value);
}

uint64_t AbsUint(ValueManager&, uint64_t value) { return value; }

double SignDouble(ValueManager&, double value) {
  if (std::isnan(value)) {
    return value;
  }
  if (value == 0.0) {
    return 0.0;
  }
  return std::signbit(value) ? -1.0 : 1.0;
}

int64_t SignInt(ValueManager&, int64_t value) {
  return value < 0 ? -1 : value > 0 ? 1 : 0;
}

uint64_t SignUint(ValueManager&, uint64_t value) { return value == 0 ? 0 : 1; }

int64_t BitAndInt(ValueManager&, int64_t lhs, int64_t rhs) { return lhs & rhs; }

uint64_t BitAndUint(ValueManager&, uint64_t lhs, uint64_t rhs) {
  return lhs & rhs;
}

int64_t BitOrInt(ValueManager&, int64_t lhs, int64_t rhs) { return lhs | rhs; }

uint64_t BitOrUint(ValueManager&, uint64_t lhs, uint64_t rhs) {
  return lhs | rhs;
}

int64_t BitXorInt(ValueManager&, int64_t lhs, int64_t rhs) { return lhs ^ rhs; }

uint64_t BitXorUint(ValueManager&, uint64_t lhs, uint64_t rhs) {
  return lhs ^ rhs;
}

int64_t BitNotInt(ValueManager&, int64_t value) { return ~value; }

uint64_t BitNotUint(ValueManager&, uint64_t value) { return ~value; }

Value BitShiftLeftInt(ValueManager&, int64_t lhs, int64_t rhs) {
  if (ABSL_PREDICT_FALSE(rhs < 0)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("math.bitShiftLeft() invalid negative shift: ", rhs)));
  }
  if (rhs > 63) {
    return IntValue(0);
  }
  return IntValue(lhs << static_cast<int>(rhs));
}

Value BitShiftLeftUint(ValueManager&, uint64_t lhs, int64_t rhs) {
  if (ABSL_PREDICT_FALSE(rhs < 0)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("math.bitShiftLeft() invalid negative shift: ", rhs)));
  }
  if (rhs > 63) {
    return UintValue(0);
  }
  return UintValue(lhs << static_cast<int>(rhs));
}

Value BitShiftRightInt(ValueManager&, int64_t lhs, int64_t rhs) {
  if (ABSL_PREDICT_FALSE(rhs < 0)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("math.bitShiftRight() invalid negative shift: ", rhs)));
  }
  if (rhs > 63) {
    return IntValue(0);
  }
  // We do not perform a sign extension shift, per the spec we just do the same
  // thing as uint.
  return IntValue(absl::bit_cast<int64_t>(absl::bit_cast<uint64_t>(lhs) >>
                                          static_cast<int>(rhs)));
}

Value BitShiftRightUint(ValueManager&, uint64_t lhs, int64_t rhs) {
  if (ABSL_PREDICT_FALSE(rhs < 0)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("math.bitShiftRight() invalid negative shift: ", rhs)));
  }
  if (rhs > 63) {
    return UintValue(0);
  }
  return UintValue(lhs >> static_cast<int>(rhs));
}

}  // namespace

absl::Status RegisterMathExtensionFunctions(FunctionRegistry& registry,
                                            const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(Identity<int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, double>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, double>::WrapFunction(Identity<double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, uint64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, uint64_t>::WrapFunction(Identity<uint64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          Min<int64_t, int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, double, double>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, double, double>::WrapFunction(
          Min<double, double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::WrapFunction(
          Min<uint64_t, uint64_t>)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<int64_t, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<double, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          kMathMin, false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          MinList)));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(Identity<int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, double>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, double>::WrapFunction(Identity<double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, uint64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, uint64_t>::WrapFunction(Identity<uint64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          Max<int64_t, int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, double, double>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, double, double>::WrapFunction(
          Max<double, double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::WrapFunction(
          Max<uint64_t, uint64_t>)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<int64_t, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<double, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          kMathMax, false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          MaxList)));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(
          "math.ceil", /*receiver_style=*/false),
      UnaryFunctionAdapter<double, double>::WrapFunction(CeilDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(
          "math.floor", /*receiver_style=*/false),
      UnaryFunctionAdapter<double, double>::WrapFunction(FloorDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(
          "math.round", /*receiver_style=*/false),
      UnaryFunctionAdapter<double, double>::WrapFunction(RoundDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(
          "math.trunc", /*receiver_style=*/false),
      UnaryFunctionAdapter<double, double>::WrapFunction(TruncDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<bool, double>::CreateDescriptor(
          "math.isInf", /*receiver_style=*/false),
      UnaryFunctionAdapter<bool, double>::WrapFunction(IsInfDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<bool, double>::CreateDescriptor(
          "math.isNaN", /*receiver_style=*/false),
      UnaryFunctionAdapter<bool, double>::WrapFunction(IsNaNDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<bool, double>::CreateDescriptor(
          "math.isFinite", /*receiver_style=*/false),
      UnaryFunctionAdapter<bool, double>::WrapFunction(IsFiniteDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(
          "math.abs", /*receiver_style=*/false),
      UnaryFunctionAdapter<double, double>::WrapFunction(AbsDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          "math.abs", /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(AbsInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<uint64_t, uint64_t>::CreateDescriptor(
          "math.abs", /*receiver_style=*/false),
      UnaryFunctionAdapter<uint64_t, uint64_t>::WrapFunction(AbsUint)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(
          "math.sign", /*receiver_style=*/false),
      UnaryFunctionAdapter<double, double>::WrapFunction(SignDouble)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<int64_t, int64_t>::CreateDescriptor(
          "math.sign", /*receiver_style=*/false),
      UnaryFunctionAdapter<int64_t, int64_t>::WrapFunction(SignInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<uint64_t, uint64_t>::CreateDescriptor(
          "math.sign", /*receiver_style=*/false),
      UnaryFunctionAdapter<uint64_t, uint64_t>::WrapFunction(SignUint)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<int64_t, int64_t, int64_t>::CreateDescriptor(
          "math.bitAnd", /*receiver_style=*/false),
      BinaryFunctionAdapter<int64_t, int64_t, int64_t>::WrapFunction(
          BitAndInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>::CreateDescriptor(
          "math.bitAnd", /*receiver_style=*/false),
      BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>::WrapFunction(
          BitAndUint)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<int64_t, int64_t, int64_t>::CreateDescriptor(
          "math.bitOr", /*receiver_style=*/false),
      BinaryFunctionAdapter<int64_t, int64_t, int64_t>::WrapFunction(
          BitOrInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>::CreateDescriptor(
          "math.bitOr", /*receiver_style=*/false),
      BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>::WrapFunction(
          BitOrUint)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<int64_t, int64_t, int64_t>::CreateDescriptor(
          "math.bitXor", /*receiver_style=*/false),
      BinaryFunctionAdapter<int64_t, int64_t, int64_t>::WrapFunction(
          BitXorInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>::CreateDescriptor(
          "math.bitXor", /*receiver_style=*/false),
      BinaryFunctionAdapter<uint64_t, uint64_t, uint64_t>::WrapFunction(
          BitXorUint)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<int64_t, int64_t>::CreateDescriptor(
          "math.bitNot", /*receiver_style=*/false),
      UnaryFunctionAdapter<int64_t, int64_t>::WrapFunction(BitNotInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<uint64_t, uint64_t>::CreateDescriptor(
          "math.bitNot", /*receiver_style=*/false),
      UnaryFunctionAdapter<uint64_t, uint64_t>::WrapFunction(BitNotUint)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          "math.bitShiftLeft", /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          BitShiftLeftInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, int64_t>::CreateDescriptor(
          "math.bitShiftLeft", /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, uint64_t, int64_t>::WrapFunction(
          BitShiftLeftUint)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          "math.bitShiftRight", /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          BitShiftRightInt)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, int64_t>::CreateDescriptor(
          "math.bitShiftRight", /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, uint64_t, int64_t>::WrapFunction(
          BitShiftRightUint)));

  return absl::OkStatus();
}

absl::Status RegisterMathExtensionFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions& options) {
  return RegisterMathExtensionFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
