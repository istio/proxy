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

#include "eval/eval/jump_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/value.h"
#include "eval/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::ErrorValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

class JumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  JumpStep(absl::optional<int> jump_offset, int64_t expr_id)
      : JumpStepBase(jump_offset, expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    return Jump(frame);
  }
};

class CondJumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  CondJumpStep(bool jump_condition, bool leave_on_stack,
               absl::optional<int> jump_offset, int64_t expr_id)
      : JumpStepBase(jump_offset, expr_id),
        jump_condition_(jump_condition),
        leave_on_stack_(leave_on_stack) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    // Peek the top value
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }

    const auto& value = frame->value_stack().Peek();
    const auto should_jump = value.Is<BoolValue>() &&
                             jump_condition_ == value.GetBool().NativeValue();

    if (!leave_on_stack_) {
      frame->value_stack().Pop(1);
    }

    if (should_jump) {
      return Jump(frame);
    }

    return absl::OkStatus();
  }

 private:
  const bool jump_condition_;
  const bool leave_on_stack_;
};

class BoolCheckJumpStep : public JumpStepBase {
 public:
  // Checks if the top value is a boolean:
  // - no-op if it is a boolean
  // - jump to the label if it is an error value
  // - jump to the label if it is unknown value
  // - jump to the label if it is neither an error nor a boolean, pops it and
  // pushes "no matching overload" error
  BoolCheckJumpStep(absl::optional<int> jump_offset, int64_t expr_id)
      : JumpStepBase(jump_offset, expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    // Peek the top value
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }

    const Value& value = frame->value_stack().Peek();

    if (value->Is<BoolValue>()) {
      return absl::OkStatus();
    }

    if (value->Is<ErrorValue>() || value->Is<UnknownValue>()) {
      return Jump(frame);
    }

    // Neither bool, error, nor unknown set.
    Value error_value =
        cel::ErrorValue(CreateNoMatchingOverloadError("<jump_condition>"));

    frame->value_stack().PopAndPush(std::move(error_value));
    return Jump(frame);

    return absl::OkStatus();
  }
};

}  // namespace

// Factory method for Conditional Jump step.
// Conditional Jump requires a boolean value to sit on the stack.
// It is compared to jump_condition, and if matched, jump is performed.
std::unique_ptr<JumpStepBase> CreateCondJumpStep(
    bool jump_condition, bool leave_on_stack, absl::optional<int> jump_offset,
    int64_t expr_id) {
  return std::make_unique<CondJumpStep>(jump_condition, leave_on_stack,
                                        jump_offset, expr_id);
}

// Factory method for Jump step.
std::unique_ptr<JumpStepBase> CreateJumpStep(absl::optional<int> jump_offset,
                                             int64_t expr_id) {
  return std::make_unique<JumpStep>(jump_offset, expr_id);
}

// Factory method for Conditional Jump step.
// Conditional Jump requires a value to sit on the stack.
// If this value is an error or unknown, a jump is performed.
std::unique_ptr<JumpStepBase> CreateBoolCheckJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id) {
  return std::make_unique<BoolCheckJumpStep>(jump_offset, expr_id);
}

}  // namespace google::api::expr::runtime
