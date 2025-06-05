// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/test_result_calculator.h"

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/data_model/output_model.h"

namespace ocpdiag::results {

TestResult TestResultCalculator::result() const {
  absl::MutexLock lock(&mutex_);
  return result_;
}

TestStatus TestResultCalculator::status() const {
  absl::MutexLock lock(&mutex_);
  return status_;
}

void TestResultCalculator::NotifyStartRun() {
  absl::MutexLock lock(&mutex_);
  CheckFinalized();
  run_started_ = true;
}

void TestResultCalculator::NotifySkip() {
  absl::MutexLock lock(&mutex_);
  CheckFinalized();
  if (status_ == TestStatus::kUnknown) {
    result_ = TestResult::kNotApplicable;
    status_ = TestStatus::kSkip;
  }
}

void TestResultCalculator::NotifyError() {
  absl::MutexLock lock(&mutex_);
  CheckFinalized();
  if (status_ == TestStatus::kUnknown) {
    result_ = TestResult::kNotApplicable;
    status_ = TestStatus::kError;
  }
}

void TestResultCalculator::NotifyFailureDiagnosis() {
  absl::MutexLock lock(&mutex_);
  CheckFinalized();
  if (result_ == TestResult::kNotApplicable && status_ == TestStatus::kUnknown)
    result_ = TestResult::kFail;
}

void TestResultCalculator::Finalize() {
  absl::MutexLock lock(&mutex_);
  CheckFinalized();
  finalized_ = true;

  if (run_started_) {
    if (status_ == TestStatus::kUnknown) {
      status_ = TestStatus::kComplete;
      if (result_ == TestResult::kNotApplicable) result_ = TestResult::kPass;
    }
  } else if (status_ != TestStatus::kError) {
    // Error status takes highest priority and so should not be overridden
    status_ = TestStatus::kSkip;
    result_ = TestResult::kNotApplicable;
  }
}

void TestResultCalculator::CheckFinalized() const
    ABSL_EXCLUSIVE_LOCKS_REQUIRED(&mutex_) {
  CHECK(!finalized_) << "Test run already finalized";
}

}  // namespace ocpdiag::results
