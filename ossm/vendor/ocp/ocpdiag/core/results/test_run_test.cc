// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/test_run.h"

#include <memory>

#include "gtest/gtest.h"
#include "ocpdiag/core/results/artifact_writer.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/output_receiver.h"

namespace ocpdiag::results {

namespace {

TestRunStart GetExampleTestRunStart() {
  return {
      .name = "mlc_test",
      .version = "1.0",
      .command_line = "mlc/mlc --use_default_thresholds=true",
      .parameters_json = R"json({"max_bandwidth": 7200.0})json",
  };
}

TEST(TestRunDeathTest, InvalidTestRunStartCausesDeath) {
  EXPECT_DEATH(TestRun invalid_start({}), "Must specify the name");
}

TEST(TestRunTest, TestRunStartAndEndAreEmittedOnDestruction) {
  OutputReceiver receiver;
  TestRunStart start = GetExampleTestRunStart();
  { TestRun test_run(start, receiver.MakeArtifactWriter()); }

  EXPECT_EQ(receiver.GetOutputModel().schema_version,
            SchemaVersionOutput(
                {.major = kMajorSchemaVersion, .minor = kMinorSchemaVersion}));
  EXPECT_EQ(receiver.GetOutputModel().test_run.start.command_line,
            start.command_line);
  EXPECT_EQ(receiver.GetOutputModel().test_run.end,
            TestRunEndOutput({
                .status = TestStatus::kSkip,
                .result = TestResult::kNotApplicable,
            }));
}

TEST(TestRunTest, SequentialInitilizationSucceeds) {
  { TestRun first_test_run(GetExampleTestRunStart()); }
  TestRun second_test_run(GetExampleTestRunStart());
}

TEST(TestRunDeathTest, InitializingSecondTestRunCausesDeath) {
  TestRunStart start = GetExampleTestRunStart();
  TestRun first_test_run(start);
  EXPECT_DEATH(TestRun second_test_run(start), "Only one TestRun");
}

TEST(TestRunTest, AddingErrorBeforeStartSucceeds) {
  OutputReceiver receiver;
  Error error = {.symptom = "really-bad-error",
                 .message = "A really bad error happened - like REALLY bad"};
  {
    TestRun test_run(GetExampleTestRunStart(), receiver.MakeArtifactWriter());
    test_run.AddPreStartError(error);
  }

  TestRunModel model = receiver.GetOutputModel().test_run;
  EXPECT_EQ(model.pre_start_errors.size(), 1);
  EXPECT_EQ(model.pre_start_errors[0].symptom, error.symptom);
  EXPECT_EQ(model.pre_start_errors[0].message, error.message);
  EXPECT_EQ(model.end.status, TestStatus::kError);
  EXPECT_EQ(model.end.result, TestResult::kNotApplicable);
}

TEST(TestRunDeathTest, AddingInvalidErrorResultsInDeath) {
  TestRun test_run(GetExampleTestRunStart());
  EXPECT_DEATH(test_run.AddPreStartError({}), "Must specify the symptom");
}

TEST(TestRunDeathTest, AddingErrorAfterStartCausesDeath) {
  TestRun test_run(GetExampleTestRunStart());
  test_run.StartAndRegisterDutInfo(std::make_unique<DutInfo>("dut", "id"));
  EXPECT_DEATH(test_run.AddPreStartError({.symptom = "valid-error-symptom"}),
               "Errors can only be added");
}

TEST(TestRunTest, AddingLogBeforeStartSucceeds) {
  OutputReceiver receiver;
  Log log = {.severity = LogSeverity::kWarning,
             .message = "This is a warning, man"};
  {
    TestRun test_run(GetExampleTestRunStart(), receiver.MakeArtifactWriter());
    test_run.AddPreStartLog(log);
  }

  TestRunModel model = receiver.GetOutputModel().test_run;
  EXPECT_EQ(model.pre_start_logs.size(), 1);
  EXPECT_EQ(model.pre_start_logs[0], log);
  EXPECT_EQ(model.end.status, TestStatus::kSkip);
  EXPECT_EQ(model.end.result, TestResult::kNotApplicable);
}

TEST(TestRunDeathTest, AddingInvalidLogCausesDeath) {
  TestRun test_run(GetExampleTestRunStart());
  EXPECT_DEATH(test_run.AddPreStartLog({}), "Must specify the message");
}

TEST(TestRunDeathTest, AddingLogAfterStartCausesDeath) {
  TestRun test_run(GetExampleTestRunStart());
  test_run.StartAndRegisterDutInfo(std::make_unique<DutInfo>("dut", "id"));
  EXPECT_DEATH(test_run.AddPreStartLog({.message = "regular old info message"}),
               "Logs can only be added");
}

TEST(TestRunDeathTest, AddingFatalLogCasuesDeath) {
  TestRun test_run(GetExampleTestRunStart());
  Log log = {.severity = LogSeverity::kFatal,
             .message = "Something super bad happened"};
  EXPECT_DEATH(test_run.AddPreStartLog(log), log.message);
}

TEST(TestRunTest, DutInfoIsEmittedAndStatusIsSetOnStart) {
  auto dut_info = std::make_unique<DutInfo>("dut", "id");
  PlatformInfo platform_info = {.info = "Some pretty cool info about that DUT"};
  dut_info->AddPlatformInfo(platform_info);

  OutputReceiver receiver;

  {
    TestRunStart start_input = GetExampleTestRunStart();
    TestRun test_run(start_input, receiver.MakeArtifactWriter());

    EXPECT_FALSE(test_run.Started());
    test_run.StartAndRegisterDutInfo(std::move(dut_info));
    EXPECT_TRUE(test_run.Started());

    test_run.GetArtifactWriter().Flush();
    TestRunStartOutput start_output = receiver.GetOutputModel().test_run.start;
    EXPECT_EQ(start_output.name, start_input.name);
    EXPECT_EQ(start_output.dut_info.dut_info_id, "id");
    EXPECT_EQ(start_output.dut_info.platform_infos.size(), 1);
    EXPECT_EQ(start_output.dut_info.platform_infos[0], platform_info);
  }

  receiver.ResetModel();
  TestRunEndOutput end_output = receiver.GetOutputModel().test_run.end;
  EXPECT_EQ(end_output.result, TestResult::kPass);
  EXPECT_EQ(end_output.status, TestStatus::kComplete);
}

TEST(TestRunDeathTest, RegisteringDutInfoAsNullptrCausesDeath) {
  TestRun test_run(GetExampleTestRunStart());
  EXPECT_DEATH(test_run.StartAndRegisterDutInfo(nullptr),
               "DutInfo must be provided");
}

TEST(TestRunTest, SkippingTestRunPropagatesToOutput) {
  OutputReceiver receiver;
  {
    TestRun test_run(GetExampleTestRunStart(), receiver.MakeArtifactWriter());
    test_run.StartAndRegisterDutInfo(std::make_unique<DutInfo>("dut", "id"));
    test_run.Skip();
    EXPECT_EQ(test_run.Status(), TestStatus::kSkip);
  }
  EXPECT_EQ(receiver.GetOutputModel().test_run.end.status, TestStatus::kSkip);
}

TEST(TestRunTest, IdsIncrementProperly) {
  TestRun test_run(GetExampleTestRunStart());
  EXPECT_EQ(test_run.GetNextStepId(), "0");
  EXPECT_EQ(test_run.GetNextMeasurementSeriesId(), "0");
  EXPECT_EQ(test_run.GetNextStepId(), "1");
  EXPECT_EQ(test_run.GetNextMeasurementSeriesId(), "1");
}

TEST(TestRunTest, ResultCalculatorOutputIsPropagatedProperly) {
  TestRun test_run(GetExampleTestRunStart());
  EXPECT_EQ(test_run.Result(), test_run.GetResultCalculator().result());
  EXPECT_EQ(test_run.Status(), test_run.GetResultCalculator().status());
}

}  // namespace

}  // namespace ocpdiag::results
