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

#include "tools/cel_field_extractor.h"

#include <string>

#include "cel/expr/syntax.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace cel {

namespace {

using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

absl::flat_hash_set<std::string> GetExtractedFields(
    const std::string& cel_query) {
  absl::StatusOr<ParsedExpr> parsed_expr_or_status = Parse(cel_query);
  ABSL_CHECK_OK(parsed_expr_or_status);
  return ExtractFieldPaths(parsed_expr_or_status.value().expr());
}

TEST(TestExtractFieldPaths, CelExprWithOneField) {
  EXPECT_THAT(GetExtractedFields("field_name"),
              UnorderedElementsAre("field_name"));
}

TEST(TestExtractFieldPaths, CelExprWithNoWithLiteral) {
  EXPECT_THAT(GetExtractedFields("'field_name'"), IsEmpty());
}

TEST(TestExtractFieldPaths, CelExprWithFunctionCallOnSingleField) {
  EXPECT_THAT(GetExtractedFields("!boolean_field"),
              UnorderedElementsAre("boolean_field"));
}

TEST(TestExtractFieldPaths, CelExprWithSizeFuncCallOnSingleField) {
  EXPECT_THAT(GetExtractedFields("size(repeated_field)"),
              UnorderedElementsAre("repeated_field"));
}

TEST(TestExtractFieldPaths, CelExprWithNestedField) {
  EXPECT_THAT(GetExtractedFields("message_field.nested_field.nested_field2"),
              UnorderedElementsAre("message_field.nested_field.nested_field2"));
}

TEST(TestExtractFieldPaths, CelExprWithNestedFieldAndIndexAccess) {
  EXPECT_THAT(GetExtractedFields(
                  "repeated_message_field.nested_field[0].nested_field2"),
              UnorderedElementsAre("repeated_message_field.nested_field"));
}

TEST(TestExtractFieldPaths, CelExprWithMultipleFunctionCalls) {
  EXPECT_THAT(GetExtractedFields(
                  "(size(repeated_field) > 0 && !boolean_field == true) || "
                  "request.valid == true && request.count == 0"),
              UnorderedElementsAre("boolean_field", "repeated_field",
                                   "request.valid", "request.count"));
}

TEST(TestExtractFieldPaths, CelExprWithNestedComprehension) {
  EXPECT_THAT(
      GetExtractedFields("repeated_field_1.exists(e, e.key == 'one') && "
                         "req.repeated_field_2.exists(x, "
                         "x.y.z == 'val' &&"
                         "x.array.exists(y, y == 'val' && req.bool_field == "
                         "true && x.bool_field == false))"),
      UnorderedElementsAre("req.repeated_field_2", "req.bool_field",
                           "repeated_field_1"));
}

TEST(TestExtractFieldPaths, CelExprWithMultipleComprehension) {
  EXPECT_THAT(
      GetExtractedFields(
          "repeated_field_1.exists(e, e.key == 'one' && y.field_1 == 'val') && "
          "repeated_field_2.exists(y, y.key == 'one' && e.field_2 == 'val')"),
      UnorderedElementsAre("repeated_field_1", "repeated_field_2", "e.field_2",
                           "y.field_1"));
}

TEST(TestExtractFieldPaths, CelExprWithListLiteral) {
  EXPECT_THAT(GetExtractedFields("['a', b, 3].exists(x, x == 1)"),
              UnorderedElementsAre("b"));
}

TEST(TestExtractFieldPaths, CelExprWithFunctionCallsAndRepeatedFields) {
  EXPECT_THAT(
      GetExtractedFields("data == 'data_1' && field_1 == 'val_1' &&"
                         "(matches(req.field_2, 'val_1') == true) &&"
                         "repeated_field[0].priority >= 200"),
      UnorderedElementsAre("data", "field_1", "req.field_2", "repeated_field"));
}

TEST(TestExtractFieldPaths, CelExprWithFunctionOnRepeatedField) {
  EXPECT_THAT(
      GetExtractedFields("(contains_data == false && "
                         "data.field_1=='value_1') || "
                         "size(data.nodes) > 0 && "
                         "data.nodes[0].field_2=='value_2'"),
      UnorderedElementsAre("contains_data", "data.field_1", "data.nodes"));
}

TEST(TestExtractFieldPaths, CelExprContainingEndsWithFunction) {
  EXPECT_THAT(GetExtractedFields("data.repeated_field.exists(f, "
                                 "f.field_1.field_2.endsWith('val_1')) || "
                                 "data.field_3.endsWith('val_3')"),
              UnorderedElementsAre("data.repeated_field", "data.field_3"));
}

TEST(TestExtractFieldPaths,
     CelExprWithMatchFunctionInsideComprehensionAndRegexConstants) {
  EXPECT_THAT(GetExtractedFields("req.field_1.field_2=='val_1' && "
                                 "data!=null && req.repeated_field.exists(f, "
                                 "f.matches('a100.*|.*h100_80gb.*|.*h200.*'))"),
              UnorderedElementsAre("req.field_1.field_2", "req.repeated_field",
                                   "data"));
}

TEST(TestExtractFieldPaths, CelExprWithMultipleChecksInComprehension) {
  EXPECT_THAT(
      GetExtractedFields("req.field.repeated_field.exists(f, f.key == 'data_1'"
                         " && f.str_value == 'val_1') && "
                         "req.metadata.type == 3"),
      UnorderedElementsAre("req.field.repeated_field", "req.metadata.type"));
}

}  // namespace

}  // namespace cel
