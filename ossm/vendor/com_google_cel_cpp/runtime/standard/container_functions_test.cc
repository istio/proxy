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

#include "runtime/standard/container_functions.h"

#include <vector>

#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

MATCHER_P3(MatchesDescriptor, name, receiver, expected_kinds, "") {
  const FunctionDescriptor& descriptor = arg.descriptor;
  const std::vector<Kind>& types = expected_kinds;
  return descriptor.name() == name && descriptor.receiver_style() == receiver &&
         descriptor.types() == types;
}

TEST(RegisterContainerFunctions, RegistersSizeFunctions) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterContainerFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kSize, false, {Kind::kAny}),
      UnorderedElementsAre(MatchesDescriptor(builtin::kSize, false,
                                             std::vector<Kind>{Kind::kList}),
                           MatchesDescriptor(builtin::kSize, false,
                                             std::vector<Kind>{Kind::kMap})));
  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kSize, true, {Kind::kAny}),
      UnorderedElementsAre(MatchesDescriptor(builtin::kSize, true,
                                             std::vector<Kind>{Kind::kList}),
                           MatchesDescriptor(builtin::kSize, true,
                                             std::vector<Kind>{Kind::kMap})));
}

TEST(RegisterContainerFunctions, RegisterListConcatEnabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_list_concat = true;

  ASSERT_OK(RegisterContainerFunctions(registry, options));

  EXPECT_THAT(
      registry.FindStaticOverloads(builtin::kAdd, false,
                                   {Kind::kAny, Kind::kAny}),
      UnorderedElementsAre(MatchesDescriptor(
          builtin::kAdd, false, std::vector<Kind>{Kind::kList, Kind::kList})));
}

TEST(RegisterContainerFunctions, RegisterListConcateDisabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_list_concat = false;

  ASSERT_OK(RegisterContainerFunctions(registry, options));

  EXPECT_THAT(registry.FindStaticOverloads(builtin::kAdd, false,
                                           {Kind::kAny, Kind::kAny}),
              IsEmpty());
}

TEST(RegisterContainerFunctions, RegisterRuntimeListAppend) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterContainerFunctions(registry, options));

  EXPECT_THAT(registry.FindStaticOverloads(builtin::kRuntimeListAppend, false,
                                           {Kind::kAny, Kind::kAny}),
              UnorderedElementsAre(MatchesDescriptor(
                  builtin::kRuntimeListAppend, false,
                  std::vector<Kind>{Kind::kList, Kind::kAny})));
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
