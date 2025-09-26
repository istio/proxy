// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_REGEX_MATCH_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_REGEX_MATCH_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "re2/re2.h"

namespace google::api::expr::runtime {

std::unique_ptr<DirectExpressionStep> CreateDirectRegexMatchStep(
    int64_t expr_id, std::unique_ptr<DirectExpressionStep> subject,
    std::shared_ptr<const RE2> re2);

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateRegexMatchStep(
    std::shared_ptr<const RE2> re2, int64_t expr_id);

}

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_REGEX_MATCH_STEP_H_
