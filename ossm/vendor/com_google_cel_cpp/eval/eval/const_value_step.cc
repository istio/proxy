#include "eval/eval/const_value_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "common/allocator.h"
#include "common/constant.h"
#include "common/value.h"
#include "eval/eval/compiler_constant_step.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/status_macros.h"
#include "runtime/internal/convert_constant.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Constant;
using ::cel::runtime_internal::ConvertConstant;

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateConstValueDirectStep(
    cel::Value value, int64_t id) {
  return std::make_unique<DirectCompilerConstantStep>(std::move(value), id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    cel::Value value, int64_t expr_id, bool comes_from_ast) {
  return std::make_unique<CompilerConstantStep>(std::move(value), expr_id,
                                                comes_from_ast);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const Constant& value, int64_t expr_id, cel::Allocator<> allocator,
    bool comes_from_ast) {
  CEL_ASSIGN_OR_RETURN(cel::Value converted_value,
                       ConvertConstant(value, allocator));

  return std::make_unique<CompilerConstantStep>(std::move(converted_value),
                                                expr_id, comes_from_ast);
}

}  // namespace google::api::expr::runtime
