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

#include "common/legacy_value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "base/internal/message_wrapper.h"
#include "common/allocator.h"
#include "common/casting.h"
#include "common/internal/arena_string.h"
#include "common/json.h"
#include "common/kind.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/unknown.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "eval/internal/cel_value_equal.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/field_backed_list_impl.h"
#include "eval/public/containers/field_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/json.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/well_known_types.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace {

using google::api::expr::runtime::CelList;
using google::api::expr::runtime::CelMap;
using google::api::expr::runtime::CelValue;
using google::api::expr::runtime::FieldBackedListImpl;
using google::api::expr::runtime::FieldBackedMapImpl;
using google::api::expr::runtime::GetGenericProtoTypeInfoInstance;
using google::api::expr::runtime::LegacyTypeInfoApis;
using google::api::expr::runtime::MessageWrapper;

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

const CelList* AsCelList(uintptr_t impl) {
  return reinterpret_cast<const CelList*>(impl);
}

const CelMap* AsCelMap(uintptr_t impl) {
  return reinterpret_cast<const CelMap*>(impl);
}

MessageWrapper AsMessageWrapper(uintptr_t message_ptr, uintptr_t type_info) {
  if ((message_ptr & base_internal::kMessageWrapperTagMask) ==
      base_internal::kMessageWrapperTagMessageValue) {
    return MessageWrapper::Builder(
               static_cast<google::protobuf::Message*>(
                   reinterpret_cast<google::protobuf::MessageLite*>(
                       message_ptr & base_internal::kMessageWrapperPtrMask)))
        .Build(reinterpret_cast<const LegacyTypeInfoApis*>(type_info));
  } else {
    return MessageWrapper::Builder(
               reinterpret_cast<google::protobuf::MessageLite*>(message_ptr))
        .Build(reinterpret_cast<const LegacyTypeInfoApis*>(type_info));
  }
}

class CelListIterator final : public ValueIterator {
 public:
  CelListIterator(google::protobuf::Arena* arena, const CelList* cel_list)
      : arena_(arena), cel_list_(cel_list), size_(cel_list_->size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager&, Value& result) override {
    if (!HasNext()) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when ValueIterator::HasNext() returns "
          "false");
    }
    auto cel_value = cel_list_->Get(arena_, index_++);
    CEL_RETURN_IF_ERROR(ModernValue(arena_, cel_value, result));
    return absl::OkStatus();
  }

 private:
  google::protobuf::Arena* const arena_;
  const CelList* const cel_list_;
  const int size_;
  int index_ = 0;
};

absl::StatusOr<Json> CelValueToJson(google::protobuf::Arena* arena, CelValue value);

absl::StatusOr<JsonString> CelValueToJsonString(CelValue value) {
  switch (value.type()) {
    case CelValue::Type::kString:
      return JsonString(value.StringOrDie().value());
    default:
      return TypeConversionError(KindToString(value.type()), "string")
          .NativeValue();
  }
}

absl::StatusOr<JsonArray> CelListToJsonArray(google::protobuf::Arena* arena,
                                             const CelList* list);

absl::StatusOr<JsonObject> CelMapToJsonObject(google::protobuf::Arena* arena,
                                              const CelMap* map);

absl::StatusOr<JsonObject> MessageWrapperToJsonObject(
    google::protobuf::Arena* arena, MessageWrapper message_wrapper);

absl::StatusOr<Json> CelValueToJson(google::protobuf::Arena* arena, CelValue value) {
  switch (value.type()) {
    case CelValue::Type::kNullType:
      return kJsonNull;
    case CelValue::Type::kBool:
      return value.BoolOrDie();
    case CelValue::Type::kInt64:
      return JsonInt(value.Int64OrDie());
    case CelValue::Type::kUint64:
      return JsonUint(value.Uint64OrDie());
    case CelValue::Type::kDouble:
      return value.DoubleOrDie();
    case CelValue::Type::kString:
      return JsonString(value.StringOrDie().value());
    case CelValue::Type::kBytes:
      return JsonBytes(value.BytesOrDie().value());
    case CelValue::Type::kMessage:
      return MessageWrapperToJsonObject(arena, value.MessageWrapperOrDie());
    case CelValue::Type::kDuration: {
      CEL_ASSIGN_OR_RETURN(
          auto json, internal::EncodeDurationToJson(value.DurationOrDie()));
      return JsonString(std::move(json));
    }
    case CelValue::Type::kTimestamp: {
      CEL_ASSIGN_OR_RETURN(
          auto json, internal::EncodeTimestampToJson(value.TimestampOrDie()));
      return JsonString(std::move(json));
    }
    case CelValue::Type::kList:
      return CelListToJsonArray(arena, value.ListOrDie());
    case CelValue::Type::kMap:
      return CelMapToJsonObject(arena, value.MapOrDie());
    case CelValue::Type::kUnknownSet:
      ABSL_FALLTHROUGH_INTENDED;
    case CelValue::Type::kCelType:
      ABSL_FALLTHROUGH_INTENDED;
    case CelValue::Type::kError:
      ABSL_FALLTHROUGH_INTENDED;
    default:
      return absl::FailedPreconditionError(absl::StrCat(
          CelValue::TypeName(value.type()), " is unserializable to JSON"));
  }
}

absl::StatusOr<JsonArray> CelListToJsonArray(google::protobuf::Arena* arena,
                                             const CelList* list) {
  JsonArrayBuilder builder;
  const auto size = static_cast<size_t>(list->size());
  builder.reserve(size);
  for (size_t index = 0; index < size; ++index) {
    CEL_ASSIGN_OR_RETURN(
        auto element,
        CelValueToJson(arena, list->Get(arena, static_cast<int>(index))));
    builder.push_back(std::move(element));
  }
  return std::move(builder).Build();
}

absl::StatusOr<JsonObject> CelMapToJsonObject(google::protobuf::Arena* arena,
                                              const CelMap* map) {
  JsonObjectBuilder builder;
  const auto size = static_cast<size_t>(map->size());
  builder.reserve(size);
  CEL_ASSIGN_OR_RETURN(const auto* keys_list, map->ListKeys(arena));
  for (size_t index = 0; index < size; ++index) {
    auto key = keys_list->Get(arena, static_cast<int>(index));
    auto value = map->Get(arena, key);
    if (!value.has_value()) {
      return absl::FailedPreconditionError(
          "ListKeys() returned key not present map");
    }
    CEL_ASSIGN_OR_RETURN(auto json_key, CelValueToJsonString(key));
    CEL_ASSIGN_OR_RETURN(auto json_value, CelValueToJson(arena, *value));
    if (!builder.insert(std::pair{std::move(json_key), std::move(json_value)})
             .second) {
      return absl::FailedPreconditionError(
          "duplicate keys encountered serializing map as JSON");
    }
  }
  return std::move(builder).Build();
}

absl::StatusOr<JsonObject> MessageWrapperToJsonObject(
    google::protobuf::Arena* arena, MessageWrapper message_wrapper) {
  JsonObjectBuilder builder;
  const auto* type_info = message_wrapper.legacy_type_info();
  const auto* access_apis = type_info->GetAccessApis(message_wrapper);
  if (access_apis == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("LegacyTypeAccessApis missing for type: ",
                     type_info->GetTypename(message_wrapper)));
  }
  auto field_names = access_apis->ListFields(message_wrapper);
  builder.reserve(field_names.size());
  for (const auto& field_name : field_names) {
    CEL_ASSIGN_OR_RETURN(
        auto field,
        access_apis->GetField(field_name, message_wrapper,
                              ProtoWrapperTypeOptions::kUnsetNull,
                              extensions::ProtoMemoryManagerRef(arena)));
    CEL_ASSIGN_OR_RETURN(auto json_field, CelValueToJson(arena, field));
    builder.insert_or_assign(JsonString(field_name), std::move(json_field));
  }
  return std::move(builder).Build();
}

std::string cel_common_internal_LegacyListValue_DebugString(uintptr_t impl) {
  return CelValue::CreateList(AsCelList(impl)).DebugString();
}

absl::Status cel_common_internal_LegacyListValue_SerializeTo(
    uintptr_t impl, absl::Cord& serialized_value) {
  google::protobuf::ListValue message;
  google::protobuf::Arena arena;
  CEL_ASSIGN_OR_RETURN(auto array, CelListToJsonArray(&arena, AsCelList(impl)));
  CEL_RETURN_IF_ERROR(internal::NativeJsonListToProtoJsonList(array, &message));
  if (!message.SerializePartialToCord(&serialized_value)) {
    return absl::UnknownError("failed to serialize google.protobuf.ListValue");
  }
  return absl::OkStatus();
}

absl::StatusOr<JsonArray>
cel_common_internal_LegacyListValue_ConvertToJsonArray(uintptr_t impl) {
  google::protobuf::Arena arena;
  return CelListToJsonArray(&arena, AsCelList(impl));
}

bool cel_common_internal_LegacyListValue_IsEmpty(uintptr_t impl) {
  return AsCelList(impl)->empty();
}

size_t cel_common_internal_LegacyListValue_Size(uintptr_t impl) {
  return static_cast<size_t>(AsCelList(impl)->size());
}

absl::Status cel_common_internal_LegacyListValue_Get(
    uintptr_t impl, ValueManager& value_manager, size_t index, Value& result) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  if (ABSL_PREDICT_FALSE(index < 0 || index >= AsCelList(impl)->size())) {
    result = value_manager.CreateErrorValue(
        absl::InvalidArgumentError("index out of bounds"));
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(ModernValue(
      arena, AsCelList(impl)->Get(arena, static_cast<int>(index)), result));
  return absl::OkStatus();
}

absl::Status cel_common_internal_LegacyListValue_ForEach(
    uintptr_t impl, ValueManager& value_manager,
    ListValue::ForEachWithIndexCallback callback) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  const auto size = AsCelList(impl)->size();
  Value element;
  for (int index = 0; index < size; ++index) {
    CEL_RETURN_IF_ERROR(
        ModernValue(arena, AsCelList(impl)->Get(arena, index), element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, Value(element)));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
cel_common_internal_LegacyListValue_NewIterator(uintptr_t impl,
                                                ValueManager& value_manager) {
  return std::make_unique<CelListIterator>(
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
      AsCelList(impl));
}

absl::Status cel_common_internal_LegacyListValue_Contains(
    uintptr_t impl, ValueManager& value_manager, const Value& other,
    Value& result) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto legacy_other, LegacyValue(arena, other));
  const auto* cel_list = AsCelList(impl);
  for (int i = 0; i < cel_list->size(); ++i) {
    auto element = cel_list->Get(arena, i);
    absl::optional<bool> equal =
        interop_internal::CelValueEqualImpl(element, legacy_other);
    // Heterogenous equality behavior is to just return false if equality
    // undefined.
    if (equal.has_value() && *equal) {
      result = BoolValue{true};
      return absl::OkStatus();
    }
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

}  // namespace

namespace common_internal {

namespace {

CelValue LegacyTrivialStructValue(absl::Nonnull<google::protobuf::Arena*> arena,
                                  const Value& value) {
  if (auto legacy_struct_value = common_internal::AsLegacyStructValue(value);
      legacy_struct_value) {
    return CelValue::CreateMessageWrapper(
        AsMessageWrapper(legacy_struct_value->message_ptr(),
                         legacy_struct_value->legacy_type_info()));
  }
  if (auto parsed_message_value = value.AsParsedMessage();
      parsed_message_value) {
    auto maybe_cloned = parsed_message_value->Clone(ArenaAllocator<>{arena});
    return CelValue::CreateMessageWrapper(MessageWrapper(
        cel::to_address(maybe_cloned), &GetGenericProtoTypeInfoInstance()));
  }
  return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
      arena, absl::InvalidArgumentError(absl::StrCat(
                 "unsupported conversion from cel::StructValue to CelValue: ",
                 value.GetRuntimeType().DebugString()))));
}

CelValue LegacyTrivialListValue(absl::Nonnull<google::protobuf::Arena*> arena,
                                const Value& value) {
  if (auto legacy_list_value = common_internal::AsLegacyListValue(value);
      legacy_list_value) {
    return CelValue::CreateList(AsCelList(legacy_list_value->NativeValue()));
  }
  if (auto parsed_repeated_field_value = value.AsParsedRepeatedField();
      parsed_repeated_field_value) {
    auto maybe_cloned =
        parsed_repeated_field_value->Clone(ArenaAllocator<>{arena});
    return CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, &maybe_cloned.message(), maybe_cloned.field(), arena));
  }
  if (auto parsed_json_list_value = value.AsParsedJsonList();
      parsed_json_list_value) {
    auto maybe_cloned = parsed_json_list_value->Clone(ArenaAllocator<>{arena});
    return CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, cel::to_address(maybe_cloned),
        well_known_types::GetListValueReflectionOrDie(
            maybe_cloned->GetDescriptor())
            .GetValuesDescriptor(),
        arena));
  }
  if (auto parsed_list_value = value.AsParsedList(); parsed_list_value) {
    auto status_or_compat_list =
        common_internal::MakeCompatListValue(arena, *parsed_list_value);
    if (!status_or_compat_list.ok()) {
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, std::move(status_or_compat_list).status()));
    }
    return CelValue::CreateList(*status_or_compat_list);
  }
  return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
      arena, absl::InvalidArgumentError(absl::StrCat(
                 "unsupported conversion from cel::ListValue to CelValue: ",
                 value.GetRuntimeType().DebugString()))));
}

CelValue LegacyTrivialMapValue(absl::Nonnull<google::protobuf::Arena*> arena,
                               const Value& value) {
  if (auto legacy_map_value = common_internal::AsLegacyMapValue(value);
      legacy_map_value) {
    return CelValue::CreateMap(AsCelMap(legacy_map_value->NativeValue()));
  }
  if (auto parsed_map_field_value = value.AsParsedMapField();
      parsed_map_field_value) {
    auto maybe_cloned = parsed_map_field_value->Clone(ArenaAllocator<>{arena});
    return CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, &maybe_cloned.message(), maybe_cloned.field(), arena));
  }
  if (auto parsed_json_map_value = value.AsParsedJsonMap();
      parsed_json_map_value) {
    auto maybe_cloned = parsed_json_map_value->Clone(ArenaAllocator<>{arena});
    return CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, cel::to_address(maybe_cloned),
        well_known_types::GetStructReflectionOrDie(
            maybe_cloned->GetDescriptor())
            .GetFieldsDescriptor(),
        arena));
  }
  if (auto parsed_map_value = value.AsParsedMap(); parsed_map_value) {
    auto status_or_compat_map =
        common_internal::MakeCompatMapValue(arena, *parsed_map_value);
    if (!status_or_compat_map.ok()) {
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, std::move(status_or_compat_map).status()));
    }
    return CelValue::CreateMap(*status_or_compat_map);
  }
  return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
      arena, absl::InvalidArgumentError(absl::StrCat(
                 "unsupported conversion from cel::MapValue to CelValue: ",
                 value.GetRuntimeType().DebugString()))));
}

}  // namespace

google::api::expr::runtime::CelValue LegacyTrivialValue(
    absl::Nonnull<google::protobuf::Arena*> arena, const TrivialValue& value) {
  switch (value->kind()) {
    case ValueKind::kNull:
      return CelValue::CreateNull();
    case ValueKind::kBool:
      return CelValue::CreateBool(value->GetBool().NativeValue());
    case ValueKind::kInt:
      return CelValue::CreateInt64(value->GetInt().NativeValue());
    case ValueKind::kUint:
      return CelValue::CreateUint64(value->GetUint().NativeValue());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(value->GetDouble().NativeValue());
    case ValueKind::kString:
      return CelValue::CreateStringView(value.ToString());
    case ValueKind::kBytes:
      return CelValue::CreateBytesView(value.ToBytes());
    case ValueKind::kStruct:
      return LegacyTrivialStructValue(arena, *value);
    case ValueKind::kDuration:
      return CelValue::CreateDuration(value->GetDuration().NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(value->GetTimestamp().NativeValue());
    case ValueKind::kList:
      return LegacyTrivialListValue(arena, *value);
    case ValueKind::kMap:
      return LegacyTrivialMapValue(arena, *value);
    case ValueKind::kType:
      return CelValue::CreateCelTypeView(value->GetType().name());
    default:
      // Everything else is unsupported.
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, absl::InvalidArgumentError(absl::StrCat(
                     "unsupported conversion from cel::Value to CelValue: ",
                     value->GetRuntimeType().DebugString()))));
  }
}

}  // namespace common_internal

namespace common_internal {

std::string LegacyListValue::DebugString() const {
  return cel_common_internal_LegacyListValue_DebugString(impl_);
}

// See `ValueInterface::SerializeTo`.
absl::Status LegacyListValue::SerializeTo(AnyToJsonConverter&,
                                          absl::Cord& value) const {
  return cel_common_internal_LegacyListValue_SerializeTo(impl_, value);
}

absl::StatusOr<JsonArray> LegacyListValue::ConvertToJsonArray(
    AnyToJsonConverter&) const {
  return cel_common_internal_LegacyListValue_ConvertToJsonArray(impl_);
}

bool LegacyListValue::IsEmpty() const {
  return cel_common_internal_LegacyListValue_IsEmpty(impl_);
}

size_t LegacyListValue::Size() const {
  return cel_common_internal_LegacyListValue_Size(impl_);
}

// See LegacyListValueInterface::Get for documentation.
absl::Status LegacyListValue::Get(ValueManager& value_manager, size_t index,
                                  Value& result) const {
  return cel_common_internal_LegacyListValue_Get(impl_, value_manager, index,
                                                 result);
}

absl::Status LegacyListValue::ForEach(ValueManager& value_manager,
                                      ForEachWithIndexCallback callback) const {
  return cel_common_internal_LegacyListValue_ForEach(impl_, value_manager,
                                                     callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyListValue::NewIterator(
    ValueManager& value_manager) const {
  return cel_common_internal_LegacyListValue_NewIterator(impl_, value_manager);
}

absl::Status LegacyListValue::Contains(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  return cel_common_internal_LegacyListValue_Contains(impl_, value_manager,
                                                      other, result);
}

}  // namespace common_internal

namespace {

std::string cel_common_internal_LegacyMapValue_DebugString(uintptr_t impl) {
  return CelValue::CreateMap(AsCelMap(impl)).DebugString();
}

absl::Status cel_common_internal_LegacyMapValue_SerializeTo(
    uintptr_t impl, absl::Cord& serialized_value) {
  google::protobuf::Struct message;
  google::protobuf::Arena arena;
  CEL_ASSIGN_OR_RETURN(auto object, CelMapToJsonObject(&arena, AsCelMap(impl)));
  CEL_RETURN_IF_ERROR(internal::NativeJsonMapToProtoJsonMap(object, &message));
  if (!message.SerializePartialToCord(&serialized_value)) {
    return absl::UnknownError("failed to serialize google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::StatusOr<JsonObject>
cel_common_internal_LegacyMapValue_ConvertToJsonObject(uintptr_t impl) {
  google::protobuf::Arena arena;
  return CelMapToJsonObject(&arena, AsCelMap(impl));
}

bool cel_common_internal_LegacyMapValue_IsEmpty(uintptr_t impl) {
  return AsCelMap(impl)->empty();
}

size_t cel_common_internal_LegacyMapValue_Size(uintptr_t impl) {
  return static_cast<size_t>(AsCelMap(impl)->size());
}

absl::StatusOr<bool> cel_common_internal_LegacyMapValue_Find(
    uintptr_t impl, ValueManager& value_manager, const Value& key,
    Value& result) {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return false;
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  auto cel_value = AsCelMap(impl)->Get(arena, cel_key);
  if (!cel_value.has_value()) {
    result = NullValue{};
    return false;
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, result));
  return true;
}

absl::Status cel_common_internal_LegacyMapValue_Get(uintptr_t impl,
                                                    ValueManager& value_manager,
                                                    const Value& key,
                                                    Value& result) {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return absl::OkStatus();
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  auto cel_value = AsCelMap(impl)->Get(arena, cel_key);
  if (!cel_value.has_value()) {
    result = NoSuchKeyError(key.DebugString());
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, result));
  return absl::OkStatus();
}

absl::Status cel_common_internal_LegacyMapValue_Has(uintptr_t impl,
                                                    ValueManager& value_manager,
                                                    const Value& key,
                                                    Value& result) {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return absl::OkStatus();
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  CEL_ASSIGN_OR_RETURN(auto has, AsCelMap(impl)->Has(cel_key));
  result = BoolValue{has};
  return absl::OkStatus();
}

absl::Status cel_common_internal_LegacyMapValue_ListKeys(
    uintptr_t impl, ValueManager& value_manager, ListValue& result) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto keys, AsCelMap(impl)->ListKeys(arena));
  result = ListValue{
      common_internal::LegacyListValue{reinterpret_cast<uintptr_t>(keys)}};
  return absl::OkStatus();
}

absl::Status cel_common_internal_LegacyMapValue_ForEach(
    uintptr_t impl, ValueManager& value_manager,
    MapValue::ForEachCallback callback) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto keys, AsCelMap(impl)->ListKeys(arena));
  const auto size = keys->size();
  Value key;
  Value value;
  for (int index = 0; index < size; ++index) {
    auto cel_key = keys->Get(arena, index);
    auto cel_value = *AsCelMap(impl)->Get(arena, cel_key);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_key, key));
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
cel_common_internal_LegacyMapValue_NewIterator(uintptr_t impl,
                                               ValueManager& value_manager) {
  auto* arena =
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager());
  CEL_ASSIGN_OR_RETURN(auto keys, AsCelMap(impl)->ListKeys(arena));
  return cel_common_internal_LegacyListValue_NewIterator(
      reinterpret_cast<uintptr_t>(keys), value_manager);
}

}  // namespace

namespace common_internal {

std::string LegacyMapValue::DebugString() const {
  return cel_common_internal_LegacyMapValue_DebugString(impl_);
}

absl::Status LegacyMapValue::SerializeTo(AnyToJsonConverter&,
                                         absl::Cord& value) const {
  return cel_common_internal_LegacyMapValue_SerializeTo(impl_, value);
}

absl::StatusOr<JsonObject> LegacyMapValue::ConvertToJsonObject(
    AnyToJsonConverter&) const {
  return cel_common_internal_LegacyMapValue_ConvertToJsonObject(impl_);
}

bool LegacyMapValue::IsEmpty() const {
  return cel_common_internal_LegacyMapValue_IsEmpty(impl_);
}

size_t LegacyMapValue::Size() const {
  return cel_common_internal_LegacyMapValue_Size(impl_);
}

absl::Status LegacyMapValue::Get(ValueManager& value_manager, const Value& key,
                                 Value& result) const {
  return cel_common_internal_LegacyMapValue_Get(impl_, value_manager, key,
                                                result);
}

absl::StatusOr<bool> LegacyMapValue::Find(ValueManager& value_manager,
                                          const Value& key,
                                          Value& result) const {
  return cel_common_internal_LegacyMapValue_Find(impl_, value_manager, key,
                                                 result);
}

absl::Status LegacyMapValue::Has(ValueManager& value_manager, const Value& key,
                                 Value& result) const {
  return cel_common_internal_LegacyMapValue_Has(impl_, value_manager, key,
                                                result);
}

absl::Status LegacyMapValue::ListKeys(ValueManager& value_manager,
                                      ListValue& result) const {
  return cel_common_internal_LegacyMapValue_ListKeys(impl_, value_manager,
                                                     result);
}

absl::Status LegacyMapValue::ForEach(ValueManager& value_manager,
                                     ForEachCallback callback) const {
  return cel_common_internal_LegacyMapValue_ForEach(impl_, value_manager,
                                                    callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyMapValue::NewIterator(
    ValueManager& value_manager) const {
  return cel_common_internal_LegacyMapValue_NewIterator(impl_, value_manager);
}

}  // namespace common_internal

namespace {

std::string cel_common_internal_LegacyStructValue_DebugString(
    uintptr_t message_ptr, uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  return message_wrapper.legacy_type_info()->DebugString(message_wrapper);
}

absl::Status cel_common_internal_LegacyStructValue_SerializeTo(
    uintptr_t message_ptr, uintptr_t type_info, absl::Cord& value) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  if (ABSL_PREDICT_TRUE(
          message_wrapper.message_ptr()->SerializePartialToCord(&value))) {
    return absl::OkStatus();
  }
  return absl::UnknownError("failed to serialize protocol buffer message");
}

absl::string_view cel_common_internal_LegacyStructValue_GetTypeName(
    uintptr_t message_ptr, uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  return message_wrapper.legacy_type_info()->GetTypename(message_wrapper);
}

absl::StatusOr<JsonObject>
cel_common_internal_LegacyStructValue_ConvertToJsonObject(uintptr_t message_ptr,
                                                          uintptr_t type_info) {
  google::protobuf::Arena arena;
  return MessageWrapperToJsonObject(&arena,
                                    AsMessageWrapper(message_ptr, type_info));
}

absl::Status cel_common_internal_LegacyStructValue_GetFieldByName(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    result = NoSuchFieldError(name);
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(
      auto cel_value,
      access_apis->GetField(name, message_wrapper, unboxing_options,
                            value_manager.GetMemoryManager()));
  CEL_RETURN_IF_ERROR(ModernValue(
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
      cel_value, result));
  return absl::OkStatus();
}

absl::Status cel_common_internal_LegacyStructValue_GetFieldByNumber(
    uintptr_t, uintptr_t, ValueManager&, int64_t, Value&,
    ProtoWrapperTypeOptions) {
  return absl::UnimplementedError(
      "access to fields by numbers is not available for legacy structs");
}

absl::StatusOr<bool> cel_common_internal_LegacyStructValue_HasFieldByName(
    uintptr_t message_ptr, uintptr_t type_info, absl::string_view name) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return NoSuchFieldError(name).NativeValue();
  }
  return access_apis->HasField(name, message_wrapper);
}

absl::StatusOr<bool> cel_common_internal_LegacyStructValue_HasFieldByNumber(
    uintptr_t, uintptr_t, int64_t) {
  return absl::UnimplementedError(
      "access to fields by numbers is not available for legacy structs");
}

absl::Status cel_common_internal_LegacyStructValue_Equal(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    const Value& other, Value& result) {
  if (auto legacy_struct_value = common_internal::AsLegacyStructValue(other);
      legacy_struct_value.has_value()) {
    auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
    const auto* access_apis =
        message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
    if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
      return absl::UnimplementedError(
          absl::StrCat("legacy access APIs missing for ",
                       cel_common_internal_LegacyStructValue_GetTypeName(
                           message_ptr, type_info)));
    }
    auto other_message_wrapper =
        AsMessageWrapper(legacy_struct_value->message_ptr(),
                         legacy_struct_value->legacy_type_info());
    result = BoolValue{
        access_apis->IsEqualTo(message_wrapper, other_message_wrapper)};
    return absl::OkStatus();
  }
  if (auto struct_value = As<StructValue>(other); struct_value.has_value()) {
    return common_internal::StructValueEqual(
        value_manager,
        common_internal::LegacyStructValue(message_ptr, type_info),
        *struct_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

bool cel_common_internal_LegacyStructValue_IsZeroValue(uintptr_t message_ptr,
                                                       uintptr_t type_info) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return false;
  }
  return access_apis->ListFields(message_wrapper).empty();
}

absl::Status cel_common_internal_LegacyStructValue_ForEachField(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    StructValue::ForEachFieldCallback callback) {
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return absl::UnimplementedError(
        absl::StrCat("legacy access APIs missing for ",
                     cel_common_internal_LegacyStructValue_GetTypeName(
                         message_ptr, type_info)));
  }
  auto field_names = access_apis->ListFields(message_wrapper);
  Value value;
  for (const auto& field_name : field_names) {
    CEL_ASSIGN_OR_RETURN(
        auto cel_value,
        access_apis->GetField(field_name, message_wrapper,
                              ProtoWrapperTypeOptions::kUnsetNull,
                              value_manager.GetMemoryManager()));
    CEL_RETURN_IF_ERROR(ModernValue(
        extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
        cel_value, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(field_name, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<int> cel_common_internal_LegacyStructValue_Qualify(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    Value& result) {
  if (ABSL_PREDICT_FALSE(qualifiers.empty())) {
    return absl::InvalidArgumentError("invalid select qualifier path.");
  }
  auto message_wrapper = AsMessageWrapper(message_ptr, type_info);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    absl::string_view field_name = absl::visit(
        absl::Overload(
            [](const FieldSpecifier& field) -> absl::string_view {
              return field.name;
            },
            [](const AttributeQualifier& field) -> absl::string_view {
              return field.GetStringKey().value_or("<invalid field>");
            }),
        qualifiers.front());
    result = NoSuchFieldError(field_name);
    return -1;
  }
  CEL_ASSIGN_OR_RETURN(
      auto legacy_result,
      access_apis->Qualify(qualifiers, message_wrapper, presence_test,
                           value_manager.GetMemoryManager()));
  CEL_RETURN_IF_ERROR(ModernValue(
      extensions::ProtoMemoryManagerArena(value_manager.GetMemoryManager()),
      legacy_result.value, result));
  return legacy_result.qualifier_count;
}

}  // namespace

namespace common_internal {

absl::string_view LegacyStructValue::GetTypeName() const {
  return cel_common_internal_LegacyStructValue_GetTypeName(message_ptr_,
                                                           type_info_);
}

std::string LegacyStructValue::DebugString() const {
  return cel_common_internal_LegacyStructValue_DebugString(message_ptr_,
                                                           type_info_);
}

absl::Status LegacyStructValue::SerializeTo(AnyToJsonConverter&,
                                            absl::Cord& value) const {
  return cel_common_internal_LegacyStructValue_SerializeTo(message_ptr_,
                                                           type_info_, value);
}

absl::StatusOr<Json> LegacyStructValue::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  return cel_common_internal_LegacyStructValue_ConvertToJsonObject(message_ptr_,
                                                                   type_info_);
}

absl::Status LegacyStructValue::Equal(ValueManager& value_manager,
                                      const Value& other, Value& result) const {
  return cel_common_internal_LegacyStructValue_Equal(
      message_ptr_, type_info_, value_manager, other, result);
}

bool LegacyStructValue::IsZeroValue() const {
  return cel_common_internal_LegacyStructValue_IsZeroValue(message_ptr_,
                                                           type_info_);
}

absl::Status LegacyStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return cel_common_internal_LegacyStructValue_GetFieldByName(
      message_ptr_, type_info_, value_manager, name, result, unboxing_options);
}

absl::Status LegacyStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return cel_common_internal_LegacyStructValue_GetFieldByNumber(
      message_ptr_, type_info_, value_manager, number, result,
      unboxing_options);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByName(
    absl::string_view name) const {
  return cel_common_internal_LegacyStructValue_HasFieldByName(message_ptr_,
                                                              type_info_, name);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByNumber(int64_t number) const {
  return cel_common_internal_LegacyStructValue_HasFieldByNumber(
      message_ptr_, type_info_, number);
}

absl::Status LegacyStructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return cel_common_internal_LegacyStructValue_ForEachField(
      message_ptr_, type_info_, value_manager, callback);
}

absl::StatusOr<int> LegacyStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  return cel_common_internal_LegacyStructValue_Qualify(
      message_ptr_, type_info_, value_manager, qualifiers, presence_test,
      result);
}

}  // namespace common_internal

absl::Status ModernValue(google::protobuf::Arena* arena,
                         google::api::expr::runtime::CelValue legacy_value,
                         Value& result) {
  switch (legacy_value.type()) {
    case CelValue::Type::kNullType:
      result = NullValue{};
      return absl::OkStatus();
    case CelValue::Type::kBool:
      result = BoolValue{legacy_value.BoolOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kInt64:
      result = IntValue{legacy_value.Int64OrDie()};
      return absl::OkStatus();
    case CelValue::Type::kUint64:
      result = UintValue{legacy_value.Uint64OrDie()};
      return absl::OkStatus();
    case CelValue::Type::kDouble:
      result = DoubleValue{legacy_value.DoubleOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kString:
      result = StringValue{
          common_internal::ArenaString(legacy_value.StringOrDie().value())};
      return absl::OkStatus();
    case CelValue::Type::kBytes:
      result = BytesValue{
          common_internal::ArenaString(legacy_value.BytesOrDie().value())};
      return absl::OkStatus();
    case CelValue::Type::kMessage: {
      auto message_wrapper = legacy_value.MessageWrapperOrDie();
      result = common_internal::LegacyStructValue{
          reinterpret_cast<uintptr_t>(message_wrapper.message_ptr()) |
              (message_wrapper.HasFullProto()
                   ? base_internal::kMessageWrapperTagMessageValue
                   : uintptr_t{0}),
          reinterpret_cast<uintptr_t>(message_wrapper.legacy_type_info())};
      return absl::OkStatus();
    }
    case CelValue::Type::kDuration:
      result = DurationValue{legacy_value.DurationOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kTimestamp:
      result = TimestampValue{legacy_value.TimestampOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kList:
      result = ListValue{common_internal::LegacyListValue{
          reinterpret_cast<uintptr_t>(legacy_value.ListOrDie())}};
      return absl::OkStatus();
    case CelValue::Type::kMap:
      result = MapValue{common_internal::LegacyMapValue{
          reinterpret_cast<uintptr_t>(legacy_value.MapOrDie())}};
      return absl::OkStatus();
    case CelValue::Type::kUnknownSet:
      result = UnknownValue{*legacy_value.UnknownSetOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kCelType: {
      result = TypeValue{common_internal::LegacyRuntimeType(
          legacy_value.CelTypeOrDie().value())};
      return absl::OkStatus();
    }
    case CelValue::Type::kError:
      result = ErrorValue{*legacy_value.ErrorOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kAny:
      return absl::InternalError(absl::StrCat(
          "illegal attempt to convert special CelValue type ",
          CelValue::TypeName(legacy_value.type()), " to cel::Value"));
    default:
      break;
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "cel::Value does not support ", KindToString(legacy_value.type())));
}

absl::StatusOr<google::api::expr::runtime::CelValue> LegacyValue(
    google::protobuf::Arena* arena, const Value& modern_value) {
  switch (modern_value.kind()) {
    case ValueKind::kNull:
      return CelValue::CreateNull();
    case ValueKind::kBool:
      return CelValue::CreateBool(Cast<BoolValue>(modern_value).NativeValue());
    case ValueKind::kInt:
      return CelValue::CreateInt64(Cast<IntValue>(modern_value).NativeValue());
    case ValueKind::kUint:
      return CelValue::CreateUint64(
          Cast<UintValue>(modern_value).NativeValue());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(
          Cast<DoubleValue>(modern_value).NativeValue());
    case ValueKind::kString: {
      const auto& string_value = Cast<StringValue>(modern_value);
      if (common_internal::AsSharedByteString(string_value).IsPooledString()) {
        return CelValue::CreateStringView(
            common_internal::AsSharedByteString(string_value).AsStringView());
      }
      return string_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateString(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kBytes: {
      const auto& bytes_value = Cast<BytesValue>(modern_value);
      if (common_internal::AsSharedByteString(bytes_value).IsPooledString()) {
        return CelValue::CreateBytesView(
            common_internal::AsSharedByteString(bytes_value).AsStringView());
      }
      return bytes_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateBytes(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kStruct:
      return common_internal::LegacyTrivialStructValue(arena, modern_value);
    case ValueKind::kDuration:
      return CelValue::CreateUncheckedDuration(
          Cast<DurationValue>(modern_value).NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(
          Cast<TimestampValue>(modern_value).NativeValue());
    case ValueKind::kList:
      return common_internal::LegacyTrivialListValue(arena, modern_value);
    case ValueKind::kMap:
      return common_internal::LegacyTrivialMapValue(arena, modern_value);
    case ValueKind::kUnknown:
      return CelValue::CreateUnknownSet(google::protobuf::Arena::Create<Unknown>(
          arena, Cast<UnknownValue>(modern_value).NativeValue()));
    case ValueKind::kType:
      return CelValue::CreateCelType(
          CelValue::CelTypeHolder(google::protobuf::Arena::Create<std::string>(
              arena, Cast<TypeValue>(modern_value).NativeValue().name())));
    case ValueKind::kError:
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, Cast<ErrorValue>(modern_value).NativeValue()));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("google::api::expr::runtime::CelValue does not support ",
                       ValueKindToString(modern_value.kind())));
  }
}

namespace interop_internal {

absl::StatusOr<Value> FromLegacyValue(google::protobuf::Arena* arena,
                                      const CelValue& legacy_value, bool) {
  switch (legacy_value.type()) {
    case CelValue::Type::kNullType:
      return NullValue{};
    case CelValue::Type::kBool:
      return BoolValue(legacy_value.BoolOrDie());
    case CelValue::Type::kInt64:
      return IntValue(legacy_value.Int64OrDie());
    case CelValue::Type::kUint64:
      return UintValue(legacy_value.Uint64OrDie());
    case CelValue::Type::kDouble:
      return DoubleValue(legacy_value.DoubleOrDie());
    case CelValue::Type::kString:
      return StringValue(
          common_internal::ArenaString(legacy_value.StringOrDie().value()));
    case CelValue::Type::kBytes:
      return BytesValue(
          common_internal::ArenaString(legacy_value.BytesOrDie().value()));
    case CelValue::Type::kMessage: {
      auto message_wrapper = legacy_value.MessageWrapperOrDie();
      return common_internal::LegacyStructValue{
          reinterpret_cast<uintptr_t>(message_wrapper.message_ptr()) |
              (message_wrapper.HasFullProto()
                   ? base_internal::kMessageWrapperTagMessageValue
                   : uintptr_t{0}),
          reinterpret_cast<uintptr_t>(message_wrapper.legacy_type_info())};
    }
    case CelValue::Type::kDuration:
      return DurationValue(legacy_value.DurationOrDie());
    case CelValue::Type::kTimestamp:
      return TimestampValue(legacy_value.TimestampOrDie());
    case CelValue::Type::kList:
      return ListValue{common_internal::LegacyListValue{
          reinterpret_cast<uintptr_t>(legacy_value.ListOrDie())}};
    case CelValue::Type::kMap:
      return MapValue{common_internal::LegacyMapValue{
          reinterpret_cast<uintptr_t>(legacy_value.MapOrDie())}};
    case CelValue::Type::kUnknownSet:
      return UnknownValue{*legacy_value.UnknownSetOrDie()};
    case CelValue::Type::kCelType:
      return CreateTypeValueFromView(arena,
                                     legacy_value.CelTypeOrDie().value());
    case CelValue::Type::kError:
      return ErrorValue(*legacy_value.ErrorOrDie());
    case CelValue::Type::kAny:
      return absl::InternalError(absl::StrCat(
          "illegal attempt to convert special CelValue type ",
          CelValue::TypeName(legacy_value.type()), " to cel::Value"));
    default:
      break;
  }
  return absl::UnimplementedError(absl::StrCat(
      "conversion from CelValue to cel::Value for type ",
      CelValue::TypeName(legacy_value.type()), " is not yet implemented"));
}

absl::StatusOr<google::api::expr::runtime::CelValue> ToLegacyValue(
    google::protobuf::Arena* arena, const Value& value, bool) {
  switch (value.kind()) {
    case ValueKind::kNull:
      return CelValue::CreateNull();
    case ValueKind::kBool:
      return CelValue::CreateBool(Cast<BoolValue>(value).NativeValue());
    case ValueKind::kInt:
      return CelValue::CreateInt64(Cast<IntValue>(value).NativeValue());
    case ValueKind::kUint:
      return CelValue::CreateUint64(Cast<UintValue>(value).NativeValue());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(Cast<DoubleValue>(value).NativeValue());
    case ValueKind::kString: {
      const auto& string_value = Cast<StringValue>(value);
      if (common_internal::AsSharedByteString(string_value).IsPooledString()) {
        return CelValue::CreateStringView(
            common_internal::AsSharedByteString(string_value).AsStringView());
      }
      return string_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateString(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateString(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kBytes: {
      const auto& bytes_value = Cast<BytesValue>(value);
      if (common_internal::AsSharedByteString(bytes_value).IsPooledString()) {
        return CelValue::CreateBytesView(
            common_internal::AsSharedByteString(bytes_value).AsStringView());
      }
      return bytes_value.NativeValue(absl::Overload(
          [arena](absl::string_view string) -> CelValue {
            return CelValue::CreateBytes(
                google::protobuf::Arena::Create<std::string>(arena, string));
          },
          [arena](const absl::Cord& string) -> CelValue {
            return CelValue::CreateBytes(google::protobuf::Arena::Create<std::string>(
                arena, static_cast<std::string>(string)));
          }));
    }
    case ValueKind::kStruct:
      return common_internal::LegacyTrivialStructValue(arena, value);
    case ValueKind::kDuration:
      return CelValue::CreateUncheckedDuration(
          Cast<DurationValue>(value).NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(
          Cast<TimestampValue>(value).NativeValue());
    case ValueKind::kList:
      return common_internal::LegacyTrivialListValue(arena, value);
    case ValueKind::kMap:
      return common_internal::LegacyTrivialMapValue(arena, value);
    case ValueKind::kUnknown:
      return CelValue::CreateUnknownSet(google::protobuf::Arena::Create<Unknown>(
          arena, Cast<UnknownValue>(value).NativeValue()));
    case ValueKind::kType:
      return CelValue::CreateCelType(
          CelValue::CelTypeHolder(google::protobuf::Arena::Create<std::string>(
              arena, Cast<TypeValue>(value).NativeValue().name())));
    case ValueKind::kError:
      return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
          arena, Cast<ErrorValue>(value).NativeValue()));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("google::api::expr::runtime::CelValue does not support ",
                       ValueKindToString(value.kind())));
  }
}

Value LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena, const google::api::expr::runtime::CelValue& value,
    bool unchecked) {
  auto status_or_value = FromLegacyValue(arena, value, unchecked);
  ABSL_CHECK_OK(status_or_value.status());  // Crash OK
  return std::move(*status_or_value);
}

std::vector<Value> LegacyValueToModernValueOrDie(
    google::protobuf::Arena* arena,
    absl::Span<const google::api::expr::runtime::CelValue> values,
    bool unchecked) {
  std::vector<Value> modern_values;
  modern_values.reserve(values.size());
  for (const auto& value : values) {
    modern_values.push_back(
        LegacyValueToModernValueOrDie(arena, value, unchecked));
  }
  return modern_values;
}

google::api::expr::runtime::CelValue ModernValueToLegacyValueOrDie(
    google::protobuf::Arena* arena, const Value& value, bool unchecked) {
  auto status_or_value = ToLegacyValue(arena, value, unchecked);
  ABSL_CHECK_OK(status_or_value.status());  // Crash OK
  return std::move(*status_or_value);
}

TypeValue CreateTypeValueFromView(google::protobuf::Arena* arena,
                                  absl::string_view input) {
  return common_internal::LegacyRuntimeType(input);
}

}  // namespace interop_internal

}  // namespace cel
