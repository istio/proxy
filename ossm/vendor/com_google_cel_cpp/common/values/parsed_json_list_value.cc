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

#include "common/values/parsed_json_list_value.h"

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
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/parsed_json_value.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/message.h"

namespace cel {

namespace common_internal {

absl::Status CheckWellKnownListValueMessage(const google::protobuf::Message& message) {
  return internal::CheckJsonList(message);
}

}  // namespace common_internal

std::string ParsedJsonListValue::DebugString() const {
  if (value_ == nullptr) {
    return "[]";
  }
  return internal::JsonListDebugString(*value_);
}

absl::Status ParsedJsonListValue::SerializeTo(AnyToJsonConverter& converter,
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

absl::StatusOr<Json> ParsedJsonListValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  if (value_ == nullptr) {
    return JsonArray();
  }
  return internal::ProtoJsonListToNativeJsonList(*value_);
}

absl::Status ParsedJsonListValue::Equal(ValueManager& value_manager,
                                        const Value& other,
                                        Value& result) const {
  if (auto other_value = other.AsParsedJsonList(); other_value) {
    result = BoolValue(*this == *other_value);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedRepeatedField(); other_value) {
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
  if (auto other_value = other.AsList(); other_value) {
    return common_internal::ListValueEqual(value_manager, ListValue(*this),
                                           *other_value, result);
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonListValue::Equal(ValueManager& value_manager,
                                                 const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

ParsedJsonListValue ParsedJsonListValue::Clone(Allocator<> allocator) const {
  if (value_ == nullptr) {
    return ParsedJsonListValue();
  }
  if (value_.arena() == allocator.arena()) {
    return *this;
  }
  auto cloned = WrapShared(value_->New(allocator.arena()), allocator);
  cloned->CopyFrom(*value_);
  return ParsedJsonListValue(std::move(cloned));
}

size_t ParsedJsonListValue::Size() const {
  if (value_ == nullptr) {
    return 0;
  }
  return static_cast<size_t>(
      well_known_types::GetListValueReflectionOrDie(value_->GetDescriptor())
          .ValuesSize(*value_));
}

// See ListValueInterface::Get for documentation.
absl::Status ParsedJsonListValue::Get(ValueManager& value_manager, size_t index,
                                      Value& result) const {
  if (value_ == nullptr) {
    result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(value_->GetDescriptor());
  if (ABSL_PREDICT_FALSE(index >=
                         static_cast<size_t>(reflection.ValuesSize(*value_)))) {
    result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  result = common_internal::ParsedJsonValue(
      value_manager.GetMemoryManager().arena(),
      Borrowed(value_, &reflection.Values(*value_, static_cast<int>(index))));
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonListValue::Get(ValueManager& value_manager,
                                               size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ParsedJsonListValue::ForEach(ValueManager& value_manager,
                                          ForEachCallback callback) const {
  return ForEach(value_manager,
                 [callback = std::move(callback)](size_t, const Value& value)
                     -> absl::StatusOr<bool> { return callback(value); });
}

absl::Status ParsedJsonListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  if (value_ == nullptr) {
    return absl::OkStatus();
  }
  Value scratch;
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(value_->GetDescriptor());
  const int size = reflection.ValuesSize(*value_);
  for (int i = 0; i < size; ++i) {
    scratch = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(),
        Borrowed(value_, &reflection.Values(*value_, i)));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(static_cast<size_t>(i), scratch));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

namespace {

class ParsedJsonListValueIterator final : public ValueIterator {
 public:
  explicit ParsedJsonListValueIterator(Owned<const google::protobuf::Message> message)
      : message_(std::move(message)),
        reflection_(well_known_types::GetListValueReflectionOrDie(
            message_->GetDescriptor())),
        size_(reflection_.ValuesSize(*message_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager& value_manager, Value& result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    result = common_internal::ParsedJsonValue(
        value_manager.GetMemoryManager().arena(),
        Borrowed(message_, &reflection_.Values(*message_, index_)));
    ++index_;
    return absl::OkStatus();
  }

 private:
  const Owned<const google::protobuf::Message> message_;
  const well_known_types::ListValueReflection reflection_;
  const int size_;
  int index_ = 0;
};

}  // namespace

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedJsonListValue::NewIterator(ValueManager& value_manager) const {
  if (value_ == nullptr) {
    return NewEmptyValueIterator();
  }
  return std::make_unique<ParsedJsonListValueIterator>(value_);
}

namespace {

absl::optional<internal::Number> AsNumber(const Value& value) {
  if (auto int_value = value.AsInt(); int_value) {
    return internal::Number::FromInt64(*int_value);
  }
  if (auto uint_value = value.AsUint(); uint_value) {
    return internal::Number::FromUint64(*uint_value);
  }
  if (auto double_value = value.AsDouble(); double_value) {
    return internal::Number::FromDouble(*double_value);
  }
  return absl::nullopt;
}

}  // namespace

absl::Status ParsedJsonListValue::Contains(ValueManager& value_manager,
                                           const Value& other,
                                           Value& result) const {
  if (value_ == nullptr) {
    result = BoolValue(false);
    return absl::OkStatus();
  }
  if (ABSL_PREDICT_FALSE(other.IsError() || other.IsUnknown())) {
    result = other;
    return absl::OkStatus();
  }
  // Other must be comparable to `null`, `double`, `string`, `list`, or `map`.
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(value_->GetDescriptor());
  if (reflection.ValuesSize(*value_) > 0) {
    const auto value_reflection = well_known_types::GetValueReflectionOrDie(
        reflection.GetValueDescriptor());
    if (other.IsNull()) {
      for (const auto& element : reflection.Values(*value_)) {
        const auto element_kind_case = value_reflection.GetKindCase(element);
        if (element_kind_case == google::protobuf::Value::KIND_NOT_SET ||
            element_kind_case == google::protobuf::Value::kNullValue) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsBool(); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kBoolValue &&
            value_reflection.GetBoolValue(element) == *other_value) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = AsNumber(other); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kNumberValue &&
            internal::Number::FromDouble(
                value_reflection.GetNumberValue(element)) == *other_value) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsString(); other_value) {
      std::string scratch;
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kStringValue &&
            absl::visit(
                [&](const auto& alternative) -> bool {
                  return *other_value == alternative;
                },
                well_known_types::AsVariant(
                    value_reflection.GetStringValue(element, scratch)))) {
          result = BoolValue(true);
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsList(); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
            google::protobuf::Value::kListValue) {
          CEL_RETURN_IF_ERROR(other_value->Equal(
              value_manager,
              ParsedJsonListValue(Owned(
                  Owner(value_), &value_reflection.GetListValue(element))),
              result));
          if (result.IsTrue()) {
            return absl::OkStatus();
          }
        }
      }
    } else if (const auto other_value = other.AsMap(); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
            google::protobuf::Value::kStructValue) {
          CEL_RETURN_IF_ERROR(other_value->Equal(
              value_manager,
              ParsedJsonMapValue(Owned(
                  Owner(value_), &value_reflection.GetStructValue(element))),
              result));
          if (result.IsTrue()) {
            return absl::OkStatus();
          }
        }
      }
    }
  }
  result = BoolValue(false);
  return absl::OkStatus();
}

absl::StatusOr<Value> ParsedJsonListValue::Contains(ValueManager& value_manager,
                                                    const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

bool operator==(const ParsedJsonListValue& lhs,
                const ParsedJsonListValue& rhs) {
  if (cel::to_address(lhs.value_) == cel::to_address(rhs.value_)) {
    return true;
  }
  if (cel::to_address(lhs.value_) == nullptr) {
    return rhs.IsEmpty();
  }
  if (cel::to_address(rhs.value_) == nullptr) {
    return lhs.IsEmpty();
  }
  return internal::JsonListEquals(*lhs.value_, *rhs.value_);
}

}  // namespace cel
