// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "runtime/standard/time_functions.h"

#include <vector>

#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::UnorderedElementsAre;

MATCHER_P3(MatchesOperatorDescriptor, name, expected_kind1, expected_kind2,
           "") {
  const FunctionDescriptor& descriptor = *arg;
  std::vector<Kind> types{expected_kind1, expected_kind2};
  return descriptor.name() == name && descriptor.receiver_style() == false &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesTimeAccessor, name, kind, "") {
  const FunctionDescriptor& descriptor = *arg;

  std::vector<Kind> types{kind};
  return descriptor.name() == name && descriptor.receiver_style() == true &&
         descriptor.types() == types;
}

MATCHER_P2(MatchesTimezoneTimeAccessor, name, kind, "") {
  const FunctionDescriptor& descriptor = *arg;

  std::vector<Kind> types{kind, Kind::kString};
  return descriptor.name() == name && descriptor.receiver_style() == true &&
         descriptor.types() == types;
}

TEST(RegisterTimeFunctions, MathOperatorsRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTimeFunctions(registry, options));

  auto registered_functions = registry.ListFunctions();

  EXPECT_THAT(registered_functions[builtin::kAdd],
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kAdd, Kind::kDuration,
                                            Kind::kDuration),
                  MatchesOperatorDescriptor(builtin::kAdd, Kind::kTimestamp,
                                            Kind::kDuration),
                  MatchesOperatorDescriptor(builtin::kAdd, Kind::kDuration,
                                            Kind::kTimestamp)));

  EXPECT_THAT(registered_functions[builtin::kSubtract],
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kSubtract, Kind::kDuration,
                                            Kind::kDuration),
                  MatchesOperatorDescriptor(builtin::kSubtract,
                                            Kind::kTimestamp, Kind::kDuration),
                  MatchesOperatorDescriptor(
                      builtin::kSubtract, Kind::kTimestamp, Kind::kTimestamp)));
}

TEST(RegisterTimeFunctions, AccessorsRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterTimeFunctions(registry, options));

  auto registered_functions = registry.ListFunctions();
  EXPECT_THAT(
      registered_functions[builtin::kFullYear],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kFullYear, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kFullYear, Kind::kTimestamp)));
  EXPECT_THAT(
      registered_functions[builtin::kDate],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kDate, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kDate, Kind::kTimestamp)));
  EXPECT_THAT(
      registered_functions[builtin::kMonth],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kMonth, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kMonth, Kind::kTimestamp)));
  EXPECT_THAT(
      registered_functions[builtin::kDayOfYear],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kDayOfYear, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kDayOfYear, Kind::kTimestamp)));
  EXPECT_THAT(
      registered_functions[builtin::kDayOfMonth],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kDayOfMonth, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kDayOfMonth, Kind::kTimestamp)));
  EXPECT_THAT(
      registered_functions[builtin::kDayOfWeek],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kDayOfWeek, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kDayOfWeek, Kind::kTimestamp)));

  EXPECT_THAT(
      registered_functions[builtin::kHours],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kHours, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kHours, Kind::kTimestamp),
          MatchesTimeAccessor(builtin::kHours, Kind::kDuration)));

  EXPECT_THAT(
      registered_functions[builtin::kMinutes],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kMinutes, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kMinutes, Kind::kTimestamp),
          MatchesTimeAccessor(builtin::kMinutes, Kind::kDuration)));

  EXPECT_THAT(
      registered_functions[builtin::kSeconds],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kSeconds, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kSeconds, Kind::kTimestamp),
          MatchesTimeAccessor(builtin::kSeconds, Kind::kDuration)));

  EXPECT_THAT(
      registered_functions[builtin::kMilliseconds],
      UnorderedElementsAre(
          MatchesTimeAccessor(builtin::kMilliseconds, Kind::kTimestamp),
          MatchesTimezoneTimeAccessor(builtin::kMilliseconds, Kind::kTimestamp),
          MatchesTimeAccessor(builtin::kMilliseconds, Kind::kDuration)));
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
