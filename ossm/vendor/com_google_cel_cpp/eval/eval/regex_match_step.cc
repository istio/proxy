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

#include "eval/eval/regex_match_step.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"
#include "re2/re2.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::StringValue;
using ::cel::Value;

inline constexpr int kNumRegexMatchArguments = 1;
inline constexpr size_t kRegexMatchStepSubject = 0;

struct MatchesVisitor final {
  const RE2& re;

  bool operator()(const absl::Cord& value) const {
    if (auto flat = value.TryFlat(); flat.has_value()) {
      return RE2::PartialMatch(*flat, re);
    }
    return RE2::PartialMatch(static_cast<std::string>(value), re);
  }

  bool operator()(absl::string_view value) const {
    return RE2::PartialMatch(value, re);
  }
};

class RegexMatchStep final : public ExpressionStepBase {
 public:
  RegexMatchStep(int64_t expr_id, std::shared_ptr<const RE2> re2)
      : ExpressionStepBase(expr_id, /*comes_from_ast=*/true),
        re2_(std::move(re2)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(kNumRegexMatchArguments)) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Insufficient arguments supplied for regular "
                          "expression match");
    }
    auto input_args = frame->value_stack().GetSpan(kNumRegexMatchArguments);
    const auto& subject = input_args[kRegexMatchStepSubject];
    if (!subject->Is<cel::StringValue>()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "First argument for regular "
                          "expression match must be a string");
    }
    bool match = subject.GetString().NativeValue(MatchesVisitor{*re2_});
    frame->value_stack().Pop(kNumRegexMatchArguments);
    frame->value_stack().Push(cel::BoolValue(match));
    return absl::OkStatus();
  }

 private:
  const std::shared_ptr<const RE2> re2_;
};

class RegexMatchDirectStep final : public DirectExpressionStep {
 public:
  RegexMatchDirectStep(int64_t expr_id,
                       std::unique_ptr<DirectExpressionStep> subject,
                       std::shared_ptr<const RE2> re2)
      : DirectExpressionStep(expr_id),
        subject_(std::move(subject)),
        re2_(std::move(re2)) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    AttributeTrail subject_attr;
    CEL_RETURN_IF_ERROR(subject_->Evaluate(frame, result, subject_attr));
    if (result.IsError() || result.IsUnknown()) {
      return absl::OkStatus();
    }

    if (!result.IsString()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "First argument for regular "
                          "expression match must be a string");
    }
    bool match = result.GetString().NativeValue(MatchesVisitor{*re2_});
    result = BoolValue(match);
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<DirectExpressionStep> subject_;
  const std::shared_ptr<const RE2> re2_;
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectRegexMatchStep(
    int64_t expr_id, std::unique_ptr<DirectExpressionStep> subject,
    std::shared_ptr<const RE2> re2) {
  return std::make_unique<RegexMatchDirectStep>(expr_id, std::move(subject),
                                                std::move(re2));
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateRegexMatchStep(
    std::shared_ptr<const RE2> re2, int64_t expr_id) {
  return std::make_unique<RegexMatchStep>(expr_id, std::move(re2));
}

}  // namespace google::api::expr::runtime
