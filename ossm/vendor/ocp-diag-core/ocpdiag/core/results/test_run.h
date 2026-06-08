// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_TEST_RUN_H_
#define OCPDIAG_CORE_RESULTS_OCP_TEST_RUN_H_
#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/flags/declare.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/int_incrementer.h"
#include "ocpdiag/core/results/log_sink.h"
#include "ocpdiag/core/results/test_result_calculator.h"

ABSL_DECLARE_FLAG(bool, ocpdiag_copy_results_to_stdout);
ABSL_DECLARE_FLAG(std::string, ocpdiag_binary_results_filepath);
ABSL_DECLARE_FLAG(bool, ocpdiag_log_to_results);

namespace ocpdiag::results {

// Class that keeps track of the start, end, and status of the test.
// This class handles emitting test run artifacts. There should only be one
// TestRun object per test and only one instance of this class can exist at a
// time.
class TestRun {
 public:
  // Initializes the TestRun with all required TestRunStart information so that
  // this artifact will always be emitted.
  //
  TestRun(const TestRunStart& test_run_start,
          std::unique_ptr<internal::ArtifactWriter> writer = nullptr);
  TestRun(const TestRun&) = delete;
  TestRun& operator=(const TestRun&) = delete;

  // Emits the TestRunEnd artifact and the TestRunStart artifact if the test
  // hasn't already been started.
  ~TestRun();

  // Emits an error artifact for an error that occurred before the TestRun
  // started, usually when gathering info from the DUT. This function will end
  // the program if called after the TestRun has started - add errors that
  // happen during the test to TestStep objects.
  //
  // added after test start
  void AddPreStartError(const Error& error);

  // Emits a log artifact to record information before the TestRun has been
  // started. This function will end the program if called after the TestRun has
  // started - add logs that are relevant during the test to TestStep objects.
  void AddPreStartLog(const Log& log);

  // Emits the TestRunStart artifact and begins the run, allowing TestSteps
  // to be created.
  void StartAndRegisterDutInfo(std::unique_ptr<DutInfo> dut_info);

  // Marks the TestRun as skipped.
  void Skip() { result_calculator_->NotifySkip(); }

  // Returns the current status of the TestRun.
  TestStatus Status() const { return result_calculator_->status(); }

  // Returns the current result of the TestRun.
  TestResult Result() const { return result_calculator_->result(); }

  // Indicates whether the TestRun has been started.
  bool Started() {
    absl::MutexLock lock(&mutex_);
    return started_;
  }

  // Returns the unique ID for the next test step. This is intended for internal
  // use.
  std::string GetNextStepId() { return absl::StrCat(step_id_.Next()); }

  // Returns the unique ID for the next measurement series. This is intended for
  // internal use.
  std::string GetNextMeasurementSeriesId() {
    return absl::StrCat(measurement_series_id_.Next());
  }

  // Returns the artifact writer. This is intended for internal use only.
  internal::ArtifactWriter& GetArtifactWriter() { return *writer_; }

  // Returns the test result caclculator. This is intended for internal use
  // only.
  TestResultCalculator& GetResultCalculator() { return *result_calculator_; }

 private:
  void CheckAndSetInitializationGuard();
  void RegisterLogSink();
  void EmitSchemaVersion();
  void End();
  void EmitStart() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void EmitEnd() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void DeregisterLogSink();
  void UnsetInitializationGuard();

  TestRunStart test_run_start_;
  std::unique_ptr<internal::ArtifactWriter> writer_;
  std::unique_ptr<TestResultCalculator> result_calculator_;
  internal::LogSink log_sink_;
  std::unique_ptr<DutInfo> dut_info_;
  internal::IntIncrementer step_id_;
  internal::IntIncrementer measurement_series_id_;

  absl::Mutex mutex_;
  bool started_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_TEST_RUN_H_
