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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_POOL_H_

#include <cstddef>
#include <cstring>
#include <tuple>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/die_if_null.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

// `OpaqueTypePool` is a thread unsafe interning factory for `OpaqueType`.
class OpaqueTypePool final {
 public:
  explicit OpaqueTypePool(absl::Nonnull<google::protobuf::Arena*> arena)
      : arena_(ABSL_DIE_IF_NULL(arena)) {}  // Crash OK

  // Returns a `OpaqueType` which has the provided parameters, interning as
  // necessary.
  OpaqueType InternOpaqueType(absl::string_view name,
                              absl::Span<const Type> parameters);

 private:
  using OpaqueTypeTuple = std::tuple<absl::string_view, absl::Span<const Type>>;

  static OpaqueTypeTuple AsTuple(const OpaqueType& opaque_type) {
    return AsTuple(opaque_type.name(), opaque_type.GetParameters());
  }

  static OpaqueTypeTuple AsTuple(absl::string_view name,
                                 absl::Span<const Type> parameters) {
    return OpaqueTypeTuple{name, parameters};
  }

  struct Hasher {
    using is_transparent = void;

    size_t operator()(const OpaqueType& data) const {
      return (*this)(AsTuple(data));
    }

    size_t operator()(const OpaqueTypeTuple& tuple) const {
      return absl::Hash<OpaqueTypeTuple>{}(tuple);
    }
  };

  struct Equaler {
    using is_transparent = void;

    bool operator()(const OpaqueType& lhs, const OpaqueType& rhs) const {
      return (*this)(AsTuple(lhs), AsTuple(rhs));
    }

    bool operator()(const OpaqueType& lhs, const OpaqueTypeTuple& rhs) const {
      return (*this)(AsTuple(lhs), rhs);
    }

    bool operator()(const OpaqueTypeTuple& lhs, const OpaqueType& rhs) const {
      return (*this)(lhs, AsTuple(rhs));
    }

    bool operator()(const OpaqueTypeTuple& lhs,
                    const OpaqueTypeTuple& rhs) const {
      return std::get<0>(lhs) == std::get<0>(rhs) &&
             absl::c_equal(std::get<1>(lhs), std::get<1>(rhs));
    }
  };

  absl::Nonnull<google::protobuf::Arena*> const arena_;
  absl::flat_hash_set<OpaqueType, Hasher, Equaler> opaque_types_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_POOL_H_
