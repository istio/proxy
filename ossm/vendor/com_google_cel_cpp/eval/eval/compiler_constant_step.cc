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
#include "eval/eval/compiler_constant_step.h"

#include "absl/status/status.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

using ::cel::Value;

absl::Status DirectCompilerConstantStep::Evaluate(
    ExecutionFrameBase& frame, Value& result, AttributeTrail& attribute) const {
  result = value_;
  return absl::OkStatus();
}

absl::Status CompilerConstantStep::Evaluate(ExecutionFrame* frame) const {
  frame->value_stack().Push(value_);

  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
