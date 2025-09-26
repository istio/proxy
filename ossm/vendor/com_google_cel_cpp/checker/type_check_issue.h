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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECK_ISSUE_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECK_ISSUE_H_

#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "common/source.h"

namespace cel {

// Represents a single issue identified in type checking.
class TypeCheckIssue {
 public:
  enum class Severity { kError, kWarning, kInformation, kDeprecated };

  TypeCheckIssue(Severity severity, SourceLocation location,
                 std::string message)
      : severity_(severity),
        location_(location),
        message_(std::move(message)) {}

  // Factory for error-severity issues.
  static TypeCheckIssue CreateError(SourceLocation location,
                                    std::string message) {
    return TypeCheckIssue(Severity::kError, location, std::move(message));
  }

  // Factory for error-severity issues.
  // line is 1-based, column is 0-based.
  static TypeCheckIssue CreateError(int line, int column, std::string message) {
    return TypeCheckIssue(Severity::kError, SourceLocation{line, column},
                          std::move(message));
  }

  // Format the issue highlighting the source position.
  std::string ToDisplayString(const Source* source) const;

  std::string ToDisplayString(const Source& source) const {
    return ToDisplayString(&source);
  }

  absl::string_view message() const { return message_; }
  Severity severity() const { return severity_; }
  SourceLocation location() const { return location_; }

 private:
  Severity severity_;
  SourceLocation location_;
  std::string message_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECK_ISSUE_H_
