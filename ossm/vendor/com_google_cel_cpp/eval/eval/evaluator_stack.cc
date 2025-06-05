#include "eval/eval/evaluator_stack.h"

namespace google::api::expr::runtime {

void EvaluatorStack::Clear() {
  stack_.clear();
  attribute_stack_.clear();
  current_size_ = 0;
}

}  // namespace google::api::expr::runtime
