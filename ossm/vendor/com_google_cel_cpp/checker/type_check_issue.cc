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

#include "checker/type_check_issue.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "common/source.h"

namespace cel {

namespace {

absl::string_view SeverityString(TypeCheckIssue::Severity severity) {
  switch (severity) {
    case TypeCheckIssue::Severity::kInformation:
      return "INFORMATION";
    case TypeCheckIssue::Severity::kWarning:
      return "WARNING";
    case TypeCheckIssue::Severity::kError:
      return "ERROR";
    case TypeCheckIssue::Severity::kDeprecated:
      return "DEPRECATED";
    default:
      return "SEVERITY_UNSPECIFIED";
  }
}

}  // namespace

std::string TypeCheckIssue::ToDisplayString(const Source& source) const {
  int column = location_.column;
  // convert to 1-based if it's in range.
  int display_column = column >= 0 ? column + 1 : column;
  return absl::StrCat(
      absl::StrFormat("%s: %s:%d:%d: %s", SeverityString(severity_),
                      source.description(), location_.line, display_column,
                      message_),
      source.DisplayErrorLocation(location_));
}

}  // namespace cel
