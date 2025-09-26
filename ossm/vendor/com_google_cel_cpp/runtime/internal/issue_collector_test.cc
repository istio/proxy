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
#include "runtime/internal/issue_collector.h"

#include "absl/status/status.h"
#include "internal/testing.h"
#include "runtime/runtime_issue.h"

namespace cel::runtime_internal {
namespace {

using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::Truly;

template <typename Matcher, typename T>
bool ApplyMatcher(Matcher m, const T& t) {
  return static_cast<testing::Matcher<T>>(m).Matches(t);
}

TEST(IssueCollector, CollectsIssues) {
  IssueCollector issue_collector(RuntimeIssue::Severity::kError);

  EXPECT_THAT(issue_collector.AddIssue(
                  RuntimeIssue::CreateError(absl::InvalidArgumentError("e1"))),
              StatusIs(absl::StatusCode::kInvalidArgument, "e1"));
  ASSERT_OK(issue_collector.AddIssue(RuntimeIssue::CreateWarning(
      absl::InvalidArgumentError("w1"),
      RuntimeIssue::ErrorCode::kNoMatchingOverload)));

  EXPECT_THAT(
      issue_collector.issues(),
      ElementsAre(
          Truly([](const RuntimeIssue& issue) {
            return issue.severity() == RuntimeIssue::Severity::kError &&
                   issue.error_code() == RuntimeIssue::ErrorCode::kOther &&
                   ApplyMatcher(
                       StatusIs(absl::StatusCode::kInvalidArgument, "e1"),
                       issue.ToStatus());
          }),
          Truly([](const RuntimeIssue& issue) {
            return issue.severity() == RuntimeIssue::Severity::kWarning &&
                   issue.error_code() ==
                       RuntimeIssue::ErrorCode::kNoMatchingOverload &&
                   ApplyMatcher(
                       StatusIs(absl::StatusCode::kInvalidArgument, "w1"),
                       issue.ToStatus());
          })));
}

TEST(IssueCollector, ReturnsStatusAtLimit) {
  IssueCollector issue_collector(RuntimeIssue::Severity::kWarning);

  EXPECT_THAT(issue_collector.AddIssue(
                  RuntimeIssue::CreateError(absl::InvalidArgumentError("e1"))),
              StatusIs(absl::StatusCode::kInvalidArgument, "e1"));

  EXPECT_THAT(issue_collector.AddIssue(RuntimeIssue::CreateWarning(
                  absl::InvalidArgumentError("w1"),
                  RuntimeIssue::ErrorCode::kNoMatchingOverload)),
              StatusIs(absl::StatusCode::kInvalidArgument, "w1"));

  EXPECT_THAT(
      issue_collector.issues(),
      ElementsAre(
          Truly([](const RuntimeIssue& issue) {
            return issue.severity() == RuntimeIssue::Severity::kError &&
                   issue.error_code() == RuntimeIssue::ErrorCode::kOther &&
                   ApplyMatcher(
                       StatusIs(absl::StatusCode::kInvalidArgument, "e1"),
                       issue.ToStatus());
          }),
          Truly([](const RuntimeIssue& issue) {
            return issue.severity() == RuntimeIssue::Severity::kWarning &&
                   issue.error_code() ==
                       RuntimeIssue::ErrorCode::kNoMatchingOverload &&
                   ApplyMatcher(
                       StatusIs(absl::StatusCode::kInvalidArgument, "w1"),
                       issue.ToStatus());
          })));
}
}  // namespace
}  // namespace cel::runtime_internal
