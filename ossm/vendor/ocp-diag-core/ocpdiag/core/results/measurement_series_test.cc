// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/measurement_series.h"

#include <string>
#include <variant>

#include "gtest/gtest.h"
#include "absl/log/check.h"
#include "ocpdiag/core/results/data_model/dut_info.h"
#include "ocpdiag/core/results/data_model/input_model.h"
#include "ocpdiag/core/results/output_receiver.h"
#include "ocpdiag/core/results/test_run.h"
#include "ocpdiag/core/results/test_step.h"

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

TestStep MakeTestStep(TestRun& run) {
  StartTestRun(run);
  // This step exists to make the test step ID different from the measurement
  // series ID to improve testing.
  { TestStep("init", run); }
  return TestStep("fake_name", run);
}

MeasurementSeries MakeMeasurementSeries(TestStep& step) {
  return MeasurementSeries({.name = "awesome series"}, step);
}

MeasurementSeriesModel GetMeasurementSeriesModelIfValid(
    OutputReceiver& receiver) {
  OutputModel model = receiver.GetOutputModel();
  CHECK_EQ(model.test_steps.size(), 2);
  EXPECT_EQ(model.test_steps[1].test_step_id, "1");
  CHECK_EQ(model.test_steps[1].measurement_series.size(), 1);
  return model.test_steps[1].measurement_series[0];
}

TEST(MeasurementSeriesInitializationDeathTest,
     CreatingSeriesWithEndedStepCausesDeath) {
  OutputReceiver receiver;
  TestRun run = MakeTestRun(receiver);
  TestStep step = MakeTestStep(run);

  step.End();
  EXPECT_DEATH(MakeMeasurementSeries(step), "active TestSteps");
}

TEST(MeasurementSeriesInitializationDeathTest,
     InvalidMeasurementSeriesStartCausesDeath) {
  OutputReceiver receiver;
  TestRun run = MakeTestRun(receiver);
  TestStep step = MakeTestStep(run);

  EXPECT_DEATH(MeasurementSeries series({}, step), "measurement series start");
}

TEST(MeasurementSeriesInitializationDeathTest,
     AddingElementWithDifferentTypeThanValidatorsCausesDeath) {
  OutputReceiver receiver;
  TestRun run = MakeTestRun(receiver);
  TestStep step = MakeTestStep(run);
  MeasurementSeries series(
      {.name = "awesome series",
       .validators = {Validator{.type = ValidatorType::kEqual,
                                .value = {123.}}}},
      step);

  EXPECT_DEATH(series.AddElement({.value = "a string value"}), "same type");
}

class MeasurementSeriesTest : public ::testing::Test {
 protected:
  MeasurementSeriesTest()
      : run_(MakeTestRun(receiver_)),
        step_(MakeTestStep(run_)),
        series_(MakeMeasurementSeries(step_)) {}

  OutputReceiver receiver_;
  TestRun run_;
  TestStep step_;
  MeasurementSeries series_;
};

class MeasurementSeriesDeathTest : public MeasurementSeriesTest {};

TEST_F(MeasurementSeriesTest, MeasurementSeriesStartIsEmittedProperly) {
  MeasurementSeriesModel model = GetMeasurementSeriesModelIfValid(receiver_);
  EXPECT_EQ(model.start.name, "awesome series");
  EXPECT_EQ(model.start.measurement_series_id, "0");
}

TEST_F(MeasurementSeriesTest, MeasurementSeriesElementIsEmittedProperly) {
  series_.AddElement({
      .value = 123.,
      .timestamp = timeval{.tv_sec = 100, .tv_usec = 150},
  });
  run_.GetArtifactWriter().Flush();

  MeasurementSeriesModel model = GetMeasurementSeriesModelIfValid(receiver_);
  ASSERT_EQ(model.elements.size(), 1);
  ASSERT_TRUE(std::holds_alternative<double>(model.elements[0].value));
  EXPECT_EQ(std::get<double>(model.elements[0].value), 123.);
  EXPECT_EQ(model.elements[0].timestamp.tv_sec, 100);
  EXPECT_EQ(model.elements[0].timestamp.tv_usec, 150);
  EXPECT_EQ(model.elements[0].index, 0);
  EXPECT_EQ(model.elements[0].measurement_series_id, "0");
}

TEST_F(MeasurementSeriesTest, TimestampIsAssignedToElementWhenNoneIsProvided) {
  series_.AddElement({.value = 123.});
  run_.GetArtifactWriter().Flush();

  MeasurementSeriesModel model = GetMeasurementSeriesModelIfValid(receiver_);
  ASSERT_EQ(model.elements.size(), 1);
  EXPECT_NE(model.elements[0].timestamp.tv_sec, 0);
}

TEST_F(MeasurementSeriesTest, ElementIndexAndIncrementsProperly) {
  int element_count = 5;
  MeasurementSeriesElement element = {.value = 123.};

  for (int i = 0; i < element_count; ++i) series_.AddElement(element);
  run_.GetArtifactWriter().Flush();

  MeasurementSeriesModel model = GetMeasurementSeriesModelIfValid(receiver_);
  ASSERT_EQ(model.elements.size(), element_count);
  for (int i = 0; i < element_count; ++i) EXPECT_EQ(model.elements[i].index, i);
}

TEST_F(MeasurementSeriesDeathTest, AddingDifferentTypeElementsCausesDeath) {
  series_.AddElement({.value = "a string value"});
  EXPECT_DEATH(series_.AddElement({.value = 123.}), "same type");
}

TEST_F(MeasurementSeriesDeathTest,
       AddingElementAfterSeriesHadEndedCausesDeath) {
  series_.End();
  EXPECT_DEATH(series_.AddElement({.value = 123.}),
               "MeasurementSeries that has ended");
}

TEST_F(MeasurementSeriesDeathTest, AddingElementAfterStepHadEndedCausesDeath) {
  step_.End();
  EXPECT_DEATH(series_.AddElement({.value = 123.}), "TestStep that has ended");
}

TEST_F(MeasurementSeriesTest, MeasurementSeriesEndIsEmittedProperly) {
  MeasurementSeriesElement element = {.value = 123.};
  int expected_count = 5;
  for (int i = 0; i < expected_count; ++i) series_.AddElement(element);

  EXPECT_FALSE(series_.Ended());
  series_.End();
  EXPECT_TRUE(series_.Ended());

  MeasurementSeriesModel model = GetMeasurementSeriesModelIfValid(receiver_);
  EXPECT_EQ(model.end.total_count, expected_count);
  EXPECT_EQ(model.end.measurement_series_id, "0");
}

TEST_F(MeasurementSeriesTest, OnlyOneEndArtifactIsEmittedIfEndedIsCalledTwice) {
  series_.End();
  series_.End();

  int artifact_count = 0;
  for (auto unused : receiver_.GetOutputContainer()) artifact_count++;

  // We expect schema version, test run start, test step start and end for the
  // dummy test step, the main test step start, and the measurement series start
  // and end for a total of 7 artifacts.
  EXPECT_EQ(artifact_count, 7);
}

}  // namespace

}  // namespace ocpdiag::results
