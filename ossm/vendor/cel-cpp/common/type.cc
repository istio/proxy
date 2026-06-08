// Copyright 2024 Google LLC
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

#include "common/type.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/type_kind.h"
#include "common/types/types.h"
#include "google/protobuf/descriptor.h"

namespace cel {

using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;

Type Type::Message(const Descriptor* absl_nonnull descriptor) {
  switch (descriptor->well_known_type()) {
    case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
      return BoolWrapperType();
    case Descriptor::WELLKNOWNTYPE_INT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_INT64VALUE:
      return IntWrapperType();
    case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
      return UintWrapperType();
    case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
      ABSL_FALLTHROUGH_INTENDED;
    case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
      return DoubleWrapperType();
    case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
      return BytesWrapperType();
    case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
      return StringWrapperType();
    case Descriptor::WELLKNOWNTYPE_ANY:
      return AnyType();
    case Descriptor::WELLKNOWNTYPE_DURATION:
      return DurationType();
    case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
      return TimestampType();
    case Descriptor::WELLKNOWNTYPE_VALUE:
      return DynType();
    case Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return ListType();
    case Descriptor::WELLKNOWNTYPE_STRUCT:
      return JsonMapType();
    default:
      return MessageType(descriptor);
  }
}

Type Type::Enum(const google::protobuf::EnumDescriptor* absl_nonnull descriptor) {
  if (descriptor->full_name() == "google.protobuf.NullValue") {
    return NullType();
  }
  return EnumType(descriptor);
}

namespace {

static constexpr std::array<TypeKind, 28> kTypeToKindArray = {
    TypeKind::kDyn,         TypeKind::kAny,           TypeKind::kBool,
    TypeKind::kBoolWrapper, TypeKind::kBytes,         TypeKind::kBytesWrapper,
    TypeKind::kDouble,      TypeKind::kDoubleWrapper, TypeKind::kDuration,
    TypeKind::kEnum,        TypeKind::kError,         TypeKind::kFunction,
    TypeKind::kInt,         TypeKind::kIntWrapper,    TypeKind::kList,
    TypeKind::kMap,         TypeKind::kNull,          TypeKind::kOpaque,
    TypeKind::kString,      TypeKind::kStringWrapper, TypeKind::kStruct,
    TypeKind::kStruct,      TypeKind::kTimestamp,     TypeKind::kTypeParam,
    TypeKind::kType,        TypeKind::kUint,          TypeKind::kUintWrapper,
    TypeKind::kUnknown};

static_assert(kTypeToKindArray.size() ==
                  absl::variant_size<common_internal::TypeVariant>(),
              "Kind indexer must match variant declaration for cel::Type.");

}  // namespace

TypeKind Type::kind() const { return kTypeToKindArray[variant_.index()]; }

absl::string_view Type::name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        return alternative.name();
      },
      variant_);
}

std::string Type::DebugString() const {
  return absl::visit(
      [](const auto& alternative) -> std::string {
        return alternative.DebugString();
      },
      variant_);
}

TypeParameters Type::GetParameters() const {
  return absl::visit(
      [](const auto& alternative) -> TypeParameters {
        return alternative.GetParameters();
      },
      variant_);
}

bool operator==(const Type& lhs, const Type& rhs) {
  if (lhs.IsStruct() && rhs.IsStruct()) {
    return lhs.GetStruct() == rhs.GetStruct();
  } else if (lhs.IsStruct() || rhs.IsStruct()) {
    return false;
  } else {
    return lhs.variant_ == rhs.variant_;
  }
}

common_internal::StructTypeVariant Type::ToStructTypeVariant() const {
  if (const auto* other = absl::get_if<MessageType>(&variant_);
      other != nullptr) {
    return common_internal::StructTypeVariant(*other);
  }
  if (const auto* other =
          absl::get_if<common_internal::BasicStructType>(&variant_);
      other != nullptr) {
    return common_internal::StructTypeVariant(*other);
  }
  return common_internal::StructTypeVariant();
}

namespace {

template <typename T>
absl::optional<T> GetOrNullopt(const common_internal::TypeVariant& variant) {
  if (const auto* alt = absl::get_if<T>(&variant); alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

}  // namespace

absl::optional<AnyType> Type::AsAny() const {
  return GetOrNullopt<AnyType>(variant_);
}

absl::optional<BoolType> Type::AsBool() const {
  return GetOrNullopt<BoolType>(variant_);
}

absl::optional<BoolWrapperType> Type::AsBoolWrapper() const {
  return GetOrNullopt<BoolWrapperType>(variant_);
}

absl::optional<BytesType> Type::AsBytes() const {
  return GetOrNullopt<BytesType>(variant_);
}

absl::optional<BytesWrapperType> Type::AsBytesWrapper() const {
  return GetOrNullopt<BytesWrapperType>(variant_);
}

absl::optional<DoubleType> Type::AsDouble() const {
  return GetOrNullopt<DoubleType>(variant_);
}

absl::optional<DoubleWrapperType> Type::AsDoubleWrapper() const {
  return GetOrNullopt<DoubleWrapperType>(variant_);
}

absl::optional<DurationType> Type::AsDuration() const {
  return GetOrNullopt<DurationType>(variant_);
}

absl::optional<DynType> Type::AsDyn() const {
  return GetOrNullopt<DynType>(variant_);
}

absl::optional<EnumType> Type::AsEnum() const {
  return GetOrNullopt<EnumType>(variant_);
}

absl::optional<ErrorType> Type::AsError() const {
  return GetOrNullopt<ErrorType>(variant_);
}

absl::optional<FunctionType> Type::AsFunction() const {
  return GetOrNullopt<FunctionType>(variant_);
}

absl::optional<IntType> Type::AsInt() const {
  return GetOrNullopt<IntType>(variant_);
}

absl::optional<IntWrapperType> Type::AsIntWrapper() const {
  return GetOrNullopt<IntWrapperType>(variant_);
}

absl::optional<ListType> Type::AsList() const {
  return GetOrNullopt<ListType>(variant_);
}

absl::optional<MapType> Type::AsMap() const {
  return GetOrNullopt<MapType>(variant_);
}

absl::optional<MessageType> Type::AsMessage() const {
  return GetOrNullopt<MessageType>(variant_);
}

absl::optional<NullType> Type::AsNull() const {
  return GetOrNullopt<NullType>(variant_);
}

absl::optional<OpaqueType> Type::AsOpaque() const {
  return GetOrNullopt<OpaqueType>(variant_);
}

absl::optional<OptionalType> Type::AsOptional() const {
  if (auto maybe_opaque = AsOpaque(); maybe_opaque.has_value()) {
    return maybe_opaque->AsOptional();
  }
  return absl::nullopt;
}

absl::optional<StringType> Type::AsString() const {
  return GetOrNullopt<StringType>(variant_);
}

absl::optional<StringWrapperType> Type::AsStringWrapper() const {
  return GetOrNullopt<StringWrapperType>(variant_);
}

absl::optional<StructType> Type::AsStruct() const {
  if (const auto* alt =
          absl::get_if<common_internal::BasicStructType>(&variant_);
      alt != nullptr) {
    return *alt;
  }
  if (const auto* alt = absl::get_if<MessageType>(&variant_); alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

absl::optional<TimestampType> Type::AsTimestamp() const {
  return GetOrNullopt<TimestampType>(variant_);
}

absl::optional<TypeParamType> Type::AsTypeParam() const {
  return GetOrNullopt<TypeParamType>(variant_);
}

absl::optional<TypeType> Type::AsType() const {
  return GetOrNullopt<TypeType>(variant_);
}

absl::optional<UintType> Type::AsUint() const {
  return GetOrNullopt<UintType>(variant_);
}

absl::optional<UintWrapperType> Type::AsUintWrapper() const {
  return GetOrNullopt<UintWrapperType>(variant_);
}

absl::optional<UnknownType> Type::AsUnknown() const {
  return GetOrNullopt<UnknownType>(variant_);
}

namespace {

template <typename T>
T GetOrDie(const common_internal::TypeVariant& variant) {
  return absl::get<T>(variant);
}

}  // namespace

AnyType Type::GetAny() const {
  ABSL_DCHECK(IsAny()) << DebugString();
  return GetOrDie<AnyType>(variant_);
}

BoolType Type::GetBool() const {
  ABSL_DCHECK(IsBool()) << DebugString();
  return GetOrDie<BoolType>(variant_);
}

BoolWrapperType Type::GetBoolWrapper() const {
  ABSL_DCHECK(IsBoolWrapper()) << DebugString();
  return GetOrDie<BoolWrapperType>(variant_);
}

BytesType Type::GetBytes() const {
  ABSL_DCHECK(IsBytes()) << DebugString();
  return GetOrDie<BytesType>(variant_);
}

BytesWrapperType Type::GetBytesWrapper() const {
  ABSL_DCHECK(IsBytesWrapper()) << DebugString();
  return GetOrDie<BytesWrapperType>(variant_);
}

DoubleType Type::GetDouble() const {
  ABSL_DCHECK(IsDouble()) << DebugString();
  return GetOrDie<DoubleType>(variant_);
}

DoubleWrapperType Type::GetDoubleWrapper() const {
  ABSL_DCHECK(IsDoubleWrapper()) << DebugString();
  return GetOrDie<DoubleWrapperType>(variant_);
}

DurationType Type::GetDuration() const {
  ABSL_DCHECK(IsDuration()) << DebugString();
  return GetOrDie<DurationType>(variant_);
}

DynType Type::GetDyn() const {
  ABSL_DCHECK(IsDyn()) << DebugString();
  return GetOrDie<DynType>(variant_);
}

EnumType Type::GetEnum() const {
  ABSL_DCHECK(IsEnum()) << DebugString();
  return GetOrDie<EnumType>(variant_);
}

ErrorType Type::GetError() const {
  ABSL_DCHECK(IsError()) << DebugString();
  return GetOrDie<ErrorType>(variant_);
}

FunctionType Type::GetFunction() const {
  ABSL_DCHECK(IsFunction()) << DebugString();
  return GetOrDie<FunctionType>(variant_);
}

IntType Type::GetInt() const {
  ABSL_DCHECK(IsInt()) << DebugString();
  return GetOrDie<IntType>(variant_);
}

IntWrapperType Type::GetIntWrapper() const {
  ABSL_DCHECK(IsIntWrapper()) << DebugString();
  return GetOrDie<IntWrapperType>(variant_);
}

ListType Type::GetList() const {
  ABSL_DCHECK(IsList()) << DebugString();
  return GetOrDie<ListType>(variant_);
}

MapType Type::GetMap() const {
  ABSL_DCHECK(IsMap()) << DebugString();
  return GetOrDie<MapType>(variant_);
}

MessageType Type::GetMessage() const {
  ABSL_DCHECK(IsMessage()) << DebugString();
  return GetOrDie<MessageType>(variant_);
}

NullType Type::GetNull() const {
  ABSL_DCHECK(IsNull()) << DebugString();
  return GetOrDie<NullType>(variant_);
}

OpaqueType Type::GetOpaque() const {
  ABSL_DCHECK(IsOpaque()) << DebugString();
  return GetOrDie<OpaqueType>(variant_);
}

OptionalType Type::GetOptional() const {
  ABSL_DCHECK(IsOptional()) << DebugString();
  return GetOrDie<OpaqueType>(variant_).GetOptional();
}

StringType Type::GetString() const {
  ABSL_DCHECK(IsString()) << DebugString();
  return GetOrDie<StringType>(variant_);
}

StringWrapperType Type::GetStringWrapper() const {
  ABSL_DCHECK(IsStringWrapper()) << DebugString();
  return GetOrDie<StringWrapperType>(variant_);
}

StructType Type::GetStruct() const {
  ABSL_DCHECK(IsStruct()) << DebugString();
  if (const auto* alt =
          absl::get_if<common_internal::BasicStructType>(&variant_);
      alt != nullptr) {
    return *alt;
  }
  if (const auto* alt = absl::get_if<MessageType>(&variant_); alt != nullptr) {
    return *alt;
  }
  return StructType();
}

TimestampType Type::GetTimestamp() const {
  ABSL_DCHECK(IsTimestamp()) << DebugString();
  return GetOrDie<TimestampType>(variant_);
}

TypeParamType Type::GetTypeParam() const {
  ABSL_DCHECK(IsTypeParam()) << DebugString();
  return GetOrDie<TypeParamType>(variant_);
}

TypeType Type::GetType() const {
  ABSL_DCHECK(IsType()) << DebugString();
  return GetOrDie<TypeType>(variant_);
}

UintType Type::GetUint() const {
  ABSL_DCHECK(IsUint()) << DebugString();
  return GetOrDie<UintType>(variant_);
}

UintWrapperType Type::GetUintWrapper() const {
  ABSL_DCHECK(IsUintWrapper()) << DebugString();
  return GetOrDie<UintWrapperType>(variant_);
}

UnknownType Type::GetUnknown() const {
  ABSL_DCHECK(IsUnknown()) << DebugString();
  return GetOrDie<UnknownType>(variant_);
}

Type Type::Unwrap() const {
  switch (kind()) {
    case TypeKind::kBoolWrapper:
      return BoolType();
    case TypeKind::kIntWrapper:
      return IntType();
    case TypeKind::kUintWrapper:
      return UintType();
    case TypeKind::kDoubleWrapper:
      return DoubleType();
    case TypeKind::kBytesWrapper:
      return BytesType();
    case TypeKind::kStringWrapper:
      return StringType();
    default:
      return *this;
  }
}

Type Type::Wrap() const {
  switch (kind()) {
    case TypeKind::kBool:
      return BoolWrapperType();
    case TypeKind::kInt:
      return IntWrapperType();
    case TypeKind::kUint:
      return UintWrapperType();
    case TypeKind::kDouble:
      return DoubleWrapperType();
    case TypeKind::kBytes:
      return BytesWrapperType();
    case TypeKind::kString:
      return StringWrapperType();
    default:
      return *this;
  }
}

namespace common_internal {

Type SingularMessageFieldType(
    const google::protobuf::FieldDescriptor* absl_nonnull descriptor) {
  ABSL_DCHECK(!descriptor->is_map());
  switch (descriptor->type()) {
    case FieldDescriptor::TYPE_BOOL:
      return BoolType();
    case FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_SINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_SINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_INT64:
      return IntType();
    case FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_UINT32:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_FIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_UINT64:
      return UintType();
    case FieldDescriptor::TYPE_FLOAT:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_DOUBLE:
      return DoubleType();
    case FieldDescriptor::TYPE_BYTES:
      return BytesType();
    case FieldDescriptor::TYPE_STRING:
      return StringType();
    case FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case FieldDescriptor::TYPE_MESSAGE:
      return Type::Message(descriptor->message_type());
    case FieldDescriptor::TYPE_ENUM:
      return Type::Enum(descriptor->enum_type());
    default:
      return Type();
  }
}

std::string BasicStructTypeField::DebugString() const {
  if (!name().empty() && number() >= 1) {
    return absl::StrCat("[", number(), "]", name());
  }
  if (!name().empty()) {
    return std::string(name());
  }
  if (number() >= 1) {
    return absl::StrCat(number());
  }
  return std::string();
}

}  // namespace common_internal

Type Type::Field(const google::protobuf::FieldDescriptor* absl_nonnull descriptor) {
  if (descriptor->is_map()) {
    return MapType(descriptor->message_type());
  }
  if (descriptor->is_repeated()) {
    return ListType(descriptor);
  }
  return common_internal::SingularMessageFieldType(descriptor);
}

std::string StructTypeField::DebugString() const {
  return absl::visit(
      [](const auto& alternative) -> std::string {
        return alternative.DebugString();
      },
      variant_);
}

absl::string_view StructTypeField::name() const {
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        return alternative.name();
      },
      variant_);
}

int32_t StructTypeField::number() const {
  return absl::visit(
      [](const auto& alternative) -> int32_t { return alternative.number(); },
      variant_);
}

Type StructTypeField::GetType() const {
  return absl::visit(
      [](const auto& alternative) -> Type { return alternative.GetType(); },
      variant_);
}

StructTypeField::operator bool() const {
  return absl::visit(
      [](const auto& alternative) -> bool {
        return static_cast<bool>(alternative);
      },
      variant_);
}

absl::optional<MessageTypeField> StructTypeField::AsMessage() const {
  if (const auto* alternative = absl::get_if<MessageTypeField>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

StructTypeField::operator MessageTypeField() const {
  ABSL_DCHECK(IsMessage());
  return absl::get<MessageTypeField>(variant_);
}

TypeParameters::TypeParameters(absl::Span<const Type> types)
    : size_(types.size()) {
  if (size_ <= 2) {
    std::memcpy(&internal_[0], types.data(), size_ * sizeof(Type));
  } else {
    external_ = types.data();
  }
}

TypeParameters::TypeParameters(const Type& element) : size_(1) {
  std::memcpy(&internal_[0], &element, sizeof(element));
}

TypeParameters::TypeParameters(const Type& key, const Type& value) : size_(2) {
  std::memcpy(&internal_[0], &key, sizeof(key));
  std::memcpy(&internal_[0] + sizeof(key), &value, sizeof(value));
}

namespace common_internal {

namespace {

constexpr absl::string_view kNullTypeName = "null_type";
constexpr absl::string_view kBoolTypeName = "bool";
constexpr absl::string_view kInt64TypeName = "int";
constexpr absl::string_view kUInt64TypeName = "uint";
constexpr absl::string_view kDoubleTypeName = "double";
constexpr absl::string_view kStringTypeName = "string";
constexpr absl::string_view kBytesTypeName = "bytes";
constexpr absl::string_view kDurationTypeName = "google.protobuf.Duration";
constexpr absl::string_view kTimestampTypeName = "google.protobuf.Timestamp";
constexpr absl::string_view kListTypeName = "list";
constexpr absl::string_view kMapTypeName = "map";
constexpr absl::string_view kCelTypeTypeName = "type";

}  // namespace

Type LegacyRuntimeType(absl::string_view name) {
  if (name == kNullTypeName) {
    return NullType{};
  }
  if (name == kBoolTypeName) {
    return BoolType{};
  }
  if (name == kInt64TypeName) {
    return IntType{};
  }
  if (name == kUInt64TypeName) {
    return UintType{};
  }
  if (name == kDoubleTypeName) {
    return DoubleType{};
  }
  if (name == kStringTypeName) {
    return StringType{};
  }
  if (name == kBytesTypeName) {
    return BytesType{};
  }
  if (name == kDurationTypeName) {
    return DurationType{};
  }
  if (name == kTimestampTypeName) {
    return TimestampType{};
  }
  if (name == kListTypeName) {
    return ListType{};
  }
  if (name == kMapTypeName) {
    return MapType{};
  }
  if (name == kCelTypeTypeName) {
    return TypeType{};
  }
  return common_internal::MakeBasicStructType(name);
}

}  // namespace common_internal

}  // namespace cel
