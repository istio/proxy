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

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel {

StructType StructValue::GetRuntimeType() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> StructType {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          ABSL_UNREACHABLE();
        } else {
          return alternative.GetRuntimeType();
        }
      },
      variant_);
}

absl::string_view StructValue::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::string_view{};
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string StructValue::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> std::string {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return std::string{};
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::Status StructValue::SerializeTo(AnyToJsonConverter& converter,
                                      absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&converter, &value](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.SerializeTo(converter, value);
        }
      },
      variant_);
}

absl::StatusOr<Json> StructValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  AssertIsValid();
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<Json> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ConvertToJson(converter);
        }
      },
      variant_);
}

bool StructValue::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> bool {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

absl::StatusOr<bool> StructValue::HasFieldByName(absl::string_view name) const {
  AssertIsValid();
  return absl::visit(
      [name](const auto& alternative) -> absl::StatusOr<bool> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.HasFieldByName(name);
        }
      },
      variant_);
}

absl::StatusOr<bool> StructValue::HasFieldByNumber(int64_t number) const {
  AssertIsValid();
  return absl::visit(
      [number](const auto& alternative) -> absl::StatusOr<bool> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.HasFieldByNumber(number);
        }
      },
      variant_);
}

namespace common_internal {

absl::Status StructValueEqual(ValueManager& value_manager,
                              const StructValue& lhs, const StructValue& rhs,
                              Value& result) {
  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      value_manager,
      [&lhs_fields](absl::string_view name,
                    const Value& lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      }));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      value_manager,
      [&value_manager, &result, &lhs_fields, &equal, &rhs_fields_count](
          absl::string_view name,
          const Value& rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_RETURN_IF_ERROR(
            lhs_field->second.Equal(value_manager, rhs_value, result));
        if (auto bool_value = As<BoolValue>(result);
            bool_value.has_value() && !bool_value->NativeValue()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      }));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  result = BoolValue{true};
  return absl::OkStatus();
}

absl::Status StructValueEqual(ValueManager& value_manager,
                              const ParsedStructValueInterface& lhs,
                              const StructValue& rhs, Value& result) {
  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      value_manager,
      [&lhs_fields](absl::string_view name,
                    const Value& lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      }));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      value_manager,
      [&value_manager, &result, &lhs_fields, &equal, &rhs_fields_count](
          absl::string_view name,
          const Value& rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_RETURN_IF_ERROR(
            lhs_field->second.Equal(value_manager, rhs_value, result));
        if (auto bool_value = As<BoolValue>(result);
            bool_value.has_value() && !bool_value->NativeValue()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      }));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  result = BoolValue{true};
  return absl::OkStatus();
}

}  // namespace common_internal

absl::optional<MessageValue> StructValue::AsMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MessageValue> StructValue::AsMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMessageValue> StructValue::AsParsedMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> StructValue::AsParsedMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

MessageValue StructValue::GetMessage() const& {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

MessageValue StructValue::GetMessage() && {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

const ParsedMessageValue& StructValue::GetParsedMessage() const& {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

ParsedMessageValue StructValue::GetParsedMessage() && {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

common_internal::ValueVariant StructValue::ToValueVariant() const& {
  return absl::visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        return alternative;
      },
      variant_);
}

common_internal::ValueVariant StructValue::ToValueVariant() && {
  return absl::visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        return std::move(alternative);
      },
      std::move(variant_));
}

}  // namespace cel
