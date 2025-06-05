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

#include "extensions/protobuf/internal/ast.h"

#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "common/ast.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions::protobuf_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::internal::test::EqualsProto;

using ExprProto = google::api::expr::v1alpha1::Expr;

struct ExprRoundtripTestCase {
  std::string input;
};

using ExprRoundTripTest = ::testing::TestWithParam<ExprRoundtripTestCase>;

TEST_P(ExprRoundTripTest, RoundTrip) {
  const auto& test_case = GetParam();
  ExprProto original_proto;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(test_case.input, &original_proto));
  Expr expr;
  ASSERT_THAT(ExprFromProto(original_proto, expr), IsOk());
  ExprProto proto;
  ASSERT_THAT(ExprToProto(expr, &proto), IsOk());
  EXPECT_THAT(proto, EqualsProto(original_proto));
}

INSTANTIATE_TEST_SUITE_P(
    ExprRoundTripTest, ExprRoundTripTest,
    ::testing::ValuesIn<ExprRoundtripTestCase>({
        {R"pb(
         )pb"},
        {R"pb(
           id: 1
         )pb"},
        {R"pb(
           id: 1
           const_expr {}
         )pb"},
        {R"pb(
           id: 1
           const_expr { null_value: NULL_VALUE }
         )pb"},
        {R"pb(
           id: 1
           const_expr { bool_value: true }
         )pb"},
        {R"pb(
           id: 1
           const_expr { int64_value: 1 }
         )pb"},
        {R"pb(
           id: 1
           const_expr { uint64_value: 1 }
         )pb"},
        {R"pb(
           id: 1
           const_expr { double_value: 1 }
         )pb"},
        {R"pb(
           id: 1
           const_expr { string_value: "foo" }
         )pb"},
        {R"pb(
           id: 1
           const_expr { bytes_value: "foo" }
         )pb"},
        {R"pb(
           id: 1
           const_expr { duration_value { seconds: 1 nanos: 1 } }
         )pb"},
        {R"pb(
           id: 1
           const_expr { timestamp_value { seconds: 1 nanos: 1 } }
         )pb"},
        {R"pb(
           id: 1
           ident_expr { name: "foo" }
         )pb"},
        {R"pb(
           id: 1
           select_expr {
             operand {
               id: 2
               ident_expr { name: "bar" }
             }
             field: "foo"
             test_only: true
           }
         )pb"},
        {R"pb(
           id: 1
           call_expr {
             target {
               id: 2
               ident_expr { name: "bar" }
             }
             function: "foo"
             args {
               id: 3
               ident_expr { name: "baz" }
             }
           }
         )pb"},
        {R"pb(
           id: 1
           list_expr {
             elements {
               id: 2
               ident_expr { name: "bar" }
             }
             elements {
               id: 3
               ident_expr { name: "baz" }
             }
             optional_indices: 0
           }
         )pb"},
        {R"pb(
           id: 1
           struct_expr {
             message_name: "google.type.Expr"
             entries {
               id: 2
               field_key: "description"
               value {
                 id: 3
                 const_expr { string_value: "foo" }
               }
               optional_entry: true
             }
             entries {
               id: 4
               field_key: "expr"
               value {
                 id: 5
                 const_expr { string_value: "bar" }
               }
             }
           }
         )pb"},
        {R"pb(
           id: 1
           struct_expr {
             entries {
               id: 2
               map_key {
                 id: 3
                 const_expr { string_value: "description" }
               }
               value {
                 id: 4
                 const_expr { string_value: "foo" }
               }
               optional_entry: true
             }
             entries {
               id: 5
               map_key {
                 id: 6
                 const_expr { string_value: "expr" }
               }
               value {
                 id: 7
                 const_expr { string_value: "foo" }
               }
               optional_entry: true
             }
           }
         )pb"},
        {R"pb(
           id: 1
           comprehension_expr {
             iter_var: "foo"
             iter_range {
               id: 2
               list_expr {}
             }
             accu_var: "bar"
             accu_init {
               id: 3
               list_expr {}
             }
             loop_condition {
               id: 4
               const_expr { bool_value: true }
             }
             loop_step {
               id: 4
               ident_expr { name: "bar" }
             }
             result {
               id: 5
               ident_expr { name: "foo" }
             }
           }
         )pb"},
    }));

TEST(ExprFromProto, StructFieldInMap) {
  ExprProto original_proto;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(
                                            id: 1
                                            struct_expr: {
                                              entries: {
                                                id: 2
                                                field_key: "foo"
                                                value: {
                                                  id: 3
                                                  ident_expr: { name: "bar" }
                                                }
                                              }
                                            }
                                          )pb",
                                          &original_proto));
  Expr expr;
  ASSERT_THAT(ExprFromProto(original_proto, expr),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ExprFromProto, MapEntryInStruct) {
  ExprProto original_proto;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(
                                            id: 1
                                            struct_expr: {
                                              message_name: "some.Message"
                                              entries: {
                                                id: 2
                                                map_key: {
                                                  id: 3
                                                  ident_expr: { name: "foo" }
                                                }
                                                value: {
                                                  id: 4
                                                  ident_expr: { name: "bar" }
                                                }
                                              }
                                            }
                                          )pb",
                                          &original_proto));
  Expr expr;
  ASSERT_THAT(ExprFromProto(original_proto, expr),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
