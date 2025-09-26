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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_

#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/utility/utility.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "internal/time.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class TimestampValue;

TimestampValue UnsafeTimestampValue(absl::Time value);

// `TimestampValue` represents values of the primitive `timestamp` type.
class TimestampValue final
    : private common_internal::ValueMixin<TimestampValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kTimestamp;

  explicit TimestampValue(absl::Time value) noexcept
      : TimestampValue(absl::in_place, value) {
    ABSL_DCHECK_OK(internal::ValidateTimestamp(value));
  }

  TimestampValue() = default;
  TimestampValue(const TimestampValue&) = default;
  TimestampValue(TimestampValue&&) = default;
  TimestampValue& operator=(const TimestampValue&) = default;
  TimestampValue& operator=(TimestampValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return TimestampType::kName; }

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

  bool IsZeroValue() const { return ToTime() == absl::UnixEpoch(); }

  ABSL_DEPRECATED("Use ToTime()")
  absl::Time NativeValue() const { return static_cast<absl::Time>(*this); }

  ABSL_DEPRECATED("Use ToTime()")
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::Time() const noexcept { return value_; }

  absl::Time ToTime() const { return value_; }

  friend void swap(TimestampValue& lhs, TimestampValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

  friend bool operator==(TimestampValue lhs, TimestampValue rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator<(const TimestampValue& lhs, const TimestampValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class common_internal::ValueMixin<TimestampValue>;
  friend TimestampValue UnsafeTimestampValue(absl::Time value);

  TimestampValue(absl::in_place_t, absl::Time value) : value_(value) {}

  absl::Time value_ = absl::UnixEpoch();
};

inline TimestampValue UnsafeTimestampValue(absl::Time value) {
  return TimestampValue(absl::in_place, value);
}

inline bool operator!=(TimestampValue lhs, TimestampValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, TimestampValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_TIMESTAMP_VALUE_H_
