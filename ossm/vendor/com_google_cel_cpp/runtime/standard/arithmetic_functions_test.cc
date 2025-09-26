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

#include "runtime/standard/arithmetic_functions.h"

#include <vector>

#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::UnorderedElementsAre;

MATCHER_P2(MatchesOperatorDescriptor, name, expected_kind, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;
  std::vector<Kind> types{expected_kind, expected_kind};
  return descriptor.name() == name && descriptor.receiver_style() == false &&
         descriptor.types() == types;
}

MATCHER_P(MatchesNegationDescriptor, expected_kind, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;
  std::vector<Kind> types{expected_kind};
  return descriptor.name() == builtin::kNeg &&
         descriptor.receiver_style() == false && descriptor.types() == types;
}

TEST(RegisterArithmeticFunctions, Registered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterArithmeticFunctions(registry, options));

  EXPECT_THAT(registry.FindStaticOverloads(builtin::kAdd, false,
                                           {Kind::kAny, Kind::kAny}),
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kAdd, Kind::kInt),
                  MatchesOperatorDescriptor(builtin::kAdd, Kind::kDouble),
                  MatchesOperatorDescriptor(builtin::kAdd, Kind::kUint)));
  EXPECT_THAT(registry.FindStaticOverloads(builtin::kSubtract, false,
                                           {Kind::kAny, Kind::kAny}),
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kSubtract, Kind::kInt),
                  MatchesOperatorDescriptor(builtin::kSubtract, Kind::kDouble),
                  MatchesOperatorDescriptor(builtin::kSubtract, Kind::kUint)));
  EXPECT_THAT(registry.FindStaticOverloads(builtin::kDivide, false,
                                           {Kind::kAny, Kind::kAny}),
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kDivide, Kind::kInt),
                  MatchesOperatorDescriptor(builtin::kDivide, Kind::kDouble),
                  MatchesOperatorDescriptor(builtin::kDivide, Kind::kUint)));
  EXPECT_THAT(registry.FindStaticOverloads(builtin::kMultiply, false,
                                           {Kind::kAny, Kind::kAny}),
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kMultiply, Kind::kInt),
                  MatchesOperatorDescriptor(builtin::kMultiply, Kind::kDouble),
                  MatchesOperatorDescriptor(builtin::kMultiply, Kind::kUint)));
  EXPECT_THAT(registry.FindStaticOverloads(builtin::kModulo, false,
                                           {Kind::kAny, Kind::kAny}),
              UnorderedElementsAre(
                  MatchesOperatorDescriptor(builtin::kModulo, Kind::kInt),
                  MatchesOperatorDescriptor(builtin::kModulo, Kind::kUint)));
  EXPECT_THAT(registry.FindStaticOverloads(builtin::kNeg, false, {Kind::kAny}),
              UnorderedElementsAre(MatchesNegationDescriptor(Kind::kInt),
                                   MatchesNegationDescriptor(Kind::kDouble)));
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
