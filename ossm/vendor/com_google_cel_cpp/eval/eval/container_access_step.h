#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONTAINER_ACCESS_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONTAINER_ACCESS_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "common/expr.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

std::unique_ptr<DirectExpressionStep> CreateDirectContainerAccessStep(
    std::unique_ptr<DirectExpressionStep> container_step,
    std::unique_ptr<DirectExpressionStep> key_step, bool enable_optional_types,
    int64_t expr_id);

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const cel::CallExpr& call, int64_t expr_id,
    bool enable_optional_types = false);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONTAINER_ACCESS_STEP_H_
