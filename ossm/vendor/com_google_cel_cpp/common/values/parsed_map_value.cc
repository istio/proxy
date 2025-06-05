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

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "common/values/values.h"
#include "eval/public/cel_value.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace {

using ::google::api::expr::runtime::CelList;
using ::google::api::expr::runtime::CelValue;

absl::Status NoSuchKeyError(const Value& key) {
  return absl::NotFoundError(
      absl::StrCat("Key not found in map : ", key.DebugString()));
}

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

class EmptyMapValueKeyIterator final : public ValueIterator {
 public:
  bool HasNext() override { return false; }

  absl::Status Next(ValueManager&, Value&) override {
    return absl::FailedPreconditionError(
        "ValueIterator::Next() called when "
        "ValueIterator::HasNext() returns false");
  }
};

class EmptyMapValue final : public common_internal::CompatMapValue {
 public:
  static const EmptyMapValue& Get() {
    static const absl::NoDestructor<EmptyMapValue> empty;
    return *empty;
  }

  EmptyMapValue() = default;

  std::string DebugString() const override { return "{}"; }

  bool IsEmpty() const override { return true; }

  size_t Size() const override { return 0; }

  absl::Status ListKeys(ValueManager&, ListValue& result) const override {
    result = ListValue();
    return absl::OkStatus();
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager&) const override {
    return std::make_unique<EmptyMapValueKeyIterator>();
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter&) const override {
    return JsonObject();
  }

  ParsedMapValue Clone(ArenaAllocator<>) const override {
    return ParsedMapValue();
  }

  absl::optional<CelValue> operator[](CelValue key) const override {
    return absl::nullopt;
  }

  absl::optional<CelValue> Get(google::protobuf::Arena* arena,
                               CelValue key) const override {
    return absl::nullopt;
  }

  absl::StatusOr<bool> Has(const CelValue& key) const override { return false; }

  int size() const override { return static_cast<int>(Size()); }

  absl::StatusOr<const CelList*> ListKeys() const override {
    return common_internal::EmptyCompatListValue();
  }

  absl::StatusOr<const CelList*> ListKeys(google::protobuf::Arena*) const override {
    return ListKeys();
  }

 private:
  absl::StatusOr<bool> FindImpl(ValueManager&, const Value&,
                                Value&) const override {
    return false;
  }

  absl::StatusOr<bool> HasImpl(ValueManager&, const Value&) const override {
    return false;
  }
};

}  // namespace

namespace common_internal {

absl::Nonnull<const CompatMapValue*> EmptyCompatMapValue() {
  return &EmptyMapValue::Get();
}

}  // namespace common_internal

absl::Status ParsedMapValueInterface::SerializeTo(
    AnyToJsonConverter& value_manager, absl::Cord& value) const {
  CEL_ASSIGN_OR_RETURN(auto json, ConvertToJsonObject(value_manager));
  return internal::SerializeStruct(json, value);
}

absl::Status ParsedMapValueInterface::Get(ValueManager& value_manager,
                                          const Value& key,
                                          Value& result) const {
  CEL_ASSIGN_OR_RETURN(bool ok, Find(value_manager, key, result));
  if (ABSL_PREDICT_FALSE(!ok)) {
    switch (result.kind()) {
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        break;
      default:
        result = ErrorValue(NoSuchKeyError(key));
        break;
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> ParsedMapValueInterface::Find(ValueManager& value_manager,
                                                   const Value& key,
                                                   Value& result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value(key);
      return false;
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      result = ErrorValue(InvalidMapKeyTypeError(key.kind()));
      return false;
  }
  CEL_ASSIGN_OR_RETURN(auto ok, FindImpl(value_manager, key, result));
  if (ok) {
    return true;
  }
  result = NullValue{};
  return false;
}

absl::Status ParsedMapValueInterface::Has(ValueManager& value_manager,
                                          const Value& key,
                                          Value& result) const {
  switch (key.kind()) {
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      result = Value{key};
      return absl::OkStatus();
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      break;
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
  CEL_ASSIGN_OR_RETURN(auto has, HasImpl(value_manager, key));
  result = BoolValue(has);
  return absl::OkStatus();
}

absl::Status ParsedMapValueInterface::ForEach(ValueManager& value_manager,
                                              ForEachCallback callback) const {
  CEL_ASSIGN_OR_RETURN(auto iterator, NewIterator(value_manager));
  while (iterator->HasNext()) {
    Value key;
    Value value;
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, key));
    CEL_RETURN_IF_ERROR(Get(value_manager, key, value));
    CEL_ASSIGN_OR_RETURN(auto ok, callback(key, value));
    if (!ok) {
      break;
    }
  }
  return absl::OkStatus();
}

absl::Status ParsedMapValueInterface::Equal(ValueManager& value_manager,
                                            const Value& other,
                                            Value& result) const {
  if (auto list_value = other.As<MapValue>(); list_value.has_value()) {
    return MapValueEqual(value_manager, *this, *list_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

ParsedMapValue::ParsedMapValue()
    : ParsedMapValue(
          common_internal::MakeShared(&EmptyMapValue::Get(), nullptr)) {}

ParsedMapValue ParsedMapValue::Clone(Allocator<> allocator) const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(!interface_)) {
    return ParsedMapValue();
  }
  if (absl::Nullable<google::protobuf::Arena*> arena = allocator.arena();
      arena != nullptr &&
      common_internal::GetReferenceCount(interface_) != nullptr) {
    return interface_->Clone(arena);
  }
  return *this;
}

}  // namespace cel
