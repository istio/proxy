// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/examples/full_spec/full_spec.h"

#include <memory>

#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/measurement_series.h"
#include "ocpdiag/core/results/test_step.h"

namespace full_spec {

using ::ocpdiag::results::DiagnosisType;
using ::ocpdiag::results::DutInfo;
using ::ocpdiag::results::LogSeverity;
using ::ocpdiag::results::MeasurementSeries;
using ::ocpdiag::results::SoftwareType;
using ::ocpdiag::results::Subcomponent;
using ::ocpdiag::results::SubcomponentType;
using ::ocpdiag::results::TestStep;
using ::ocpdiag::results::ValidatorType;
using ::ocpdiag::results::internal::ArtifactWriter;

FullSpec::FullSpec(std::unique_ptr<ArtifactWriter> writer)
    : run_(
          {
              .name = "mlc_test",
              .version = "1.0",
              .command_line = "mlc/mlc --use_default_thresholds=true "
                              "--data_collection_mode=true",
              .parameters_json =
                  R"json({"max_bandwidth": 7200.0, "mode": "fast_mode", "data_collection_mode": true, "min_bandwidth": 700.0, "use_default_thresholds": true})json",
              .metadata_json = R"json({"some": "JSON"})json",
          },
          std::move(writer)) {}

void FullSpec::ExecuteTest() {
  AddPreStartArtifacts();
  run_.StartAndRegisterDutInfo(CreateDutInfo());
  AddBasicMeasurementAndDiagnosisStep();
  AddOtherStepArtifactsStep();
  AddSkippedStep();
  AddMeasurementSeriesStep();
}

void FullSpec::AddPreStartArtifacts() {
  run_.AddPreStartLog({.message = "Adding log before test start."});
  run_.AddPreStartLog(
      {.severity = LogSeverity::kWarning, .message = "This is a warning log."});
  run_.AddPreStartError({
      .symptom = "pre-start-error",
      .message = "This would be an error that occurs before starting the test, "
                 "usually when gathering DUT info.",
  });
}

std::unique_ptr<DutInfo> FullSpec::CreateDutInfo() {
  auto dut_info = std::make_unique<DutInfo>("ocp_lab_0222", "1");
  hw_infos_.push_back(dut_info->AddHardwareInfo({
      .name = "primary node",
      .computer_system = "primary_node",
      .location = "MB/DIMM_A1",
      .odata_id = "/redfish/v1/Systems/System.Embedded.1/Memory/DIMMSLOTA1",
      .part_number = "P03052-091",
      .serial_number = "HMA2022029281901",
      .manager = "bmc0",
      .manufacturer = "hynix",
      .manufacturer_part_number = "HMA84GR7AFR4N-VK",
      .part_type = "DIMM",
      .version = "1",
      .revision = "2",
  }));
  sw_infos_.push_back(dut_info->AddSoftwareInfo({
      .name = "bmc_firmware",
      .computer_system = "primary_node",
      .version = "1",
      .revision = "2",
      .software_type = SoftwareType::kFirmware,
  }));
  sw_infos_.push_back(dut_info->AddSoftwareInfo({
      .name = "bios",
      .computer_system = "primary_node",
      .version = "132.01",
      .revision = "2",
      .software_type = SoftwareType::kSystem,
  }));
  dut_info->AddPlatformInfo({.info = "memory_optimized"});
  dut_info->SetMetadataJson(R"json({"internal-id": "jktur"})json");

  return dut_info;
}

void FullSpec::AddBasicMeasurementAndDiagnosisStep() {
  TestStep step("intranode-bandwidth-check", run_);
  step.AddMeasurement({
      .name = "measured-fan-speed-100",
      .unit = "RPM",
      .hardware_info = hw_infos_[0],
      .subcomponent =
          Subcomponent{
              .name = "FAN1",
              .type = SubcomponentType::kUnspecified,
              .location = "F0_1",
              .version = "1",
              .revision = "1",
          },
      .validators = {{
                         .type = ValidatorType::kLessThanOrEqual,
                         .value = {11000.0},
                         .name = "80mm_fan_upper_limit",
                     },
                     {
                         .type = ValidatorType::kGreaterThanOrEqual,
                         .value = {8000.0},
                         .name = "80mm_fan_lower_limit",
                     }},
      .value = 9502.3,
      .metadata_json = R"json({"measurement-type": "FAN"})json",
  });
  step.AddDiagnosis({.verdict = "mlc-intranode-bandwidth-pass",
                     .type = DiagnosisType::kPass,
                     .message = "intranode bandwidth within threshold.",
                     .hardware_info = hw_infos_[0],
                     .subcomponent = Subcomponent{
                         .name = "QPI1",
                         .type = SubcomponentType::kBus,
                         .location = "CPU-3-2-3",
                         .version = "1",
                         .revision = "0",
                     }});
}

void FullSpec::AddOtherStepArtifactsStep() {
  TestStep step("dimm-configuration-check", run_);
  step.AddError({
      .symptom = "bad-return-code",
      .message = "software exited abnormally.",
      .software_infos = sw_infos_,
  });
  step.AddFile({
      .display_name = "mem_cfg_log",
      .uri = "file:///root/mem_cfg_log",
      .is_snapshot = false,
      .description = "DIMM configuration settings.",
      .content_type = "text/plain",
  });
  step.AddLog({
      .severity = LogSeverity::kDebug,
      .message = "This is a debug string.",
  });
  step.AddExtension({
      .name = "Extension",
      .content_json = R"json({"extra-identifier": 17})json",
  });
}

void FullSpec::AddSkippedStep() {
  TestStep step("skipped-step", run_);
  step.Skip();
}

void FullSpec::AddMeasurementSeriesStep() {
  TestStep step("fan-speed-measurements", run_);
  MeasurementSeries series(
      {
          .name = "measured-fan-speed-100",
          .unit = "RPM",
          .hardware_info = hw_infos_[0],
          .subcomponent =
              Subcomponent{
                  .name = "FAN1",
                  .type = SubcomponentType::kUnspecified,
                  .location = "F0_1",
                  .version = "1",
                  .revision = "1",
              },
          .validators = {{
                             .type = ValidatorType::kLessThan,
                             .value = {11000.0},
                             .name = "80mm_fan_upper_limit",
                         },
                         {
                             .type = ValidatorType::kGreaterThan,
                             .value = {8000.0},
                             .name = "80mm_fan_lower_limit",
                         }},
          .metadata_json = R"json({"extra-key": 5})json",
      },
      step);
  series.AddElement({.value = 9502.3});
  series.AddElement({.value = 9501.2});
}

}  // namespace full_spec
