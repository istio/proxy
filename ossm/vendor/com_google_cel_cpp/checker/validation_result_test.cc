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

#include "checker/validation_result.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "checker/type_check_issue.h"
#include "common/ast/ast_impl.h"
#include "common/source.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::ast_internal::AstImpl;
using ::testing::_;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

using Severity = TypeCheckIssue::Severity;

TEST(ValidationResultTest, IsValidWithAst) {
  ValidationResult result(std::make_unique<AstImpl>(), {});
  EXPECT_TRUE(result.IsValid());
  EXPECT_THAT(result.GetAst(), NotNull());
  EXPECT_THAT(result.ReleaseAst(), IsOkAndHolds(NotNull()));
}

TEST(ValidationResultTest, IsNotValidWithoutAst) {
  ValidationResult result({});
  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(result.GetAst(), IsNull());
  EXPECT_THAT(result.ReleaseAst(),
              StatusIs(absl::StatusCode::kFailedPrecondition, _));
}

TEST(ValidationResultTest, GetIssues) {
  ValidationResult result(
      {TypeCheckIssue::CreateError({-1, -1}, "Issue1"),
       TypeCheckIssue(Severity::kInformation, {-1, -1}, "Issue2")});
  EXPECT_FALSE(result.IsValid());

  ASSERT_THAT(result.GetIssues(), SizeIs(2));

  EXPECT_THAT(result.GetIssues()[0].message(), "Issue1");
  EXPECT_THAT(result.GetIssues()[0].severity(), Severity::kError);

  EXPECT_THAT(result.GetIssues()[1].message(), "Issue2");
  EXPECT_THAT(result.GetIssues()[1].severity(), Severity::kInformation);
}

TEST(ValidationResultTest, FormatError) {
  ValidationResult result(
      {TypeCheckIssue::CreateError({1, 2}, "Issue1"),
       TypeCheckIssue(Severity::kInformation, {-1, -1}, "Issue2")});
  EXPECT_FALSE(result.IsValid());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Source> source,
                       NewSource("source.cel", "<description>"));
  result.SetSource(std::move(source));

  ASSERT_THAT(result.GetIssues(), SizeIs(2));

  EXPECT_THAT(result.FormatError(),
              "ERROR: <description>:1:3: Issue1\n"
              " | source.cel\n"
              " | ..^\n"
              "INFORMATION: <description>:-1:-1: Issue2");
}

}  // namespace
}  // namespace cel
