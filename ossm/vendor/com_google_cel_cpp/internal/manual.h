// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_MANUAL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_MANUAL_H_

#include <new>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"

namespace cel::internal {

template <typename T>
class Manual final {
 public:
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_array_v<T>, "T must not be an array");
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");

  using element_type = T;

  Manual() = default;

  Manual(const Manual&) = delete;
  Manual(Manual&&) = delete;

  ~Manual() = default;

  Manual& operator=(const Manual&) = delete;
  Manual& operator=(Manual&&) = delete;

  constexpr T* absl_nonnull get() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::launder(reinterpret_cast<T*>(&storage_[0]));
  }

  constexpr const T* absl_nonnull get() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return std::launder(reinterpret_cast<const T*>(&storage_[0]));
  }

  constexpr T& operator*() ABSL_ATTRIBUTE_LIFETIME_BOUND { return *get(); }

  constexpr const T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return *get();
  }

  constexpr T* absl_nonnull operator->() ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get();
  }

  constexpr const T* absl_nonnull operator->() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return get();
  }

  template <typename... Args>
  T* absl_nonnull Construct(Args&&... args) ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return ::new (static_cast<void*>(&storage_[0]))
        T(std::forward<Args>(args)...);
  }

  T* absl_nonnull DefaultConstruct() {
    return ::new (static_cast<void*>(&storage_[0])) T;
  }

  T* absl_nonnull ValueConstruct() {
    return ::new (static_cast<void*>(&storage_[0])) T();
  }

  void Destruct() { get()->~T(); }

 private:
  alignas(T) char storage_[sizeof(T)];
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MANUAL_H_
