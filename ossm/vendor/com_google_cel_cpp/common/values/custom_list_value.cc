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

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/values/list_value_builder.h"
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

using ::cel::well_known_types::ListValueReflection;
using ::cel::well_known_types::ValueReflection;
using ::google::api::expr::runtime::CelValue;

class EmptyListValue final : public common_internal::CompatListValue {
 public:
  static const EmptyListValue& Get() {
    static const absl::NoDestructor<EmptyListValue> empty;
    return *empty;
  }

  EmptyListValue() = default;

  std::string DebugString() const override { return "[]"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::Status ConvertToJsonArray(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(json != nullptr);
    ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                   google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

    json->Clear();
    return absl::OkStatus();
  }

  CustomListValue Clone(google::protobuf::Arena* absl_nonnull arena) const override {
    return CustomListValue(&EmptyListValue::Get(), arena);
  }

  int size() const override { return 0; }

  CelValue operator[](int index) const override {
    static const absl::NoDestructor<absl::Status> error(
        absl::InvalidArgumentError("index out of bounds"));
    return CelValue::CreateError(&*error);
  }

  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      return (*this)[index];
    }
    return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
        arena, absl::InvalidArgumentError("index out of bounds")));
  }

 private:
  absl::Status Get(size_t index, const google::protobuf::DescriptorPool* absl_nonnull,
                   google::protobuf::MessageFactory* absl_nonnull,
                   google::protobuf::Arena* absl_nonnull,
                   Value* absl_nonnull result) const override {
    *result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
};

}  // namespace

namespace common_internal {

const CompatListValue* absl_nonnull EmptyCompatListValue() {
  return &EmptyListValue::Get();
}

}  // namespace common_internal

class CustomListValueInterfaceIterator final : public ValueIterator {
 public:
  explicit CustomListValueInterfaceIterator(
      const CustomListValueInterface& interface)
      : interface_(interface), size_(interface_.Size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    CEL_RETURN_IF_ERROR(interface_.Get(index_, descriptor_pool, message_factory,
                                       arena, result));
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
    CEL_RETURN_IF_ERROR(interface_.Get(index_, descriptor_pool, message_factory,
                                       arena, key_or_value));
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
      CEL_RETURN_IF_ERROR(interface_.Get(index_, descriptor_pool,
                                         message_factory, arena, value));
    }
    *key = IntValue(index_);
    ++index_;
    return true;
  }

 private:
  const CustomListValueInterface& interface_;
  const size_t size_;
  size_t index_ = 0;
};

namespace {

class CustomListValueDispatcherIterator final : public ValueIterator {
 public:
  explicit CustomListValueDispatcherIterator(
      const CustomListValueDispatcher* absl_nonnull dispatcher,
      CustomListValueContent content, size_t size)
      : dispatcher_(dispatcher), content_(content), size_(size) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    CEL_RETURN_IF_ERROR(dispatcher_->get(dispatcher_, content_, index_,
                                         descriptor_pool, message_factory,
                                         arena, result));
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
    CEL_RETURN_IF_ERROR(dispatcher_->get(dispatcher_, content_, index_,
                                         descriptor_pool, message_factory,
                                         arena, key_or_value));
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
      CEL_RETURN_IF_ERROR(dispatcher_->get(dispatcher_, content_, index_,
                                           descriptor_pool, message_factory,
                                           arena, value));
    }
    *key = IntValue(index_);
    ++index_;
    return true;
  }

 private:
  const CustomListValueDispatcher* absl_nonnull const dispatcher_;
  const CustomListValueContent content_;
  const size_t size_;
  size_t index_ = 0;
};

}  // namespace

absl::Status CustomListValueInterface::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  ListValueReflection reflection;
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
      ConvertToJsonArray(descriptor_pool, message_factory, message));
  if (!message->SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        "failed to serialize message: google.protobuf.ListValue");
  }
  return absl::OkStatus();
}

absl::Status CustomListValueInterface::ForEach(
    ForEachWithIndexCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  const size_t size = Size();
  for (size_t index = 0; index < size; ++index) {
    Value element;
    CEL_RETURN_IF_ERROR(
        Get(index, descriptor_pool, message_factory, arena, &element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, element));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl_nonnull ValueIteratorPtr>
CustomListValueInterface::NewIterator() const {
  return std::make_unique<CustomListValueInterfaceIterator>(*this);
}

absl::Status CustomListValueInterface::Equal(
    const ListValue& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return ListValueEqual(*this, other, descriptor_pool, message_factory, arena,
                        result);
}

absl::Status CustomListValueInterface::Contains(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  Value outcome = BoolValue(false);
  Value equal;
  CEL_RETURN_IF_ERROR(ForEach(
      [&](size_t index, const Value& element) -> absl::StatusOr<bool> {
        CEL_RETURN_IF_ERROR(element.Equal(other, descriptor_pool,
                                          message_factory, arena, &equal));
        if (auto bool_result = As<BoolValue>(equal);
            bool_result.has_value() && bool_result->NativeValue()) {
          outcome = BoolValue(true);
          return false;
        }
        return true;
      },
      descriptor_pool, message_factory, arena));
  *result = outcome;
  return absl::OkStatus();
}

CustomListValue::CustomListValue() {
  content_ = CustomListValueContent::From(CustomListValueInterface::Content{
      .interface = &EmptyListValue::Get(), .arena = nullptr});
}

NativeTypeId CustomListValue::GetTypeId() const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->GetNativeTypeId();
  }
  return dispatcher_->get_type_id(dispatcher_, content_);
}

absl::string_view CustomListValue::GetTypeName() const { return "list"; }

std::string CustomListValue::DebugString() const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->DebugString();
  }
  if (dispatcher_->debug_string != nullptr) {
    return dispatcher_->debug_string(dispatcher_, content_);
  }
  return "list";
}

absl::Status CustomListValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
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

absl::Status CustomListValue::ConvertToJson(
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
  google::protobuf::Message* json_array = value_reflection.MutableListValue(json);

  return ConvertToJsonArray(descriptor_pool, message_factory, json_array);
}

absl::Status CustomListValue::ConvertToJsonArray(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->ConvertToJsonArray(descriptor_pool,
                                                 message_factory, json);
  }
  if (dispatcher_->convert_to_json_array != nullptr) {
    return dispatcher_->convert_to_json_array(
        dispatcher_, content_, descriptor_pool, message_factory, json);
  }
  return absl::UnimplementedError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status CustomListValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_list_value = other.AsList(); other_list_value) {
    if (dispatcher_ == nullptr) {
      CustomListValueInterface::Content content =
          content_.To<CustomListValueInterface::Content>();
      ABSL_DCHECK(content.interface != nullptr);
      return content.interface->Equal(*other_list_value, descriptor_pool,
                                      message_factory, arena, result);
    }
    if (dispatcher_->equal != nullptr) {
      return dispatcher_->equal(dispatcher_, content_, *other_list_value,
                                descriptor_pool, message_factory, arena,
                                result);
    }
    return common_internal::ListValueEqual(*this, *other_list_value,
                                           descriptor_pool, message_factory,
                                           arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

bool CustomListValue::IsZeroValue() const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->IsZeroValue();
  }
  return dispatcher_->is_zero_value(dispatcher_, content_);
}

CustomListValue CustomListValue::Clone(
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    if (content.arena != arena) {
      return content.interface->Clone(arena);
    }
    return *this;
  }
  return dispatcher_->clone(dispatcher_, content_, arena);
}

bool CustomListValue::IsEmpty() const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->IsEmpty();
  }
  if (dispatcher_->is_empty != nullptr) {
    return dispatcher_->is_empty(dispatcher_, content_);
  }
  return dispatcher_->size(dispatcher_, content_) == 0;
}

size_t CustomListValue::Size() const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->Size();
  }
  return dispatcher_->size(dispatcher_, content_);
}

absl::Status CustomListValue::Get(
    size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->Get(index, descriptor_pool, message_factory,
                                  arena, result);
  }
  return dispatcher_->get(dispatcher_, content_, index, descriptor_pool,
                          message_factory, arena, result);
}

absl::Status CustomListValue::ForEach(
    ForEachWithIndexCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->ForEach(callback, descriptor_pool,
                                      message_factory, arena);
  }
  if (dispatcher_->for_each != nullptr) {
    return dispatcher_->for_each(dispatcher_, content_, callback,
                                 descriptor_pool, message_factory, arena);
  }
  const size_t size = dispatcher_->size(dispatcher_, content_);
  for (size_t index = 0; index < size; ++index) {
    Value element;
    CEL_RETURN_IF_ERROR(dispatcher_->get(dispatcher_, content_, index,
                                         descriptor_pool, message_factory,
                                         arena, &element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, element));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl_nonnull ValueIteratorPtr> CustomListValue::NewIterator()
    const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->NewIterator();
  }
  if (dispatcher_->new_iterator != nullptr) {
    return dispatcher_->new_iterator(dispatcher_, content_);
  }
  return std::make_unique<CustomListValueDispatcherIterator>(
      dispatcher_, content_, dispatcher_->size(dispatcher_, content_));
}

absl::Status CustomListValue::Contains(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (dispatcher_ == nullptr) {
    CustomListValueInterface::Content content =
        content_.To<CustomListValueInterface::Content>();
    ABSL_DCHECK(content.interface != nullptr);
    return content.interface->Contains(other, descriptor_pool, message_factory,
                                       arena, result);
  }
  if (dispatcher_->contains != nullptr) {
    return dispatcher_->contains(dispatcher_, content_, other, descriptor_pool,
                                 message_factory, arena, result);
  }
  Value outcome = BoolValue(false);
  Value equal;
  CEL_RETURN_IF_ERROR(ForEach(
      [&](size_t index, const Value& element) -> absl::StatusOr<bool> {
        CEL_RETURN_IF_ERROR(element.Equal(other, descriptor_pool,
                                          message_factory, arena, &equal));
        if (auto bool_result = As<BoolValue>(equal);
            bool_result.has_value() && bool_result->NativeValue()) {
          outcome = BoolValue(true);
          return false;
        }
        return true;
      },
      descriptor_pool, message_factory, arena));
  *result = outcome;
  return absl::OkStatus();
}

}  // namespace cel
