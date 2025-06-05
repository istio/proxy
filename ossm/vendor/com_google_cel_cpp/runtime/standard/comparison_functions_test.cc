// Copyright 2021 Google LLC
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

#include "runtime/standard/comparison_functions.h"

#include <array>

#include "absl/strings/str_cat.h"
#include "base/builtins.h"
#include "base/kind.h"
#include "internal/testing.h"

namespace cel {
namespace {

MATCHER_P2(DefinesHomogenousOverload, name, argument_kind,
           absl::StrCat(name, " for ", KindToString(argument_kind))) {
  const cel::FunctionRegistry& registry = arg;
  return !registry
              .FindStaticOverloads(name, /*receiver_style=*/false,
                                   {argument_kind, argument_kind})
              .empty();
}

constexpr std::array<Kind, 8> kOrderableTypes = {
    Kind::kBool,   Kind::kInt64, Kind::kUint64,   Kind::kString,
    Kind::kDouble, Kind::kBytes, Kind::kDuration, Kind::kTimestamp};

TEST(RegisterComparisonFunctionsTest, LessThanDefined) {
  RuntimeOptions default_options;
  FunctionRegistry registry;
  ASSERT_OK(RegisterComparisonFunctions(registry, default_options));
  for (Kind kind : kOrderableTypes) {
    EXPECT_THAT(registry, DefinesHomogenousOverload(builtin::kLess, kind));
  }
}

TEST(RegisterComparisonFunctionsTest, LessThanOrEqualDefined) {
  RuntimeOptions default_options;
  FunctionRegistry registry;
  ASSERT_OK(RegisterComparisonFunctions(registry, default_options));
  for (Kind kind : kOrderableTypes) {
    EXPECT_THAT(registry,
                DefinesHomogenousOverload(builtin::kLessOrEqual, kind));
  }
}

TEST(RegisterComparisonFunctionsTest, GreaterThanDefined) {
  RuntimeOptions default_options;
  FunctionRegistry registry;
  ASSERT_OK(RegisterComparisonFunctions(registry, default_options));
  for (Kind kind : kOrderableTypes) {
    EXPECT_THAT(registry, DefinesHomogenousOverload(builtin::kGreater, kind));
  }
}

TEST(RegisterComparisonFunctionsTest, GreaterThanOrEqualDefined) {
  RuntimeOptions default_options;
  FunctionRegistry registry;
  ASSERT_OK(RegisterComparisonFunctions(registry, default_options));
  for (Kind kind : kOrderableTypes) {
    EXPECT_THAT(registry,
                DefinesHomogenousOverload(builtin::kGreaterOrEqual, kind));
  }
}

// TODO: move functional tests from wrapper library after top-level
// APIs are available for planning and running an expression.

}  // namespace
}  // namespace cel
