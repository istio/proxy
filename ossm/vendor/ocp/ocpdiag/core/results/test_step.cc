// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/test_step.h"

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/data_model/struct_to_proto.h"
#include "ocpdiag/core/results/data_model/struct_validators.h"
#include "ocpdiag/core/results/test_run.h"

namespace ocpdiag::results {

TestStep::TestStep(absl::string_view name, TestRun& test_run)
    : test_run_(test_run), id_(test_run.GetNextStepId()), name_(name) {
  CHECK(test_run.Started())
      << "TestSteps must be created after the test run has started";
  EmitStart();
}

void TestStep::EmitStart() {
  CHECK(!name_.empty()) << "Test step names cannot be empty";
  ocpdiag_results_v2_pb::TestStepArtifact step_proto;
  ocpdiag_results_v2_pb::TestStepStart* start_proto =
      step_proto.mutable_test_step_start();
  start_proto->set_name(name_);
  AssignIdAndEmitArtifact(step_proto);
  GetArtifactWriter().Flush();
}

void TestStep::AddMeasurement(const Measurement& measurement) {
  ValidateStructOrDie(measurement);
  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_measurement() = internal::StructToProto(measurement);
  CheckEndedAndEmitArtifact(proto);
}

void TestStep::AddDiagnosis(const Diagnosis& diagnosis) {
  ValidateStructOrDie(diagnosis);
  if (diagnosis.type == DiagnosisType::kFail)
    test_run_.GetResultCalculator().NotifyFailureDiagnosis();
  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_diagnosis() = internal::StructToProto(diagnosis);
  CheckEndedAndEmitArtifact(proto);
}

void TestStep::AddError(const Error& error) {
  ValidateStructOrDie(error);
  {
    absl::MutexLock lock(&mutex_);
    status_ = TestStatus::kError;
  }
  test_run_.GetResultCalculator().NotifyError();

  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_error() = internal::StructToProto(error);
  CheckEndedAndEmitArtifact(proto);
}

void TestStep::AddFile(const File& file) {
  ValidateStructOrDie(file);
  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_file() = internal::StructToProto(file);
  CheckEndedAndEmitArtifact(proto);
}

void TestStep::AddLog(const Log& log) {
  ValidateStructOrDie(log);
  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_log() = internal::StructToProto(log);
  CheckEndedAndEmitArtifact(proto);

  // If the log is fatal, re-log the message to let Abseil handle exiting the
  // program.
  //
  if (log.severity == LogSeverity::kFatal) {
    test_run_.GetArtifactWriter().Flush();
    LOG(FATAL) << log.message;
  }
}

void TestStep::AddExtension(const Extension& extension) {
  ValidateStructOrDie(extension);
  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_extension() = internal::StructToProto(extension);
  CheckEndedAndEmitArtifact(proto);
}

void TestStep::CheckEndedAndEmitArtifact(
    ocpdiag_results_v2_pb::TestStepArtifact& artifact) {
  absl::MutexLock lock(&mutex_);
  CHECK(!ended_) << "Artifacts cannot be added once the step has ended";
  AssignIdAndEmitArtifact(artifact);
}

void TestStep::Skip() {
  {
    absl::MutexLock lock(&mutex_);
    if (status_ == TestStatus::kUnknown) status_ = TestStatus::kSkip;
  }
  End();
}

TestStatus TestStep::Status() const {
  absl::MutexLock lock(&mutex_);
  return status_;
}

bool TestStep::Ended() const {
  absl::MutexLock lock(&mutex_);
  return ended_;
}

void TestStep::End() {
  absl::MutexLock lock(&mutex_);
  if (ended_) return;
  ended_ = true;
  if (status_ == TestStatus::kUnknown) status_ = TestStatus::kComplete;
  EmitEnd();
}

void TestStep::EmitEnd() {
  ocpdiag_results_v2_pb::TestStepArtifact step_proto;
  ocpdiag_results_v2_pb::TestStepEnd* end_proto =
      step_proto.mutable_test_step_end();
  end_proto->set_status(ocpdiag_results_v2_pb::TestRunEnd::TestStatus(status_));
  AssignIdAndEmitArtifact(step_proto);
  GetArtifactWriter().Flush();
}

void TestStep::AssignIdAndEmitArtifact(
    ocpdiag_results_v2_pb::TestStepArtifact& artifact) {
  artifact.set_test_step_id(id_);
  GetArtifactWriter().Write(artifact);
}

internal::ArtifactWriter& TestStep::GetArtifactWriter() {
  return test_run_.GetArtifactWriter();
}

}  // namespace ocpdiag::results
