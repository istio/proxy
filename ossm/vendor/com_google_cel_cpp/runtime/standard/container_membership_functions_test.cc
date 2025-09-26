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

#include "runtime/standard/container_membership_functions.h"

#include <array>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "internal/testing.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

using ::testing::UnorderedElementsAre;

MATCHER_P3(MatchesDescriptor, name, receiver, expected_kinds, "") {
  const FunctionDescriptor& descriptor = *arg;
  const std::vector<Kind>& types = expected_kinds;
  return descriptor.name() == name && descriptor.receiver_style() == receiver &&
         descriptor.types() == types;
}

static constexpr std::array<absl::string_view, 3> kInOperators = {
    builtin::kIn, builtin::kInDeprecated, builtin::kInFunction};

TEST(RegisterContainerMembershipFunctions, RegistersHomogeneousInOperator) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_heterogeneous_equality = false;

  ASSERT_OK(RegisterContainerMembershipFunctions(registry, options));

  auto overloads = registry.ListFunctions();

  for (absl::string_view operator_name : kInOperators) {
    EXPECT_THAT(
        overloads[operator_name],
        UnorderedElementsAre(
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kInt, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kUint, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kDouble, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kString, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kBytes, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kBool, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kInt, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kUint, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kString, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kBool, Kind::kMap})));
  }
}

TEST(RegisterContainerMembershipFunctions, RegistersHeterogeneousInOperation) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_heterogeneous_equality = true;

  ASSERT_OK(RegisterContainerMembershipFunctions(registry, options));

  auto overloads = registry.ListFunctions();

  for (absl::string_view operator_name : kInOperators) {
    EXPECT_THAT(
        overloads[operator_name],
        UnorderedElementsAre(
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kAny, Kind::kList}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kInt, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kUint, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kDouble, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kString, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kBool, Kind::kMap})));
  }
}

TEST(RegisterContainerMembershipFunctions, RegistersInOperatorListsDisabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_list_contains = false;

  ASSERT_OK(RegisterContainerMembershipFunctions(registry, options));

  auto overloads = registry.ListFunctions();

  for (absl::string_view operator_name : kInOperators) {
    EXPECT_THAT(
        overloads[operator_name],
        UnorderedElementsAre(

            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kInt, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kUint, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kDouble, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kString, Kind::kMap}),
            MatchesDescriptor(operator_name, false,
                              std::vector<Kind>{Kind::kBool, Kind::kMap})));
  }
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
