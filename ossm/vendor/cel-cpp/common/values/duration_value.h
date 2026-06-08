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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_

#include <ostream>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
class DurationValue;

DurationValue UnsafeDurationValue(absl::Duration value);
absl::StatusOr<DurationValue> SafeDurationValue(absl::Duration value);

// `DurationValue` represents values of the primitive `duration` type.
class DurationValue final : private common_internal::ValueMixin<DurationValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kDuration;

  explicit DurationValue(absl::Duration value) noexcept
      : DurationValue(absl::in_place, value) {
    ABSL_DCHECK_OK(internal::ValidateDuration(value));
  }

  DurationValue() = default;
  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;
  DurationValue& operator=(const DurationValue&) = default;
  DurationValue& operator=(DurationValue&&) = default;

  ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return DurationType::kName; }

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

  bool IsZeroValue() const { return ToDuration() == absl::ZeroDuration(); }

  ABSL_DEPRECATED("Use ToDuration()")
  absl::Duration NativeValue() const {
    return static_cast<absl::Duration>(*this);
  }

  ABSL_DEPRECATED("Use ToDuration()")
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::Duration() const noexcept { return value_; }

  absl::Duration ToDuration() const { return value_; }

  friend void swap(DurationValue& lhs, DurationValue& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
  }

  friend bool operator==(DurationValue lhs, DurationValue rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator<(const DurationValue& lhs, const DurationValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class common_internal::ValueMixin<DurationValue>;
  friend DurationValue UnsafeDurationValue(absl::Duration value);

  DurationValue(absl::in_place_t, absl::Duration value) : value_(value) {}

  absl::Duration value_ = absl::ZeroDuration();
};

inline DurationValue UnsafeDurationValue(absl::Duration value) {
  return DurationValue(absl::in_place, value);
}

inline absl::StatusOr<DurationValue> SafeDurationValue(absl::Duration value) {
  absl::Status status = internal::ValidateDuration(value);
  if (!status.ok()) {
    return status;
  }
  return UnsafeDurationValue(value);
}

inline bool operator!=(DurationValue lhs, DurationValue rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, DurationValue value) {
  return out << value.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_DURATION_VALUE_H_
