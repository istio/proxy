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
#include "runtime/standard/regex_functions.h"

#include <vector>

#include "base/builtins.h"
#include "common/function_descriptor.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

enum class CallStyle { kFree, kReceiver };

MATCHER_P2(MatchesDescriptor, name, call_style, "") {
  bool receiver_style;
  switch (call_style) {
    case CallStyle::kReceiver:
      receiver_style = true;
      break;
    case CallStyle::kFree:
      receiver_style = false;
      break;
  }
  const FunctionDescriptor& descriptor = *arg;
  std::vector<Kind> types{Kind::kString, Kind::kString};
  return descriptor.name() == name &&
         descriptor.receiver_style() == receiver_style &&
         descriptor.types() == types;
}

TEST(RegisterRegexFunctions, Registered) {
  FunctionRegistry registry;
  RuntimeOptions options;

  ASSERT_OK(RegisterRegexFunctions(registry, options));

  auto overloads = registry.ListFunctions();

  EXPECT_THAT(overloads[builtin::kRegexMatch],
              UnorderedElementsAre(
                  MatchesDescriptor(builtin::kRegexMatch, CallStyle::kReceiver),
                  MatchesDescriptor(builtin::kRegexMatch, CallStyle::kFree)));
}

TEST(RegisterRegexFunctions, NotRegisteredIfDisabled) {
  FunctionRegistry registry;
  RuntimeOptions options;
  options.enable_regex = false;

  ASSERT_OK(RegisterRegexFunctions(registry, options));

  auto overloads = registry.ListFunctions();

  EXPECT_THAT(overloads[builtin::kRegexMatch], IsEmpty());
}

// TODO(uncreated-issue/41): move functional parsed expr tests when modern APIs for
// evaluator available.

}  // namespace
}  // namespace cel
