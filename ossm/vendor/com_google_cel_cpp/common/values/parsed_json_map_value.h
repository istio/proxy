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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_MAP_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/map_value_interface.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class ValueManager;
class ListValue;
class ValueIterator;
class ParsedMapFieldValue;

namespace common_internal {
absl::Status CheckWellKnownStructMessage(const google::protobuf::Message& message);
}  // namespace common_internal

// ParsedJsonMapValue is a MapValue backed by the google.protobuf.Struct
// well known message type.
class ParsedJsonMapValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kMap;
  static constexpr absl::string_view kName = "google.protobuf.Struct";

  using element_type = const google::protobuf::Message;

  explicit ParsedJsonMapValue(Owned<const google::protobuf::Message> value)
      : value_(std::move(value)) {
    ABSL_DCHECK_OK(CheckStruct(cel::to_address(value_)));
  }

  // Constructs an empty `ParsedJsonMapValue`.
  ParsedJsonMapValue() = default;
  ParsedJsonMapValue(const ParsedJsonMapValue&) = default;
  ParsedJsonMapValue(ParsedJsonMapValue&&) = default;
  ParsedJsonMapValue& operator=(const ParsedJsonMapValue&) = default;
  ParsedJsonMapValue& operator=(ParsedJsonMapValue&&) = default;

  static ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return kName; }

  static MapType GetRuntimeType() { return JsonMapType(); }

  const google::protobuf::Message& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return *value_;
  }

  absl::Nonnull<const google::protobuf::Message*> operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(*this);
    return value_.operator->();
  }

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const {
    CEL_ASSIGN_OR_RETURN(auto value, ConvertToJson(converter));
    return absl::get<JsonObject>(std::move(value));
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return IsEmpty(); }

  ParsedJsonMapValue Clone(Allocator<> allocator) const;

  bool IsEmpty() const { return Size() == 0; }

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

  explicit operator bool() const { return static_cast<bool>(value_); }

  friend void swap(ParsedJsonMapValue& lhs, ParsedJsonMapValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

  friend bool operator==(const ParsedJsonMapValue& lhs,
                         const ParsedJsonMapValue& rhs);

 private:
  friend std::pointer_traits<ParsedJsonMapValue>;
  friend class ParsedMapFieldValue;

  static absl::Status CheckStruct(
      absl::Nullable<const google::protobuf::Message*> message) {
    return message == nullptr
               ? absl::OkStatus()
               : common_internal::CheckWellKnownStructMessage(*message);
  }

  Owned<const google::protobuf::Message> value_;
};

inline bool operator!=(const ParsedJsonMapValue& lhs,
                       const ParsedJsonMapValue& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedJsonMapValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

namespace std {

template <>
struct pointer_traits<cel::ParsedJsonMapValue> {
  using pointer = cel::ParsedJsonMapValue;
  using element_type = typename cel::ParsedJsonMapValue::element_type;
  using difference_type = ptrdiff_t;

  static element_type* to_address(const pointer& p) noexcept {
    return cel::to_address(p.value_);
  }
};

}  // namespace std

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_JSON_MAP_VALUE_H_
