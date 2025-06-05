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
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/allocator.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "google/protobuf/arena.h"

namespace cel {

class Value;
class ValueManager;
class ErrorValue;
class TypeManager;

// `ErrorValue` represents values of the `ErrorType`.
class ErrorValue final {
 public:
  static constexpr ValueKind kKind = ValueKind::kError;

  explicit ErrorValue(absl::Status value)
      : variant_(absl::in_place_type<absl::Status>, std::move(value)) {
    ABSL_DCHECK(*this) << "ErrorValue requires a non-OK absl::Status";
  }

  ErrorValue& operator=(absl::Status status) {
    variant_.emplace<absl::Status>(std::move(status));
    ABSL_DCHECK(*this) << "ErrorValue requires a non-OK absl::Status";
    return *this;
  }

  // By default, this creates an UNKNOWN error. You should always create a more
  // specific error value.
  ErrorValue();
  ErrorValue(const ErrorValue&) = default;
  ErrorValue(ErrorValue&&) = default;
  ErrorValue& operator=(const ErrorValue&) = default;
  ErrorValue& operator=(ErrorValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return ErrorType::kName; }

  std::string DebugString() const;

  // `SerializeTo` always returns `FAILED_PRECONDITION` as `ErrorValue` is not
  // serializable.
  absl::Status SerializeTo(AnyToJsonConverter&, absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& value_manager) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return false; }

  ErrorValue Clone(Allocator<> allocator) const;

  absl::Status NativeValue() const&;

  absl::Status NativeValue() &&;

  friend void swap(ErrorValue& lhs, ErrorValue& rhs) noexcept;

  explicit operator bool() const;

 private:
  using ArenaStatus = std::pair<absl::Nullable<google::protobuf::Arena*>,
                                absl::Nonnull<const absl::Status*>>;
  using Variant = absl::variant<absl::Status, ArenaStatus>;

  ErrorValue(absl::Nullable<google::protobuf::Arena*> arena,
             absl::Nonnull<const absl::Status*> status)
      : variant_(absl::in_place_type<ArenaStatus>, arena, status) {}

  explicit ErrorValue(const ArenaStatus& status)
      : ErrorValue(status.first, status.second) {}

  Variant variant_;
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

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_ERROR_VALUE_H_
