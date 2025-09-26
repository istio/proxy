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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_TYPE_POOL_H_

#include <cstddef>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

// `TypeTypePool` is a thread unsafe interning factory for `TypeType`.
class TypeTypePool final {
 public:
  explicit TypeTypePool(google::protobuf::Arena* absl_nonnull arena)
      : arena_(ABSL_DIE_IF_NULL(arena)) {}  // Crash OK

  // Returns a `TypeType` which has the provided parameters, interning as
  // necessary.
  TypeType InternTypeType(const Type& type);

 private:
  struct Hasher {
    using is_transparent = void;

    size_t operator()(const TypeType& type_type) const {
      ABSL_DCHECK_EQ(type_type.GetParameters().size(), 1);
      return (*this)(type_type.GetParameters().front());
    }

    size_t operator()(const Type& type) const {
      return absl::Hash<Type>{}(type);
    }
  };

  struct Equaler {
    using is_transparent = void;

    bool operator()(const TypeType& lhs, const TypeType& rhs) const {
      ABSL_DCHECK_EQ(lhs.GetParameters().size(), 1);
      ABSL_DCHECK_EQ(rhs.GetParameters().size(), 1);
      return (*this)(lhs.GetParameters().front(), rhs.GetParameters().front());
    }

    bool operator()(const TypeType& lhs, const Type& rhs) const {
      ABSL_DCHECK_EQ(lhs.GetParameters().size(), 1);
      return (*this)(lhs.GetParameters().front(), rhs);
    }

    bool operator()(const Type& lhs, const TypeType& rhs) const {
      ABSL_DCHECK_EQ(rhs.GetParameters().size(), 1);
      return (*this)(lhs, rhs.GetParameters().front());
    }

    bool operator()(const Type& lhs, const Type& rhs) const {
      return lhs == rhs;
    }
  };

  google::protobuf::Arena* absl_nonnull const arena_;
  absl::flat_hash_set<TypeType, Hasher, Equaler> type_types_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_TYPE_POOL_H_
