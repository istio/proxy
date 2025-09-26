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
#include "common/casting.h"
#include "common/kind.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/unknown.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "common/values/values.h"
#include "eval/internal/cel_value_equal.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/field_backed_list_impl.h"
#include "eval/public/containers/field_backed_map_impl.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrap_util.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "internal/json.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

// TODO(uncreated-issue/76): improve coverage for JSON/Any handling

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
using ::google::api::expr::runtime::internal::MaybeWrapValueToMessage;

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

MessageWrapper AsMessageWrapper(
    const google::protobuf::Message* absl_nullability_unknown message_ptr,
    const LegacyTypeInfoApis* absl_nullability_unknown type_info) {
  return MessageWrapper(message_ptr, type_info);
}

class CelListIterator final : public ValueIterator {
 public:
  explicit CelListIterator(const CelList* cel_list)
      : cel_list_(cel_list), size_(cel_list_->size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (!HasNext()) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when ValueIterator::HasNext() returns "
          "false");
    }
    auto cel_value = cel_list_->Get(arena, index_);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, *result));
    ++index_;
    return absl::OkStatus();
  }

  absl::StatusOr<bool> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull key_or_value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key_or_value != nullptr);

    if (index_ >= size_) {
      return false;
    }
    auto cel_value = cel_list_->Get(arena, index_);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, *key_or_value));
    ++index_;
    return true;
  }

  absl::StatusOr<bool> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull key,
      Value* absl_nullable value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key != nullptr);

    if (index_ >= size_) {
      return false;
    }
    if (value != nullptr) {
      auto cel_value = cel_list_->Get(arena, index_);
      CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, *value));
    }
    *key = IntValue(index_);
    ++index_;
    return true;
  }

 private:
  const CelList* const cel_list_;
  const int size_;
  int index_ = 0;
};

class CelMapIterator final : public ValueIterator {
 public:
  explicit CelMapIterator(const CelMap* cel_map)
      : cel_map_(cel_map), size_(cel_map->size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (!HasNext()) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when ValueIterator::HasNext() returns "
          "false");
    }
    CEL_RETURN_IF_ERROR(ProjectKeys(arena));
    auto cel_value = (*cel_list_)->Get(arena, index_);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, *result));
    ++index_;
    return absl::OkStatus();
  }

  absl::StatusOr<bool> Next1(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull key_or_value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key_or_value != nullptr);

    if (index_ >= size_) {
      return false;
    }
    CEL_RETURN_IF_ERROR(ProjectKeys(arena));
    auto cel_value = (*cel_list_)->Get(arena, index_);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, *key_or_value));
    ++index_;
    return true;
  }

  absl::StatusOr<bool> Next2(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull key,
      Value* absl_nullable value) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(key != nullptr);

    if (index_ >= size_) {
      return false;
    }
    CEL_RETURN_IF_ERROR(ProjectKeys(arena));
    auto cel_key = (*cel_list_)->Get(arena, index_);
    if (value != nullptr) {
      auto cel_value = cel_map_->Get(arena, cel_key);
      if (!cel_value) {
        return absl::DataLossError(
            "map iterator returned key that was not present in the map");
      }
      CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, *value));
    }
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_key, *key));
    ++index_;
    return true;
  }

 private:
  absl::Status ProjectKeys(google::protobuf::Arena* arena) {
    if (cel_list_.ok() && *cel_list_ == nullptr) {
      cel_list_ = cel_map_->ListKeys(arena);
    }
    return cel_list_.status();
  }

  const CelMap* const cel_map_;
  const int size_ = 0;
  absl::StatusOr<const CelList*> cel_list_ = nullptr;
  int index_ = 0;
};

}  // namespace

namespace common_internal {

namespace {

CelValue LegacyTrivialStructValue(google::protobuf::Arena* absl_nonnull arena,
                                  const Value& value) {
  if (auto legacy_struct_value = common_internal::AsLegacyStructValue(value);
      legacy_struct_value) {
    return CelValue::CreateMessageWrapper(
        AsMessageWrapper(legacy_struct_value->message_ptr(),
                         legacy_struct_value->legacy_type_info()));
  }
  if (auto parsed_message_value = value.AsParsedMessage();
      parsed_message_value) {
    auto maybe_cloned = parsed_message_value->Clone(arena);
    return CelValue::CreateMessageWrapper(MessageWrapper(
        cel::to_address(maybe_cloned), &GetGenericProtoTypeInfoInstance()));
  }
  return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
      arena, absl::InvalidArgumentError(absl::StrCat(
                 "unsupported conversion from cel::StructValue to CelValue: ",
                 value.GetRuntimeType().DebugString()))));
}

CelValue LegacyTrivialListValue(google::protobuf::Arena* absl_nonnull arena,
                                const Value& value) {
  if (auto legacy_list_value = common_internal::AsLegacyListValue(value);
      legacy_list_value) {
    return CelValue::CreateList(legacy_list_value->cel_list());
  }
  if (auto parsed_repeated_field_value = value.AsParsedRepeatedField();
      parsed_repeated_field_value) {
    auto maybe_cloned = parsed_repeated_field_value->Clone(arena);
    return CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, &maybe_cloned.message(), maybe_cloned.field(), arena));
  }
  if (auto parsed_json_list_value = value.AsParsedJsonList();
      parsed_json_list_value) {
    auto maybe_cloned = parsed_json_list_value->Clone(arena);
    return CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, cel::to_address(maybe_cloned),
        well_known_types::GetListValueReflectionOrDie(
            maybe_cloned->GetDescriptor())
            .GetValuesDescriptor(),
        arena));
  }
  if (auto custom_list_value = value.AsCustomList(); custom_list_value) {
    auto status_or_compat_list = common_internal::MakeCompatListValue(
        *custom_list_value, google::protobuf::DescriptorPool::generated_pool(),
        google::protobuf::MessageFactory::generated_factory(), arena);
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

CelValue LegacyTrivialMapValue(google::protobuf::Arena* absl_nonnull arena,
                               const Value& value) {
  if (auto legacy_map_value = common_internal::AsLegacyMapValue(value);
      legacy_map_value) {
    return CelValue::CreateMap(legacy_map_value->cel_map());
  }
  if (auto parsed_map_field_value = value.AsParsedMapField();
      parsed_map_field_value) {
    auto maybe_cloned = parsed_map_field_value->Clone(arena);
    return CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, &maybe_cloned.message(), maybe_cloned.field(), arena));
  }
  if (auto parsed_json_map_value = value.AsParsedJsonMap();
      parsed_json_map_value) {
    auto maybe_cloned = parsed_json_map_value->Clone(arena);
    return CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, cel::to_address(maybe_cloned),
        well_known_types::GetStructReflectionOrDie(
            maybe_cloned->GetDescriptor())
            .GetFieldsDescriptor(),
        arena));
  }
  if (auto custom_map_value = value.AsCustomMap(); custom_map_value) {
    auto status_or_compat_map = common_internal::MakeCompatMapValue(
        *custom_map_value, google::protobuf::DescriptorPool::generated_pool(),
        google::protobuf::MessageFactory::generated_factory(), arena);
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

google::api::expr::runtime::CelValue UnsafeLegacyValue(
    const Value& value, bool stable, google::protobuf::Arena* absl_nonnull arena) {
  switch (value.kind()) {
    case ValueKind::kNull:
      return CelValue::CreateNull();
    case ValueKind::kBool:
      return CelValue::CreateBool(value.GetBool());
    case ValueKind::kInt:
      return CelValue::CreateInt64(value.GetInt());
    case ValueKind::kUint:
      return CelValue::CreateUint64(value.GetUint());
    case ValueKind::kDouble:
      return CelValue::CreateDouble(value.GetDouble());
    case ValueKind::kString:
      return CelValue::CreateStringView(
          LegacyStringValue(value.GetString(), stable, arena));
    case ValueKind::kBytes:
      return CelValue::CreateBytesView(
          LegacyBytesValue(value.GetBytes(), stable, arena));
    case ValueKind::kStruct:
      return LegacyTrivialStructValue(arena, value);
    case ValueKind::kDuration:
      return CelValue::CreateDuration(value.GetDuration().ToDuration());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(value.GetTimestamp().ToTime());
    case ValueKind::kList:
      return LegacyTrivialListValue(arena, value);
    case ValueKind::kMap:
      return LegacyTrivialMapValue(arena, value);
    case ValueKind::kType:
      return CelValue::CreateCelTypeView(value.GetType().name());
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
  return CelValue::CreateList(impl_).DebugString();
}

// See `ValueInterface::SerializeTo`.
absl::Status LegacyListValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  const google::protobuf::Descriptor* descriptor =
      descriptor_pool->FindMessageTypeByName("google.protobuf.ListValue");
  if (descriptor == nullptr) {
    return absl::InternalError(
        "unable to locate descriptor for message type: "
        "google.protobuf.ListValue");
  }

  google::protobuf::Arena arena;
  const google::protobuf::Message* wrapped = MaybeWrapValueToMessage(
      descriptor, message_factory, CelValue::CreateList(impl_), &arena);
  if (wrapped == nullptr) {
    return absl::UnknownError("failed to convert legacy map to JSON");
  }
  if (!wrapped->SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", wrapped->GetTypeName()));
  }
  return absl::OkStatus();
}

absl::Status LegacyListValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);
    ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                   google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

    google::protobuf::Arena arena;
    const google::protobuf::Message* wrapped =
        MaybeWrapValueToMessage(json->GetDescriptor(), message_factory,
                                CelValue::CreateList(impl_), &arena);
    if (wrapped == nullptr) {
      return absl::UnknownError("failed to convert legacy list to JSON");
    }

    if (wrapped->GetDescriptor() == json->GetDescriptor()) {
      // We can directly use google::protobuf::Message::Copy().
      json->CopyFrom(*wrapped);
    } else {
      // Equivalent descriptors but not identical. Must serialize and
      // deserialize.
      absl::Cord serialized;
      if (!wrapped->SerializePartialToCord(&serialized)) {
        return absl::UnknownError(absl::StrCat("failed to serialize message: ",
                                               wrapped->GetTypeName()));
      }
      if (!json->ParsePartialFromCord(serialized)) {
        return absl::UnknownError(
            absl::StrCat("failed to parsed message: ", json->GetTypeName()));
      }
    }
    return absl::OkStatus();
  }
}

absl::Status LegacyListValue::ConvertToJsonArray(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);
    ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                   google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

    google::protobuf::Arena arena;
    const google::protobuf::Message* wrapped =
        MaybeWrapValueToMessage(json->GetDescriptor(), message_factory,
                                CelValue::CreateList(impl_), &arena);
    if (wrapped == nullptr) {
      return absl::UnknownError("failed to convert legacy list to JSON");
    }

    if (wrapped->GetDescriptor() == json->GetDescriptor()) {
      // We can directly use google::protobuf::Message::Copy().
      json->CopyFrom(*wrapped);
    } else {
      // Equivalent descriptors but not identical. Must serialize and
      // deserialize.
      absl::Cord serialized;
      if (!wrapped->SerializePartialToCord(&serialized)) {
        return absl::UnknownError(absl::StrCat("failed to serialize message: ",
                                               wrapped->GetTypeName()));
      }
      if (!json->ParsePartialFromCord(serialized)) {
        return absl::UnknownError(
            absl::StrCat("failed to parsed message: ", json->GetTypeName()));
      }
    }
    return absl::OkStatus();
  }
}

bool LegacyListValue::IsEmpty() const { return impl_->empty(); }

size_t LegacyListValue::Size() const {
  return static_cast<size_t>(impl_->size());
}

// See LegacyListValueInterface::Get for documentation.
absl::Status LegacyListValue::Get(
    size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (ABSL_PREDICT_FALSE(index < 0 || index >= impl_->size())) {
    *result = ErrorValue(absl::InvalidArgumentError("index out of bounds"));
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(
      ModernValue(arena, impl_->Get(arena, static_cast<int>(index)), *result));
  return absl::OkStatus();
}

absl::Status LegacyListValue::ForEach(
    ForEachWithIndexCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  const auto size = impl_->size();
  Value element;
  for (int index = 0; index < size; ++index) {
    CEL_RETURN_IF_ERROR(ModernValue(arena, impl_->Get(arena, index), element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, Value(element)));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl_nonnull ValueIteratorPtr> LegacyListValue::NewIterator()
    const {
  return std::make_unique<CelListIterator>(impl_);
}

absl::Status LegacyListValue::Contains(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  CEL_ASSIGN_OR_RETURN(auto legacy_other, LegacyValue(arena, other));
  const auto* cel_list = impl_;
  for (int i = 0; i < cel_list->size(); ++i) {
    auto element = cel_list->Get(arena, i);
    absl::optional<bool> equal =
        interop_internal::CelValueEqualImpl(element, legacy_other);
    // Heterogeneous equality behavior is to just return false if equality
    // undefined.
    if (equal.has_value() && *equal) {
      *result = TrueValue();
      return absl::OkStatus();
    }
  }
  *result = FalseValue();
  return absl::OkStatus();
}

std::string LegacyMapValue::DebugString() const {
  return CelValue::CreateMap(impl_).DebugString();
}

absl::Status LegacyMapValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  const google::protobuf::Descriptor* descriptor =
      descriptor_pool->FindMessageTypeByName("google.protobuf.Struct");
  if (descriptor == nullptr) {
    return absl::InternalError(
        "unable to locate descriptor for message type: google.protobuf.Struct");
  }

  google::protobuf::Arena arena;
  const google::protobuf::Message* wrapped = MaybeWrapValueToMessage(
      descriptor, message_factory, CelValue::CreateMap(impl_), &arena);
  if (wrapped == nullptr) {
    return absl::UnknownError("failed to convert legacy map to JSON");
  }
  if (!wrapped->SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize message: ", wrapped->GetTypeName()));
  }
  return absl::OkStatus();
}

absl::Status LegacyMapValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  google::protobuf::Arena arena;
  const google::protobuf::Message* wrapped =
      MaybeWrapValueToMessage(json->GetDescriptor(), message_factory,
                              CelValue::CreateMap(impl_), &arena);
  if (wrapped == nullptr) {
    return absl::UnknownError("failed to convert legacy map to JSON");
  }

  if (wrapped->GetDescriptor() == json->GetDescriptor()) {
    // We can directly use google::protobuf::Message::Copy().
    json->CopyFrom(*wrapped);
  } else {
    // Equivalent descriptors but not identical. Must serialize and deserialize.
    absl::Cord serialized;
    if (!wrapped->SerializePartialToCord(&serialized)) {
      return absl::UnknownError(absl::StrCat("failed to serialize message: ",
                                             wrapped->GetTypeName()));
    }
    if (!json->ParsePartialFromCord(serialized)) {
      return absl::UnknownError(
          absl::StrCat("failed to parsed message: ", json->GetTypeName()));
    }
  }
  return absl::OkStatus();
}

absl::Status LegacyMapValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  google::protobuf::Arena arena;
  const google::protobuf::Message* wrapped =
      MaybeWrapValueToMessage(json->GetDescriptor(), message_factory,
                              CelValue::CreateMap(impl_), &arena);
  if (wrapped == nullptr) {
    return absl::UnknownError("failed to convert legacy map to JSON");
  }

  if (wrapped->GetDescriptor() == json->GetDescriptor()) {
    // We can directly use google::protobuf::Message::Copy().
    json->CopyFrom(*wrapped);
  } else {
    // Equivalent descriptors but not identical. Must serialize and deserialize.
    absl::Cord serialized;
    if (!wrapped->SerializePartialToCord(&serialized)) {
      return absl::UnknownError(absl::StrCat("failed to serialize message: ",
                                             wrapped->GetTypeName()));
    }
    if (!json->ParsePartialFromCord(serialized)) {
      return absl::UnknownError(
          absl::StrCat("failed to parsed message: ", json->GetTypeName()));
    }
  }
  return absl::OkStatus();
}

bool LegacyMapValue::IsEmpty() const { return impl_->empty(); }

size_t LegacyMapValue::Size() const {
  return static_cast<size_t>(impl_->size());
}

absl::Status LegacyMapValue::Get(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      *result = Value{key};
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
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  auto cel_value = impl_->Get(arena, cel_key);
  if (!cel_value.has_value()) {
    *result = NoSuchKeyError(key.DebugString());
    return absl::OkStatus();
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, *result));
  return absl::OkStatus();
}

absl::StatusOr<bool> LegacyMapValue::Find(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      *result = Value{key};
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
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  auto cel_value = impl_->Get(arena, cel_key);
  if (!cel_value.has_value()) {
    *result = NullValue{};
    return false;
  }
  CEL_RETURN_IF_ERROR(ModernValue(arena, *cel_value, *result));
  return true;
}

absl::Status LegacyMapValue::Has(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      *result = Value{key};
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
  CEL_ASSIGN_OR_RETURN(auto cel_key, LegacyValue(arena, key));
  CEL_ASSIGN_OR_RETURN(auto has, impl_->Has(cel_key));
  *result = BoolValue{has};
  return absl::OkStatus();
}

absl::Status LegacyMapValue::ListKeys(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const {
  CEL_ASSIGN_OR_RETURN(auto keys, impl_->ListKeys(arena));
  *result = ListValue{common_internal::LegacyListValue(keys)};
  return absl::OkStatus();
}

absl::Status LegacyMapValue::ForEach(
    ForEachCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  CEL_ASSIGN_OR_RETURN(auto keys, impl_->ListKeys(arena));
  const auto size = keys->size();
  Value key;
  Value value;
  for (int index = 0; index < size; ++index) {
    auto cel_key = keys->Get(arena, index);
    auto cel_value = *impl_->Get(arena, cel_key);
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_key, key));
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl_nonnull ValueIteratorPtr> LegacyMapValue::NewIterator()
    const {
  return std::make_unique<CelMapIterator>(impl_);
}

absl::string_view LegacyStructValue::GetTypeName() const {
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  return message_wrapper.legacy_type_info()->GetTypename(message_wrapper);
}

std::string LegacyStructValue::DebugString() const {
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  return message_wrapper.legacy_type_info()->DebugString(message_wrapper);
}

absl::Status LegacyStructValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  if (ABSL_PREDICT_TRUE(
          message_wrapper.message_ptr()->SerializePartialToZeroCopyStream(
              output))) {
    return absl::OkStatus();
  }
  return absl::UnknownError("failed to serialize protocol buffer message");
}

absl::Status LegacyStructValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);

  return internal::MessageToJson(
      *google::protobuf::DownCastMessage<google::protobuf::Message>(message_wrapper.message_ptr()),
      descriptor_pool, message_factory, json);
}

absl::Status LegacyStructValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);

  return internal::MessageToJson(
      *google::protobuf::DownCastMessage<google::protobuf::Message>(message_wrapper.message_ptr()),
      descriptor_pool, message_factory, json);
}

absl::Status LegacyStructValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (auto legacy_struct_value = common_internal::AsLegacyStructValue(other);
      legacy_struct_value.has_value()) {
    auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
    const auto* access_apis =
        message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
    if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
      return absl::UnimplementedError(
          absl::StrCat("legacy access APIs missing for ", GetTypeName()));
    }
    auto other_message_wrapper =
        AsMessageWrapper(legacy_struct_value->message_ptr(),
                         legacy_struct_value->legacy_type_info());
    *result = BoolValue{
        access_apis->IsEqualTo(message_wrapper, other_message_wrapper)};
    return absl::OkStatus();
  }
  if (auto struct_value = other.AsStruct(); struct_value.has_value()) {
    return common_internal::StructValueEqual(
        common_internal::LegacyStructValue(message_ptr_, legacy_type_info_),
        *struct_value, descriptor_pool, message_factory, arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

bool LegacyStructValue::IsZeroValue() const {
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return false;
  }
  return access_apis->ListFields(message_wrapper).empty();
}

absl::Status LegacyStructValue::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    *result = NoSuchFieldError(name);
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(
      auto cel_value,
      access_apis->GetField(name, message_wrapper, unboxing_options,
                            MemoryManagerRef::Pooling(arena)));
  CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, *result));
  return absl::OkStatus();
}

absl::Status LegacyStructValue::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return absl::UnimplementedError(
      "access to fields by numbers is not available for legacy structs");
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByName(
    absl::string_view name) const {
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return NoSuchFieldError(name).NativeValue();
  }
  return access_apis->HasField(name, message_wrapper);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByNumber(int64_t number) const {
  return absl::UnimplementedError(
      "access to fields by numbers is not available for legacy structs");
}

absl::Status LegacyStructValue::ForEachField(
    ForEachFieldCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
  const auto* access_apis =
      message_wrapper.legacy_type_info()->GetAccessApis(message_wrapper);
  if (ABSL_PREDICT_FALSE(access_apis == nullptr)) {
    return absl::UnimplementedError(
        absl::StrCat("legacy access APIs missing for ", GetTypeName()));
  }
  auto field_names = access_apis->ListFields(message_wrapper);
  Value value;
  for (const auto& field_name : field_names) {
    CEL_ASSIGN_OR_RETURN(
        auto cel_value,
        access_apis->GetField(field_name, message_wrapper,
                              ProtoWrapperTypeOptions::kUnsetNull,
                              MemoryManagerRef::Pooling(arena)));
    CEL_RETURN_IF_ERROR(ModernValue(arena, cel_value, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(field_name, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::Status LegacyStructValue::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
    int* absl_nonnull count) const {
  if (ABSL_PREDICT_FALSE(qualifiers.empty())) {
    return absl::InvalidArgumentError("invalid select qualifier path.");
  }
  auto message_wrapper = AsMessageWrapper(message_ptr_, legacy_type_info_);
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
    *result = NoSuchFieldError(field_name);
    *count = -1;
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(
      auto legacy_result,
      access_apis->Qualify(qualifiers, message_wrapper, presence_test,
                           MemoryManager::Pooling(arena)));
  CEL_RETURN_IF_ERROR(ModernValue(arena, legacy_result.value, *result));
  *count = legacy_result.qualifier_count;
  return absl::OkStatus();
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
      result = StringValue(Borrower::Arena(arena),
                           legacy_value.StringOrDie().value());
      return absl::OkStatus();
    case CelValue::Type::kBytes:
      result =
          BytesValue(Borrower::Arena(arena), legacy_value.BytesOrDie().value());
      return absl::OkStatus();
    case CelValue::Type::kMessage: {
      auto message_wrapper = legacy_value.MessageWrapperOrDie();
      result = common_internal::LegacyStructValue(
          google::protobuf::DownCastMessage<google::protobuf::Message>(
              message_wrapper.message_ptr()),
          message_wrapper.legacy_type_info());
      return absl::OkStatus();
    }
    case CelValue::Type::kDuration:
      result = UnsafeDurationValue(legacy_value.DurationOrDie());
      return absl::OkStatus();
    case CelValue::Type::kTimestamp:
      result = UnsafeTimestampValue(legacy_value.TimestampOrDie());
      return absl::OkStatus();
    case CelValue::Type::kList:
      result =
          ListValue(common_internal::LegacyListValue(legacy_value.ListOrDie()));
      return absl::OkStatus();
    case CelValue::Type::kMap:
      result =
          MapValue(common_internal::LegacyMapValue(legacy_value.MapOrDie()));
      return absl::OkStatus();
    case CelValue::Type::kUnknownSet:
      result = UnknownValue{*legacy_value.UnknownSetOrDie()};
      return absl::OkStatus();
    case CelValue::Type::kCelType: {
      auto type_name = legacy_value.CelTypeOrDie().value();
      if (type_name.empty()) {
        return absl::InvalidArgumentError("empty type name in CelValue");
      }
      result = TypeValue(common_internal::LegacyRuntimeType(type_name));
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
    case ValueKind::kString:
      return CelValue::CreateStringView(common_internal::LegacyStringValue(
          modern_value.GetString(), /*stable=*/false, arena));
    case ValueKind::kBytes:
      return CelValue::CreateBytesView(common_internal::LegacyBytesValue(
          modern_value.GetBytes(), /*stable=*/false, arena));
    case ValueKind::kStruct:
      return common_internal::LegacyTrivialStructValue(arena, modern_value);
    case ValueKind::kDuration:
      return CelValue::CreateUncheckedDuration(
          modern_value.GetDuration().NativeValue());
    case ValueKind::kTimestamp:
      return CelValue::CreateTimestamp(
          modern_value.GetTimestamp().NativeValue());
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
      return StringValue(Borrower::Arena(arena),
                         legacy_value.StringOrDie().value());
    case CelValue::Type::kBytes:
      return BytesValue(Borrower::Arena(arena),
                        legacy_value.BytesOrDie().value());
    case CelValue::Type::kMessage: {
      auto message_wrapper = legacy_value.MessageWrapperOrDie();
      return common_internal::LegacyStructValue(
          google::protobuf::DownCastMessage<google::protobuf::Message>(
              message_wrapper.message_ptr()),
          message_wrapper.legacy_type_info());
    }
    case CelValue::Type::kDuration:
      return UnsafeDurationValue(legacy_value.DurationOrDie());
    case CelValue::Type::kTimestamp:
      return UnsafeTimestampValue(legacy_value.TimestampOrDie());
    case CelValue::Type::kList:
      return ListValue(
          common_internal::LegacyListValue(legacy_value.ListOrDie()));
    case CelValue::Type::kMap:
      return MapValue(common_internal::LegacyMapValue(legacy_value.MapOrDie()));
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
    case ValueKind::kString:
      return CelValue::CreateStringView(common_internal::LegacyStringValue(
          value.GetString(), /*stable=*/false, arena));
    case ValueKind::kBytes:
      return CelValue::CreateBytesView(common_internal::LegacyBytesValue(
          value.GetBytes(), /*stable=*/false, arena));
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
  return TypeValue(common_internal::LegacyRuntimeType(input));
}

}  // namespace interop_internal

}  // namespace cel
