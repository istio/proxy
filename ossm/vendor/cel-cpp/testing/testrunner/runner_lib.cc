// Copyright 2025 Google LLC.
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
#include "testing/testrunner/runner_lib.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

#include "cel/expr/eval.pb.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/ast_proto.h"
#include "common/internal/value_conversion.h"
#include "common/value.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/transform_utility.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "testing/testrunner/cel_expression_source.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/coverage_index.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/field_comparator.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel::test {
namespace {

using ::cel::expr::conformance::test::InputValue;
using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::conformance::test::TestOutput;
using ::cel::expr::CheckedExpr;
using ::google::api::expr::runtime::CelExpression;
using ::google::api::expr::runtime::ValueToCelValue;
using ::google::api::expr::runtime::Activation;

using LegacyCelValue = ::google::api::expr::runtime::CelValue;
using ValueProto = ::cel::expr::Value;

absl::StatusOr<std::string> ReadFileToString(absl::string_view file_path) {
  std::ifstream file_stream{std::string(file_path)};
  if (!file_stream.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Unable to open file: ", file_path));
  }
  std::stringstream buffer;
  buffer << file_stream.rdbuf();
  return buffer.str();
}

absl::StatusOr<CheckedExpr> Compile(absl::string_view expression,
                                    const CelTestContext& context) {
  const auto* compiler = context.compiler();
  if (compiler == nullptr) {
    return absl::InvalidArgumentError(
        "A compiler must be provided to compile a raw expression or .cel "
        "file.");
  }

  CEL_ASSIGN_OR_RETURN(ValidationResult validation_result,
                       compiler->Compile(expression));
  if (!validation_result.IsValid()) {
    return absl::InternalError(validation_result.FormatError());
  }

  CheckedExpr checked_expr;
  CEL_RETURN_IF_ERROR(
      AstToCheckedExpr(*validation_result.GetAst(), &checked_expr));
  return checked_expr;
}

absl::StatusOr<std::unique_ptr<cel::Program>> Plan(
    const CheckedExpr& checked_expr, const cel::Runtime* runtime) {
  std::unique_ptr<cel::Ast> ast;
  CEL_ASSIGN_OR_RETURN(ast, cel::CreateAstFromCheckedExpr(checked_expr));
  if (ast == nullptr) {
    return absl::InternalError("No expression provided for testing.");
  }
  return runtime->CreateProgram(std::move(ast));
}

const google::protobuf::DescriptorPool* GetDescriptorPool(const CelTestContext& context) {
  return context.cel_expression_builder() != nullptr
             ? google::protobuf::DescriptorPool::generated_pool()
             : context.runtime()->GetDescriptorPool();
}

google::protobuf::MessageFactory* GetMessageFactory(const CelTestContext& context) {
  return context.cel_expression_builder() != nullptr
             ? google::protobuf::MessageFactory::generated_factory()
             : context.runtime()->GetMessageFactory();
}

absl::StatusOr<cel::Value> EvalWithModernBindings(
    const CheckedExpr& checked_expr, const CelTestContext& context,
    const cel::Activation& activation, google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Program> program,
                       Plan(checked_expr, context.runtime()));
  return program->Evaluate(arena, activation);
}

absl::StatusOr<cel::Value> EvalWithLegacyBindings(
    const CheckedExpr& checked_expr, const CelTestContext& context,
    const Activation& activation, google::protobuf::Arena* arena) {
  const auto* builder = context.cel_expression_builder();

  CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpression> sub_expression,
                       builder->CreateExpression(&checked_expr));

  CEL_ASSIGN_OR_RETURN(LegacyCelValue legacy_result,
                       sub_expression->Evaluate(activation, arena));

  ValueProto result_proto;
  CEL_RETURN_IF_ERROR(CelValueToValue(legacy_result, &result_proto));
  return FromExprValue(result_proto, GetDescriptorPool(context),
                       GetMessageFactory(context), arena);
}

absl::StatusOr<cel::Value> ResolveValue(const InputValue& input_value,
                                        const CelTestContext& context,
                                        google::protobuf::Arena* arena) {
  return FromExprValue(input_value.value(), GetDescriptorPool(context),
                       GetMessageFactory(context), arena);
}

absl::StatusOr<cel::Value> ResolveExpr(absl::string_view expr,
                                       const CelTestContext& context,
                                       google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(CheckedExpr checked_expr, Compile(expr, context));
  if (context.runtime() != nullptr) {
    cel::Activation empty_activation;
    return EvalWithModernBindings(checked_expr, context, empty_activation,
                                  arena);
  } else {
    Activation empty_activation;
    return EvalWithLegacyBindings(checked_expr, context, empty_activation,
                                  arena);
  }
}

absl::StatusOr<cel::Value> ResolveInputValue(const InputValue& input_value,
                                             const CelTestContext& context,
                                             google::protobuf::Arena* arena) {
  switch (input_value.kind_case()) {
    case InputValue::kValue: {
      return ResolveValue(input_value, context, arena);
    }
    case InputValue::kExpr: {
      return ResolveExpr(input_value.expr(), context, arena);
    }
    default:
      return absl::InvalidArgumentError("Unknown InputValue kind.");
  }
}

absl::Status AddCustomBindingsToModernActivation(const CelTestContext& context,
                                                 cel::Activation& activation,
                                                 google::protobuf::Arena* arena) {
  for (const auto& binding : context.custom_bindings()) {
    CEL_ASSIGN_OR_RETURN(cel::Value value,
                         FromExprValue(/*value_proto=*/binding.second,
                                       GetDescriptorPool(context),
                                       GetMessageFactory(context), arena));
    activation.InsertOrAssignValue(/*name=*/binding.first, value);
  }
  return absl::OkStatus();
}

absl::Status AddTestCaseBindingsToModernActivation(
    const TestCase& test_case, const CelTestContext& context,
    cel::Activation& activation, google::protobuf::Arena* arena) {
  for (const auto& binding : test_case.input()) {
    CEL_ASSIGN_OR_RETURN(
        cel::Value value,
        ResolveInputValue(/*input_value=*/binding.second, context, arena));
    activation.InsertOrAssignValue(/*name=*/binding.first, std::move(value));
  }
  return absl::OkStatus();
}

absl::StatusOr<cel::Activation> GetActivation(const CelTestContext& context,
                                              const TestCase& test_case,
                                              google::protobuf::Arena* arena) {
  if (context.activation_factory() != nullptr) {
    return context.activation_factory()(test_case, arena);
  }
  return cel::Activation();
}

absl::StatusOr<cel::Activation> CreateModernActivationFromBindings(
    const TestCase& test_case, const CelTestContext& context,
    google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(cel::Activation activation,
                       GetActivation(context, test_case, arena));
  CEL_RETURN_IF_ERROR(
      AddCustomBindingsToModernActivation(context, activation, arena));

  CEL_RETURN_IF_ERROR(AddTestCaseBindingsToModernActivation(test_case, context,
                                                            activation, arena));

  return activation;
}

absl::Status AddCustomBindingsToLegacyActivation(const CelTestContext& context,
                                                 Activation& activation,
                                                 google::protobuf::Arena* arena) {
  for (const auto& binding : context.custom_bindings()) {
    CEL_ASSIGN_OR_RETURN(
        LegacyCelValue value,
        ValueToCelValue(/*value_proto=*/binding.second, arena));
    activation.InsertValue(/*name=*/binding.first, value);
  }
  return absl::OkStatus();
}

absl::Status AddTestCaseBindingsToLegacyActivation(
    const TestCase& test_case, const CelTestContext& context,
    Activation& activation, google::protobuf::Arena* arena) {
  auto* message_factory = GetMessageFactory(context);
  auto* descriptor_pool = GetDescriptorPool(context);
  for (const auto& binding : test_case.input()) {
    CEL_ASSIGN_OR_RETURN(
        cel::Value resolved_cel_value,
        ResolveInputValue(/*input_value=*/binding.second, context, arena));
    CEL_ASSIGN_OR_RETURN(ValueProto value_proto,
                         ToExprValue(resolved_cel_value, descriptor_pool,
                                     message_factory, arena));
    CEL_ASSIGN_OR_RETURN(LegacyCelValue value,
                         ValueToCelValue(value_proto, arena));
    activation.InsertValue(/*name=*/binding.first, value);
  }
  return absl::OkStatus();
}

absl::StatusOr<Activation> CreateLegacyActivationFromBindings(
    const TestCase& test_case, const CelTestContext& context,
    google::protobuf::Arena* arena) {
  Activation activation;

  CEL_RETURN_IF_ERROR(
      AddCustomBindingsToLegacyActivation(context, activation, arena));

  CEL_RETURN_IF_ERROR(AddTestCaseBindingsToLegacyActivation(test_case, context,
                                                            activation, arena));

  return activation;
}

bool IsEqual(const ValueProto& expected, const ValueProto& actual) {
  static auto* kFieldComparator = []() {
    auto* field_comparator = new google::protobuf::util::DefaultFieldComparator();
    field_comparator->set_treat_nan_as_equal(true);
    return field_comparator;
  }();
  static auto* kDifferencer = []() {
    auto* differencer = new google::protobuf::util::MessageDifferencer();
    differencer->set_message_field_comparison(
        google::protobuf::util::MessageDifferencer::EQUIVALENT);
    differencer->set_field_comparator(kFieldComparator);
    const auto* descriptor = cel::expr::MapValue::descriptor();
    const auto* entries_field = descriptor->FindFieldByName("entries");
    const auto* key_field =
        entries_field->message_type()->FindFieldByName("key");
    differencer->TreatAsMap(entries_field, key_field);
    return differencer;
  }();
  return kDifferencer->Compare(expected, actual);
}

MATCHER_P(MatchesValue, expected, "") { return IsEqual(arg, expected); }
}  // namespace

void TestRunner::AssertValue(const cel::Value& computed,
                             const TestOutput& output, google::protobuf::Arena* arena) {
  if (computed.IsError()) {
    ADD_FAILURE() << "Expected value but got error: " << computed.DebugString();
    return;
  }
  ValueProto expected_value_proto;
  const auto* descriptor_pool = GetDescriptorPool(*test_context_);
  auto* message_factory = GetMessageFactory(*test_context_);
  if (output.has_result_value()) {
    expected_value_proto = output.result_value();
  } else if (output.has_result_expr()) {
    InputValue input_value;
    input_value.set_expr(output.result_expr());
    ASSERT_OK_AND_ASSIGN(cel::Value resolved_cel_value,
                         ResolveInputValue(input_value, *test_context_, arena));
    ASSERT_OK_AND_ASSIGN(expected_value_proto,
                         ToExprValue(resolved_cel_value, descriptor_pool,
                                     message_factory, arena));
  }
  ValueProto computed_expr_value;
  ASSERT_OK_AND_ASSIGN(
      computed_expr_value,
      ToExprValue(computed, descriptor_pool, message_factory, arena));
  EXPECT_THAT(computed_expr_value, MatchesValue(expected_value_proto));
}

void TestRunner::AssertError(const cel::Value& computed,
                             const TestOutput& output) {
  if (!computed.IsError()) {
    ADD_FAILURE() << "Expected error but got value: " << computed.DebugString();
    return;
  }
  absl::Status computed_status = computed.AsError()->ToStatus();
  // We selected the first error in the set for comparison because there is only
  // one runtime error that is reported even if there are multiple errors in the
  // critical path.
  ASSERT_TRUE(output.eval_error().errors_size() == 1)
      << "Expected exactly one error but got: "
      << output.eval_error().errors_size();
  ASSERT_EQ(computed_status.message(), output.eval_error().errors(0).message());
}

void TestRunner::Assert(const cel::Value& computed, const TestCase& test_case,
                        google::protobuf::Arena* arena) {
  if (test_context_->assert_fn()) {
    test_context_->assert_fn()(computed, test_case, arena);
    return;
  }
  TestOutput output = test_case.output();
  if (output.has_result_value() || output.has_result_expr()) {
    AssertValue(computed, output, arena);
  } else if (output.has_eval_error()) {
    AssertError(computed, output);
  } else if (output.has_unknown()) {
    ADD_FAILURE() << "Unknown assertions not implemented yet.";
  } else {
    ADD_FAILURE() << "Unexpected  output kind.";
  }
}

absl::StatusOr<cel::Value> TestRunner::EvalWithRuntime(
    const CheckedExpr& checked_expr, const TestCase& test_case,
    google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(
      cel::Activation activation,
      CreateModernActivationFromBindings(test_case, *test_context_, arena));
  return EvalWithModernBindings(checked_expr, *test_context_, activation,
                                arena);
}

absl::StatusOr<cel::Value> TestRunner::EvalWithCelExpressionBuilder(
    const CheckedExpr& checked_expr, const TestCase& test_case,
    google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(
      Activation activation,
      CreateLegacyActivationFromBindings(test_case, *test_context_, arena));
  return EvalWithLegacyBindings(checked_expr, *test_context_, activation,
                                arena);
}

absl::StatusOr<CheckedExpr> TestRunner::GetCheckedExpr() const {
  const CelExpressionSource* source_ptr = test_context_->expression_source();
  if (source_ptr == nullptr) {
    return absl::InvalidArgumentError("No expression source provided.");
  }
  return std::visit(
      absl::Overload([](const cel::expr::CheckedExpr& v)
                         -> absl::StatusOr<CheckedExpr> { return v; },
                     [this](const CelExpressionSource::RawExpression& v)
                         -> absl::StatusOr<CheckedExpr> {
                       return Compile(v.value, *test_context_);
                     },
                     [this](const CelExpressionSource::CelFile& v)
                         -> absl::StatusOr<CheckedExpr> {
                       CEL_ASSIGN_OR_RETURN(std::string contents,
                                            ReadFileToString(v.path));
                       return Compile(contents, *test_context_);
                     }),
      source_ptr->source());
}

absl::Status TestRunner::EnableCoverage() {
  if (test_context_ != nullptr && test_context_->enable_coverage()) {
    coverage_index_ = std::make_unique<CoverageIndex>();

    if (test_context_->runtime() != nullptr) {
      auto* runtime = const_cast<cel::Runtime*>(test_context_->runtime());
      CEL_RETURN_IF_ERROR(EnableCoverageInRuntime(*runtime, *coverage_index_));
    } else if (test_context_->cel_expression_builder() != nullptr) {
      auto* builder =
          const_cast<google::api::expr::runtime::CelExpressionBuilder*>(
              test_context_->cel_expression_builder());
      CEL_RETURN_IF_ERROR(
          EnableCoverageInCelExpressionBuilder(*builder, *coverage_index_));
    }
  }
  return absl::OkStatus();
}

void TestRunner::RunTest(const TestCase& test_case) {
  // The arena has to be declared in RunTest because cel::Value returned by
  // EvalWithRuntime or EvalWithCelExpressionBuilder might contain pointers to
  // the arena. The arena has to be alive during the assertion.
  google::protobuf::Arena arena;
  ASSERT_THAT(EnableCoverage(), absl_testing::IsOk());
  ASSERT_OK_AND_ASSIGN(CheckedExpr checked_expr, GetCheckedExpr());

  if (coverage_index_) {
    coverage_index_->Init(checked_expr);
  }

  if (test_context_->runtime() != nullptr) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         EvalWithRuntime(checked_expr, test_case, &arena));
    ASSERT_NO_FATAL_FAILURE(Assert(result, test_case, &arena));
  } else if (test_context_->cel_expression_builder() != nullptr) {
    ASSERT_OK_AND_ASSIGN(
        cel::Value result,
        EvalWithCelExpressionBuilder(checked_expr, test_case, &arena));
    ASSERT_NO_FATAL_FAILURE(Assert(result, test_case, &arena));
  }
}
}  // namespace cel::test
