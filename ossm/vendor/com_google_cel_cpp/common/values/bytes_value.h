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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/arena.h"
#include "common/internal/byte_string.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class BytesValue;
class BytesValueInputStream;
class BytesValueOutputStream;

namespace common_internal {
absl::string_view LegacyBytesValue(const BytesValue& value, bool stable,
                                   google::protobuf::Arena* absl_nonnull arena);
}  // namespace common_internal

// `BytesValue` represents values of the primitive `bytes` type.
class BytesValue final : private common_internal::ValueMixin<BytesValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kBytes;

  static BytesValue From(const char* absl_nullable value,
                         google::protobuf::Arena* absl_nonnull arena
                             ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static BytesValue From(absl::string_view value,
                         google::protobuf::Arena* absl_nonnull arena
                             ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static BytesValue From(const absl::Cord& value);
  static BytesValue From(std::string&& value,
                         google::protobuf::Arena* absl_nonnull arena
                             ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static BytesValue Wrap(absl::string_view value,
                         google::protobuf::Arena* absl_nullable arena
                             ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static BytesValue Wrap(absl::string_view value);
  static BytesValue Wrap(const absl::Cord& value);
  static BytesValue Wrap(std::string&& value) = delete;
  static BytesValue Wrap(std::string&& value,
                         google::protobuf::Arena* absl_nullable arena
                             ABSL_ATTRIBUTE_LIFETIME_BOUND) = delete;

  static BytesValue Concat(const BytesValue& lhs, const BytesValue& rhs,
                           google::protobuf::Arena* absl_nonnull arena
                               ABSL_ATTRIBUTE_LIFETIME_BOUND);

  ABSL_DEPRECATED("Use From")
  explicit BytesValue(const char* absl_nullable value) : value_(value) {}

  ABSL_DEPRECATED("Use From")
  explicit BytesValue(absl::string_view value) : value_(value) {}

  ABSL_DEPRECATED("Use From")
  explicit BytesValue(const absl::Cord& value) : value_(value) {}

  ABSL_DEPRECATED("Use From")
  explicit BytesValue(std::string&& value) : value_(std::move(value)) {}

  ABSL_DEPRECATED("Use From")
  BytesValue(Allocator<> allocator, const char* absl_nullable value)
      : value_(allocator, value) {}

  ABSL_DEPRECATED("Use From")
  BytesValue(Allocator<> allocator, absl::string_view value)
      : value_(allocator, value) {}

  ABSL_DEPRECATED("Use From")
  BytesValue(Allocator<> allocator, const absl::Cord& value)
      : value_(allocator, value) {}

  ABSL_DEPRECATED("Use From")
  BytesValue(Allocator<> allocator, std::string&& value)
      : value_(allocator, std::move(value)) {}

  ABSL_DEPRECATED("Use Wrap")
  BytesValue(Borrower borrower, absl::string_view value)
      : value_(borrower, value) {}

  ABSL_DEPRECATED("Use Wrap")
  BytesValue(Borrower borrower, const absl::Cord& value)
      : value_(borrower, value) {}

  BytesValue() = default;
  BytesValue(const BytesValue&) = default;
  BytesValue(BytesValue&&) = default;
  BytesValue& operator=(const BytesValue&) = default;
  BytesValue& operator=(BytesValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return BytesType::kName; }

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

  bool IsZeroValue() const {
    return NativeValue([](const auto& value) -> bool { return value.empty(); });
  }

  BytesValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  ABSL_DEPRECATED("Use ToString()")
  std::string NativeString() const { return value_.ToString(); }

  ABSL_DEPRECATED("Use ToStringView()")
  absl::string_view NativeString(
      std::string& scratch
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.ToStringView(&scratch);
  }

  ABSL_DEPRECATED("Use ToCord()")
  absl::Cord NativeCord() const { return value_.ToCord(); }

  template <typename Visitor>
  ABSL_DEPRECATED("Use TryFlat()")
  std::common_type_t<
      std::invoke_result_t<Visitor, absl::string_view>,
      std::invoke_result_t<Visitor, const absl::Cord&>> NativeValue(Visitor&&
                                                                        visitor)
      const {
    return value_.Visit(std::forward<Visitor>(visitor));
  }

  void swap(BytesValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

  size_t Size() const;

  bool IsEmpty() const;

  bool Equals(absl::string_view bytes) const;
  bool Equals(const absl::Cord& bytes) const;
  bool Equals(const BytesValue& bytes) const;

  int Compare(absl::string_view bytes) const;
  int Compare(const absl::Cord& bytes) const;
  int Compare(const BytesValue& bytes) const;

  absl::optional<absl::string_view> TryFlat() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.TryFlat();
  }

  std::string ToString() const { return value_.ToString(); }

  void CopyToString(std::string* absl_nonnull out) const {
    value_.CopyToString(out);
  }

  void AppendToString(std::string* absl_nonnull out) const {
    value_.AppendToString(out);
  }

  absl::Cord ToCord() const { return value_.ToCord(); }

  void CopyToCord(absl::Cord* absl_nonnull out) const {
    value_.CopyToCord(out);
  }

  void AppendToCord(absl::Cord* absl_nonnull out) const {
    value_.AppendToCord(out);
  }

  absl::string_view ToStringView(
      std::string* absl_nonnull scratch
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.ToStringView(scratch);
  }

  friend bool operator<(const BytesValue& lhs, const BytesValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class common_internal::ValueMixin<BytesValue>;
  friend class BytesValueInputStream;
  friend class BytesValueOutputStream;
  friend absl::string_view common_internal::LegacyBytesValue(
      const BytesValue& value, bool stable, google::protobuf::Arena* absl_nonnull arena);
  friend struct ArenaTraits<BytesValue>;

  explicit BytesValue(common_internal::ByteString value) noexcept
      : value_(std::move(value)) {}

  common_internal::ByteString value_;
};

inline void swap(BytesValue& lhs, BytesValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const BytesValue& value) {
  return out << value.DebugString();
}

inline bool operator==(const BytesValue& lhs, absl::string_view rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(absl::string_view lhs, const BytesValue& rhs) {
  return rhs == lhs;
}

inline bool operator!=(const BytesValue& lhs, absl::string_view rhs) {
  return !lhs.Equals(rhs);
}

inline bool operator!=(absl::string_view lhs, const BytesValue& rhs) {
  return rhs != lhs;
}

inline BytesValue BytesValue::From(const char* absl_nullable value,
                                   google::protobuf::Arena* absl_nonnull arena
                                       ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return From(absl::NullSafeStringView(value), arena);
}

inline BytesValue BytesValue::From(absl::string_view value,
                                   google::protobuf::Arena* absl_nonnull arena
                                       ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(arena != nullptr);

  return BytesValue(arena, value);
}

inline BytesValue BytesValue::From(const absl::Cord& value) {
  return BytesValue(value);
}

inline BytesValue BytesValue::From(std::string&& value,
                                   google::protobuf::Arena* absl_nonnull arena
                                       ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(arena != nullptr);

  return BytesValue(arena, std::move(value));
}

inline BytesValue BytesValue::Wrap(absl::string_view value,
                                   google::protobuf::Arena* absl_nullable arena
                                       ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(arena != nullptr);

  return BytesValue(Borrower::Arena(arena), value);
}

inline BytesValue BytesValue::Wrap(absl::string_view value) {
  return Wrap(value, nullptr);
}

inline BytesValue BytesValue::Wrap(const absl::Cord& value) {
  return BytesValue(value);
}

namespace common_internal {

inline absl::string_view LegacyBytesValue(const BytesValue& value, bool stable,
                                          google::protobuf::Arena* absl_nonnull arena) {
  return LegacyByteString(value.value_, stable, arena);
}

}  // namespace common_internal

template <>
struct ArenaTraits<BytesValue> {
  using constructible = std::true_type;

  static bool trivially_destructible(const BytesValue& value) {
    return ArenaTraits<>::trivially_destructible(value.value_);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_H_
