#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "absl/status/status.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

// Comprehension Evaluation
//
// 0: <iter_range>              1 -> 1
// 1: ComprehensionInitStep     1 -> 1
// 2: <accu_init>               1 -> 2
// 3: ComprehensionNextStep     2 -> 1
// 4: <loop_condition>          1 -> 2
// 5: ComprehensionCondStep     2 -> 1
// 6: <loop_step>               1 -> 2
// 8: <result>                  1 -> 2
// 9: ComprehensionFinishStep   2 -> 1

class ComprehensionInitStep final : public ExpressionStepBase {
 public:
  explicit ComprehensionInitStep(int64_t expr_id)
      : ExpressionStepBase(expr_id, /*comes_from_ast=*/false) {}

  void set_error_jump_offset(int offset) { error_jump_offset_ = offset; }

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  int error_jump_offset_ = std::numeric_limits<int>::max();
};

class ComprehensionNextStep final : public ExpressionStepBase {
 public:
  ComprehensionNextStep(size_t iter_slot, size_t iter2_slot, size_t accu_slot,
                        int64_t expr_id)
      : ExpressionStepBase(expr_id, /*comes_from_ast=*/false),
        iter_slot_(iter_slot),
        iter2_slot_(iter2_slot),
        accu_slot_(accu_slot) {}

  void set_jump_offset(int offset) { jump_offset_ = offset; }

  void set_error_jump_offset(int offset) { error_jump_offset_ = offset; }

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    return iter_slot_ == iter2_slot_ ? Evaluate1(frame) : Evaluate2(frame);
  }

 private:
  absl::Status Evaluate1(ExecutionFrame* frame) const;

  absl::Status Evaluate2(ExecutionFrame* frame) const;

  const size_t iter_slot_;
  const size_t iter2_slot_;
  const size_t accu_slot_;
  int jump_offset_ = std::numeric_limits<int>::max();
  int error_jump_offset_ = std::numeric_limits<int>::max();
};

class ComprehensionCondStep final : public ExpressionStepBase {
 public:
  ComprehensionCondStep(size_t iter_slot, size_t iter2_slot, size_t accu_slot,
                        bool shortcircuiting, int64_t expr_id)
      : ExpressionStepBase(expr_id, /*comes_from_ast=*/false),
        iter_slot_(iter_slot),
        iter2_slot_(iter2_slot),
        accu_slot_(accu_slot),
        shortcircuiting_(shortcircuiting) {}

  void set_jump_offset(int offset) { jump_offset_ = offset; }

  void set_error_jump_offset(int offset) { error_jump_offset_ = offset; }

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    return iter_slot_ == iter2_slot_ ? Evaluate1(frame) : Evaluate2(frame);
  }

 private:
  absl::Status Evaluate1(ExecutionFrame* frame) const;

  absl::Status Evaluate2(ExecutionFrame* frame) const;

  const size_t iter_slot_;
  const size_t iter2_slot_;
  const size_t accu_slot_;
  int jump_offset_ = std::numeric_limits<int>::max();
  int error_jump_offset_ = std::numeric_limits<int>::max();
  const bool shortcircuiting_;
};

// Creates a step for executing a comprehension.
std::unique_ptr<DirectExpressionStep> CreateDirectComprehensionStep(
    size_t iter_slot, size_t iter2_slot, size_t accu_slot,
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

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
