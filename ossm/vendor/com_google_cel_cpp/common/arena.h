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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_H_

#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "google/protobuf/arena.h"

namespace cel {

template <typename T = void>
struct ArenaTraits;

namespace common_internal {

template <typename T>
struct AssertArenaType : std::false_type {
  static_assert(!std::is_void_v<T>, "T must not be void");
  static_assert(!std::is_reference_v<T>, "T must not be a reference");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_array_v<T>, "T must not be an array");
};

template <typename, typename = void>
struct ArenaTraitsConstructible {
  using type = std::false_type;
};

template <typename T>
struct ArenaTraitsConstructible<
    T, std::void_t<decltype(ArenaTraits<T>::constructible)>> {
  using type = typename ArenaTraits<T>::constructible;
};

template <typename T>
std::enable_if_t<google::protobuf::Arena::is_arena_constructable<T>::value,
                 google::protobuf::Arena* absl_nullable>
GetArena(const T* absl_nullable ptr) {
  return ptr != nullptr ? ptr->GetArena() : nullptr;
}

template <typename T>
std::enable_if_t<!google::protobuf::Arena::is_arena_constructable<T>::value,
                 google::protobuf::Arena* absl_nullable>
GetArena([[maybe_unused]] const T* absl_nullable ptr) {
  return nullptr;
}

template <typename, typename = void>
struct HasArenaTraitsTriviallyDestructible : std::false_type {};

template <typename T>
struct HasArenaTraitsTriviallyDestructible<
    T, std::void_t<decltype(ArenaTraits<T>::trivially_destructible(
           std::declval<const T&>()))>> : std::true_type {};

}  // namespace common_internal

template <>
struct ArenaTraits<void> {
  template <typename U>
  using constructible = std::disjunction<
      typename common_internal::AssertArenaType<U>::type,
      typename common_internal::ArenaTraitsConstructible<U>::type>;

  template <typename U>
  using always_trivially_destructible =
      std::disjunction<typename common_internal::AssertArenaType<U>::type,
                       std::is_trivially_destructible<U>>;

  template <typename U>
  static bool trivially_destructible(const U& obj) {
    static_assert(!std::is_void_v<U>, "T must not be void");
    static_assert(!std::is_reference_v<U>, "T must not be a reference");
    static_assert(!std::is_volatile_v<U>, "T must not be volatile qualified");
    static_assert(!std::is_const_v<U>, "T must not be const qualified");
    static_assert(!std::is_array_v<U>, "T must not be an array");

    if constexpr (always_trivially_destructible<U>()) {
      return true;
    } else if constexpr (google::protobuf::Arena::is_destructor_skippable<U>::value) {
      return obj.GetArena() != nullptr;
    } else if constexpr (common_internal::HasArenaTraitsTriviallyDestructible<
                             U>::value) {
      return ArenaTraits<U>::trivially_destructible(obj);
    } else {
      return false;
    }
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_H_
