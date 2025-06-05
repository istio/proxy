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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_FUNCTION_TYPE_POOL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_FUNCTION_TYPE_POOL_H_

#include <cstddef>
#include <cstring>
#include <functional>
#include <tuple>

#include "absl/algorithm/container.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/log/die_if_null.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {

// `FunctionTypePool` is a thread unsafe interning factory for `FunctionType`.
class FunctionTypePool final {
 public:
  explicit FunctionTypePool(absl::Nonnull<google::protobuf::Arena*> arena)
      : arena_(ABSL_DIE_IF_NULL(arena)) {}  // Crash OK

  // Returns a `FunctionType` which has the provided parameters, interning as
  // necessary.
  FunctionType InternFunctionType(const Type& result,
                                  absl::Span<const Type> args);

 private:
  using FunctionTypeTuple =
      std::tuple<std::reference_wrapper<const Type>, absl::Span<const Type>>;

  static FunctionTypeTuple AsTuple(const FunctionType& function_type) {
    return AsTuple(function_type.result(), function_type.args());
  }

  static FunctionTypeTuple AsTuple(const Type& result,
                                   absl::Span<const Type> args) {
    return FunctionTypeTuple{std::cref(result), args};
  }

  struct Hasher {
    using is_transparent = void;

    size_t operator()(const FunctionType& data) const {
      return (*this)(AsTuple(data));
    }

    size_t operator()(const FunctionTypeTuple& tuple) const {
      return absl::Hash<FunctionTypeTuple>{}(tuple);
    }
  };

  struct Equaler {
    using is_transparent = void;

    bool operator()(const FunctionType& lhs, const FunctionType& rhs) const {
      return (*this)(AsTuple(lhs), AsTuple(rhs));
    }

    bool operator()(const FunctionType& lhs,
                    const FunctionTypeTuple& rhs) const {
      return (*this)(AsTuple(lhs), rhs);
    }

    bool operator()(const FunctionTypeTuple& lhs,
                    const FunctionType& rhs) const {
      return (*this)(lhs, AsTuple(rhs));
    }

    bool operator()(const FunctionTypeTuple& lhs,
                    const FunctionTypeTuple& rhs) const {
      return std::get<0>(lhs) == std::get<0>(rhs) &&
             absl::c_equal(std::get<1>(lhs), std::get<1>(rhs));
    }
  };

  absl::Nonnull<google::protobuf::Arena*> const arena_;
  absl::flat_hash_set<FunctionType, Hasher, Equaler> function_types_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_FUNCTION_TYPE_POOL_H_
