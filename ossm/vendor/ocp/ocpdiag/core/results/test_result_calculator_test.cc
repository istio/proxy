// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/test_result_calculator.h"

#include "gtest/gtest.h"
#include "ocpdiag/core/results/data_model/results.pb.h"

namespace ocpdiag::results {
namespace {

TEST(TestResultCalculatorTest, Passing) {
  TestResultCalculator calculator;
  calculator.NotifyStartRun();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kPass);
  EXPECT_EQ(calculator.status(), TestStatus::kComplete);
}

TEST(TestResultCalculatorTest, SkippedNotStarted) {
  TestResultCalculator calculator;
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kNotApplicable);
  EXPECT_EQ(calculator.status(), TestStatus::kSkip);
}

TEST(TestResultCalculatorTest, SkippedIntentionally) {
  TestResultCalculator calculator;
  calculator.NotifyStartRun();
  calculator.NotifySkip();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kNotApplicable);
  EXPECT_EQ(calculator.status(), TestStatus::kSkip);
}

TEST(TestResultCalculatorTest, Error) {
  TestResultCalculator calculator;
  calculator.NotifyStartRun();
  calculator.NotifyError();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kNotApplicable);
  EXPECT_EQ(calculator.status(), TestStatus::kError);
}

TEST(TestResultCalculatorTest, ErrorBeforeStart) {
  TestResultCalculator calculator;
  calculator.NotifyError();
  calculator.NotifyStartRun();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kNotApplicable);
  EXPECT_EQ(calculator.status(), TestStatus::kError);
}

TEST(TestResultCalculatorTest, SkipDoesNotOverrideError) {
  TestResultCalculator calculator;
  calculator.NotifyStartRun();
  calculator.NotifyError();
  calculator.NotifySkip();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kNotApplicable);
  EXPECT_EQ(calculator.status(), TestStatus::kError);
}

TEST(TestResultCalculatorTest, Failing) {
  TestResultCalculator calculator;
  calculator.NotifyStartRun();
  calculator.NotifyFailureDiagnosis();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kFail);
  EXPECT_EQ(calculator.status(), TestStatus::kComplete);
}

TEST(TestResultCalculatorTest, ErrorOverridesFail) {
  TestResultCalculator calculator;
  calculator.NotifyStartRun();
  calculator.NotifyFailureDiagnosis();
  calculator.NotifyError();
  calculator.Finalize();
  EXPECT_EQ(calculator.result(), TestResult::kNotApplicable);
  EXPECT_EQ(calculator.status(), TestStatus::kError);
}

}  // namespace
}  // namespace ocpdiag::results
