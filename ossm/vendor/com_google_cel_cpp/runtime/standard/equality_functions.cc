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

#include "runtime/standard/equality_functions.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/internal/errors.h"
#include "runtime/register_function_helper.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::cel::builtin::kEqual;
using ::cel::builtin::kInequal;
using ::cel::internal::Number;

// Declaration for the functors for generic equality operator.
// Equal only defined for same-typed values.
// Nullopt is returned if equality is not defined.
struct HomogenousEqualProvider {
  static constexpr bool kIsHeterogeneous = false;
  absl::StatusOr<absl::optional<bool>> operator()(
      const Value& lhs, const Value& rhs,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;
};

// Equal defined between compatible types.
// Nullopt is returned if equality is not defined.
struct HeterogeneousEqualProvider {
  static constexpr bool kIsHeterogeneous = true;

  absl::StatusOr<absl::optional<bool>> operator()(
      const Value& lhs, const Value& rhs,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;
};

// Comparison template functions
template <class Type>
absl::optional<bool> Inequal(Type lhs, Type rhs) {
  return lhs != rhs;
}

template <>
absl::optional<bool> Inequal(const StringValue& lhs, const StringValue& rhs) {
  return !lhs.Equals(rhs);
}

template <>
absl::optional<bool> Inequal(const BytesValue& lhs, const BytesValue& rhs) {
  return !lhs.Equals(rhs);
}

template <>
absl::optional<bool> Inequal(const NullValue&, const NullValue&) {
  return false;
}

template <>
absl::optional<bool> Inequal(const TypeValue& lhs, const TypeValue& rhs) {
  return lhs.name() != rhs.name();
}

template <class Type>
absl::optional<bool> Equal(Type lhs, Type rhs) {
  return lhs == rhs;
}

template <>
absl::optional<bool> Equal(const StringValue& lhs, const StringValue& rhs) {
  return lhs.Equals(rhs);
}

template <>
absl::optional<bool> Equal(const BytesValue& lhs, const BytesValue& rhs) {
  return lhs.Equals(rhs);
}

template <>
absl::optional<bool> Equal(const NullValue&, const NullValue&) {
  return true;
}

template <>
absl::optional<bool> Equal(const TypeValue& lhs, const TypeValue& rhs) {
  return lhs.name() == rhs.name();
}

// Equality for lists. Template parameter provides either heterogeneous or
// homogenous equality for comparing members.
template <typename EqualsProvider>
absl::StatusOr<absl::optional<bool>> ListEqual(
    const ListValue& lhs, const ListValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (&lhs == &rhs) {
    return true;
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_size, lhs.Size());
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    return false;
  }

  for (int i = 0; i < lhs_size; ++i) {
    CEL_ASSIGN_OR_RETURN(auto lhs_i,
                         lhs.Get(i, descriptor_pool, message_factory, arena));
    CEL_ASSIGN_OR_RETURN(auto rhs_i,
                         rhs.Get(i, descriptor_pool, message_factory, arena));
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> eq,
                         EqualsProvider()(lhs_i, rhs_i, descriptor_pool,
                                          message_factory, arena));
    if (!eq.has_value() || !*eq) {
      return eq;
    }
  }
  return true;
}

// Opaque types only support heterogeneous equality, and by extension that means
// optionals. Heterogeneous equality being enabled is enforced by
// `EnableOptionalTypes`.
absl::StatusOr<absl::optional<bool>> OpaqueEqual(
    const OpaqueValue& lhs, const OpaqueValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  Value result;
  CEL_RETURN_IF_ERROR(
      lhs.Equal(rhs, descriptor_pool, message_factory, arena, &result));
  if (auto bool_value = result.AsBool(); bool_value) {
    return bool_value->NativeValue();
  }
  return TypeConversionError(result.GetTypeName(), "bool").NativeValue();
}

absl::optional<Number> NumberFromValue(const Value& value) {
  if (value.Is<IntValue>()) {
    return Number::FromInt64(value.GetInt().NativeValue());
  } else if (value.Is<UintValue>()) {
    return Number::FromUint64(value.GetUint().NativeValue());
  } else if (value.Is<DoubleValue>()) {
    return Number::FromDouble(value.GetDouble().NativeValue());
  }

  return absl::nullopt;
}

absl::StatusOr<absl::optional<Value>> CheckAlternativeNumericType(
    const Value& key, const MapValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  absl::optional<Number> number = NumberFromValue(key);

  if (!number.has_value()) {
    return absl::nullopt;
  }

  if (!key.IsInt() && number->LosslessConvertibleToInt()) {
    absl::optional<Value> entry;
    CEL_ASSIGN_OR_RETURN(entry,
                         rhs.Find(IntValue(number->AsInt()), descriptor_pool,
                                  message_factory, arena));
    if (entry) {
      return entry;
    }
  }

  if (!key.IsUint() && number->LosslessConvertibleToUint()) {
    absl::optional<Value> entry;
    CEL_ASSIGN_OR_RETURN(entry,
                         rhs.Find(UintValue(number->AsUint()), descriptor_pool,
                                  message_factory, arena));
    if (entry) {
      return entry;
    }
  }

  return absl::nullopt;
}

// Equality for maps. Template parameter provides either heterogeneous or
// homogenous equality for comparing values.
template <typename EqualsProvider>
absl::StatusOr<absl::optional<bool>> MapEqual(
    const MapValue& lhs, const MapValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (&lhs == &rhs) {
    return true;
  }
  if (lhs.Size() != rhs.Size()) {
    return false;
  }

  CEL_ASSIGN_OR_RETURN(auto iter, lhs.NewIterator());

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto lhs_key,
                         iter->Next(descriptor_pool, message_factory, arena));

    absl::optional<Value> entry;
    CEL_ASSIGN_OR_RETURN(
        entry, rhs.Find(lhs_key, descriptor_pool, message_factory, arena));

    if (!entry && EqualsProvider::kIsHeterogeneous) {
      CEL_ASSIGN_OR_RETURN(
          entry, CheckAlternativeNumericType(lhs_key, rhs, descriptor_pool,
                                             message_factory, arena));
    }
    if (!entry) {
      return false;
    }

    CEL_ASSIGN_OR_RETURN(auto lhs_value, lhs.Get(lhs_key, descriptor_pool,
                                                 message_factory, arena));
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> eq,
                         EqualsProvider()(lhs_value, *entry, descriptor_pool,
                                          message_factory, arena));

    if (!eq.has_value() || !*eq) {
      return eq;
    }
  }

  return true;
}

// Helper for wrapping ==/!= implementations.
// Name should point to a static constexpr string so the lambda capture is safe.
template <typename Type, typename Op>
std::function<Value(Type, Type, const google::protobuf::DescriptorPool* absl_nonnull,
                    google::protobuf::MessageFactory* absl_nonnull,
                    google::protobuf::Arena* absl_nonnull)>
WrapComparison(Op op, absl::string_view name) {
  return [op = std::move(op), name](
             Type lhs, Type rhs,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena) -> Value {
    absl::optional<bool> result = op(lhs, rhs);

    if (result.has_value()) {
      return BoolValue(*result);
    }

    return ErrorValue(
        cel::runtime_internal::CreateNoMatchingOverloadError(name));
  };
}

// Helper method
//
// Registers all equality functions for template parameters type.
template <class Type>
absl::Status RegisterEqualityFunctionsForType(cel::FunctionRegistry& registry) {
  using FunctionAdapter =
      cel::RegisterHelper<BinaryFunctionAdapter<Value, Type, Type>>;
  // Inequality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kInequal, WrapComparison<Type>(&Inequal<Type>, kInequal), registry));

  // Equality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kEqual, WrapComparison<Type>(&Equal<Type>, kEqual), registry));

  return absl::OkStatus();
}

template <typename Type, typename Op>
auto ComplexEquality(Op&& op) {
  return [op = std::forward<Op>(op)](
             const Type& t1, const Type& t2,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena) -> absl::StatusOr<Value> {
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> result,
                         op(t1, t2, descriptor_pool, message_factory, arena));
    if (!result.has_value()) {
      return ErrorValue(
          cel::runtime_internal::CreateNoMatchingOverloadError(kEqual));
    }
    return BoolValue(*result);
  };
}

template <typename Type, typename Op>
auto ComplexInequality(Op&& op) {
  return [op = std::forward<Op>(op)](
             Type t1, Type t2,
             const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
             google::protobuf::MessageFactory* absl_nonnull message_factory,
             google::protobuf::Arena* absl_nonnull arena) -> absl::StatusOr<Value> {
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> result,
                         op(t1, t2, descriptor_pool, message_factory, arena));
    if (!result.has_value()) {
      return ErrorValue(
          cel::runtime_internal::CreateNoMatchingOverloadError(kInequal));
    }
    return BoolValue(!*result);
  };
}

template <class Type>
absl::Status RegisterComplexEqualityFunctionsForType(
    absl::FunctionRef<absl::StatusOr<absl::optional<bool>>(
        Type, Type, const google::protobuf::DescriptorPool* absl_nonnull,
        google::protobuf::MessageFactory* absl_nonnull, google::protobuf::Arena* absl_nonnull)>
        op,
    cel::FunctionRegistry& registry) {
  using FunctionAdapter = cel::RegisterHelper<
      BinaryFunctionAdapter<absl::StatusOr<Value>, Type, Type>>;
  // Inequality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kInequal, ComplexInequality<Type>(op), registry));

  // Equality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kEqual, ComplexEquality<Type>(op), registry));

  return absl::OkStatus();
}

absl::Status RegisterHomogenousEqualityFunctions(
    cel::FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<bool>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<int64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<uint64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<double>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::StringValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::BytesValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<absl::Duration>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<absl::Time>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::NullValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::TypeValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComplexEqualityFunctionsForType<const cel::ListValue&>(
          &ListEqual<HomogenousEqualProvider>, registry));

  CEL_RETURN_IF_ERROR(
      RegisterComplexEqualityFunctionsForType<const cel::MapValue&>(
          &MapEqual<HomogenousEqualProvider>, registry));

  return absl::OkStatus();
}

absl::Status RegisterNullMessageEqualityFunctions(FunctionRegistry& registry) {
  // equals
  CEL_RETURN_IF_ERROR(
      (cel::RegisterHelper<
          BinaryFunctionAdapter<bool, const StructValue&, const NullValue&>>::
           RegisterGlobalOverload(
               kEqual,
               [](const StructValue&, const NullValue&) { return false; },
               registry)));

  CEL_RETURN_IF_ERROR(
      (cel::RegisterHelper<
          BinaryFunctionAdapter<bool, const NullValue&, const StructValue&>>::
           RegisterGlobalOverload(
               kEqual,
               [](const NullValue&, const StructValue&) { return false; },
               registry)));

  // inequals
  CEL_RETURN_IF_ERROR(
      (cel::RegisterHelper<
          BinaryFunctionAdapter<bool, const StructValue&, const NullValue&>>::
           RegisterGlobalOverload(
               kInequal,
               [](const StructValue&, const NullValue&) { return true; },
               registry)));

  return cel::RegisterHelper<
      BinaryFunctionAdapter<bool, const NullValue&, const StructValue&>>::
      RegisterGlobalOverload(
          kInequal, [](const NullValue&, const StructValue&) { return true; },
          registry);
}

template <typename EqualsProvider>
absl::StatusOr<absl::optional<bool>> HomogenousValueEqual(
    const Value& v1, const Value& v2,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (v1.kind() != v2.kind()) {
    return absl::nullopt;
  }

  static_assert(std::is_lvalue_reference_v<decltype(v1.GetString())>,
                "unexpected value copy");

  switch (v1->kind()) {
    case ValueKind::kBool:
      return Equal<bool>(v1.GetBool().NativeValue(),
                         v2.GetBool().NativeValue());
    case ValueKind::kNull:
      return Equal<const NullValue&>(v1.GetNull(), v2.GetNull());
    case ValueKind::kInt:
      return Equal<int64_t>(v1.GetInt().NativeValue(),
                            v2.GetInt().NativeValue());
    case ValueKind::kUint:
      return Equal<uint64_t>(v1.GetUint().NativeValue(),
                             v2.GetUint().NativeValue());
    case ValueKind::kDouble:
      return Equal<double>(v1.GetDouble().NativeValue(),
                           v2.GetDouble().NativeValue());
    case ValueKind::kDuration:
      return Equal<absl::Duration>(v1.GetDuration().NativeValue(),
                                   v2.GetDuration().NativeValue());
    case ValueKind::kTimestamp:
      return Equal<absl::Time>(v1.GetTimestamp().NativeValue(),
                               v2.GetTimestamp().NativeValue());
    case ValueKind::kCelType:
      return Equal<const TypeValue&>(v1.GetType(), v2.GetType());
    case ValueKind::kString:
      return Equal<const StringValue&>(v1.GetString(), v2.GetString());
    case ValueKind::kBytes:
      return Equal<const cel::BytesValue&>(v1.GetBytes(), v2.GetBytes());
    case ValueKind::kList:
      return ListEqual<EqualsProvider>(v1.GetList(), v2.GetList(),
                                       descriptor_pool, message_factory, arena);
    case ValueKind::kMap:
      return MapEqual<EqualsProvider>(v1.GetMap(), v2.GetMap(), descriptor_pool,
                                      message_factory, arena);
    case ValueKind::kOpaque:
      return OpaqueEqual(v1.GetOpaque(), v2.GetOpaque(), descriptor_pool,
                         message_factory, arena);
    default:
      return absl::nullopt;
  }
}

absl::StatusOr<Value> EqualOverloadImpl(
    const Value& lhs, const Value& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(absl::optional<bool> result,
                       runtime_internal::ValueEqualImpl(
                           lhs, rhs, descriptor_pool, message_factory, arena));
  if (result.has_value()) {
    return BoolValue(*result);
  }
  return ErrorValue(
      cel::runtime_internal::CreateNoMatchingOverloadError(kEqual));
}

absl::StatusOr<Value> InequalOverloadImpl(
    const Value& lhs, const Value& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(absl::optional<bool> result,
                       runtime_internal::ValueEqualImpl(
                           lhs, rhs, descriptor_pool, message_factory, arena));
  if (result.has_value()) {
    return BoolValue(!*result);
  }
  return ErrorValue(
      cel::runtime_internal::CreateNoMatchingOverloadError(kInequal));
}

absl::Status RegisterHeterogeneousEqualityFunctions(
    cel::FunctionRegistry& registry) {
  using Adapter = cel::RegisterHelper<
      BinaryFunctionAdapter<absl::StatusOr<Value>, const Value&, const Value&>>;
  CEL_RETURN_IF_ERROR(
      Adapter::RegisterGlobalOverload(kEqual, &EqualOverloadImpl, registry));

  CEL_RETURN_IF_ERROR(Adapter::RegisterGlobalOverload(
      kInequal, &InequalOverloadImpl, registry));

  return absl::OkStatus();
}

absl::StatusOr<absl::optional<bool>> HomogenousEqualProvider::operator()(
    const Value& lhs, const Value& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  return HomogenousValueEqual<HomogenousEqualProvider>(
      lhs, rhs, descriptor_pool, message_factory, arena);
}

absl::StatusOr<absl::optional<bool>> HeterogeneousEqualProvider::operator()(
    const Value& lhs, const Value& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  return runtime_internal::ValueEqualImpl(lhs, rhs, descriptor_pool,
                                          message_factory, arena);
}

}  // namespace

namespace runtime_internal {

absl::StatusOr<absl::optional<bool>> ValueEqualImpl(
    const Value& v1, const Value& v2,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) {
  if (v1.kind() == v2.kind()) {
    if (v1.IsStruct() && v2.IsStruct()) {
      CEL_ASSIGN_OR_RETURN(
          Value result,
          v1.GetStruct().Equal(v2, descriptor_pool, message_factory, arena));
      if (result.IsBool()) {
        return result.GetBool().NativeValue();
      }
      return false;
    }
    return HomogenousValueEqual<HeterogeneousEqualProvider>(
        v1, v2, descriptor_pool, message_factory, arena);
  }

  absl::optional<Number> lhs = NumberFromValue(v1);
  absl::optional<Number> rhs = NumberFromValue(v2);

  if (rhs.has_value() && lhs.has_value()) {
    return *lhs == *rhs;
  }

  // TODO(uncreated-issue/6): It's currently possible for the interpreter to create a
  // map containing an Error. Return no matching overload to propagate an error
  // instead of a false result.
  if (v1.IsError() || v1.IsUnknown() || v2.IsError() || v2.IsUnknown()) {
    return absl::nullopt;
  }

  return false;
}

}  // namespace runtime_internal

absl::Status RegisterEqualityFunctions(FunctionRegistry& registry,
                                       const RuntimeOptions& options) {
  if (options.enable_heterogeneous_equality) {
    if (options.enable_fast_builtins) {
      // If enabled, the evaluator provides an implementation that works
      // directly on the value stack.
      return absl::OkStatus();
    }
    // Heterogeneous equality uses one generic overload that delegates to the
    // right equality implementation at runtime.
    CEL_RETURN_IF_ERROR(RegisterHeterogeneousEqualityFunctions(registry));
  } else {
    CEL_RETURN_IF_ERROR(RegisterHomogenousEqualityFunctions(registry));

    CEL_RETURN_IF_ERROR(RegisterNullMessageEqualityFunctions(registry));
  }
  return absl::OkStatus();
}

}  // namespace cel
