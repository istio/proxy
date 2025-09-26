// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OUTPUT_RECEIVER_H_
#define OCPDIAG_CORE_RESULTS_OUTPUT_RECEIVER_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/output_iterator.h"

namespace ocpdiag::results {

// A helper class for unit tests that consumes OCPDiag output artifacts. It
// collects any artifacts written to artifact_writer. The output can be consumed
// in two ways. Either you can use the structured OutputModel, or iterate over
// the class itself to get one OutputArtifact at a time.
//
// This class is not thread-safe and not meant for production code. It is
// intended to be used for unit testing.
class OutputReceiver {
 public:
  OutputReceiver();

  // Creates an artifact writer that will write to this receiver instance. This
  // should only be called once per OutputReceiver instance. Note that this
  // artifact writer will be set to not spin up additional threads (for periodic
  // file flushing) as this can disrupt unit tests.
  std::unique_ptr<internal::ArtifactWriter> MakeArtifactWriter();

  // Returns an iterable container of the raw output artifacts. It can be
  // iterated over as many times as you like. This should not be called until
  // an artifact writer has been created.
  const OutputContainer& GetOutputContainer() const;

  // Returns all the output artifacts in a structured data model. The
  // results are cached after the first call, so you should only call this after
  // the test has run to completion.
  //
  // This will store all of the output in memory at once. If that is a
  // problem, consider iterating over this class which holds only one output
  // artifact in memory at a time.
  const OutputModel& GetOutputModel();

  //
  // Unsets the currently stored model (if any) so that it will be rebuilt when
  // next accessed.
  void ResetModel() { model_.reset(); }

 private:
  void BuildModel();
  void HandleOutputArtifact(const OutputArtifact& artifact);
  void HandleTestRunArtifact(const TestRunArtifact& artifact);
  void HandleTestStepArtifact(const TestStepArtifact& artifact);
  int GetTestStepIdx(const std::string& test_step_id);
  void HandleMeasurementSeriesModel(const MeasurementSeriesModel& model);
  int GetMeasurementSeriesIdx(const std::string& measurement_series_id,
                              int step_idx);

  OutputContainer container_;
  std::optional<OutputModel> model_;
  absl::flat_hash_map<std::string, int> test_step_id_to_idx_;
  absl::flat_hash_map<std::string, int> measurement_series_id_to_idx_;
  bool writer_created_ = false;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OUTPUT_RECEIVER_H_
