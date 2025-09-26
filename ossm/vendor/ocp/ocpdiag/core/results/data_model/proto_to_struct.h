// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_PROTO_CONVERTERS_H_
#define OCPDIAG_CORE_RESULTS_OCP_PROTO_CONVERTERS_H_

#include "google/protobuf/struct.pb.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"

namespace ocpdiag::results::internal {

// Coverts a protobuf to its corresponding OCP data output struct
SchemaVersionOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::SchemaVersion& schema_version);
TestRunStartOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestRunStart& test_run_start);
TestRunEndOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestRunEnd& test_run_end);
LogOutput ProtoToStruct(const ocpdiag_results_v2_pb::Log& log);
ErrorOutput ProtoToStruct(const ocpdiag_results_v2_pb::Error& error);
TestStepStartOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestStepStart& test_step_start);
TestStepEndOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestStepEnd& test_step_end);
MeasurementOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::Measurement& measurement);
MeasurementSeriesStartOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::MeasurementSeriesStart&
        measurement_series_start);
MeasurementSeriesElementOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::MeasurementSeriesElement& element);
MeasurementSeriesEndOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::MeasurementSeriesEnd& measurement_series_end);
DiagnosisOutput ProtoToStruct(const ocpdiag_results_v2_pb::Diagnosis& diagnosis);
FileOutput ProtoToStruct(const ocpdiag_results_v2_pb::File& file);
ExtensionOutput ProtoToStruct(const ocpdiag_results_v2_pb::Extension& extension);
OutputArtifact ProtoToStruct(
    const ocpdiag_results_v2_pb::OutputArtifact& artifact);

// Converts a protobuf struct to a JSON string or throws a fatal CHECK error
std::string ProtoToJsonOrDie(const google::protobuf::Struct& proto);

}  // namespace ocpdiag::results::internal

#endif  // OCPDIAG_CORE_RESULTS_OCP_PROTO_CONVERTERS_H_
