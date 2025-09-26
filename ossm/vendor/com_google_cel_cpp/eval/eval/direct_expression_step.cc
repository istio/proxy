// Copyright 2024 Google LLC
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
#include "eval/eval/direct_expression_step.h"

#include <utility>

#include "absl/status/status.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

absl::Status WrappedDirectStep::Evaluate(ExecutionFrame* frame) const {
  cel::Value result;
  AttributeTrail attribute_trail;
  CEL_RETURN_IF_ERROR(impl_->Evaluate(*frame, result, attribute_trail));
  frame->value_stack().Push(std::move(result), std::move(attribute_trail));
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
