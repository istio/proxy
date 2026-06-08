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
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value.h"
#include "common/values/value_variant.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

StructType StructValue::GetRuntimeType() const {
  return variant_.Visit([](const auto& alternative) -> StructType {
    return alternative.GetRuntimeType();
  });
}

absl::string_view StructValue::GetTypeName() const {
  return variant_.Visit([](const auto& alternative) -> absl::string_view {
    return alternative.GetTypeName();
  });
}

NativeTypeId StructValue::GetTypeId() const {
  return variant_.Visit([](const auto& alternative) -> NativeTypeId {
    return NativeTypeId::Of(alternative);
  });
}

std::string StructValue::DebugString() const {
  return variant_.Visit([](const auto& alternative) -> std::string {
    return alternative.DebugString();
  });
}

absl::Status StructValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(output != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.SerializeTo(descriptor_pool, message_factory, output);
  });
}

absl::Status StructValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.ConvertToJson(descriptor_pool, message_factory, json);
  });
}

absl::Status StructValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                           json);
  });
}

absl::Status StructValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.Equal(other, descriptor_pool, message_factory, arena,
                             result);
  });
}

bool StructValue::IsZeroValue() const {
  return variant_.Visit([](const auto& alternative) -> bool {
    return alternative.IsZeroValue();
  });
}

absl::StatusOr<bool> StructValue::HasFieldByName(absl::string_view name) const {
  return variant_.Visit(
      [name](const auto& alternative) -> absl::StatusOr<bool> {
        return alternative.HasFieldByName(name);
      });
}

absl::StatusOr<bool> StructValue::HasFieldByNumber(int64_t number) const {
  return variant_.Visit(
      [number](const auto& alternative) -> absl::StatusOr<bool> {
        return alternative.HasFieldByNumber(number);
      });
}

absl::Status StructValue::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.GetFieldByName(name, unboxing_options, descriptor_pool,
                                      message_factory, arena, result);
  });
}

absl::Status StructValue::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.GetFieldByNumber(number, unboxing_options,
                                        descriptor_pool, message_factory, arena,
                                        result);
  });
}

absl::Status StructValue::ForEachField(
    ForEachFieldCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.ForEachField(callback, descriptor_pool, message_factory,
                                    arena);
  });
}

absl::Status StructValue::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
    int* absl_nonnull count) const {
  ABSL_DCHECK(!qualifiers.empty());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  ABSL_DCHECK(count != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.Qualify(qualifiers, presence_test, descriptor_pool,
                               message_factory, arena, result, count);
  });
}

namespace common_internal {

absl::Status StructValueEqual(
    const StructValue& lhs, const StructValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      [&lhs_fields](absl::string_view name,
                    const Value& lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      },
      descriptor_pool, message_factory, arena));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      [&](absl::string_view name,
          const Value& rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_RETURN_IF_ERROR(lhs_field->second.Equal(
            rhs_value, descriptor_pool, message_factory, arena, result));
        if (result->IsFalse()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      },
      descriptor_pool, message_factory, arena));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  *result = TrueValue();
  return absl::OkStatus();
}

absl::Status StructValueEqual(
    const CustomStructValueInterface& lhs, const StructValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      [&lhs_fields](absl::string_view name,
                    const Value& lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      },
      descriptor_pool, message_factory, arena));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      [&](absl::string_view name,
          const Value& rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_RETURN_IF_ERROR(lhs_field->second.Equal(
            rhs_value, descriptor_pool, message_factory, arena, result));
        if (result->IsFalse()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      },
      descriptor_pool, message_factory, arena));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  *result = TrueValue();
  return absl::OkStatus();
}

}  // namespace common_internal

absl::optional<MessageValue> StructValue::AsMessage() const& {
  if (const auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MessageValue> StructValue::AsMessage() && {
  if (auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMessageValue> StructValue::AsParsedMessage() const& {
  if (const auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> StructValue::AsParsedMessage() && {
  if (auto* alternative = variant_.As<ParsedMessageValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

MessageValue StructValue::GetMessage() const& {
  ABSL_DCHECK(IsMessage()) << *this;

  return variant_.Get<ParsedMessageValue>();
}

MessageValue StructValue::GetMessage() && {
  ABSL_DCHECK(IsMessage()) << *this;

  return std::move(variant_).Get<ParsedMessageValue>();
}

const ParsedMessageValue& StructValue::GetParsedMessage() const& {
  ABSL_DCHECK(IsParsedMessage()) << *this;

  return variant_.Get<ParsedMessageValue>();
}

ParsedMessageValue StructValue::GetParsedMessage() && {
  ABSL_DCHECK(IsParsedMessage()) << *this;

  return std::move(variant_).Get<ParsedMessageValue>();
}

common_internal::ValueVariant StructValue::ToValueVariant() const& {
  return variant_.Visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        return common_internal::ValueVariant(alternative);
      });
}

common_internal::ValueVariant StructValue::ToValueVariant() && {
  return std::move(variant_).Visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        // NOLINTNEXTLINE(bugprone-move-forwarding-reference)
        return common_internal::ValueVariant(std::move(alternative));
      });
}

}  // namespace cel
