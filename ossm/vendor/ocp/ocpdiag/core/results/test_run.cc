// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/test_run.h"

#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/base/thread_annotations.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/log_sink_registry.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/data_model/struct_to_proto.h"
#include "ocpdiag/core/results/data_model/struct_validators.h"
#include "ocpdiag/core/results/log_sink.h"
#include "ocpdiag/core/results/test_result_calculator.h"

ABSL_FLAG(bool, ocpdiag_copy_results_to_stdout, true,
          "Prints human-readable JSONL result artifacts to stdout");

ABSL_FLAG(std::string, ocpdiag_binary_results_filepath, "",
          "Fully-qualified file path where binary-proto result data will be "
          "written.");

ABSL_FLAG(bool, ocpdiag_log_to_results, true,
          "If set to true, the Abseil logger will be directed to OCPDiag "
          "results in addition to the Abseil default logging destination.");

namespace ocpdiag::results {

namespace {

ABSL_CONST_INIT absl::Mutex initialization_mutex(absl::kConstInit);
bool initialized ABSL_GUARDED_BY(initialization_mutex) = false;

}  // namespace

TestRun::TestRun(const TestRunStart& test_run_start,
                 std::unique_ptr<internal::ArtifactWriter> writer)
    : test_run_start_(test_run_start),
      writer_(writer == nullptr
                  ? std::make_unique<internal::ArtifactWriter>(
                        absl::GetFlag(FLAGS_ocpdiag_binary_results_filepath),
                        absl::GetFlag(FLAGS_ocpdiag_copy_results_to_stdout)
                            ? &std::cout
                            : nullptr)
                  : std::move(writer)),
      result_calculator_(std::make_unique<TestResultCalculator>()),
      log_sink_(*writer_) {
  CheckAndSetInitializationGuard();
  RegisterLogSink();
  ValidateStructOrDie(test_run_start);
  EmitSchemaVersion();
}

void TestRun::CheckAndSetInitializationGuard() {
  absl::MutexLock lock(&initialization_mutex);
  CHECK(!initialized)
      << "Only one TestRun object can be active at a time within a program";
  initialized = true;
}

void TestRun::RegisterLogSink() {
  if (absl::GetFlag(FLAGS_ocpdiag_log_to_results))
    absl::AddLogSink(&log_sink_);
}

void TestRun::EmitSchemaVersion() {
  ocpdiag_results_v2_pb::SchemaVersion schema_version;
  schema_version.set_major(kMajorSchemaVersion);
  schema_version.set_minor(kMinorSchemaVersion);
  writer_->Write(schema_version);
}

void TestRun::AddPreStartError(const Error& error) {
  absl::MutexLock lock(&mutex_);
  CHECK(!started_)
      << "Errors can only be added to the TestRun before it has been started - "
         "add errors that happen during the run to TestSteps";
  ValidateStructOrDie(error);
  ocpdiag_results_v2_pb::TestRunArtifact run_proto;
  *run_proto.mutable_error() = internal::StructToProto(error);
  writer_->Write(run_proto);
  result_calculator_->NotifyError();
}

void TestRun::AddPreStartLog(const Log& log) {
  absl::MutexLock lock(&mutex_);
  CHECK(!started_)
      << "Logs can only be added to the TestRun before it has been started - "
         "add logs that happen during the run to TestSteps";
  ValidateStructOrDie(log);
  ocpdiag_results_v2_pb::TestRunArtifact run_proto;
  *run_proto.mutable_log() = internal::StructToProto(log);
  writer_->Write(run_proto);

  // If the log is fatal, re-log the message to let Abseil handle exiting the
  // program
  if (log.severity == LogSeverity::kFatal) {
    writer_->Flush();
    LOG(FATAL) << log.message;
  }
}

void TestRun::StartAndRegisterDutInfo(std::unique_ptr<DutInfo> dut_info) {
  absl::MutexLock lock(&mutex_);
  CHECK(dut_info) << "DutInfo must be provided";
  result_calculator_->NotifyStartRun();
  started_ = true;
  dut_info_ = std::move(dut_info);
  EmitStart();
}

TestRun::~TestRun() {
  End();
  DeregisterLogSink();
  UnsetInitializationGuard();
}

void TestRun::End() {
  absl::MutexLock lock(&mutex_);
  if (!started_) EmitStart();
  result_calculator_->Finalize();
  EmitEnd();
}

void TestRun::EmitStart() {
  ocpdiag_results_v2_pb::TestRunStart start_proto =
      internal::StructToProto(test_run_start_);
  if (dut_info_ != nullptr)
    *start_proto.mutable_dut_info() = internal::DutInfoToProto(*dut_info_);
  ocpdiag_results_v2_pb::TestRunArtifact run_proto;
  *run_proto.mutable_test_run_start() = start_proto;
  writer_->Write(run_proto);
}

void TestRun::EmitEnd() {
  ocpdiag_results_v2_pb::TestRunArtifact run_proto;
  ocpdiag_results_v2_pb::TestRunEnd* end_proto =
      run_proto.mutable_test_run_end();
  end_proto->set_status(ocpdiag_results_v2_pb::TestRunEnd::TestStatus(
      result_calculator_->status()));
  end_proto->set_result(ocpdiag_results_v2_pb::TestRunEnd::TestResult(
      result_calculator_->result()));
  writer_->Write(run_proto);
  writer_->Flush();
}

void TestRun::DeregisterLogSink() {
  if (absl::GetFlag(FLAGS_ocpdiag_log_to_results))
    absl::RemoveLogSink(&log_sink_);
}

void TestRun::UnsetInitializationGuard() {
  absl::MutexLock initialization_lock(&initialization_mutex);
  initialized = false;
}

}  // namespace ocpdiag::results
