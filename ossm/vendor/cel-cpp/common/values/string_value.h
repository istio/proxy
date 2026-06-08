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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRING_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRING_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
class ListValue;
class StringValue;

namespace common_internal {
absl::string_view LegacyStringValue(const StringValue& value, bool stable,
                                    google::protobuf::Arena* absl_nonnull arena);
}  // namespace common_internal

// `StringValue` represents values of the primitive `string` type.
class StringValue final : private common_internal::ValueMixin<StringValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kString;

  static StringValue From(const char* absl_nullable value,
                          google::protobuf::Arena* absl_nonnull arena
                              ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static StringValue From(absl::string_view value,
                          google::protobuf::Arena* absl_nonnull arena
                              ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static StringValue From(const absl::Cord& value);
  static StringValue From(std::string&& value,
                          google::protobuf::Arena* absl_nonnull arena
                              ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static StringValue Wrap(absl::string_view value,
                          google::protobuf::Arena* absl_nullable arena
                              ABSL_ATTRIBUTE_LIFETIME_BOUND);
  static StringValue Wrap(absl::string_view value);
  static StringValue Wrap(const absl::Cord& value);
  static StringValue Wrap(std::string&& value) = delete;
  static StringValue Wrap(std::string&& value,
                          google::protobuf::Arena* absl_nullable arena
                              ABSL_ATTRIBUTE_LIFETIME_BOUND) = delete;

  static StringValue Concat(const StringValue& lhs, const StringValue& rhs,
                            google::protobuf::Arena* absl_nonnull arena
                                ABSL_ATTRIBUTE_LIFETIME_BOUND);

  ABSL_DEPRECATED("Use From")
  explicit StringValue(const char* absl_nullable value) : value_(value) {}

  ABSL_DEPRECATED("Use From")
  explicit StringValue(absl::string_view value) : value_(value) {}

  ABSL_DEPRECATED("Use From")
  explicit StringValue(const absl::Cord& value) : value_(value) {}

  ABSL_DEPRECATED("Use From")
  explicit StringValue(std::string&& value) : value_(std::move(value)) {}

  ABSL_DEPRECATED("Use From")
  StringValue(Allocator<> allocator, const char* absl_nullable value)
      : value_(allocator, value) {}

  ABSL_DEPRECATED("Use From")
  StringValue(Allocator<> allocator, absl::string_view value)
      : value_(allocator, value) {}

  ABSL_DEPRECATED("Use From")
  StringValue(Allocator<> allocator, const absl::Cord& value)
      : value_(allocator, value) {}

  ABSL_DEPRECATED("Use From")
  StringValue(Allocator<> allocator, std::string&& value)
      : value_(allocator, std::move(value)) {}

  ABSL_DEPRECATED("Use Wrap")
  StringValue(Borrower borrower, absl::string_view value)
      : value_(borrower, value) {}

  ABSL_DEPRECATED("Use Wrap")
  StringValue(Borrower borrower, const absl::Cord& value)
      : value_(borrower, value) {}

  StringValue() = default;
  StringValue(const StringValue&) = default;
  StringValue(StringValue&&) = default;
  StringValue& operator=(const StringValue&) = default;
  StringValue& operator=(StringValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return StringType::kName; }

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

  StringValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  bool IsZeroValue() const {
    return NativeValue([](const auto& value) -> bool { return value.empty(); });
  }

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

  void swap(StringValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

  size_t Size() const;

  bool IsEmpty() const;

  bool Equals(absl::string_view string) const;
  bool Equals(const absl::Cord& string) const;
  bool Equals(const StringValue& string) const;

  int Compare(absl::string_view string) const;
  int Compare(const absl::Cord& string) const;
  int Compare(const StringValue& string) const;

  bool StartsWith(absl::string_view string) const;
  bool StartsWith(const absl::Cord& string) const;
  bool StartsWith(const StringValue& string) const;

  bool EndsWith(absl::string_view string) const;
  bool EndsWith(const absl::Cord& string) const;
  bool EndsWith(const StringValue& string) const;

  bool Contains(absl::string_view string) const;
  bool Contains(const absl::Cord& string) const;
  bool Contains(const StringValue& string) const;

  // Returns the 0-based index of the first occurrence of `string` in this
  // string, or `absl::nullopt` if `string` is not found.
  absl::optional<int64_t> IndexOf(absl::string_view string) const;
  absl::optional<int64_t> IndexOf(const absl::Cord& string) const;
  absl::optional<int64_t> IndexOf(const StringValue& string) const;
  // Returns the 0-based index of the first occurrence of `string` in this
  // string at or after `pos`, or `absl::nullopt` if `string` is not found.
  absl::optional<int64_t> IndexOf(absl::string_view string, int64_t pos) const;
  absl::optional<int64_t> IndexOf(const absl::Cord& string, int64_t pos) const;
  absl::optional<int64_t> IndexOf(const StringValue& string, int64_t pos) const;

  // Returns the 0-based index of the last occurrence of `string` in this
  // string, or `absl::nullopt` if `string` is not found.
  absl::optional<int64_t> LastIndexOf(absl::string_view string) const;
  absl::optional<int64_t> LastIndexOf(const absl::Cord& string) const;
  absl::optional<int64_t> LastIndexOf(const StringValue& string) const;
  // Returns the 0-based index of the last occurrence of `string` in this
  // string at or before `pos`, or `absl::nullopt` if `string` is not found.
  absl::optional<int64_t> LastIndexOf(absl::string_view string,
                                      int64_t pos) const;
  absl::optional<int64_t> LastIndexOf(const absl::Cord& string,
                                      int64_t pos) const;
  absl::optional<int64_t> LastIndexOf(const StringValue& string,
                                      int64_t pos) const;

  Value Substring(int64_t start) const;

  Value Substring(int64_t start, int64_t end) const;

  // Returns a new `StringValue` with all lowercase ASCII characters
  // converted to lowercase.
  StringValue LowerAscii(google::protobuf::Arena* absl_nonnull arena) const;

  // Returns a new `StringValue` with all lowercase ASCII characters
  // converted to uppercase.
  StringValue UpperAscii(google::protobuf::Arena* absl_nonnull arena) const;

  StringValue Trim() const;

  // Returns a new `StringValue` with the string surrounded by double quotes.
  StringValue Quote(google::protobuf::Arena* absl_nonnull arena) const;

  // Returns a new `StringValue` with the characters in reverse order.
  StringValue Reverse(google::protobuf::Arena* absl_nonnull arena) const;

  // Joins the elements of `list` with this string using `separator` as the
  // separator.
  absl::Status Join(const ListValue& list,
                    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
                    google::protobuf::MessageFactory* absl_nonnull message_factory,
                    google::protobuf::Arena* absl_nonnull arena,
                    Value* absl_nonnull result) const;
  absl::StatusOr<Value> Join(
      const ListValue& list,
      const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
      google::protobuf::MessageFactory* absl_nonnull message_factory,
      google::protobuf::Arena* absl_nonnull arena) const;

  // Splits this string on `delimiter`, returning a list of strings. If `limit`
  // is provided and non-negative, the string is split into at most `limit`
  // substrings.
  absl::Status Split(const StringValue& delimiter, int64_t limit,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  absl::StatusOr<Value> Split(const StringValue& delimiter, int64_t limit,
                              google::protobuf::Arena* absl_nonnull arena) const;
  absl::Status Split(const StringValue& delimiter,
                     google::protobuf::Arena* absl_nonnull arena,
                     Value* absl_nonnull result) const;
  absl::StatusOr<Value> Split(const StringValue& delimiter,
                              google::protobuf::Arena* absl_nonnull arena) const;

  // Replaces occurrences of `needle` with `replacement`. If `limit` is provided
  // and non-negative, only the first `limit` occurrences are replaced.
  absl::Status Replace(const StringValue& needle,
                       const StringValue& replacement, int64_t limit,
                       google::protobuf::Arena* absl_nonnull arena,
                       Value* absl_nonnull result) const;
  absl::StatusOr<Value> Replace(const StringValue& needle,
                                const StringValue& replacement, int64_t limit,
                                google::protobuf::Arena* absl_nonnull arena) const;
  absl::Status Replace(const StringValue& needle,
                       const StringValue& replacement,
                       google::protobuf::Arena* absl_nonnull arena,
                       Value* absl_nonnull result) const;
  absl::StatusOr<Value> Replace(const StringValue& needle,
                                const StringValue& replacement,
                                google::protobuf::Arena* absl_nonnull arena) const;

  // Returns the character at `pos` as a new `StringValue`. `pos` is a
  // 0-based index based on Unicode code points. Returns `ErrorValue` if `pos`
  // is out of range.
  Value CharAt(int64_t pos) const;

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

  template <typename H>
  friend H AbslHashValue(H state, const StringValue& string) {
    return H::combine(std::move(state), string.value_);
  }

  friend bool operator==(const StringValue& lhs, const StringValue& rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator<(const StringValue& lhs, const StringValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class common_internal::ValueMixin<StringValue>;
  friend absl::string_view common_internal::LegacyStringValue(
      const StringValue& value, bool stable, google::protobuf::Arena* absl_nonnull arena);
  friend struct ArenaTraits<StringValue>;

  explicit StringValue(common_internal::ByteString value) noexcept
      : value_(std::move(value)) {}

  common_internal::ByteString value_;
};

inline void swap(StringValue& lhs, StringValue& rhs) noexcept { lhs.swap(rhs); }

inline bool operator==(const StringValue& lhs, absl::string_view rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(absl::string_view lhs, const StringValue& rhs) {
  return rhs == lhs;
}

inline bool operator==(const StringValue& lhs, const absl::Cord& rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(const absl::Cord& lhs, const StringValue& rhs) {
  return rhs == lhs;
}

inline bool operator!=(const StringValue& lhs, absl::string_view rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(absl::string_view lhs, const StringValue& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const StringValue& lhs, const absl::Cord& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const absl::Cord& lhs, const StringValue& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const StringValue& lhs, const StringValue& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(const StringValue& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<(absl::string_view lhs, const StringValue& rhs) {
  return rhs.Compare(lhs) > 0;
}

inline bool operator<(const StringValue& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs) < 0;
}

inline bool operator<(const absl::Cord& lhs, const StringValue& rhs) {
  return rhs.Compare(lhs) > 0;
}

inline std::ostream& operator<<(std::ostream& out, const StringValue& value) {
  return out << value.DebugString();
}

inline StringValue StringValue::From(const char* absl_nullable value,
                                     google::protobuf::Arena* absl_nonnull arena
                                         ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return From(absl::NullSafeStringView(value), arena);
}

inline StringValue StringValue::From(absl::string_view value,
                                     google::protobuf::Arena* absl_nonnull arena
                                         ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(arena != nullptr);

  return StringValue(arena, value);
}

inline StringValue StringValue::From(const absl::Cord& value) {
  return StringValue(value);
}

inline StringValue StringValue::From(std::string&& value,
                                     google::protobuf::Arena* absl_nonnull arena
                                         ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(arena != nullptr);

  return StringValue(arena, std::move(value));
}

inline StringValue StringValue::Wrap(absl::string_view value,
                                     google::protobuf::Arena* absl_nullable arena
                                         ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  ABSL_DCHECK(arena != nullptr);

  return StringValue(Borrower::Arena(arena), value);
}

inline StringValue StringValue::Wrap(absl::string_view value) {
  return Wrap(value, nullptr);
}

inline StringValue StringValue::Wrap(const absl::Cord& value) {
  return StringValue(value);
}

namespace common_internal {

inline absl::string_view LegacyStringValue(const StringValue& value,
                                           bool stable,
                                           google::protobuf::Arena* absl_nonnull arena) {
  return LegacyByteString(value.value_, stable, arena);
}

}  // namespace common_internal

template <>
struct ArenaTraits<StringValue> {
  using constructible = std::true_type;

  static bool trivially_destructible(const StringValue& value) {
    return ArenaTraits<>::trivially_destructible(value.value_);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRING_VALUE_H_
