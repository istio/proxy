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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_POOL_H_

#include <cstddef>
#include <functional>
#include <tuple>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/die_if_null.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

// `MapTypePool` is a thread unsafe interning factory for `MapType`.
class MapTypePool final {
 public:
  explicit MapTypePool(absl::Nonnull<google::protobuf::Arena*> arena)
      : arena_(ABSL_DIE_IF_NULL(arena)) {}  // Crash OK

  // Returns a `MapType` which has the provided parameters, interning as
  // necessary.
  MapType InternMapType(const Type& key, const Type& value);

 private:
  using MapTypeTuple = std::tuple<std::reference_wrapper<const Type>,
                                  std::reference_wrapper<const Type>>;

  static MapTypeTuple AsTuple(const MapType& map_type) {
    return AsTuple(map_type.key(), map_type.value());
  }

  static MapTypeTuple AsTuple(const Type& key, const Type& value) {
    return MapTypeTuple{std::cref(key), std::cref(value)};
  }

  struct Hasher {
    using is_transparent = void;

    size_t operator()(const MapType& map_type) const {
      return (*this)(AsTuple(map_type));
    }

    size_t operator()(const MapTypeTuple& tuple) const {
      return absl::Hash<MapTypeTuple>{}(tuple);
    }
  };

  struct Equaler {
    using is_transparent = void;

    bool operator()(const MapType& lhs, const MapType& rhs) const {
      return (*this)(AsTuple(lhs), AsTuple(rhs));
    }

    bool operator()(const MapType& lhs, const MapTypeTuple& rhs) const {
      return (*this)(AsTuple(lhs), rhs);
    }

    bool operator()(const MapTypeTuple& lhs, const MapType& rhs) const {
      return (*this)(lhs, AsTuple(rhs));
    }

    bool operator()(const MapTypeTuple& lhs, const MapTypeTuple& rhs) const {
      return lhs == rhs;
    }
  };

  absl::Nonnull<google::protobuf::Arena*> const arena_;
  absl::flat_hash_set<MapType, Hasher, Equaler> map_types_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_POOL_H_
