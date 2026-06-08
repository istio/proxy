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

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_RUNNER_LIBRARY_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_RUNNER_LIBRARY_H_

#include <memory>
#include <optional>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "testing/testrunner/cel_test_context.h"
#include "testing/testrunner/coverage_index.h"
#include "testing/testrunner/coverage_reporting.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"

namespace cel::test {

// The test runner class for running CEL tests.
class TestRunner {
 public:
  explicit TestRunner(std::unique_ptr<CelTestContext> test_context)
      : test_context_(std::move(test_context)) {}

  // Automatically reports coverage results.
  ~TestRunner() {
    if (coverage_index_) {
      CoverageReportingEnvironment reporter(*coverage_index_);
      reporter.TearDown();
    }
  }

  // Evaluates the checked expression in the test case, performs the
  // assertions against the expected result.
  void RunTest(const cel::expr::conformance::test::TestCase& test_case);

  // Returns the checked expression for the test case.
  absl::StatusOr<cel::expr::CheckedExpr> GetCheckedExpr() const;

  // Returns the coverage report for the test case.
  std::optional<CoverageIndex::CoverageReport> GetCoverageReport() const;

 private:
  absl::StatusOr<cel::Value> EvalWithRuntime(
      const cel::expr::CheckedExpr& checked_expr,
      const cel::expr::conformance::test::TestCase& test_case,
      google::protobuf::Arena* arena);

  absl::StatusOr<cel::Value> EvalWithCelExpressionBuilder(
      const cel::expr::CheckedExpr& checked_expr,
      const cel::expr::conformance::test::TestCase& test_case,
      google::protobuf::Arena* arena);

  void Assert(const cel::Value& computed,
              const cel::expr::conformance::test::TestCase& test_case,
              google::protobuf::Arena* arena);

  void AssertValue(const cel::Value& computed,
                   const cel::expr::conformance::test::TestOutput& output,
                   google::protobuf::Arena* arena);

  void AssertError(const cel::Value& computed,
                   const cel::expr::conformance::test::TestOutput& output);

  absl::Status EnableCoverage();

  std::unique_ptr<cel::test::CelTestContext> test_context_;

  std::unique_ptr<CoverageIndex> coverage_index_;
};

}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_RUNNER_LIBRARY_H_
