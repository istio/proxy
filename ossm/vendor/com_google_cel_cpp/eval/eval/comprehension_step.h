#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

class ComprehensionNextStep : public ExpressionStepBase {
 public:
  ComprehensionNextStep(size_t iter_slot, size_t accu_slot, int64_t expr_id);

  void set_jump_offset(int offset);
  void set_error_jump_offset(int offset);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  size_t iter_slot_;
  size_t accu_slot_;
  int jump_offset_;
  int error_jump_offset_;
};

class ComprehensionCondStep : public ExpressionStepBase {
 public:
  ComprehensionCondStep(size_t iter_slot, size_t accu_slot,
                        bool shortcircuiting, int64_t expr_id);

  void set_jump_offset(int offset);
  void set_error_jump_offset(int offset);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  size_t iter_slot_;
  size_t accu_slot_;
  int jump_offset_;
  int error_jump_offset_;
  bool shortcircuiting_;
};

// Creates a step for executing a comprehension.
std::unique_ptr<DirectExpressionStep> CreateDirectComprehensionStep(
    size_t iter_slot, size_t accu_slot,
    std::unique_ptr<DirectExpressionStep> range,
    std::unique_ptr<DirectExpressionStep> accu_init,
    std::unique_ptr<DirectExpressionStep> loop_step,
    std::unique_ptr<DirectExpressionStep> condition_step,
    std::unique_ptr<DirectExpressionStep> result_step, bool shortcircuiting,
    int64_t expr_id);

// Creates a cleanup step for the comprehension.
// Removes the comprehension context then pushes the 'result' sub expression to
// the top of the stack.
std::unique_ptr<ExpressionStep> CreateComprehensionFinishStep(size_t accu_slot,
                                                              int64_t expr_id);

// Creates a step that checks that the input is iterable and sets up the loop
// context for the comprehension.
std::unique_ptr<ExpressionStep> CreateComprehensionInitStep(int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
