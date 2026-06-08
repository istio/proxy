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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_

#include <cstddef>
#include <memory>
#include <new>
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
#include "common/arena.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/zero_copy_stream.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;

// `ErrorValue` represents values of the `ErrorType`.
class ABSL_ATTRIBUTE_TRIVIAL_ABI ErrorValue final
    : private common_internal::ValueMixin<ErrorValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kError;

  explicit ErrorValue(absl::Status value) : arena_(nullptr) {
    ::new (static_cast<void*>(&status_.val[0])) absl::Status(std::move(value));
    ABSL_DCHECK(*this) << "ErrorValue requires a non-OK absl::Status";
  }

  // By default, this creates an UNKNOWN error. You should always create a more
  // specific error value.
  ErrorValue();

  ErrorValue(const ErrorValue& other) { CopyConstruct(other); }

  ErrorValue(ErrorValue&& other) noexcept { MoveConstruct(other); }

  ~ErrorValue() { Destruct(); }

  ErrorValue& operator=(const ErrorValue& other) {
    if (this != &other) {
      Destruct();
      CopyConstruct(other);
    }
    return *this;
  }

  ErrorValue& operator=(ErrorValue&& other) noexcept {
    if (this != &other) {
      Destruct();
      MoveConstruct(other);
    }
    return *this;
  }

  static constexpr ValueKind kind() { return kKind; }

  static absl::string_view GetTypeName() { return ErrorType::kName; }

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

  bool IsZeroValue() const { return false; }

  ErrorValue Clone(google::protobuf::Arena* absl_nonnull arena) const;

  absl::Status ToStatus() const&;

  absl::Status ToStatus() &&;

  ABSL_DEPRECATED("Use ToStatus()")
  absl::Status NativeValue() const& { return ToStatus(); }

  ABSL_DEPRECATED("Use ToStatus()")
  absl::Status NativeValue() && { return std::move(*this).ToStatus(); }

  friend void swap(ErrorValue& lhs, ErrorValue& rhs) noexcept;

  explicit operator bool() const;

 private:
  friend class common_internal::ValueMixin<ErrorValue>;
  friend struct ArenaTraits<ErrorValue>;

  ErrorValue(google::protobuf::Arena* absl_nonnull arena,
             const absl::Status* absl_nonnull status)
      : arena_(arena), status_{.ptr = status} {}

  void CopyConstruct(const ErrorValue& other) {
    arena_ = other.arena_;
    if (arena_ == nullptr) {
      ::new (static_cast<void*>(&status_.val[0])) absl::Status(*std::launder(
          reinterpret_cast<const absl::Status*>(&other.status_.val[0])));
    } else {
      status_.ptr = other.status_.ptr;
    }
  }

  void MoveConstruct(ErrorValue& other) {
    arena_ = other.arena_;
    if (arena_ == nullptr) {
      ::new (static_cast<void*>(&status_.val[0]))
          absl::Status(std::move(*std::launder(
              reinterpret_cast<absl::Status*>(&other.status_.val[0]))));
    } else {
      status_.ptr = other.status_.ptr;
    }
  }

  void Destruct() {
    if (arena_ == nullptr) {
      std::launder(reinterpret_cast<absl::Status*>(&status_.val[0]))->~Status();
    }
  }

  google::protobuf::Arena* absl_nullable arena_;
  union {
    alignas(absl::Status) char val[sizeof(absl::Status)];
    const absl::Status* absl_nonnull ptr;
  } status_;
};

ErrorValue NoSuchFieldError(absl::string_view field);

ErrorValue NoSuchKeyError(absl::string_view key);

ErrorValue NoSuchTypeError(absl::string_view type);

ErrorValue DuplicateKeyError();

ErrorValue TypeConversionError(absl::string_view from, absl::string_view to);

ErrorValue TypeConversionError(const Type& from, const Type& to);

ErrorValue IndexOutOfBoundsError(size_t index);

ErrorValue IndexOutOfBoundsError(ptrdiff_t index);

// Catch other integrals and forward them to the above ones. This is needed to
// avoid ambiguous overload issues for smaller integral types like `int`.
template <typename T>
std::enable_if_t<std::conjunction_v<std::is_integral<T>, std::is_unsigned<T>,
                                    std::negation<std::is_same<T, size_t>>>,
                 ErrorValue>
IndexOutOfBoundsError(T index) {
  static_assert(sizeof(T) <= sizeof(size_t));
  return IndexOutOfBoundsError(static_cast<size_t>(index));
}
template <typename T>
std::enable_if_t<std::conjunction_v<std::is_integral<T>, std::is_signed<T>,
                                    std::negation<std::is_same<T, ptrdiff_t>>>,
                 ErrorValue>
IndexOutOfBoundsError(T index) {
  static_assert(sizeof(T) <= sizeof(ptrdiff_t));
  return IndexOutOfBoundsError(static_cast<ptrdiff_t>(index));
}

inline std::ostream& operator<<(std::ostream& out, const ErrorValue& value) {
  return out << value.DebugString();
}

bool IsNoSuchField(const ErrorValue& value);

bool IsNoSuchKey(const ErrorValue& value);

class ErrorValueReturn final {
 public:
  ErrorValueReturn() = default;

  ErrorValue operator()(absl::Status status) const {
    return ErrorValue(std::move(status));
  }
};

namespace common_internal {

struct ImplicitlyConvertibleStatus {
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::Status() const { return absl::OkStatus(); }

  template <typename T>
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator absl::StatusOr<T>() const {
    return T();
  }
};

}  // namespace common_internal

// For use with `RETURN_IF_ERROR(...).With(cel::ErrorValueAssign(&result))` and
// `ASSIGN_OR_RETURN(..., ..., _.With(cel::ErrorValueAssign(&result)))`.
//
// IMPORTANT:
// If the returning type is `absl::Status` the result will be
// `absl::OkStatus()`. If the returning type is `absl::StatusOr<T>` the result
// will be `T()`.
class ErrorValueAssign final {
 public:
  ErrorValueAssign() = delete;

  explicit ErrorValueAssign(Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ErrorValueAssign(std::addressof(value)) {}

  explicit ErrorValueAssign(
      Value* absl_nonnull value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(value) {
    ABSL_DCHECK(value != nullptr);
  }

  common_internal::ImplicitlyConvertibleStatus operator()(
      absl::Status status) const;

 private:
  Value* absl_nonnull value_;
};

template <>
struct ArenaTraits<ErrorValue> {
  static bool trivially_destructible(const ErrorValue& value) {
    return value.arena_ != nullptr;
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_
