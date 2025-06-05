#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "base/ast_internal/expr.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

std::unique_ptr<DirectExpressionStep> CreateConstValueDirectStep(
    cel::Value value, int64_t expr_id = -1);

// Factory method for Constant Value expression step.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    cel::Value value, int64_t expr_id, bool comes_from_ast = true);

// Factory method for Constant AST node expression step.
// Copies the Constant Expr node to avoid lifecycle dependency on source
// expression.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const cel::ast_internal::Constant&, int64_t expr_id,
    cel::ValueManager& value_factory, bool comes_from_ast = true);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
