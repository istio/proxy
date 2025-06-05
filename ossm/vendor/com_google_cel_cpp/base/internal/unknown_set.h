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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_UNKNOWN_SET_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_UNKNOWN_SET_H_

#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "base/attribute_set.h"
#include "base/function_result_set.h"

namespace cel::base_internal {

// For compatibility with the old API and to avoid unnecessary copying when
// converting between the old and new representations, we store the historical
// members of google::api::expr::runtime::UnknownSet in this struct for use with
// std::shared_ptr.
struct UnknownSetRep final {
  UnknownSetRep() = default;

  UnknownSetRep(AttributeSet attributes, FunctionResultSet function_results)
      : attributes(std::move(attributes)),
        function_results(std::move(function_results)) {}

  explicit UnknownSetRep(AttributeSet attributes)
      : attributes(std::move(attributes)) {}

  explicit UnknownSetRep(FunctionResultSet function_results)
      : function_results(std::move(function_results)) {}

  AttributeSet attributes;
  FunctionResultSet function_results;
};

const AttributeSet& EmptyAttributeSet();

const FunctionResultSet& EmptyFunctionResultSet();

struct UnknownSetAccess;

class UnknownSet final {
 private:
  using Rep = UnknownSetRep;

 public:
  // Construct the empty set.
  // Uses singletons instead of allocating new containers.
  UnknownSet() = default;

  UnknownSet(const UnknownSet&) = default;
  UnknownSet(UnknownSet&&) = default;
  UnknownSet& operator=(const UnknownSet&) = default;
  UnknownSet& operator=(UnknownSet&&) = default;

  // Initialization specifying subcontainers
  explicit UnknownSet(AttributeSet attributes)
      : rep_(std::make_shared<Rep>(std::move(attributes))) {}

  explicit UnknownSet(FunctionResultSet function_results)
      : rep_(std::make_shared<Rep>(std::move(function_results))) {}

  UnknownSet(AttributeSet attributes, FunctionResultSet function_results)
      : rep_(std::make_shared<Rep>(std::move(attributes),
                                   std::move(function_results))) {}

  // Merge constructor
  UnknownSet(const UnknownSet& set1, const UnknownSet& set2)
      : UnknownSet(
            AttributeSet(set1.unknown_attributes(), set2.unknown_attributes()),
            FunctionResultSet(set1.unknown_function_results(),
                              set2.unknown_function_results())) {}

  const AttributeSet& unknown_attributes() const {
    return rep_ != nullptr ? rep_->attributes : EmptyAttributeSet();
  }
  const FunctionResultSet& unknown_function_results() const {
    return rep_ != nullptr ? rep_->function_results : EmptyFunctionResultSet();
  }

  bool operator==(const UnknownSet& other) const {
    return this == &other ||
           (unknown_attributes() == other.unknown_attributes() &&
            unknown_function_results() == other.unknown_function_results());
  }

  bool operator!=(const UnknownSet& other) const { return !operator==(other); }

 private:
  friend struct UnknownSetAccess;

  explicit UnknownSet(std::shared_ptr<Rep> impl) : rep_(std::move(impl)) {}

  void Add(const UnknownSet& other) {
    if (rep_ == nullptr) {
      rep_ = std::make_shared<Rep>();
    }
    rep_->attributes.Add(other.unknown_attributes());
    rep_->function_results.Add(other.unknown_function_results());
  }

  std::shared_ptr<Rep> rep_;
};

struct UnknownSetAccess final {
  static UnknownSet Construct(std::shared_ptr<UnknownSetRep> rep) {
    return UnknownSet(std::move(rep));
  }

  static void Add(UnknownSet& dest, const UnknownSet& src) { dest.Add(src); }

  static const std::shared_ptr<UnknownSetRep>& Rep(const UnknownSet& value) {
    return value.rep_;
  }
};

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_UNKNOWN_SET_H_
