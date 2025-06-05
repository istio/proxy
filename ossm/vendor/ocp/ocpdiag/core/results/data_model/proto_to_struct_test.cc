// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/proto_to_struct.h"

#include <vector>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/testing/parse_text_proto.h"

using ::ocpdiag::testing::ParseTextProtoOrDie;

namespace ocpdiag::results::internal {

Subcomponent GetExampleSubcomponent() {
  return {
      .name = "FAN1",
      .type = SubcomponentType::kUnspecified,
      .location = "F0_1",
      .version = "1",
      .revision = "1",
  };
}

TEST(ProtoToStructTest, SchemaVersionProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    schema_version { major: 2 minor: 0 }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact(
                {.artifact = SchemaVersionOutput({.major = 2, .minor = 0})}));
}

TEST(ProtoToStructTest, TestRunStartProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(
      R"pb(
        test_run_artifact {
          test_run_start {
            name: "mlc_test"
            version: "1.0"
            command_line: "mlc/mlc --use_default_thresholds=true --data_collection_mode=true"
            parameters {
              fields {
                key: "use_default_thresholds"
                value { bool_value: true }
              }
            }
            dut_info {
              dut_info_id: "mydut"
              name: "dut"
              platform_infos { info: "memory_optimized" }
              hardware_infos {
                hardware_info_id: "1"
                computer_system: "primary_node"
                name: "primary node"
                location: "MB/DIMM_A1"
                odata_id: "/redfish/v1/Systems/System.Embedded.1/Memory/DIMMSLOTA1"
                part_number: "P03052-091"
                serial_number: "HMA2022029281901"
                manager: "bmc0"
                manufacturer: "hynix"
                manufacturer_part_number: "HMA84GR7AFR4N-VK"
                part_type: "DIMM"
                version: "1"
                revision: "2"
              }
              software_infos {
                software_info_id: "1"
                computer_system: "primary_node"
                name: "bmc_firmware"
                version: "1"
                revision: "2"
                software_type: FIRMWARE
              }
              metadata {
                fields {
                  key: "some"
                  value { string_value: "JSON" }
                }
              }
            }
            metadata {
              fields {
                key: "some"
                value { string_value: "JSON" }
              }
            }
          }
        }
      )pb");

  TestRunStartOutput test_run_start = {
      {
          .name = "mlc_test",
          .version = "1.0",
          .command_line = "mlc/mlc --use_default_thresholds=true "
                          "--data_collection_mode=true",
          .parameters_json = R"json({"use_default_thresholds":true})json",
          .metadata_json = R"json({"some":"JSON"})json",
      },
      {
          .dut_info_id = "mydut",
          .name = "dut",
          .metadata_json = R"json({"some":"JSON"})json",
          .platform_infos = {{.info = "memory_optimized"}},
          .hardware_infos = {{{
                                  .name = "primary node",
                                  .computer_system = "primary_node",
                                  .location = "MB/DIMM_A1",
                                  .odata_id = "/redfish/v1/Systems/"
                                              "System.Embedded.1/"
                                              "Memory/DIMMSLOTA1",
                                  .part_number = "P03052-091",
                                  .serial_number = "HMA2022029281901",
                                  .manager = "bmc0",
                                  .manufacturer = "hynix",
                                  .manufacturer_part_number =
                                      "HMA84GR7AFR4N-VK",
                                  .part_type = "DIMM",
                                  .version = "1",
                                  .revision = "2",
                              },
                              "1"}},
          .software_infos = {{{
                                  .name = "bmc_firmware",
                                  .computer_system = "primary_node",
                                  .version = "1",
                                  .revision = "2",
                                  .software_type = SoftwareType::kFirmware,
                              },
                              "1"}},
      },
  };

  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact{
                .artifact = {TestRunArtifact{.artifact = test_run_start}}});
}

TEST(ProtoToStructTest, TestRunEndProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_run_artifact { test_run_end { status: COMPLETE result: PASS } }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact({.artifact = TestRunArtifact(
                                {.artifact = TestRunEndOutput(
                                     {.status = TestStatus::kComplete,
                                      .result = TestResult::kPass})})}));
}

TEST(ProtoToStructTest, LogProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_run_artifact {
      log {
        severity: ERROR
        message: "file operation not completed successfully."
      }
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact(
          {.artifact = TestRunArtifact(
               {.artifact = LogOutput({
                    .severity = LogSeverity::kError,
                    .message = "file operation not completed successfully.",
                })})}));
}

TEST(ProtoToStructTest, ErrorProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_run_artifact {
      error {
        symptom: "bad-return-code"
        message: "software exited abnormally."
        software_info_ids: "1"
        software_info_ids: "2"
      }
    }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact({.artifact = TestRunArtifact(
                                {.artifact = ErrorOutput({
                                     .symptom = "bad-return-code",
                                     .message = "software exited abnormally.",
                                     .software_info_ids = {"1", "2"},
                                 })})}));
}

TEST(ProtoToStructTest, TestStepStartProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact { test_step_start { name: "my step" } }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact(
                {.artifact = TestStepArtifact(
                     {.artifact = TestStepStartOutput({.name = "my step"})})}));
}

TEST(ProtoToStructTest, TestStepEndProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact { test_step_end { status: ERROR } }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact({.artifact = TestStepArtifact(
                                {.artifact = TestStepEndOutput(
                                     {.status = TestStatus::kError})})}));
}

TEST(ProtoToStructTest, MeasurementProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      measurement {
        name: "measured-fan-speed-100"
        unit: "RPM"
        hardware_info_id: "5"
        subcomponent {
          name: "FAN1"
          location: "F0_1"
          version: "1"
          revision: "1"
          type: UNSPECIFIED
        }
        validators {
          name: "Fan name"
          type: EQUAL
          value: { string_value: "My fan name" }
        }
        value { string_value: "My fan name" }
        metadata {
          fields {
            key: "some"
            value { string_value: "JSON" }
          }
        }
      }
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact({.artifact = TestStepArtifact(
                          {.artifact = MeasurementOutput({
                               .name = "measured-fan-speed-100",
                               .unit = "RPM",
                               .hardware_info_id = "5",
                               .subcomponent = GetExampleSubcomponent(),
                               .validators = {{
                                   .type = ValidatorType::kEqual,
                                   .value = {"My fan name"},
                                   .name = "Fan name",
                               }},
                               .value = "My fan name",
                               .metadata_json = R"json({"some":"JSON"})json",
                           })})}));
}

TEST(ProtoToStructTest, MeasurementSeriesStartProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      measurement_series_start {
        measurement_series_id: "13"
        name: "measured-fan-speed-100"
        unit: "RPM"
        hardware_info_id: "5"
        subcomponent {
          name: "FAN1"
          location: "F0_1"
          version: "1"
          revision: "1"
          type: UNSPECIFIED
        }
        validators {
          name: "80mm_fan_upper_limit"
          type: LESS_THAN_OR_EQUAL
          value: { number_value: 11000.0 }
        }
        validators {
          name: "80mm_fan_lower_limit"
          type: GREATER_THAN_OR_EQUAL
          value: { number_value: 8000.0 }
        }
        metadata {
          fields {
            key: "some"
            value { string_value: "JSON" }
          }
        }
      }
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact(
          {.artifact = TestStepArtifact(
               {.artifact = MeasurementSeriesStartOutput({
                    .measurement_series_id = "13",
                    .name = "measured-fan-speed-100",
                    .unit = "RPM",
                    .hardware_info_id = "5",
                    .subcomponent = GetExampleSubcomponent(),
                    .validators = {{
                                       .type = ValidatorType::kLessThanOrEqual,
                                       .value = {11000.},
                                       .name = "80mm_fan_upper_limit",
                                   },
                                   {
                                       .type =
                                           ValidatorType::kGreaterThanOrEqual,
                                       .value = {8000.},
                                       .name = "80mm_fan_lower_limit",
                                   }},
                    .metadata_json = R"json({"some":"JSON"})json",
                })})}));
}

TEST(ProtoToStructTest, MeasurementSeriesElementProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      measurement_series_element {
        index: 144
        measurement_series_id: "12"
        value { number_value: 100219.0 }
        timestamp { seconds: 1000 nanos: 150000 }
        metadata {
          fields {
            key: "some"
            value { string_value: "JSON" }
          }
        }
      }
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact({.artifact = TestStepArtifact(
                          {.artifact = MeasurementSeriesElementOutput({
                               .index = 144,
                               .measurement_series_id = "12",
                               .value = 100219.,
                               .timestamp =
                                   {
                                       .tv_sec = 1000,
                                       .tv_usec = 150,
                                   },
                               .metadata_json = R"json({"some":"JSON"})json",
                           })})}));
}

TEST(ProtoToStructTest, MeasurementSeriesEndProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      measurement_series_end { measurement_series_id: "3" total_count: 51 }
    }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact({.artifact = TestStepArtifact(
                                {.artifact = MeasurementSeriesEndOutput({
                                     .measurement_series_id = "3",
                                     .total_count = 51,
                                 })})}));
}

TEST(ProtoToStructTest, DiagnosisProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      diagnosis {
        verdict: "mlc-intranode-bandwidth-pass"
        type: PASS
        message: "intranode bandwidth within threshold."
        hardware_info_id: "10"
        subcomponent {
          type: BUS
          name: "QPI1"
          location: "CPU-3-2-3"
          version: "1"
          revision: "0"
        }
      }
    }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact(
                {.artifact = TestStepArtifact(
                     {.artifact = DiagnosisOutput({
                          .verdict = "mlc-intranode-bandwidth-pass",
                          .type = DiagnosisType::kPass,
                          .message = "intranode bandwidth within threshold.",
                          .hardware_info_id = "10",
                          .subcomponent = Subcomponent({
                              .name = "QPI1",
                              .type = SubcomponentType::kBus,
                              .location = "CPU-3-2-3",
                              .version = "1",
                              .revision = "0",
                          }),
                      })})}));
}

TEST(ProtoToStructTest, FileProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      file {
        display_name: "mem_cfg_log"
        uri: "file:///root/mem_cfg_log"
        description: "DIMM configuration settings."
        content_type: "text/plain"
        is_snapshot: false
      }
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact({.artifact = TestStepArtifact(
                          {.artifact = FileOutput({
                               .display_name = "mem_cfg_log",
                               .uri = "file:///root/mem_cfg_log",
                               .is_snapshot = false,
                               .description = "DIMM configuration settings.",
                               .content_type = "text/plain",
                           })})}));
}

TEST(ProtoToStructTest, ExtensionProtoConvertsSuccessfully) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      extension {
        name: "Extension"
        content {
          fields {
            key: "some"
            value { string_value: "JSON" }
          }
        }
      }
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact({.artifact = TestStepArtifact(
                          {.artifact = ExtensionOutput({
                               .name = "Extension",
                               .content_json = R"json({"some":"JSON"})json",
                           })})}));
}

TEST(ProtoToStructTest, OutputArtifactFieldsAreSetProperlyDuringConversion) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    schema_version { major: 2 minor: 0 }
    sequence_number: 3
    timestamp { seconds: 101 nanos: 102000 }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact({.artifact = SchemaVersionOutput({.major = 2, .minor = 0}),
                      .sequence_number = 3,
                      .timestamp = {.tv_sec = 101, .tv_usec = 102}}));
}

TEST(ProtoToStructTest, TestStepFieldsAreSetProperlyDuringConversion) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      log { severity: ERROR message: "Fake error message" }
      test_step_id: "4"
    }
  )pb");
  EXPECT_EQ(ProtoToStruct(proto),
            OutputArtifact(
                {.artifact = TestStepArtifact(
                     {.artifact = LogOutput({.severity = LogSeverity::kError,
                                             .message = "Fake error message"}),
                      .test_step_id = "4"})}));
}

TEST(ProtoToStructTest, ErrorProtoConvertsSuccessfullyForTestStep) {
  ocpdiag_results_v2_pb::OutputArtifact proto = ParseTextProtoOrDie(R"pb(
    test_step_artifact {
      error {
        symptom: "internal-error"
        message: "fake"
        software_info_ids: "1"
        software_info_ids: "2"
      }
      test_step_id: "7"
    }
  )pb");
  EXPECT_EQ(
      ProtoToStruct(proto),
      OutputArtifact(
          {.artifact = TestStepArtifact(
               {.artifact = ErrorOutput({.symptom = "internal-error",
                                         .message = "fake",
                                         .software_info_ids = {"1", "2"}}),
                .test_step_id = "7"})}));
}

TEST(ProtoToStructDeathTest, EmptyOutputArtifactDies) {
  ocpdiag_results_v2_pb::OutputArtifact proto;
  EXPECT_DEATH(ProtoToStruct(proto), "empty or unexepected OutputArtifact");
}

TEST(ProtoToStructDeathTest, EmptyTestRunArtifactDies) {
  ocpdiag_results_v2_pb::OutputArtifact proto =
      ParseTextProtoOrDie(R"pb(test_run_artifact {})pb");
  EXPECT_DEATH(ProtoToStruct(proto), "empty or unexepected TestRunArtifact");
}

TEST(ProtoToStructDeathTest, EmptyTestStepArtifactDies) {
  ocpdiag_results_v2_pb::OutputArtifact proto =
      ParseTextProtoOrDie(R"pb(test_step_artifact {})pb");
  EXPECT_DEATH(ProtoToStruct(proto), "empty or unexepected TestStepArtifact");
}

TEST(ProtoToJsonOrDie, ValidProtoConvertsSuccessfully) {
  google::protobuf::Struct proto = ParseTextProtoOrDie(R"pb(
    fields {
      key: "data_collection_mode"
      value { bool_value: true }
    }
  )pb");

  EXPECT_EQ(ProtoToJsonOrDie(proto),
            R"json({"data_collection_mode":true})json");
}

}  // namespace ocpdiag::results::internal
