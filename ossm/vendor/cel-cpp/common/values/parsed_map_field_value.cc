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

#include "common/values/parsed_map_field_value.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "common/values/values.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace cel {

using ::cel::well_known_types::ValueReflection;

std::string ParsedMapFieldValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return "INVALID";
  }
  return "VALID";
}

absl::Status ParsedMapFieldValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return absl::OkStatus();
  }
  // We have to convert to google.protobuf.Struct first.
  google::protobuf::Value message;
  CEL_RETURN_IF_ERROR(internal::MessageFieldToJson(
      *message_, field_, descriptor_pool, message_factory, &message));
  if (!message.list_value().SerializePartialToZeroCopyStream(output)) {
    return absl::UnknownError("failed to serialize google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::Status ParsedMapFieldValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    ValueReflection value_reflection;
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(json->GetDescriptor()));
    value_reflection.MutableStructValue(json)->Clear();
    return absl::OkStatus();
  }
  return internal::MessageFieldToJson(*message_, field_, descriptor_pool,
                                      message_factory, json);
}

absl::Status ParsedMapFieldValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    json->Clear();
    return absl::OkStatus();
  }
  return internal::MessageFieldToJson(*message_, field_, descriptor_pool,
                                      message_factory, json);
}

absl::Status ParsedMapFieldValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  if (auto other_value = other.AsParsedMapField(); other_value) {
    ABSL_DCHECK(field_ != nullptr);
    ABSL_DCHECK(other_value->field_ != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageFieldEquals(
                        *message_, field_, *other_value->message_,
                        other_value->field_, descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedJsonMap(); other_value) {
    if (other_value->value_ == nullptr) {
      *result = BoolValue(IsEmpty());
      return absl::OkStatus();
    }
    ABSL_DCHECK(field_ != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal,
        internal::MessageFieldEquals(*message_, field_, *other_value->value_,
                                     descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsMap(); other_value) {
    return common_internal::MapValueEqual(MapValue(*this), *other_value,
                                          descriptor_pool, message_factory,
                                          arena, result);
  }
  *result = BoolValue(false);
  return absl::OkStatus();
}

bool ParsedMapFieldValue::IsZeroValue() const { return IsEmpty(); }

ParsedMapFieldValue ParsedMapFieldValue::Clone(
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(*this);

  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return ParsedMapFieldValue();
  }
  if (arena_ == arena) {
    return *this;
  }
  auto field = message_->GetReflection()->GetRepeatedFieldRef<google::protobuf::Message>(
      *message_, field_);
  auto* cloned = message_->New(arena);
  auto cloned_field =
      cloned->GetReflection()->GetMutableRepeatedFieldRef<google::protobuf::Message>(
          cloned, field_);
  cloned_field.CopyFrom(field);
  return ParsedMapFieldValue(cloned, field_, arena);
}

bool ParsedMapFieldValue::IsEmpty() const { return Size() == 0; }

size_t ParsedMapFieldValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return 0;
  }
  return static_cast<size_t>(extensions::protobuf_internal::MapSize(
      *GetReflection(), *message_, *field_));
}

namespace {

absl::optional<int32_t> ValueAsInt32(const Value& value) {
  if (auto int_value = value.AsInt();
      int_value &&
      int_value->NativeValue() >= std::numeric_limits<int32_t>::min() &&
      int_value->NativeValue() <= std::numeric_limits<int32_t>::max()) {
    return static_cast<int32_t>(int_value->NativeValue());
  } else if (auto uint_value = value.AsUint();
             uint_value &&
             uint_value->NativeValue() <= std::numeric_limits<int32_t>::max()) {
    return static_cast<int32_t>(uint_value->NativeValue());
  } else if (auto double_value = value.AsDouble();
             double_value &&
             static_cast<double>(static_cast<int32_t>(
                 double_value->NativeValue())) == double_value->NativeValue()) {
    return static_cast<int32_t>(double_value->NativeValue());
  }
  return absl::nullopt;
}

absl::optional<int64_t> ValueAsInt64(const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    return int_value->NativeValue();
  } else if (auto uint_value = value.AsUint();
             uint_value &&
             uint_value->NativeValue() <= std::numeric_limits<int64_t>::max()) {
    return static_cast<int64_t>(uint_value->NativeValue());
  } else if (auto double_value = value.AsDouble();
             double_value &&
             static_cast<double>(static_cast<int64_t>(
                 double_value->NativeValue())) == double_value->NativeValue()) {
    return static_cast<int64_t>(double_value->NativeValue());
  }
  return absl::nullopt;
}

absl::optional<uint32_t> ValueAsUInt32(const Value& value) {
  if (auto int_value = value.AsInt();
      int_value && int_value->NativeValue() >= 0 &&
      int_value->NativeValue() <= std::numeric_limits<uint32_t>::max()) {
    return static_cast<uint32_t>(int_value->NativeValue());
  } else if (auto uint_value = value.AsUint();
             uint_value && uint_value->NativeValue() <=
                               std::numeric_limits<uint32_t>::max()) {
    return static_cast<uint32_t>(uint_value->NativeValue());
  } else if (auto double_value = value.AsDouble();
             double_value &&
             static_cast<double>(static_cast<uint32_t>(
                 double_value->NativeValue())) == double_value->NativeValue()) {
    return static_cast<uint32_t>(double_value->NativeValue());
  }
  return absl::nullopt;
}

absl::optional<uint64_t> ValueAsUInt64(const Value& value) {
  if (auto int_value = value.AsInt();
      int_value && int_value->NativeValue() >= 0) {
    return static_cast<uint64_t>(int_value->NativeValue());
  } else if (auto uint_value = value.AsUint(); uint_value) {
    return uint_value->NativeValue();
  } else if (auto double_value = value.AsDouble();
             double_value &&
             static_cast<double>(static_cast<uint64_t>(
                 double_value->NativeValue())) == double_value->NativeValue()) {
    return static_cast<uint64_t>(double_value->NativeValue());
  }
  return absl::nullopt;
}

bool ValueToProtoMapKey(const Value& key,
                        google::protobuf::FieldDescriptor::CppType cpp_type,
                        google::protobuf::MapKey* absl_nonnull proto_key,
                        std::string& proto_key_scratch) {
  switch (cpp_type) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL: {
      if (auto bool_key = key.AsBool(); bool_key) {
        proto_key->SetBoolValue(bool_key->NativeValue());
        return true;
      }
      return false;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32: {
      if (auto int_key = ValueAsInt32(key); int_key) {
        proto_key->SetInt32Value(*int_key);
        return true;
      }
      return false;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64: {
      if (auto int_key = ValueAsInt64(key); int_key) {
        proto_key->SetInt64Value(*int_key);
        return true;
      }
      return false;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32: {
      if (auto int_key = ValueAsUInt32(key); int_key) {
        proto_key->SetUInt32Value(*int_key);
        return true;
      }
      return false;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64: {
      if (auto int_key = ValueAsUInt64(key); int_key) {
        proto_key->SetUInt64Value(*int_key);
        return true;
      }
      return false;
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      if (auto string_key = key.AsString(); string_key) {
        proto_key_scratch = string_key->NativeString();
        proto_key->SetStringValue(proto_key_scratch);
        return true;
      }
      return false;
    }
    default:
      // protobuf map keys can only be bool, integrals, or string.
      return false;
  }
}

}  // namespace

absl::Status ParsedMapFieldValue::Get(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  CEL_ASSIGN_OR_RETURN(
      bool ok, Find(key, descriptor_pool, message_factory, arena, result));
  if (ABSL_PREDICT_FALSE(!ok) && !(result->IsError() || result->IsUnknown())) {
    *result = ErrorValue(NoSuchKeyError(key.DebugString()));
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> ParsedMapFieldValue::Find(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(*this);
  ABSL_DCHECK(message_ != nullptr);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    *result = NullValue();
    return false;
  }
  if (key.IsError() || key.IsUnknown()) {
    *result = key;
    return false;
  }
  const google::protobuf::Descriptor* absl_nonnull entry_descriptor =
      field_->message_type();
  const google::protobuf::FieldDescriptor* absl_nonnull key_field =
      entry_descriptor->map_key();
  const google::protobuf::FieldDescriptor* absl_nonnull value_field =
      entry_descriptor->map_value();
  std::string proto_key_scratch;
  google::protobuf::MapKey proto_key;
  if (!ValueToProtoMapKey(key, key_field->cpp_type(), &proto_key,
                          proto_key_scratch)) {
    *result = NullValue();
    return false;
  }
  google::protobuf::MapValueConstRef proto_value;
  if (!extensions::protobuf_internal::LookupMapValue(
          *GetReflection(), *message_, *field_, proto_key, &proto_value)) {
    *result = NullValue();
    return false;
  }
  if (arena_ == nullptr) {
    *result =
        Value::WrapMapFieldValueUnsafe(proto_value, message_, value_field,
                                       descriptor_pool, message_factory, arena);
  } else {
    *result = Value::WrapMapFieldValue(proto_value, message_, value_field,
                                       descriptor_pool, message_factory, arena);
  }
  return true;
}

absl::Status ParsedMapFieldValue::Has(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    *result = BoolValue(false);
    return absl::OkStatus();
  }
  const google::protobuf::FieldDescriptor* absl_nonnull key_field =
      field_->message_type()->map_key();
  std::string proto_key_scratch;
  google::protobuf::MapKey proto_key;
  bool bool_result;
  if (ValueToProtoMapKey(key, key_field->cpp_type(), &proto_key,
                         proto_key_scratch)) {
    google::protobuf::MapValueConstRef proto_value;
    bool_result = extensions::protobuf_internal::LookupMapValue(
        *GetReflection(), *message_, *field_, proto_key, &proto_value);
  } else {
    bool_result = false;
  }
  *result = BoolValue(bool_result);
  return absl::OkStatus();
}

absl::Status ParsedMapFieldValue::ListKeys(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const {
  ABSL_DCHECK(*this);
  if (field_ == nullptr) {
    *result = ListValue();
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  if (reflection->FieldSize(*message_, field_) == 0) {
    *result = ListValue();
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto key_accessor,
                       common_internal::MapFieldKeyAccessorFor(
                           field_->message_type()->map_key()));
  auto builder = NewListValueBuilder(arena);
  builder->Reserve(Size());
  auto begin =
      extensions::protobuf_internal::MapBegin(*reflection, *message_, *field_);
  const auto end =
      extensions::protobuf_internal::MapEnd(*reflection, *message_, *field_);
  for (; begin != end; ++begin) {
    Value scratch;
    (*key_accessor)(begin.GetKey(), message_, arena, &scratch);
    CEL_RETURN_IF_ERROR(builder->Add(std::move(scratch)));
  }
  *result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::Status ParsedMapFieldValue::ForEach(
    ForEachCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(*this);
  if (field_ == nullptr) {
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  if (reflection->FieldSize(*message_, field_) > 0) {
    const auto* value_field = field_->message_type()->map_value();
    CEL_ASSIGN_OR_RETURN(auto key_accessor,
                         common_internal::MapFieldKeyAccessorFor(
                             field_->message_type()->map_key()));
    CEL_ASSIGN_OR_RETURN(
        auto value_accessor,
        common_internal::MapFieldValueAccessorFor(value_field));
    auto begin = extensions::protobuf_internal::MapBegin(*reflection, *message_,
                                                         *field_);
    const auto end =
        extensions::protobuf_internal::MapEnd(*reflection, *message_, *field_);
    Value key_scratch;
    Value value_scratch;
    for (; begin != end; ++begin) {
      (*key_accessor)(begin.GetKey(), message_, arena, &key_scratch);
      (*value_accessor)(begin.GetValueRef(), message_, value_field,
                        descriptor_pool, message_factory, arena,
                        &value_scratch);
      CEL_ASSIGN_OR_RETURN(auto ok, callback(key_scratch, value_scratch));
      if (!ok) {
        break;
      }
    }
  }
  return absl::OkStatus();
}

namespace {

class ParsedMapFieldValueIterator final : public ValueIterator {
 public:
  ParsedMapFieldValueIterator(
      const google::protobuf::Message* absl_nonnull message,
      const google::protobuf::FieldDescriptor* absl_nonnull field,
      absl_nonnull common_internal::MapFieldKeyAccessor key_accessor,
      absl_nonnull common_internal::MapFieldValueAccessor value_accessor)
      : message_(message),
        value_field_(field->message_type()->map_value()),
        key_accessor_(key_accessor),
        value_accessor_(value_accessor),
        begin_(extensions::protobuf_internal::MapBegin(
            *message_->GetReflection(), *message_, *field)),
        end_(extensions::protobuf_internal::MapEnd(*message_->GetReflection(),
                                                   *message_, *field)) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next called after ValueIterator::HasNext returned "
          "false");
    }
    (*key_accessor_)(begin_.GetKey(), message_, arena, result);
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
    (*key_accessor_)(begin_.GetKey(), message_, arena, key_or_value);
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
    (*key_accessor_)(begin_.GetKey(), message_, arena, key);
    if (value != nullptr) {
      (*value_accessor_)(begin_.GetValueRef(), message_, value_field_,
                         descriptor_pool, message_factory, arena, value);
    }
    ++begin_;
    return true;
  }

 private:
  const google::protobuf::Message* absl_nonnull const message_;
  const google::protobuf::FieldDescriptor* absl_nonnull const value_field_;
  const absl_nonnull common_internal::MapFieldKeyAccessor key_accessor_;
  const absl_nonnull common_internal::MapFieldValueAccessor value_accessor_;
  google::protobuf::MapIterator begin_;
  const google::protobuf::MapIterator end_;
};

}  // namespace

absl::StatusOr<absl_nonnull std::unique_ptr<ValueIterator>>
ParsedMapFieldValue::NewIterator() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return NewEmptyValueIterator();
  }
  CEL_ASSIGN_OR_RETURN(auto key_accessor,
                       common_internal::MapFieldKeyAccessorFor(
                           field_->message_type()->map_key()));
  CEL_ASSIGN_OR_RETURN(auto value_accessor,
                       common_internal::MapFieldValueAccessorFor(
                           field_->message_type()->map_value()));
  return std::make_unique<ParsedMapFieldValueIterator>(
      message_, field_, key_accessor, value_accessor);
}

const google::protobuf::Reflection* absl_nonnull ParsedMapFieldValue::GetReflection()
    const {
  return message_->GetReflection();
}

}  // namespace cel
