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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or astied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "testutil/baseline_tests.h"

#include <memory>
#include <string>

#include "common/ast.h"
#include "internal/testing.h"
#include "google/protobuf/text_format.h"

namespace cel::test {
namespace {

using ::cel::expr::CheckedExpr;

TEST(FormatBaselineAst, Basic) {
  Ast ast;
  ast.mutable_root_expr().mutable_ident_expr().set_name("foo");
  ast.mutable_root_expr().set_id(1);
  ast.mutable_type_map()[1] = TypeSpec(PrimitiveType::kInt64);
  ast.mutable_reference_map()[1].set_name("foo");

  EXPECT_EQ(FormatBaselineAst(ast), "foo~int^foo");
}

TEST(FormatBaselineAst, NoType) {
  Ast ast;
  ast.mutable_root_expr().mutable_ident_expr().set_name("foo");
  ast.mutable_root_expr().set_id(1);
  ast.mutable_reference_map()[1].set_name("foo");

  EXPECT_EQ(FormatBaselineAst(ast), "foo^foo");
}

TEST(FormatBaselineAst, NoReference) {
  Ast ast;
  ast.mutable_root_expr().mutable_ident_expr().set_name("foo");
  ast.mutable_root_expr().set_id(1);
  ast.mutable_type_map()[1] = TypeSpec(PrimitiveType::kInt64);

  EXPECT_EQ(FormatBaselineAst(ast), "foo~int");
}

TEST(FormatBaselineAst, MutlipleReferences) {
  Ast ast;
  ast.mutable_root_expr().mutable_call_expr().set_function("_+_");
  ast.mutable_root_expr().set_id(1);
  ast.mutable_type_map()[1] = TypeSpec(DynTypeSpec());
  ast.mutable_reference_map()[1].mutable_overload_id().push_back(
      "add_timestamp_duration");
  ast.mutable_reference_map()[1].mutable_overload_id().push_back(
      "add_duration_duration");
  {
    auto& arg1 = ast.mutable_root_expr().mutable_call_expr().add_args();
    arg1.mutable_ident_expr().set_name("a");
    arg1.set_id(2);
    ast.mutable_type_map()[2] = TypeSpec(DynTypeSpec());
    ast.mutable_reference_map()[2].set_name("a");
  }
  {
    auto& arg2 = ast.mutable_root_expr().mutable_call_expr().add_args();
    arg2.mutable_ident_expr().set_name("b");
    arg2.set_id(3);
    ast.mutable_type_map()[3] = TypeSpec(WellKnownTypeSpec::kDuration);
    ast.mutable_reference_map()[3].set_name("b");
  }

  EXPECT_EQ(FormatBaselineAst(ast),
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
  TypeSpec type;
  std::string expected_string;
};

class FormatBaselineTypeSpecTest : public testing::TestWithParam<TestCase> {};

TEST_P(FormatBaselineTypeSpecTest, Runner) {
  Ast ast;
  ast.mutable_root_expr().set_id(1);
  ast.mutable_root_expr().mutable_ident_expr().set_name("x");
  ast.mutable_type_map()[1] = GetParam().type;

  EXPECT_EQ(FormatBaselineAst(ast), GetParam().expected_string);
}

INSTANTIATE_TEST_SUITE_P(
    Types, FormatBaselineTypeSpecTest,
    ::testing::Values(
        TestCase{TypeSpec(PrimitiveType::kBool), "x~bool"},
        TestCase{TypeSpec(PrimitiveType::kInt64), "x~int"},
        TestCase{TypeSpec(PrimitiveType::kUint64), "x~uint"},
        TestCase{TypeSpec(PrimitiveType::kDouble), "x~double"},
        TestCase{TypeSpec(PrimitiveType::kString), "x~string"},
        TestCase{TypeSpec(PrimitiveType::kBytes), "x~bytes"},
        TestCase{TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kBool)),
                 "x~wrapper(bool)"},
        TestCase{TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kInt64)),
                 "x~wrapper(int)"},
        TestCase{TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kUint64)),
                 "x~wrapper(uint)"},
        TestCase{TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kDouble)),
                 "x~wrapper(double)"},
        TestCase{TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kString)),
                 "x~wrapper(string)"},
        TestCase{TypeSpec(PrimitiveTypeWrapper(PrimitiveType::kBytes)),
                 "x~wrapper(bytes)"},
        TestCase{TypeSpec(WellKnownTypeSpec::kAny), "x~google.protobuf.Any"},
        TestCase{TypeSpec(WellKnownTypeSpec::kDuration),
                 "x~google.protobuf.Duration"},
        TestCase{TypeSpec(WellKnownTypeSpec::kTimestamp),
                 "x~google.protobuf.Timestamp"},
        TestCase{TypeSpec(DynTypeSpec()), "x~dyn"},
        TestCase{TypeSpec(NullTypeSpec()), "x~null"},
        TestCase{TypeSpec(UnsetTypeSpec()), "x~<error>"},
        TestCase{TypeSpec(MessageTypeSpec("com.example.Type")),
                 "x~com.example.Type"},
        TestCase{TypeSpec(AbstractType("optional_type",
                                       {TypeSpec(PrimitiveType::kInt64)})),
                 "x~optional_type(int)"},
        TestCase{TypeSpec(std::make_unique<TypeSpec>()), "x~type"},
        TestCase{TypeSpec(std::make_unique<TypeSpec>(PrimitiveType::kInt64)),
                 "x~type(int)"},
        TestCase{TypeSpec(ParamTypeSpec("T")), "x~T"},
        TestCase{TypeSpec(MapTypeSpec(
                     std::make_unique<TypeSpec>(PrimitiveType::kString),
                     std::make_unique<TypeSpec>(PrimitiveType::kString))),
                 "x~map(string, string)"},
        TestCase{TypeSpec(ListTypeSpec(
                     std::make_unique<TypeSpec>(PrimitiveType::kString))),
                 "x~list(string)"}));

}  // namespace
}  // namespace cel::test
