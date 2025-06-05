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

#ifndef THIRD_PARTY_CEL_CPP_OPTIONAL_REF_H_
#define THIRD_PARTY_CEL_CPP_OPTIONAL_REF_H_

#include <memory>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"

namespace cel {

// `optional_ref<T>` looks and feels like `absl::optional<T>`, but instead of
// owning the underlying value, it retains a reference to the value it accepts
// in its constructor.
template <typename T>
class optional_ref final {
 public:
  static_assert(!std::is_reference_v<T>, "T must not be a reference.");
  static_assert(!std::is_same_v<absl::nullopt_t, std::remove_cv_t<T>>,
                "optional_ref<absl::nullopt_t> is not allowed.");
  static_assert(!std::is_same_v<absl::in_place_t, std::remove_cv_t<T>>,
                "optional_ref<absl::in_place_t> is not allowed.");

  using value_type = T;

  optional_ref() = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr optional_ref(absl::nullopt_t) : optional_ref() {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr optional_ref(T& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(std::addressof(value)) {}

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::is_const<T>, std::is_same<std::decay_t<U>, std::decay_t<T>>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr optional_ref(
      const absl::optional<U>& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(value.has_value() ? std::addressof(*value) : nullptr) {}

  template <typename U, typename = std::enable_if_t<
                            std::is_same_v<std::decay_t<U>, std::decay_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr optional_ref(absl::optional<U>& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : value_(value.has_value() ? std::addressof(*value) : nullptr) {}

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>,
          std::is_convertible<std::add_pointer_t<U>, std::add_pointer_t<T>>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr optional_ref(const optional_ref<U>& other) : value_(other.value_) {}

  optional_ref(const optional_ref<T>&) = default;

  optional_ref<T>& operator=(const optional_ref<T>&) = delete;

  constexpr bool has_value() const { return value_ != nullptr; }

  constexpr explicit operator bool() const { return has_value(); }

  constexpr T& value() const {
    return ABSL_PREDICT_TRUE(has_value())
               ? *value_
               : (absl::optional<T>().value(), *value_);
  }

  constexpr T& operator*() const {
    ABSL_ASSERT(has_value());
    return *value_;
  }

  constexpr absl::Nonnull<T*> operator->() const {
    ABSL_ASSERT(has_value());
    return value_;
  }

 private:
  template <typename U>
  friend class optional_ref;

  T* const value_ = nullptr;
};

template <typename T>
optional_ref(const T&) -> optional_ref<const T>;

template <typename T>
optional_ref(T&) -> optional_ref<T>;

template <typename T>
optional_ref(const absl::optional<T>&) -> optional_ref<const T>;

template <typename T>
optional_ref(absl::optional<T>&) -> optional_ref<T>;

template <typename T>
constexpr bool operator==(const optional_ref<T>& lhs, absl::nullopt_t) {
  return !lhs.has_value();
}

template <typename T>
constexpr bool operator==(absl::nullopt_t, const optional_ref<T>& rhs) {
  return !rhs.has_value();
}

template <typename T>
constexpr bool operator!=(const optional_ref<T>& lhs, absl::nullopt_t) {
  return !operator==(lhs, absl::nullopt);
}

template <typename T>
constexpr bool operator!=(absl::nullopt_t, const optional_ref<T>& rhs) {
  return !operator==(absl::nullopt, rhs);
}

namespace common_internal {

template <typename T>
absl::optional<std::decay_t<T>> AsOptional(optional_ref<T> ref) {
  if (ref) {
    return *ref;
  }
  return absl::nullopt;
}

template <typename T>
absl::optional<T> AsOptional(absl::optional<T> opt) {
  return opt;
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_OPTIONAL_REF_H_
