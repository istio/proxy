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

#include "google/rpc/context/attribute_context.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "codelab/exercise2.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::codelab {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::google::rpc::context::AttributeContext;

// Helper for a simple CelExpression with no context.
absl::StatusOr<bool> TruthTableTest(absl::string_view statement) {
  return ParseAndEvaluate(statement, /*unused*/ false);
}

TEST(Exercise3, LogicalOr) {
  EXPECT_THAT(TruthTableTest("true || (1 / 0 > 2)"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("false || (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || true"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || false"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) || (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("true || true"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("true || false"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("false || true"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("false || false"), IsOkAndHolds(false));
}

TEST(Exercise3, LogicalAnd) {
  EXPECT_THAT(TruthTableTest("true && (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("false && (1 / 0 > 2)"), IsOkAndHolds(false));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) && true"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) && false"), IsOkAndHolds(false));
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) && (1 / 0 > 2)"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("true && true"), IsOkAndHolds(true));
  EXPECT_THAT(TruthTableTest("true && false"), IsOkAndHolds(false));
  EXPECT_THAT(TruthTableTest("false && true"), IsOkAndHolds(false));
  EXPECT_THAT(TruthTableTest("false && false"), IsOkAndHolds(false));
}

TEST(Exercise3, Ternary) {
  EXPECT_THAT(TruthTableTest("(1 / 0 > 2) ? false : false"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("true ? (1 / 0 > 2) : false"),
              StatusIs(absl::StatusCode::kInvalidArgument, "divide by zero"));
  EXPECT_THAT(TruthTableTest("false ? (1 / 0 > 2) : false"),
              IsOkAndHolds(false));
}

TEST(Exercise3Context, BadFieldAccess) {
  // This type of error is normally caught by the type checker, but we can
  // surface it here since we are only parsing.
  AttributeContext context;

  // typo-ed field name from 'request.host'
  EXPECT_THAT(
      ParseAndEvaluate("request.hostname == 'localhost' && true", context),
      StatusIs(absl::StatusCode::kNotFound, "no_such_field : hostname"));
  EXPECT_THAT(
      ParseAndEvaluate("request.hostname == 'localhost' && false", context),
      IsOkAndHolds(false));

  EXPECT_THAT(
      ParseAndEvaluate("request.hostname == 'localhost' || true", context),
      IsOkAndHolds(true));
  EXPECT_THAT(
      ParseAndEvaluate("request.hostname == 'localhost' || false", context),
      StatusIs(absl::StatusCode::kNotFound, "no_such_field : hostname"));
}

}  // namespace
}  // namespace google::api::expr::codelab
