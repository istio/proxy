// Copyright 2025 Google LLC
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

// This binary is a test runner for CEL tests. It is used to run CEL tests
// written in the CEL test suite format.
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_expression.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime.h"
#include "testing/testrunner/cel_expression_source.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/cel_test_factories.h"
#include "testing/testrunner/coverage_index.h"
#include "testing/testrunner/coverage_reporting.h"
#include "testing/testrunner/runner_lib.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(std::string, test_suite_path, "",
          "The path to the file containing the test suite to run.");
ABSL_FLAG(std::string, expr_source_type, "",
          "The kind of expression source: 'raw', 'file', or 'checked'.");
ABSL_FLAG(std::string, expr_source, "",
          "The value of the CEL expression source. For 'raw', it's the "
          "expression string. For 'file' and 'checked', it's the file path.");

ABSL_FLAG(bool, collect_coverage, false, "Whether to collect code coverage.");

namespace {

using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::conformance::test::TestSuite;
using ::cel::test::CelExpressionSource;
using ::cel::test::CelTestContext;
using ::cel::test::CoverageIndex;
using ::cel::test::TestRunner;
using ::cel::expr::CheckedExpr;
using ::google::api::expr::runtime::CelExpressionBuilder;

class CelTest : public testing::Test {
 public:
  explicit CelTest(std::shared_ptr<TestRunner> test_runner,
                   const TestCase& test_case)
      : test_runner_(std::move(test_runner)), test_case_(test_case) {}

  void TestBody() override { test_runner_->RunTest(test_case_); }

 private:
  std::shared_ptr<TestRunner> test_runner_;
  TestCase test_case_;
};

absl::Status RegisterTests(const TestSuite& test_suite,
                           const std::shared_ptr<TestRunner>& test_runner) {
  for (const auto& section : test_suite.sections()) {
    for (const TestCase& test_case : section.tests()) {
      testing::RegisterTest(
          test_suite.name().c_str(),
          absl::StrCat(section.name(), "/", test_case.name()).c_str(), nullptr,
          nullptr, __FILE__, __LINE__, [&test_runner, test_case]() -> CelTest* {
            return new CelTest(test_runner, test_case);
          });
    }
  }
  return absl::OkStatus();
}

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

template <typename T>
absl::StatusOr<T> ReadTextProtoFromFile(absl::string_view file_path) {
  CEL_ASSIGN_OR_RETURN(std::string contents, ReadFileToString(file_path));
  T message;
  if (!google::protobuf::TextFormat::ParseFromString(contents, &message)) {
    return absl::InternalError(absl::StrCat(
        "Failed to parse text-format proto from file: ", file_path));
  }
  return message;
}

absl::StatusOr<CheckedExpr> ReadBinaryProtoFromFile(
    absl::string_view file_path) {
  CheckedExpr message;
  std::ifstream file_stream{std::string(file_path), std::ios::binary};
  if (!file_stream.is_open()) {
    return absl::NotFoundError(
        absl::StrCat("Unable to open file: ", file_path));
  }
  if (!message.ParseFromIstream(&file_stream)) {
    return absl::InternalError(
        absl::StrCat("Failed to parse binary proto from file: ", file_path));
  }
  return message;
}

TestSuite ReadTestSuiteFromPath(absl::string_view test_suite_path) {
  absl::StatusOr<TestSuite> test_suite_or =
      ReadTextProtoFromFile<TestSuite>(test_suite_path);

  if (!test_suite_or.ok()) {
    ABSL_LOG(FATAL) << "Failed to load test suite from " << test_suite_path
                    << ": " << test_suite_or.status();
  }
  return *std::move(test_suite_or);
}

absl::StatusOr<CheckedExpr> ReadCheckedExprFromFile(
    absl::string_view file_path) {
  if (absl::EndsWith(file_path, ".textproto")) {
    return ReadTextProtoFromFile<CheckedExpr>(file_path);
  }
  if (absl::EndsWith(file_path, ".binarypb")) {
    return ReadBinaryProtoFromFile(file_path);
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Unknown file extension for checked expression. ",
      "Please use .textproto, .textpb, .pb, or .binarypb: ", file_path));
}

TestSuite GetTestSuite() {
  std::string test_suite_path = absl::GetFlag(FLAGS_test_suite_path);
  if (!test_suite_path.empty()) {
    return ReadTestSuiteFromPath(test_suite_path);
  }

  // If no test suite path is provided, use the factory function to get the
  // test suite after checking if the factory function is empty or not.
  std::function<TestSuite()> test_suite_factory =
      cel::test::internal::GetCelTestSuiteFactory();
  if (test_suite_factory == nullptr) {
    ABSL_LOG(FATAL)
        << "No CEL test suite provided. Please provide a test suite using "
           "either the bzl macro or the CEL_REGISTER_TEST_SUITE_FACTORY "
           "preprocessor macro.";
  }
  return test_suite_factory();
}

void UpdateWithExpressionFromCommandLineFlags(
    CelTestContext& cel_test_context) {
  if (absl::GetFlag(FLAGS_expr_source).empty()) {
    return;
  }

  constexpr absl::string_view kRawExpressionKind = "raw";
  constexpr absl::string_view kFileExpressionKind = "file";
  constexpr absl::string_view kCheckedExpressionKind = "checked";

  std::string kind = absl::GetFlag(FLAGS_expr_source_type);
  std::string value = absl::GetFlag(FLAGS_expr_source);

  std::optional<CelExpressionSource> expression_source_from_flags;
  if (kind == kRawExpressionKind) {
    expression_source_from_flags =
        CelExpressionSource::FromRawExpression(value);
  } else if (kind == kFileExpressionKind) {
    expression_source_from_flags = CelExpressionSource::FromCelFile(value);
  } else if (kind == kCheckedExpressionKind) {
    absl::StatusOr<CheckedExpr> checked_expr = ReadCheckedExprFromFile(value);
    if (!checked_expr.ok()) {
      ABSL_LOG(FATAL) << "Failed to read checked expression from file: "
                      << checked_expr.status();
    }
    expression_source_from_flags =
        CelExpressionSource::FromCheckedExpr(std::move(*checked_expr));
  } else {
    ABSL_LOG(FATAL) << "Unknown expression kind: " << kind;
  }

  // Check for conflicting expression sources.
  if (cel_test_context.expression_source() != nullptr) {
    ABSL_LOG(FATAL)
        << "Expression source can only be set once and is currently set via "
           "the factory.";
  }

  if (expression_source_from_flags.has_value()) {
    cel_test_context.SetExpressionSource(
        std::move(*expression_source_from_flags));
  }
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // Create a test context using the factory function returned by the global
  // factory function provider which was initialized by the user.
  absl::StatusOr<std::unique_ptr<cel::test::CelTestContext>>
      cel_test_context_or = cel::test::internal::GetCelTestContextFactory()();
  if (!cel_test_context_or.ok()) {
    ABSL_LOG(FATAL) << "Failed to create CEL test context from factory: "
                    << cel_test_context_or.status();
  }
  std::unique_ptr<cel::test::CelTestContext> cel_test_context =
      std::move(cel_test_context_or.value());

  // We manually enable coverage here instead of just setting the
  // `enable_coverage` flag on the context. This is intentional and necessary
  // for this binary's reporting model.
  //
  // This binary needs a single coverage report for all tests run.
  // We create `coverage_index` here, local to the `main` function, so its
  // lifetime spans the entire test run.
  //
  // We must pass this specific instance to the
  // `CoverageReportingEnvironment`, which Google Test calls after all
  // dynamically registered tests are finished.
  //
  // If we just set the `enable_coverage` flag, the `TestRunner`'s
  // constructor (as used in our `cc_test` files) would create its own
  // internal `CoverageIndex`. That internal index would be destroyed
  // with the `TestRunner` and would not populate the `coverage_index`
  // instance needed by our global reporter.
  //
  // This manual approach ensures all tests populate the same `coverage_index`
  // (the one local to `main`), which is then ready for the final report.
  cel::test::CoverageIndex coverage_index;

  if (absl::GetFlag(FLAGS_collect_coverage)) {
    if (cel_test_context->runtime() != nullptr) {
      ABSL_CHECK_OK(cel::test::EnableCoverageInRuntime(
          const_cast<cel::Runtime&>(*cel_test_context->runtime()),
          coverage_index));
    } else if (cel_test_context->cel_expression_builder() != nullptr) {
      ABSL_CHECK_OK(cel::test::EnableCoverageInCelExpressionBuilder(
          const_cast<CelExpressionBuilder&>(
              *cel_test_context->cel_expression_builder()),
          coverage_index));
    }
  }

  // Update the context with an expression from flags, if provided.
  // This will FATAL if an expression is set by both the factory and flags.
  UpdateWithExpressionFromCommandLineFlags(*cel_test_context);

  auto test_runner = std::make_shared<TestRunner>(std::move(cel_test_context));
  ABSL_CHECK_OK(RegisterTests(GetTestSuite(), test_runner));

  // Make sure the checked expression exists during the entire test run since
  // the ast references it during coverage collection at teardown.
  absl::StatusOr<cel::expr::CheckedExpr> checked_expr =
      test_runner->GetCheckedExpr();
  if (!checked_expr.ok()) {
    ABSL_LOG(FATAL) << "Failed to get checked expression: "
                    << checked_expr.status();
  }

  if (absl::GetFlag(FLAGS_collect_coverage)) {
    coverage_index.Init(*checked_expr);
    testing::AddGlobalTestEnvironment(
        new cel::test::CoverageReportingEnvironment(coverage_index));
  }

  return RUN_ALL_TESTS();
}
