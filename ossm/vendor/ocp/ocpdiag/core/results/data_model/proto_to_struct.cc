// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/proto_to_struct.h"

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/time_util.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "ocpdiag/core/compat/status_converters.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/results/data_model/variant.h"

namespace ocpdiag::results::internal {

Variant ProtoToVariant(google::protobuf::Value value) {
  if (value.has_string_value()) {
    return Variant(value.string_value());
  } else if (value.has_number_value()) {
    return value.number_value();
  } else if (value.has_bool_value()) {
    return value.bool_value();
  }
  LOG(FATAL) << "Tried to convert an invalid value protobuf to a Variant.";
}

SchemaVersionOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::SchemaVersion& schema_version) {
  return {.major = schema_version.major(), .minor = schema_version.minor()};
}

PlatformInfoOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::PlatformInfo& platform_info) {
  return {.info = platform_info.info()};
}

HardwareInfoOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::HardwareInfo& hardware_info) {
  return {
      {
          .name = hardware_info.name(),
          .computer_system = hardware_info.computer_system(),
          .location = hardware_info.location(),
          .odata_id = hardware_info.odata_id(),
          .part_number = hardware_info.part_number(),
          .serial_number = hardware_info.serial_number(),
          .manager = hardware_info.manager(),
          .manufacturer = hardware_info.manufacturer(),
          .manufacturer_part_number = hardware_info.manufacturer_part_number(),
          .part_type = hardware_info.part_type(),
          .version = hardware_info.version(),
          .revision = hardware_info.revision(),
      },
      hardware_info.hardware_info_id()};
}

SoftwareInfoOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::SoftwareInfo& software_info) {
  return {{
              .name = software_info.name(),
              .computer_system = software_info.computer_system(),
              .version = software_info.version(),
              .revision = software_info.revision(),
              .software_type = SoftwareType(software_info.software_type()),
          },
          software_info.software_info_id()};
}

DutInfoOutput ProtoToStruct(const ocpdiag_results_v2_pb::DutInfo& dut_info) {
  std::vector<PlatformInfoOutput> platform_infos;
  for (const ocpdiag_results_v2_pb::PlatformInfo& platform_info :
       dut_info.platform_infos())
    platform_infos.push_back(ProtoToStruct(platform_info));
  std::vector<HardwareInfoOutput> hardware_infos;
  for (const ocpdiag_results_v2_pb::HardwareInfo& hardware_info :
       dut_info.hardware_infos())
    hardware_infos.push_back(ProtoToStruct(hardware_info));
  std::vector<SoftwareInfoOutput> software_infos;
  for (const ocpdiag_results_v2_pb::SoftwareInfo& software_info :
       dut_info.software_infos())
    software_infos.push_back(ProtoToStruct(software_info));
  return {
      .dut_info_id = dut_info.dut_info_id(),
      .name = dut_info.name(),
      .metadata_json = ProtoToJsonOrDie(dut_info.metadata()),
      .platform_infos = platform_infos,
      .hardware_infos = hardware_infos,
      .software_infos = software_infos,
  };
}

TestRunStartOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestRunStart& test_run_start) {
  return {{
              .name = test_run_start.name(),
              .version = test_run_start.version(),
              .command_line = test_run_start.command_line(),
              .parameters_json = ProtoToJsonOrDie(test_run_start.parameters()),
              .metadata_json = ProtoToJsonOrDie(test_run_start.metadata()),
          },
          ProtoToStruct(test_run_start.dut_info())};
}

TestRunEndOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestRunEnd& test_run_end) {
  return {
      .status = TestStatus(test_run_end.status()),
      .result = TestResult(test_run_end.result()),
  };
}

LogOutput ProtoToStruct(const ocpdiag_results_v2_pb::Log& log) {
  return {.severity = LogSeverity(log.severity()), .message = log.message()};
}

ErrorOutput ProtoToStruct(const ocpdiag_results_v2_pb::Error& error) {
  return {
      .symptom = error.symptom(),
      .message = error.message(),
      .software_info_ids = std::vector<std::string>(
          error.software_info_ids().begin(), error.software_info_ids().end()),
  };
}

TestStepStartOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestStepStart& test_step_start) {
  return {.name = test_step_start.name()};
}

TestStepEndOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::TestStepEnd& test_step_end) {
  return {.status = TestStatus(test_step_end.status())};
}

SubcomponentOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::Subcomponent subcomponent) {
  return {
      .name = subcomponent.name(),
      .type = SubcomponentType(subcomponent.type()),
      .location = subcomponent.location(),
      .version = subcomponent.version(),
      .revision = subcomponent.revision(),
  };
}

ValidatorOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::Validator& validator) {
  std::vector<Variant> values;
  if (validator.value().has_list_value()) {
    for (const google::protobuf::Value& value :
         validator.value().list_value().values())
      values.push_back(ProtoToVariant(value));
  } else {
    values.push_back(ProtoToVariant(validator.value()));
  }
  return {
      .type = ValidatorType(validator.type()),
      .value = values,
      .name = validator.name(),
  };
}

MeasurementOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::Measurement& measurement) {
  std::optional<SubcomponentOutput> subcomponent = {};
  if (measurement.has_subcomponent())
    subcomponent = ProtoToStruct(measurement.subcomponent());
  std::vector<ValidatorOutput> validators;
  for (const ocpdiag_results_v2_pb::Validator& validator :
       measurement.validators())
    validators.push_back(ProtoToStruct(validator));
  return {
      .name = measurement.name(),
      .unit = measurement.unit(),
      .hardware_info_id = measurement.hardware_info_id(),
      .subcomponent = subcomponent,
      .validators = validators,
      .value = ProtoToVariant(measurement.value()),
      .metadata_json = ProtoToJsonOrDie(measurement.metadata()),
  };
}

MeasurementSeriesStartOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::MeasurementSeriesStart&
        measurement_series_start) {
  std::optional<SubcomponentOutput> subcomponent = {};
  if (measurement_series_start.has_subcomponent())
    subcomponent = ProtoToStruct(measurement_series_start.subcomponent());
  std::vector<ValidatorOutput> validators;
  for (const ocpdiag_results_v2_pb::Validator& validator :
       measurement_series_start.validators())
    validators.push_back(ProtoToStruct(validator));
  return {
      .measurement_series_id = measurement_series_start.measurement_series_id(),
      .name = measurement_series_start.name(),
      .unit = measurement_series_start.unit(),
      .hardware_info_id = measurement_series_start.hardware_info_id(),
      .subcomponent = subcomponent,
      .validators = validators,
      .metadata_json = ProtoToJsonOrDie(measurement_series_start.metadata()),
  };
}

MeasurementSeriesElementOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::MeasurementSeriesElement& element) {
  return {
      .index = element.index(),
      .measurement_series_id = element.measurement_series_id(),
      .value = ProtoToVariant(element.value()),
      .timestamp =
          google::protobuf::util::TimeUtil::TimestampToTimeval(element.timestamp()),
      .metadata_json = ProtoToJsonOrDie(element.metadata()),
  };
}

MeasurementSeriesEndOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::MeasurementSeriesEnd& measurement_series_end) {
  return {
      .measurement_series_id = measurement_series_end.measurement_series_id(),
      .total_count = measurement_series_end.total_count(),
  };
}

DiagnosisOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::Diagnosis& diagnosis) {
  std::optional<SubcomponentOutput> subcomponent = {};
  if (diagnosis.has_subcomponent())
    subcomponent = ProtoToStruct(diagnosis.subcomponent());
  return {
      .verdict = diagnosis.verdict(),
      .type = DiagnosisType(diagnosis.type()),
      .message = diagnosis.message(),
      .hardware_info_id = diagnosis.hardware_info_id(),
      .subcomponent = subcomponent,
  };
}

FileOutput ProtoToStruct(const ocpdiag_results_v2_pb::File& file) {
  return {
      .display_name = file.display_name(),
      .uri = file.uri(),
      .is_snapshot = file.is_snapshot(),
      .description = file.description(),
      .content_type = file.content_type(),
  };
}

ExtensionOutput ProtoToStruct(
    const ocpdiag_results_v2_pb::Extension& extension) {
  return {
      .name = extension.name(),
      .content_json = ProtoToJsonOrDie(extension.content()),
  };
}

TestStepArtifact ProtoToStruct(
    const ocpdiag_results_v2_pb::TestStepArtifact& artifact) {
  TestStepVariant variant;
  switch (artifact.artifact_case()) {
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kTestStepStart:
      variant = ProtoToStruct(artifact.test_step_start());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kTestStepEnd:
      variant = ProtoToStruct(artifact.test_step_end());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kMeasurement:
      variant = ProtoToStruct(artifact.measurement());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::
        kMeasurementSeriesStart:
      variant = ProtoToStruct(artifact.measurement_series_start());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::
        kMeasurementSeriesEnd:
      variant = ProtoToStruct(artifact.measurement_series_end());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::
        kMeasurementSeriesElement:
      variant = ProtoToStruct(artifact.measurement_series_element());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kDiagnosis:
      variant = ProtoToStruct(artifact.diagnosis());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kError:
      variant = ProtoToStruct(artifact.error());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kFile:
      variant = ProtoToStruct(artifact.file());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kLog:
      variant = ProtoToStruct(artifact.log());
      break;
    case ocpdiag_results_v2_pb::TestStepArtifact::ArtifactCase::kExtension:
      variant = ProtoToStruct(artifact.extension());
      break;
    default:
      LOG(FATAL) << "Tried to convert an empty or unexepected TestStepArtifact "
                    "from proto";
  }
  return {
      .artifact = variant,
      .test_step_id = artifact.test_step_id(),
  };
}

TestRunArtifact ProtoToStruct(
    const ocpdiag_results_v2_pb::TestRunArtifact& artifact) {
  switch (artifact.artifact_case()) {
    case ocpdiag_results_v2_pb::TestRunArtifact::ArtifactCase::kTestRunStart:
      return {.artifact = ProtoToStruct(artifact.test_run_start())};
    case ocpdiag_results_v2_pb::TestRunArtifact::ArtifactCase::kTestRunEnd:
      return {.artifact = ProtoToStruct(artifact.test_run_end())};
    case ocpdiag_results_v2_pb::TestRunArtifact::ArtifactCase::kLog:
      return {.artifact = ProtoToStruct(artifact.log())};
    case ocpdiag_results_v2_pb::TestRunArtifact::ArtifactCase::kError:
      return {.artifact = ProtoToStruct(artifact.error())};
    default:
      LOG(FATAL) << "Tried to convert an empty or unexepected TestRunArtifact "
                    "from proto";
  }
}

OutputArtifact ProtoToStruct(
    const ocpdiag_results_v2_pb::OutputArtifact& artifact) {
  OutputVariant variant;
  switch (artifact.artifact_case()) {
    case ocpdiag_results_v2_pb::OutputArtifact::ArtifactCase::kSchemaVersion:
      variant = ProtoToStruct(artifact.schema_version());
      break;
    case ocpdiag_results_v2_pb::OutputArtifact::ArtifactCase::kTestRunArtifact:
      variant = ProtoToStruct(artifact.test_run_artifact());
      break;
    case ocpdiag_results_v2_pb::OutputArtifact::ArtifactCase::kTestStepArtifact:
      variant = ProtoToStruct(artifact.test_step_artifact());
      break;
    default:
      LOG(FATAL) << "Tried to convert an empty or unexepected OutputArtifact "
                    "from proto";
  }
  return {
      .artifact = variant,
      .sequence_number = artifact.sequence_number(),
      .timestamp =
          google::protobuf::util::TimeUtil::TimestampToTimeval(artifact.timestamp()),
  };
}

std::string ProtoToJsonOrDie(const google::protobuf::Struct& proto) {
  std::string json;
  absl::Status status =
      AsAbslStatus(google::protobuf::util::MessageToJsonString(proto, &json));
  CHECK_OK(status) << "Issue converting struct type to JSON";
  return json;
}

}  // namespace ocpdiag::results::internal
