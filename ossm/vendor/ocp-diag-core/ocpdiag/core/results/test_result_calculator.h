// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_CALCULATOR_H_
#define OCPDIAG_CORE_RESULTS_OCP_CALCULATOR_H_

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/data_model/output_model.h"

namespace ocpdiag::results {

// This class encapsulates the test result calculation from a OCPDiag test run.
// Call the Notify*() methods to update the result calculation of various events
// during the test run, and then call Finalize() when the test is done to get
// the final result.
class TestResultCalculator {
 public:
  TestResultCalculator() = default;

  // Returns the test result.
  TestResult result() const;

  // Returns the test status.
  TestStatus status() const;

  // Tells the result calculation that the run has started.
  void NotifyStartRun();

  // Tells the result calculation that the run was skipped.
  void NotifySkip();

  // Tells the result calculation that there was an error.
  void NotifyError();

  // Tells the result calculation that there was a failure diagnosis.
  void NotifyFailureDiagnosis();

  // Finalizes the test result. The result cannot be changed after this.
  void Finalize();

 private:
  void CheckFinalized() const;

  mutable absl::Mutex mutex_;
  bool finalized_ ABSL_GUARDED_BY(mutex_) = false;
  bool run_started_ ABSL_GUARDED_BY(mutex_) = false;
  TestResult result_ ABSL_GUARDED_BY(mutex_) = TestResult::kNotApplicable;
  TestStatus status_ ABSL_GUARDED_BY(mutex_) = TestStatus::kUnknown;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_CALCULATOR_H_
