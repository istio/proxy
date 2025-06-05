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

#include "checker/type_check_issue.h"

#include "common/source.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(TypeCheckIssueTest, DisplayString) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("test{\n\tfield1: 123\n}"));
  TypeCheckIssue issue = TypeCheckIssue::CreateError(2, 2, "test error");
  // Note: The column is displayed as 1 based to match the Go checker.
  EXPECT_EQ(issue.ToDisplayString(*source),
            "ERROR: <input>:2:3: test error\n"
            " |  field1: 123\n"
            " | ..^");
}

TEST(TypeCheckIssueTest, DisplayStringNoPosition) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("test{\n\tfield1: 123\n}"));
  TypeCheckIssue issue = TypeCheckIssue::CreateError(-1, -1, "test error");
  EXPECT_EQ(issue.ToDisplayString(*source), "ERROR: <input>:-1:-1: test error");
}

TEST(TypeCheckIssueTest, DisplayStringDeprecated) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("test{\n\tfield1: 123\n}"));
  TypeCheckIssue issue = TypeCheckIssue(TypeCheckIssue::Severity::kDeprecated,
                                        {-1, -1}, "test error 2");
  EXPECT_EQ(issue.ToDisplayString(*source),
            "DEPRECATED: <input>:-1:-1: test error 2");
}

}  // namespace
}  // namespace cel
