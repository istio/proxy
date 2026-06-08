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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EQUALITY_STEPS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EQUALITY_STEPS_H_

#include <cstdint>
#include <memory>

#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for recursive _==_/_!=_ Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectEqualityStep(
    std::unique_ptr<DirectExpressionStep> lhs,
    std::unique_ptr<DirectExpressionStep> rhs, bool negation, int64_t expr_id);

// Factory method for iterative _==_/_!=_ Execution step
std::unique_ptr<ExpressionStep> CreateEqualityStep(bool negation,
                                                   int64_t expr_id);

// Factory method for recursive @in Execution step
std::unique_ptr<DirectExpressionStep> CreateDirectInStep(
    std::unique_ptr<DirectExpressionStep> item,
    std::unique_ptr<DirectExpressionStep> container, int64_t expr_id);

// Factory method for iterative @in Execution step
std::unique_ptr<ExpressionStep> CreateInStep(int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EQUALITY_STEPS_H_
