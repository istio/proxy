// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_FUNCTION_RESULT_SET_H_
#define THIRD_PARTY_CEL_CPP_BASE_FUNCTION_RESULT_SET_H_

#include <initializer_list>
#include <utility>

#include "absl/container/btree_set.h"
#include "base/function_result.h"

namespace google::api::expr::runtime {
class AttributeUtility;
}  // namespace google::api::expr::runtime

namespace cel {

class UnknownValue;
namespace base_internal {
class UnknownSet;
}

// Represents a collection of unknown function results at a particular point in
// execution. Execution should advance further if this set of unknowns are
// provided. It may not advance if only a subset are provided.
// Set semantics use |IsEqualTo()| defined on |FunctionResult|.
class FunctionResultSet final {
 private:
  using Container = absl::btree_set<FunctionResult>;

 public:
  using value_type = typename Container::value_type;
  using size_type = typename Container::size_type;
  using iterator = typename Container::const_iterator;
  using const_iterator = typename Container::const_iterator;

  FunctionResultSet() = default;
  FunctionResultSet(const FunctionResultSet&) = default;
  FunctionResultSet(FunctionResultSet&&) = default;
  FunctionResultSet& operator=(const FunctionResultSet&) = default;
  FunctionResultSet& operator=(FunctionResultSet&&) = default;

  // Merge constructor -- effectively union(lhs, rhs).
  FunctionResultSet(const FunctionResultSet& lhs, const FunctionResultSet& rhs);

  // Initialize with a single FunctionResult.
  explicit FunctionResultSet(FunctionResult initial)
      : function_results_{std::move(initial)} {}

  FunctionResultSet(std::initializer_list<FunctionResult> il)
      : function_results_(il) {}

  iterator begin() const { return function_results_.begin(); }

  const_iterator cbegin() const { return function_results_.cbegin(); }

  iterator end() const { return function_results_.end(); }

  const_iterator cend() const { return function_results_.cend(); }

  size_type size() const { return function_results_.size(); }

  bool empty() const { return function_results_.empty(); }

  bool operator==(const FunctionResultSet& other) const {
    return this == &other || function_results_ == other.function_results_;
  }

  bool operator!=(const FunctionResultSet& other) const {
    return !operator==(other);
  }

 private:
  friend class google::api::expr::runtime::AttributeUtility;
  friend class UnknownValue;
  friend class base_internal::UnknownSet;

  void Add(const FunctionResult& function_result) {
    function_results_.insert(function_result);
  }

  void Add(const FunctionResultSet& other) {
    for (const auto& function_result : other) {
      Add(function_result);
    }
  }

  Container function_results_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_RESULT_SET_H_
