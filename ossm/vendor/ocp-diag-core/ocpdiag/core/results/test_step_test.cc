// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/test_step.h"

#include <memory>
#include <string>

#include "gtest/gtest.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/output_receiver.h"
#include "ocpdiag/core/results/test_run.h"

namespace ocpdiag::results {

namespace {

TestRun MakeTestRun(OutputReceiver& receiver) {
  return TestRun(
      {
          .name = "mlc_test",
          .version = "1.0",
          .command_line = "mlc/mlc --use_default_thresholds=true",
          .parameters_json = R"json({"max_bandwidth": 7200.0})json",
      },
      receiver.MakeArtifactWriter());
}

void StartTestRun(TestRun& test_run) {
  test_run.StartAndRegisterDutInfo(std::make_unique<DutInfo>("dut", "id"));
}

TEST(TestStepInitializationDeathTest, CreatingStepFromInactiveRunCausesDeath) {
  OutputReceiver receiver;
  TestRun run = MakeTestRun(receiver);
  EXPECT_DEATH(TestStep step("name", run), "after the test run has started");
}

TEST(TestStepInitializationDeathTest, EmptyStepNameCausesDeath) {
  OutputReceiver receiver;
  TestRun run = MakeTestRun(receiver);
  StartTestRun(run);
  EXPECT_DEATH(TestStep step("", run), "names cannot be empty");
}

class TestStepTest : public ::testing::Test {
 protected:
  TestStepTest()
      : run_(MakeTestRun(receiver_)), step_([&] {
          StartTestRun(run_);
          return TestStep("fake_name", run_);
        }()) {}

  OutputReceiver receiver_;
  TestRun run_;
  TestStep step_;
};

class TestStepDeathTest : public TestStepTest {};

TEST_F(TestStepTest, TestStepStartIsEmittedProperly) {
  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  EXPECT_EQ(receiver_.GetOutputModel().test_steps[0].start.name, step_.Name());
}

TEST_F(TestStepTest, MeasurementIsEmittedProperly) {
  Measurement measurement = {.name = "Fake measurement", .value = 132.};
  step_.AddMeasurement(measurement);
  run_.GetArtifactWriter().Flush();

  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  TestStepModel model = receiver_.GetOutputModel().test_steps[0];
  EXPECT_EQ(model.test_step_id, step_.Id());
  EXPECT_EQ(model.measurements.size(), 1);
  EXPECT_EQ(model.measurements[0].name, measurement.name);
  EXPECT_EQ(model.measurements[0].value, measurement.value);
}

TEST_F(TestStepDeathTest, AddingInvalidMeasurementCausesDeath) {
  EXPECT_DEATH(step_.AddMeasurement({.value = 100.}), "");
}

TEST_F(TestStepTest, DiagnosisIsEmittedProperly) {
  Diagnosis diagnosis = {.verdict = "fake-verdict",
                         .type = DiagnosisType::kPass};
  step_.AddDiagnosis(diagnosis);
  run_.GetArtifactWriter().Flush();

  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  TestStepModel model = receiver_.GetOutputModel().test_steps[0];
  EXPECT_EQ(model.test_step_id, step_.Id());
  EXPECT_EQ(model.diagnoses.size(), 1);
  EXPECT_EQ(model.diagnoses[0].verdict, diagnosis.verdict);
  EXPECT_EQ(model.diagnoses[0].type, diagnosis.type);
}

TEST_F(TestStepTest, AddingFailDiagnosisCausesTestRunFailure) {
  step_.AddDiagnosis({.verdict = "fake-verdict", .type = DiagnosisType::kFail});
  EXPECT_EQ(run_.Result(), TestResult::kFail);
}

TEST_F(TestStepDeathTest, AddingInvalidDiagnosisCausesDeath) {
  EXPECT_DEATH(step_.AddDiagnosis({}), "");
}

TEST_F(TestStepTest, ErrorUpdatesStatusesAndEmitsProperly) {
  Error error = {.symptom = "fake-symptom"};
  step_.AddError(error);
  run_.GetArtifactWriter().Flush();

  EXPECT_EQ(step_.Status(), TestStatus::kError);
  EXPECT_EQ(run_.Status(), TestStatus::kError);

  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  TestStepModel model = receiver_.GetOutputModel().test_steps[0];
  EXPECT_EQ(model.test_step_id, step_.Id());
  EXPECT_EQ(model.errors.size(), 1);
  EXPECT_EQ(model.errors[0].symptom, error.symptom);
}

TEST_F(TestStepDeathTest, AddingInvalidErrorResultsInDeath) {
  EXPECT_DEATH(step_.AddError({}), "");
}

TEST_F(TestStepTest, FileEmitsProperly) {
  File file = {.display_name = "fake-file", .uri = "file:///dev/null"};
  step_.AddFile(file);
  run_.GetArtifactWriter().Flush();

  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  TestStepModel model = receiver_.GetOutputModel().test_steps[0];
  EXPECT_EQ(model.test_step_id, step_.Id());
  EXPECT_EQ(model.files.size(), 1);
  EXPECT_EQ(model.files[0], file);
}

TEST_F(TestStepDeathTest, AddingInvalidFileCausesDeath) {
  EXPECT_DEATH(step_.AddFile({}), "");
}

TEST_F(TestStepTest, LogEmitsProperly) {
  Log log = {.message = "fake message"};
  step_.AddLog(log);
  run_.GetArtifactWriter().Flush();

  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  TestStepModel model = receiver_.GetOutputModel().test_steps[0];
  EXPECT_EQ(model.test_step_id, step_.Id());
  EXPECT_EQ(model.logs.size(), 1);
  EXPECT_EQ(model.logs[0], log);
}

TEST_F(TestStepDeathTest, AddingInvalidLogCausesDeath) {
  EXPECT_DEATH(step_.AddLog({}), "");
}

TEST_F(TestStepDeathTest, AddingFatalLogCausesDeath) {
  std::string message = "fake fatal message";
  EXPECT_DEATH(
      step_.AddLog({.severity = LogSeverity::kFatal, .message = message}),
      message);
}

TEST_F(TestStepTest, ExtensionEmitsProperly) {
  Extension extension = {.name = "fake-extension",
                         .content_json = R"json({"some":"json"})json"};
  step_.AddExtension(extension);
  run_.GetArtifactWriter().Flush();

  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  TestStepModel model = receiver_.GetOutputModel().test_steps[0];
  EXPECT_EQ(model.test_step_id, step_.Id());
  EXPECT_EQ(model.extensions.size(), 1);
  EXPECT_EQ(model.extensions[0], extension);
}

TEST_F(TestStepDeathTest, AddingInvalidExtensionCausesDeath) {
  EXPECT_DEATH(step_.AddExtension({}), "");
}

TEST_F(TestStepTest, SkippingStepUpdatesStatusAndEndsStep) {
  step_.Skip();
  EXPECT_EQ(step_.Status(), TestStatus::kSkip);
  EXPECT_TRUE(step_.Ended());
}

TEST_F(TestStepTest, SkippingStepDoesNotOverrideExistingStatus) {
  step_.AddError({.symptom = "fake-symptom"});
  step_.Skip();
  EXPECT_EQ(step_.Status(), TestStatus::kError);
}

TEST_F(TestStepTest, EndEmitsTestStepEndProperly) {
  EXPECT_FALSE(step_.Ended());
  step_.End();
  EXPECT_TRUE(step_.Ended());
  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  EXPECT_EQ(receiver_.GetOutputModel().test_steps[0].end.status,
            TestStatus::kComplete);
}

TEST_F(TestStepTest, EndDoesNotOverrideExistingStatus) {
  step_.AddError({.symptom = "fake-symptom"});
  step_.End();
  EXPECT_EQ(receiver_.GetOutputModel().test_steps.size(), 1);
  EXPECT_EQ(receiver_.GetOutputModel().test_steps[0].end.status,
            TestStatus::kError);
}

TEST_F(TestStepDeathTest, AddingArtifactsAfterEndingStepCausesDeath) {
  step_.End();
  EXPECT_DEATH(
      step_.AddMeasurement({.name = "Fake measurement", .value = 132.}), "");
  EXPECT_DEATH(step_.AddDiagnosis(
                   {.verdict = "fake-verdict", .type = DiagnosisType::kPass}),
               "");
  EXPECT_DEATH(step_.AddError({.symptom = "fake-symptom"}), "");
  EXPECT_DEATH(
      step_.AddFile({.display_name = "fake", .uri = "file:///dev/null"}), "");
  EXPECT_DEATH(step_.AddLog({.message = "fake message"}), "");
  EXPECT_DEATH(
      step_.AddExtension({.name = "fake-extension",
                          .content_json = R"json({"some":"json"})json"}),
      "");
}

TEST_F(TestStepTest, CallingEndMultipleTimesEmitsOneEndArtifact) {
  step_.End();
  step_.End();

  int count = 0;
  for (auto unused : receiver_.GetOutputContainer()) count++;

  // We expect schema version, test run start, and test step start and
  // end for a total of 4 artifacts
  EXPECT_EQ(count, 4);
}

TEST_F(TestStepTest, TestRunCanBeRetrieved) {
  EXPECT_EQ(&run_, &step_.GetTestRun());
}

TEST(TestStepDestructionTest, DestructorEmitsTestStepEndProperly) {
  OutputReceiver receiver;
  TestRun run = MakeTestRun(receiver);
  StartTestRun(run);
  { TestStep step("name", run); }

  EXPECT_EQ(receiver.GetOutputModel().test_steps.size(), 1);
  EXPECT_EQ(receiver.GetOutputModel().test_steps[0].end.status,
            TestStatus::kComplete);
}

}  // namespace

}  // namespace ocpdiag::results
