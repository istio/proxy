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

#include "runtime/standard/equality_functions.h"

#include <vector>

#include "absl/status/status_matchers.h"
#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "common/kind.h"
#include "internal/testing.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

MATCHER_P3(MatchesDescriptor, name, receiver, expected_kinds, "") {
  const FunctionDescriptor& descriptor = *arg;
  const std::vector<Kind>& types = expected_kinds;
  return descriptor.name() == name && descriptor.receiver_style() == receiver &&
         descriptor.types() == types;
}

TEST(RegisterEqualityFunctionsHomogeneous, RegistersEqualOperators) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_heterogeneous_equality = false;

  ASSERT_THAT(RegisterEqualityFunctions(registry, options), IsOk());
  auto overloads = registry.ListFunctions();
  EXPECT_THAT(
      overloads[builtin::kEqual],
      UnorderedElementsAre(
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kList, Kind::kList}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kMap, Kind::kMap}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kBool, Kind::kBool}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kInt, Kind::kInt}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kUint, Kind::kUint}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kDouble, Kind::kDouble}),
          MatchesDescriptor(
              builtin::kEqual, false,
              std::vector<Kind>{Kind::kDuration, Kind::kDuration}),
          MatchesDescriptor(
              builtin::kEqual, false,
              std::vector<Kind>{Kind::kTimestamp, Kind::kTimestamp}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kString, Kind::kString}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kBytes, Kind::kBytes}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kType, Kind::kType}),
          // Structs comparable to null, but struct == struct undefined.
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kStruct, Kind::kNullType}),
          MatchesDescriptor(builtin::kEqual, false,
                            std::vector<Kind>{Kind::kNullType, Kind::kStruct}),
          MatchesDescriptor(
              builtin::kEqual, false,
              std::vector<Kind>{Kind::kNullType, Kind::kNullType})));

  EXPECT_THAT(
      overloads[builtin::kInequal],
      UnorderedElementsAre(
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kList, Kind::kList}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kMap, Kind::kMap}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kBool, Kind::kBool}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kInt, Kind::kInt}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kUint, Kind::kUint}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kDouble, Kind::kDouble}),
          MatchesDescriptor(
              builtin::kInequal, false,
              std::vector<Kind>{Kind::kDuration, Kind::kDuration}),
          MatchesDescriptor(
              builtin::kInequal, false,
              std::vector<Kind>{Kind::kTimestamp, Kind::kTimestamp}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kString, Kind::kString}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kBytes, Kind::kBytes}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kType, Kind::kType}),
          // Structs comparable to null, but struct != struct undefined.
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kStruct, Kind::kNullType}),
          MatchesDescriptor(builtin::kInequal, false,
                            std::vector<Kind>{Kind::kNullType, Kind::kStruct}),
          MatchesDescriptor(
              builtin::kInequal, false,
              std::vector<Kind>{Kind::kNullType, Kind::kNullType})));
}

TEST(RegisterEqualityFunctionsHeterogeneous, RegistersEqualOperators) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_fast_builtins = false;

  ASSERT_THAT(RegisterEqualityFunctions(registry, options), IsOk());
  auto overloads = registry.ListFunctions();

  EXPECT_THAT(
      overloads[builtin::kEqual],
      UnorderedElementsAre(MatchesDescriptor(
          builtin::kEqual, false, std::vector<Kind>{Kind::kAny, Kind::kAny})));

  EXPECT_THAT(overloads[builtin::kInequal],
              UnorderedElementsAre(MatchesDescriptor(
                  builtin::kInequal, false,
                  std::vector<Kind>{Kind::kAny, Kind::kAny})));
}

TEST(RegisterEqualityFunctionsHeterogeneous,
     NotRegisteredWhenFastBuiltinsEnabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_fast_builtins = true;

  ASSERT_THAT(RegisterEqualityFunctions(registry, options), IsOk());
  auto overloads = registry.ListFunctions();

  EXPECT_THAT(overloads[builtin::kEqual], IsEmpty());

  EXPECT_THAT(overloads[builtin::kInequal], IsEmpty());
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
