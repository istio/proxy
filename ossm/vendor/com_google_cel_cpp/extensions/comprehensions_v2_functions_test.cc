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

#include "extensions/comprehensions_v2_functions.h"

#include <memory>
#include <string>
#include <utility>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/source.h"
#include "common/value_testing.h"
#include "extensions/bindings_ext.h"
#include "extensions/comprehensions_v2_macros.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/strings.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/standard_macros.h"
#include "runtime/activation.h"
#include "runtime/optional_types.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::cel::test::BoolValueIs;
using ::google::api::expr::parser::EnrichedParse;
using ::testing::TestWithParam;

struct ComprehensionsV2FunctionsTestCase {
  std::string expression;
};

class ComprehensionsV2FunctionsTest
    : public TestWithParam<ComprehensionsV2FunctionsTestCase> {
 public:
  void SetUp() override {
    RuntimeOptions options;
    options.enable_qualified_type_identifiers = true;
    ASSERT_OK_AND_ASSIGN(auto builder,
                         CreateStandardRuntimeBuilder(
                             internal::GetTestingDescriptorPool(), options));
    ASSERT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
                IsOk());
    ASSERT_THAT(
        RegisterComprehensionsV2Functions(builder.function_registry(), options),
        IsOk());
    ASSERT_THAT(EnableOptionalTypes(builder), IsOk());
    ASSERT_THAT(
        EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways),
        IsOk());
    ASSERT_OK_AND_ASSIGN(runtime_, std::move(builder).Build());
  }

  absl::StatusOr<cel::expr::ParsedExpr> Parse(absl::string_view text) {
    CEL_ASSIGN_OR_RETURN(auto source, NewSource(text));

    ParserOptions options;
    options.enable_optional_syntax = true;

    MacroRegistry registry;
    CEL_RETURN_IF_ERROR(RegisterStandardMacros(registry, options));
    CEL_RETURN_IF_ERROR(RegisterComprehensionsV2Macros(registry, options));
    CEL_RETURN_IF_ERROR(RegisterBindingsMacros(registry, options));

    CEL_ASSIGN_OR_RETURN(auto result,
                         EnrichedParse(*source, registry, options));
    return result.parsed_expr();
  }

 protected:
  std::unique_ptr<const Runtime> runtime_;
};

TEST_P(ComprehensionsV2FunctionsTest, Basic) {
  ASSERT_OK_AND_ASSIGN(auto ast, Parse(GetParam().expression));
  ASSERT_OK_AND_ASSIGN(auto program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime_, ast));
  google::protobuf::Arena arena;
  Activation activation;
  EXPECT_THAT(program->Evaluate(&arena, activation),
              IsOkAndHolds(BoolValueIs(true)))
      << GetParam().expression;
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensionsV2FunctionsTest, ComprehensionsV2FunctionsTest,
    ::testing::ValuesIn<ComprehensionsV2FunctionsTestCase>({
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
        // list.exists()
        {
            .expression =
                R"cel(cel.bind(l, ['hello', 'world', 'hello!', 'worlds'], l.exists(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next.endsWith('world')).orValue(false))))cel",
        },
        // list.existsOne()
        {
            .expression =
                R"cel(cel.bind(l, ['hello', 'world', 'hello!', 'worlds'], l.existsOne(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next.endsWith('world')).orValue(false))))cel",
        },
        {
            .expression =
                R"cel(cel.bind(l, ['hello', 'goodbye', 'hello!', 'goodbye'], l.existsOne(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next == "goodbye").orValue(false))) == false)cel",
        },
        // list.transformList()
        {
            .expression =
                R"cel(['Hello', 'world'].transformList(i, v, "[" + string(i) + "]" + v.lowerAscii()) == ["[0]hello", "[1]world"])cel",
        },
        {
            .expression =
                R"cel(['hello', 'world'].transformList(i, v, v.startsWith('greeting'), "[" + string(i) + "]" + v) == [])cel",
        },
        {
            .expression =
                R"cel([1, 2, 3].transformList(indexVar, valueVar, (indexVar * valueVar) + valueVar) == [1, 4, 9])cel",
        },
        {
            .expression =
                R"cel([1, 2, 3].transformList(indexVar, valueVar, indexVar % 2 == 0, (indexVar * valueVar) + valueVar) == [1, 9])cel",
        },
        // map.transformMap()
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
                R"cel({'Hello': 'world'}.transformList(k, v, k.lowerAscii() + "=" + v) == ["hello=world"])cel",
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
                R"cel({'hello': 'world', 'goodbye': 'cruel world'}.transformMap(k, v, k + ", " + v + "!") == {'hello': 'hello, world!', 'goodbye': 'goodbye, cruel world!'})cel",
        },
        {
            .expression =
                R"cel({'hello': 'world', 'goodbye': 'cruel world'}.transformMap(k, v, v.startsWith('world'), k + ", " + v + "!") == {'hello': 'hello, world!'})cel",
        },
    }));

}  // namespace
}  // namespace cel::extensions
