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

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/values/parsed_json_value.h"
#include "common/values/values.h"
#include "internal/json.h"
#include "internal/message_equality.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

using ::cel::well_known_types::ValueReflection;

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

absl::Status ParsedJsonListValue::SerializeTo(
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
        "failed to serialize message: google.protobuf.ListValue");
  }
  return absl::OkStatus();
}

absl::Status ParsedJsonListValue::ConvertToJson(
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
  auto* message = value_reflection.MutableListValue(json);
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

absl::Status ParsedJsonListValue::ConvertToJsonArray(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

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

absl::Status ParsedJsonListValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (auto other_value = other.AsParsedJsonList(); other_value) {
    *result = BoolValue(*this == *other_value);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsParsedRepeatedField(); other_value) {
    if (value_ == nullptr) {
      *result = BoolValue(other_value->IsEmpty());
      return absl::OkStatus();
    }
    CEL_ASSIGN_OR_RETURN(
        auto equal, internal::MessageFieldEquals(
                        *value_, *other_value->message_, other_value->field_,
                        descriptor_pool, message_factory));
    *result = BoolValue(equal);
    return absl::OkStatus();
  }
  if (auto other_value = other.AsList(); other_value) {
    return common_internal::ListValueEqual(ListValue(*this), *other_value,
                                           descriptor_pool, message_factory,
                                           arena, result);
  }
  *result = BoolValue(false);
  return absl::OkStatus();
}

ParsedJsonListValue ParsedJsonListValue::Clone(
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(arena != nullptr);

  if (value_ == nullptr) {
    return ParsedJsonListValue();
  }
  if (arena_ == arena) {
    return *this;
  }
  auto* cloned = value_->New(arena);
  cloned->CopyFrom(*value_);
  return ParsedJsonListValue(cloned, arena);
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
absl::Status ParsedJsonListValue::Get(
    size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (value_ == nullptr) {
    *result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(value_->GetDescriptor());
  if (ABSL_PREDICT_FALSE(index >=
                         static_cast<size_t>(reflection.ValuesSize(*value_)))) {
    *result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  *result = common_internal::ParsedJsonValue(
      &reflection.Values(*value_, static_cast<int>(index)), arena);
  return absl::OkStatus();
}

absl::Status ParsedJsonListValue::ForEach(
    ForEachWithIndexCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  if (value_ == nullptr) {
    return absl::OkStatus();
  }
  Value scratch;
  const auto reflection =
      well_known_types::GetListValueReflectionOrDie(value_->GetDescriptor());
  const int size = reflection.ValuesSize(*value_);
  for (int i = 0; i < size; ++i) {
    scratch =
        common_internal::ParsedJsonValue(&reflection.Values(*value_, i), arena);
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
  explicit ParsedJsonListValueIterator(
      const google::protobuf::Message* absl_nonnull message)
      : message_(message),
        reflection_(well_known_types::GetListValueReflectionOrDie(
            message_->GetDescriptor())),
        size_(reflection_.ValuesSize(*message_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) override {
    ABSL_DCHECK(descriptor_pool != nullptr);
    ABSL_DCHECK(message_factory != nullptr);
    ABSL_DCHECK(arena != nullptr);
    ABSL_DCHECK(result != nullptr);

    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "`ValueIterator::Next` called after `ValueIterator::HasNext` "
          "returned false");
    }
    *result = common_internal::ParsedJsonValue(
        &reflection_.Values(*message_, index_), arena);
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
    *key_or_value = common_internal::ParsedJsonValue(
        &reflection_.Values(*message_, index_), arena);
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
      *value = common_internal::ParsedJsonValue(
          &reflection_.Values(*message_, index_), arena);
    }
    *key = IntValue(index_);
    ++index_;
    return true;
  }

 private:
  const google::protobuf::Message* absl_nonnull const message_;
  const well_known_types::ListValueReflection reflection_;
  const int size_;
  int index_ = 0;
};

}  // namespace

absl::StatusOr<absl_nonnull std::unique_ptr<ValueIterator>>
ParsedJsonListValue::NewIterator() const {
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

absl::Status ParsedJsonListValue::Contains(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (value_ == nullptr) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  if (ABSL_PREDICT_FALSE(other.IsError() || other.IsUnknown())) {
    *result = other;
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
          *result = TrueValue();
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsBool(); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kBoolValue &&
            value_reflection.GetBoolValue(element) == *other_value) {
          *result = TrueValue();
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = AsNumber(other); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
                google::protobuf::Value::kNumberValue &&
            internal::Number::FromDouble(
                value_reflection.GetNumberValue(element)) == *other_value) {
          *result = TrueValue();
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
          *result = TrueValue();
          return absl::OkStatus();
        }
      }
    } else if (const auto other_value = other.AsList(); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
            google::protobuf::Value::kListValue) {
          CEL_RETURN_IF_ERROR(other_value->Equal(
              ParsedJsonListValue(&value_reflection.GetListValue(element),
                                  arena),
              descriptor_pool, message_factory, arena, result));
          if (result->IsTrue()) {
            return absl::OkStatus();
          }
        }
      }
    } else if (const auto other_value = other.AsMap(); other_value) {
      for (const auto& element : reflection.Values(*value_)) {
        if (value_reflection.GetKindCase(element) ==
            google::protobuf::Value::kStructValue) {
          CEL_RETURN_IF_ERROR(other_value->Equal(
              ParsedJsonMapValue(&value_reflection.GetStructValue(element),
                                 arena),
              descriptor_pool, message_factory, arena, result));
          if (result->IsTrue()) {
            return absl::OkStatus();
          }
        }
      }
    }
  }
  *result = FalseValue();
  return absl::OkStatus();
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
