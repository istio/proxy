// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/output_receiver.h"

#include <filesystem>  //
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <variant>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/output_iterator.h"
#include "ocpdiag/core/testing/file_utils.h"

namespace ocpdiag::results {

OutputReceiver::OutputReceiver()
    : container_([] {
        std::string path = testutils::MkTempFileOrDie("output_receiver");
        if (std::filesystem::exists(path))
          CHECK(std::filesystem::remove(path)) << "Cannot remove temp file";
        return path;
      }()) {}

std::unique_ptr<internal::ArtifactWriter> OutputReceiver::MakeArtifactWriter() {
  CHECK(!writer_created_)
      << "Attempted to create an Artifact Writer when one has already been "
         "created for this Output Receiver";
  writer_created_ = true;

  // Create an artifact writer that outputs to a file, as well as stdout for
  // easier examination during unit tests.
  std::ostream* out_stream = nullptr;

  return std::make_unique<internal::ArtifactWriter>(
      container_.file_path(), out_stream, /*flush_each_minute=*/false);
}

const OutputContainer& OutputReceiver::GetOutputContainer() const {
  CHECK(writer_created_) << "Attempted to access receiver contents before "
                            "creating an Artifact Writer";
  return container_;
}

const OutputModel& OutputReceiver::GetOutputModel() {
  if (!model_.has_value()) BuildModel();
  return *model_;
}

void OutputReceiver::BuildModel() {
  model_ = OutputModel();
  for (const OutputArtifact& artifact : GetOutputContainer())
    HandleOutputArtifact(artifact);
}

void OutputReceiver::HandleOutputArtifact(const OutputArtifact& artifact) {
  if (auto* test_run = std::get_if<TestRunArtifact>(&artifact.artifact);
      test_run != nullptr) {
    HandleTestRunArtifact(*test_run);
  } else if (auto* test_step =
                 std::get_if<TestStepArtifact>(&artifact.artifact);
             test_step != nullptr) {
    HandleTestStepArtifact(*test_step);
  } else if (auto* schema_version =
                 std::get_if<SchemaVersionOutput>(&artifact.artifact);
             schema_version != nullptr) {
    model_->schema_version = *schema_version;
  } else {
    LOG(FATAL) << "Tried to parse an invalid output artifact.";
  }
}

void OutputReceiver::HandleTestRunArtifact(const TestRunArtifact& artifact) {
  if (auto* test_run_start =
          std::get_if<TestRunStartOutput>(&artifact.artifact);
      test_run_start != nullptr) {
    model_->test_run.start = *test_run_start;
  } else if (auto* test_run_end =
                 std::get_if<TestRunEndOutput>(&artifact.artifact);
             test_run_end != nullptr) {
    model_->test_run.end = *test_run_end;
  } else if (auto* log = std::get_if<LogOutput>(&artifact.artifact);
             log != nullptr) {
    model_->test_run.pre_start_logs.push_back(*log);
  } else if (auto* error = std::get_if<ErrorOutput>(&artifact.artifact);
             error != nullptr) {
    model_->test_run.pre_start_errors.push_back(*error);
  } else {
    LOG(FATAL) << "Tried to parse an invalid test run artifact.";
  }
}

void OutputReceiver::HandleTestStepArtifact(const TestStepArtifact& artifact) {
  int idx = GetTestStepIdx(artifact.test_step_id);
  if (auto* test_step_start =
          std::get_if<TestStepStartOutput>(&artifact.artifact);
      test_step_start != nullptr) {
    model_->test_steps[idx].start = *test_step_start;
  } else if (auto* test_step_end =
                 std::get_if<TestStepEndOutput>(&artifact.artifact);
             test_step_end != nullptr) {
    model_->test_steps[idx].end = *test_step_end;
  } else if (auto* log = std::get_if<LogOutput>(&artifact.artifact);
             log != nullptr) {
    model_->test_steps[idx].logs.push_back(*log);
  } else if (auto* error = std::get_if<ErrorOutput>(&artifact.artifact);
             error != nullptr) {
    model_->test_steps[idx].errors.push_back(*error);
  } else if (auto* file = std::get_if<FileOutput>(&artifact.artifact);
             file != nullptr) {
    model_->test_steps[idx].files.push_back(*file);
  } else if (auto* extension = std::get_if<ExtensionOutput>(&artifact.artifact);
             extension != nullptr) {
    model_->test_steps[idx].extensions.push_back(*extension);
  } else if (auto* measurement_series_start =
                 std::get_if<MeasurementSeriesStartOutput>(&artifact.artifact);
             measurement_series_start != nullptr) {
    model_->test_steps[idx]
        .measurement_series[GetMeasurementSeriesIdx(
            measurement_series_start->measurement_series_id, idx)]
        .start = *measurement_series_start;
  } else if (auto* measurement_series_element =
                 std::get_if<MeasurementSeriesElementOutput>(
                     &artifact.artifact);
             measurement_series_element != nullptr) {
    model_->test_steps[idx]
        .measurement_series[GetMeasurementSeriesIdx(
            measurement_series_element->measurement_series_id, idx)]
        .elements.push_back(*measurement_series_element);
  } else if (auto* measurement_series_end =
                 std::get_if<MeasurementSeriesEndOutput>(&artifact.artifact);
             measurement_series_end != nullptr) {
    model_->test_steps[idx]
        .measurement_series[GetMeasurementSeriesIdx(
            measurement_series_end->measurement_series_id, idx)]
        .end = *measurement_series_end;
  } else if (auto* measurement =
                 std::get_if<MeasurementOutput>(&artifact.artifact);
             measurement != nullptr) {
    model_->test_steps[idx].measurements.push_back(*measurement);
  } else if (auto* diagnosis = std::get_if<DiagnosisOutput>(&artifact.artifact);
             diagnosis != nullptr) {
    model_->test_steps[idx].diagnoses.push_back(*diagnosis);
  } else {
    LOG(FATAL) << "Tried to parse an invalid test step artifact.";
  }
}

int OutputReceiver::GetTestStepIdx(const std::string& test_step_id) {
  if (test_step_id_to_idx_.contains(test_step_id))
    return test_step_id_to_idx_[test_step_id];

  int idx = model_->test_steps.size();
  test_step_id_to_idx_[test_step_id] = idx;
  model_->test_steps.push_back(TestStepModel({.test_step_id = test_step_id}));
  return idx;
}

// Note that measurement_series_ids are unique to the test run while the
// indices returned by this function will be relative to the test step
int OutputReceiver::GetMeasurementSeriesIdx(
    const std::string& measurement_series_id, int step_idx) {
  if (measurement_series_id_to_idx_.contains(measurement_series_id))
    return measurement_series_id_to_idx_[measurement_series_id];

  int idx = model_->test_steps[step_idx].measurement_series.size();
  measurement_series_id_to_idx_[measurement_series_id] = idx;
  model_->test_steps[step_idx].measurement_series.push_back(
      MeasurementSeriesModel());
  return idx;
}

}  // namespace ocpdiag::results
