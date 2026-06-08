// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/struct_to_proto.h"

#include <variant>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/time_util.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/compat/status_converters.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/data_model/variant.h"

namespace ocpdiag::results::internal {

//
google::protobuf::Value VariantToProto(const Variant& value) {
  google::protobuf::Value proto;
  if (auto* str_val = std::get_if<std::string>(&value); str_val != nullptr) {
    proto.set_string_value(*str_val);
  } else if (auto* bool_val = std::get_if<bool>(&value); bool_val != nullptr) {
    proto.set_bool_value(*bool_val);
  } else if (auto* double_val = std::get_if<double>(&value);
             double_val != nullptr) {
    proto.set_number_value(*double_val);
  } else {
    LOG(FATAL) << "Tried to convert an invalid value.";
  }
  return proto;
}

ocpdiag_results_v2_pb::Validator StructToProto(const Validator& validator) {
  ocpdiag_results_v2_pb::Validator proto;
  proto.set_name(validator.name);
  proto.set_type(
      ocpdiag_results_v2_pb::Validator::ValidatorType(validator.type));

  if (validator.value.size() == 1) {
    *proto.mutable_value() = VariantToProto(validator.value[0]);
  } else {
    for (const Variant& value : validator.value) {
      *proto.mutable_value()->mutable_list_value()->add_values() =
          VariantToProto(value);
    }
  }
  return proto;
}

ocpdiag_results_v2_pb::HardwareInfo StructToProto(
    const HardwareInfoOutput& info) {
  ocpdiag_results_v2_pb::HardwareInfo proto;
  proto.set_hardware_info_id(info.hardware_info_id);
  proto.set_name(info.name);
  proto.set_computer_system(info.computer_system);
  proto.set_location(info.location);
  proto.set_odata_id(info.odata_id);
  proto.set_part_number(info.part_number);
  proto.set_serial_number(info.serial_number);
  proto.set_manager(info.manager);
  proto.set_manufacturer(info.manufacturer);
  proto.set_manufacturer_part_number(info.manufacturer_part_number);
  proto.set_part_type(info.part_type);
  proto.set_version(info.version);
  proto.set_revision(info.revision);
  return proto;
}

ocpdiag_results_v2_pb::SoftwareInfo StructToProto(
    const SoftwareInfoOutput& info) {
  ocpdiag_results_v2_pb::SoftwareInfo proto;
  proto.set_software_info_id(info.software_info_id);
  proto.set_name(info.name);
  proto.set_computer_system(info.computer_system);
  proto.set_version(info.version);
  proto.set_revision(info.revision);
  proto.set_software_type(
      ocpdiag_results_v2_pb::SoftwareInfo::SoftwareType(info.software_type));
  return proto;
}

ocpdiag_results_v2_pb::PlatformInfo StructToProto(const PlatformInfo& info) {
  ocpdiag_results_v2_pb::PlatformInfo proto;
  proto.set_info(info.info);
  return proto;
}

ocpdiag_results_v2_pb::Subcomponent StructToProto(
    const Subcomponent& subcomponent) {
  ocpdiag_results_v2_pb::Subcomponent proto;
  proto.set_name(subcomponent.name);
  proto.set_type(
      ocpdiag_results_v2_pb::Subcomponent::SubcomponentType(subcomponent.type));
  proto.set_location(subcomponent.location);
  proto.set_version(subcomponent.version);
  proto.set_revision(subcomponent.revision);
  return proto;
}

ocpdiag_results_v2_pb::MeasurementSeriesStart StructToProto(
    const MeasurementSeriesStart& measurement_series_start) {
  ocpdiag_results_v2_pb::MeasurementSeriesStart proto;
  proto.set_name(measurement_series_start.name);
  proto.set_unit(measurement_series_start.unit);
  if (measurement_series_start.hardware_info.has_value())
    proto.set_hardware_info_id(measurement_series_start.hardware_info->id());
  if (measurement_series_start.subcomponent.has_value())
    *proto.mutable_subcomponent() =
        StructToProto(*measurement_series_start.subcomponent);
  for (const Validator& v : measurement_series_start.validators)
    *proto.add_validators() = StructToProto(v);
  *proto.mutable_metadata() =
      JsonToProtoOrDie(measurement_series_start.metadata_json);
  return proto;
}

ocpdiag_results_v2_pb::MeasurementSeriesElement StructToProto(
    const MeasurementSeriesElement& measurement_series_element) {
  ocpdiag_results_v2_pb::MeasurementSeriesElement proto;
  *proto.mutable_value() = VariantToProto(measurement_series_element.value);
  if (measurement_series_element.timestamp.has_value()) {
    *proto.mutable_timestamp() = google::protobuf::util::TimeUtil::TimevalToTimestamp(
        *measurement_series_element.timestamp);
  }
  *proto.mutable_metadata() =
      JsonToProtoOrDie(measurement_series_element.metadata_json);
  return proto;
}

ocpdiag_results_v2_pb::Measurement StructToProto(
    const Measurement& measurement) {
  ocpdiag_results_v2_pb::Measurement proto;
  *proto.mutable_value() = VariantToProto(measurement.value);
  proto.set_name(measurement.name);
  proto.set_unit(measurement.unit);
  if (measurement.hardware_info.has_value())
    proto.set_hardware_info_id(measurement.hardware_info->id());
  if (measurement.subcomponent.has_value())
    *proto.mutable_subcomponent() = StructToProto(*measurement.subcomponent);
  for (const Validator& v : measurement.validators)
    *proto.add_validators() = StructToProto(v);
  *proto.mutable_metadata() = JsonToProtoOrDie(measurement.metadata_json);
  return proto;
}

ocpdiag_results_v2_pb::Diagnosis StructToProto(const Diagnosis& diagnosis) {
  ocpdiag_results_v2_pb::Diagnosis proto;
  proto.set_verdict(diagnosis.verdict);
  proto.set_type(ocpdiag_results_v2_pb::Diagnosis::Type(diagnosis.type));
  proto.set_message(diagnosis.message);
  if (diagnosis.hardware_info.has_value())
    proto.set_hardware_info_id(diagnosis.hardware_info->id());
  if (diagnosis.subcomponent.has_value())
    *proto.mutable_subcomponent() = StructToProto(*diagnosis.subcomponent);
  return proto;
}

ocpdiag_results_v2_pb::Error StructToProto(const Error& error) {
  ocpdiag_results_v2_pb::Error proto;
  proto.set_symptom(error.symptom);
  proto.set_message(error.message);
  for (const RegisteredSoftwareInfo& info : error.software_infos)
    proto.add_software_info_ids(info.id());
  return proto;
}

ocpdiag_results_v2_pb::File StructToProto(const File& file) {
  ocpdiag_results_v2_pb::File proto;
  proto.set_display_name(file.display_name);
  proto.set_uri(file.uri);
  proto.set_is_snapshot(file.is_snapshot);
  proto.set_description(file.description);
  proto.set_content_type(file.content_type);
  return proto;
}

ocpdiag_results_v2_pb::TestRunStart StructToProto(
    const TestRunStart& test_run_start) {
  ocpdiag_results_v2_pb::TestRunStart proto;
  proto.set_name(test_run_start.name);
  proto.set_version(test_run_start.version);
  proto.set_command_line(test_run_start.command_line);
  *proto.mutable_parameters() =
      JsonToProtoOrDie(test_run_start.parameters_json);
  *proto.mutable_metadata() = JsonToProtoOrDie(test_run_start.metadata_json);
  return proto;
}

ocpdiag_results_v2_pb::Log StructToProto(const Log& log) {
  ocpdiag_results_v2_pb::Log proto;
  proto.set_message(log.message);
  proto.set_severity(ocpdiag_results_v2_pb::Log::Severity(log.severity));
  return proto;
}

ocpdiag_results_v2_pb::Extension StructToProto(const Extension& extension) {
  ocpdiag_results_v2_pb::Extension proto;
  proto.set_name(extension.name);
  *proto.mutable_content() = JsonToProtoOrDie(extension.content_json);
  return proto;
}

google::protobuf::Struct JsonToProtoOrDie(absl::string_view json) {
  google::protobuf::Struct proto;
  if (json.empty()) return proto;
  absl::Status status =
      AsAbslStatus(google::protobuf::util::JsonStringToMessage(json, &proto));
  CHECK_OK(status) << "Must pass a valid JSON string to results objects: "
                   << status.ToString();
  return proto;
}

ocpdiag_results_v2_pb::DutInfo DutInfoToProto(const DutInfo& dut_info) {
  ocpdiag_results_v2_pb::DutInfo proto;
  proto.set_dut_info_id(dut_info.id());
  proto.set_name(dut_info.name());
  *proto.mutable_metadata() = JsonToProtoOrDie(dut_info.GetMetadataJson());

  for (const PlatformInfo& platform_info : dut_info.GetPlatformInfos())
    *proto.add_platform_infos() = StructToProto(platform_info);
  for (const HardwareInfoOutput& hardware_info : dut_info.GetHardwareInfos())
    *proto.add_hardware_infos() = StructToProto(hardware_info);
  for (const SoftwareInfoOutput& software_info : dut_info.GetSoftwareInfos())
    *proto.add_software_infos() = StructToProto(software_info);

  return proto;
}

}  // namespace ocpdiag::results::internal
