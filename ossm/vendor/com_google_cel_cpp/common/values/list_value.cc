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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "common/values/value_variant.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

NativeTypeId ListValue::GetTypeId() const {
  return variant_.Visit([](const auto& alternative) -> NativeTypeId {
    return NativeTypeId::Of(alternative);
  });
}

std::string ListValue::DebugString() const {
  return variant_.Visit([](const auto& alternative) -> std::string {
    return alternative.DebugString();
  });
}

absl::Status ListValue::SerializeTo(
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

absl::Status ListValue::ConvertToJson(
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

absl::Status ListValue::ConvertToJsonArray(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(json != nullptr);
  ABSL_DCHECK_EQ(json->GetDescriptor()->well_known_type(),
                 google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.ConvertToJsonArray(descriptor_pool, message_factory,
                                          json);
  });
}

absl::Status ListValue::Equal(
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

bool ListValue::IsZeroValue() const {
  return variant_.Visit([](const auto& alternative) -> bool {
    return alternative.IsZeroValue();
  });
}

absl::StatusOr<bool> ListValue::IsEmpty() const {
  return variant_.Visit([](const auto& alternative) -> absl::StatusOr<bool> {
    return alternative.IsEmpty();
  });
}

absl::StatusOr<size_t> ListValue::Size() const {
  return variant_.Visit([](const auto& alternative) -> absl::StatusOr<size_t> {
    return alternative.Size();
  });
}

absl::Status ListValue::Get(
    size_t index, const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.Get(index, descriptor_pool, message_factory, arena,
                           result);
  });
}

absl::Status ListValue::ForEach(
    ForEachWithIndexCallback callback,
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

absl::StatusOr<absl_nonnull ValueIteratorPtr> ListValue::NewIterator() const {
  return variant_.Visit([](const auto& alternative)
                            -> absl::StatusOr<absl_nonnull ValueIteratorPtr> {
    return alternative.NewIterator();
  });
}

absl::Status ListValue::Contains(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  return variant_.Visit([&](const auto& alternative) -> absl::Status {
    return alternative.Contains(other, descriptor_pool, message_factory, arena,
                                result);
  });
}

namespace common_internal {

absl::Status ListValueEqual(
    const ListValue& lhs, const ListValue& rhs,
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
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator());
  Value lhs_element;
  Value rhs_element;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(lhs_iterator->Next(descriptor_pool, message_factory,
                                           arena, &lhs_element));
    CEL_RETURN_IF_ERROR(rhs_iterator->Next(descriptor_pool, message_factory,
                                           arena, &rhs_element));
    CEL_RETURN_IF_ERROR(lhs_element.Equal(rhs_element, descriptor_pool,
                                          message_factory, arena, result));
    if (result->IsFalse()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  *result = TrueValue();
  return absl::OkStatus();
}

absl::Status ListValueEqual(
    const CustomListValueInterface& lhs, const ListValue& rhs,
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
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator());
  Value lhs_element;
  Value rhs_element;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(lhs_iterator->Next(descriptor_pool, message_factory,
                                           arena, &lhs_element));
    CEL_RETURN_IF_ERROR(rhs_iterator->Next(descriptor_pool, message_factory,
                                           arena, &rhs_element));
    CEL_RETURN_IF_ERROR(lhs_element.Equal(rhs_element, descriptor_pool,
                                          message_factory, arena, result));
    if (result->IsFalse()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  *result = TrueValue();
  return absl::OkStatus();
}

}  // namespace common_internal

optional_ref<const CustomListValue> ListValue::AsCustom() const& {
  if (const auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<CustomListValue> ListValue::AsCustom() && {
  if (auto* alternative = variant_.As<CustomListValue>();
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

const CustomListValue& ListValue::GetCustom() const& {
  ABSL_DCHECK(IsCustom());

  return variant_.Get<CustomListValue>();
}

CustomListValue ListValue::GetCustom() && {
  ABSL_DCHECK(IsCustom());

  return std::move(variant_).Get<CustomListValue>();
}

common_internal::ValueVariant ListValue::ToValueVariant() const& {
  return variant_.Visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        return common_internal::ValueVariant(alternative);
      });
}

common_internal::ValueVariant ListValue::ToValueVariant() && {
  return std::move(variant_).Visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        // NOLINTNEXTLINE(bugprone-move-forwarding-reference)
        return common_internal::ValueVariant(std::move(alternative));
      });
}

}  // namespace cel
