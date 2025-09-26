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
#include "runtime/standard/string_functions.h"

#include <vector>

#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

enum class CallStyle { kFree, kReceiver };

MATCHER_P3(MatchesDescriptor, name, call_style, expected_kinds, "") {
  bool receiver_style;
  switch (call_style) {
    case CallStyle::kFree:
      receiver_style = false;
      break;
    case CallStyle::kReceiver:
      receiver_style = true;
      break;
  }
  const FunctionDescriptor& descriptor = *arg;
  const std::vector<Kind>& types = expected_kinds;
  return descriptor.name() == name &&
         descriptor.receiver_style() == receiver_style &&
         descriptor.types() == types;
}

TEST(RegisterStringFunctions, FunctionsRegistered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterStringFunctions(registry, options));
  auto overloads = registry.ListFunctions();

  EXPECT_THAT(
      overloads[builtin::kAdd],
      UnorderedElementsAre(
          MatchesDescriptor(builtin::kAdd, CallStyle::kFree,
                            std::vector<Kind>{Kind::kString, Kind::kString}),
          MatchesDescriptor(builtin::kAdd, CallStyle::kFree,
                            std::vector<Kind>{Kind::kBytes, Kind::kBytes})));

  EXPECT_THAT(overloads[builtin::kSize],
              UnorderedElementsAre(
                  MatchesDescriptor(builtin::kSize, CallStyle::kFree,
                                    std::vector<Kind>{Kind::kString}),
                  MatchesDescriptor(builtin::kSize, CallStyle::kFree,
                                    std::vector<Kind>{Kind::kBytes}),
                  MatchesDescriptor(builtin::kSize, CallStyle::kReceiver,
                                    std::vector<Kind>{Kind::kString}),
                  MatchesDescriptor(builtin::kSize, CallStyle::kReceiver,
                                    std::vector<Kind>{Kind::kBytes})));

  EXPECT_THAT(
      overloads[builtin::kStringContains],
      UnorderedElementsAre(
          MatchesDescriptor(builtin::kStringContains, CallStyle::kFree,
                            std::vector<Kind>{Kind::kString, Kind::kString}),

          MatchesDescriptor(builtin::kStringContains, CallStyle::kReceiver,
                            std::vector<Kind>{Kind::kString, Kind::kString})));
  EXPECT_THAT(
      overloads[builtin::kStringStartsWith],
      UnorderedElementsAre(
          MatchesDescriptor(builtin::kStringStartsWith, CallStyle::kFree,
                            std::vector<Kind>{Kind::kString, Kind::kString}),

          MatchesDescriptor(builtin::kStringStartsWith, CallStyle::kReceiver,
                            std::vector<Kind>{Kind::kString, Kind::kString})));
  EXPECT_THAT(
      overloads[builtin::kStringEndsWith],
      UnorderedElementsAre(
          MatchesDescriptor(builtin::kStringEndsWith, CallStyle::kFree,
                            std::vector<Kind>{Kind::kString, Kind::kString}),

          MatchesDescriptor(builtin::kStringEndsWith, CallStyle::kReceiver,
                            std::vector<Kind>{Kind::kString, Kind::kString})));
}

TEST(RegisterStringFunctions, ConcatSkippedWhenDisabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_string_concat = false;

  ASSERT_OK(RegisterStringFunctions(registry, options));
  auto overloads = registry.ListFunctions();

  EXPECT_THAT(overloads[builtin::kAdd], IsEmpty());
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
