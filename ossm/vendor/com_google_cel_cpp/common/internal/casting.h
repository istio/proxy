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

// IWYU pragma: private, include "common/casting.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_CASTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_CASTING_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"
#include "absl/types/optional.h"
#include "internal/casts.h"

namespace cel {

namespace common_internal {

template <typename To, typename From>
using propagate_const_t =
    std::conditional_t<std::is_const_v<std::remove_reference_t<From>>,
                       std::add_const_t<To>, To>;

template <typename To, typename From>
using propagate_volatile_t =
    std::conditional_t<std::is_volatile_v<std::remove_reference_t<From>>,
                       std::add_volatile_t<To>, To>;

template <typename To, typename From>
using propagate_reference_t =
    std::conditional_t<std::is_lvalue_reference_v<From>,
                       std::add_lvalue_reference_t<To>,
                       std::conditional_t<std::is_rvalue_reference_v<From>,
                                          std::add_rvalue_reference_t<To>, To>>;

template <typename To, typename From>
using propagate_cvref_t = propagate_reference_t<
    propagate_volatile_t<propagate_const_t<To, From>, From>, From>;

}  // namespace common_internal

namespace common_internal {

// Implementation of `cel::InstanceOf`.
template <typename To>
struct ABSL_DEPRECATED("Use Is member functions instead.")
    InstanceOfImpl final {
  static_assert(!std::is_pointer_v<To>, "To must not be a pointer");
  static_assert(!std::is_array_v<To>, "To must not be an array");
  static_assert(!std::is_lvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_rvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_const_v<To>, "To must not be const qualified");
  static_assert(!std::is_volatile_v<To>, "To must not be volatile qualified");
  static_assert(std::is_class_v<To>, "To must be a non-union class");

  explicit InstanceOfImpl() = default;

  template <typename From>
  ABSL_DEPRECATED("Use Is member functions instead.")
  ABSL_MUST_USE_RESULT bool operator()(const From& from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    if constexpr (std::is_same_v<absl::remove_cvref_t<From>, To>) {
      // Same type. Separate from the next `else if` to work on in-complete
      // types.
      return true;
    } else if constexpr (std::is_polymorphic_v<To> &&
                         std::is_polymorphic_v<absl::remove_cvref_t<From>> &&
                         std::is_base_of_v<To, absl::remove_cvref_t<From>>) {
      // Polymorphic upcast.
      return true;
    } else if constexpr (!std::is_polymorphic_v<To> &&
                         !std::is_polymorphic_v<absl::remove_cvref_t<From>> &&
                         (std::is_convertible_v<const From&, To> ||
                          std::is_convertible_v<From&, To> ||
                          std::is_convertible_v<const From&&, To> ||
                          std::is_convertible_v<From&&, To>)) {
      // Implicitly convertible.
      return true;
    } else {
      // Something else.
      return from.template Is<To>();
    }
  }

  template <typename From>
  ABSL_DEPRECATED("Use Is member functions instead.")
  ABSL_MUST_USE_RESULT bool operator()(const From* from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    return from != nullptr && (*this)(*from);
  }
};

// Implementation of `cel::Cast`.
template <typename To>
struct ABSL_DEPRECATED(
    "Use explicit conversion functions instead through static_cast.")
    CastImpl final {
  static_assert(!std::is_pointer_v<To>, "To must not be a pointer");
  static_assert(!std::is_array_v<To>, "To must not be an array");
  static_assert(!std::is_lvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_rvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_const_v<To>, "To must not be const qualified");
  static_assert(!std::is_volatile_v<To>, "To must not be volatile qualified");
  static_assert(std::is_class_v<To>, "To must be a non-union class");

  explicit CastImpl() = default;

  template <typename From>
  ABSL_DEPRECATED(
      "Use explicit conversion functions instead through static_cast.")
  ABSL_MUST_USE_RESULT decltype(auto)
  operator()(From&& from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<absl::remove_cvref_t<From>>,
                  "From must be a non-union class");
    if constexpr (std::is_polymorphic_v<From>) {
      static_assert(std::is_lvalue_reference_v<From>,
                    "polymorphic casts are only possible on lvalue references");
    }
    if constexpr (std::is_same_v<absl::remove_cvref_t<From>, To>) {
      // Same type. Separate from the next `else if` to work on in-complete
      // types.
      return static_cast<propagate_cvref_t<To, From>>(from);
    } else if constexpr (std::is_polymorphic_v<To> &&
                         std::is_polymorphic_v<absl::remove_cvref_t<From>> &&
                         std::is_base_of_v<To, absl::remove_cvref_t<From>>) {
      // Polymorphic upcast.
      return static_cast<propagate_cvref_t<To, From>>(from);
    } else if constexpr (std::is_polymorphic_v<To> &&
                         std::is_polymorphic_v<absl::remove_cvref_t<From>> &&
                         std::is_base_of_v<absl::remove_cvref_t<From>, To>) {
      // Polymorphic downcast.
      return cel::internal::down_cast<propagate_cvref_t<To, From>>(
          std::forward<From>(from));
    } else if constexpr (std::is_convertible_v<From, To> &&
                         !std::is_polymorphic_v<To> &&
                         !std::is_polymorphic_v<absl::remove_cvref_t<From>>) {
      return static_cast<To>(std::forward<From>(from));
    } else {
      // Something else.
      return std::forward<From>(from).template Get<To>();
    }
  }

  template <typename From>
  ABSL_DEPRECATED(
      "Use explicit conversion functions instead through static_cast.")
  ABSL_MUST_USE_RESULT decltype(auto)
  operator()(From* from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    using R = decltype((*this)(*from));
    static_assert(std::is_lvalue_reference_v<R>);
    if (from == nullptr) {
      return static_cast<std::add_pointer_t<std::remove_reference_t<R>>>(
          nullptr);
    }
    return static_cast<std::add_pointer_t<std::remove_reference_t<R>>>(
        std::addressof((*this)(*from)));
  }
};

// Implementation of `cel::As`.
template <typename To>
struct ABSL_DEPRECATED("Use As member functions instead.") AsImpl final {
  static_assert(!std::is_pointer_v<To>, "To must not be a pointer");
  static_assert(!std::is_array_v<To>, "To must not be an array");
  static_assert(!std::is_lvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_rvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_const_v<To>, "To must not be const qualified");
  static_assert(!std::is_volatile_v<To>, "To must not be volatile qualified");
  static_assert(std::is_class_v<To>, "To must be a non-union class");

  explicit AsImpl() = default;

  template <typename From>
  ABSL_DEPRECATED("Use As member functions instead.")
  ABSL_MUST_USE_RESULT decltype(auto) operator()(From&& from) const {
    // Returns either `absl::optional` or `cel::optional_ref`
    // depending on the return type of `CastTraits::Convert`. The use of these
    // two types is an implementation detail.
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<absl::remove_cvref_t<From>>,
                  "From must be a non-union class");
    return std::forward<From>(from).template As<To>();
  }

  // Returns a pointer.
  template <typename From>
  ABSL_DEPRECATED("Use As member functions instead.")
  ABSL_MUST_USE_RESULT decltype(auto) operator()(From* from) const {
    // Returns either `absl::optional` or `To*` depending on the return type of
    // `CastTraits::Convert`. The use of these two types is an implementation
    // detail.
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    using R = decltype(from->template As<To>());
    if (from == nullptr) {
      return R{absl::nullopt};
    }
    return from->template As<To>();
  }
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_CASTING_H_
