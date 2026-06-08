// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/measurement_series.h"

#include <iostream>

#include "google/protobuf/timestamp.pb.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/struct_to_proto.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/data_model/struct_validators.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/test_step.h"
#include "google/protobuf/util/time_util.h"

namespace ocpdiag::results {

using google::protobuf::util::TimeUtil;

MeasurementSeries::MeasurementSeries(const MeasurementSeriesStart& start,
                                     TestStep& test_step)
    : test_step_(test_step),
      series_id_(test_step.GetTestRun().GetNextMeasurementSeriesId()) {
  CHECK(!test_step.Ended())
      << "MeasurementSeries can only be created with active TestSteps";
  ValidateStructOrDie(start);
  if (!start.validators.empty()) {
    // Validation garuntees that all validators have the same type, so we can
    // use the index of the first one
    SetAndCheckSeriesType(start.validators[0].value[0].index());
  }
  EmitStart(start);
}

void MeasurementSeries::EmitStart(const MeasurementSeriesStart& start) {
  ocpdiag_results_v2_pb::TestStepArtifact proto;
  *proto.mutable_measurement_series_start() = internal::StructToProto(start);
  proto.mutable_measurement_series_start()->set_measurement_series_id(
      series_id_);
  AssignStepIdAndEmitArtifact(proto);
  GetArtifactWriter().Flush();
}

void MeasurementSeries::AddElement(const MeasurementSeriesElement& element) {
  google::protobuf::Timestamp now = TimeUtil::GetCurrentTime();
  SetAndCheckSeriesType(element.value.index());

  ocpdiag_results_v2_pb::TestStepArtifact step_proto;
  *step_proto.mutable_measurement_series_element() =
      internal::StructToProto(element);

  ocpdiag_results_v2_pb::MeasurementSeriesElement* element_proto =
      step_proto.mutable_measurement_series_element();
  if (!element.timestamp.has_value()) *element_proto->mutable_timestamp() = now;
  element_proto->set_index(element_count_.Next());
  element_proto->set_measurement_series_id(series_id_);

  absl::MutexLock lock(&mutex_);
  CHECK(!test_step_.Ended()) << "Cannot add elements to a MeasurementSeries "
                                "associated with a TestStep that has ended";
  CHECK(!ended_) << "Cannot add elements to a MeasurementSeries that has ended";
  AssignStepIdAndEmitArtifact(step_proto);
}

void MeasurementSeries::SetAndCheckSeriesType(int type_index) {
  absl::MutexLock lock(&mutex_);
  if (type_index_ == -1) type_index_ = type_index;
  CHECK(type_index_ == type_index)
      << "All validators and elements in a measurement series "
         "must have the same type.";
}

void MeasurementSeries::End() {
  absl::MutexLock lock(&mutex_);
  if (ended_) return;

  // Cannot use a CHECK error here because it is called in the destructor, so
  // just log to cerr
  if (test_step_.Ended()) {
    std::cerr << "The MeasurementSeries with id \"" << series_id_
              << "\" must be ended before "
                 "the TestStep that is associated with it.";
  }

  ended_ = true;
  EmitEnd();
}

void MeasurementSeries::EmitEnd() {
  ocpdiag_results_v2_pb::TestStepArtifact step_proto;
  ocpdiag_results_v2_pb::MeasurementSeriesEnd* end_proto =
      step_proto.mutable_measurement_series_end();
  end_proto->set_measurement_series_id(series_id_);
  end_proto->set_total_count(element_count_.Next());
  AssignStepIdAndEmitArtifact(step_proto);
  GetArtifactWriter().Flush();
}

void MeasurementSeries::AssignStepIdAndEmitArtifact(
    ocpdiag_results_v2_pb::TestStepArtifact& artifact) {
  artifact.set_test_step_id(test_step_.Id());
  GetArtifactWriter().Write(artifact);
}

internal::ArtifactWriter& MeasurementSeries::GetArtifactWriter() {
  return test_step_.GetTestRun().GetArtifactWriter();
}

bool MeasurementSeries::Ended() const {
  absl::MutexLock lock(&mutex_);
  return ended_;
}

}  // namespace ocpdiag::results
