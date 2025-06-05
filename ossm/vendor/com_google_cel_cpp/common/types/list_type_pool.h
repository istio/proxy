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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_LIST_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_LIST_TYPE_POOL_H_

#include <cstddef>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/die_if_null.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

// `ListTypePool` is a thread unsafe interning factory for `ListType`.
class ListTypePool final {
 public:
  explicit ListTypePool(absl::Nonnull<google::protobuf::Arena*> arena)
      : arena_(ABSL_DIE_IF_NULL(arena)) {}  // Crash OK

  // Returns a `ListType` which has the provided parameters, interning as
  // necessary.
  ListType InternListType(const Type& element);

 private:
  struct Hasher {
    using is_transparent = void;

    size_t operator()(const ListType& list_type) const {
      return (*this)(list_type.element());
    }

    size_t operator()(const Type& type) const {
      return absl::Hash<Type>{}(type);
    }
  };

  struct Equaler {
    using is_transparent = void;

    bool operator()(const ListType& lhs, const ListType& rhs) const {
      return (*this)(lhs.element(), rhs.element());
    }

    bool operator()(const ListType& lhs, const Type& rhs) const {
      return (*this)(lhs.element(), rhs);
    }

    bool operator()(const Type& lhs, const ListType& rhs) const {
      return (*this)(lhs, rhs.element());
    }

    bool operator()(const Type& lhs, const Type& rhs) const {
      return lhs == rhs;
    }
  };

  absl::Nonnull<google::protobuf::Arena*> const arena_;
  absl::flat_hash_set<ListType, Hasher, Equaler> list_types_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_LIST_TYPE_POOL_H_
