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

#include "extensions/lists_functions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "common/source.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/standard_macros.h"
#include "runtime/activation.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::cel::expr::Expr;
using ::cel::expr::ParsedExpr;
using ::cel::expr::SourceInfo;
using ::testing::HasSubstr;
using ::testing::ValuesIn;

struct TestInfo {
  std::string expr;
  std::string err = "";
};

class ListsFunctionsTest : public testing::TestWithParam<TestInfo> {};

TEST_P(ListsFunctionsTest, EndToEnd) {
  const TestInfo& test_info = GetParam();
  RecordProperty("cel_expression", test_info.expr);
  if (!test_info.err.empty()) {
    RecordProperty("cel_expected_error", test_info.err);
  }

  ASSERT_OK_AND_ASSIGN(auto source, cel::NewSource(test_info.expr, "<input>"));

  MacroRegistry macro_registry;
  ParserOptions parser_options{.add_macro_calls = true};
  ASSERT_THAT(RegisterStandardMacros(macro_registry, parser_options), IsOk());
  ASSERT_THAT(RegisterListsMacros(macro_registry, parser_options), IsOk());
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       google::api::expr::parser::Parse(*source, macro_registry,
                                                        parser_options));
  Expr expr = parsed_expr.expr();
  SourceInfo source_info = parsed_expr.source_info();

  google::protobuf::Arena arena;
  const auto options = RuntimeOptions{};
  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));

  // Needed to resolve namespaced functions when evaluating a ParsedExpr.
  ASSERT_THAT(cel::EnableReferenceResolver(
                  builder, cel::ReferenceResolverEnabled::kAlways),
              IsOk());
  EXPECT_THAT(RegisterListsFunctions(builder.function_registry(), options),
              IsOk());
  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(Value result, program->Evaluate(&arena, activation));
  if (!test_info.err.empty()) {
    EXPECT_THAT(result,
                ErrorValueIs(StatusIs(testing::_, HasSubstr(test_info.err))));
    return;
  }
  ASSERT_TRUE(result.IsBool())
      << test_info.expr << " -> " << result.DebugString();
  EXPECT_TRUE(result.GetBool().NativeValue())
      << test_info.expr << " -> " << result.DebugString();
}

INSTANTIATE_TEST_SUITE_P(
    ListsFunctionsTest, ListsFunctionsTest,
    testing::ValuesIn<TestInfo>({
        // lists.range()
        {R"cel(lists.range(4) == [0,1,2,3])cel"},
        {R"cel(lists.range(0) == [])cel"},

        // .reverse()
        {R"cel([5,1,2,3].reverse() == [3,2,1,5])cel"},
        {R"cel([] == [])cel"},
        {R"cel([1] == [1])cel"},
        {R"cel(
          ['are', 'you', 'as', 'bored', 'as', 'I', 'am'].reverse()
          == ['am', 'I', 'as', 'bored', 'as', 'you', 'are']
        )cel"},
        {R"cel(
          [false, true, true].reverse().reverse() == [false, true, true]
        )cel"},

        // .slice()
        {R"cel([1,2,3,4].slice(0, 4) == [1,2,3,4])cel"},
        {R"cel([1,2,3,4].slice(0, 0) == [])cel"},
        {R"cel([1,2,3,4].slice(1, 1) == [])cel"},
        {R"cel([1,2,3,4].slice(4, 4) == [])cel"},
        {R"cel([1,2,3,4].slice(1, 3) == [2, 3])cel"},
        {R"cel([1,2,3,4].slice(3, 0))cel",
         "cannot slice(3, 0), start index must be less than or equal to end "
         "index"},
        {R"cel([1,2,3,4].slice(0, 10))cel",
         "cannot slice(0, 10), list is length 4"},
        {R"cel([1,2,3,4].slice(-5, 10))cel",
         "cannot slice(-5, 10), negative indexes not supported"},
        {R"cel([1,2,3,4].slice(-5, -3))cel",
         "cannot slice(-5, -3), negative indexes not supported"},

        // .flatten()
        {R"cel(dyn([]).flatten() == [])cel"},
        {R"cel(dyn([1,2,3,4]).flatten() == [1,2,3,4])cel"},
        {R"cel([1,[2,[3,4]]].flatten() == [1,2,[3,4]])cel"},
        {R"cel([1,2,[],[],[3,4]].flatten() == [1,2,3,4])cel"},
        {R"cel([1,[2,[3,4]]].flatten(2) == [1,2,3,4])cel"},
        {R"cel([1,[2,[3,[4]]]].flatten(-1))cel", "level must be non-negative"},

        // .sort()
        {R"cel([].sort() == [])cel"},
        {R"cel([1].sort() == [1])cel"},
        {R"cel([4, 3, 2, 1].sort() == [1, 2, 3, 4])cel"},
        {R"cel(["d", "a", "b", "c"].sort() == ["a", "b", "c", "d"])cel"},
        {R"cel([b"d", b"a", b"aa"].sort() == [b"a", b"aa", b"d"])cel"},
        {R"cel(
          [1.0, -1.5, 2.0, 1.0, -1.5, -1.5].sort()
          == [-1.5, -1.5, -1.5, 1.0, 1.0, 2.0]
        )cel"},
        {R"cel(
          [42u, 3u, 1337u, 42u, 1337u, 3u, 42u].sort()
          == [3u, 3u, 42u, 42u, 42u, 1337u, 1337u]
        )cel"},
        {R"cel([false, true, false].sort() == [false, false, true])cel"},
        {R"cel(
          [
            timestamp('2024-01-03T00:00:00Z'),
            timestamp('2024-01-01T00:00:00Z'),
            timestamp('2024-01-02T00:00:00Z'),
          ].sort() == [
            timestamp('2024-01-01T00:00:00Z'),
            timestamp('2024-01-02T00:00:00Z'),
            timestamp('2024-01-03T00:00:00Z'),
          ]
        )cel"},
        {R"cel(
          [duration('1m'), duration('2s'), duration('3h')].sort()
          == [duration('2s'), duration('1m'), duration('3h')]
        )cel"},
        {R"cel(["d", 3, 2, "c"].sort())cel",
         "list elements must have the same type"},
        {R"cel([google.api.expr.runtime.TestMessage{}].sort())cel",
         "unsupported type google.api.expr.runtime.TestMessage"},
        {R"cel([[1], [2]].sort())cel", "unsupported type list"},

        // .sortBy()
        {R"cel([].sortBy(e, e) == [])cel"},
        {R"cel(["a"].sortBy(e, e) == ["a"])cel"},
        {R"cel(
          [-3, 1, -5, -2, 4].sortBy(e, -(e * e)) == [-5, 4, -3, -2, 1]
        )cel"},
        {R"cel(
          [-3, 1, -5, -2, 4].map(e, e * 2).sortBy(e, -(e * e))
          == [-10, 8, -6, -4, 2]
        )cel"},
        {R"cel(lists.range(3).sortBy(e, -e) == [2, 1, 0])cel"},
        {R"cel(
          ["a", "c", "b", "first"].sortBy(e, e == "first" ? "" : e)
          == ["first", "a", "b", "c"]
        )cel"},
        {R"cel(
          [
            google.api.expr.runtime.TestMessage{string_value: 'foo'},
            google.api.expr.runtime.TestMessage{string_value: 'bar'},
            google.api.expr.runtime.TestMessage{string_value: 'baz'}
          ].sortBy(e, e.string_value) == [
            google.api.expr.runtime.TestMessage{string_value: 'bar'},
            google.api.expr.runtime.TestMessage{string_value: 'baz'},
            google.api.expr.runtime.TestMessage{string_value: 'foo'}
          ]
        )cel"},
        {R"cel([[2], [1], [3]].sortBy(e, e[0]) == [[1], [2], [3]])cel"},
        {R"cel([[1], ["a"]].sortBy(e, e[0]))cel",
         "list elements must have the same type"},
        {R"cel([[1], [2]].sortBy(e, e))cel", "unsupported type list"},
        {R"cel([google.api.expr.runtime.TestMessage{}].sortBy(e, e))cel",
         "unsupported type google.api.expr.runtime.TestMessage"},

        // .distinct()
        {R"cel([].distinct() == [])cel"},
        {R"cel([1].distinct() == [1])cel"},
        {R"cel([-2, 5, -2, 1, 1, 5, -2, 1].distinct() == [-2, 5, 1])cel"},
        {R"cel(
          [2u, 5u, 100u, 1u, 1u, 5u, 2u, 1u].distinct() == [2u, 5u, 100u, 1u]
        )cel"},
        {R"cel([false, true, true, false].distinct() == [false, true])cel"},
        {R"cel(
          ['c', 'a', 'a', 'b', 'a', 'b', 'c', 'c'].distinct()
          == ['c', 'a', 'b']
        )cel"},
        {R"cel([1, 2.0, "c", 3, "c", 1].distinct() == [1, 2.0, "c", 3])cel"},
        {R"cel([1, 1.0, 2].distinct() == [1, 2])cel"},
        {R"cel([1, 1u].distinct() == [1])cel"},
        {R"cel([[1], [1], [2]].distinct() == [[1], [2]])cel"},
        {R"cel(
          [
            google.api.expr.runtime.TestMessage{string_value: 'a'},
            google.api.expr.runtime.TestMessage{string_value: 'b'},
            google.api.expr.runtime.TestMessage{string_value: 'a'}
          ].distinct() == [
            google.api.expr.runtime.TestMessage{string_value: 'a'},
            google.api.expr.runtime.TestMessage{string_value: 'b'}
          ]
        )cel"},
        {R"cel(
          [
            google.api.expr.runtime.TestMessage{string_value: 'a'},
            1,
            42.0,
            [1, 2, 3],
            false,
          ].distinct() == [
            google.api.expr.runtime.TestMessage{string_value: 'a'},
            1,
            42.0,
            [1, 2, 3],
            false,
          ]
        )cel"},
    }));

TEST(ListsFunctionsTest, ListSortByMacroParseError) {
  ASSERT_OK_AND_ASSIGN(auto source,
                       cel::NewSource("100.sortBy(e, e)", "<input>"));
  MacroRegistry macro_registry;
  ParserOptions parser_options{.add_macro_calls = true};
  ASSERT_THAT(RegisterListsMacros(macro_registry, parser_options), IsOk());
  EXPECT_THAT(
      google::api::expr::parser::Parse(*source, macro_registry, parser_options),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("sortBy can only be applied to")));
}

struct ListCheckerTestCase {
  std::string expr;
  std::string error_substr;
};

class ListsCheckerLibraryTest
    : public ::testing::TestWithParam<ListCheckerTestCase> {
 public:
  void SetUp() override {
    // Arrange: Configure the compiler.
    // Add the lists checker library to the compiler builder.
    ASSERT_OK_AND_ASSIGN(
        std::unique_ptr<CompilerBuilder> compiler_builder,
        NewCompilerBuilder(internal::GetTestingDescriptorPool()));
    ASSERT_THAT(compiler_builder->AddLibrary(StandardCompilerLibrary()),
                IsOk());
    ASSERT_THAT(compiler_builder->AddLibrary(ListsCompilerLibrary()), IsOk());
    compiler_builder->GetCheckerBuilder().set_container(
        "cel.expr.conformance.proto3");
    ASSERT_OK_AND_ASSIGN(compiler_, std::move(*compiler_builder).Build());
  }

  std::unique_ptr<Compiler> compiler_;
};

TEST_P(ListsCheckerLibraryTest, ListsFunctionsTypeCheckerSuccess) {
  // Act & Assert: Compile the expression and validate the result.
  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       compiler_->Compile(GetParam().expr));
  absl::string_view error_substr = GetParam().error_substr;
  EXPECT_EQ(result.IsValid(), error_substr.empty());

  if (!error_substr.empty()) {
    EXPECT_THAT(result.FormatError(), HasSubstr(error_substr));
  }
}

// Returns a vector of test cases for the ListsCheckerLibraryTest.
// Returns both positive and negative test cases for the lists functions.
std::vector<ListCheckerTestCase> createListsCheckerParams() {
  return {
      // lists.distinct()
      {R"([1,2,3,4,4].distinct() == [1,2,3,4])"},
      {R"('abc'.distinct() == [1,2,3,4])",
       "no matching overload for 'distinct'"},
      {R"([1,2,3,4,4].distinct() == 'abc')", "no matching overload for '_==_'"},
      {R"([1,2,3,4,4].distinct(1) == [1,2,3,4])", "undeclared reference"},
      // lists.flatten()
      {R"([1,2,3,4].flatten() == [1,2,3,4])"},
      {R"([1,2,3,4].flatten(1) == [1,2,3,4])"},
      {R"('abc'.flatten() == [1,2,3,4])", "no matching overload for 'flatten'"},
      {R"([1,2,3,4].flatten() == 'abc')", "no matching overload for '_==_'"},
      {R"('abc'.flatten(1) == [1,2,3,4])",
       "no matching overload for 'flatten'"},
      {R"([1,2,3,4].flatten('abc') == [1,2,3,4])",
       "no matching overload for 'flatten'"},
      {R"([1,2,3,4].flatten(1) == 'abc')", "no matching overload"},
      // lists.range()
      {R"(lists.range(4) == [0,1,2,3])"},
      {R"(lists.range('abc') == [])", "no matching overload for 'lists.range'"},
      {R"(lists.range(4) == 'abc')", "no matching overload for '_==_'"},
      {R"(lists.range(4, 4) == [0,1,2,3])", "undeclared reference"},
      // lists.reverse()
      {R"([1,2,3,4].reverse() == [4,3,2,1])"},
      {R"('abc'.reverse() == [])", "no matching overload for 'reverse'"},
      {R"([1,2,3,4].reverse() == 'abc')", "no matching overload for '_==_'"},
      {R"([1,2,3,4].reverse(1) == [4,3,2,1])", "undeclared reference"},
      // lists.slice()
      {R"([1,2,3,4].slice(0, 4) == [1,2,3,4])"},
      {R"('abc'.slice(0, 4) == [1,2,3,4])", "no matching overload for 'slice'"},
      {R"([1,2,3,4].slice('abc', 4) == [1,2,3,4])",
       "no matching overload for 'slice'"},
      {R"([1,2,3,4].slice(0, 'abc') == [1,2,3,4])",
       "no matching overload for 'slice'"},
      {R"([1,2,3,4].slice(0, 4) == 'abc')", "no matching overload for '_==_'"},
      {R"([1,2,3,4].slice(0, 2, 3) == [1,2,3,4])", "undeclared reference"},
      // lists.sort()
      {R"([1,2,3,4].sort() == [1,2,3,4])"},
      {R"([TestAllTypes{}, TestAllTypes{}].sort() == [])",
       "no matching overload for 'sort'"},
      {R"('abc'.sort() == [])", "no matching overload for 'sort'"},
      {R"([1,2,3,4].sort() == 'abc')", "no matching overload for '_==_'"},
      {R"([1,2,3,4].sort(2) == [1,2,3,4])", "undeclared reference"},
      // sortBy macro
      {R"([1,2,3,4].sortBy(x, -x) == [4,3,2,1])"},
      {R"([TestAllTypes{}, TestAllTypes{}].sortBy(x, x) == [])",
       "no matching overload for '@sortByAssociatedKeys'"},
      {R"(
        [TestAllTypes{single_int64: 2}, TestAllTypes{single_int64: 1}]
          .sortBy(x, x.single_int64) ==
        [TestAllTypes{single_int64: 1}, TestAllTypes{single_int64: 2}])"},
  };
}

INSTANTIATE_TEST_SUITE_P(ListsCheckerLibraryTest, ListsCheckerLibraryTest,
                         ValuesIn(createListsCheckerParams()));

}  // namespace
}  // namespace cel::extensions
