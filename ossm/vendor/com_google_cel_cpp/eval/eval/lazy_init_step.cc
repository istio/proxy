// Copyright 2023 Google LLC
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

#include "eval/eval/lazy_init_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "google/api/expr/v1alpha1/value.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Value;

class LazyInitStep final : public ExpressionStepBase {
 public:
  LazyInitStep(size_t slot_index, size_t subexpression_index, int64_t expr_id)
      : ExpressionStepBase(expr_id),
        slot_index_(slot_index),
        subexpression_index_(subexpression_index) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (auto* slot = frame->comprehension_slots().Get(slot_index_);
        slot != nullptr) {
      frame->value_stack().Push(slot->value, slot->attribute);
    } else {
      frame->Call(slot_index_, subexpression_index_);
    }
    return absl::OkStatus();
  }

 private:
  const size_t slot_index_;
  const size_t subexpression_index_;
};

class DirectLazyInitStep final : public DirectExpressionStep {
 public:
  DirectLazyInitStep(size_t slot_index,
                     const DirectExpressionStep* subexpression, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        slot_index_(slot_index),
        subexpression_(subexpression) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    if (auto* slot = frame.comprehension_slots().Get(slot_index_);
        slot != nullptr) {
      result = slot->value;
      attribute = slot->attribute;
    } else {
      CEL_RETURN_IF_ERROR(subexpression_->Evaluate(frame, result, attribute));
      frame.comprehension_slots().Set(slot_index_, result, attribute);
    }
    return absl::OkStatus();
  }

 private:
  const size_t slot_index_;
  const absl::Nonnull<const DirectExpressionStep*> subexpression_;
};

class BindStep : public DirectExpressionStep {
 public:
  BindStep(size_t slot_index,
           std::unique_ptr<DirectExpressionStep> subexpression, int64_t expr_id)
      : DirectExpressionStep(expr_id),
        slot_index_(slot_index),
        subexpression_(std::move(subexpression)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    CEL_RETURN_IF_ERROR(subexpression_->Evaluate(frame, result, attribute));

    frame.comprehension_slots().ClearSlot(slot_index_);

    return absl::OkStatus();
  }

 private:
  size_t slot_index_;
  std::unique_ptr<DirectExpressionStep> subexpression_;
};

class AssignSlotAndPopStepStep final : public ExpressionStepBase {
 public:
  explicit AssignSlotAndPopStepStep(size_t slot_index)
      : ExpressionStepBase(/*expr_id=*/-1, /*comes_from_ast=*/false),
        slot_index_(slot_index) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(1)) {
      return absl::InternalError("Stack underflow assigning lazy value");
    }

    frame->comprehension_slots().Set(slot_index_, frame->value_stack().Peek(),
                                     frame->value_stack().PeekAttribute());
    frame->value_stack().Pop(1);

    return absl::OkStatus();
  }

 private:
  const size_t slot_index_;
};

class ClearSlotStep : public ExpressionStepBase {
 public:
  explicit ClearSlotStep(size_t slot_index, int64_t expr_id)
      : ExpressionStepBase(expr_id), slot_index_(slot_index) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    frame->comprehension_slots().ClearSlot(slot_index_);
    return absl::OkStatus();
  }

 private:
  size_t slot_index_;
};

class ClearSlotsStep final : public ExpressionStepBase {
 public:
  explicit ClearSlotsStep(size_t slot_index, size_t slot_count, int64_t expr_id)
      : ExpressionStepBase(expr_id),
        slot_index_(slot_index),
        slot_count_(slot_count) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    for (size_t i = 0; i < slot_count_; ++i) {
      frame->comprehension_slots().ClearSlot(slot_index_ + i);
    }
    return absl::OkStatus();
  }

 private:
  const size_t slot_index_;
  const size_t slot_count_;
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectBindStep(
    size_t slot_index, std::unique_ptr<DirectExpressionStep> expression,
    int64_t expr_id) {
  return std::make_unique<BindStep>(slot_index, std::move(expression), expr_id);
}

std::unique_ptr<DirectExpressionStep> CreateDirectLazyInitStep(
    size_t slot_index, absl::Nonnull<const DirectExpressionStep*> subexpression,
    int64_t expr_id) {
  return std::make_unique<DirectLazyInitStep>(slot_index, subexpression,
                                              expr_id);
}

std::unique_ptr<ExpressionStep> CreateLazyInitStep(size_t slot_index,
                                                   size_t subexpression_index,
                                                   int64_t expr_id) {
  return std::make_unique<LazyInitStep>(slot_index, subexpression_index,
                                        expr_id);
}

std::unique_ptr<ExpressionStep> CreateAssignSlotAndPopStep(size_t slot_index) {
  return std::make_unique<AssignSlotAndPopStepStep>(slot_index);
}

std::unique_ptr<ExpressionStep> CreateClearSlotStep(size_t slot_index,
                                                    int64_t expr_id) {
  return std::make_unique<ClearSlotStep>(slot_index, expr_id);
}

std::unique_ptr<ExpressionStep> CreateClearSlotsStep(size_t slot_index,
                                                     size_t slot_count,
                                                     int64_t expr_id) {
  ABSL_DCHECK_GT(slot_count, 0);
  return std::make_unique<ClearSlotsStep>(slot_index, slot_count, expr_id);
}

}  // namespace google::api::expr::runtime
