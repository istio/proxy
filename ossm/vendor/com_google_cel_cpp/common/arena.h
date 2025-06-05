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

#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "google/protobuf/arena.h"

namespace cel {

template <typename T>
using IsArenaConstructible = google::protobuf::Arena::is_arena_constructable<T>;

template <typename T>
using IsArenaDestructorSkippable =
    absl::conjunction<IsArenaConstructible<T>,
                      google::protobuf::Arena::is_destructor_skippable<T>>;

namespace common_internal {

template <typename T>
std::enable_if_t<IsArenaConstructible<T>::value, absl::Nullable<google::protobuf::Arena*>>
GetArena(const T* ptr) {
  return ptr != nullptr ? ptr->GetArena() : nullptr;
}

template <typename T>
std::enable_if_t<!IsArenaConstructible<T>::value,
                 absl::Nullable<google::protobuf::Arena*>>
GetArena([[maybe_unused]] const T* ptr) {
  return nullptr;
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_H_
