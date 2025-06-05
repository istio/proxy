// Copyright 2021 Google LLC
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

#include "runtime/standard/comparison_functions.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {

namespace {

using ::cel::internal::Number;

// Comparison template functions
template <class Type>
bool LessThan(ValueManager&, Type t1, Type t2) {
  return (t1 < t2);
}

template <class Type>
bool LessThanOrEqual(ValueManager&, Type t1, Type t2) {
  return (t1 <= t2);
}

template <class Type>
bool GreaterThan(ValueManager& factory, Type t1, Type t2) {
  return LessThan(factory, t2, t1);
}

template <class Type>
bool GreaterThanOrEqual(ValueManager& factory, Type t1, Type t2) {
  return LessThanOrEqual(factory, t2, t1);
}

// String value comparions specializations
template <>
bool LessThan(ValueManager&, const StringValue& t1, const StringValue& t2) {
  return t1.Compare(t2) < 0;
}

template <>
bool LessThanOrEqual(ValueManager&, const StringValue& t1,
                     const StringValue& t2) {
  return t1.Compare(t2) <= 0;
}

template <>
bool GreaterThan(ValueManager&, const StringValue& t1, const StringValue& t2) {
  return t1.Compare(t2) > 0;
}

template <>
bool GreaterThanOrEqual(ValueManager&, const StringValue& t1,
                        const StringValue& t2) {
  return t1.Compare(t2) >= 0;
}

// bytes value comparions specializations
template <>
bool LessThan(ValueManager&, const BytesValue& t1, const BytesValue& t2) {
  return t1.Compare(t2) < 0;
}

template <>
bool LessThanOrEqual(ValueManager&, const BytesValue& t1,
                     const BytesValue& t2) {
  return t1.Compare(t2) <= 0;
}

template <>
bool GreaterThan(ValueManager&, const BytesValue& t1, const BytesValue& t2) {
  return t1.Compare(t2) > 0;
}

template <>
bool GreaterThanOrEqual(ValueManager&, const BytesValue& t1,
                        const BytesValue& t2) {
  return t1.Compare(t2) >= 0;
}

// Duration comparison specializations
template <>
bool LessThan(ValueManager&, absl::Duration t1, absl::Duration t2) {
  return absl::operator<(t1, t2);
}

template <>
bool LessThanOrEqual(ValueManager&, absl::Duration t1, absl::Duration t2) {
  return absl::operator<=(t1, t2);
}

template <>
bool GreaterThan(ValueManager&, absl::Duration t1, absl::Duration t2) {
  return absl::operator>(t1, t2);
}

template <>
bool GreaterThanOrEqual(ValueManager&, absl::Duration t1, absl::Duration t2) {
  return absl::operator>=(t1, t2);
}

// Timestamp comparison specializations
template <>
bool LessThan(ValueManager&, absl::Time t1, absl::Time t2) {
  return absl::operator<(t1, t2);
}

template <>
bool LessThanOrEqual(ValueManager&, absl::Time t1, absl::Time t2) {
  return absl::operator<=(t1, t2);
}

template <>
bool GreaterThan(ValueManager&, absl::Time t1, absl::Time t2) {
  return absl::operator>(t1, t2);
}

template <>
bool GreaterThanOrEqual(ValueManager&, absl::Time t1, absl::Time t2) {
  return absl::operator>=(t1, t2);
}

template <typename T, typename U>
bool CrossNumericLessThan(ValueManager&, T t, U u) {
  return Number(t) < Number(u);
}

template <typename T, typename U>
bool CrossNumericGreaterThan(ValueManager&, T t, U u) {
  return Number(t) > Number(u);
}

template <typename T, typename U>
bool CrossNumericLessOrEqualTo(ValueManager&, T t, U u) {
  return Number(t) <= Number(u);
}

template <typename T, typename U>
bool CrossNumericGreaterOrEqualTo(ValueManager&, T t, U u) {
  return Number(t) >= Number(u);
}

template <class Type>
absl::Status RegisterComparisonFunctionsForType(
    cel::FunctionRegistry& registry) {
  using FunctionAdapter = BinaryFunctionAdapter<bool, Type, Type>;
  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kLess, false),
      FunctionAdapter::WrapFunction(LessThan<Type>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kLessOrEqual, false),
      FunctionAdapter::WrapFunction(LessThanOrEqual<Type>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kGreater, false),
      FunctionAdapter::WrapFunction(GreaterThan<Type>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kGreaterOrEqual, false),
      FunctionAdapter::WrapFunction(GreaterThanOrEqual<Type>)));

  return absl::OkStatus();
}

absl::Status RegisterHomogenousComparisonFunctions(
    cel::FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<bool>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<int64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<uint64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<double>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<const StringValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<const BytesValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<absl::Duration>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<absl::Time>(registry));

  return absl::OkStatus();
}

template <typename T, typename U>
absl::Status RegisterCrossNumericComparisons(cel::FunctionRegistry& registry) {
  using FunctionAdapter = BinaryFunctionAdapter<bool, T, U>;
  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kLess,
                                        /*receiver_style=*/false),
      FunctionAdapter::WrapFunction(&CrossNumericLessThan<T, U>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kGreater,
                                        /*receiver_style=*/false),
      FunctionAdapter::WrapFunction(&CrossNumericGreaterThan<T, U>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kGreaterOrEqual,
                                        /*receiver_style=*/false),
      FunctionAdapter::WrapFunction(&CrossNumericGreaterOrEqualTo<T, U>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kLessOrEqual,
                                        /*receiver_style=*/false),
      FunctionAdapter::WrapFunction(&CrossNumericLessOrEqualTo<T, U>)));
  return absl::OkStatus();
}

absl::Status RegisterHeterogeneousComparisonFunctions(
    cel::FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<double, int64_t>(registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<double, uint64_t>(registry)));

  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<uint64_t, double>(registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<uint64_t, int64_t>(registry)));

  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<int64_t, uint64_t>(registry)));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<bool>(registry));
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<int64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<uint64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<double>(registry));
  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<const StringValue&>(registry));
  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<const BytesValue&>(registry));
  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<absl::Duration>(registry));
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<absl::Time>(registry));

  return absl::OkStatus();
}
}  // namespace

absl::Status RegisterComparisonFunctions(FunctionRegistry& registry,
                                         const RuntimeOptions& options) {
  if (options.enable_heterogeneous_equality) {
    CEL_RETURN_IF_ERROR(RegisterHeterogeneousComparisonFunctions(registry));
  } else {
    CEL_RETURN_IF_ERROR(RegisterHomogenousComparisonFunctions(registry));
  }
  return absl::OkStatus();
}

}  // namespace cel
