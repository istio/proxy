// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/struct_validators.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ocpdiag/core/results/data_model/input_model.h"

namespace ocpdiag::results {

using ::testing::Values;

class ValidValidatorStructTest : public ::testing::TestWithParam<Validator> {};

TEST_P(ValidValidatorStructTest, ValidValidatorsPass) {
  ValidateStructOrDie(GetParam());
}

INSTANTIATE_TEST_SUITE_P(ValidValidatorsPass, ValidValidatorStructTest,
                         Values(Validator({
                                    .type = ValidatorType::kEqual,
                                    .value = {100.},
                                }),
                                Validator({
                                    .type = ValidatorType::kLessThanOrEqual,
                                    .value = {10.5},
                                }),
                                Validator({
                                    .type = ValidatorType::kRegexNoMatch,
                                    .value = {"made", "up", "strings"},
                                }),
                                Validator({
                                    .type = ValidatorType::kNotInSet,
                                    .value = {"Bad", "values"},
                                })));

struct InvalidValidatorTestCase {
  Validator validator;
  std::string want_error_regex;
};

class InvalidValidatorStructDeathTest
    : public ::testing::TestWithParam<InvalidValidatorTestCase> {};

TEST_P(InvalidValidatorStructDeathTest, InvalidValidatorsFail) {
  InvalidValidatorTestCase t = GetParam();
  EXPECT_DEATH(ValidateStructOrDie(t.validator), t.want_error_regex);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidValidatorsFail, InvalidValidatorStructDeathTest,
    Values(
        InvalidValidatorTestCase({.validator =
                                      {
                                          .type = ValidatorType::kUnspecified,
                                          .value = {100.},
                                      },
                                  .want_error_regex = "Must specify type"}),
        InvalidValidatorTestCase({.validator =
                                      {
                                          .type = ValidatorType::kEqual,
                                      },
                                  .want_error_regex = "At least one value"}),
        InvalidValidatorTestCase(
            {.validator =
                 {
                     .type = ValidatorType::kEqual,
                     .value = {100., "str"},
                 },
             .want_error_regex = "All values must be of the same type"}),
        InvalidValidatorTestCase(
            {.validator =
                 {
                     .type = ValidatorType::kEqual,
                     .value = {100., 200.},
                 },
             .want_error_regex = "Must specify exactly one value for EQUAL"}),
        InvalidValidatorTestCase(
            {.validator =
                 {
                     .type = ValidatorType::kGreaterThan,
                     .value = {100., 200.},
                 },
             .want_error_regex = "Must specify exactly one value for numerical "
                                 "comparison type"}),
        InvalidValidatorTestCase(
            {.validator =
                 {
                     .type = ValidatorType::kGreaterThan,
                     .value = {"str"},
                 },
             .want_error_regex =
                 "Value must be numerical for numerical comparison"}),
        InvalidValidatorTestCase(
            {.validator =
                 {
                     .type = ValidatorType::kRegexMatch,
                     .value = {100., 200.},
                 },
             .want_error_regex =
                 "Value must be a string or string collection for REGEX"}),
        InvalidValidatorTestCase(
            {.validator =
                 {
                     .type = ValidatorType::kInSet,
                     .value = {true, false},
                 },
             .want_error_regex =
                 "Value must be a string or numerical type for set"})));

TEST(ValidateHardwareInfoTest, ValidHardwareInfoPasses) {
  ValidateStructOrDie(HardwareInfo({.name = "Test name"}));
}

TEST(ValidateHardwareInfoDeathTest, NoNameHardwareInfoFails) {
  EXPECT_DEATH(ValidateStructOrDie(HardwareInfo()), "hardware info");
}

TEST(ValidateSoftwareInfoTest, ValidSoftwareInfoPasses) {
  ValidateStructOrDie(SoftwareInfo({.name = "Test name"}));
}

TEST(ValidateSoftwareInfoDeathTest, NoNameSoftwareInfoFails) {
  EXPECT_DEATH(ValidateStructOrDie(SoftwareInfo()), "software info");
}

TEST(ValidatePlatformInfoTest, ValidPlatformInfoFails) {
  ValidateStructOrDie(PlatformInfo({.info = "Test info"}));
}

TEST(ValidatePlatformInfoDeathTest, NoInfoPlatformInfoFails) {
  EXPECT_DEATH(ValidateStructOrDie(PlatformInfo()), "platform info");
}

TEST(ValidateMeasurementSeriesStartTest, ValidMeasurementSeriesStartPasses) {
  ValidateStructOrDie(MeasurementSeriesStart({
      .name = "Example measurement",
      .subcomponent = Subcomponent({.name = "Example subcomponent"}),
      .validators = {{.type = ValidatorType::kEqual, .value = {100.}}},
  }));
}

struct InvalidMeasurementSeriesStartTestCase {
  MeasurementSeriesStart measurement_series_start;
  std::string want_error_regex;
};

class InvalidMeasurementSeriesStartStructDeathTest
    : public ::testing::TestWithParam<InvalidMeasurementSeriesStartTestCase> {};

TEST_P(InvalidMeasurementSeriesStartStructDeathTest,
       InvalidMeasurementSeriesStartFails) {
  InvalidMeasurementSeriesStartTestCase t = GetParam();
  EXPECT_DEATH(ValidateStructOrDie(t.measurement_series_start),
               t.want_error_regex);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidMeasurementSeriesStartFails,
    InvalidMeasurementSeriesStartStructDeathTest,
    Values(
        InvalidMeasurementSeriesStartTestCase(
            {.measurement_series_start = MeasurementSeriesStart(),
             .want_error_regex = "name field of the measurement series start"}),
        InvalidMeasurementSeriesStartTestCase(
            {.measurement_series_start =
                 {
                     .name = "Bad Subcomponent",
                     .subcomponent = Subcomponent(),
                 },
             .want_error_regex = "name field of the subcomponent"}),
        InvalidMeasurementSeriesStartTestCase(
            {.measurement_series_start =
                 {.name = "Bad Value Typing",
                  .validators =
                      {
                          {.type = ValidatorType::kEqual, .value = {100.}},
                          {.type = ValidatorType::kEqual, .value = {"str"}},
                      }},
             .want_error_regex = "All validators must be the same type"})));

TEST(ValidateMeasurementTest, ValidMeasurementPasses) {
  ValidateStructOrDie(Measurement({.name = "Fake name", .value = {100.}}));
}

struct InvalidMeasurementTestCase {
  Measurement measurement;
  std::string want_error_regex;
};

class InvalidMeasurementStructDeathTest
    : public ::testing::TestWithParam<InvalidMeasurementTestCase> {};

TEST_P(InvalidMeasurementStructDeathTest, InvalidMeasurementFails) {
  InvalidMeasurementTestCase t = GetParam();
  EXPECT_DEATH(ValidateStructOrDie(t.measurement), t.want_error_regex);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidMeasurementFails, InvalidMeasurementStructDeathTest,
    Values(InvalidMeasurementTestCase(
               {.measurement = Measurement({.value = {100.}}),
                .want_error_regex = "name field of the measurement"}),
           InvalidMeasurementTestCase(
               {.measurement =
                    {
                        .name = "Bad Subcomponent",
                        .subcomponent = Subcomponent(),
                        .value = {100.},
                    },
                .want_error_regex = "name field of the subcomponent"}),
           InvalidMeasurementTestCase(
               {.measurement = {.name = "Bad Value Typing",
                                .validators =
                                    {
                                        {.type = ValidatorType::kEqual,
                                         .value = {100.}},
                                    },
                                .value = {"string"}},
                .want_error_regex =
                    "All validators and the value must be the same type"}),
           InvalidMeasurementTestCase(
               {.measurement = {.name = "Bad Validator",
                                .validators = {Validator()},
                                .value = {"string"}},
                .want_error_regex = "At least one value must be specified"}),
           InvalidMeasurementTestCase(
               {.measurement =
                    {.name = "Bad Validator Types",
                     .validators =
                         {
                             {.type = ValidatorType::kEqual, .value = {100.}},
                             {.type = ValidatorType::kEqual, .value = {"str"}},
                         },
                     .value = {200.}},
                .want_error_regex =
                    "All validators and the value must be the same type"})));

TEST(ValidateDiagnosisTest, ValidDiagnosisPasses) {
  ValidateStructOrDie(Diagnosis({
      .verdict = "example-verdict",
      .type = DiagnosisType::kPass,
  }));
}

struct InvalidDiagnosisTestCase {
  Diagnosis diagnosis;
  std::string want_error_regex;
};

class InvalidDiagnosisStructDeathTest
    : public ::testing::TestWithParam<InvalidDiagnosisTestCase> {};

TEST_P(InvalidDiagnosisStructDeathTest, InvalidDiagnosisFails) {
  InvalidDiagnosisTestCase t = GetParam();
  EXPECT_DEATH(ValidateStructOrDie(t.diagnosis), t.want_error_regex);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidDiagnosisFail, InvalidDiagnosisStructDeathTest,
    Values(InvalidDiagnosisTestCase(
               {.diagnosis = Diagnosis(),
                .want_error_regex = "verdict field of the diagnosis"}),
           InvalidDiagnosisTestCase(
               {.diagnosis =
                    {
                        .verdict = "example-verdict",
                        .type = DiagnosisType::kPass,
                        .subcomponent = Subcomponent(),
                    },
                .want_error_regex = "name field of the subcomponent"}),
           InvalidDiagnosisTestCase(
               {.diagnosis = {.verdict = "example-verdict"},
                .want_error_regex = "type for all diagnoses"})));

TEST(ValidateErrorTest, ValidErrorPasses) {
  ValidateStructOrDie(Error({.symptom = "example-symptom"}));
}

TEST(ValidateErrorDeathTest, NoSymptomErrorFails) {
  EXPECT_DEATH(ValidateStructOrDie(Error()), "symptom field of the error");
}

TEST(ValidateLogTest, ValidLogPasses) {
  ValidateStructOrDie(Log({.message = "An awesome log"}));
}

TEST(ValidateLogDeathTest, NoMessageLogFails) {
  EXPECT_DEATH(ValidateStructOrDie(Log()), "message field of the log");
}

TEST(ValidateFileTest, ValidFilePasses) {
  ValidateStructOrDie(
      File({.display_name = "temp_file", .uri = "file:///usr/bin/sample.txt"}));
}

TEST(ValidateFileDeathTest, NoDisplayNameFileFails) {
  EXPECT_DEATH(ValidateStructOrDie(File({.uri = "file:///usr/bin/sample.txt"})),
               "display name of the file");
}

TEST(ValidateFileDeathTest, NoUriFileFails) {
  EXPECT_DEATH(ValidateStructOrDie(File({.display_name = "temp_file"})),
               "URI of the file");
}

TEST(ValidateTestRunInfoTest, ValidTestRunInfoPasses) {
  ValidateStructOrDie(TestRunStart(
      {.name = "my-diag", .version = "1.0", .command_line = "./my-diag"}));
}

struct InvalidTestRunInfoTestCase {
  TestRunStart test_run_info;
  std::string want_error_regex;
};

class InvalidTestRunInfoStructDeathTest
    : public ::testing::TestWithParam<InvalidTestRunInfoTestCase> {};

TEST_P(InvalidTestRunInfoStructDeathTest, InvalidTestRunInfoFails) {
  InvalidTestRunInfoTestCase t = GetParam();
  EXPECT_DEATH(ValidateStructOrDie(t.test_run_info), t.want_error_regex);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidTestRunInfoFail, InvalidTestRunInfoStructDeathTest,
    Values(
        InvalidTestRunInfoTestCase(
            {.test_run_info = {.version = "1.0", .command_line = "./my-diag"},
             .want_error_regex = "name of the test run info"}),
        InvalidTestRunInfoTestCase(
            {.test_run_info = {.name = "my-diag", .command_line = "./my-diag"},
             .want_error_regex = "version in the test run info"}),
        InvalidTestRunInfoTestCase(
            {.test_run_info = {.name = "my-diag", .version = "1.0"},
             .want_error_regex =
                 "command line invocation in the test run info"})));

TEST(ValidateExtensionTest, ValidExtensionPasses) {
  ValidateStructOrDie(Extension({
      .name = "A super cool extension",
      .content_json = R"json({"cool":"extension"})json",
  }));
}

struct InvalidExtensionTestCase {
  Extension extension;
  std::string want_error_regex;
};

class InvalidExtensionStructDeathTest
    : public ::testing::TestWithParam<InvalidExtensionTestCase> {};

TEST_P(InvalidExtensionStructDeathTest, InvalidExtensionFails) {
  InvalidExtensionTestCase t = GetParam();
  EXPECT_DEATH(ValidateStructOrDie(t.extension), t.want_error_regex);
}

INSTANTIATE_TEST_SUITE_P(
    InvalidExtensionFail, InvalidExtensionStructDeathTest,
    Values(InvalidExtensionTestCase(
               {.extension = {.content_json = R"json({"no":"name"})json"},
                .want_error_regex = "name of the extension"}),
           InvalidExtensionTestCase(
               {.extension = {.name = "No content JSON"},
                .want_error_regex = "content of the extension"})));

}  // namespace ocpdiag::results
