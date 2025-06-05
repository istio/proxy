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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_

#include <memory>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/types/variant.h"

namespace cel {

class ValueManager;

class ValueInterface;
class ListValueInterface;
class MapValueInterface;
class StructValueInterface;

class Value;
class BoolValue;
class BytesValue;
class DoubleValue;
class DurationValue;
class ErrorValue;
class IntValue;
class ListValue;
class MapValue;
class NullValue;
class OpaqueValue;
class OptionalValue;
class StringValue;
class StructValue;
class TimestampValue;
class TypeValue;
class UintValue;
class UnknownValue;
class ParsedMessageValue;
class ParsedMapFieldValue;
class ParsedRepeatedFieldValue;
class ParsedJsonListValue;
class ParsedJsonMapValue;

class ParsedListValue;
class ParsedListValueInterface;

class ParsedMapValue;
class ParsedMapValueInterface;

class ParsedStructValue;
class ParsedStructValueInterface;

class ValueIterator;
using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

namespace common_internal {

class SharedByteString;
class SharedByteStringView;

class LegacyListValue;

class LegacyMapValue;

class LegacyStructValue;

template <typename T>
struct IsListValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<ListValueInterface, T>>,
                             std::is_base_of<ListValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsListValueInterfaceV = IsListValueInterface<T>::value;

template <typename T>
struct IsListValueAlternative
    : std::bool_constant<std::disjunction_v<std::is_base_of<ParsedListValue, T>,
                                            std::is_same<LegacyListValue, T>>> {
};

template <typename T>
inline constexpr bool IsListValueAlternativeV =
    IsListValueAlternative<T>::value;

using ListValueVariant =
    absl::variant<ParsedListValue, LegacyListValue, ParsedRepeatedFieldValue,
                  ParsedJsonListValue>;

template <typename T>
struct IsMapValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<MapValueInterface, T>>,
                             std::is_base_of<MapValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsMapValueInterfaceV = IsMapValueInterface<T>::value;

template <typename T>
struct IsMapValueAlternative
    : std::bool_constant<std::disjunction_v<std::is_base_of<ParsedMapValue, T>,
                                            std::is_same<LegacyMapValue, T>>> {
};

template <typename T>
inline constexpr bool IsMapValueAlternativeV = IsMapValueAlternative<T>::value;

using MapValueVariant = absl::variant<ParsedMapValue, LegacyMapValue,
                                      ParsedMapFieldValue, ParsedJsonMapValue>;

template <typename T>
struct IsStructValueInterface
    : std::bool_constant<std::conjunction_v<
          std::negation<std::is_same<StructValueInterface, T>>,
          std::is_base_of<StructValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsStructValueInterfaceV =
    IsStructValueInterface<T>::value;

template <typename T>
struct IsStructValueAlternative
    : std::bool_constant<
          std::disjunction_v<std::is_base_of<ParsedStructValue, T>,
                             std::is_same<LegacyStructValue, T>>> {};

template <typename T>
inline constexpr bool IsStructValueAlternativeV =
    IsStructValueAlternative<T>::value;

using StructValueVariant = absl::variant<absl::monostate, ParsedStructValue,
                                         LegacyStructValue, ParsedMessageValue>;

template <typename T>
struct IsValueInterface
    : std::bool_constant<
          std::conjunction_v<std::negation<std::is_same<ValueInterface, T>>,
                             std::is_base_of<ValueInterface, T>>> {};

template <typename T>
inline constexpr bool IsValueInterfaceV = IsValueInterface<T>::value;

template <typename T>
struct IsValueAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<BoolValue, T>, std::is_same<BytesValue, T>,
          std::is_same<DoubleValue, T>, std::is_same<DurationValue, T>,
          std::is_same<ErrorValue, T>, std::is_same<IntValue, T>,
          IsListValueAlternative<T>, IsMapValueAlternative<T>,
          std::is_same<NullValue, T>, std::is_base_of<OpaqueValue, T>,
          std::is_same<StringValue, T>, IsStructValueAlternative<T>,
          std::is_same<TimestampValue, T>, std::is_same<TypeValue, T>,
          std::is_same<UintValue, T>, std::is_same<UnknownValue, T>>> {};

template <typename T>
inline constexpr bool IsValueAlternativeV = IsValueAlternative<T>::value;

using ValueVariant = absl::variant<
    absl::monostate, BoolValue, BytesValue, DoubleValue, DurationValue,
    ErrorValue, IntValue, LegacyListValue, ParsedListValue,
    ParsedRepeatedFieldValue, ParsedJsonListValue, LegacyMapValue,
    ParsedMapValue, ParsedMapFieldValue, ParsedJsonMapValue, NullValue,
    OpaqueValue, StringValue, LegacyStructValue, ParsedStructValue,
    ParsedMessageValue, TimestampValue, TypeValue, UintValue, UnknownValue>;

// Get the base type alternative for the given alternative or interface. The
// base type alternative is the type stored in the `ValueVariant`.
template <typename T, typename = void>
struct BaseValueAlternativeFor {
  static_assert(IsValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseValueAlternativeFor<T, std::enable_if_t<IsValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedListValue, T>>> {
  using type = ParsedListValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<OpaqueValue, T>>> {
  using type = OpaqueValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedMapValue, T>>> {
  using type = ParsedMapValue;
};

template <typename T>
struct BaseValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedStructValue, T>>> {
  using type = ParsedStructValue;
};

template <typename T>
using BaseValueAlternativeForT = typename BaseValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseListValueAlternativeFor {
  static_assert(IsListValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseListValueAlternativeFor<T,
                                   std::enable_if_t<IsListValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseListValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedListValue, T>>> {
  using type = ParsedListValue;
};

template <typename T>
using BaseListValueAlternativeForT =
    typename BaseListValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseMapValueAlternativeFor {
  static_assert(IsMapValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseMapValueAlternativeFor<T, std::enable_if_t<IsMapValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseMapValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedMapValue, T>>> {
  using type = ParsedMapValue;
};

template <typename T>
using BaseMapValueAlternativeForT =
    typename BaseMapValueAlternativeFor<T>::type;

template <typename T, typename = void>
struct BaseStructValueAlternativeFor {
  static_assert(IsStructValueAlternativeV<T>);
  using type = T;
};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<IsStructValueInterfaceV<T>>>
    : BaseValueAlternativeFor<typename T::alternative_type> {};

template <typename T>
struct BaseStructValueAlternativeFor<
    T, std::enable_if_t<std::is_base_of_v<ParsedStructValue, T>>> {
  using type = ParsedStructValue;
};

template <typename T>
using BaseStructValueAlternativeForT =
    typename BaseStructValueAlternativeFor<T>::type;

ErrorValue GetDefaultErrorValue();

ParsedListValue GetEmptyDynListValue();

ParsedMapValue GetEmptyDynDynMapValue();

OptionalValue GetEmptyDynOptionalValue();

absl::Status ListValueEqual(ValueManager& value_manager, const ListValue& lhs,
                            const ListValue& rhs, Value& result);

absl::Status ListValueEqual(ValueManager& value_manager,
                            const ParsedListValueInterface& lhs,
                            const ListValue& rhs, Value& result);

absl::Status MapValueEqual(ValueManager& value_manager, const MapValue& lhs,
                           const MapValue& rhs, Value& result);

absl::Status MapValueEqual(ValueManager& value_manager,
                           const ParsedMapValueInterface& lhs,
                           const MapValue& rhs, Value& result);

absl::Status StructValueEqual(ValueManager& value_manager,
                              const StructValue& lhs, const StructValue& rhs,
                              Value& result);

absl::Status StructValueEqual(ValueManager& value_manager,
                              const ParsedStructValueInterface& lhs,
                              const StructValue& rhs, Value& result);

const SharedByteString& AsSharedByteString(const BytesValue& value);

const SharedByteString& AsSharedByteString(const StringValue& value);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUES_H_
