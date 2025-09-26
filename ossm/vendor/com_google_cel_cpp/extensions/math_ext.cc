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
#include "absl/base/nullability.h"
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
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

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
Value Identity(T v1) {
  return NumberToValue(CelNumber(v1));
}

template <typename T, typename U>
Value Min(T v1, U v2) {
  return MinValue(CelNumber(v1), CelNumber(v2));
}

absl::StatusOr<Value> MinList(
    const ListValue& values,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(auto iterator, values.NewIterator());
  if (!iterator->HasNext()) {
    return ErrorValue(
        absl::InvalidArgumentError("math.@min argument must not be empty"));
  }
  Value value;
  CEL_RETURN_IF_ERROR(
      iterator->Next(descriptor_pool, message_factory, arena, &value));
  absl::StatusOr<CelNumber> current = ValueToNumber(value, kMathMin);
  if (!current.ok()) {
    return ErrorValue{current.status()};
  }
  CelNumber min = *current;
  while (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(
        iterator->Next(descriptor_pool, message_factory, arena, &value));
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
Value Max(T v1, U v2) {
  return MaxValue(CelNumber(v1), CelNumber(v2));
}

absl::StatusOr<Value> MaxList(
    const ListValue& values,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(auto iterator, values.NewIterator());
  if (!iterator->HasNext()) {
    return ErrorValue(
        absl::InvalidArgumentError("math.@max argument must not be empty"));
  }
  Value value;
  CEL_RETURN_IF_ERROR(
      iterator->Next(descriptor_pool, message_factory, arena, &value));
  absl::StatusOr<CelNumber> current = ValueToNumber(value, kMathMax);
  if (!current.ok()) {
    return ErrorValue{current.status()};
  }
  CelNumber min = *current;
  while (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(
        iterator->Next(descriptor_pool, message_factory, arena, &value));
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
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, T, U>::RegisterGlobalOverload(
          kMathMin, Min<T, U>, registry)));

  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, U, T>::RegisterGlobalOverload(
          kMathMin, Min<U, T>, registry)));

  return absl::OkStatus();
}

template <typename T, typename U>
absl::Status RegisterCrossNumericMax(FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, T, U>::RegisterGlobalOverload(
          kMathMax, Max<T, U>, registry)));

  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, U, T>::RegisterGlobalOverload(
          kMathMax, Max<U, T>, registry)));

  return absl::OkStatus();
}

double CeilDouble(double value) { return std::ceil(value); }

double FloorDouble(double value) { return std::floor(value); }

double RoundDouble(double value) { return std::round(value); }

double TruncDouble(double value) { return std::trunc(value); }

double SqrtDouble(double value) { return std::sqrt(value); }

double SqrtInt(int64_t value) { return std::sqrt(value); }

double SqrtUint(uint64_t value) { return std::sqrt(value); }

bool IsInfDouble(double value) { return std::isinf(value); }

bool IsNaNDouble(double value) { return std::isnan(value); }

bool IsFiniteDouble(double value) { return std::isfinite(value); }

double AbsDouble(double value) { return std::fabs(value); }

Value AbsInt(int64_t value) {
  if (ABSL_PREDICT_FALSE(value == std::numeric_limits<int64_t>::min())) {
    return ErrorValue(absl::InvalidArgumentError("integer overflow"));
  }
  return IntValue(value < 0 ? -value : value);
}

uint64_t AbsUint(uint64_t value) { return value; }

double SignDouble(double value) {
  if (std::isnan(value)) {
    return value;
  }
  if (value == 0.0) {
    return 0.0;
  }
  return std::signbit(value) ? -1.0 : 1.0;
}

int64_t SignInt(int64_t value) { return value < 0 ? -1 : value > 0 ? 1 : 0; }

uint64_t SignUint(uint64_t value) { return value == 0 ? 0 : 1; }

int64_t BitAndInt(int64_t lhs, int64_t rhs) { return lhs & rhs; }

uint64_t BitAndUint(uint64_t lhs, uint64_t rhs) { return lhs & rhs; }

int64_t BitOrInt(int64_t lhs, int64_t rhs) { return lhs | rhs; }

uint64_t BitOrUint(uint64_t lhs, uint64_t rhs) { return lhs | rhs; }

int64_t BitXorInt(int64_t lhs, int64_t rhs) { return lhs ^ rhs; }

uint64_t BitXorUint(uint64_t lhs, uint64_t rhs) { return lhs ^ rhs; }

int64_t BitNotInt(int64_t value) { return ~value; }

uint64_t BitNotUint(uint64_t value) { return ~value; }

Value BitShiftLeftInt(int64_t lhs, int64_t rhs) {
  if (ABSL_PREDICT_FALSE(rhs < 0)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("math.bitShiftLeft() invalid negative shift: ", rhs)));
  }
  if (rhs > 63) {
    return IntValue(0);
  }
  return IntValue(lhs << static_cast<int>(rhs));
}

Value BitShiftLeftUint(uint64_t lhs, int64_t rhs) {
  if (ABSL_PREDICT_FALSE(rhs < 0)) {
    return ErrorValue(absl::InvalidArgumentError(
        absl::StrCat("math.bitShiftLeft() invalid negative shift: ", rhs)));
  }
  if (rhs > 63) {
    return UintValue(0);
  }
  return UintValue(lhs << static_cast<int>(rhs));
}

Value BitShiftRightInt(int64_t lhs, int64_t rhs) {
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

Value BitShiftRightUint(uint64_t lhs, int64_t rhs) {
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
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, int64_t>::RegisterGlobalOverload(
          kMathMin, Identity<int64_t>, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, double>::RegisterGlobalOverload(
          kMathMin, Identity<double>, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, uint64_t>::RegisterGlobalOverload(
          kMathMin, Identity<uint64_t>, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, int64_t, int64_t>::RegisterGlobalOverload(
          kMathMin, Min<int64_t, int64_t>, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, double, double>::RegisterGlobalOverload(
          kMathMin, Min<double, double>, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, uint64_t, uint64_t>::RegisterGlobalOverload(
          kMathMin, Min<uint64_t, uint64_t>, registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<int64_t, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<double, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           ListValue>::RegisterGlobalOverload(kMathMin, MinList,
                                                              registry)));

  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, int64_t>::RegisterGlobalOverload(
          kMathMax, Identity<int64_t>, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, double>::RegisterGlobalOverload(
          kMathMax, Identity<double>, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, uint64_t>::RegisterGlobalOverload(
          kMathMax, Identity<uint64_t>, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, int64_t, int64_t>::RegisterGlobalOverload(
          kMathMax, Max<int64_t, int64_t>, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, double, double>::RegisterGlobalOverload(
          kMathMax, Max<double, double>, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, uint64_t, uint64_t>::RegisterGlobalOverload(
          kMathMax, Max<uint64_t, uint64_t>, registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<int64_t, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<double, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((
      UnaryFunctionAdapter<absl::StatusOr<Value>,
                           ListValue>::RegisterGlobalOverload(kMathMax, MaxList,
                                                              registry)));

  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.ceil", CeilDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.floor", FloorDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.round", RoundDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.sqrt", SqrtDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, int64_t>::RegisterGlobalOverload(
          "math.sqrt", SqrtInt, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, uint64_t>::RegisterGlobalOverload(
          "math.sqrt", SqrtUint, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.trunc", TruncDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<bool, double>::RegisterGlobalOverload(
          "math.isInf", IsInfDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<bool, double>::RegisterGlobalOverload(
          "math.isNaN", IsNaNDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<bool, double>::RegisterGlobalOverload(
          "math.isFinite", IsFiniteDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.abs", AbsDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<Value, int64_t>::RegisterGlobalOverload(
          "math.abs", AbsInt, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<uint64_t, uint64_t>::RegisterGlobalOverload(
          "math.abs", AbsUint, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<double, double>::RegisterGlobalOverload(
          "math.sign", SignDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<int64_t, int64_t>::RegisterGlobalOverload(
          "math.sign", SignInt, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<uint64_t, uint64_t>::RegisterGlobalOverload(
          "math.sign", SignUint, registry)));

  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<int64_t, int64_t, int64_t>::RegisterGlobalOverload(
          "math.bitAnd", BitAndInt, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<uint64_t, uint64_t,
                             uint64_t>::RegisterGlobalOverload("math.bitAnd",
                                                               BitAndUint,
                                                               registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<int64_t, int64_t, int64_t>::RegisterGlobalOverload(
          "math.bitOr", BitOrInt, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<uint64_t, uint64_t,
                             uint64_t>::RegisterGlobalOverload("math.bitOr",
                                                               BitOrUint,
                                                               registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<int64_t, int64_t, int64_t>::RegisterGlobalOverload(
          "math.bitXor", BitXorInt, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<uint64_t, uint64_t,
                             uint64_t>::RegisterGlobalOverload("math.bitXor",
                                                               BitXorUint,
                                                               registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<int64_t, int64_t>::RegisterGlobalOverload(
          "math.bitNot", BitNotInt, registry)));
  CEL_RETURN_IF_ERROR(
      (UnaryFunctionAdapter<uint64_t, uint64_t>::RegisterGlobalOverload(
          "math.bitNot", BitNotUint, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, int64_t, int64_t>::RegisterGlobalOverload(
          "math.bitShiftLeft", BitShiftLeftInt, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, uint64_t, int64_t>::RegisterGlobalOverload(
          "math.bitShiftLeft", BitShiftLeftUint, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, int64_t, int64_t>::RegisterGlobalOverload(
          "math.bitShiftRight", BitShiftRightInt, registry)));
  CEL_RETURN_IF_ERROR(
      (BinaryFunctionAdapter<Value, uint64_t, int64_t>::RegisterGlobalOverload(
          "math.bitShiftRight", BitShiftRightUint, registry)));

  return absl::OkStatus();
}

absl::Status RegisterMathExtensionFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions& options) {
  return RegisterMathExtensionFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
