// Copyright 2025 Google LLC
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
//
// Factory functions for creating typical type checker library subsets.

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_SUBSET_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_SUBSET_FACTORY_H_

#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "checker/type_checker_builder.h"

namespace cel {

// Subsets a type checker library to only include the given overload ids.
TypeCheckerSubset::FunctionPredicate IncludeOverloadsByIdPredicate(
    absl::flat_hash_set<std::string> overload_ids);

TypeCheckerSubset::FunctionPredicate IncludeOverloadsByIdPredicate(
    absl::Span<const absl::string_view> overload_ids);

// Subsets a type checker library to exclude the given overload ids.
TypeCheckerSubset::FunctionPredicate ExcludeOverloadsByIdPredicate(
    absl::flat_hash_set<std::string> overload_ids);

TypeCheckerSubset::FunctionPredicate ExcludeOverloadsByIdPredicate(
    absl::Span<const absl::string_view> overload_ids);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_SUBSET_FACTORY_H_
