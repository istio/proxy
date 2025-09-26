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
//
// Definitions for implementation details of the function adapter utility.

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_FUNCTION_ADAPTER_H_

#include <cstdint>
#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "common/casting.h"
#include "common/kind.h"
#include "common/value.h"

namespace cel::runtime_internal {

// Helper for triggering static asserts in an unspecialized template overload.
template <typename T>
struct UnhandledType : std::false_type {};

// Adapts the type param Type to the appropriate Kind.
// A static assertion fails if the provided type does not map to a cel::Value
// kind.
template <typename Type>
constexpr Kind AdaptedKind() {
  static_assert(UnhandledType<Type>::value,
                "Unsupported primitive type to cel::Kind conversion");
  return Kind::kNotForUseWithExhaustiveSwitchStatements;
}

template <>
constexpr Kind AdaptedKind<int64_t>() {
  return Kind::kInt64;
}

template <>
constexpr Kind AdaptedKind<uint64_t>() {
  return Kind::kUint64;
}

template <>
constexpr Kind AdaptedKind<double>() {
  return Kind::kDouble;
}

template <>
constexpr Kind AdaptedKind<bool>() {
  return Kind::kBool;
}

template <>
constexpr Kind AdaptedKind<absl::Time>() {
  return Kind::kTimestamp;
}

template <>
constexpr Kind AdaptedKind<absl::Duration>() {
  return Kind::kDuration;
}

// ValueTypes without a canonical c++ type representation can be referenced by
// Handle, cref Handle, or cref ValueType.
#define HANDLE_ADAPTED_KIND_OVL(value_type, kind)   \
  template <>                                       \
  constexpr Kind AdaptedKind<const value_type&>() { \
    return kind;                                    \
  }                                                 \
                                                    \
  template <>                                       \
  constexpr Kind AdaptedKind<value_type>() {        \
    return kind;                                    \
  }

HANDLE_ADAPTED_KIND_OVL(Value, Kind::kAny);
HANDLE_ADAPTED_KIND_OVL(StringValue, Kind::kString);
HANDLE_ADAPTED_KIND_OVL(BytesValue, Kind::kBytes);
HANDLE_ADAPTED_KIND_OVL(StructValue, Kind::kStruct);
HANDLE_ADAPTED_KIND_OVL(MapValue, Kind::kMap);
HANDLE_ADAPTED_KIND_OVL(ListValue, Kind::kList);
HANDLE_ADAPTED_KIND_OVL(NullValue, Kind::kNullType);
HANDLE_ADAPTED_KIND_OVL(OpaqueValue, Kind::kOpaque);
HANDLE_ADAPTED_KIND_OVL(TypeValue, Kind::kType);

#undef HANDLE_ADAPTED_KIND_OVL

// Adapt a Value to its corresponding argument type in a wrapped c++
// function.
struct HandleToAdaptedVisitor {
  absl::Status operator()(int64_t* out) const {
    if (!InstanceOf<IntValue>(input)) {
      return absl::InvalidArgumentError("expected int value");
    }
    *out = Cast<IntValue>(input).NativeValue();
    return absl::OkStatus();
  }

  absl::Status operator()(uint64_t* out) const {
    if (!InstanceOf<UintValue>(input)) {
      return absl::InvalidArgumentError("expected uint value");
    }
    *out = Cast<UintValue>(input).NativeValue();
    return absl::OkStatus();
  }

  absl::Status operator()(double* out) const {
    if (!InstanceOf<DoubleValue>(input)) {
      return absl::InvalidArgumentError("expected double value");
    }
    *out = Cast<DoubleValue>(input).NativeValue();
    return absl::OkStatus();
  }

  absl::Status operator()(bool* out) const {
    if (!InstanceOf<BoolValue>(input)) {
      return absl::InvalidArgumentError("expected bool value");
    }
    *out = Cast<BoolValue>(input).NativeValue();
    return absl::OkStatus();
  }

  absl::Status operator()(absl::Time* out) const {
    if (!InstanceOf<TimestampValue>(input)) {
      return absl::InvalidArgumentError("expected timestamp value");
    }
    *out = Cast<TimestampValue>(input).NativeValue();
    return absl::OkStatus();
  }

  absl::Status operator()(absl::Duration* out) const {
    if (!InstanceOf<DurationValue>(input)) {
      return absl::InvalidArgumentError("expected duration value");
    }
    *out = Cast<DurationValue>(input).NativeValue();
    return absl::OkStatus();
  }

  absl::Status operator()(Value* out) const {
    *out = input;
    return absl::OkStatus();
  }

  absl::Status operator()(const Value** out) const {
    *out = &input;
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status operator()(T* out) const {
    if (!InstanceOf<std::remove_const_t<T>>(input)) {
      return absl::InvalidArgumentError(
          absl::StrCat("expected ", ValueKindToString(T::kKind), " value"));
    }
    *out = Cast<std::remove_const_t<T>>(input);
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status operator()(T** out) const {
    if (!InstanceOf<std::remove_const_t<T>>(input)) {
      return absl::InvalidArgumentError(
          absl::StrCat("expected ", ValueKindToString(T::kKind), " value"));
    }
    static_assert(std::is_lvalue_reference_v<
                      decltype(Cast<std::remove_const_t<T>>(input))>,
                  "expected l-value reference return type for Cast.");
    *out = &Cast<std::remove_const_t<T>>(input);
    return absl::OkStatus();
  }

  const Value& input;
};

// Adapts the return value of a wrapped C++ function to its corresponding
// Value representation.
struct AdaptedToHandleVisitor {
  absl::StatusOr<Value> operator()(int64_t in) { return IntValue(in); }

  absl::StatusOr<Value> operator()(uint64_t in) { return UintValue(in); }

  absl::StatusOr<Value> operator()(double in) { return DoubleValue(in); }

  absl::StatusOr<Value> operator()(bool in) { return BoolValue(in); }

  absl::StatusOr<Value> operator()(absl::Time in) {
    // Type matching may have already occurred. It's too late to change up the
    // type and return an error.
    return TimestampValue(in);
  }

  absl::StatusOr<Value> operator()(absl::Duration in) {
    // Type matching may have already occurred. It's too late to change up the
    // type and return an error.
    return DurationValue(in);
  }

  absl::StatusOr<Value> operator()(Value in) { return in; }

  template <typename T>
  absl::StatusOr<Value> operator()(T in) {
    return in;
  }

  // Special case for StatusOr<T> return value -- wrap the underlying value if
  // present, otherwise return the status.
  template <typename T>
  absl::StatusOr<Value> operator()(absl::StatusOr<T> wrapped) {
    if (!wrapped.ok()) {
      return std::move(wrapped).status();
    }
    return this->operator()(std::move(wrapped).value());
  }
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_INTERNAL_FUNCTION_ADAPTER_H_
