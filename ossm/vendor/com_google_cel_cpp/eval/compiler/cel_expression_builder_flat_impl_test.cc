// Copyright 2023 Google LLC
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
//
// Smoke tests for CelExpressionBuilderFlatImpl. This class is a thin wrapper
// over FlatExprBuilder, so most of the tests are just covering the conversion
// code from the legacy APIs to the implementation. See
// flat_expr_builder_test.cc for additional tests.
#include "eval/compiler/cel_expression_builder_flat_impl.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/constant_folding.h"
#include "eval/compiler/regex_precompilation_optimization.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "eval/public/testing/matchers.h"
#include "extensions/bindings_ext.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/macro.h"
#include "parser/parser.h"
#include "runtime/runtime_options.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::v1alpha1::SourceInfo;
using ::google::api::expr::parser::Macro;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::parser::ParseWithMacros;
using ::google::api::expr::test::v1::proto3::NestedTestAllTypes;
using ::google::api::expr::test::v1::proto3::TestAllTypes;
using ::testing::_;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::IsNull;
using ::testing::NotNull;

TEST(CelExpressionBuilderFlatImplTest, Error) {
  Expr expr;
  SourceInfo source_info;
  CelExpressionBuilderFlatImpl builder;
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid empty expression")));
}

TEST(CelExpressionBuilderFlatImplTest, ParsedExpr) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));

  CelExpressionBuilderFlatImpl builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelInt64(3));
}

struct RecursiveTestCase {
  std::string test_name;
  std::string expr;
  test::CelValueMatcher matcher;
};

class RecursivePlanTest : public ::testing::TestWithParam<RecursiveTestCase> {
 protected:
  absl::Status SetupBuilder(CelExpressionBuilderFlatImpl& builder) {
    builder.GetTypeRegistry()->RegisterTypeProvider(
        std::make_unique<ProtobufDescriptorProvider>(
            google::protobuf::DescriptorPool::generated_pool(),
            google::protobuf::MessageFactory::generated_factory()));
    builder.GetTypeRegistry()->RegisterEnum("TestEnum",
                                            {{"FOO", 1}, {"BAR", 2}});

    CEL_RETURN_IF_ERROR(RegisterBuiltinFunctions(builder.GetRegistry()));
    return builder.GetRegistry()->RegisterLazyFunction(CelFunctionDescriptor(
        "LazilyBoundMult", false,
        {CelValue::Type::kInt64, CelValue::Type::kInt64}));
  }

  absl::Status SetupActivation(Activation& activation, google::protobuf::Arena* arena) {
    activation.InsertValue("int_1", CelValue::CreateInt64(1));
    activation.InsertValue("string_abc", CelValue::CreateStringView("abc"));
    activation.InsertValue("string_def", CelValue::CreateStringView("def"));
    auto* map = google::protobuf::Arena::Create<CelMapBuilder>(arena);
    CEL_RETURN_IF_ERROR(
        map->Add(CelValue::CreateStringView("a"), CelValue::CreateInt64(1)));
    CEL_RETURN_IF_ERROR(
        map->Add(CelValue::CreateStringView("b"), CelValue::CreateInt64(2)));
    activation.InsertValue("map_var", CelValue::CreateMap(map));
    auto* msg = google::protobuf::Arena::Create<NestedTestAllTypes>(arena);
    msg->mutable_child()->mutable_payload()->set_single_int64(42);
    activation.InsertValue("struct_var",
                           CelProtoWrapper::CreateMessage(msg, arena));
    activation.InsertValue("TestEnum.BAR", CelValue::CreateInt64(-1));

    CEL_RETURN_IF_ERROR(activation.InsertFunction(
        PortableBinaryFunctionAdapter<int64_t, int64_t, int64_t>::Create(
            "LazilyBoundMult", false,
            [](google::protobuf::Arena*, int64_t lhs, int64_t rhs) -> int64_t {
              return lhs * rhs;
            })));

    return absl::OkStatus();
  }
};

absl::StatusOr<ParsedExpr> ParseWithBind(absl::string_view cel) {
  static const std::vector<Macro>* kMacros = []() {
    auto* result = new std::vector<Macro>(Macro::AllMacros());
    absl::c_copy(cel::extensions::bindings_macros(),
                 std::back_inserter(*result));
    return result;
  }();
  return ParseWithMacros(cel, *kMacros, "<input>");
}

TEST_P(RecursivePlanTest, ParsedExprRecursiveImpl) {
  const RecursiveTestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, ParseWithBind(test_case.expr));
  cel::RuntimeOptions options;
  options.container = "google.api.expr.test.v1.proto3";
  google::protobuf::Arena arena;
  // Unbounded.
  options.max_recursion_depth = -1;
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK(SetupBuilder(builder));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  EXPECT_THAT(dynamic_cast<const CelExpressionRecursiveImpl*>(plan.get()),
              NotNull());

  Activation activation;

  ASSERT_OK(SetupActivation(activation, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test_case.matcher);
}

TEST_P(RecursivePlanTest, ParsedExprRecursiveOptimizedImpl) {
  const RecursiveTestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, ParseWithBind(test_case.expr));
  cel::RuntimeOptions options;
  options.container = "google.api.expr.test.v1.proto3";
  google::protobuf::Arena arena;
  // Unbounded.
  options.max_recursion_depth = -1;
  options.enable_comprehension_list_append = true;
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK(SetupBuilder(builder));

  builder.flat_expr_builder().AddProgramOptimizer(
      cel::runtime_internal::CreateConstantFoldingOptimizer(
          cel::extensions::ProtoMemoryManagerRef(&arena)));
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateRegexPrecompilationExtension(options.regex_max_program_size));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  EXPECT_THAT(dynamic_cast<const CelExpressionRecursiveImpl*>(plan.get()),
              NotNull());

  Activation activation;

  ASSERT_OK(SetupActivation(activation, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test_case.matcher);
}

TEST_P(RecursivePlanTest, ParsedExprRecursiveTraceSupport) {
  const RecursiveTestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, ParseWithBind(test_case.expr));
  cel::RuntimeOptions options;
  options.container = "google.api.expr.test.v1.proto3";
  google::protobuf::Arena arena;
  auto cb = [](int64_t id, const CelValue& value, google::protobuf::Arena* arena) {
    return absl::OkStatus();
  };
  // Unbounded.
  options.max_recursion_depth = -1;
  options.enable_recursive_tracing = true;
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK(SetupBuilder(builder));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  EXPECT_THAT(dynamic_cast<const CelExpressionRecursiveImpl*>(plan.get()),
              NotNull());

  Activation activation;

  ASSERT_OK(SetupActivation(activation, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Trace(activation, &arena, cb));
  EXPECT_THAT(result, test_case.matcher);
}

TEST_P(RecursivePlanTest, Disabled) {
  google::protobuf::LinkMessageReflection<TestAllTypes>();

  const RecursiveTestCase& test_case = GetParam();
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, ParseWithBind(test_case.expr));
  cel::RuntimeOptions options;
  options.container = "google.api.expr.test.v1.proto3";
  google::protobuf::Arena arena;
  // disabled.
  options.max_recursion_depth = 0;
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK(SetupBuilder(builder));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  EXPECT_THAT(dynamic_cast<const CelExpressionRecursiveImpl*>(plan.get()),
              IsNull());

  Activation activation;

  ASSERT_OK(SetupActivation(activation, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test_case.matcher);
}

INSTANTIATE_TEST_SUITE_P(
    RecursivePlanTest, RecursivePlanTest,
    testing::ValuesIn(std::vector<RecursiveTestCase>{
        {"constant", "'abc'", test::IsCelString("abc")},
        {"call", "1 + 2", test::IsCelInt64(3)},
        {"nested_call", "1 + 1 + 1 + 1", test::IsCelInt64(4)},
        {"and", "true && false", test::IsCelBool(false)},
        {"or", "true || false", test::IsCelBool(true)},
        {"ternary", "(true || false) ? 2 + 2 : 3 + 3", test::IsCelInt64(4)},
        {"create_list", "3 in [1, 2, 3]", test::IsCelBool(true)},
        {"create_list_complex", "3 in [2 / 2, 4 / 2, 6 / 2]",
         test::IsCelBool(true)},
        {"ident", "int_1 == 1", test::IsCelBool(true)},
        {"ident_complex", "int_1 + 2 > 4 ? string_abc : string_def",
         test::IsCelString("def")},
        {"select", "struct_var.child.payload.single_int64",
         test::IsCelInt64(42)},
        {"nested_select", "[map_var.a, map_var.b].size() == 2",
         test::IsCelBool(true)},
        {"map_index", "map_var['b']", test::IsCelInt64(2)},
        {"list_index", "[1, 2, 3][1]", test::IsCelInt64(2)},
        {"compre_exists", "[1, 2, 3, 4].exists(x, x == 3)",
         test::IsCelBool(true)},
        {"compre_map", "8 in [1, 2, 3, 4].map(x, x * 2)",
         test::IsCelBool(true)},
        {"map_var_compre_exists", "map_var.exists(key, key == 'b')",
         test::IsCelBool(true)},
        {"map_compre_exists", "{'a': 1, 'b': 2}.exists(k, k == 'b')",
         test::IsCelBool(true)},
        {"create_map", "{'a': 42, 'b': 0, 'c': 0}.size()", test::IsCelInt64(3)},
        {"create_struct",
         "NestedTestAllTypes{payload: TestAllTypes{single_int64: "
         "-42}}.payload.single_int64",
         test::IsCelInt64(-42)},
        {"bind", R"(cel.bind(x, "1", x + x + x + x))",
         test::IsCelString("1111")},
        {"nested_bind", R"(cel.bind(x, 20, cel.bind(y, 30, x + y)))",
         test::IsCelInt64(50)},
        {"bind_with_comprehensions",
         R"(cel.bind(x, [1, 2], cel.bind(y, x.map(z, z * 2), y.exists(z, z == 4))))",
         test::IsCelBool(true)},
        {"shadowable_value_default", R"(TestEnum.FOO == 1)",
         test::IsCelBool(true)},
        {"shadowable_value_shadowed", R"(TestEnum.BAR == -1)",
         test::IsCelBool(true)},
        {"lazily_resolved_function", "LazilyBoundMult(123, 2) == 246",
         test::IsCelBool(true)},
        {"re_matches", "matches(string_abc, '[ad][be][cf]')",
         test::IsCelBool(true)},
        {"re_matches_receiver",
         "(string_abc + string_def).matches(r'(123)?' + r'abc' + r'def')",
         test::IsCelBool(true)},
    }),

    [](const testing::TestParamInfo<RecursiveTestCase>& info) -> std::string {
      return info.param.test_name;
    });

TEST(CelExpressionBuilderFlatImplTest, ParsedExprWithWarnings) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));
  cel::RuntimeOptions options;
  options.fail_on_warnings = false;

  CelExpressionBuilderFlatImpl builder(options);
  std::vector<absl::Status> warnings;

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelExpression> plan,
      builder.CreateExpression(&parsed_expr.expr(), &parsed_expr.source_info(),
                               &warnings));

  EXPECT_THAT(warnings, Contains(StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("No overloads"))));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelError(
                          StatusIs(_, HasSubstr("No matching overloads"))));
}

TEST(CelExpressionBuilderFlatImplTest, CheckedExpr) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));
  CheckedExpr checked_expr;
  checked_expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  checked_expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());

  CelExpressionBuilderFlatImpl builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&checked_expr));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelInt64(3));
}

TEST(CelExpressionBuilderFlatImplTest, CheckedExprWithWarnings) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("1 + 2"));
  CheckedExpr checked_expr;
  checked_expr.mutable_expr()->Swap(parsed_expr.mutable_expr());
  checked_expr.mutable_source_info()->Swap(parsed_expr.mutable_source_info());
  cel::RuntimeOptions options;
  options.fail_on_warnings = false;

  CelExpressionBuilderFlatImpl builder(options);
  std::vector<absl::Status> warnings;

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> plan,
                       builder.CreateExpression(&checked_expr, &warnings));

  EXPECT_THAT(warnings, Contains(StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr("No overloads"))));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelError(
                          StatusIs(_, HasSubstr("No matching overloads"))));
}

}  // namespace

}  // namespace google::api::expr::runtime
