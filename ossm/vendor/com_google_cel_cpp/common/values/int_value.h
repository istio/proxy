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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_

#include <cstdint>
#include <ostream>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
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
class IntValue;

// `IntValue` represents values of the primitive `int` type.
class IntValue final : private common_internal::ValueMixin<IntValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kInt;

  explicit IntValue(int64_t value) noexcept : value_(value) {}

  IntValue() = default;
  IntValue(const IntValue&) = default;
  IntValue(IntValue&&) = default;
  IntValue& operator=(const IntValue&) = default;
  IntValue& operator=(IntValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return IntType::kName; }

  std::string DebugString() const;

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

  bool IsZeroValue() const { return NativeValue() == 0; }

  int64_t NativeValue() const { return static_cast<int64_t>(*this); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator int64_t() const noexcept { return value_; }

  friend void swap(IntValue& lhs, IntValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

 private:
  friend class common_internal::ValueMixin<IntValue>;

  int64_t value_ = 0;
};

template <typename H>
H AbslHashValue(H state, IntValue value) {
  return H::combine(std::move(state), value.NativeValue());
}

inline bool operator==(IntValue lhs, IntValue rhs) {
  return lhs.NativeValue() == rhs.NativeValue();
}

inline bool operator!=(IntValue lhs, IntValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, IntValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_INT_VALUE_H_
