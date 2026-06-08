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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_TO_ADDRESS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_TO_ADDRESS_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"

namespace cel::internal {

// -----------------------------------------------------------------------------
// Function Template: to_address()
// -----------------------------------------------------------------------------
//
// Backport of std::to_address introduced in C++20. Enables obtaining the
// address of an object regardless of whether the pointer is raw or fancy.
#if defined(__cpp_lib_to_address) && __cpp_lib_to_address >= 201711L
using std::to_address;
#else
template <typename T>
constexpr T* to_address(T* ptr) noexcept {
  static_assert(!std::is_function<T>::value, "T must not be a function");
  return ptr;
}

template <typename T, typename = void>
struct PointerTraitsToAddress {
  static constexpr auto Dispatch(
      const T& p ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return internal::to_address(p.operator->());
  }
};

template <typename T>
struct PointerTraitsToAddress<
    T, absl::void_t<decltype(std::pointer_traits<T>::to_address(
           std::declval<const T&>()))> > {
  static constexpr auto Dispatch(
      const T& p ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    return std::pointer_traits<T>::to_address(p);
  }
};

template <typename T>
constexpr auto to_address(const T& ptr ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return PointerTraitsToAddress<T>::Dispatch(ptr);
}
#endif

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_TO_ADDRESS_H_
