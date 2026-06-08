#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/expr.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

std::unique_ptr<DirectExpressionStep> CreateDirectIdentStep(
    absl::string_view identifier, int64_t expr_id);

std::unique_ptr<DirectExpressionStep> CreateDirectSlotIdentStep(
    absl::string_view identifier, size_t slot_index, int64_t expr_id);

// Factory method for Ident - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const cel::IdentExpr& ident, int64_t expr_id);

// Factory method for identifier that has been assigned to a slot.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStepForSlot(
    const cel::IdentExpr& ident_expr, size_t slot_index, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_
