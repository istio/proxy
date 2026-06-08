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

#include "common/values/message_value.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "common/values/parsed_message_value.h"
#include "common/values/value_variant.h"
#include "common/values/values.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

const google::protobuf::Descriptor* absl_nonnull MessageValue::GetDescriptor() const {
  ABSL_CHECK(*this);  // Crash OK
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> const google::protobuf::Descriptor* absl_nonnull {
            ABSL_UNREACHABLE();
          },
          [](const ParsedMessageValue& alternative)
              -> const google::protobuf::Descriptor* absl_nonnull {
            return alternative.GetDescriptor();
          }),
      variant_);
}

std::string MessageValue::DebugString() const {
  return absl::visit(
      absl::Overload([](absl::monostate) -> std::string { return "INVALID"; },
                     [](const ParsedMessageValue& alternative) -> std::string {
                       return alternative.DebugString();
                     }),
      variant_);
}

bool MessageValue::IsZeroValue() const {
  ABSL_DCHECK(*this);
  return absl::visit(
      absl::Overload([](absl::monostate) -> bool { return true; },
                     [](const ParsedMessageValue& alternative) -> bool {
                       return alternative.IsZeroValue();
                     }),
      variant_);
}

absl::Status MessageValue::SerializeTo(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `ConvertToJson` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.SerializeTo(descriptor_pool, message_factory,
                                           output);
          }),
      variant_);
}

absl::Status MessageValue::ConvertToJson(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `ConvertToJson` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.ConvertToJson(descriptor_pool, message_factory,
                                             json);
          }),
      variant_);
}

absl::Status MessageValue::ConvertToJsonObject(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Message* absl_nonnull json) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `ConvertToJsonObject` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.ConvertToJsonObject(descriptor_pool,
                                                   message_factory, json);
          }),
      variant_);
}

absl::Status MessageValue::Equal(
    const Value& other,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `Equal` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.Equal(other, descriptor_pool, message_factory,
                                     arena, result);
          }),
      variant_);
}

absl::Status MessageValue::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `GetFieldByName` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.GetFieldByName(name, unboxing_options,
                                              descriptor_pool, message_factory,
                                              arena, result);
          }),
      variant_);
}

absl::Status MessageValue::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `GetFieldByNumber` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.GetFieldByNumber(number, unboxing_options,
                                                descriptor_pool,
                                                message_factory, arena, result);
          }),
      variant_);
}

absl::StatusOr<bool> MessageValue::HasFieldByName(
    absl::string_view name) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<bool> {
            return absl::InternalError(
                "unexpected attempt to invoke `HasFieldByName` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<bool> {
            return alternative.HasFieldByName(name);
          }),
      variant_);
}

absl::StatusOr<bool> MessageValue::HasFieldByNumber(int64_t number) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<bool> {
            return absl::InternalError(
                "unexpected attempt to invoke `HasFieldByNumber` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<bool> {
            return alternative.HasFieldByNumber(number);
          }),
      variant_);
}

absl::Status MessageValue::ForEachField(
    ForEachFieldCallback callback,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `ForEachField` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.ForEachField(callback, descriptor_pool,
                                            message_factory, arena);
          }),
      variant_);
}

absl::Status MessageValue::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena, Value* absl_nonnull result,
    int* absl_nonnull count) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `Qualify` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.Qualify(qualifiers, presence_test,
                                       descriptor_pool, message_factory, arena,
                                       result, count);
          }),
      variant_);
}

cel::optional_ref<const ParsedMessageValue> MessageValue::AsParsed() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> MessageValue::AsParsed() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

const ParsedMessageValue& MessageValue::GetParsed() const& {
  ABSL_DCHECK(IsParsed());
  return absl::get<ParsedMessageValue>(variant_);
}

ParsedMessageValue MessageValue::GetParsed() && {
  ABSL_DCHECK(IsParsed());
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

common_internal::ValueVariant MessageValue::ToValueVariant() const& {
  return common_internal::ValueVariant(absl::get<ParsedMessageValue>(variant_));
}

common_internal::ValueVariant MessageValue::ToValueVariant() && {
  return common_internal::ValueVariant(
      absl::get<ParsedMessageValue>(std::move(variant_)));
}

common_internal::StructValueVariant MessageValue::ToStructValueVariant()
    const& {
  return common_internal::StructValueVariant(
      absl::get<ParsedMessageValue>(variant_));
}

common_internal::StructValueVariant MessageValue::ToStructValueVariant() && {
  return common_internal::StructValueVariant(
      absl::get<ParsedMessageValue>(std::move(variant_)));
}

}  // namespace cel
