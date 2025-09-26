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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_NOOP_DELETE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_NOOP_DELETE_H_

#include <type_traits>

#include "absl/base/nullability.h"

namespace cel::internal {

// Like `std::default_delete`, except it does nothing.
template <typename T>
struct NoopDelete {
  static_assert(!std::is_function<T>::value,
                "NoopDelete cannot be instantiated for function types");

  constexpr NoopDelete() noexcept = default;
  constexpr NoopDelete(const NoopDelete<T>&) noexcept = default;

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<T, U>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr NoopDelete(const NoopDelete<U>&) noexcept {}

  constexpr void operator()(T* absl_nullable) const noexcept {
    static_assert(sizeof(T) >= 0, "cannot delete an incomplete type");
    static_assert(!std::is_void<T>::value, "cannot delete an incomplete type");
  }
};

template <typename T>
inline constexpr NoopDelete<T> NoopDeleteFor() noexcept {
  return NoopDelete<T>{};
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_NOOP_DELETE_H_
