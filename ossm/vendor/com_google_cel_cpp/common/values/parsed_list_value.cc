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
#include <memory>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/values/list_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace {

using ::google::api::expr::runtime::CelValue;

class EmptyListValue final : public common_internal::CompatListValue {
 public:
  static const EmptyListValue& Get() {
    static const absl::NoDestructor<EmptyListValue> empty;
    return *empty;
  }

  EmptyListValue() = default;

  std::string DebugString() const override { return "[]"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter&) const override {
    return JsonArray();
  }

  ParsedListValue Clone(ArenaAllocator<>) const override {
    return ParsedListValue();
  }

  int size() const override { return 0; }

  CelValue operator[](int index) const override {
    static const absl::NoDestructor<absl::Status> error(
        absl::InvalidArgumentError("index out of bounds"));
    return CelValue::CreateError(&*error);
  }

  CelValue Get(google::protobuf::Arena* arena, int index) const override {
    if (arena == nullptr) {
      return (*this)[index];
    }
    return CelValue::CreateError(google::protobuf::Arena::Create<absl::Status>(
        arena, absl::InvalidArgumentError("index out of bounds")));
  }

 private:
  absl::Status GetImpl(ValueManager&, size_t, Value&) const override {
    // Not reachable, `Get` performs index checking.
    return absl::InternalError("unreachable");
  }
};

}  // namespace

namespace common_internal {

absl::Nonnull<const CompatListValue*> EmptyCompatListValue() {
  return &EmptyListValue::Get();
}

}  // namespace common_internal

class ParsedListValueInterfaceIterator final : public ValueIterator {
 public:
  explicit ParsedListValueInterfaceIterator(
      const ParsedListValueInterface& interface, ValueManager& value_manager)
      : interface_(interface),
        value_manager_(value_manager),
        size_(interface_.Size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::Status Next(ValueManager&, Value& result) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ValueIterator::Next() called when "
          "ValueIterator::HasNext() returns false");
    }
    return interface_.GetImpl(value_manager_, index_++, result);
  }

 private:
  const ParsedListValueInterface& interface_;
  ValueManager& value_manager_;
  const size_t size_;
  size_t index_ = 0;
};

absl::Status ParsedListValueInterface::SerializeTo(
    AnyToJsonConverter& converter, absl::Cord& value) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJsonArray(converter));
  return internal::SerializeListValue(json, value);
}

absl::Status ParsedListValueInterface::Get(ValueManager& value_manager,
                                           size_t index, Value& result) const {
  if (ABSL_PREDICT_FALSE(index >= Size())) {
    result = IndexOutOfBoundsError(index);
    return absl::OkStatus();
  }
  return GetImpl(value_manager, index, result);
}

absl::Status ParsedListValueInterface::ForEach(ValueManager& value_manager,
                                               ForEachCallback callback) const {
  return ForEach(
      value_manager,
      [callback](size_t, const Value& value) -> absl::StatusOr<bool> {
        return callback(value);
      });
}

absl::Status ParsedListValueInterface::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  const size_t size = Size();
  for (size_t index = 0; index < size; ++index) {
    Value element;
    CEL_RETURN_IF_ERROR(GetImpl(value_manager, index, element));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(index, element));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValueInterface::NewIterator(ValueManager& value_manager) const {
  return std::make_unique<ParsedListValueInterfaceIterator>(*this,
                                                            value_manager);
}

absl::Status ParsedListValueInterface::Equal(ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) const {
  if (auto list_value = other.As<ListValue>(); list_value.has_value()) {
    return ListValueEqual(value_manager, *this, *list_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

absl::Status ParsedListValueInterface::Contains(ValueManager& value_manager,
                                                const Value& other,
                                                Value& result) const {
  Value outcome = BoolValue(false);
  Value equal;
  CEL_RETURN_IF_ERROR(
      ForEach(value_manager,
              [&value_manager, other, &outcome,
               &equal](const Value& element) -> absl::StatusOr<bool> {
                CEL_RETURN_IF_ERROR(element.Equal(value_manager, other, equal));
                if (auto bool_result = As<BoolValue>(equal);
                    bool_result.has_value() && bool_result->NativeValue()) {
                  outcome = BoolValue(true);
                  return false;
                }
                return true;
              }));
  result = outcome;
  return absl::OkStatus();
}

ParsedListValue::ParsedListValue()
    : ParsedListValue(
          common_internal::MakeShared(&EmptyListValue::Get(), nullptr)) {}

ParsedListValue ParsedListValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(!interface_)) {
    return ParsedListValue();
  }
  if (absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      arena != nullptr &&
      common_internal::GetReferenceCount(interface_) != nullptr) {
    return interface_->Clone(arena);
  }
  return *this;
}

}  // namespace cel
