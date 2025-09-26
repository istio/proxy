// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_TEST_STEP_H_
#define OCPDIAG_CORE_RESULTS_OCP_TEST_STEP_H_

#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/test_run.h"

namespace ocpdiag::results {

// A logical subdivision of the TestRun used to emit most of artifacts created
// during the test.
class TestStep {
 public:
  TestStep(absl::string_view name, TestRun& test_run);
  TestStep(const TestStep&) = delete;
  TestStep& operator=(const TestStep&) = delete;
  ~TestStep() { End(); }

  // Adds a measurement to the test step.
  void AddMeasurement(const Measurement& measurement);

  // Adds a diagnosis to the test step. A fail diagnosis will cause the test run
  // as a whole to gain the fail result.
  void AddDiagnosis(const Diagnosis& diagnosis);

  // Adds an error to the test step. This will cause both the test step and the
  // test run as whole to gain the error status.
  void AddError(const Error& error);

  // Adds a file to the test step.
  //
  void AddFile(const File& file);

  // Adds a log to the test step. A fatal log will cause the program to exit.
  void AddLog(const Log& log);

  // Adds an extension to the test step.
  void AddExtension(const Extension& extension);

  // Updates the test step status to skipped and ends the test. This will not
  // override the error status.
  void Skip();

  // Ends the test step, emitting the TestStepEnd artifact. No additional
  // artifacts can be added to the TestStep after this has been called.
  void End();

  // Returns the current test step status.
  TestStatus Status() const;

  // Returns true if the test step has ended.
  bool Ended() const;

  // Returns the test step id.
  std::string Id() const { return id_; }

  // Returns the test step name.
  std::string Name() const { return name_; }

  // Returns a reference to the TestRun. This is intended for internal use only.
  TestRun& GetTestRun() { return test_run_; }

 private:
  void EmitStart();
  void CheckEndedAndEmitArtifact(
      ocpdiag_results_v2_pb::TestStepArtifact& artifact);
  void EmitEnd() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void AssignIdAndEmitArtifact(
      ocpdiag_results_v2_pb::TestStepArtifact& artifact);
  internal::ArtifactWriter& GetArtifactWriter();

  TestRun& test_run_;
  std::string id_;
  std::string name_;
  mutable absl::Mutex mutex_;
  TestStatus status_ ABSL_GUARDED_BY(mutex_) = TestStatus::kUnknown;
  bool ended_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_TEST_STEP_H_
