#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "common/value.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Create an identifier resolution step with a default value that may be
// shadowed by an identifier of the same name within the runtime-provided
// Activation.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    std::string identifier, cel::Value value, int64_t expr_id);

std::unique_ptr<DirectExpressionStep> CreateDirectShadowableValueStep(
    std::string identifier, cel::Value value, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_
