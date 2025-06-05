/* Copyright 2016 Google Inc. All Rights Reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

// For inclusion in .h files. The real class definition is in
// simple_lru_cache_inl.h.

#pragma once

#include <functional>

#include "absl/container/flat_hash_map.h"  // for hash<>

namespace google {
namespace simple_lru_cache {

namespace internal {
template <typename T>
struct SimpleLRUHash : public std::hash<T> {};
}  // namespace internal

template <typename Key, typename Value,
          typename H = internal::SimpleLRUHash<Key>,
          typename EQ = std::equal_to<Key>>
class SimpleLRUCache;

// Deleter is a functor that defines how to delete a Value*. That is, it
// contains a public method:
//  operator() (Value* value)
// See example in the associated unittest.
template <typename Key, typename Value, typename Deleter,
          typename H = internal::SimpleLRUHash<Key>,
          typename EQ = std::equal_to<Key>>
class SimpleLRUCacheWithDeleter;

}  // namespace simple_lru_cache
}  // namespace google