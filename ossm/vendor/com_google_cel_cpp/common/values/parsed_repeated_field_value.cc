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

#include "common/values/parsed_repeated_field_value.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

std::string ParsedRepeatedFieldValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return "INVALID";
  }
  return "VALID";
}

absl::Status ParsedRepeatedFieldValue::SerializeTo(
    AnyToJsonConverter& converter, absl::Cord& value) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    value.Clear();
    return absl::OkStatus();
  }
  // We have to convert to google.protobuf.Struct first.
  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
  absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
  std::tie(descriptor_pool, message_factory) =
      GetDescriptorPoolAndMessageFactory(converter, *message_);
  google::protobuf::Arena arena;
  auto* json = google::protobuf::Arena::Create<google::protobuf::Value>(&arena);
  CEL_RETURN_IF_ERROR(internal::MessageFieldToJson(
      *message_, field_, descriptor_pool, message_factory, json));
  if (!json->list_value().SerializePartialToCord(&value)) {
    return absl::UnknownError("failed to serialize google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> ParsedRepeatedFieldValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return JsonObject();
  }
  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
  absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
  std::tie(descriptor_pool, message_factory) =
      GetDescriptorPoolAndMessageFactory(converter, *message_);
  google::protobuf::Arena arena;
  auto* json = google::protobuf::Arena::Create<google::protobuf::Value>(&arena);
  CEL_RETURN_IF_ERROR(internal::MessageFieldToJson(
      *message_, field_, descriptor_pool, message_factory, json));
  return internal::ProtoJsonListToNativeJsonList(json->list_value());
}

absl::StatusOr<JsonArray> ParsedRepeatedFieldValue::ConvertToJsonArray(
    AnyToJsonConverter& converter) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJson(converter));
  return absl::get<JsonArray>(std::move(json));
}

absl::Status ParsedRepeatedFieldValue::Equal(ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) const {
  if (auto other_value = other.AsParsedRepeatedField(); other_value) {
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
    std::tie(descriptor_pool, message_factory) =
        GetDescriptorPoolAndMessageFactory(value_manager, *message_);
    ABSL_DCHECK(field_ != nullptr);
    ABSL_DCHECK(other_value->field_ != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageFieldEquals(
                        *message_, field_, *other_value->message_,
                        other_value->field_, descriptor_pool, message_factory));
    result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedJsonList(); other_value) {
    if (other_value->value_ == nullptr) {
      result = BoolValue(IsEmpty());
      return absl::OkStatus();
    }
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
    std::tie(descriptor_pool, message_factory) =
        GetDescriptorPoolAndMessageFactory(value_manager, *message_);
    ABSL_DCHECK(field_ != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal,
        internal::MessageFieldEquals(*message_, field_, *other_value->value_,
                                     descriptor_pool, message_factory));
    result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsList(); other_value) {
    return common_internal::ListValueEqual(value_manager, ListValue(*this),
                                           *other_value, result);
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedRepeatedFieldValue::Equal(
    ValueManager& value_manager, const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool ParsedRepeatedFieldValue::IsZeroValue() const { return IsEmpty(); }

ParsedRepeatedFieldValue ParsedRepeatedFieldValue::Clone(
    Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return ParsedRepeatedFieldValue();
  }
  if (message_.arena() == allocator.arena()) {
    return *this;
  }
  auto field = message_->GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(
      *message_, field_);
  auto cloned = WrapShared(message_->New(allocator.arena()), allocator);
  auto cloned_field =
      cloned->GetReflection()->GetMutableRepeatedFieldRef<google::protobuf::Message>(
          cel::to_address(cloned), field_);
  cloned_field.Reserve(field.size());
  cloned_field.CopyFrom(field);
  return ParsedRepeatedFieldValue(std::move(cloned), field_);
}

bool ParsedRepeatedFieldValue::IsEmpty() const { return Size() == 0; }

size_t ParsedRepeatedFieldValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return 0;
  }
  return static_cast<size_t>(GetReflection()->FieldSize(*message_, field_));
}

// See ListValueInterface::Get for documentation.
absl::Status ParsedRepeatedFieldValue::Get(ValueManager& value_manager,
                                           size_t index, Value& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr ||
                         index >= std::numeric_limits<int>::max() ||
                         static_cast<int>(index) >=
                             GetReflection()->FieldSize(*message_, field_))) {
    result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
  absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
  std::tie(descriptor_pool, message_factory) =
      GetDescriptorPoolAndMessageFactory(value_manager, *message_);
  result = Value::RepeatedField(message_, field_, static_cast<int>(index),
                                descriptor_pool, message_factory);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedRepeatedFieldValue::Get(ValueManager& value_manager,
                                                    size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ParsedRepeatedFieldValue::ForEach(ValueManager& value_manager,
                                               ForEachCallback callback) const {
  return ForEach(
      value_manager,
      [callback](size_t, const Value& element) -> absl::StatusOr<bool> {
        return callback(element);
      });
}

absl::Status ParsedRepeatedFieldValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  const int size = reflection->FieldSize(*message_, field_);
  if (size > 0) {
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
    std::tie(descriptor_pool, message_factory) =
        GetDescriptorPoolAndMessageFactory(value_manager, *message_);
    Allocator<> allocator = value_manager.GetMemoryManager().arena();
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         common_internal::RepeatedFieldAccessorFor(field_));
    Value scratch;
    for (int i = 0; i < size; ++i) {
      (*accessor)(allocator, message_, field_, reflection, i, descriptor_pool,
                  message_factory, scratch);
      CEL_ASSIGN_OR_RETURN(auto ok, callback(static_cast<size_t>(i), scratch));
      if (!ok) {
        break;
      }
    }
  }
  return absl::OkStatus();
}

namespace {

class ParsedRepeatedFieldValueIterator final : public ValueIterator {
 public:
  ParsedRepeatedFieldValueIterator(
      Owned<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<common_internal::RepeatedFieldAccessor> accessor)
      : message_(std::move(message)),
        field_(field),
        reflection_(message_->GetReflection()),
        accessor_(accessor),
        size_(reflection_->FieldSize(*message_, field_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next called after ValueIterator::HasNext returned "
          "false");
    }
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
    std::tie(descriptor_pool, message_factory) =
        GetDescriptorPoolAndMessageFactory(value_manager, *message_);
    (*accessor_)(value_manager.GetMemoryManager().arena(), message_, field_,
                 reflection_, index_, descriptor_pool, message_factory, result);
    ++index_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const absl::Nonnull<const google::protobuf::FieldDescriptor*> field_;
  const absl::Nonnull<const google::protobuf::Reflection*> reflection_;
  const absl::Nonnull<common_internal::RepeatedFieldAccessor> accessor_;
  const int size_;
  int index_ = 0;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedRepeatedFieldValue::NewIterator(ValueManager& value_manager) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return NewEmptyValueIterator();
  }
  CEL_ASSIGN_OR_RETURN(auto accessor,
                       common_internal::RepeatedFieldAccessorFor(field_));
  return std::make_unique<ParsedRepeatedFieldValueIterator>(message_, field_,
                                                            accessor);
}

absl::Status ParsedRepeatedFieldValue::Contains(ValueManager& value_manager,
                                                const Value& other,
                                                Value& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    result = BoolValue(false);
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  const int size = reflection->FieldSize(*message_, field_);
  if (size > 0) {
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
    std::tie(descriptor_pool, message_factory) =
        GetDescriptorPoolAndMessageFactory(value_manager, *message_);
    Allocator<> allocator = value_manager.GetMemoryManager().arena();
    CEL_ASSIGN_OR_RETURN(auto accessor,
                         common_internal::RepeatedFieldAccessorFor(field_));
    Value scratch;
    for (int i = 0; i < size; ++i) {
      (*accessor)(allocator, message_, field_, reflection, i, descriptor_pool,
                  message_factory, scratch);
      CEL_RETURN_IF_ERROR(scratch.Equal(value_manager, other, result));
      if (result.IsTrue()) {
        return absl::OkStatus();
      }
    }
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedRepeatedFieldValue::Contains(
    ValueManager& value_manager, const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

absl::Nonnull<const google::protobuf::Reflection*>
ParsedRepeatedFieldValue::GetReflection() const {
  return message_->GetReflection();
}

}  // namespace cel
