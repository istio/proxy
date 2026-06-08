// Copyright 2025 Google LLC
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

#include "extensions/comprehensions_v2.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "checker/standard_library.h"
#include "checker/validation_result.h"
#include "common/value_testing.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "compiler/compiler_factory.h"
#include "compiler/optional.h"
#include "extensions/bindings_ext.h"
#include "extensions/comprehensions_v2_functions.h"
#include "extensions/strings.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "runtime/activation.h"
#include "runtime/optional_types.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::ErrorValueIs;
using ::testing::HasSubstr;
using ::testing::TestWithParam;

absl::StatusOr<std::unique_ptr<Program>> CreateProgram(
    const std::string& expression, bool enable_mutable_accumulator,
    int max_recursion_depth) {
  // Configure the compiler
  CEL_ASSIGN_OR_RETURN(
      auto compiler_builder,
      NewCompilerBuilder(internal::GetTestingDescriptorPool()));
  CEL_RETURN_IF_ERROR(compiler_builder->AddLibrary(StandardCheckerLibrary()));
  CEL_RETURN_IF_ERROR(compiler_builder->AddLibrary(OptionalCompilerLibrary()));
  CEL_RETURN_IF_ERROR(compiler_builder->AddLibrary(BindingsCompilerLibrary()));
  CEL_RETURN_IF_ERROR(compiler_builder->AddLibrary(StringsCompilerLibrary()));
  CEL_RETURN_IF_ERROR(compiler_builder->AddLibrary(
      extensions::ComprehensionsV2CompilerLibrary()));

  CEL_ASSIGN_OR_RETURN(auto compiler, std::move(*compiler_builder).Build());

  // Configure the runtime
  cel::RuntimeOptions options;
  options.enable_qualified_type_identifiers = true;
  options.enable_comprehension_list_append = enable_mutable_accumulator;
  options.enable_comprehension_mutable_map = enable_mutable_accumulator;
  options.max_recursion_depth = max_recursion_depth;

  CEL_ASSIGN_OR_RETURN(auto runtime_builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  CEL_RETURN_IF_ERROR(EnableOptionalTypes(runtime_builder));
  CEL_RETURN_IF_ERROR(
      RegisterStringsFunctions(runtime_builder.function_registry(), options));
  CEL_RETURN_IF_ERROR(RegisterComprehensionsV2Functions(
      runtime_builder.function_registry(), options));
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<const Runtime> runtime,
                       std::move(runtime_builder).Build());

  CEL_ASSIGN_OR_RETURN(ValidationResult result, compiler->Compile(expression));
  if (!result.IsValid()) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
                        result.FormatError());
  }
  return runtime->CreateProgram(*result.ReleaseAst());
}

struct TestOptions {
  bool enable_mutable_accumulator;
  int max_recursion_depth;
};

struct ComprehensionsV2TestCase {
  std::string expression;
  absl::StatusCode expected_status_code = absl::StatusCode::kOk;
  std::string expected_error;
};

class ComprehensionsV2Test
    : public TestWithParam<std::tuple<ComprehensionsV2TestCase, TestOptions>> {
};

TEST_P(ComprehensionsV2Test, Basic) {
  const ComprehensionsV2TestCase& test_case = std::get<0>(GetParam());
  const TestOptions& options = std::get<1>(GetParam());

  absl::StatusOr<std::unique_ptr<Program>> program =
      CreateProgram(test_case.expression, options.enable_mutable_accumulator,
                    options.max_recursion_depth);

  if (!program.ok()) {
    EXPECT_THAT(program, StatusIs(absl::StatusCode::kInvalidArgument,
                                  HasSubstr(test_case.expected_error)));
    // The error is expected. Nothing more to do in this test case
    return;
  }

  ASSERT_THAT(program, IsOk());

  google::protobuf::Arena arena;
  Activation activation;

  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_THAT(program.value()->Evaluate(&arena, activation),
                IsOkAndHolds(BoolValueIs(true)))
        << test_case.expression;
  } else {
    EXPECT_THAT(program.value()->Evaluate(&arena, activation),
                IsOkAndHolds(ErrorValueIs(StatusIs(
                    test_case.expected_status_code, test_case.expected_error))))
        << test_case.expression;
  }
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensionsV2Test, ComprehensionsV2Test,
    ::testing::Combine(
        ::testing::ValuesIn<ComprehensionsV2TestCase>({
            // list.all()
            {.expression = "[1, 2, 3, 4].all(i, v, i < 5 && v > 0)"},
            {.expression = "[1, 2, 3, 4].all(i, v, i < v)"},
            {.expression = "[1, 2, 3, 4].all(i, v, i > v) == false"},
            {
                .expression =
                    R"cel(cel.bind(listA, [1, 2, 3, 4], cel.bind(listB, [1, 2, 3, 4, 5], listA.all(i, v, listB[?i].hasValue() && listB[i] == v))))cel",
            },
            {
                .expression =
                    R"cel(cel.bind(listA, [1, 2, 3, 4, 5, 6], cel.bind(listB, [1, 2, 3, 4, 5], listA.all(i, v, listB[?i].hasValue() && listB[i] == v))) == false)cel",
            },
            {
                .expression = "[].all(__result__, v, v == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].all(__result__, v, v == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].all(i, __result__, i == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].all(e, e, e == e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "[].all(foo.bar, e, true)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "[].all(e, foo.bar, true)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },

            // list.exists()
            {
                .expression =
                    R"cel(cel.bind(l, ['hello', 'world', 'hello!', 'worlds'], l.exists(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next.endsWith('world')).orValue(false))))cel",
            },
            {
                .expression = "[].exists(__result__, v, v == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].exists(i, __result__, i == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].exists(e, e, e == e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "[].exists(foo.bar, e, true)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "[].exists(e, foo.bar, true)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            // list.existsOne()
            {
                .expression =
                    R"cel(cel.bind(l, ['hello', 'world', 'hello!', 'worlds'], l.existsOne(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next.endsWith('world')).orValue(false))))cel",
            },
            {
                .expression =
                    R"cel(cel.bind(l, ['hello', 'goodbye', 'hello!', 'goodbye'], l.existsOne(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next == 'goodbye').orValue(false))) == false)cel",
            },
            {
                .expression = "[].existsOne(__result__, v, v == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].existsOne(i, __result__, i == 0)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].existsOne(e, e, e == e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "[].existsOne(foo.bar, e, true)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "[].existsOne(e, foo.bar, true)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            // list.transformList()
            {
                .expression =
                    R"cel(['Hello', 'world'].transformList(i, v, '[' + string(i) + ']' + v.lowerAscii()) == ['[0]hello', '[1]world'])cel",
            },
            {
                .expression =
                    R"cel(['hello', 'world'].transformList(i, v, v.startsWith('greeting'), '[' + string(i) + ']' + v) == [])cel",
            },
            {
                .expression =
                    R"cel([1, 2, 3].transformList(indexVar, valueVar, (indexVar * valueVar) + valueVar) == [1, 4, 9])cel",
            },
            {
                .expression =
                    R"cel([1, 2, 3].transformList(indexVar, valueVar, indexVar % 2 == 0, (indexVar * valueVar) + valueVar) == [1, 9])cel",
            },
            {
                .expression = "[].transformList(__result__, v, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].transformList(i, __result__, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].transformList(e, e, e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "[].transformList(foo.bar, e, e)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "[].transformList(e, foo.bar, e)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            {
                .expression = "[].transformList(__result__, v, v == 0, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].transformList(i, __result__, i == 0, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "[].transformList(e, e, e == e, e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "[].transformList(foo.bar, e, true, e)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "[].transformList(e, foo.bar, true, e)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            // list.transformMap()
            {
                .expression =
                    R"cel(['Hello', 'world'].transformMap(i, v, [v.lowerAscii()]) == {0: ['hello'], 1: ['world']})cel",
            },
            {
                .expression =
                    R"cel([1, 2, 3].transformMap(indexVar, valueVar, (indexVar * valueVar) + valueVar) == {0: 1, 1: 4, 2: 9})cel",
            },
            {
                .expression =
                    R"cel([1, 2, 3].transformMap(indexVar, valueVar, indexVar % 2 == 0, (indexVar * valueVar) + valueVar) == {0: 1, 2: 9})cel",
            },
            // map.all()
            {
                .expression =
                    R"cel({'hello': 'world', 'hello!': 'world'}.all(k, v, k.startsWith('hello') && v == 'world'))cel",
            },
            {
                .expression =
                    R"cel({'hello': 'world', 'hello!': 'worlds'}.all(k, v, k.startsWith('hello') && v.endsWith('world')) == false)cel",
            },
            // map.exists()
            {
                .expression =
                    R"cel({'hello': 'world', 'hello!': 'worlds'}.exists(k, v, k.startsWith('hello') && v.endsWith('world')))cel",
            },
            // map.existsOne()
            {
                .expression =
                    R"cel({'hello': 'world', 'hello!': 'worlds'}.existsOne(k, v, k.startsWith('hello') && v.endsWith('world')))cel",
            },
            {
                .expression =
                    R"cel({'hello': 'world', 'hello!': 'wow, world'}.existsOne(k, v, k.startsWith('hello') && v.endsWith('world')) == false)cel",
            },
            // map.transformList()
            {
                .expression =
                    R"cel({'Hello': 'world'}.transformList(k, v, k.lowerAscii() + "=" + v) == ['hello=world'])cel",
            },
            {
                .expression =
                    R"cel({'hello': 'world'}.transformList(k, v, k.startsWith('greeting'), k + "=" + v) == [])cel",
            },
            {
                .expression =
                    R"cel(cel.bind(m, {'farewell': 'goodbye', 'greeting': 'hello'}.transformList(k, _, k), m == ['farewell', 'greeting'] || m == ['greeting', 'farewell']))cel",
            },
            {
                .expression =
                    R"cel(cel.bind(m, {'greeting': 'hello', 'farewell': 'goodbye'}.transformList(_, v, v), m == ['goodbye', 'hello'] || m == ['hello', 'goodbye']))cel",
            },
            // map.transformMap()
            {
                .expression =
                    R"cel({'hello': 'world', 'goodbye': 'cruel world'}.transformMap(k, v, k + ', ' + v + '!') == {'hello': 'hello, world!', 'goodbye': 'goodbye, cruel world!'})cel",
            },
            {
                .expression =
                    R"cel({'hello': 'world', 'goodbye': 'cruel world'}.transformMap(k, v, v.startsWith('world'), k + ", " + v + "!") == {'hello': 'hello, world!'})cel",
            },
            {
                .expression = "{}.transformMap(__result__, v, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMap(k, __result__, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMap(e, e, e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "{}.transformMap(foo.bar, e, e)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "{}.transformMap(e, foo.bar, e)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            {
                .expression = "{}.transformMap(__result__, v, v == 0, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMap(k, __result__, k == 0, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMap(e, e, e == e, e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "{}.transformMap(foo.bar, e, true, e)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "{}.transformMap(e, foo.bar, true, e)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            // map.transformMapEntry
            {
                .expression =
                    R"cel({'hello': 'world', 'greetings': 'tacocat'}.transformMapEntry(k, v, {v: k}) == {'world': 'hello', 'tacocat': 'greetings'})cel",
            },
            {
                .expression =
                    R"cel({'hello': 'world', 'greetings': 'tacocat'}.transformMapEntry(k, v, {}) == {})cel",
            },
            {
                .expression =
                    R"cel({'a': 'same', 'c': 'same'}.transformMapEntry(k, v, {v: k}))cel",
                .expected_status_code = absl::StatusCode::kAlreadyExists,
                .expected_error = "duplicate key in map",
            },
            {
                .expression = "{}.transformMapEntry(__result__, v, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMapEntry(k, __result__, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMapEntry(e, e, e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "{}.transformMapEntry(foo.bar, e, e)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "{}.transformMapEntry(e, foo.bar, e)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            // transformMapEntry(k, v, filter, expr)
            {
                .expression =
                    R"cel({'hello': 'world', 'same': 'same'}.transformMapEntry(k, v, k != v, {v: k}) == {'world': 'hello'})cel",
            },
            {
                .expression = "{}.transformMapEntry(__result__, v, v == 0, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMapEntry(k, __result__, k == 0, v)",
                .expected_error = "variable name cannot be __result__",
            },
            {
                .expression = "{}.transformMapEntry(e, e, e == e, e)",
                .expected_error =
                    "second variable must be different from the first variable",
            },
            {
                .expression = "{}.transformMapEntry(foo.bar, e, true, e)",
                .expected_error =
                    "first variable name must be a simple identifier",
            },
            {
                .expression = "{}.transformMapEntry(e, foo.bar, true, e)",
                .expected_error =
                    "second variable name must be a simple identifier",
            },
            // list.transformMapEntry
            {
                .expression =
                    R"cel(['one', 'two'].transformMapEntry(k, v, {k + 1: 'is ' + v}) == {1: 'is one', 2: 'is two'})cel",
            },
        }),
        ::testing::ValuesIn<TestOptions>({
            {
                .enable_mutable_accumulator = true,
                .max_recursion_depth = 0,
            },
            {
                .enable_mutable_accumulator = false,
                .max_recursion_depth = 0,
            },
            {
                .enable_mutable_accumulator = true,
                .max_recursion_depth = -1,
            },
            {
                .enable_mutable_accumulator = false,
                .max_recursion_depth = -1,
            },
        })));

class ComprehensionsV2TestMutableAccumulator
    : public TestWithParam<std::tuple<ComprehensionsV2TestCase, TestOptions>> {
};

TEST_P(ComprehensionsV2TestMutableAccumulator, MutableAccumulator) {
  const ComprehensionsV2TestCase& test_case = std::get<0>(GetParam());
  const TestOptions& options = std::get<1>(GetParam());

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<Program> program,
      CreateProgram(test_case.expression, options.enable_mutable_accumulator,
                    options.max_recursion_depth));

  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto result, program->Evaluate(&arena, activation));
  bool is_mutable_accumulator = common_internal::IsMutableListValue(result) ||
                                common_internal::IsMutableMapValue(result);
  EXPECT_EQ(is_mutable_accumulator, options.enable_mutable_accumulator);
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensionsV2Test, ComprehensionsV2TestMutableAccumulator,
    ::testing::Combine(
        ::testing::ValuesIn<ComprehensionsV2TestCase>({
            {.expression =
                 R"cel(['Hello', 'world'].transformList(i, v, i))cel"},
            {
                .expression =
                    R"cel({'hello': 'world'}.transformMap(k, v, k + v))cel",
            },
            {
                .expression =
                    R"cel(['hello', 'world'].transformMap(k, v, v))cel",
            },
            {
                .expression =
                    R"cel({'hello': 'world'}.transformMapEntry(k, v, {v: k}))cel",
            },
            {
                .expression =
                    R"cel(['hello', 'world'].transformMapEntry(k, v, {v: k}))cel",
            },
        }),
        ::testing::ValuesIn<TestOptions>({
            {
                .enable_mutable_accumulator = true,
                .max_recursion_depth = 0,
            },
            {
                .enable_mutable_accumulator = false,
                .max_recursion_depth = 0,
            },
            {
                .enable_mutable_accumulator = true,
                .max_recursion_depth = -1,
            },
            {
                .enable_mutable_accumulator = false,
                .max_recursion_depth = -1,
            },
        })));

}  // namespace
}  // namespace cel::extensions
