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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_

#include <type_traits>

#include "absl/meta/type_traits.h"
#include "absl/types/variant.h"

namespace cel {

class Type;
class AnyType;
class BoolType;
class BoolWrapperType;
class BytesType;
class BytesWrapperType;
class DoubleType;
class DoubleWrapperType;
class DurationType;
class DynType;
class EnumType;
class ErrorType;
class FunctionType;
class IntType;
class IntWrapperType;
class ListType;
class MapType;
class NullType;
class OpaqueType;
class OptionalType;
class StringType;
class StringWrapperType;
class StructType;
class MessageType;
class TimestampType;
class TypeParamType;
class TypeType;
class UintType;
class UintWrapperType;
class UnknownType;

namespace common_internal {

class BasicStructType;

template <typename T, typename U = absl::remove_cv_t<T>>
struct IsTypeAlternative
    : std::bool_constant<std::disjunction_v<
          std::is_same<AnyType, U>, std::is_same<BoolType, U>,
          std::is_same<BoolWrapperType, U>, std::is_same<BytesType, U>,
          std::is_same<BytesWrapperType, U>, std::is_same<DoubleType, U>,
          std::is_same<DoubleWrapperType, U>, std::is_same<DurationType, U>,
          std::is_same<DynType, U>, std::is_same<EnumType, U>,
          std::is_same<ErrorType, U>, std::is_same<FunctionType, U>,
          std::is_same<IntType, U>, std::is_same<IntWrapperType, U>,
          std::is_same<ListType, U>, std::is_same<MapType, U>,
          std::is_same<NullType, U>, std::is_same<OpaqueType, U>,
          std::is_same<StringType, U>, std::is_same<StringWrapperType, U>,
          std::is_same<MessageType, U>, std::is_same<BasicStructType, U>,
          std::is_same<TimestampType, U>, std::is_same<TypeParamType, U>,
          std::is_same<TypeType, U>, std::is_same<UintType, U>,
          std::is_same<UintWrapperType, U>, std::is_same<UnknownType, U>>> {};

template <typename T>
inline constexpr bool IsTypeAlternativeV = IsTypeAlternative<T>::value;

using TypeVariant =
    absl::variant<DynType, AnyType, BoolType, BoolWrapperType, BytesType,
                  BytesWrapperType, DoubleType, DoubleWrapperType, DurationType,
                  EnumType, ErrorType, FunctionType, IntType, IntWrapperType,
                  ListType, MapType, NullType, OpaqueType, StringType,
                  StringWrapperType, MessageType, BasicStructType,
                  TimestampType, TypeParamType, TypeType, UintType,
                  UintWrapperType, UnknownType>;

using StructTypeVariant =
    absl::variant<absl::monostate, BasicStructType, MessageType>;

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPES_H_
