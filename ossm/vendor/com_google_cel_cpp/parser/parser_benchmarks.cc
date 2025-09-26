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

#include <string>
#include <thread>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "internal/benchmark.h"
#include "internal/testing.h"
#include "parser/macro.h"
#include "parser/options.h"
#include "parser/parser.h"

namespace google::api::expr::parser {

namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

enum class ParseResult { kSuccess, kError };

struct TestInfo {
  static TestInfo ErrorCase(absl::string_view expr) {
    TestInfo info;
    info.expr = expr;
    info.result = ParseResult::kError;
    return info;
  }
  // The expression to parse.
  std::string expr = "";

  // The expected result of the parse.
  ParseResult result = ParseResult::kSuccess;
};

const std::vector<TestInfo>& GetTestCases() {
  static const std::vector<TestInfo>* kInstance = new std::vector<TestInfo>{
      // Simple test cases we started with
      {"x * 2"},
      {"x * 2u"},
      {"x * 2.0"},
      {"\"\\u2764\""},
      {"\"\u2764\""},
      {"! false"},
      {"-a"},
      {"a.b(5)"},
      {"a[3]"},
      {"SomeMessage{foo: 5, bar: \"xyz\"}"},
      {"[3, 4, 5]"},
      {"{foo: 5, bar: \"xyz\"}"},
      {"a > 5 && a < 10"},
      {"a < 5 || a > 10"},
      TestInfo::ErrorCase("{"),

      // test cases from Go
      {"\"A\""},
      {"true"},
      {"false"},
      {"0"},
      {"42"},
      {"0u"},
      {"23u"},
      {"24u"},
      {"0xAu"},
      {"-0xA"},
      {"0xA"},
      {"-1"},
      {"4--4"},
      {"4--4.1"},
      {"b\"abc\""},
      {"23.39"},
      {"!a"},
      {"a"},
      {"a?b:c"},
      {"a || b"},
      {"a || b || c || d || e || f "},
      {"a && b"},
      {"a && b && c && d && e && f && g"},
      {"a && b && c && d || e && f && g && h"},
      {"a + b"},
      {"a - b"},
      {"a * b"},
      {"a / b"},
      {"a % b"},
      {"a in b"},
      {"a == b"},
      {"a != b"},
      {"a > b"},
      {"a >= b"},
      {"a < b"},
      {"a <= b"},
      {"a.b"},
      {"a.b.c"},
      {"a[b]"},
      {"foo{ }"},
      {"foo{ a:b }"},
      {"foo{ a:b, c:d }"},
      {"{}"},
      {"{a:b, c:d}"},
      {"[]"},
      {"[a]"},
      {"[a, b, c]"},
      {"(a)"},
      {"((a))"},
      {"a()"},
      {"a(b)"},
      {"a(b, c)"},
      {"a.b()"},
      {"a.b(c)"},
      {"aaa.bbb(ccc)"},

      // Parse error tests
      TestInfo::ErrorCase("*@a | b"),
      TestInfo::ErrorCase("a | b"),
      TestInfo::ErrorCase("?"),
      TestInfo::ErrorCase("t{>C}"),

      // Macro tests
      {"has(m.f)"},
      {"m.exists_one(v, f)"},
      {"m.map(v, f)"},
      {"m.map(v, p, f)"},
      {"m.filter(v, p)"},

      // Tests from Java parser
      {"[] + [1,2,3,] + [4]"},
      {"{1:2u, 2:3u}"},
      {"TestAllTypes{single_int32: 1, single_int64: 2}"},

      TestInfo::ErrorCase("TestAllTypes(){single_int32: 1, single_int64: 2}"),
      {"size(x) == x.size()"},
      TestInfo::ErrorCase("1 + $"),
      TestInfo::ErrorCase("1 + 2\n"
                          "3 +"),
      {"\"\\\"\""},
      {"[1,3,4][0]"},
      TestInfo::ErrorCase("1.all(2, 3)"),
      {"x[\"a\"].single_int32 == 23"},
      {"x.single_nested_message != null"},
      {"false && !true || false ? 2 : 3"},
      {"b\"abc\" + B\"def\""},
      {"1 + 2 * 3 - 1 / 2 == 6 % 1"},
      {"---a"},
      TestInfo::ErrorCase("1 + +"),
      {"\"abc\" + \"def\""},
      TestInfo::ErrorCase("{\"a\": 1}.\"a\""),
      {"\"\\xC3\\XBF\""},
      {"\"\\303\\277\""},
      {"\"hi\\u263A \\u263Athere\""},
      {"\"\\U000003A8\\?\""},
      {"\"\\a\\b\\f\\n\\r\\t\\v'\\\"\\\\\\? Legal escapes\""},
      TestInfo::ErrorCase("\"\\xFh\""),
      TestInfo::ErrorCase(
          "\"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\""),
      {"'😁' in ['😁', '😑', '😦']"},
      {"'\u00ff' in ['\u00ff', '\u00ff', '\u00ff']"},
      {"'\u00ff' in ['\uffff', '\U00100000', '\U0010ffff']"},
      {"'\u00ff' in ['\U00100000', '\uffff', '\U0010ffff']"},
      TestInfo::ErrorCase("'😁' in ['😁', '😑', '😦']\n"
                          "   && in.😁"),
      TestInfo::ErrorCase("as"),
      TestInfo::ErrorCase("break"),
      TestInfo::ErrorCase("const"),
      TestInfo::ErrorCase("continue"),
      TestInfo::ErrorCase("else"),
      TestInfo::ErrorCase("for"),
      TestInfo::ErrorCase("function"),
      TestInfo::ErrorCase("if"),
      TestInfo::ErrorCase("import"),
      TestInfo::ErrorCase("in"),
      TestInfo::ErrorCase("let"),
      TestInfo::ErrorCase("loop"),
      TestInfo::ErrorCase("package"),
      TestInfo::ErrorCase("namespace"),
      TestInfo::ErrorCase("return"),
      TestInfo::ErrorCase("var"),
      TestInfo::ErrorCase("void"),
      TestInfo::ErrorCase("while"),
      TestInfo::ErrorCase("[1, 2, 3].map(var, var * var)"),
      TestInfo::ErrorCase("[\n\t\r[\n\t\r[\n\t\r]\n\t\r]\n\t\r"),

      // Identifier quoting syntax tests.
      {"a.`b`"},
      {"a.`b-c`"},
      {"a.`b c`"},
      {"a.`b/c`"},
      {"a.`b.c`"},
      {"a.`in`"},
      {"A{`b`: 1}"},
      {"A{`b-c`: 1}"},
      {"A{`b c`: 1}"},
      {"A{`b/c`: 1}"},
      {"A{`b.c`: 1}"},
      {"A{`in`: 1}"},
      {"has(a.`b/c`)"},
      // Unsupported quoted identifiers.
      TestInfo::ErrorCase("a.`b\tc`"),
      TestInfo::ErrorCase("a.`@foo`"),
      TestInfo::ErrorCase("a.`$foo`"),
      TestInfo::ErrorCase("`a.b`"),
      TestInfo::ErrorCase("`a.b`()"),
      TestInfo::ErrorCase("foo.`a.b`()"),
      // Macro calls tests
      {"x.filter(y, y.filter(z, z > 0))"},
      {"has(a.b).filter(c, c)"},
      {"x.filter(y, y.exists(z, has(z.a)) && y.exists(z, has(z.b)))"},
      {"has(a.b).asList().exists(c, c)"},
      TestInfo::ErrorCase("b'\\UFFFFFFFF'"),
      {"a.?b[?0] && a[?c]"},
      {"{?'key': value}"},
      {"[?a, ?b]"},
      {"[?a[?b]]"},
      {"Msg{?field: value}"},
      {"m.optMap(v, f)"},
      {"m.optFlatMap(v, f)"}};
  return *kInstance;
}

class BenchmarkCaseTest : public testing::TestWithParam<TestInfo> {};

TEST_P(BenchmarkCaseTest, ExpectedResult) {
  std::vector<Macro> macros = Macro::AllMacros();
  macros.push_back(cel::OptMapMacro());
  macros.push_back(cel::OptFlatMapMacro());
  const TestInfo& test_info = GetParam();
  ParserOptions options;
  options.enable_optional_syntax = true;
  options.enable_quoted_identifiers = true;

  auto result = EnrichedParse(test_info.expr, macros, "<input>", options);
  switch (test_info.result) {
    case ParseResult::kSuccess:
      ASSERT_THAT(result, IsOk());
      break;
    case ParseResult::kError:
      ASSERT_THAT(result, Not(IsOk()));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(CelParserTest, BenchmarkCaseTest,
                         testing::ValuesIn(GetTestCases()));

// This is not a proper microbenchmark, but is used to check for major
// regressions in the ANTLR generated code or concurrency issues. Each benchmark
// iteration parses all of the basic test cases from the unit-tests.
void BM_Parse(benchmark::State& state) {
  std::vector<Macro> macros = Macro::AllMacros();
  macros.push_back(cel::OptMapMacro());
  macros.push_back(cel::OptFlatMapMacro());
  ParserOptions options;
  options.enable_optional_syntax = true;
  options.enable_quoted_identifiers = true;
  for (auto s : state) {
    for (const auto& test_case : GetTestCases()) {
      auto result = ParseWithMacros(test_case.expr, macros, "<input>", options);
      ABSL_DCHECK_EQ(result.ok(), test_case.result == ParseResult::kSuccess);
      benchmark::DoNotOptimize(result);
    }
  }
}

BENCHMARK(BM_Parse)->ThreadRange(1, std::thread::hardware_concurrency());

}  // namespace
}  // namespace google::api::expr::parser
