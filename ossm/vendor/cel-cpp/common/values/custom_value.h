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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_CUSTOM_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_CUSTOM_VALUE_H_

#include <cstddef>
#include <cstring>
#include <memory>
#include <type_traits>

namespace cel {

// CustomValueContent is an opaque 16-byte trivially copyable value. The format
// of the data stored within is unknown to everything except the the caller
// which creates it. Do not try to interpret it otherwise.
class CustomValueContent final {
 public:
  static CustomValueContent Zero() {
    CustomValueContent content;
    std::memset(&content, 0, sizeof(content));
    return content;
  }

  template <typename T>
  static CustomValueContent From(T value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert(sizeof(T) <= 16, "sizeof(T) must be no greater than 16");

    CustomValueContent content;
    std::memcpy(content.raw_, std::addressof(value), sizeof(T));
    return content;
  }

  template <typename T, size_t N>
  static CustomValueContent From(const T (&array)[N]) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert((sizeof(T) * N) <= 16,
                  "sizeof(T[N]) must be no greater than 16");

    CustomValueContent content;
    std::memcpy(content.raw_, array, sizeof(T) * N);
    return content;
  }

  template <typename T>
  T To() const {
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable");
    static_assert(sizeof(T) <= 16, "sizeof(T) must be no greater than 16");

    T value;
    std::memcpy(std::addressof(value), raw_, sizeof(T));
    return value;
  }

 private:
  alignas(void*) std::byte raw_[16];
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_CUSTOM_VALUE_H_
