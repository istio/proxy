// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Tests for memory safety using the CEL Evaluator.
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::rpc::context::AttributeContext;
using testutil::EqualsProto;

struct TestCase {
  std::string name;
  std::string expression;
  absl::flat_hash_map<absl::string_view, CelValue> activation;
  test::CelValueMatcher expected_matcher;
  bool reference_resolver_enabled = false;
};

enum Options { kDefault, kExhaustive, kFoldConstants };

using ParamType = std::tuple<TestCase, Options>;

std::string TestCaseName(const testing::TestParamInfo<ParamType>& param_info) {
  const ParamType& param = param_info.param;
  absl::string_view opt;
  switch (std::get<1>(param)) {
    case Options::kDefault:
      opt = "default";
      break;
    case Options::kExhaustive:
      opt = "exhaustive";
      break;
    case Options::kFoldConstants:
      opt = "opt";
      break;
  }

  return absl::StrCat(std::get<0>(param).name, "_", opt);
}

class EvaluatorMemorySafetyTest : public testing::TestWithParam<ParamType> {
 public:
  EvaluatorMemorySafetyTest() {
    google::protobuf::LinkMessageReflection<AttributeContext>();
    google::protobuf::LinkMessageReflection<AttributeContext::Request>();
    google::protobuf::LinkMessageReflection<AttributeContext::Peer>();
  }

 protected:
  const TestCase& GetTestCase() { return std::get<0>(GetParam()); }

  InterpreterOptions GetOptions() {
    InterpreterOptions options;
    options.constant_arena = &arena_;

    switch (std::get<1>(GetParam())) {
      case Options::kDefault:
        options.enable_regex_precompilation = false;
        options.constant_folding = false;
        options.enable_comprehension_list_append = false;
        options.enable_comprehension_vulnerability_check = true;
        options.short_circuiting = true;
        break;
      case Options::kExhaustive:
        options.enable_regex_precompilation = false;
        options.constant_folding = false;
        options.enable_comprehension_list_append = false;
        options.enable_comprehension_vulnerability_check = true;
        options.short_circuiting = false;
        break;
      case Options::kFoldConstants:
        options.enable_regex_precompilation = true;
        options.constant_folding = true;
        options.enable_comprehension_list_append = true;
        options.enable_comprehension_vulnerability_check = false;
        options.short_circuiting = true;
        break;
    }

    options.enable_qualified_identifier_rewrites =
        GetTestCase().reference_resolver_enabled;

    return options;
  }

  google::protobuf::Arena arena_;
};

bool IsPrivateIpv4Impl(google::protobuf::Arena* arena, CelValue::StringHolder addr) {
  // Implementation for demonstration, this is simple but incomplete and
  // brittle.
  return absl::StartsWith(addr.value(), "192.168.") ||
         absl::StartsWith(addr.value(), "10.");
}

TEST_P(EvaluatorMemorySafetyTest, Basic) {
  const auto& test_case = GetTestCase();
  InterpreterOptions options = GetOptions();

  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  builder->set_container("google.rpc.context");
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  absl::string_view function_name = "IsPrivate";
  if (test_case.reference_resolver_enabled) {
    function_name = "net.IsPrivate";
  }
  ASSERT_OK((FunctionAdapter<bool, CelValue::StringHolder>::CreateAndRegister(
      function_name, false, &IsPrivateIpv4Impl, builder->GetRegistry())));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(test_case.expression));

  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  Activation activation;
  for (const auto& [key, value] : test_case.activation) {
    activation.InsertValue(key, value);
  }

  absl::StatusOr<CelValue> got = plan->Evaluate(activation, &arena_);

  EXPECT_THAT(got, IsOkAndHolds(test_case.expected_matcher));
}

// Check no use after free errors if evaluated after AST is freed.
TEST_P(EvaluatorMemorySafetyTest, NoAstDependency) {
  const auto& test_case = GetTestCase();
  InterpreterOptions options = GetOptions();

  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  builder->set_container("google.rpc.context");
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  absl::string_view function_name = "IsPrivate";
  if (test_case.reference_resolver_enabled) {
    function_name = "net.IsPrivate";
  }
  ASSERT_OK((FunctionAdapter<bool, CelValue::StringHolder>::CreateAndRegister(
      function_name, false, &IsPrivateIpv4Impl, builder->GetRegistry())));

  auto parsed_expr = parser::Parse(test_case.expression);
  ASSERT_OK(parsed_expr.status());
  auto expr = std::make_unique<ParsedExpr>(std::move(parsed_expr).value());
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<CelExpression> plan,
      builder->CreateExpression(&expr->expr(), &expr->source_info()));

  expr.reset();  // ParsedExpr expr freed

  Activation activation;
  for (const auto& [key, value] : test_case.activation) {
    activation.InsertValue(key, value);
  }

  absl::StatusOr<CelValue> got = plan->Evaluate(activation, &arena_);

  EXPECT_THAT(got, IsOkAndHolds(test_case.expected_matcher));
}

// TODO: make expression plan memory safe after builder is freed.
// TEST_P(EvaluatorMemorySafetyTest, NoBuilderDependency)

INSTANTIATE_TEST_SUITE_P(
    Expression, EvaluatorMemorySafetyTest,
    testing::Combine(
        testing::ValuesIn(std::vector<TestCase>{
            {
                "bool",
                "(true && false) || x || y == 'test_str'",
                {{"x", CelValue::CreateBool(false)},
                 {"y", CelValue::CreateStringView("test_str")}},
                test::IsCelBool(true),
            },
            {
                "const_str",
                "condition ? 'left_hand_string' : 'right_hand_string'",
                {{"condition", CelValue::CreateBool(false)}},
                test::IsCelString("right_hand_string"),
            },
            {
                "long_const_string",
                "condition ? 'left_hand_string' : "
                "'long_right_hand_string_0123456789'",
                {{"condition", CelValue::CreateBool(false)}},
                test::IsCelString("long_right_hand_string_0123456789"),
            },
            {
                "computed_string",
                "(condition ? 'a.b' : 'b.c') + '.d.e.f'",
                {{"condition", CelValue::CreateBool(false)}},
                test::IsCelString("b.c.d.e.f"),
            },
            {
                "regex",
                R"('192.168.128.64'.matches(r'^192\.168\.[0-2]?[0-9]?[0-9]\.[0-2]?[0-9]?[0-9]') )",
                {},
                test::IsCelBool(true),
            },
            {
                "list_create",
                "[1, 2, 3, 4, 5, 6][3] == 4",
                {},
                test::IsCelBool(true),
            },
            {
                "list_create_strings",
                "['1', '2', '3', '4', '5', '6'][2] == '3'",
                {},
                test::IsCelBool(true),
            },
            {
                "map_create",
                "{'1': 'one', '2': 'two'}['2']",
                {},
                test::IsCelString("two"),
            },
            {
                "struct_create",
                R"(
                  AttributeContext{
                    request: AttributeContext.Request{
                      method: 'GET',
                      path: '/index'
                    },
                    origin: AttributeContext.Peer{
                      ip: '10.0.0.1'
                    }
                  }
                )",
                {},
                test::IsCelMessage(EqualsProto(R"pb(
                  request { method: "GET" path: "/index" }
                  origin { ip: "10.0.0.1" }
                )pb")),
            },
            {"extension_function",
             "IsPrivate('8.8.8.8')",
             {},
             test::IsCelBool(false),
             /*enable_reference_resolver=*/false},
            {"namespaced_function",
             "net.IsPrivate('192.168.0.1')",
             {},
             test::IsCelBool(true),
             /*enable_reference_resolver=*/true},
            {
                "comprehension",
                "['abc', 'def', 'ghi', 'jkl'].exists(el, el == 'mno')",
                {},
                test::IsCelBool(false),
            },
            {
                "comprehension_complex",
                "['a' + 'b' + 'c', 'd' + 'ef', 'g' + 'hi', 'j' + 'kl']"
                ".exists(el, el.startsWith('g'))",
                {},
                test::IsCelBool(true),
            }}),
        testing::Values(Options::kDefault, Options::kExhaustive,
                        Options::kFoldConstants)),
    &TestCaseName);

}  // namespace
}  // namespace google::api::expr::runtime
