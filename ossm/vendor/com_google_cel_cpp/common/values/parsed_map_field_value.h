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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_FIELD_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_FIELD_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/map_value_interface.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueManager;
class ValueIterator;
class ListValue;
class ParsedJsonMapValue;

// ParsedMapFieldValue is a MapValue over a map field of a parsed protocol
// buffer message.
class ParsedMapFieldValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;
  static constexpr absl::string_view kName = "map";

  ParsedMapFieldValue(Owned<const google::protobuf::Message> message,
                      absl::Nonnull<const google::protobuf::FieldDescriptor*> field)
      : message_(std::move(message)), field_(field) {
    ABSL_DCHECK(field_->is_map())
        << field_->full_name() << " must be a map field";
  }

  // Places the `ParsedMapFieldValue` into an invalid state. Anything
  // except assigning to `ParsedMapFieldValue` is undefined behavior.
  ParsedMapFieldValue() = default;

  ParsedMapFieldValue(const ParsedMapFieldValue&) = default;
  ParsedMapFieldValue(ParsedMapFieldValue&&) = default;
  ParsedMapFieldValue& operator=(const ParsedMapFieldValue&) = default;
  ParsedMapFieldValue& operator=(ParsedMapFieldValue&&) = default;

  static ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return kName; }

  static MapType GetRuntimeType() { return MapType(); }

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  ParsedMapFieldValue Clone(Allocator<> allocator) const;

  bool IsEmpty() const;

  size_t Size() const;

  absl::Status Get(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager,
                            const Value& key) const;

  absl::StatusOr<bool> Find(ValueManager& value_manager, const Value& key,
                            Value& result) const;
  absl::StatusOr<std::pair<Value, bool>> Find(ValueManager& value_manager,
                                              const Value& key) const;

  absl::Status Has(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Has(ValueManager& value_manager,
                            const Value& key) const;

  absl::Status ListKeys(ValueManager& value_manager, ListValue& result) const;
  absl::StatusOr<ListValue> ListKeys(ValueManager& value_manager) const;

  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>> NewIterator(
      ValueManager& value_manager) const;

  const google::protobuf::Message& message() const {
    ABSL_DCHECK(*this);
    return *message_;
  }

  absl::Nonnull<const google::protobuf::FieldDescriptor*> field() const {
    ABSL_DCHECK(*this);
    return field_;
  }

  // Returns `true` if `ParsedMapFieldValue` is in a valid state.
  explicit operator bool() const { return field_ != nullptr; }

  friend void swap(ParsedMapFieldValue& lhs,
                   ParsedMapFieldValue& rhs) noexcept {
    using std::swap;
    swap(lhs.message_, rhs.message_);
    swap(lhs.field_, rhs.field_);
  }

 private:
  friend class ParsedJsonMapValue;

  absl::Nonnull<const google::protobuf::Reflection*> GetReflection() const;

  Owned<const google::protobuf::Message> message_;
  absl::Nullable<const google::protobuf::FieldDescriptor*> field_ = nullptr;
};

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedMapFieldValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_MAP_FIELD_VALUE_H_
