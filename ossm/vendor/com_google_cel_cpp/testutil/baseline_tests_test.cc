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

#include "testutil/baseline_tests.h"

#include <memory>
#include <string>

#include "common/ast/ast_impl.h"
#include "common/ast/expr.h"
#include "internal/testing.h"
#include "google/protobuf/text_format.h"

namespace cel::test {
namespace {

using ::cel::ast_internal::AstImpl;
using ::cel::expr::CheckedExpr;

using AstType = ast_internal::Type;

TEST(FormatBaselineAst, Basic) {
  AstImpl impl;
  impl.root_expr().mutable_ident_expr().set_name("foo");
  impl.root_expr().set_id(1);
  impl.type_map()[1] = AstType(ast_internal::PrimitiveType::kInt64);
  impl.reference_map()[1].set_name("foo");

  EXPECT_EQ(FormatBaselineAst(impl), "foo~int^foo");
}

TEST(FormatBaselineAst, NoType) {
  AstImpl impl;
  impl.root_expr().mutable_ident_expr().set_name("foo");
  impl.root_expr().set_id(1);
  impl.reference_map()[1].set_name("foo");

  EXPECT_EQ(FormatBaselineAst(impl), "foo^foo");
}

TEST(FormatBaselineAst, NoReference) {
  AstImpl impl;
  impl.root_expr().mutable_ident_expr().set_name("foo");
  impl.root_expr().set_id(1);
  impl.type_map()[1] = AstType(ast_internal::PrimitiveType::kInt64);

  EXPECT_EQ(FormatBaselineAst(impl), "foo~int");
}

TEST(FormatBaselineAst, MutlipleReferences) {
  AstImpl impl;
  impl.root_expr().mutable_call_expr().set_function("_+_");
  impl.root_expr().set_id(1);
  impl.type_map()[1] = AstType(ast_internal::DynamicType());
  impl.reference_map()[1].mutable_overload_id().push_back(
      "add_timestamp_duration");
  impl.reference_map()[1].mutable_overload_id().push_back(
      "add_duration_duration");
  {
    auto& arg1 = impl.root_expr().mutable_call_expr().add_args();
    arg1.mutable_ident_expr().set_name("a");
    arg1.set_id(2);
    impl.type_map()[2] = AstType(ast_internal::DynamicType());
    impl.reference_map()[2].set_name("a");
  }
  {
    auto& arg2 = impl.root_expr().mutable_call_expr().add_args();
    arg2.mutable_ident_expr().set_name("b");
    arg2.set_id(3);
    impl.type_map()[3] = AstType(ast_internal::WellKnownType::kDuration);
    impl.reference_map()[3].set_name("b");
  }

  EXPECT_EQ(FormatBaselineAst(impl),
            "_+_(\n"
            "  a~dyn^a,\n"
            "  b~google.protobuf.Duration^b\n"
            ")~dyn^add_timestamp_duration|add_duration_duration");
}

TEST(FormatBaselineCheckedExpr, MutlipleReferences) {
  CheckedExpr checked;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          id: 1
          call_expr {
            function: "_+_"
            args {
              id: 2
              ident_expr { name: "a" }
            }
            args {
              id: 3
              ident_expr { name: "b" }
            }
          }
        }
        type_map {
          key: 1
          value { dyn {} }
        }
        type_map {
          key: 2
          value { dyn {} }
        }
        type_map {
          key: 3
          value { well_known: DURATION }
        }
        reference_map {
          key: 1
          value {
            overload_id: "add_timestamp_duration"
            overload_id: "add_duration_duration"
          }
        }
        reference_map {
          key: 2
          value { name: "a" }
        }
        reference_map {
          key: 3
          value { name: "b" }
        }
      )pb",
      &checked));

  EXPECT_EQ(FormatBaselineCheckedExpr(checked),
            "_+_(\n"
            "  a~dyn^a,\n"
            "  b~google.protobuf.Duration^b\n"
            ")~dyn^add_timestamp_duration|add_duration_duration");
}

struct TestCase {
  AstType type;
  std::string expected_string;
};

class FormatBaselineAstTypeTest : public testing::TestWithParam<TestCase> {};

TEST_P(FormatBaselineAstTypeTest, Runner) {
  AstImpl impl;
  impl.root_expr().set_id(1);
  impl.root_expr().mutable_ident_expr().set_name("x");
  impl.type_map()[1] = GetParam().type;

  EXPECT_EQ(FormatBaselineAst(impl), GetParam().expected_string);
}

INSTANTIATE_TEST_SUITE_P(
    Types, FormatBaselineAstTypeTest,
    ::testing::Values(
        TestCase{AstType(ast_internal::PrimitiveType::kBool), "x~bool"},
        TestCase{AstType(ast_internal::PrimitiveType::kInt64), "x~int"},
        TestCase{AstType(ast_internal::PrimitiveType::kUint64), "x~uint"},
        TestCase{AstType(ast_internal::PrimitiveType::kDouble), "x~double"},
        TestCase{AstType(ast_internal::PrimitiveType::kString), "x~string"},
        TestCase{AstType(ast_internal::PrimitiveType::kBytes), "x~bytes"},
        TestCase{AstType(ast_internal::PrimitiveTypeWrapper(
                     ast_internal::PrimitiveType::kBool)),
                 "x~wrapper(bool)"},
        TestCase{AstType(ast_internal::PrimitiveTypeWrapper(
                     ast_internal::PrimitiveType::kInt64)),
                 "x~wrapper(int)"},
        TestCase{AstType(ast_internal::PrimitiveTypeWrapper(
                     ast_internal::PrimitiveType::kUint64)),
                 "x~wrapper(uint)"},
        TestCase{AstType(ast_internal::PrimitiveTypeWrapper(
                     ast_internal::PrimitiveType::kDouble)),
                 "x~wrapper(double)"},
        TestCase{AstType(ast_internal::PrimitiveTypeWrapper(
                     ast_internal::PrimitiveType::kString)),
                 "x~wrapper(string)"},
        TestCase{AstType(ast_internal::PrimitiveTypeWrapper(
                     ast_internal::PrimitiveType::kBytes)),
                 "x~wrapper(bytes)"},
        TestCase{AstType(ast_internal::WellKnownType::kAny),
                 "x~google.protobuf.Any"},
        TestCase{AstType(ast_internal::WellKnownType::kDuration),
                 "x~google.protobuf.Duration"},
        TestCase{AstType(ast_internal::WellKnownType::kTimestamp),
                 "x~google.protobuf.Timestamp"},
        TestCase{AstType(ast_internal::DynamicType()), "x~dyn"},
        TestCase{AstType(nullptr), "x~null"},
        TestCase{AstType(ast_internal::UnspecifiedType()), "x~<error>"},
        TestCase{AstType(ast_internal::MessageType("com.example.Type")),
                 "x~com.example.Type"},
        TestCase{AstType(ast_internal::AbstractType(
                     "optional_type",
                     {AstType(ast_internal::PrimitiveType::kInt64)})),
                 "x~optional_type(int)"},
        TestCase{AstType(std::make_unique<AstType>()), "x~type"},
        TestCase{AstType(std::make_unique<AstType>(
                     ast_internal::PrimitiveType::kInt64)),
                 "x~type(int)"},
        TestCase{AstType(ast_internal::ParamType("T")), "x~T"},
        TestCase{
            AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kString),
                std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kString))),
            "x~map(string, string)"},
        TestCase{AstType(ast_internal::ListType(std::make_unique<AstType>(
                     ast_internal::PrimitiveType::kString))),
                 "x~list(string)"}));

}  // namespace
}  // namespace cel::test
