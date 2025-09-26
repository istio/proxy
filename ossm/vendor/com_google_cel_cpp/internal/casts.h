// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_CASTS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_CASTS_H_

#include <cassert>
#include <memory>
#include <type_traits>

namespace cel::internal {

template <typename To, typename From>
To down_cast(From* from) {
  static_assert(std::is_pointer_v<To>, "Target type not a pointer.");
  static_assert((std::is_base_of_v<From, std::remove_pointer_t<To>>),
                "Target type not derived from source type.");
#if !defined(__GNUC__) || defined(__GXX_RTTI)
  assert(from == nullptr || dynamic_cast<To>(from) != nullptr);
#endif
  return static_cast<To>(from);
}

template <typename To, typename From>
To down_cast(From& from) {
  static_assert(std::is_lvalue_reference_v<To>,
                "Target type not a lvalue reference.");
  static_assert((std::is_base_of_v<From, std::remove_reference_t<To>>),
                "Target type not derived from source type.");
#if !defined(__GNUC__) || defined(__GXX_RTTI)
  assert(dynamic_cast<std::add_pointer_t<std::remove_reference_t<To>>>(
             std::addressof(from)) != nullptr);
#endif
  return static_cast<To>(from);
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_CASTS_H_
