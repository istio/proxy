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
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "common/json.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "common/values/parsed_message_value.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"

namespace cel {

absl::Nonnull<const google::protobuf::Descriptor*> MessageValue::GetDescriptor() const {
  ABSL_CHECK(*this);  // Crash OK
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Nonnull<const google::protobuf::Descriptor*> {
            ABSL_UNREACHABLE();
          },
          [](const ParsedMessageValue& alternative)
              -> absl::Nonnull<const google::protobuf::Descriptor*> {
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

absl::Status MessageValue::SerializeTo(AnyToJsonConverter& converter,
                                       absl::Cord& value) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `ConvertToJson` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.SerializeTo(converter, value);
          }),
      variant_);
}

absl::StatusOr<Json> MessageValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<Json> {
            return absl::InternalError(
                "unexpected attempt to invoke `ConvertToJson` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<Json> {
            return alternative.ConvertToJson(converter);
          }),
      variant_);
}

absl::Status MessageValue::Equal(ValueManager& value_manager,
                                 const Value& other, Value& result) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `Equal` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.Equal(value_manager, other, result);
          }),
      variant_);
}

absl::StatusOr<Value> MessageValue::Equal(ValueManager& value_manager,
                                          const Value& other) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<Value> {
            return absl::InternalError(
                "unexpected attempt to invoke `Equal` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<Value> {
            return alternative.Equal(value_manager, other);
          }),
      variant_);
}

absl::Status MessageValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `GetFieldByName` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.GetFieldByName(value_manager, name, result,
                                              unboxing_options);
          }),
      variant_);
}

absl::StatusOr<Value> MessageValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<Value> {
            return absl::InternalError(
                "unexpected attempt to invoke `GetFieldByName` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<Value> {
            return alternative.GetFieldByName(value_manager, name,
                                              unboxing_options);
          }),
      variant_);
}

absl::Status MessageValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `GetFieldByNumber` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.GetFieldByNumber(value_manager, number, result,
                                                unboxing_options);
          }),
      variant_);
}

absl::StatusOr<Value> MessageValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<Value> {
            return absl::InternalError(
                "unexpected attempt to invoke `GetFieldByNumber` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<Value> {
            return alternative.GetFieldByNumber(value_manager, number,
                                                unboxing_options);
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

absl::Status MessageValue::ForEachField(ValueManager& value_manager,
                                        ForEachFieldCallback callback) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::Status {
            return absl::InternalError(
                "unexpected attempt to invoke `ForEachField` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::Status {
            return alternative.ForEachField(value_manager, callback);
          }),
      variant_);
}

absl::StatusOr<int> MessageValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<int> {
            return absl::InternalError(
                "unexpected attempt to invoke `Qualify` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative) -> absl::StatusOr<int> {
            return alternative.Qualify(value_manager, qualifiers, presence_test,
                                       result);
          }),
      variant_);
}

absl::StatusOr<std::pair<Value, int>> MessageValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  return absl::visit(
      absl::Overload(
          [](absl::monostate) -> absl::StatusOr<std::pair<Value, int>> {
            return absl::InternalError(
                "unexpected attempt to invoke `Qualify` on "
                "an invalid `MessageValue`");
          },
          [&](const ParsedMessageValue& alternative)
              -> absl::StatusOr<std::pair<Value, int>> {
            return alternative.Qualify(value_manager, qualifiers,
                                       presence_test);
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
  return absl::get<ParsedMessageValue>(variant_);
}

common_internal::ValueVariant MessageValue::ToValueVariant() && {
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

common_internal::StructValueVariant MessageValue::ToStructValueVariant()
    const& {
  return absl::get<ParsedMessageValue>(variant_);
}

common_internal::StructValueVariant MessageValue::ToStructValueVariant() && {
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

}  // namespace cel
