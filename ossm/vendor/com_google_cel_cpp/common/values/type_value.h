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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class TypeValue;

// `TypeValue` represents values of the primitive `type` type.
class TypeValue final : private common_internal::ValueMixin<TypeValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kType;

  explicit TypeValue(Type value) : value_(value) {}

  TypeValue() = default;
  TypeValue(const TypeValue&) = default;
  TypeValue(TypeValue&&) = default;
  TypeValue& operator=(const TypeValue&) = default;
  TypeValue& operator=(TypeValue&&) = default;

  static constexpr ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return TypeType::kName; }

  std::string DebugString() const { return type().DebugString(); }

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::io::ZeroCopyOutputStream* absl_nonnull output) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Message* absl_nonnull json) const;

  absl::Status Equal(const Value& other,
                     const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                     google::protobuf::MessageFactory* absl_nonnull message_factory,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  using ValueMixin::Equal;

  bool IsZeroValue() const { return false; }

  ABSL_DEPRECATED(("Use type()"))
  const Type& NativeValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return type();
  }

  const Type& type() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return value_; }

  absl::string_view name() const { return type().name(); }

  friend void swap(TypeValue& lhs, TypeValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  friend class common_internal::ValueMixin<TypeValue>;

  Type value_;
};

inline std::ostream& operator<<(std::ostream& out, const TypeValue& value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_TYPE_VALUE_H_
