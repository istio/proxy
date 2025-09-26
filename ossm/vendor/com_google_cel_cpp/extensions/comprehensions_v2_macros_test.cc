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

#include "extensions/comprehensions_v2_macros.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "common/source.h"
#include "internal/testing.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::google::api::expr::parser::EnrichedParse;
using ::testing::HasSubstr;

struct ComprehensionsV2MacrosTestCase {
  std::string expression;
  std::string error;
};

using ComprehensionsV2MacrosTest =
    ::testing::TestWithParam<ComprehensionsV2MacrosTestCase>;

TEST_P(ComprehensionsV2MacrosTest, Basic) {
  const auto& test_param = GetParam();
  ASSERT_OK_AND_ASSIGN(auto source, NewSource(test_param.expression));

  MacroRegistry registry;
  ASSERT_THAT(RegisterComprehensionsV2Macros(registry, ParserOptions()),
              IsOk());

  EXPECT_THAT(EnrichedParse(*source, registry),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr(test_param.error)));
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensionsV2MacrosTest, ComprehensionsV2MacrosTest,
    ::testing::ValuesIn<ComprehensionsV2MacrosTestCase>({
        {
            .expression = "[].all(__result__, v, v == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].all(i, __result__, i == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].all(e, e, e == e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "[].all(foo.bar, e, true)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "[].all(e, foo.bar, true)",
            .error = "second variable name must be a simple identifier",
        },
        {
            .expression = "[].exists(__result__, v, v == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].exists(i, __result__, i == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].exists(e, e, e == e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "[].exists(foo.bar, e, true)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "[].exists(e, foo.bar, true)",
            .error = "second variable name must be a simple identifier",
        },
        {
            .expression = "[].existsOne(__result__, v, v == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].existsOne(i, __result__, i == 0)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].existsOne(e, e, e == e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "[].existsOne(foo.bar, e, true)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "[].existsOne(e, foo.bar, true)",
            .error = "second variable name must be a simple identifier",
        },
        {
            .expression = "[].transformList(__result__, v, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].transformList(i, __result__, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].transformList(e, e, e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "[].transformList(foo.bar, e, e)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "[].transformList(e, foo.bar, e)",
            .error = "second variable name must be a simple identifier",
        },
        {
            .expression = "[].transformList(__result__, v, v == 0, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].transformList(i, __result__, i == 0, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "[].transformList(e, e, e == e, e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "[].transformList(foo.bar, e, true, e)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "[].transformList(e, foo.bar, true, e)",
            .error = "second variable name must be a simple identifier",
        },
        {
            .expression = "{}.transformMap(__result__, v, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "{}.transformMap(k, __result__, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "{}.transformMap(e, e, e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "{}.transformMap(foo.bar, e, e)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "{}.transformMap(e, foo.bar, e)",
            .error = "second variable name must be a simple identifier",
        },
        {
            .expression = "{}.transformMap(__result__, v, v == 0, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "{}.transformMap(k, __result__, k == 0, v)",
            .error = "variable name cannot be __result__",
        },
        {
            .expression = "{}.transformMap(e, e, e == e, e)",
            .error =
                "second variable must be different from the first variable",
        },
        {
            .expression = "{}.transformMap(foo.bar, e, true, e)",
            .error = "first variable name must be a simple identifier",
        },
        {
            .expression = "{}.transformMap(e, foo.bar, true, e)",
            .error = "second variable name must be a simple identifier",
        },
    }));

}  // namespace
}  // namespace cel::extensions
