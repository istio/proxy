// Copyright 2023 Google LLC
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
// Program steps for lazily initialized aliases (e.g. cel.bind).
//
// When used, any reference to variable should be replaced with a conditional
// step that either runs the initialization routine or pushes the already
// initialized variable to the stack.
//
// All references to the variable should be replaced with:
//
// +-----------------+-------------------+--------------------+
// |    stack        |       pc          |    step            |
// +-----------------+-------------------+--------------------+
// |    {}           |       0           | check init slot(i) |
// +-----------------+-------------------+--------------------+
// |    {value}      |       1           | assign slot(i)     |
// +-----------------+-------------------+--------------------+
// |    {value}      |       2           | <expr using value> |
// +-----------------+-------------------+--------------------+
// |                  ....                                    |
// +-----------------+-------------------+--------------------+
// |    {...}        | n (end of scope)  | clear slot(i)      |
// +-----------------+-------------------+--------------------+

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_LAZY_INIT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_LAZY_INIT_STEP_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/base/nullability.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Creates a step representing a Bind expression.
std::unique_ptr<DirectExpressionStep> CreateDirectBindStep(
    size_t slot_index, std::unique_ptr<DirectExpressionStep> expression,
    int64_t expr_id);

// Creates a step representing a cel.@block expression.
std::unique_ptr<DirectExpressionStep> CreateDirectBlockStep(
    size_t slot_index, size_t slot_count,
    std::unique_ptr<DirectExpressionStep> expression, int64_t expr_id);

// Creates a direct step representing accessing a lazily evaluated alias from
// a bind or block.
std::unique_ptr<DirectExpressionStep> CreateDirectLazyInitStep(
    size_t slot_index, const DirectExpressionStep* absl_nonnull subexpression,
    int64_t expr_id);

// Creates a step representing accessing a lazily evaluated alias from
// a bind or block.
std::unique_ptr<ExpressionStep> CreateLazyInitStep(size_t slot_index,
                                                   size_t subexpression_index,
                                                   int64_t expr_id);

// Helper step to assign a slot value from the top of stack on initialization.
std::unique_ptr<ExpressionStep> CreateAssignSlotAndPopStep(size_t slot_index);

// Helper step to clear a slot.
// Slots may be reused in different contexts so need to be cleared after a
// context is done.
std::unique_ptr<ExpressionStep> CreateClearSlotStep(size_t slot_index,
                                                    int64_t expr_id);

std::unique_ptr<ExpressionStep> CreateClearSlotsStep(size_t slot_index,
                                                     size_t slot_count,
                                                     int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_LAZY_INIT_STEP_H_
