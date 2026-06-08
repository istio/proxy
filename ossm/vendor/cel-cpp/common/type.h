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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/type_kind.h"
#include "common/types/any_type.h"   // IWYU pragma: export
#include "common/types/bool_type.h"  // IWYU pragma: export
#include "common/types/bool_wrapper_type.h"  // IWYU pragma: export
#include "common/types/bytes_type.h"  // IWYU pragma: export
#include "common/types/bytes_wrapper_type.h"  // IWYU pragma: export
#include "common/types/double_type.h"  // IWYU pragma: export
#include "common/types/double_wrapper_type.h"  // IWYU pragma: export
#include "common/types/duration_type.h"  // IWYU pragma: export
#include "common/types/dyn_type.h"    // IWYU pragma: export
#include "common/types/enum_type.h"   // IWYU pragma: export
#include "common/types/error_type.h"  // IWYU pragma: export
#include "common/types/function_type.h"  // IWYU pragma: export
#include "common/types/int_type.h"  // IWYU pragma: export
#include "common/types/int_wrapper_type.h"  // IWYU pragma: export
#include "common/types/list_type.h"  // IWYU pragma: export
#include "common/types/map_type.h"   // IWYU pragma: export
#include "common/types/message_type.h"  // IWYU pragma: export
#include "common/types/null_type.h"  // IWYU pragma: export
#include "common/types/opaque_type.h"  // IWYU pragma: export
#include "common/types/optional_type.h"  // IWYU pragma: export
#include "common/types/string_type.h"  // IWYU pragma: export
#include "common/types/string_wrapper_type.h"  // IWYU pragma: export
#include "common/types/struct_type.h"  // IWYU pragma: export
#include "common/types/timestamp_type.h"  // IWYU pragma: export
#include "common/types/type_param_type.h"  // IWYU pragma: export
#include "common/types/type_type.h"  // IWYU pragma: export
#include "common/types/types.h"
#include "common/types/uint_type.h"  // IWYU pragma: export
#include "common/types/uint_wrapper_type.h"  // IWYU pragma: export
#include "common/types/unknown_type.h"  // IWYU pragma: export
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class Type;
class TypeParameters;

// `Type` is a composition type which encompasses all types supported by the
// Common Expression Language. When default constructed, `Type` is in a
// known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
//
// The data underlying `Type` is either static or owned by `google::protobuf::Arena`. As
// such, care must be taken to ensure types remain valid throughout their use.
class Type final {
 public:
  // Returns an appropriate `Type` for the dynamic protobuf message. For well
  // known message types, the appropriate `Type` is returned. All others return
  // `MessageType`.
  static Type Message(const google::protobuf::Descriptor* absl_nonnull descriptor
                          ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Type` for the dynamic protobuf message field.
  static Type Field(const google::protobuf::FieldDescriptor* absl_nonnull descriptor
                        ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Returns an appropriate `Type` for the dynamic protobuf enum. For well
  // known enum types, the appropriate `Type` is returned. All others return
  // `EnumType`.
  static Type Enum(const google::protobuf::EnumDescriptor* absl_nonnull descriptor
                       ABSL_ATTRIBUTE_LIFETIME_BOUND);

  using Parameters = TypeParameters;

  // The default constructor results in Type being DynType.
  Type() = default;
  Type(const Type&) = default;
  Type(Type&&) = default;
  Type& operator=(const Type&) = default;
  Type& operator=(Type&&) = default;

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeAlternativeV<
                absl::remove_reference_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Type(T&& alternative) noexcept
      : variant_(absl::in_place_type<absl::remove_cvref_t<T>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeAlternativeV<
                absl::remove_reference_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(T&& type) noexcept {
    variant_.emplace<absl::remove_cvref_t<T>>(std::forward<T>(type));
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(StructType alternative) : variant_(alternative.ToTypeVariant()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(StructType alternative) {
    variant_ = alternative.ToTypeVariant();
    return *this;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(OptionalType alternative) : Type(OpaqueType(std::move(alternative))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(OptionalType alternative) {
    return *this = OpaqueType(std::move(alternative));
  }

  TypeKind kind() const;

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Returns a debug string for the type. Not suitable for user-facing error
  // messages.
  std::string DebugString() const;

  Parameters GetParameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  template <typename H>
  friend H AbslHashValue(H state, const Type& type) {
    return absl::visit(
        [state = std::move(state)](const auto& alternative) mutable -> H {
          return H::combine(std::move(state), alternative, alternative.kind());
        },
        type.variant_);
  }

  friend bool operator==(const Type& lhs, const Type& rhs);

  friend std::ostream& operator<<(std::ostream& out, const Type& type) {
    return absl::visit(
        [&out](const auto& alternative) -> std::ostream& {
          return out << alternative;
        },
        type.variant_);
  }

  bool IsAny() const { return absl::holds_alternative<AnyType>(variant_); }

  bool IsBool() const { return absl::holds_alternative<BoolType>(variant_); }

  bool IsBoolWrapper() const {
    return absl::holds_alternative<BoolWrapperType>(variant_);
  }

  bool IsBytes() const { return absl::holds_alternative<BytesType>(variant_); }

  bool IsBytesWrapper() const {
    return absl::holds_alternative<BytesWrapperType>(variant_);
  }

  bool IsDouble() const {
    return absl::holds_alternative<DoubleType>(variant_);
  }

  bool IsDoubleWrapper() const {
    return absl::holds_alternative<DoubleWrapperType>(variant_);
  }

  bool IsDuration() const {
    return absl::holds_alternative<DurationType>(variant_);
  }

  bool IsDyn() const { return absl::holds_alternative<DynType>(variant_); }

  bool IsEnum() const { return absl::holds_alternative<EnumType>(variant_); }

  bool IsError() const { return absl::holds_alternative<ErrorType>(variant_); }

  bool IsFunction() const {
    return absl::holds_alternative<FunctionType>(variant_);
  }

  bool IsInt() const { return absl::holds_alternative<IntType>(variant_); }

  bool IsIntWrapper() const {
    return absl::holds_alternative<IntWrapperType>(variant_);
  }

  bool IsList() const { return absl::holds_alternative<ListType>(variant_); }

  bool IsMap() const { return absl::holds_alternative<MapType>(variant_); }

  bool IsMessage() const {
    return absl::holds_alternative<MessageType>(variant_);
  }

  bool IsNull() const { return absl::holds_alternative<NullType>(variant_); }

  bool IsOpaque() const {
    return absl::holds_alternative<OpaqueType>(variant_);
  }

  bool IsOptional() const { return IsOpaque() && GetOpaque().IsOptional(); }

  bool IsString() const {
    return absl::holds_alternative<StringType>(variant_);
  }

  bool IsStringWrapper() const {
    return absl::holds_alternative<StringWrapperType>(variant_);
  }

  bool IsStruct() const {
    return absl::holds_alternative<common_internal::BasicStructType>(
               variant_) ||
           absl::holds_alternative<MessageType>(variant_);
  }

  bool IsTimestamp() const {
    return absl::holds_alternative<TimestampType>(variant_);
  }

  bool IsTypeParam() const {
    return absl::holds_alternative<TypeParamType>(variant_);
  }

  bool IsType() const { return absl::holds_alternative<TypeType>(variant_); }

  bool IsUint() const { return absl::holds_alternative<UintType>(variant_); }

  bool IsUintWrapper() const {
    return absl::holds_alternative<UintWrapperType>(variant_);
  }

  bool IsUnknown() const {
    return absl::holds_alternative<UnknownType>(variant_);
  }

  bool IsWrapper() const {
    return IsBoolWrapper() || IsIntWrapper() || IsUintWrapper() ||
           IsDoubleWrapper() || IsBytesWrapper() || IsStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<AnyType, T>, bool> Is() const {
    return IsAny();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolType, T>, bool> Is() const {
    return IsBool();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolWrapperType, T>, bool> Is() const {
    return IsBoolWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesType, T>, bool> Is() const {
    return IsBytes();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesWrapperType, T>, bool> Is() const {
    return IsBytesWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleType, T>, bool> Is() const {
    return IsDouble();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleWrapperType, T>, bool> Is() const {
    return IsDoubleWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DurationType, T>, bool> Is() const {
    return IsDuration();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DynType, T>, bool> Is() const {
    return IsDyn();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<EnumType, T>, bool> Is() const {
    return IsEnum();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorType, T>, bool> Is() const {
    return IsError();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<FunctionType, T>, bool> Is() const {
    return IsFunction();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntType, T>, bool> Is() const {
    return IsInt();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntWrapperType, T>, bool> Is() const {
    return IsIntWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ListType, T>, bool> Is() const {
    return IsList();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MapType, T>, bool> Is() const {
    return IsMap();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, bool> Is() const {
    return IsMessage();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<NullType, T>, bool> Is() const {
    return IsNull();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueType, T>, bool> Is() const {
    return IsOpaque();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>, bool> Is() const {
    return IsOptional();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringType, T>, bool> Is() const {
    return IsString();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringWrapperType, T>, bool> Is() const {
    return IsStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StructType, T>, bool> Is() const {
    return IsStruct();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampType, T>, bool> Is() const {
    return IsTimestamp();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeParamType, T>, bool> Is() const {
    return IsTypeParam();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeType, T>, bool> Is() const {
    return IsType();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintType, T>, bool> Is() const {
    return IsUint();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintWrapperType, T>, bool> Is() const {
    return IsUintWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownType, T>, bool> Is() const {
    return IsUnknown();
  }

  absl::optional<AnyType> AsAny() const;

  absl::optional<BoolType> AsBool() const;

  absl::optional<BoolWrapperType> AsBoolWrapper() const;

  absl::optional<BytesType> AsBytes() const;

  absl::optional<BytesWrapperType> AsBytesWrapper() const;

  absl::optional<DoubleType> AsDouble() const;

  absl::optional<DoubleWrapperType> AsDoubleWrapper() const;

  absl::optional<DurationType> AsDuration() const;

  absl::optional<DynType> AsDyn() const;

  absl::optional<EnumType> AsEnum() const;

  absl::optional<ErrorType> AsError() const;

  absl::optional<FunctionType> AsFunction() const;

  absl::optional<IntType> AsInt() const;

  absl::optional<IntWrapperType> AsIntWrapper() const;

  absl::optional<ListType> AsList() const;

  absl::optional<MapType> AsMap() const;

  // AsMessage performs a checked cast, returning `MessageType` if this type is
  // both a struct and a message or `absl::nullopt` otherwise. If you have
  // already called `IsMessage()` it is more performant to perform to do
  // `static_cast<MessageType>(type)`.
  absl::optional<MessageType> AsMessage() const;

  absl::optional<NullType> AsNull() const;

  absl::optional<OpaqueType> AsOpaque() const;

  absl::optional<OptionalType> AsOptional() const;

  absl::optional<StringType> AsString() const;

  absl::optional<StringWrapperType> AsStringWrapper() const;

  // AsStruct performs a checked cast, returning `StructType` if this type is a
  // struct or `absl::nullopt` otherwise. If you have already called
  // `IsStruct()` it is more performant to perform to do
  // `static_cast<StructType>(type)`.
  absl::optional<StructType> AsStruct() const;

  absl::optional<TimestampType> AsTimestamp() const;

  absl::optional<TypeParamType> AsTypeParam() const;

  absl::optional<TypeType> AsType() const;

  absl::optional<UintType> AsUint() const;

  absl::optional<UintWrapperType> AsUintWrapper() const;

  absl::optional<UnknownType> AsUnknown() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<AnyType, T>, absl::optional<AnyType>> As()
      const {
    return AsAny();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolType, T>, absl::optional<BoolType>> As()
      const {
    return AsBool();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolWrapperType, T>,
                   absl::optional<BoolWrapperType>>
  As() const {
    return AsBoolWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesType, T>, absl::optional<BytesType>> As()
      const {
    return AsBytes();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesWrapperType, T>,
                   absl::optional<BytesWrapperType>>
  As() const {
    return AsBytesWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleType, T>, absl::optional<DoubleType>>
  As() const {
    return AsDouble();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleWrapperType, T>,
                   absl::optional<DoubleWrapperType>>
  As() const {
    return AsDoubleWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DurationType, T>,
                   absl::optional<DurationType>>
  As() const {
    return AsDuration();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DynType, T>, absl::optional<DynType>> As()
      const {
    return AsDyn();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<EnumType, T>, absl::optional<EnumType>> As()
      const {
    return AsEnum();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorType, T>, absl::optional<ErrorType>> As()
      const {
    return AsError();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<FunctionType, T>,
                   absl::optional<FunctionType>>
  As() const {
    return AsFunction();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntType, T>, absl::optional<IntType>> As()
      const {
    return AsInt();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntWrapperType, T>,
                   absl::optional<IntWrapperType>>
  As() const {
    return AsIntWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ListType, T>, absl::optional<ListType>> As()
      const {
    return AsList();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MapType, T>, absl::optional<MapType>> As()
      const {
    return AsMap();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, absl::optional<MessageType>>
  As() const {
    return AsMessage();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<NullType, T>, absl::optional<NullType>> As()
      const {
    return AsNull();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueType, T>, absl::optional<OpaqueType>>
  As() const {
    return AsOpaque();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>,
                   absl::optional<OptionalType>>
  As() const {
    return AsOptional();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringType, T>, absl::optional<StringType>>
  As() const {
    return AsString();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringWrapperType, T>,
                   absl::optional<StringWrapperType>>
  As() const {
    return AsStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StructType, T>, absl::optional<StructType>>
  As() const {
    return AsStruct();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampType, T>,
                   absl::optional<TimestampType>>
  As() const {
    return AsTimestamp();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeParamType, T>,
                   absl::optional<TypeParamType>>
  As() const {
    return AsTypeParam();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeType, T>, absl::optional<TypeType>> As()
      const {
    return AsType();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintType, T>, absl::optional<UintType>> As()
      const {
    return AsUint();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintWrapperType, T>,
                   absl::optional<UintWrapperType>>
  As() const {
    return AsUintWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownType, T>, absl::optional<UnknownType>>
  As() const {
    return AsUnknown();
  }

  AnyType GetAny() const;

  BoolType GetBool() const;

  BoolWrapperType GetBoolWrapper() const;

  BytesType GetBytes() const;

  BytesWrapperType GetBytesWrapper() const;

  DoubleType GetDouble() const;

  DoubleWrapperType GetDoubleWrapper() const;

  DurationType GetDuration() const;

  DynType GetDyn() const;

  EnumType GetEnum() const;

  ErrorType GetError() const;

  FunctionType GetFunction() const;

  IntType GetInt() const;

  IntWrapperType GetIntWrapper() const;

  ListType GetList() const;

  MapType GetMap() const;

  MessageType GetMessage() const;

  NullType GetNull() const;

  OpaqueType GetOpaque() const;

  OptionalType GetOptional() const;

  StringType GetString() const;

  StringWrapperType GetStringWrapper() const;

  StructType GetStruct() const;

  TimestampType GetTimestamp() const;

  TypeParamType GetTypeParam() const;

  TypeType GetType() const;

  UintType GetUint() const;

  UintWrapperType GetUintWrapper() const;

  UnknownType GetUnknown() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<AnyType, T>, AnyType> Get() const {
    return GetAny();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolType, T>, BoolType> Get() const {
    return GetBool();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BoolWrapperType, T>, BoolWrapperType> Get()
      const {
    return GetBoolWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesType, T>, BytesType> Get() const {
    return GetBytes();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<BytesWrapperType, T>, BytesWrapperType> Get()
      const {
    return GetBytesWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleType, T>, DoubleType> Get() const {
    return GetDouble();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DoubleWrapperType, T>, DoubleWrapperType>
  Get() const {
    return GetDoubleWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DurationType, T>, DurationType> Get() const {
    return GetDuration();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<DynType, T>, DynType> Get() const {
    return GetDyn();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<EnumType, T>, EnumType> Get() const {
    return GetEnum();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ErrorType, T>, ErrorType> Get() const {
    return GetError();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<FunctionType, T>, FunctionType> Get() const {
    return GetFunction();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntType, T>, IntType> Get() const {
    return GetInt();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<IntWrapperType, T>, IntWrapperType> Get()
      const {
    return GetIntWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<ListType, T>, ListType> Get() const {
    return GetList();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MapType, T>, MapType> Get() const {
    return GetMap();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<MessageType, T>, MessageType> Get() const {
    return GetMessage();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<NullType, T>, NullType> Get() const {
    return GetNull();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OpaqueType, T>, OpaqueType> Get() const {
    return GetOpaque();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>, OptionalType> Get() const {
    return GetOptional();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringType, T>, StringType> Get() const {
    return GetString();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StringWrapperType, T>, StringWrapperType>
  Get() const {
    return GetStringWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<StructType, T>, StructType> Get() const {
    return GetStruct();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TimestampType, T>, TimestampType> Get()
      const {
    return GetTimestamp();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeParamType, T>, TypeParamType> Get()
      const {
    return GetTypeParam();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<TypeType, T>, TypeType> Get() const {
    return GetType();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintType, T>, UintType> Get() const {
    return GetUint();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UintWrapperType, T>, UintWrapperType> Get()
      const {
    return GetUintWrapper();
  }

  template <typename T>
  std::enable_if_t<std::is_same_v<UnknownType, T>, UnknownType> Get() const {
    return GetUnknown();
  }

  // Returns an unwrapped `Type` for a wrapped type, otherwise just returns
  // this.
  Type Unwrap() const;

  // Returns an wrapped `Type` for a primitive type, otherwise just returns
  // this.
  Type Wrap() const;

 private:
  friend class StructType;
  friend class MessageType;
  friend class common_internal::BasicStructType;

  common_internal::StructTypeVariant ToStructTypeVariant() const;

  common_internal::TypeVariant variant_;
};

inline bool operator!=(const Type& lhs, const Type& rhs) {
  return !operator==(lhs, rhs);
}

inline Type JsonType() { return DynType(); }

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Type>);
static_assert(std::is_copy_constructible_v<Type>);
static_assert(std::is_copy_assignable_v<Type>);
static_assert(std::is_nothrow_move_constructible_v<Type>);
static_assert(std::is_nothrow_move_assignable_v<Type>);

// TypeParameters is a specialized view of a contiguous list of `Type`. It is
// very similar to `absl::Span<const Type>`, except that it has a small amount
// of inline storage. Thus the pointers and references returned by
// TypeParameters are invalidated upon copying or moving.
//
// We store up to 2 types inline. This is done to accommodate list and map types
// which correspond to protocol buffer message fields. We launder around their
// descriptors and would have to allocate to return the type parameters. We want
// to avoid this, as types are supposed to be constant after creation.
class TypeParameters final {
 public:
  using element_type = const Type;
  using value_type = Type;
  using pointer = element_type*;
  using const_pointer = const element_type*;
  using reference = element_type&;
  using const_reference = const element_type&;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;

  explicit TypeParameters(absl::Span<const Type> types);

  TypeParameters() = default;
  TypeParameters(const TypeParameters&) = default;
  TypeParameters(TypeParameters&&) = default;
  TypeParameters& operator=(const TypeParameters&) = default;
  TypeParameters& operator=(TypeParameters&&) = default;

  size_type size() const { return size_; }

  bool empty() const { return size() == 0; }

  const_reference front() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!empty());
    return data()[0];
  }

  const_reference back() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!empty());
    return data()[size() - 1];
  }

  const_reference operator[](size_type index) const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK_LT(index, size());
    return data()[index];
  }

  const_pointer data() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return size() <= 2 ? reinterpret_cast<const Type*>(&internal_[0])
                       : external_;
  }

  const_iterator begin() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return data(); }

  const_iterator cbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return begin();
  }

  const_iterator end() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return data() + size();
  }

  const_iterator cend() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return end(); }

  const_reverse_iterator rbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::make_reverse_iterator(end());
  }

  const_reverse_iterator crbegin() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rbegin();
  }

  const_reverse_iterator rend() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::make_reverse_iterator(begin());
  }

  const_reverse_iterator crend() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return rend();
  }

 private:
  friend class ListType;
  friend class MapType;

  explicit TypeParameters(const Type& element);

  explicit TypeParameters(const Type& key, const Type& value);

  // When size_ <= 2, elements are stored directly in `internal_`. Otherwise we
  // store a pointer to the elements in `external_`.
  size_t size_ = 0;
  union {
    const Type* external_ = nullptr;
    // Old versions of GCC do not like `Type internal_[2]`, so we cheat.
    alignas(Type) char internal_[sizeof(Type) * 2];
  };
};

// Now that TypeParameters is defined, we can define `GetParameters()` for most
// types.

inline TypeParameters AnyType::GetParameters() { return {}; }

inline TypeParameters BoolType::GetParameters() { return {}; }

inline TypeParameters BoolWrapperType::GetParameters() { return {}; }

inline TypeParameters BytesType::GetParameters() { return {}; }

inline TypeParameters BytesWrapperType::GetParameters() { return {}; }

inline TypeParameters DoubleType::GetParameters() { return {}; }

inline TypeParameters DoubleWrapperType::GetParameters() { return {}; }

inline TypeParameters DurationType::GetParameters() { return {}; }

inline TypeParameters DynType::GetParameters() { return {}; }

inline TypeParameters EnumType::GetParameters() { return {}; }

inline TypeParameters ErrorType::GetParameters() { return {}; }

inline TypeParameters IntType::GetParameters() { return {}; }

inline TypeParameters IntWrapperType::GetParameters() { return {}; }

inline TypeParameters MessageType::GetParameters() { return {}; }

inline TypeParameters NullType::GetParameters() { return {}; }

inline TypeParameters OptionalType::GetParameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return opaque_.GetParameters();
}

inline TypeParameters StringType::GetParameters() { return {}; }

inline TypeParameters StringWrapperType::GetParameters() { return {}; }

inline TypeParameters TimestampType::GetParameters() { return {}; }

inline TypeParameters TypeParamType::GetParameters() { return {}; }

inline TypeParameters UintType::GetParameters() { return {}; }

inline TypeParameters UintWrapperType::GetParameters() { return {}; }

inline TypeParameters UnknownType::GetParameters() { return {}; }

namespace common_internal {

inline TypeParameters BasicStructType::GetParameters() { return {}; }

Type SingularMessageFieldType(
    const google::protobuf::FieldDescriptor* absl_nonnull descriptor);

class BasicStructTypeField final {
 public:
  BasicStructTypeField(absl::string_view name, int32_t number, Type type)
      : name_(name), number_(number), type_(type) {}

  BasicStructTypeField(const BasicStructTypeField&) = default;
  BasicStructTypeField(BasicStructTypeField&&) = default;
  BasicStructTypeField& operator=(const BasicStructTypeField&) = default;
  BasicStructTypeField& operator=(BasicStructTypeField&&) = default;

  std::string DebugString() const;

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return name_; }

  int32_t number() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return number_; }

  Type GetType() const { return type_; }

  explicit operator bool() const { return !name_.empty() || number_ >= 1; }

 private:
  absl::string_view name_;
  int32_t number_ = 0;
  Type type_;
};

inline bool operator==(const BasicStructTypeField& lhs,
                       const BasicStructTypeField& rhs) {
  return lhs.name() == rhs.name() && lhs.number() == rhs.number() &&
         lhs.GetType() == rhs.GetType();
}

inline bool operator!=(const BasicStructTypeField& lhs,
                       const BasicStructTypeField& rhs) {
  return !operator==(lhs, rhs);
}

}  // namespace common_internal

class StructTypeField final {
 public:
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeField(common_internal::BasicStructTypeField field)
      : variant_(absl::in_place_type<common_internal::BasicStructTypeField>,
                 field) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeField(MessageTypeField field)
      : variant_(absl::in_place_type<MessageTypeField>, field) {}

  StructTypeField() = delete;
  StructTypeField(const StructTypeField&) = default;
  StructTypeField(StructTypeField&&) = default;
  StructTypeField& operator=(const StructTypeField&) = default;
  StructTypeField& operator=(StructTypeField&&) = default;

  std::string DebugString() const;

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  int32_t number() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  Type GetType() const;

  explicit operator bool() const;

  bool IsMessage() const {
    return absl::holds_alternative<MessageTypeField>(variant_);
  }

  absl::optional<MessageTypeField> AsMessage() const;

  explicit operator MessageTypeField() const;

 private:
  absl::variant<common_internal::BasicStructTypeField, MessageTypeField>
      variant_;
};

inline bool operator==(const StructTypeField& lhs, const StructTypeField& rhs) {
  return lhs.name() == rhs.name() && lhs.number() == rhs.number() &&
         lhs.GetType() == rhs.GetType();
}

inline bool operator!=(const StructTypeField& lhs, const StructTypeField& rhs) {
  return !operator==(lhs, rhs);
}

// Now that Type is defined, we can define everything else.

namespace common_internal {

struct ListTypeData final {
  static ListTypeData* absl_nonnull Create(google::protobuf::Arena* absl_nonnull arena,
                                           const Type& element);

  ListTypeData() = default;
  ListTypeData(const ListTypeData&) = delete;
  ListTypeData(ListTypeData&&) = delete;
  ListTypeData& operator=(const ListTypeData&) = delete;
  ListTypeData& operator=(ListTypeData&&) = delete;

  Type element = DynType();

 private:
  explicit ListTypeData(const Type& element);
};

struct MapTypeData final {
  static MapTypeData* absl_nonnull Create(google::protobuf::Arena* absl_nonnull arena,
                                          const Type& key, const Type& value);

  Type key_and_value[2];
};

struct FunctionTypeData final {
  static FunctionTypeData* absl_nonnull Create(
      google::protobuf::Arena* absl_nonnull arena, const Type& result,
      absl::Span<const Type> args);

  FunctionTypeData() = delete;
  FunctionTypeData(const FunctionTypeData&) = delete;
  FunctionTypeData(FunctionTypeData&&) = delete;
  FunctionTypeData& operator=(const FunctionTypeData&) = delete;
  FunctionTypeData& operator=(FunctionTypeData&&) = delete;

  const size_t args_size;
  // Flexible array, has `args_size` elements, with the first element being the
  // return type. FunctionTypeData has a variable length size, which includes
  // this flexible array.
  Type args[];

 private:
  FunctionTypeData(const Type& result, absl::Span<const Type> args);
};

struct OpaqueTypeData final {
  static OpaqueTypeData* absl_nonnull Create(google::protobuf::Arena* absl_nonnull arena,
                                             absl::string_view name,
                                             absl::Span<const Type> parameters);

  OpaqueTypeData() = delete;
  OpaqueTypeData(const OpaqueTypeData&) = delete;
  OpaqueTypeData(OpaqueTypeData&&) = delete;
  OpaqueTypeData& operator=(const OpaqueTypeData&) = delete;
  OpaqueTypeData& operator=(OpaqueTypeData&&) = delete;

  const absl::string_view name;
  const size_t parameters_size;
  // Flexible array, has `parameters_size` elements. OpaqueTypeData has a
  // variable length size, which includes this flexible array.
  Type parameters[];

 private:
  OpaqueTypeData(absl::string_view name, absl::Span<const Type> parameters);
};

}  // namespace common_internal

inline bool operator==(const MessageTypeField& lhs,
                       const MessageTypeField& rhs) {
  return lhs.name() == rhs.name() && lhs.number() == rhs.number() &&
         lhs.GetType() == rhs.GetType();
}

inline bool operator!=(const MessageTypeField& lhs,
                       const MessageTypeField& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator==(const ListType& lhs, const ListType& rhs) {
  return &lhs == &rhs || lhs.GetElement() == rhs.GetElement();
}

template <typename H>
inline H AbslHashValue(H state, const ListType& type) {
  return H::combine(std::move(state), type.GetElement(), size_t{1});
}

inline bool operator==(const MapType& lhs, const MapType& rhs) {
  return &lhs == &rhs ||
         (lhs.GetKey() == rhs.GetKey() && lhs.GetValue() == rhs.GetValue());
}

template <typename H>
inline H AbslHashValue(H state, const MapType& type) {
  return H::combine(std::move(state), type.GetKey(), type.GetValue(),
                    size_t{2});
}

inline bool operator==(const OpaqueType& lhs, const OpaqueType& rhs) {
  return lhs.name() == rhs.name() &&
         absl::c_equal(lhs.GetParameters(), rhs.GetParameters());
}

template <typename H>
inline H AbslHashValue(H state, const OpaqueType& type) {
  state = H::combine(std::move(state), type.name());
  auto parameters = type.GetParameters();
  for (const auto& parameter : parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return H::combine(std::move(state), parameters.size());
}

inline bool operator==(const FunctionType& lhs, const FunctionType& rhs) {
  return lhs.result() == rhs.result() && absl::c_equal(lhs.args(), rhs.args());
}

template <typename H>
inline H AbslHashValue(H state, const FunctionType& type) {
  state = H::combine(std::move(state), type.result());
  auto args = type.args();
  for (const auto& arg : args) {
    state = H::combine(std::move(state), arg);
  }
  return H::combine(std::move(state), args.size());
}

namespace common_internal {

// Converts the string returned from `CelValue::CelTypeHolder` to `cel::Type`.
// The underlying content of `name` must outlive the resulting type and any of
// its shallow copies.
Type LegacyRuntimeType(absl::string_view name);

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
