// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/output_receiver.h"

#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/output_model.h"
#include "ocpdiag/core/results/data_model/proto_to_struct.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/testing/parse_text_proto.h"

namespace ocpdiag::results {

using ::ocpdiag::results::internal::ProtoToStruct;
using ::ocpdiag::testing::ParseTextProtoOrDie;

namespace {

ocpdiag_results_v2_pb::SchemaVersion GetExampleSchemaVersion() {
  return ParseTextProtoOrDie(R"pb(major: 2 minor: 0)pb");
}

TEST(OutputReceiverDeathTest, CreatingMultipleArtifactWritersCausesDeath) {
  OutputReceiver receiver;
  receiver.MakeArtifactWriter();
  EXPECT_DEATH(receiver.MakeArtifactWriter(),
               "Attempted to create an Artifact Writer");
}

TEST(OutputReceiverDeathTest,
     AccessingDataBeforeCreatingArtifactWriterCasuesDeath) {
  OutputReceiver receiver;
  EXPECT_DEATH(receiver.GetOutputContainer(),
               "Attempted to access receiver contents");
  EXPECT_DEATH(receiver.GetOutputModel(),
               "Attempted to access receiver contents");
}

TEST(OutputReceiverTest, RebuildingModelAddsWrittenArtifacts) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::SchemaVersion artifact = GetExampleSchemaVersion();
  SchemaVersionOutput schema_version = ProtoToStruct(artifact);
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();

  EXPECT_NE(receiver.GetOutputModel().schema_version, schema_version);
  writer->Write(artifact);
  writer->Flush();
  EXPECT_NE(receiver.GetOutputModel().schema_version, schema_version);
  receiver.ResetModel();
  EXPECT_EQ(receiver.GetOutputModel().schema_version, schema_version);
}

TEST(OutputReceiverTest, OutputContainerIteratesProperly) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::SchemaVersion first_artifact =
      GetExampleSchemaVersion();
  ocpdiag_results_v2_pb::TestRunArtifact second_artifact = ParseTextProtoOrDie(
      R"pb(test_run_end { status: COMPLETE result: PASS })pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(first_artifact);
  writer->Write(second_artifact);
  writer->Flush();

  std::vector<OutputArtifact> output_artifacts;
  for (const OutputArtifact& output_artifact : receiver.GetOutputContainer())
    output_artifacts.push_back(output_artifact);

  auto* schema_version =
      std::get_if<SchemaVersionOutput>(&output_artifacts[0].artifact);
  ASSERT_NE(schema_version, nullptr);
  EXPECT_EQ(*schema_version, ProtoToStruct(first_artifact));

  auto* test_run_artifact =
      std::get_if<TestRunArtifact>(&output_artifacts[1].artifact);
  ASSERT_NE(test_run_artifact, nullptr);
  auto* test_run_end =
      std::get_if<TestRunEndOutput>(&test_run_artifact->artifact);
  ASSERT_NE(test_run_end, nullptr);
  EXPECT_EQ(*test_run_end, ProtoToStruct(second_artifact.test_run_end()));
}

TEST(OutputReceiverTest, SchemaVersionAppearsInModel) {
  OutputReceiver receiver;
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(GetExampleSchemaVersion());
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(GetExampleSchemaVersion()),
            receiver.GetOutputModel().schema_version);
}

TEST(OutputReceiverTest, TestRunStartAppearsInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestRunArtifact artifact = ParseTextProtoOrDie(
      R"pb(
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
          }
        }
      )pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.test_run_start()),
            receiver.GetOutputModel().test_run.start);
}

TEST(OutputReceiverTest, TestRunEndAppearsInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestRunArtifact artifact = ParseTextProtoOrDie(
      R"pb(test_run_end { status: COMPLETE result: PASS })pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.test_run_end()),
            receiver.GetOutputModel().test_run.end);
}

TEST(OutputReceiverTest, TestRunLogsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestRunArtifact artifact = ParseTextProtoOrDie(
      R"pb(log {
             severity: ERROR
             message: "file operation not completed successfully."
           })pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.log()),
            receiver.GetOutputModel().test_run.pre_start_logs[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_run.pre_start_logs.size(), 2);
}

TEST(OutputReceiverTest, TestRunErrorsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestRunArtifact artifact = ParseTextProtoOrDie(
      R"pb(error {
             symptom: "bad-return-code"
             message: "software exited abnormally."
             software_info_ids: "1"
             software_info_ids: "2"
           })pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.error()),
            receiver.GetOutputModel().test_run.pre_start_errors[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_run.pre_start_errors.size(), 2);
}

TEST(OutputReceiverTest, TestStepStartAppearsInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact =
      ParseTextProtoOrDie(R"pb(test_step_start { name: "my step" }
                               test_step_id: "5")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.test_step_start()),
            receiver.GetOutputModel().test_steps[0].start);
}

TEST(OutputReceiverTest, TestStepEndAppearsInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact =
      ParseTextProtoOrDie(R"pb(test_step_end { status: ERROR }
                               test_step_id: "5")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.test_step_end()),
            receiver.GetOutputModel().test_steps[0].end);
}

TEST(OutputReceiverTest, MeasurementsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact =
      ParseTextProtoOrDie(R"pb(measurement {
                                 name: "measured-fan-speed-100"
                                 unit: "RPM"
                                 hardware_info_id: "5"
                                 value { string_value: "My fan name" }
                               }
                               test_step_id: "5")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.measurement()),
            receiver.GetOutputModel().test_steps[0].measurements[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].measurements.size(), 2);
}

TEST(OutputReceiverTest, MeasurementSeriesStartsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact first_artifact =
      ParseTextProtoOrDie(R"pb(measurement_series_start {
                                 measurement_series_id: "13"
                                 name: "measured-fan-speed-100"
                                 unit: "RPM"
                                 hardware_info_id: "5"
                               }
                               test_step_id: "5")pb");
  ocpdiag_results_v2_pb::TestStepArtifact second_artifact =
      ParseTextProtoOrDie(R"pb(measurement_series_start {
                                 measurement_series_id: "5"
                                 name: "measured-fan-speed-2"
                                 unit: "RPM"
                                 hardware_info_id: "3"
                               }
                               test_step_id: "5")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(first_artifact);
  writer->Write(second_artifact);
  writer->Flush();

  EXPECT_EQ(
      ProtoToStruct(first_artifact.measurement_series_start()),
      receiver.GetOutputModel().test_steps[0].measurement_series[0].start);
  EXPECT_EQ(
      ProtoToStruct(second_artifact.measurement_series_start()),
      receiver.GetOutputModel().test_steps[0].measurement_series[1].start);
}

TEST(OutputReceiverTest, MeasurementSeriesElementsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact first_artifact =
      ParseTextProtoOrDie(R"pb(measurement_series_element {
                                 index: 1
                                 measurement_series_id: "12"
                                 value { number_value: 100219.0 }
                                 timestamp { seconds: 1000 nanos: 150000 }
                               }
                               test_step_id: "5")pb");
  ocpdiag_results_v2_pb::TestStepArtifact second_artifact =
      ParseTextProtoOrDie(R"pb(measurement_series_element {
                                 index: 2
                                 measurement_series_id: "12"
                                 value { number_value: 100214.0 }
                                 timestamp { seconds: 1001 nanos: 153000 }
                               }
                               test_step_id: "5")pb");
  ocpdiag_results_v2_pb::TestStepArtifact third_artifact =
      ParseTextProtoOrDie(R"pb(measurement_series_element {
                                 index: 1
                                 measurement_series_id: "13"
                                 value { number_value: 100214.0 }
                                 timestamp { seconds: 1001 nanos: 156000 }
                               }
                               test_step_id: "5")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(first_artifact);
  writer->Write(second_artifact);
  writer->Write(third_artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(first_artifact.measurement_series_element()),
            receiver.GetOutputModel()
                .test_steps[0]
                .measurement_series[0]
                .elements[0]);
  EXPECT_EQ(ProtoToStruct(second_artifact.measurement_series_element()),
            receiver.GetOutputModel()
                .test_steps[0]
                .measurement_series[0]
                .elements[1]);
  EXPECT_EQ(ProtoToStruct(third_artifact.measurement_series_element()),
            receiver.GetOutputModel()
                .test_steps[0]
                .measurement_series[1]
                .elements[0]);
}

TEST(OutputReceiverTest, MeasurementSeriesEndsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact first_artifact = ParseTextProtoOrDie(
      R"pb(measurement_series_end { measurement_series_id: "3" total_count: 3 }
           test_step_id: "5")pb");
  ocpdiag_results_v2_pb::TestStepArtifact second_artifact = ParseTextProtoOrDie(
      R"pb(measurement_series_end { measurement_series_id: "4" total_count: 10 }
           test_step_id: "6")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(first_artifact);
  writer->Write(second_artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(first_artifact.measurement_series_end()),
            receiver.GetOutputModel().test_steps[0].measurement_series[0].end);
  EXPECT_EQ(ProtoToStruct(second_artifact.measurement_series_end()),
            receiver.GetOutputModel().test_steps[1].measurement_series[0].end);
}

TEST(OutputReceiverTest, DiagnosesAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact = ParseTextProtoOrDie(
      R"pb(diagnosis {
             verdict: "mlc-intranode-bandwidth-pass"
             type: PASS
             message: "intranode bandwidth within threshold."
             hardware_info_id: "10"
           }
           test_step_id: "1")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.diagnosis()),
            receiver.GetOutputModel().test_steps[0].diagnoses[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].diagnoses.size(), 2);
}

TEST(OutputReceiverTest, FilesAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact = ParseTextProtoOrDie(
      R"pb(file {
             display_name: "mem_cfg_log"
             uri: "file:///root/mem_cfg_log"
             description: "DIMM configuration settings."
             content_type: "text/plain"
             is_snapshot: false
           }
           test_step_id: "1")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.file()),
            receiver.GetOutputModel().test_steps[0].files[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].files.size(), 2);
}

TEST(OutputReceiverTest, ExtensionsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact = ParseTextProtoOrDie(
      R"pb(extension {
             name: "Extension"
             content {
               fields {
                 key: "some"
                 value { string_value: "JSON" }
               }
             }
           }
           test_step_id: "1")pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.extension()),
            receiver.GetOutputModel().test_steps[0].extensions[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].extensions.size(), 2);
}

TEST(OutputReceiverTest, TestStepLogsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact = ParseTextProtoOrDie(
      R"pb(log {
             severity: ERROR
             message: "file operation not completed successfully."
           })pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.log()),
            receiver.GetOutputModel().test_steps[0].logs[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].logs.size(), 2);
}

TEST(OutputReceiverTest, TestStepErrorsAppearInModel) {
  OutputReceiver receiver;
  ocpdiag_results_v2_pb::TestStepArtifact artifact = ParseTextProtoOrDie(
      R"pb(error {
             symptom: "bad-return-code"
             message: "software exited abnormally."
             software_info_ids: "1"
             software_info_ids: "2"
           })pb");
  std::unique_ptr<internal::ArtifactWriter> writer =
      receiver.MakeArtifactWriter();
  writer->Write(artifact);
  writer->Write(artifact);
  writer->Flush();

  EXPECT_EQ(ProtoToStruct(artifact.error()),
            receiver.GetOutputModel().test_steps[0].errors[0]);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].errors.size(), 2);
}

}  // namespace
}  // namespace ocpdiag::results
