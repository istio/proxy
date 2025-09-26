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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_ISSUE_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_ISSUE_H_

#include <utility>

#include "absl/status/status.h"

namespace cel {

// Represents an issue with a given CEL expression.
//
// The error details are represented as an absl::Status for compatibility
// reasons, but users should not depend on this.
class RuntimeIssue {
 public:
  // Severity of the RuntimeIssue.
  //
  // Can be used to determine whether to continue program planning or return
  // early.
  enum class Severity {
    // The issue may lead to runtime errors in evaluation.
    kWarning = 0,
    // The expression is invalid or unsupported.
    kError = 1,
    // Arbitrary max value above Error.
    kNotForUseWithExhaustiveSwitchStatements = 15
  };

  // Code for well-known runtime error kinds.
  enum class ErrorCode {
    // Overload not provided for given function call signature.
    kNoMatchingOverload,
    // Field access refers to unknown field for given type.
    kNoSuchField,
    // Other error outside the canonical set.
    kOther,
  };

  static RuntimeIssue CreateError(absl::Status status,
                                  ErrorCode error_code = ErrorCode::kOther) {
    return RuntimeIssue(std::move(status), Severity::kError, error_code);
  }

  static RuntimeIssue CreateWarning(absl::Status status,
                                    ErrorCode error_code = ErrorCode::kOther) {
    return RuntimeIssue(std::move(status), Severity::kWarning, error_code);
  }

  RuntimeIssue(const RuntimeIssue& other) = default;
  RuntimeIssue& operator=(const RuntimeIssue& other) = default;
  RuntimeIssue(RuntimeIssue&& other) = default;
  RuntimeIssue& operator=(RuntimeIssue&& other) = default;

  Severity severity() const { return severity_; }

  ErrorCode error_code() const { return error_code_; }

  const absl::Status& ToStatus() const& { return status_; }
  absl::Status ToStatus() && { return std::move(status_); }

 private:
  RuntimeIssue(absl::Status status, Severity severity, ErrorCode error_code)
      : status_(std::move(status)),
        error_code_(error_code),
        severity_(severity) {}

  absl::Status status_;
  ErrorCode error_code_;
  Severity severity_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_ISSUE_H_
