// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/eval/evaluator_core.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/value.h"
#include "runtime/activation_interface.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

void FlatExpressionEvaluatorState::Reset() {
  value_stack_.Clear();
  iterator_stack_.Clear();
  comprehension_slots_.Reset();
}

const ExpressionStep* ExecutionFrame::Next() {
  while (true) {
    const size_t end_pos = execution_path_.size();

    if (ABSL_PREDICT_TRUE(pc_ < end_pos)) {
      const auto* step = execution_path_[pc_++].get();
      ABSL_ASSUME(step != nullptr);
      return step;
    }
    if (ABSL_PREDICT_TRUE(pc_ == end_pos)) {
      if (!call_stack_.empty()) {
        SubFrame& subframe = call_stack_.back();
        pc_ = subframe.return_pc;
        execution_path_ = subframe.return_expression;
        ABSL_DCHECK_EQ(value_stack().size(), subframe.expected_stack_size);
        comprehension_slots().Set(subframe.slot_index, value_stack().Peek(),
                                  value_stack().PeekAttribute());
        call_stack_.pop_back();
        continue;
      }
    } else {
      ABSL_LOG(ERROR) << "Attempting to step beyond the end of execution path.";
    }
    return nullptr;
  }
}

namespace {

// This class abuses the fact that `absl::Status` is trivially destructible when
// `absl::Status::ok()` is `true`. If the implementation of `absl::Status` every
// changes, LSan and ASan should catch it. We cannot deal with the cost of extra
// move assignment and destructor calls.
//
// This is useful only in the evaluation loop and is a direct replacement for
// `RETURN_IF_ERROR`. It yields the most improvements on benchmarks with lots of
// steps which never return non-OK `absl::Status`.
class EvaluationStatus final {
 public:
  explicit EvaluationStatus(absl::Status&& status) {
    ::new (static_cast<void*>(&status_[0])) absl::Status(std::move(status));
  }

  EvaluationStatus() = delete;
  EvaluationStatus(const EvaluationStatus&) = delete;
  EvaluationStatus(EvaluationStatus&&) = delete;
  EvaluationStatus& operator=(const EvaluationStatus&) = delete;
  EvaluationStatus& operator=(EvaluationStatus&&) = delete;

  absl::Status Consume() && {
    return std::move(*reinterpret_cast<absl::Status*>(&status_[0]));
  }

  bool ok() const {
    return ABSL_PREDICT_TRUE(
        reinterpret_cast<const absl::Status*>(&status_[0])->ok());
  }

 private:
  alignas(absl::Status) char status_[sizeof(absl::Status)];
};

}  // namespace

absl::StatusOr<cel::Value> ExecutionFrame::Evaluate(
    EvaluationListener& listener) {
  const size_t initial_stack_size = value_stack().size();

  if (!listener) {
    for (const ExpressionStep* expr = Next();
         ABSL_PREDICT_TRUE(expr != nullptr); expr = Next()) {
      if (EvaluationStatus status(expr->Evaluate(this)); !status.ok()) {
        return std::move(status).Consume();
      }
    }
  } else {
    for (const ExpressionStep* expr = Next();
         ABSL_PREDICT_TRUE(expr != nullptr); expr = Next()) {
      if (EvaluationStatus status(expr->Evaluate(this)); !status.ok()) {
        return std::move(status).Consume();
      }

      if (pc_ == 0 || !expr->comes_from_ast()) {
        // Skip if we just started a Call or if the step doesn't map to an
        // AST id.
        continue;
      }

      if (ABSL_PREDICT_FALSE(value_stack().empty())) {
        ABSL_LOG(ERROR) << "Stack is empty after a ExpressionStep.Evaluate. "
                           "Try to disable short-circuiting.";
        continue;
      }
      if (EvaluationStatus status(listener(expr->id(), value_stack().Peek(),
                                           descriptor_pool(), message_factory(),
                                           arena()));
          !status.ok()) {
        return std::move(status).Consume();
      }
    }
  }

  const size_t final_stack_size = value_stack().size();
  if (ABSL_PREDICT_FALSE(final_stack_size != initial_stack_size + 1 ||
                         final_stack_size == 0)) {
    return absl::InternalError(absl::StrCat(
        "Stack error during evaluation: expected=", initial_stack_size + 1,
        ", actual=", final_stack_size));
  }

  cel::Value value = std::move(value_stack().Peek());
  value_stack().Pop(1);
  return value;
}

FlatExpressionEvaluatorState FlatExpression::MakeEvaluatorState(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::MessageFactory* absl_nonnull message_factory,
    google::protobuf::Arena* absl_nonnull arena) const {
  return FlatExpressionEvaluatorState(path_.size(), comprehension_slots_size_,
                                      type_provider_, descriptor_pool,
                                      message_factory, arena);
}

absl::StatusOr<cel::Value> FlatExpression::EvaluateWithCallback(
    const cel::ActivationInterface& activation, EvaluationListener listener,
    FlatExpressionEvaluatorState& state) const {
  state.Reset();

  ExecutionFrame frame(subexpressions_, activation, options_, state,
                       std::move(listener));

  return frame.Evaluate(frame.callback());
}

}  // namespace google::api::expr::runtime
