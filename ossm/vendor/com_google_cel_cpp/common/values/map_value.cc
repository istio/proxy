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
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/value_variant.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

}  // namespace

NativeTypeId MapValue::GetTypeId() const {
  return variant_.Visit([](const auto& alternative) -> NativeTypeId {
    return NativeTypeId::Of(alternative);
  });
}

std::string MapValue::DebugString() const {
  return variant_.Visit([](const auto& alternative) -> std::string {
    return alternative.DebugString();
  });
}

absl::Status MapValue::SerializeTo(
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

absl::Status MapValue::ConvertToJson(
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

absl::Status MapValue::ConvertToJsonObject(
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

absl::Status MapValue::Equal(
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

bool MapValue::IsZeroValue() const {
  return variant_.Visit([](const auto& alternative) -> bool {
    return alternative.IsZeroValue();
  });
}

absl::StatusOr<bool> MapValue::IsEmpty() const {
  return variant_.Visit([](const auto& alternative) -> absl::StatusOr<bool> {
    return alternative.IsEmpty();
  });
}

absl::StatusOr<size_t> MapValue::Size() const {
  return variant_.Visit([](const auto& alternative) -> absl::StatusOr<size_t> {
    return alternative.Size();
  });
}

absl::Status MapValue::Get(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.Get(key, descriptor_pool, message_factory, arena,
                           result);
  });
}

absl::StatusOr<bool> MapValue::Find(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::StatusOr<bool> {
    return alternative.Find(key, descriptor_pool, message_factory, arena,
                            result);
  });
}

absl::Status MapValue::Has(
    const Value& key,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.Has(key, descriptor_pool, message_factory, arena,
                           result);
  });
}

absl::Status MapValue::ListKeys(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, ListValue* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.ListKeys(descriptor_pool, message_factory, arena,
                                result);
  });
}

absl::Status MapValue::ForEach(
    ForEachCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.ForEach(callback, descriptor_pool, message_factory,
                               arena);
  });
}

absl::StatusOr<absl_nonnull ValueIteratorPtr> MapValue::NewIterator() const {
  return variant_.Visit([](const auto& alternative)
                            -> absl::StatusOr<absl_nonnull ValueIteratorPtr> {
    return alternative.NewIterator();
  });
}

namespace common_internal {

absl::Status MapValueEqual(
    const MapValue& lhs, const MapValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  CEL_ASSIGN_OR_RETURN(auto lhs_size, lhs.Size());
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator());
  Value lhs_key;
  Value lhs_value;
  Value rhs_value;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(
        lhs_iterator->Next(descriptor_pool, message_factory, arena, &lhs_key));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(
        rhs_value_found,
        rhs.Find(lhs_key, descriptor_pool, message_factory, arena, &rhs_value));
    if (!rhs_value_found) {
      *result = FalseValue();
      return absl::OkStatus();
    }
    CEL_RETURN_IF_ERROR(
        lhs.Get(lhs_key, descriptor_pool, message_factory, arena, &lhs_value));
    CEL_RETURN_IF_ERROR(lhs_value.Equal(rhs_value, descriptor_pool,
                                        message_factory, arena, result));
    if (result->IsFalse()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  *result = TrueValue();
  return absl::OkStatus();
}

absl::Status MapValueEqual(
    const CustomMapValueInterface& lhs, const MapValue& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  auto lhs_size = lhs.Size();
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator());
  Value lhs_key;
  Value lhs_value;
  Value rhs_value;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(
        lhs_iterator->Next(descriptor_pool, message_factory, arena, &lhs_key));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(
        rhs_value_found,
        rhs.Find(lhs_key, descriptor_pool, message_factory, arena, &rhs_value));
    if (!rhs_value_found) {
      *result = FalseValue();
      return absl::OkStatus();
    }
    CEL_RETURN_IF_ERROR(
        CustomMapValue(&lhs, arena)
            .Get(lhs_key, descriptor_pool, message_factory, arena, &lhs_value));
    CEL_RETURN_IF_ERROR(lhs_value.Equal(rhs_value, descriptor_pool,
                                        message_factory, arena, result));
    if (result->IsFalse()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  *result = TrueValue();
  return absl::OkStatus();
}

}  // namespace common_internal

absl::Status CheckMapKey(const Value& key) {
  switch (key.kind()) {
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      return absl::OkStatus();
    case ValueKind::kError:
      return key.GetError().NativeValue();
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
}

optional_ref<const CustomMapValue> MapValue::AsCustom() const& {
  if (const auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<CustomMapValue> MapValue::AsCustom() && {
  if (auto* alternative = variant_.As<CustomMapValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

const CustomMapValue& MapValue::GetCustom() const& {
  ABSL_DCHECK(IsCustom());

  return variant_.Get<CustomMapValue>();
}

CustomMapValue MapValue::GetCustom() && {
  ABSL_DCHECK(IsCustom());

  return std::move(variant_).Get<CustomMapValue>();
}

common_internal::ValueVariant MapValue::ToValueVariant() const& {
  return variant_.Visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        return common_internal::ValueVariant(alternative);
      });
}

common_internal::ValueVariant MapValue::ToValueVariant() && {
  return std::move(variant_).Visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        // NOLINTNEXTLINE(bugprone-move-forwarding-reference)
        return common_internal::ValueVariant(std::move(alternative));
      });
}

}  // namespace cel
