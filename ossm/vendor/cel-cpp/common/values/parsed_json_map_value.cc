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

#include "common/values/parsed_json_map_value.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/values/parsed_json_value.h"
#include "common/values/values.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/map.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel {

using ::cel::well_known_types::ValueReflection;

namespace common_internal {

absl::Status CheckWellKnownStructMessage(const google::protobuf::Message& message) {
  return internal::CheckJsonMap(message);
}

}  // namespace common_internal

std::string ParsedJsonMapValue::DebugString() const {
  if (value_ == nullptr) {
    return "{}";
  }
  return internal::JsonMapDebugString(*value_);
}

absl::Status ParsedJsonMapValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  if (value_ == nullptr) {
    return absl::OkStatus();
  }

  if (!value_->SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError(
        "failed to serialize message: google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::Status ParsedJsonMapValue::ConvertToJson(
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
  auto* message = value_reflection.MutableStructValue(json);
  message->Clear();

  if (value_ == nullptr) {
    return absl::OkStatus();
  }

  if (value_->GetDescriptor() == message->GetDescriptor()) {
    // We can directly use google::protobuf::Message::Copy().
    message->CopyFrom(*value_);
  } else {
    // Equivalent descriptors but not identical. Must serialize and deserialize.
    absl::Cord serialized;
    if (!value_->SerializePartialToString(&serialized)) {
      return absl::UnknownError(
          absl::StrCat("failed to serialize message: ", value_->GetTypeName()));
    }
    if (!message->ParsePartialFromString(serialized)) {
      return absl::UnknownError(
          absl::StrCat("failed to parsed message: ", message->GetTypeName()));
    }
  }
  return absl::OkStatus();
}

absl::Status ParsedJsonMapValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  if (value_ == nullptr) {
    json->Clear();
    return absl::OkStatus();
  }

  if (value_->GetDescriptor() == json->GetDescriptor()) {
    // We can directly use google::protobuf::Message::Copy().
    json->CopyFrom(*value_);
  } else {
    // Equivalent descriptors but not identical. Must serialize and deserialize.
    absl::Cord serialized;
    if (!value_->SerializePartialToString(&serialized)) {
      return absl::UnknownError(
          absl::StrCat("failed to serialize message: ", value_->GetTypeName()));
    }
    if (!json->ParsePartialFromString(serialized)) {
      return absl::UnknownError(
          absl::StrCat("failed to parsed message: ", json->GetTypeName()));
    }
  }
  return absl::OkStatus();
}

absl::Status ParsedJsonMapValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (auto other_value = other.AsParsedJsonMap(); other_value) {
    *result = BoolValue(*this == *other_value);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedMapField(); other_value) {
    if (value_ == nullptr) {
      *result = BoolValue(other_value->IsEmpty());
      return absl::OkStatus();
    }
    ABSL_DCHECK(other_value->field_ != nullptr);
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageFieldEquals(
                        *value_, *other_value->message_, other_value->field_,
                        descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsMap(); other_value) {
    return common_internal::MapValueEqual(MapValue(*this), *other_value,
                                          descriptor_pool, message_factory,
                                          arena, result);
  }
  *result = FalseValue();
  return absl::OkStatus();
}

ParsedJsonMapValue ParsedJsonMapValue::Clone(
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  if (value_ == nullptr) {
    return ParsedJsonMapValue();
  }
  if (arena_ == arena) {
    return *this;
  }
  auto* cloned = value_->New(arena);
  cloned->CopyFrom(*value_);
  return ParsedJsonMapValue(cloned, arena);
}

size_t ParsedJsonMapValue::Size() const {
  if (value_ == nullptr) {
    return 0;
  }
  return static_cast<size_t>(
      well_known_types::GetStructReflectionOrDie(value_->GetDescriptor())
          .FieldsSize(*value_));
}

absl::Status ParsedJsonMapValue::Get(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  CEL_ASSIGN_OR_RETURN(
      bool ok, Find(key, descriptor_pool, message_factory, arena, result));
  if (ABSL_PREDICT_FALSE(!ok) && !(result->IsError() || result->IsUnknown())) {
    *result = NoSuchKeyError(key.DebugString());
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> ParsedJsonMapValue::Find(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (key.IsError() || key.IsUnknown()) {
    *result = key;
    return false;
  }
  if (value_ != nullptr) {
    if (auto string_key = key.AsString(); string_key) {
      if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
        *result = NullValue();
        return false;
      }
      std::string key_scratch;
      if (const auto* value =
              well_known_types::GetStructReflectionOrDie(
                  value_->GetDescriptor())
                  .FindField(*value_, string_key->NativeString(key_scratch));
          value != nullptr) {
        *result = common_internal::ParsedJsonValue(value, arena);
        return true;
      }
      *result = NullValue();
      return false;
    }
  }
  *result = NullValue();
  return false;
}

absl::Status ParsedJsonMapValue::Has(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (key.IsError() || key.IsUnknown()) {
    *result = key;
    return absl::OkStatus();
  }
  if (value_ != nullptr) {
    if (auto string_key = key.AsString(); string_key) {
      if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
        *result = FalseValue();
        return absl::OkStatus();
      }
      std::string key_scratch;
      if (const auto* value =
              well_known_types::GetStructReflectionOrDie(
                  value_->GetDescriptor())
                  .FindField(*value_, string_key->NativeString(key_scratch));
          value != nullptr) {
        *result = TrueValue();
      } else {
        *result = FalseValue();
      }
      return absl::OkStatus();
    }
  }
  *result = FalseValue();
  return absl::OkStatus();
}

absl::Status ParsedJsonMapValue::ListKeys(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const {
  if (value_ == nullptr) {
    *result = ListValue();
    return absl::OkStatus();
  }
  const auto reflection =
      well_known_types::GetStructReflectionOrDie(value_->GetDescriptor());
  auto builder = NewListValueBuilder(arena);
  builder->Reserve(static_cast<size_t>(reflection.FieldsSize(*value_)));
  auto keys_begin = reflection.BeginFields(*value_);
  const auto keys_end = reflection.EndFields(*value_);
  for (; keys_begin != keys_end; ++keys_begin) {
    CEL_RETURN_IF_ERROR(builder->Add(
        Value::WrapMapFieldKeyString(keys_begin.GetKey(), value_, arena)));
  }
  *result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::Status ParsedJsonMapValue::ForEach(
    ForEachCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  if (value_ == nullptr) {
    return absl::OkStatus();
  }
  const auto reflection =
      well_known_types::GetStructReflectionOrDie(value_->GetDescriptor());
  Value key_scratch;
  Value value_scratch;
  auto map_begin = reflection.BeginFields(*value_);
  const auto map_end = reflection.EndFields(*value_);
  for (; map_begin != map_end; ++map_begin) {
    // We have to copy until `google::protobuf::MapKey` is just a view.
    key_scratch = StringValue(arena, map_begin.GetKey().GetStringValue());
    value_scratch = common_internal::ParsedJsonValue(
        &map_begin.GetValueRef().GetMessageValue(), arena);
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key_scratch, value_scratch));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

namespace {

class ParsedJsonMapValueIterator final : public ValueIterator {
 public:
  explicit ParsedJsonMapValueIterator(
      const google::protobuf::Message* absl_nonnull message)
      : message_(message),
        reflection_(well_known_types::GetStructReflectionOrDie(
            message_->GetDescriptor())),
        begin_(reflection_.BeginFields(*message_)),
        end_(reflection_.EndFields(*message_)) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    *result = Value::WrapMapFieldKeyString(begin_.GetKey(), message_, arena);
    ++begin_;
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

    if (begin_ == end_) {
      return false;
    }
    *key_or_value =
        Value::WrapMapFieldKeyString(begin_.GetKey(), message_, arena);
    ++begin_;
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

    if (begin_ == end_) {
      return false;
    }
    *key = Value::WrapMapFieldKeyString(begin_.GetKey(), message_, arena);
    if (value != nullptr) {
      *value = common_internal::ParsedJsonValue(
          &begin_.GetValueRef().GetMessageValue(), arena);
    }
    ++begin_;
    return true;
  }

 private:
  const google::protobuf::Message* absl_nonnull const message_;
  const well_known_types::StructReflection reflection_;
  google::protobuf::MapIterator begin_;
  const google::protobuf::MapIterator end_;
  std::string scratch_;
};

}  // namespace

absl::StatusOr<absl_nonnull std::unique_ptr<ValueIterator>>
ParsedJsonMapValue::NewIterator() const {
  if (value_ == nullptr) {
    return NewEmptyValueIterator();
  }
  return std::make_unique<ParsedJsonMapValueIterator>(value_);
}

bool operator==(const ParsedJsonMapValue& lhs, const ParsedJsonMapValue& rhs) {
  if (cel::to_address(lhs.value_) == cel::to_address(rhs.value_)) {
    return true;
  }
  if (cel::to_address(lhs.value_) == nullptr) {
    return rhs.IsEmpty();
  }
  if (cel::to_address(rhs.value_) == nullptr) {
    return lhs.IsEmpty();
  }
  return internal::JsonMapEquals(*lhs.value_, *rhs.value_);
}

}  // namespace cel
