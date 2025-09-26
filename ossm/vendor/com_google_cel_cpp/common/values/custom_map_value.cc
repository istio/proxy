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

#include <cstddef>
#include <memory>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

using ::cel::well_known_types::StructReflection;
using ::cel::well_known_types::ValueReflection;
using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelValue;

absl::Status NoSuchKeyError(const Value& key) {
  return absl::NotFoundError(
      absl::StrCat("Key not found in map : ", key.DebugString()));
}

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

class EmptyMapValue final : public common_internal::CompatMapValue {
 public:
  static const EmptyMapValue& Get() {
    static const absl::NoDestructor<EmptyMapValue> empty;
    return *empty;
  }

  EmptyMapValue() = default;

  std::string DebugString() const override { return "{}"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::Status ListKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      ListValue* absl_nonnull result) const override {
    *result = ListValue();
    return absl::OkStatus();
  }

  absl::StatusOr<absl_nonnull ValueIteratorPtr> NewIterator() const override {
    return NewEmptyValueIterator();
  }

  absl::Status ConvertToJsonObject(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);
    ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                   google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

    json->Clear();
    return absl::OkStatus();
  }

  CustomMapValue Clone(google::protobuf::Arena* absl_nonnull) const override {
    return CustomMapValue();
  }

  absl::optional<CelValue> operator[](CelValue key) const override {
    return absl::nullopt;
  }

  using CompatMapValue::Get;
  absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                               CelValue key) const override {
    return absl::nullopt;
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override { return false; }

  int size() const override { return static_cast<int>(Size()); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return common_internal::EmptyCompatListValue();
  }

  absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena*) const override {
    return ListKeys();
  }

 private:
  absl::StatusOr<bool> Find(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena,
      Value* absl_nonnull result) const override {
    return false;
  }

  absl::StatusOr<bool> Has(
      const Value& key,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const override {
    return false;
  }
};

}  // namespace

namespace common_internal {

const CompatMapValue* absl_nonnull EmptyCompatMapValue() {
  return &EmptyMapValue::Get();
}

}  // namespace common_internal

class CustomMapValueInterfaceIterator final : public ValueIterator {
 public:
  explicit CustomMapValueInterfaceIterator(
      const CustomMapValueInterface* absl_nonnull interface)
      : interface_(interface) {}

  bool HasNext() override {
    if (keys_iterator_ == nullptr) {
      return !interface_->IsEmpty();
    }
    return keys_iterator_->HasNext();
  }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (keys_iterator_ == nullptr) {
      if (interface_->IsEmpty()) {
        return absl::FailedPreconditionError(
            "ValueIterator::Next() called when "
            "ValueIterator::HasNext() returns false");
      }
      CEL_RETURN_IF_ERROR(ProjectKeys(descriptor_pool, message_factory, arena));
    }
    return keys_iterator_->Next(descriptor_pool, message_factory, arena,
                                result);
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

    if (keys_iterator_ == nullptr) {
      if (interface_->IsEmpty()) {
        return false;
      }
      CEL_RETURN_IF_ERROR(ProjectKeys(descriptor_pool, message_factory, arena));
    }

    return keys_iterator_->Next1(descriptor_pool, message_factory, arena,
                                 key_or_value);
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

    if (keys_iterator_ == nullptr) {
      if (interface_->IsEmpty()) {
        return false;
      }
      CEL_RETURN_IF_ERROR(ProjectKeys(descriptor_pool, message_factory, arena));
    }

    CEL_ASSIGN_OR_RETURN(
        bool ok,
        keys_iterator_->Next1(descriptor_pool, message_factory, arena, key));
    if (!ok) {
      return false;
    }
    if (value != nullptr) {
      CEL_ASSIGN_OR_RETURN(ok, interface_->Find(*key, descriptor_pool,
                                                message_factory, arena, value));
      if (!ok) {
        return absl::DataLossError(
            "map iterator returned key that was not present in the map");
      }
    }
    return true;
  }

 private:
  // Projects the keys from the map, setting `keys_` and `keys_iterator_`. If
  // this returns OK it is guaranteed that `keys_iterator_` is not null.
  absl::Status ProjectKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) {
    ABSL_DCHECK(keys_iterator_ == nullptr);

    CEL_RETURN_IF_ERROR(
        interface_->ListKeys(descriptor_pool, message_factory, arena, &keys_));
    CEL_ASSIGN_OR_RETURN(keys_iterator_, keys_.NewIterator());
    ABSL_CHECK(keys_iterator_->HasNext());  // Crash OK
    return absl::OkStatus();
  }

  const CustomMapValueInterface* absl_nonnull const interface_;
  ListValue keys_;
  absl_nullable ValueIteratorPtr keys_iterator_;
};

namespace {

class CustomMapValueDispatcherIterator final : public ValueIterator {
 public:
  explicit CustomMapValueDispatcherIterator(
      const CustomMapValueDispatcher* absl_nonnull dispatcher,
      CustomMapValueContent content)
      : dispatcher_(dispatcher), content_(content) {}

  bool HasNext() override {
    if (keys_iterator_ == nullptr) {
      if (dispatcher_->is_empty != nullptr) {
        return !dispatcher_->is_empty(dispatcher_, content_);
      }
      return dispatcher_->size(dispatcher_, content_) != 0;
    }
    return keys_iterator_->HasNext();
  }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (keys_iterator_ == nullptr) {
      if (dispatcher_->is_empty != nullptr
              ? dispatcher_->is_empty(dispatcher_, content_)
              : dispatcher_->size(dispatcher_, content_) == 0) {
        return absl::FailedPreconditionError(
            "ValueIterator::Next() called when "
            "ValueIterator::HasNext() returns false");
      }
      CEL_RETURN_IF_ERROR(ProjectKeys(descriptor_pool, message_factory, arena));
    }
    return keys_iterator_->Next(descriptor_pool, message_factory, arena,
                                result);
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

    if (keys_iterator_ == nullptr) {
      if (dispatcher_->is_empty != nullptr
              ? dispatcher_->is_empty(dispatcher_, content_)
              : dispatcher_->size(dispatcher_, content_) == 0) {
        return false;
      }
      CEL_RETURN_IF_ERROR(ProjectKeys(descriptor_pool, message_factory, arena));
    }

    return keys_iterator_->Next1(descriptor_pool, message_factory, arena,
                                 key_or_value);
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
    ABSL_DCHECK(value != nullptr);

    if (keys_iterator_ == nullptr) {
      if (dispatcher_->is_empty != nullptr
              ? dispatcher_->is_empty(dispatcher_, content_)
              : dispatcher_->size(dispatcher_, content_) == 0) {
        return false;
      }
      CEL_RETURN_IF_ERROR(ProjectKeys(descriptor_pool, message_factory, arena));
    }

    CEL_ASSIGN_OR_RETURN(
        bool ok,
        keys_iterator_->Next1(descriptor_pool, message_factory, arena, key));
    if (!ok) {
      return false;
    }
    if (value != nullptr) {
      CEL_ASSIGN_OR_RETURN(
          ok, dispatcher_->find(dispatcher_, content_, *key, descriptor_pool,
                                message_factory, arena, value));
      if (!ok) {
        return absl::DataLossError(
            "map iterator returned key that was not present in the map");
      }
    }
    return true;
  }

 private:
  absl::Status ProjectKeys(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) {
    ABSL_DCHECK(keys_iterator_ == nullptr);

    CEL_RETURN_IF_ERROR(dispatcher_->list_keys(dispatcher_, content_,
                                               descriptor_pool, message_factory,
                                               arena, &keys_));
    CEL_ASSIGN_OR_RETURN(keys_iterator_, keys_.NewIterator());
    ABSL_CHECK(keys_iterator_->HasNext());  // Crash OK
    return absl::OkStatus();
  }

  const CustomMapValueDispatcher* absl_nonnull const dispatcher_;
  const CustomMapValueContent content_;
  ListValue keys_;
  absl_nullable ValueIteratorPtr keys_iterator_;
};

}  // namespace

absl::Status CustomMapValueInterface::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  StructReflection reflection;
  CEL_RETURN_IF_ERROR(reflection.Initialize(descriptor_pool));
  const google::protobuf::Message* prototype =
      message_factory->GetPrototype(reflection.GetDescriptor());
  if (prototype == nullptr) {
    return absl::UnknownError(
        absl::StrCat("failed to get message prototype: ",
                     reflection.GetDescriptor()->full_name()));
  }
  google::protobuf::Arena arena;
  google::protobuf::Message* message = prototype->New(&arena);
  CEL_RETURN_IF_ERROR(
      ConvertToJsonObject(descriptor_pool, message_factory, message));
  if (!message->SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        "failed to serialize message: google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::Status CustomMapValueInterface::ForEach(
    ForEachCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  CEL_ASSIGN_OR_RETURN(auto iterator, NewIterator());
  while (iterator->HasNext()) {
    Value key;
    Value value;
    CEL_RETURN_IF_ERROR(
        iterator->Next(descriptor_pool, message_factory, arena, &key));
    CEL_ASSIGN_OR_RETURN(
        bool found, Find(key, descriptor_pool, message_factory, arena, &value));
    if (!found) {
      value = ErrorValue(NoSuchKeyError(key));
    }
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl_nonnull ValueIteratorPtr>
CustomMapValueInterface::NewIterator() const {
  return std::make_unique<CustomMapValueInterfaceIterator>(this);
}

absl::Status CustomMapValueInterface::Equal(
    const MapValue& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return MapValueEqual(*this, other, descriptor_pool, message_factory, arena,
                       result);
}

CustomMapValue::CustomMapValue() {
  content_ = CustomMapValueContent::From(CustomMapValueInterface::Content{
      .interface = &EmptyMapValue::Get(), .arena = nullptr});
}

NativeTypeId CustomMapValue::GetTypeId() const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetNativeTypeId();
  }
  return dispatcher_->get_type_id(dispatcher_, content_);
}

absl::string_view CustomMapValue::GetTypeName() const { return "map"; }

std::string CustomMapValue::DebugString() const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->DebugString();
  }
  if (dispatcher_->debug_string != nullptr) {
    return dispatcher_->debug_string(dispatcher_, content_);
  }
  return "map";
}

absl::Status CustomMapValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->SerializeTo(descriptor_pool, message_factory,
                                          output);
  }
  if (dispatcher_->serialize_to != nullptr) {
    return dispatcher_->serialize_to(dispatcher_, content_, descriptor_pool,
                                     message_factory, output);
  }
  return absl::UnimplementedError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status CustomMapValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  ValueReflection value_reflection;
  CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
  google::protobuf::Message* json_object = value_reflection.MutableStructValue(json);

  return ConvertToJsonObject(descriptor_pool, message_factory, json_object);
}

absl::Status CustomMapValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->ConvertToJsonObject(descriptor_pool,
                                                  message_factory, json);
  }
  if (dispatcher_->convert_to_json_object != nullptr) {
    return dispatcher_->convert_to_json_object(
        dispatcher_, content_, descriptor_pool, message_factory, json);
  }
  return absl::UnimplementedError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status CustomMapValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_map_value = other.AsMap(); other_map_value) {
    if (dispatcher_ == nullptr) {
      CustomMapValueInterface::Content content =
          content_.To<CustomMapValueInterface::Content>();
      ABSL_DCHECK(content.interface != nullptr);
      return content.interface->Equal(*other_map_value, descriptor_pool,
                                      message_factory, arena, result);
    }
    if (dispatcher_->equal != nullptr) {
      return dispatcher_->equal(dispatcher_, content_, *other_map_value,
                                descriptor_pool, message_factory, arena,
                                result);
    }
    return common_internal::MapValueEqual(*this, *other_map_value,
                                          descriptor_pool, message_factory,
                                          arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

bool CustomMapValue::IsZeroValue() const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->IsZeroValue();
  }
  return dispatcher_->is_zero_value(dispatcher_, content_);
}

CustomMapValue CustomMapValue::Clone(google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    if (content.arena != arena) {
      return content.interface->Clone(arena);
    }
    return *this;
  }
  return dispatcher_->clone(dispatcher_, content_, arena);
}

bool CustomMapValue::IsEmpty() const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->IsEmpty();
  }
  if (dispatcher_->is_empty != nullptr) {
    return dispatcher_->is_empty(dispatcher_, content_);
  }
  return dispatcher_->size(dispatcher_, content_) == 0;
}

size_t CustomMapValue::Size() const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->Size();
  }
  return dispatcher_->size(dispatcher_, content_);
}

absl::Status CustomMapValue::Get(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  CEL_ASSIGN_OR_RETURN(
      bool ok, Find(key, descriptor_pool, message_factory, arena, result));
  if (ABSL_PREDICT_FALSE(!ok)) {
    switch (result->kind()) {
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        break;
      default:
        *result = ErrorValue(NoSuchKeyError(key));
        break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> CustomMapValue::Find(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      *result = key;
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
      *result = ErrorValue(InvalidMapKeyTypeError(key.kind()));
      return false;
  }

  bool ok;
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    CEL_ASSIGN_OR_RETURN(
        ok, content.interface->Find(key, descriptor_pool, message_factory,
                                    arena, result));
  } else {
    CEL_ASSIGN_OR_RETURN(
        ok, dispatcher_->find(dispatcher_, content_, key, descriptor_pool,
                              message_factory, arena, result));
  }
  if (ok) {
    return true;
  }
  *result = NullValue{};
  return false;
}

absl::Status CustomMapValue::Has(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      *result = key;
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
      *result = ErrorValue(InvalidMapKeyTypeError(key.kind()));
      return absl::OkStatus();
  }
  bool has;
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    CEL_ASSIGN_OR_RETURN(has, content.interface->Has(key, descriptor_pool,
                                                     message_factory, arena));
  } else {
    CEL_ASSIGN_OR_RETURN(
        has, dispatcher_->has(dispatcher_, content_, key, descriptor_pool,
                              message_factory, arena));
  }
  *result = BoolValue(has);
  return absl::OkStatus();
}

absl::Status CustomMapValue::ListKeys(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->ListKeys(descriptor_pool, message_factory, arena,
                                       result);
  }
  return dispatcher_->list_keys(dispatcher_, content_, descriptor_pool,
                                message_factory, arena, result);
}

absl::Status CustomMapValue::ForEach(
    ForEachCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->ForEach(callback, descriptor_pool,
                                      message_factory, arena);
  }
  if (dispatcher_->for_each != nullptr) {
    return dispatcher_->for_each(dispatcher_, content_, callback,
                                 descriptor_pool, message_factory, arena);
  }
  absl_nonnull ValueIteratorPtr iterator;
  if (dispatcher_->new_iterator != nullptr) {
    CEL_ASSIGN_OR_RETURN(iterator,
                         dispatcher_->new_iterator(dispatcher_, content_));
  } else {
    iterator = std::make_unique<CustomMapValueDispatcherIterator>(dispatcher_,
                                                                  content_);
  }
  while (iterator->HasNext()) {
    Value key;
    Value value;
    CEL_RETURN_IF_ERROR(
        iterator->Next(descriptor_pool, message_factory, arena, &key));
    CEL_ASSIGN_OR_RETURN(
        bool found,
        dispatcher_->find(dispatcher_, content_, key, descriptor_pool,
                          message_factory, arena, &value));
    if (!found) {
      value = ErrorValue(NoSuchKeyError(key));
    }
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl_nonnull ValueIteratorPtr> CustomMapValue::NewIterator()
    const {
  if (dispatcher_ == nullptr) {
    CustomMapValueInterface::Content content =
        content_.To<CustomMapValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->NewIterator();
  }
  if (dispatcher_->new_iterator != nullptr) {
    return dispatcher_->new_iterator(dispatcher_, content_);
  }
  return std::make_unique<CustomMapValueDispatcherIterator>(dispatcher_,
                                                            content_);
}

}  // namespace cel
