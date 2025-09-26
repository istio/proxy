// Copyright 2022 Google LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "ocpdiag/core/results/data_model/input_model_helpers.h"

#include <vector>

#include "gtest/gtest.h"
#include "ocpdiag/core/results/data_model/input_model.h"

namespace ocpdiag::results {

namespace {

TEST(InputModelHelpersTest,
     CommandLineStringSuccessfullyGeneratedFromMainArgs) {
  const char* argv[] = {"diagname", "--flag", "flag_value"};
  EXPECT_EQ(CommandLineStringFromMainArgs(3, argv),
            "diagname --flag flag_value");
}

TEST(InputModelHelpersTest,
     ParameterJsonSuccessfullyGeneratedFromMultipleArgs) {
  const char* argv[] = {"diagname", "--flag", "flag_value", "-f2", "val 2"};
  EXPECT_EQ(ParameterJsonFromMainArgs(5, argv),
            R"json({"flag":"flag_value","f2":"val 2"})json");
}

TEST(InputModelHelpersTest, ParameterJsonSuccessfullyGeneratedFromOneArg) {
  const char* argv[] = {"diagname", "--flag", "flag_value"};
  EXPECT_EQ(ParameterJsonFromMainArgs(3, argv),
            R"json({"flag":"flag_value"})json");
}

TEST(InputModelHelpersTest, ParameterJsonSuccessfullyGeneratedFromNoArgs) {
  const char* argv[] = {"diagname"};
  EXPECT_EQ(ParameterJsonFromMainArgs(1, argv), R"json({})json");
}

TEST(InputModelHelpersTest,
     ValidateWithinInclusiveLimitsReturnsExpectedValidators) {
  std::vector<Validator> validators =
      ValidateWithinInclusiveLimits(2.0, 10.0, "Example");
  Validator expected_lower = Validator{
      .type = ValidatorType::kGreaterThanOrEqual,
      .value = {2.0},
      .name = "Example Lower",
  };
  Validator expected_upper = Validator{
      .type = ValidatorType::kLessThanOrEqual,
      .value = {10.0},
      .name = "Example Upper",
  };
  EXPECT_EQ(validators[0], expected_lower);
  EXPECT_EQ(validators[1], expected_upper);
}

TEST(InputModelHelpersTest,
     ValidateWithinExclusiveLimitsReturnsExpectedValidators) {
  std::vector<Validator> validators =
      ValidateWithinExclusiveLimits(5.0, 6.0, "Example");
  Validator expected_lower = Validator{
      .type = ValidatorType::kGreaterThanOrEqual,
      .value = {5.0},
      .name = "Example Lower",
  };
  Validator expected_upper = Validator{
      .type = ValidatorType::kLessThan,
      .value = {6.0},
      .name = "Example Upper",
  };
  EXPECT_EQ(validators[0], expected_lower);
  EXPECT_EQ(validators[1], expected_upper);
}

TEST(InputModelHelpersDeathTest, ValidateWithinLimitsDiesForInvalidLimits) {
  EXPECT_DEATH(ValidateWithinInclusiveLimits(5.0, 3.0), "");
  EXPECT_DEATH(ValidateWithinExclusiveLimits(5.0, 1.0), "");
}

}  // namespace

}  // namespace ocpdiag::results
