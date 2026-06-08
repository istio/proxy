#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_LIST_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_LIST_STEP_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "common/expr.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for CreateList that evaluates recursively.
std::unique_ptr<DirectExpressionStep> CreateDirectListStep(
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    absl::flat_hash_set<int32_t> optional_indices, int64_t expr_id);

// Factory method for CreateList which constructs an immutable list.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateListStep(
    const cel::ListExpr& create_list_expr, int64_t expr_id);

// Factory method for CreateList which constructs a mutable list.
//
// This is intended for the list construction step is generated for a
// list-building comprehension (rather than a user authored expression).
std::unique_ptr<ExpressionStep> CreateMutableListStep(int64_t expr_id);

// Factory method for CreateList which constructs a mutable list.
//
// This is intended for the list construction step is generated for a
// list-building comprehension (rather than a user authored expression).
std::unique_ptr<DirectExpressionStep> CreateDirectMutableListStep(
    int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_LIST_STEP_H_
