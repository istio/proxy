// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#ifndef OCPDIAG_CORE_RESULTS_OCP_STRUCT_TO_PROTO_H_
#define OCPDIAG_CORE_RESULTS_OCP_STRUCT_TO_PROTO_H_

#include "google/protobuf/struct.pb.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"

namespace ocpdiag::results::internal {

// Converts the OCP data struct to its corresponding protobuf
ocpdiag_results_v2_pb::MeasurementSeriesStart StructToProto(
    const MeasurementSeriesStart& measurement_series_start);
ocpdiag_results_v2_pb::MeasurementSeriesElement StructToProto(
    const MeasurementSeriesElement& measurement_series_element);
ocpdiag_results_v2_pb::Measurement StructToProto(const Measurement& measurement);
ocpdiag_results_v2_pb::Diagnosis StructToProto(const Diagnosis& diagnosis);
ocpdiag_results_v2_pb::Error StructToProto(const Error& error);
ocpdiag_results_v2_pb::File StructToProto(const File& file);
ocpdiag_results_v2_pb::TestRunStart StructToProto(
    const TestRunStart& test_run_start);
ocpdiag_results_v2_pb::Log StructToProto(const Log& log);
ocpdiag_results_v2_pb::Extension StructToProto(const Extension& extension);

// Converts a JSON string to a generic protobuf struct or throws a fatal
// CHECK error
google::protobuf::Struct JsonToProtoOrDie(absl::string_view json);

// Convert the DutInfo class into its corresponding protobuf
ocpdiag_results_v2_pb::DutInfo DutInfoToProto(const DutInfo& dut_info);

}  // namespace ocpdiag::results::internal

#endif  // OCPDIAG_CORE_RESULTS_OCP_STRUCT_TO_PROTO_H_
