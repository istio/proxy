// Copyright 2024 Google LLC
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

#include "parser/standard_macros.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/source.h"
#include "internal/testing.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::google::api::expr::parser::EnrichedParse;
using ::testing::HasSubstr;

struct StandardMacrosTestCase {
  std::string expression;
  std::string error;
};

using StandardMacrosTest = ::testing::TestWithParam<StandardMacrosTestCase>;

TEST_P(StandardMacrosTest, Errors) {
  const auto& test_param = GetParam();
  ASSERT_OK_AND_ASSIGN(auto source, NewSource(test_param.expression));

  ParserOptions options;
  options.enable_optional_syntax = true;

  MacroRegistry registry;
  ASSERT_THAT(RegisterStandardMacros(registry, options), IsOk());

  EXPECT_THAT(EnrichedParse(*source, registry, options),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(test_param.error)));
}

INSTANTIATE_TEST_SUITE_P(
    StandardMacrosTest, StandardMacrosTest,
    ::testing::ValuesIn<StandardMacrosTestCase>({
        {
            .expression = "[].all(__result__, __result__ == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].exists(__result__, __result__ == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].exists_one(__result__, __result__ == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].map(__result__, __result__)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].map(__result__, true, __result__)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].filter(__result__, __result__ == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "foo.optMap(__result__, __result__)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "foo.optFlatMap(__result__, __result__)",
            .error = "variable name cannot be __result__",
        },
    }));

}  // namespace
}  // namespace cel
