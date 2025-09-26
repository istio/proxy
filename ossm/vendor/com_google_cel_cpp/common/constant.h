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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CONSTANT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CONSTANT_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/functional/overload.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"

namespace cel {

class Expr;
class Constant;
class BytesConstant;
class StringConstant;
class VariableDecl;

class BytesConstant final : public std::string {
 public:
  explicit BytesConstant(std::string string) : std::string(std::move(string)) {}

  explicit BytesConstant(absl::string_view string)
      : BytesConstant(std::string(string)) {}

  explicit BytesConstant(const char* string)
      : BytesConstant(absl::NullSafeStringView(string)) {}

  BytesConstant() = default;
  BytesConstant(const BytesConstant&) = default;
  BytesConstant(BytesConstant&&) = default;
  BytesConstant& operator=(const BytesConstant&) = default;
  BytesConstant& operator=(BytesConstant&&) = default;

  BytesConstant(const StringConstant&) = delete;
  BytesConstant(StringConstant&&) = delete;
  BytesConstant& operator=(const StringConstant&) = delete;
  BytesConstant& operator=(StringConstant&&) = delete;

 private:
  static const BytesConstant& default_instance();

  friend class Constant;
};

class StringConstant final : public std::string {
 public:
  explicit StringConstant(std::string string)
      : std::string(std::move(string)) {}

  explicit StringConstant(absl::string_view string)
      : StringConstant(std::string(string)) {}

  explicit StringConstant(const char* string)
      : StringConstant(absl::NullSafeStringView(string)) {}

  StringConstant() = default;
  StringConstant(const StringConstant&) = default;
  StringConstant(StringConstant&&) = default;
  StringConstant& operator=(const StringConstant&) = default;
  StringConstant& operator=(StringConstant&&) = default;

  StringConstant(const BytesConstant&) = delete;
  StringConstant(BytesConstant&&) = delete;
  StringConstant& operator=(const BytesConstant&) = delete;
  StringConstant& operator=(BytesConstant&&) = delete;

 private:
  static const StringConstant& default_instance();

  friend class Constant;
};

namespace common_internal {

template <size_t I, typename U, typename T, typename... Ts>
struct ConstantKindIndexer {
  static constexpr size_t value =
      std::conditional_t<std::is_same_v<U, T>,
                         std::integral_constant<size_t, I>,
                         ConstantKindIndexer<I + 1, U, Ts...>>::value;
};

template <size_t I, typename U, typename T>
struct ConstantKindIndexer<I, U, T> {
  static constexpr size_t value = std::conditional_t<
      std::is_same_v<U, T>, std::integral_constant<size_t, I>,
      std::integral_constant<size_t, absl::variant_npos>>::value;
};

template <typename... Ts>
struct ConstantKindImpl {
  using VariantType = absl::variant<Ts...>;

  template <typename U>
  static constexpr size_t IndexOf() {
    return ConstantKindIndexer<0, U, Ts...>::value;
  }
};

using ConstantKind =
    ConstantKindImpl<absl::monostate, std::nullptr_t, bool, int64_t, uint64_t,
                     double, BytesConstant, StringConstant, absl::Duration,
                     absl::Time>;

static_assert(ConstantKind::IndexOf<absl::monostate>() == 0);
static_assert(ConstantKind::IndexOf<std::nullptr_t>() == 1);
static_assert(ConstantKind::IndexOf<bool>() == 2);
static_assert(ConstantKind::IndexOf<int64_t>() == 3);
static_assert(ConstantKind::IndexOf<uint64_t>() == 4);
static_assert(ConstantKind::IndexOf<double>() == 5);
static_assert(ConstantKind::IndexOf<BytesConstant>() == 6);
static_assert(ConstantKind::IndexOf<StringConstant>() == 7);
static_assert(ConstantKind::IndexOf<absl::Duration>() == 8);
static_assert(ConstantKind::IndexOf<absl::Time>() == 9);
static_assert(ConstantKind::IndexOf<void>() == absl::variant_npos);

}  // namespace common_internal

// Constant is a variant composed of all the literal types support by the Common
// Expression Language.
using ConstantKind = common_internal::ConstantKind::VariantType;

enum class ConstantKindCase {
  kUnspecified,
  kNull,
  kBool,
  kInt,
  kUint,
  kDouble,
  kBytes,
  kString,
  kDuration,
  kTimestamp,
};

template <typename U>
constexpr size_t ConstantKindIndexOf() {
  return common_internal::ConstantKind::IndexOf<U>();
}

// Returns the `null` literal.
std::string FormatNullConstant();
inline std::string FormatNullConstant(std::nullptr_t) {
  return FormatNullConstant();
}

// Formats `value` as a bool literal.
std::string FormatBoolConstant(bool value);

// Formats `value` as a int literal.
std::string FormatIntConstant(int64_t value);

// Formats `value` as a uint literal.
std::string FormatUintConstant(uint64_t value);

// Formats `value` as a double literal-like representation. Due to Common
// Expression Language not having NaN or infinity literals, the result will not
// always be syntactically valid.
std::string FormatDoubleConstant(double value);

// Formats `value` as a bytes literal.
std::string FormatBytesConstant(absl::string_view value);

// Formats `value` as a string literal.
std::string FormatStringConstant(absl::string_view value);

// Formats `value` as a duration constant.
std::string FormatDurationConstant(absl::Duration value);

// Formats `value` as a timestamp constant.
std::string FormatTimestampConstant(absl::Time value);

// Represents a primitive literal.
//
// This is similar as the primitives supported in the well-known type
// `google.protobuf.Value`, but richer so it can represent CEL's full range of
// primitives.
//
// Lists and structs are not included as constants as these aggregate types may
// contain [Expr][] elements which require evaluation and are thus not constant.
//
// Examples of constants include: `"hello"`, `b'bytes'`, `1u`, `4.2`, `-2`,
// `true`, `null`.
class Constant final {
 public:
  Constant() = default;
  Constant(const Constant&) = default;
  Constant(Constant&&) = default;
  Constant& operator=(const Constant&) = default;
  Constant& operator=(Constant&&) = default;

  explicit Constant(ConstantKind kind) : kind_(std::move(kind)) {}

  ABSL_MUST_USE_RESULT const ConstantKind& kind() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kind_;
  }

  ABSL_DEPRECATED("Use kind()")
  ABSL_MUST_USE_RESULT const ConstantKind& constant_kind() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kind();
  }

  ABSL_MUST_USE_RESULT bool has_value() const {
    return !absl::holds_alternative<absl::monostate>(kind());
  }

  ABSL_MUST_USE_RESULT bool has_null_value() const {
    return absl::holds_alternative<std::nullptr_t>(kind());
  }

  ABSL_MUST_USE_RESULT std::nullptr_t null_value() const { return nullptr; }

  void set_null_value() { mutable_kind().emplace<std::nullptr_t>(); }

  void set_null_value(std::nullptr_t) { set_null_value(); }

  ABSL_MUST_USE_RESULT bool has_bool_value() const {
    return absl::holds_alternative<bool>(kind());
  }

  void set_bool_value(bool value) { mutable_kind().emplace<bool>(value); }

  ABSL_MUST_USE_RESULT bool bool_value() const { return get_value<bool>(); }

  ABSL_MUST_USE_RESULT bool has_int_value() const {
    return absl::holds_alternative<int64_t>(kind());
  }

  void set_int_value(int64_t value) { mutable_kind().emplace<int64_t>(value); }

  ABSL_MUST_USE_RESULT int64_t int_value() const {
    return get_value<int64_t>();
  }

  ABSL_MUST_USE_RESULT bool has_uint_value() const {
    return absl::holds_alternative<uint64_t>(kind());
  }

  void set_uint_value(uint64_t value) {
    mutable_kind().emplace<uint64_t>(value);
  }

  ABSL_MUST_USE_RESULT uint64_t uint_value() const {
    return get_value<uint64_t>();
  }

  ABSL_DEPRECATED("Use has_int_value")
  ABSL_MUST_USE_RESULT bool has_int64_value() const { return has_int_value(); }

  ABSL_DEPRECATED("Use set_int_value()")
  void set_int64_value(int64_t value) { set_int_value(value); }

  ABSL_DEPRECATED("Use int_value()")
  ABSL_MUST_USE_RESULT int64_t int64_value() const { return int_value(); }

  ABSL_DEPRECATED("Use has_uint_value()")
  ABSL_MUST_USE_RESULT bool has_uint64_value() const {
    return has_uint_value();
  }

  ABSL_DEPRECATED("Use set_uint_value()")
  void set_uint64_value(uint64_t value) { set_uint_value(value); }

  ABSL_DEPRECATED("Use uint_value()")
  ABSL_MUST_USE_RESULT uint64_t uint64_value() const { return uint_value(); }

  ABSL_MUST_USE_RESULT bool has_double_value() const {
    return absl::holds_alternative<double>(kind());
  }

  void set_double_value(double value) { mutable_kind().emplace<double>(value); }

  ABSL_MUST_USE_RESULT double double_value() const {
    return get_value<double>();
  }

  ABSL_MUST_USE_RESULT bool has_bytes_value() const {
    return absl::holds_alternative<BytesConstant>(kind());
  }

  void set_bytes_value(BytesConstant value) {
    mutable_kind().emplace<BytesConstant>(std::move(value));
  }

  void set_bytes_value(std::string value) {
    set_bytes_value(BytesConstant{std::move(value)});
  }

  void set_bytes_value(absl::string_view value) {
    set_bytes_value(BytesConstant{value});
  }

  void set_bytes_value(const char* value) {
    set_bytes_value(absl::NullSafeStringView(value));
  }

  ABSL_MUST_USE_RESULT const std::string& bytes_value() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (const auto* alt = absl::get_if<BytesConstant>(&kind()); alt) {
      return *alt;
    }
    return BytesConstant::default_instance();
  }

  ABSL_MUST_USE_RESULT std::string release_bytes_value() {
    std::string string;
    if (auto* alt = absl::get_if<BytesConstant>(&mutable_kind()); alt) {
      string.swap(*alt);
    }
    mutable_kind().emplace<absl::monostate>();
    return string;
  }

  ABSL_MUST_USE_RESULT bool has_string_value() const {
    return absl::holds_alternative<StringConstant>(kind());
  }

  void set_string_value(StringConstant value) {
    mutable_kind().emplace<StringConstant>(std::move(value));
  }

  void set_string_value(std::string value) {
    set_string_value(StringConstant{std::move(value)});
  }

  void set_string_value(absl::string_view value) {
    set_string_value(StringConstant{value});
  }

  void set_string_value(const char* value) {
    set_string_value(absl::NullSafeStringView(value));
  }

  ABSL_MUST_USE_RESULT const std::string& string_value() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    if (const auto* alt = absl::get_if<StringConstant>(&kind()); alt) {
      return *alt;
    }
    return StringConstant::default_instance();
  }

  ABSL_MUST_USE_RESULT std::string release_string_value() {
    std::string string;
    if (auto* alt = absl::get_if<StringConstant>(&mutable_kind()); alt) {
      string.swap(*alt);
    }
    mutable_kind().emplace<absl::monostate>();
    return string;
  }

  ABSL_DEPRECATED("duration is no longer considered a builtin type")
  ABSL_MUST_USE_RESULT bool has_duration_value() const {
    return absl::holds_alternative<absl::Duration>(kind());
  }

  ABSL_DEPRECATED("duration is no longer considered a builtin type")
  void set_duration_value(absl::Duration value) {
    mutable_kind().emplace<absl::Duration>(value);
  }

  ABSL_DEPRECATED("duration is no longer considered a builtin type")
  ABSL_MUST_USE_RESULT absl::Duration duration_value() const {
    return get_value<absl::Duration>();
  }

  ABSL_DEPRECATED("timestamp is no longer considered a builtin type")
  ABSL_MUST_USE_RESULT bool has_timestamp_value() const {
    return absl::holds_alternative<absl::Time>(kind());
  }

  ABSL_DEPRECATED("timestamp is no longer considered a builtin type")
  void set_timestamp_value(absl::Time value) {
    mutable_kind().emplace<absl::Time>(value);
  }

  ABSL_DEPRECATED("timestamp is no longer considered a builtin type")
  ABSL_MUST_USE_RESULT absl::Time timestamp_value() const {
    return get_value<absl::Time>();
  }

  ABSL_DEPRECATED("Use has_timestamp_value()")
  ABSL_MUST_USE_RESULT bool has_time_value() const {
    return has_timestamp_value();
  }

  ABSL_DEPRECATED("Use set_timestamp_value()")
  void set_time_value(absl::Time value) { set_timestamp_value(value); }

  ABSL_DEPRECATED("Use timestamp_value()")
  ABSL_MUST_USE_RESULT absl::Time time_value() const {
    return timestamp_value();
  }

  ConstantKindCase kind_case() const {
    static_assert(absl::variant_size_v<ConstantKind> == 10);
    if (kind_.index() <= 10) {
      return static_cast<ConstantKindCase>(kind_.index());
    }
    return ConstantKindCase::kUnspecified;
  }

 private:
  friend class Expr;
  friend class VariableDecl;

  static const Constant& default_instance();

  ABSL_MUST_USE_RESULT ConstantKind& mutable_kind()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return kind_;
  }

  template <typename T>
  T get_value() const {
    if (const auto* alt = absl::get_if<T>(&kind()); alt) {
      return *alt;
    }
    return T{};
  }

  ConstantKind kind_;
};

inline bool operator==(const Constant& lhs, const Constant& rhs) {
  return lhs.kind() == rhs.kind();
}

inline bool operator!=(const Constant& lhs, const Constant& rhs) {
  return lhs.kind() != rhs.kind();
}

template <typename Sink>
void AbslStringify(Sink& sink, const Constant& constant) {
  absl::visit(
      absl::Overload(
          [&sink](absl::monostate) -> void { sink.Append("<unspecified>"); },
          [&sink](std::nullptr_t value) -> void {
            sink.Append(FormatNullConstant(value));
          },
          [&sink](bool value) -> void {
            sink.Append(FormatBoolConstant(value));
          },
          [&sink](int64_t value) -> void {
            sink.Append(FormatIntConstant(value));
          },
          [&sink](uint64_t value) -> void {
            sink.Append(FormatUintConstant(value));
          },
          [&sink](double value) -> void {
            sink.Append(FormatDoubleConstant(value));
          },
          [&sink](const BytesConstant& value) -> void {
            sink.Append(FormatBytesConstant(value));
          },
          [&sink](const StringConstant& value) -> void {
            sink.Append(FormatStringConstant(value));
          },
          [&sink](absl::Duration value) -> void {
            sink.Append(FormatDurationConstant(value));
          },
          [&sink](absl::Time value) -> void {
            sink.Append(FormatTimestampConstant(value));
          }),
      constant.kind());
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_CONSTANT_H_
