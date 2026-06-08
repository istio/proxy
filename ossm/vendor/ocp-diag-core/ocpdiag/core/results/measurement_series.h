// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_MEASUREMENT_SERIES_H_
#define OCPDIAG_CORE_RESULTS_OCP_MEASUREMENT_SERIES_H_

#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/int_incrementer.h"
#include "ocpdiag/core/results/test_step.h"

namespace ocpdiag::results {

class MeasurementSeries {
 public:
  MeasurementSeries(const MeasurementSeriesStart& start, TestStep& test_step);
  MeasurementSeries(const MeasurementSeries&) = delete;
  MeasurementSeries& operator=(const MeasurementSeries&) = delete;
  ~MeasurementSeries() { End(); }

  // Adds an element to the MeasurementSeries. Elements cannot be added once the
  // series or its assocated test step has been ended. All elements must be the
  // same type as each other and the Validators included in
  // MeasurementSeriesStart, if any.
  void AddElement(const MeasurementSeriesElement& element);

  // Ends the series. Ending the series after the associated test step will
  // cause a failure.
  void End();

  // Indicates whether the series has been ended.
  bool Ended() const;

  // Returns the measurement series id.
  std::string Id() const { return series_id_; }

 private:
  void EmitStart(const MeasurementSeriesStart& start);
  void EmitEnd() ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  void SetAndCheckSeriesType(int type_index);
  void AssignStepIdAndEmitArtifact(
      ocpdiag_results_v2_pb::TestStepArtifact& artifact);
  internal::ArtifactWriter& GetArtifactWriter();

  TestStep& test_step_;
  std::string series_id_;
  internal::IntIncrementer element_count_;

  mutable absl::Mutex mutex_;
  bool ended_ ABSL_GUARDED_BY(mutex_) = false;
  int type_index_ ABSL_GUARDED_BY(mutex_) = -1;
};

}  // namespace ocpdiag::results

#endif  // OCPDIAG_CORE_RESULTS_OCP_MEASUREMENT_SERIES_H_
