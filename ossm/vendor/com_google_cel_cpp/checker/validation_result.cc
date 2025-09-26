// Copyright 2025 Google LLC
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

#include "checker/validation_result.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "checker/type_check_issue.h"

namespace cel {

std::string ValidationResult::FormatError() const {
  return absl::StrJoin(
      issues_, "\n", [this](std::string* out, const TypeCheckIssue& issue) {
        absl::StrAppend(out, issue.ToDisplayString(source_.get()));
      });
}

}  // namespace cel
