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
#include <tuple>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace cel {

std::string ParsedMapFieldValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return "INVALID";
  }
  return "VALID";
}

absl::Status ParsedMapFieldValue::SerializeTo(AnyToJsonConverter& converter,
                                              absl::Cord& value) const {
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
  if (!json->struct_value().SerializePartialToCord(&value)) {
    return absl::UnknownError("failed to serialize google.protobuf.Struct");
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> ParsedMapFieldValue::ConvertToJson(
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
  return internal::ProtoJsonMapToNativeJsonMap(json->struct_value());
}

absl::StatusOr<JsonObject> ParsedMapFieldValue::ConvertToJsonObject(
    AnyToJsonConverter& converter) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJson(converter));
  return absl::get<JsonObject>(std::move(json));
}

absl::Status ParsedMapFieldValue::Equal(ValueManager& value_manager,
                                        const Value& other,
                                        Value& result) const {
  if (auto other_value = other.AsParsedMapField(); other_value) {
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
  if (auto other_value = other.AsParsedJsonMap(); other_value) {
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
  if (auto other_value = other.AsMap(); other_value) {
    return common_internal::MapValueEqual(value_manager, MapValue(*this),
                                          *other_value, result);
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedMapFieldValue::Equal(ValueManager& value_manager,
                                                 const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool ParsedMapFieldValue::IsZeroValue() const { return IsEmpty(); }

ParsedMapFieldValue ParsedMapFieldValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return ParsedMapFieldValue();
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
  return ParsedMapFieldValue(std::move(cloned), field_);
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
                        absl::Nonnull<google::protobuf::MapKey*> proto_key,
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

absl::Status ParsedMapFieldValue::Get(ValueManager& value_manager,
                                      const Value& key, Value& result) const {
  CEL_ASSIGN_OR_RETURN(bool ok, Find(value_manager, key, result));
  if (ABSL_PREDICT_FALSE(!ok) && !(result.IsError() || result.IsUnknown())) {
    result = ErrorValue(NoSuchKeyError(key.DebugString()));
  }
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedMapFieldValue::Get(ValueManager& value_manager,
                                               const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> ParsedMapFieldValue::Find(ValueManager& value_manager,
                                               const Value& key,
                                               Value& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    result = NullValue();
    return false;
  }
  if (key.IsError() || key.IsUnknown()) {
    result = key;
    return false;
  }
  absl::Nonnull<const google::protobuf::Descriptor*> entry_descriptor =
      field_->message_type();
  absl::Nonnull<const google::protobuf::FieldDescriptor*> key_field =
      entry_descriptor->map_key();
  absl::Nonnull<const google::protobuf::FieldDescriptor*> value_field =
      entry_descriptor->map_value();
  std::string proto_key_scratch;
  google::protobuf::MapKey proto_key;
  if (!ValueToProtoMapKey(key, key_field->cpp_type(), &proto_key,
                          proto_key_scratch)) {
    result = NullValue();
    return false;
  }
  google::protobuf::MapValueConstRef proto_value;
  if (!extensions::protobuf_internal::LookupMapValue(
          *GetReflection(), *message_, *field_, proto_key, &proto_value)) {
    result = NullValue();
    return false;
  }
  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
  absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
  std::tie(descriptor_pool, message_factory) =
      GetDescriptorPoolAndMessageFactory(value_manager, *message_);
  result = Value::MapFieldValue(message_, value_field, proto_value,
                                descriptor_pool, message_factory);
  return true;
}

absl::StatusOr<std::pair<Value, bool>> ParsedMapFieldValue::Find(
    ValueManager& value_manager, const Value& key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto found, Find(value_manager, key, result));
  if (found) {
    return std::pair{std::move(result), found};
  }
  return std::pair{NullValue(), found};
}

absl::Status ParsedMapFieldValue::Has(ValueManager& value_manager,
                                      const Value& key, Value& result) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    result = BoolValue(false);
    return absl::OkStatus();
  }
  absl::Nonnull<const google::protobuf::FieldDescriptor*> key_field =
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
  result = BoolValue(bool_result);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedMapFieldValue::Has(ValueManager& value_manager,
                                               const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status ParsedMapFieldValue::ListKeys(ValueManager& value_manager,
                                           ListValue& result) const {
  ABSL_DCHECK(*this);
  if (field_ == nullptr) {
    result = ListValue();
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  if (reflection->FieldSize(*message_, field_) == 0) {
    result = ListValue();
    return absl::OkStatus();
  }
  Allocator<> allocator = value_manager.GetMemoryManager().arena();
  CEL_ASSIGN_OR_RETURN(auto key_accessor,
                       common_internal::MapFieldKeyAccessorFor(
                           field_->message_type()->map_key()));
  CEL_ASSIGN_OR_RETURN(auto builder,
                       value_manager.NewListValueBuilder(ListType()));
  builder->Reserve(Size());
  auto begin =
      extensions::protobuf_internal::MapBegin(*reflection, *message_, *field_);
  const auto end =
      extensions::protobuf_internal::MapEnd(*reflection, *message_, *field_);
  for (; begin != end; ++begin) {
    Value scratch;
    (*key_accessor)(allocator, message_, begin.GetKey(), scratch);
    CEL_RETURN_IF_ERROR(builder->Add(std::move(scratch)));
  }
  result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::StatusOr<ListValue> ParsedMapFieldValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
}

absl::Status ParsedMapFieldValue::ForEach(ValueManager& value_manager,
                                          ForEachCallback callback) const {
  ABSL_DCHECK(*this);
  if (field_ == nullptr) {
    return absl::OkStatus();
  }
  const auto* reflection = message_->GetReflection();
  if (reflection->FieldSize(*message_, field_) > 0) {
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool;
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory;
    std::tie(descriptor_pool, message_factory) =
        GetDescriptorPoolAndMessageFactory(value_manager, *message_);
    Allocator<> allocator = value_manager.GetMemoryManager().arena();
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
      (*key_accessor)(allocator, message_, begin.GetKey(), key_scratch);
      (*value_accessor)(message_, begin.GetValueRef(), value_field,
                        descriptor_pool, message_factory, value_scratch);
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
      Owned<const google::protobuf::Message> message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<common_internal::MapFieldKeyAccessor> accessor)
      : message_(std::move(message)),
        accessor_(accessor),
        begin_(extensions::protobuf_internal::MapBegin(
            *message_->GetReflection(), *message_, *field)),
        end_(extensions::protobuf_internal::MapEnd(*message_->GetReflection(),
                                                   *message_, *field)) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next called after ValueIterator::HasNext returned "
          "false");
    }
    (*accessor_)(value_manager.GetMemoryManager().arena(), message_,
                 begin_.GetKey(), result);
    ++begin_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const absl::Nonnull<common_internal::MapFieldKeyAccessor> accessor_;
  google::protobuf::MapIterator begin_;
  const google::protobuf::MapIterator end_;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedMapFieldValue::NewIterator(ValueManager& value_manager) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return NewEmptyValueIterator();
  }
  CEL_ASSIGN_OR_RETURN(auto accessor, common_internal::MapFieldKeyAccessorFor(
                                          field_->message_type()->map_key()));
  return std::make_unique<ParsedMapFieldValueIterator>(message_, field_,
                                                       accessor);
}

absl::Nonnull<const google::protobuf::Reflection*> ParsedMapFieldValue::GetReflection()
    const {
  return message_->GetReflection();
}

}  // namespace cel
