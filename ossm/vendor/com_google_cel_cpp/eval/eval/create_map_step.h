// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_MAP_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_MAP_STEP_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Creates an expression step that evaluates a create map expression.
//
// Deps must have an even number of elements, that alternate key, value pairs.
// (key1, value1, key2, value2...).
std::unique_ptr<DirectExpressionStep> CreateDirectCreateMapStep(
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id);

// Creates an `ExpressionStep` which performs `CreateStruct` for a map.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForMap(
    size_t entry_count, absl::flat_hash_set<int32_t> optional_indices,
    int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_MAP_STEP_H_
