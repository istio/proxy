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
#include "testing/testrunner/runner_lib.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest-spi.h"
#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/ast_proto.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "testing/testrunner/cel_expression_source.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/coverage_index.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(std::string, test_cel_file_path, "",
          "Path to the .cel file for testing");

namespace cel::test {
namespace {

using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::CheckedExpr;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ValueProto = ::cel::expr::Value;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::StartsWith;

template <typename T>
T ParseTextProtoOrDie(absl::string_view text_proto) {
  T result;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text_proto, &result));
  return result;
}

int CountSubstrings(absl::string_view text, absl::string_view substr) {
  int count = 0;
  size_t pos = 0;
  while ((pos = text.find(substr, pos)) != absl::string_view::npos) {
    ++count;
    ++pos;
  }
  return count;
}

absl::StatusOr<std::unique_ptr<cel::Compiler>> CreateBasicCompiler() {
  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<cel::CompilerBuilder> builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  CEL_RETURN_IF_ERROR(builder->AddLibrary(cel::StandardCompilerLibrary()));
  cel::TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
  CEL_RETURN_IF_ERROR(
      checker_builder.AddVariable(cel::MakeVariableDecl("x", cel::IntType())));
  CEL_RETURN_IF_ERROR(
      checker_builder.AddVariable(cel::MakeVariableDecl("y", cel::IntType())));
  return std::move(builder)->Build();
}

absl::StatusOr<std::unique_ptr<const cel::Runtime>> CreateTestRuntime() {
  CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder standard_runtime_builder,
                       cel::CreateStandardRuntimeBuilder(
                           cel::internal::GetTestingDescriptorPool(), {}));
  return std::move(standard_runtime_builder).Build();
}

absl::StatusOr<std::unique_ptr<CelExpressionBuilder>>
CreateTestCelExpressionBuilder() {
  auto builder = google::api::expr::runtime::CreateCelExpressionBuilder();
  CEL_RETURN_IF_ERROR(google::api::expr::runtime::RegisterBuiltinFunctions(
      builder->GetRegistry()));
  return builder;
}

// Creates a static, singleton instance of the basic compiler to be shared
// across tests, avoiding repeated setup costs.
const cel::Compiler& DefaultCompiler() {
  static const cel::Compiler* instance = []() {
    absl::StatusOr<std::unique_ptr<cel::Compiler>> s = CreateBasicCompiler();
    ABSL_QCHECK_OK(s.status());
    return s->release();
  }();
  return *instance;
}

enum class RuntimeApi { kRuntime, kBuilder };

// Parameterized test fixture for tests that are run against both the Runtime
// and the CelExpressionBuilder backends.
class TestRunnerParamTest : public ::testing::TestWithParam<RuntimeApi> {
 protected:
  // Helper to create the appropriate CelTestContext based on the test
  // parameter.
  absl::StatusOr<std::unique_ptr<CelTestContext>> CreateTestContext() {
    if (GetParam() == RuntimeApi::kRuntime) {
      CEL_ASSIGN_OR_RETURN(std::unique_ptr<const cel::Runtime> runtime,
                           CreateTestRuntime());
      return CelTestContext::CreateFromRuntime(std::move(runtime));
    }
    CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpressionBuilder> builder,
                         CreateTestCelExpressionBuilder());
    return CelTestContext::CreateFromCelExpressionBuilder(std::move(builder));
  }
};

TEST_P(TestRunnerParamTest, BasicTestReportsSuccess) {
  ASSERT_OK_AND_ASSIGN(
      cel::ValidationResult validation_result,
      DefaultCompiler().Compile("{'sum': x + y, 'literal': 3}"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output {
      result_value {
        map_value {
          entries {
            key { string_value: "literal" }
            value { int64_value: 3 }
          }
          entries {
            key { string_value: "sum" }
            value { int64_value: 3 }
          }
        }
      }
    }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());

  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));

  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_P(TestRunnerParamTest, BasicTestReportsFailure) {
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y == 3"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output { result_value { bool_value: false } }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "bool_value: true");  // expected true got false
}

TEST_P(TestRunnerParamTest, DynamicInputAndOutputReportsSuccess) {
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { expr: "1 + 1" }
    }
    input {
      key: "y"
      value { expr: "10 - 7" }
    }
    output { result_expr: "7 - 2" }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  context->SetCompiler(std::move(compiler));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_P(TestRunnerParamTest, DynamicInputAndOutputReportsFailure) {
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { expr: "1 + 1" }
    }
    input {
      key: "y"
      value { expr: "10 - 7" }
    }
    output { result_expr: "10" }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  context->SetCompiler(std::move(compiler));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "int64_value: 5");  // expected 5 got 10
}

TEST_P(TestRunnerParamTest, RawExpressionWithCompilerReportsSuccess) {
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 10 } }
    }
    input {
      key: "y"
      value { value { int64_value: 3 } }
    }
    output { result_value { int64_value: 7 } }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  context->SetCompiler(std::move(compiler));
  context->SetExpressionSource(CelExpressionSource::FromRawExpression("x - y"));
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_P(TestRunnerParamTest, RawExpressionWithCompilerReportsFailure) {
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 10 } }
    }
    input {
      key: "y"
      value { value { int64_value: 3 } }
    }
    output { result_value { int64_value: 100 } }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  context->SetCompiler(std::move(compiler));
  context->SetExpressionSource(CelExpressionSource::FromRawExpression("x - y"));
  TestRunner test_runner(std::move(context));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "int64_value: 7");  // expected 7 got 100
}

TEST_P(TestRunnerParamTest, CelFileWithCompilerReportsSuccess) {
  const std::string cel_file_path = absl::GetFlag(FLAGS_test_cel_file_path);
  ASSERT_FALSE(cel_file_path.empty())
      << "Flag --test_cel_file_path must be set";
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 10 } }
    }
    input {
      key: "y"
      value { value { int64_value: 3 } }
    }
    output { result_value { int64_value: 7 } }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  context->SetCompiler(std::move(compiler));
  context->SetExpressionSource(CelExpressionSource::FromCelFile(cel_file_path));
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_P(TestRunnerParamTest, CelFileWithCompilerReportsFailure) {
  const std::string cel_file_path = absl::GetFlag(FLAGS_test_cel_file_path);
  ASSERT_FALSE(cel_file_path.empty())
      << "Flag --test_cel_file_path must be set";
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 10 } }
    }
    input {
      key: "y"
      value { value { int64_value: 3 } }
    }
    output { result_value { int64_value: 123 } }
  )pb");
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       CreateBasicCompiler());
  context->SetCompiler(std::move(compiler));
  context->SetExpressionSource(CelExpressionSource::FromCelFile(cel_file_path));
  TestRunner test_runner(std::move(context));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "int64_value: 7");  // expected 7 got 123
}

TEST_P(TestRunnerParamTest, BasicTestWithCustomBindingsSucceeds) {
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());

  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 10 } }
    }
    output { result_value { int64_value: 15 } }
  )pb");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  absl::flat_hash_map<std::string, ValueProto> bindings;
  bindings["y"] = ParseTextProtoOrDie<ValueProto>(R"pb(int64_value: 5)pb");
  context->SetCustomBindings(std::move(bindings));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));

  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST_P(TestRunnerParamTest, BasicTestWithCustomBindingsReportsFailure) {
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());

  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 10 } }
    }
    output { result_value { int64_value: 999 } }
  )pb");

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelTestContext> context,
                       CreateTestContext());
  absl::flat_hash_map<std::string, ValueProto> bindings;
  bindings["y"] = ParseTextProtoOrDie<ValueProto>(R"pb(int64_value: 5)pb");
  context->SetCustomBindings(std::move(bindings));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));

  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "int64_value: 15");  // expected 15 got 999.
}

INSTANTIATE_TEST_SUITE_P(TestRunnerTests, TestRunnerParamTest,
                         ::testing::Values(RuntimeApi::kRuntime,
                                           RuntimeApi::kBuilder));

TEST(TestRunnerStandaloneTest, DynamicInputWithoutCompilerFails) {
  const std::string expected_error =
      "INVALID_ARGUMENT: A compiler must be provided to compile a raw "
      "expression or .cel file.";

  EXPECT_FATAL_FAILURE(
      {
        //  Create a compiler.
        ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                             CreateBasicCompiler());

        ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                             compiler->Compile("x + y"));
        CheckedExpr checked_expr;
        ASSERT_THAT(
            cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
            absl_testing::IsOk());

        TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
          input {
            key: "x"
            value { expr: "1 + 1" }
          }
          input {
            key: "y"
            value { value { int64_value: 2 } }
          }
          output { result_value { int64_value: 3 } }
        )pb");

        //  Create the expression builder.
        ASSERT_OK_AND_ASSIGN(auto builder, CreateTestCelExpressionBuilder());

        //  Create the TestRunner without the compiler.
        std::unique_ptr<CelTestContext> context =
            CelTestContext::CreateFromCelExpressionBuilder(
                /*cel_expression_builder=*/std::move(builder));
        context->SetExpressionSource(
            CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
        TestRunner test_runner(std::move(context));

        test_runner.RunTest(test_case);
      },
      expected_error);
}

TEST(TestRunnerStandaloneTest,
     RuntimeUsesRuntimePoolToResolveCustomProtoLiteral) {
  //  Create a custom CompilerBuilder.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  cel::TypeCheckerBuilder& checker_builder = builder->GetCheckerBuilder();
  ASSERT_THAT(checker_builder.AddVariable(cel::MakeVariableDecl(
                  "custom_var", cel::MessageType(TestAllTypes::descriptor()))),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Compiler> compiler,
                       std::move(builder)->Build());

  //  Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("custom_var.single_int32 == 123"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());

  //  Create a runtime configured with the testing descriptor pool.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());

  //  Define the test case. The important part is the "custom_var" input,
  //  which forces 'ResolveValue' to run on a custom type. This succeeds because
  //  the testing descriptor pool (used by CreateTestRuntime()) is configured
  //  to contain the TestAllTypes descriptor.
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "custom_var"
      value {
        value {
          object_value {
            [type.googleapis.com/cel.expr.conformance.proto3.TestAllTypes] {
              single_int32: 123
            }
          }
        }
      }
    }
    output { result_value { bool_value: true } }
  )pb");

  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST(TestRunnerStandaloneTest, RunTestFailsWhenNoExpressionSourceIsProvided) {
  const std::string expected_error =
      "INVALID_ARGUMENT: No expression source provided.";

  EXPECT_FATAL_FAILURE(
      {
        // Create a runtime.
        ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                             CreateTestRuntime());
        TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
          input {
            key: "x"
            value { value { int64_value: 10 } }
          }
          input {
            key: "y"
            value { value { int64_value: 3 } }
          }
          output { result_value { int64_value: 123 } }
        )pb");
        ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                             CreateBasicCompiler());

        // Create a TestRunner but without an expression source.
        std::unique_ptr<CelTestContext> context =
            CelTestContext::CreateFromRuntime(std::move(runtime));
        context->SetCompiler(std::move(compiler));
        TestRunner test_runner(std::move(context));
        test_runner.RunTest(test_case);
      },
      expected_error);
}

TEST(TestRunnerStandaloneTest, BasicTestWithErrorAssertion) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    output {
      eval_error {
        errors { message: "No value with name \"y\" found in Activation" }
      }
    }
  )pb");
  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST(TestRunnerStandaloneTest, BasicTestFailsWhenExpectingErrorButGotValue) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("1 + 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    output {
      eval_error {
        errors { message: "No value with name \"y\" found in Activation" }
      }
    }
  )pb");
  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NONFATAL_FAILURE(test_runner.RunTest(test_case),
                          "Expected error but got value");
}

TEST(TestRunnerStandaloneTest, BasicTestWithActivationFactorySucceeds) {
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("x + y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetActivationFactory(
      [](const TestCase& test_case,
         google::protobuf::Arena* arena) -> absl::StatusOr<cel::Activation> {
        cel::Activation activation;
        activation.InsertOrAssignValue("x", cel::IntValue(10));
        activation.InsertOrAssignValue("y", cel::IntValue(5));
        return activation;
      });
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));

  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    output { result_value { int64_value: 15 } }
  )pb");
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));

  // Input bindings should override values set by the activation factory.
  test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 4 } }
    }
    output { result_value { int64_value: 9 } }
  )pb");
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST(TestRunnerStandaloneTest, CustomAssertFnIsUsed) {
  // Compile the expression.
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       DefaultCompiler().Compile("1 + 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Create a runtime.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  // Set the output to a value that would fail the default assertion.
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    output { result_value { int64_value: 102 } }
  )pb");
  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));

  context->SetAssertFn([&](const cel::Value& computed,
                           const TestCase& test_case, google::protobuf::Arena* arena) {
    ASSERT_TRUE(computed.Is<cel::IntValue>());
    EXPECT_EQ(computed.As<cel::IntValue>().value(), 2);
  });

  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(std::move(checked_expr)));
  TestRunner test_runner(std::move(context));
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));
}

TEST(CoverageTest, RuntimeCoverage) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("x", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("y", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("x > 1 && y > 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 2 } }
    }
    input {
      key: "y"
      value { value { int64_value: 0 } }
    }
    output { result_value { bool_value: false } }
  )pb");

  CoverageIndex coverage_index;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  ASSERT_THAT(EnableCoverageInRuntime(*const_cast<cel::Runtime*>(runtime.get()),
                                      coverage_index),
              absl_testing::IsOk());

  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(checked_expr));
  TestRunner test_runner(std::move(context));
  coverage_index.Init(checked_expr);
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));

  CoverageIndex::CoverageReport report = coverage_index.GetCoverageReport();
  EXPECT_GT(report.nodes, 0);
  EXPECT_GT(report.covered_nodes, 0);
  EXPECT_EQ(report.branches, 6);
  EXPECT_EQ(report.covered_boolean_outcomes, 3);
  EXPECT_THAT(
      report.unencountered_branches,
      ::testing::ElementsAre(
          HasSubstr("\nExpression ID 7 ('x > 1 && y > 1'): Never "
                    "evaluated to 'true'"),
          HasSubstr(
              "\n\t\tExpression ID 2 ('x > 1'): Never evaluated to 'false'"),
          HasSubstr(
              "\n\t\tExpression ID 5 ('y > 1'): Never evaluated to 'true'")));
}

TEST(CoverageTest, BuilderCoverage) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("x", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("y", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("x > 1 && y > 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 0 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output { result_value { bool_value: false } }
  )pb");

  CoverageIndex coverage_index;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpressionBuilder> builder,
                       CreateTestCelExpressionBuilder());
  ASSERT_THAT(EnableCoverageInCelExpressionBuilder(*builder, coverage_index),
              absl_testing::IsOk());

  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromCelExpressionBuilder(std::move(builder));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(checked_expr));
  TestRunner test_runner(std::move(context));
  coverage_index.Init(checked_expr);
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));

  CoverageIndex::CoverageReport report = coverage_index.GetCoverageReport();
  EXPECT_GT(report.nodes, 0);
  EXPECT_GT(report.covered_nodes, 0);
  EXPECT_EQ(report.branches, 6);
  EXPECT_EQ(report.covered_boolean_outcomes, 2);
  EXPECT_THAT(report.unencountered_nodes,
              ::testing::UnorderedElementsAre(HasSubstr("y > 1")));
  EXPECT_THAT(
      report.unencountered_branches,
      ::testing::UnorderedElementsAre(HasSubstr("Never evaluated to 'true'"),
                                      HasSubstr("Never evaluated to 'true'")));
}

TEST(CoverageTest, DotGraphIsGeneratedForRuntime) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("x", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("y", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("x > 1 && y > 1"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 2 } }
    }
    input {
      key: "y"
      value { value { int64_value: 0 } }
    }
    output { result_value { bool_value: false } }
  )pb");

  CoverageIndex coverage_index;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  ASSERT_THAT(EnableCoverageInRuntime(*const_cast<cel::Runtime*>(runtime.get()),
                                      coverage_index),
              absl_testing::IsOk());

  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(checked_expr));
  TestRunner test_runner(std::move(context));
  coverage_index.Init(checked_expr);
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));

  CoverageIndex::CoverageReport report = coverage_index.GetCoverageReport();

  absl::string_view dot_graph = report.dot_graph;

  // Check for graph structure
  EXPECT_THAT(dot_graph, StartsWith(kDigraphHeader));
  EXPECT_THAT(dot_graph, EndsWith("}\n"));
  EXPECT_THAT(dot_graph, HasSubstr("->"));
  EXPECT_THAT(dot_graph, HasSubstr("shape=record"));

  // Check for the existence of complete labels for key nodes, using the actual
  // expression IDs from the build log.
  EXPECT_THAT(dot_graph, HasSubstr("label=\"{<1> exprID: 7 | <2> Call Node} | "
                                   "<3> x \\> 1 && y \\> 1\""));
  EXPECT_THAT(
      dot_graph,
      HasSubstr("label=\"{<1> exprID: 2 | <2> Call Node} | <3> x \\> 1\""));
  EXPECT_THAT(
      dot_graph,
      HasSubstr("label=\"{<1> exprID: 5 | <2> Call Node} | <3> y \\> 1\""));

  // Check for coverage styles
  EXPECT_THAT(dot_graph, HasSubstr(kCompletelyCoveredNodeStyle));
  EXPECT_THAT(dot_graph, HasSubstr(kPartiallyCoveredNodeStyle));
  EXPECT_THAT(dot_graph, Not(HasSubstr(kUncoveredNodeStyle)));
}

TEST(CoverageTest, DotGraphIsGeneratedForComprehension) {
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));

  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());

  ASSERT_OK_AND_ASSIGN(cel::ValidationResult validation_result,
                       compiler->Compile("[1, 2, 3].all(i, i > 0)"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  // Test case expects 'true' since all elements are > 0.
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    output { result_value { bool_value: true } }
  )pb");

  CoverageIndex coverage_index;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  ASSERT_THAT(EnableCoverageInRuntime(*const_cast<cel::Runtime*>(runtime.get()),
                                      coverage_index),
              absl_testing::IsOk());

  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(checked_expr));
  TestRunner test_runner(std::move(context));
  coverage_index.Init(checked_expr);
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));

  CoverageIndex::CoverageReport report = coverage_index.GetCoverageReport();
  absl::string_view dot_graph = report.dot_graph;

  // Assert that the specific kinds for comprehension nodes are present in the
  // generated graph.
  EXPECT_THAT(dot_graph, HasSubstr("IterRange"));
  EXPECT_THAT(dot_graph, HasSubstr("AccuInit"));
  EXPECT_THAT(dot_graph, HasSubstr("LoopCondition"));
  EXPECT_THAT(dot_graph, HasSubstr("LoopStep"));
  EXPECT_THAT(dot_graph, HasSubstr("Result"));

  // The expression is fully evaluated, so no nodes should be uncovered.
  EXPECT_THAT(dot_graph, Not(HasSubstr(kUncoveredNodeStyle)));
}

TEST(CoverageTest, PartiallyCoveredBooleanNodeIsStyledCorrectly) {
  // This test is designed to kill a mutant that incorrectly styles partially
  // covered boolean nodes as completely covered. It uses a short-circuiting
  // expression to ensure that some boolean nodes are only evaluated one way
  // (e.g., only to 'true'), making them partially covered.
  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<cel::CompilerBuilder> compiler_builder,
      cel::NewCompilerBuilder(cel::internal::GetTestingDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("x", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_THAT(compiler_builder->GetCheckerBuilder().AddVariable(
                  cel::MakeVariableDecl("y", cel::IntType())),
              absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Compiler> compiler,
                       std::move(compiler_builder)->Build());
  ASSERT_OK_AND_ASSIGN(
      cel::ValidationResult validation_result,
      compiler->Compile("{'sum': x + y, 'literal': 3}.sum == 3 || x == y"));
  CheckedExpr checked_expr;
  ASSERT_THAT(cel::AstToCheckedExpr(*validation_result.GetAst(), &checked_expr),
              absl_testing::IsOk());
  TestCase test_case = ParseTextProtoOrDie<TestCase>(R"pb(
    input {
      key: "x"
      value { value { int64_value: 1 } }
    }
    input {
      key: "y"
      value { value { int64_value: 2 } }
    }
    output { result_value { bool_value: true } }
  )pb");

  CoverageIndex coverage_index;
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<const cel::Runtime> runtime,
                       CreateTestRuntime());
  ASSERT_THAT(EnableCoverageInRuntime(*const_cast<cel::Runtime*>(runtime.get()),
                                      coverage_index),
              absl_testing::IsOk());
  std::unique_ptr<CelTestContext> context =
      CelTestContext::CreateFromRuntime(std::move(runtime));
  context->SetExpressionSource(
      CelExpressionSource::FromCheckedExpr(checked_expr));
  TestRunner test_runner(std::move(context));
  coverage_index.Init(checked_expr);
  EXPECT_NO_FATAL_FAILURE(test_runner.RunTest(test_case));

  CoverageIndex::CoverageReport report = coverage_index.GetCoverageReport();

  // With x=1, y=2, the left side of '||' is true, so the right side ('x == y')
  // is short-circuited and never evaluated.
  // - The '||' node and the '==' node are partially covered (only 'true').
  // - The 'x == y' branch (and its children) are uncovered.
  // - All other evaluated nodes are fully covered.
  EXPECT_EQ(CountSubstrings(report.dot_graph, kPartiallyCoveredNodeStyle), 2);
  EXPECT_EQ(CountSubstrings(report.dot_graph, kUncoveredNodeStyle), 3);
  EXPECT_EQ(CountSubstrings(report.dot_graph, kCompletelyCoveredNodeStyle), 9);
}
}  // namespace
}  // namespace cel::test
