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
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/list_value_builder.h"
#include "common/values/parsed_json_value.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/map.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"

namespace cel {

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

absl::Status ParsedJsonMapValue::SerializeTo(AnyToJsonConverter& converter,
                                             absl::Cord& value) const {
  if (value_ == nullptr) {
    value.Clear();
    return absl::OkStatus();
  }
  if (!value_->SerializePartialToCord(&value)) {
    return absl::UnknownError("failed to serialize protocol buffer message");
  }
  return absl::OkStatus();
}

absl::StatusOr<Json> ParsedJsonMapValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  if (value_ == nullptr) {
    return JsonObject();
  }
  return internal::ProtoJsonMapToNativeJsonMap(*value_);
}

absl::Status ParsedJsonMapValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  if (auto other_value = other.AsParsedJsonMap(); other_value) {
    result = BoolValue(*this == *other_value);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedMapField(); other_value) {
    if (value_ == nullptr) {
      result = BoolValue(other_value->IsEmpty());
      return absl::OkStatus();
    }
    const auto* descriptor_pool = value_manager.descriptor_pool();
    auto* message_factory = value_manager.message_factory();
    if (descriptor_pool == nullptr) {
      descriptor_pool = other_value->message_->GetDescriptor()->file()->pool();
      if (message_factory == nullptr) {
        message_factory =
            other_value->message_->GetReflection()->GetMessageFactory();
      }
    }
    ABSL_DCHECK(other_value->field_ != nullptr);
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageFieldEquals(
                        *value_, *other_value->message_, other_value->field_,
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

absl::StatusOr<Value> ParsedJsonMapValue::Equal(ValueManager& value_manager,
                                                const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

ParsedJsonMapValue ParsedJsonMapValue::Clone(Allocator<> allocator) const {
  if (value_ == nullptr) {
    return ParsedJsonMapValue();
  }
  if (value_.arena() == allocator.arena()) {
    return *this;
  }
  auto cloned = WrapShared(value_->New(allocator.arena()), allocator);
  cloned->CopyFrom(*value_);
  return ParsedJsonMapValue(std::move(cloned));
}

size_t ParsedJsonMapValue::Size() const {
  if (value_ == nullptr) {
    return 0;
  }
  return static_cast<size_t>(
      well_known_types::GetStructReflectionOrDie(value_->GetDescriptor())
          .FieldsSize(*value_));
}

absl::Status ParsedJsonMapValue::Get(ValueManager& value_manager,
                                     const Value& key, Value& result) const {
  CEL_ASSIGN_OR_RETURN(bool ok, Find(value_manager, key, result));
  if (ABSL_PREDICT_FALSE(!ok) && !(result.IsError() || result.IsUnknown())) {
    result = NoSuchKeyError(key.DebugString());
  }
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonMapValue::Get(ValueManager& value_manager,
                                              const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> ParsedJsonMapValue::Find(ValueManager& value_manager,
                                              const Value& key,
                                              Value& result) const {
  if (key.IsError() || key.IsUnknown()) {
    result = key;
    return false;
  }
  if (value_ != nullptr) {
    if (auto string_key = key.AsString(); string_key) {
      if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
        result = NullValue();
        return false;
      }
      std::string key_scratch;
      if (const auto* value =
              well_known_types::GetStructReflectionOrDie(
                  value_->GetDescriptor())
                  .FindField(*value_, string_key->NativeString(key_scratch));
          value != nullptr) {
        result = common_internal::ParsedJsonValue(
            value_manager.GetMemoryManager().arena(), Borrowed(value_, value));
        return true;
      }
      result = NullValue();
      return false;
    }
  }
  result = NullValue();
  return false;
}

absl::StatusOr<std::pair<Value, bool>> ParsedJsonMapValue::Find(
    ValueManager& value_manager, const Value& key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto found, Find(value_manager, key, result));
  if (found) {
    return std::pair{std::move(result), found};
  }
  return std::pair{NullValue(), found};
}

absl::Status ParsedJsonMapValue::Has(ValueManager& value_manager,
                                     const Value& key, Value& result) const {
  if (key.IsError() || key.IsUnknown()) {
    result = key;
    return absl::OkStatus();
  }
  if (value_ != nullptr) {
    if (auto string_key = key.AsString(); string_key) {
      if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
        result = BoolValue(false);
        return absl::OkStatus();
      }
      std::string key_scratch;
      if (const auto* value =
              well_known_types::GetStructReflectionOrDie(
                  value_->GetDescriptor())
                  .FindField(*value_, string_key->NativeString(key_scratch));
          value != nullptr) {
        result = BoolValue(true);
      } else {
        result = BoolValue(false);
      }
      return absl::OkStatus();
    }
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonMapValue::Has(ValueManager& value_manager,
                                              const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status ParsedJsonMapValue::ListKeys(ValueManager& value_manager,
                                          ListValue& result) const {
  if (value_ == nullptr) {
    result = ListValue();
    return absl::OkStatus();
  }
  const auto reflection =
      well_known_types::GetStructReflectionOrDie(value_->GetDescriptor());
  auto builder = common_internal::NewListValueBuilder(value_manager);
  builder->Reserve(static_cast<size_t>(reflection.FieldsSize(*value_)));
  auto keys_begin = reflection.BeginFields(*value_);
  const auto keys_end = reflection.EndFields(*value_);
  for (; keys_begin != keys_end; ++keys_begin) {
    CEL_RETURN_IF_ERROR(
        builder->Add(Value::MapFieldKeyString(value_, keys_begin.GetKey())));
  }
  result = std::move(*builder).Build();
  return absl::OkStatus();
}

absl::StatusOr<ListValue> ParsedJsonMapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
}

absl::Status ParsedJsonMapValue::ForEach(ValueManager& value_manager,
                                         ForEachCallback callback) const {
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
    key_scratch = StringValue(value_manager.GetMemoryManager().arena(),
                              map_begin.GetKey().GetStringValue());
    value_scratch = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(),
        Borrowed(value_, &map_begin.GetValueRef().GetMessageValue()));
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
  explicit ParsedJsonMapValueIterator(Owned<const google::protobuf::Message> message)
      : message_(std::move(message)),
        reflection_(well_known_types::GetStructReflectionOrDie(
            message_->GetDescriptor())),
        begin_(reflection_.BeginFields(*message_)),
        end_(reflection_.EndFields(*message_)) {}

  bool HasNext() override { return begin_ != end_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(begin_ == end_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    // We have to copy until `google::protobuf::MapKey` is just a view.
    std::string scratch =
        static_cast<std::string>(begin_.GetKey().GetStringValue());
    result = StringValue(value_manager.GetMemoryManager().arena(),
                         std::move(scratch));
    ++begin_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const well_known_types::StructReflection reflection_;
  google::protobuf::MapIterator begin_;
  const google::protobuf::MapIterator end_;
  std::string scratch_;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedJsonMapValue::NewIterator(ValueManager& value_manager) const {
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
