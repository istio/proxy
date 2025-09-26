// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/artifact_writer.h"

#include <stdlib.h>

#include <cstdlib>
#include <filesystem>  //
#include <thread>      //

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "ocpdiag/core/results/data_model/results.pb.h"
#include "ocpdiag/core/testing/file_utils.h"
#include "ocpdiag/core/testing/proto_matchers.h"
#include "ocpdiag/core/testing/status_matchers.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/records/record_reader.h"

namespace ocpdiag::results::internal {

using ::ocpdiag::testing::EqualsProto;
using ::ocpdiag::testing::IsOkAndHolds;
using ::ocpdiag::testing::Partially;
using ::testing::HasSubstr;

namespace {

constexpr int kWriterThreads = 15;
constexpr int kArtifactsPerWriter = 500;

std::string GetTempFilepath() {
  std::string filepath = testutils::MkTempFileOrDie("artifact_writer");
  if (std::filesystem::exists(filepath))
    CHECK(std::filesystem::remove(filepath)) << "Cannot remove temp file";
  return filepath;
}

absl::StatusOr<ocpdiag_results_v2_pb::OutputArtifact> ReadArtifact(
    absl::string_view filepath) {
  riegeli::RecordReader<riegeli::FdReader<>> reader(
      riegeli::FdReader<>{filepath});
  absl::Cleanup closer = [&reader] { reader.Close(); };

  ocpdiag_results_v2_pb::OutputArtifact proto;
  if (!reader.ReadRecord(proto))
    return absl::UnknownError("Couldn't read record file");
  return proto;
}

TEST(ArtifactWriterDeathTest, NoFilepathOrOutputStreamCausesDeath) {
  EXPECT_DEATH(ArtifactWriter(""), "specify a valid filepath or output stream");
}

TEST(ArtifactWriterDeathTest, InvalidFilepathDeath) {
  EXPECT_DEATH(ArtifactWriter("invalid/\\//\\filepath"), "File writer error");
}

TEST(ArtifactWriterTest, SchemaVersionWritesSuccessfully) {
  ocpdiag_results_v2_pb::SchemaVersion input_proto;
  input_proto.set_major(2);
  input_proto.set_minor(0);
  std::string tmp_filepath = GetTempFilepath();

  std::stringstream json_stream;
  {
    ArtifactWriter writer(tmp_filepath, &json_stream);
    writer.Write(input_proto);
  }

  absl::string_view expected_json =
      "\"schemaVersion\":{\"major\":2,\"minor\":0}";
  EXPECT_THAT(json_stream.str(), HasSubstr(expected_json));
  EXPECT_THAT(ReadArtifact(tmp_filepath),
              IsOkAndHolds(Partially(EqualsProto(R"pb(
                schema_version { major: 2 minor: 0 }
              )pb"))));
}

TEST(ArtifactWriterTest, TestRunArtifactWritesSuccessfully) {
  ocpdiag_results_v2_pb::TestRunArtifact input_proto;
  ocpdiag_results_v2_pb::Error* error_proto = input_proto.mutable_error();
  error_proto->set_symptom("fake-symptom");
  error_proto->set_message("Fake message");
  *error_proto->add_software_info_ids() = "1";
  std::string tmp_filepath = GetTempFilepath();

  std::stringstream json_stream;
  {
    ArtifactWriter writer(tmp_filepath, &json_stream);
    writer.Write(input_proto);
  }

  absl::string_view expected_json =
      "\"testRunArtifact\":{\"error\":{\"symptom\":\"fake-symptom\","
      "\"message\":\"Fake message\",\"softwareInfoIds\":[\"1\"]}}";
  EXPECT_THAT(json_stream.str(), HasSubstr(expected_json));
  EXPECT_THAT(ReadArtifact(tmp_filepath),
              IsOkAndHolds(Partially(EqualsProto(R"pb(
                test_run_artifact {
                  error {
                    symptom: "fake-symptom"
                    message: "Fake message"
                    software_info_ids: "1"
                  }
                }
              )pb"))));
}

TEST(ArtifactWriterTest, TestStepArtifactWritesSuccessfully) {
  ocpdiag_results_v2_pb::TestStepArtifact input_proto;
  input_proto.set_test_step_id("5");
  ocpdiag_results_v2_pb::Measurement* measurement_proto =
      input_proto.mutable_measurement();
  measurement_proto->set_name("fake name");
  measurement_proto->set_unit("fake unit");
  measurement_proto->set_hardware_info_id("1");
  google::protobuf::Value* value_proto = measurement_proto->mutable_value();
  value_proto->set_number_value(3.4);
  std::string tmp_filepath = GetTempFilepath();

  std::stringstream json_stream;
  {
    ArtifactWriter writer(tmp_filepath, &json_stream);
    writer.Write(input_proto);
  }

  absl::string_view expected_json =
      "\"testStepArtifact\":{\"measurement\":{\"name\":\"fake "
      "name\",\"unit\":\"fake "
      "unit\",\"hardwareInfoId\":\"1\",\"validators\":[],\"value\":3.4},"
      "\"testStepId\":\"5\"}";
  EXPECT_THAT(json_stream.str(), HasSubstr(expected_json));
  EXPECT_THAT(ReadArtifact(tmp_filepath),
              IsOkAndHolds(Partially(EqualsProto(R"pb(
                test_step_artifact {
                  measurement {
                    name: "fake name"
                    unit: "fake unit"
                    hardware_info_id: "1"
                    value { number_value: 3.4 }
                  }
                  test_step_id: "5"
                }
              )pb"))));
}

TEST(ArtifactWriterTest, SimultaneousWritesExecuteSuccessfully) {
  std::string tmp_filepath = GetTempFilepath();
  {
    ArtifactWriter writer(tmp_filepath);
    std::vector<std::thread> threads;
    for (int i = 0; i < kWriterThreads; i++) {
      threads.push_back(std::thread([&writer] {
        for (int i = 0; i < kArtifactsPerWriter; i++) {
          ocpdiag_results_v2_pb::SchemaVersion proto;
          writer.Write(proto);
        }
      }));
    }
    for (std::thread& thread : threads) thread.join();
  }

  riegeli::RecordReader<riegeli::FdReader<>> reader(
      riegeli::FdReader<>{tmp_filepath});
  absl::Cleanup closer = [&reader] { reader.Close(); };

  int got_count = 0;
  ocpdiag_results_v2_pb::OutputArtifact got;
  while (reader.ReadRecord(got)) {
    EXPECT_EQ(got_count, got.sequence_number());
    got_count++;
  }
  EXPECT_EQ(got_count, kWriterThreads * kArtifactsPerWriter);
}

}  // namespace

}  // namespace ocpdiag::results::internal
