// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_VALIDATION_RESULT_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_VALIDATION_RESULT_H_

#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "checker/type_check_issue.h"
#include "common/ast.h"

namespace cel {

// ValidationResult holds the result of TypeChecking.
//
// Error states are captured as type check issues where possible.
class ValidationResult {
 public:
  ValidationResult(std::unique_ptr<Ast> ast, std::vector<TypeCheckIssue> issues)
      : ast_(std::move(ast)), issues_(std::move(issues)) {}

  explicit ValidationResult(std::vector<TypeCheckIssue> issues)
      : ast_(nullptr), issues_(std::move(issues)) {}

  bool IsValid() const { return ast_ != nullptr; }

  // Returns the AST if validation was successful.
  //
  // This is a non-null pointer if IsValid() is true.
  absl::Nullable<const Ast*> GetAst() const { return ast_.get(); }

  absl::StatusOr<std::unique_ptr<Ast>> ReleaseAst() {
    if (ast_ == nullptr) {
      return absl::FailedPreconditionError(
          "ValidationResult is empty. Check for TypeCheckIssues.");
    }
    return std::move(ast_);
  }

  absl::Span<const TypeCheckIssue> GetIssues() const { return issues_; }

 private:
  absl::Nullable<std::unique_ptr<Ast>> ast_;
  std::vector<TypeCheckIssue> issues_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_VALIDATION_RESULT_H_
