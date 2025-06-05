#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_TERNARY_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_TERNARY_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for ternary (_?_:_) recursive execution step
std::unique_ptr<DirectExpressionStep> CreateDirectTernaryStep(
    std::unique_ptr<DirectExpressionStep> condition,
    std::unique_ptr<DirectExpressionStep> left,
    std::unique_ptr<DirectExpressionStep> right, int64_t expr_id,
    bool shortcircuiting = true);

// Factory method for ternary (_?_:_) execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateTernaryStep(
    int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_TERNARY_STEP_H_
