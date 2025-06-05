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

#include <memory>
#include <string>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "extensions/bindings_ext.h"
#include "internal/benchmark.h"
#include "internal/testing.h"
#include "parser/macro.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::parser::ParseWithMacros;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::test::CelValueMatcher;
using ::google::api::expr::runtime::test::IsCelBool;
using ::google::api::expr::runtime::test::IsCelString;

struct BenchmarkCase {
  std::string name;
  std::string expression;
  CelValueMatcher matcher;
};

const std::vector<BenchmarkCase>& BenchmarkCases() {
  static absl::NoDestructor<std::vector<BenchmarkCase>> cases(
      std::vector<BenchmarkCase>{
          {"simple", R"(cel.bind(x, "ab", x))", IsCelString("ab")},
          {"multiple_references", R"(cel.bind(x, "ab", x + x + x + x))",
           IsCelString("abababab")},
          {"nested",
           R"(
            cel.bind(
              x,
              "ab",
              cel.bind(
                y,
                "cd",
                x + y + "ef")))",
           IsCelString("abcdef")},
          {"nested_defintion",
           R"(
            cel.bind(
              x,
              "ab",
              cel.bind(
                y,
                x + "cd",
                y + "ef"
              )))",
           IsCelString("abcdef")},
          {"bind_outside_loop",
           R"(
            cel.bind(
              outer_value,
              [1, 2, 3],
              [3, 2, 1].all(
                value,
                value in outer_value)
              ))",
           IsCelBool(true)},
          {"bind_inside_loop",
           R"(
              [3, 2, 1].all(
                x,
                cel.bind(value, x * x, value < 16)
              ))",
           IsCelBool(true)},
          {"bind_loop_bind",
           R"(
            cel.bind(
              outer_value,
              {1: 2, 2: 3, 3: 4},
              outer_value.all(
                key,
                cel.bind(
                  value,
                  outer_value[key],
                  value == key + 1
                )
              )))",
           IsCelBool(true)},
          {"ternary_depends_on_bind",
           R"(
            cel.bind(
              a,
              "ab",
              (true && a.startsWith("c")) ? a : "cd"
            ))",
           IsCelString("cd")},
          {"ternary_does_not_depend_on_bind",
           R"(
            cel.bind(
              a,
              "ab",
              (false && a.startsWith("c")) ? a : "cd"
            ))",
           IsCelString("cd")},
          {"twice_nested_defintion",
           R"(
            cel.bind(
              x,
              "ab",
              cel.bind(
                y,
                x + "cd",
                cel.bind(
                  z,
                  y + "ef",
                  z)))
             )",
           IsCelString("abcdef")},
      });

  return *cases;
}

class BindingsBenchmarkTest : public ::testing::TestWithParam<BenchmarkCase> {
 protected:
  google::protobuf::Arena arena_;
};

TEST_P(BindingsBenchmarkTest, CheckBenchmarkCaseWorks) {
  const BenchmarkCase& benchmark = GetParam();

  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  ASSERT_OK_AND_ASSIGN(
      auto expr, ParseWithMacros(benchmark.expression, all_macros, "<input>"));

  InterpreterOptions options;
  auto builder =
      google::api::expr::runtime::CreateCelExpressionBuilder(options);

  ASSERT_OK(google::api::expr::runtime::RegisterBuiltinFunctions(
      builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(auto program, builder->CreateExpression(
                                         &expr.expr(), &expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, program->Evaluate(activation, &arena));

  EXPECT_THAT(result, benchmark.matcher);
}

void RunBenchmark(const BenchmarkCase& benchmark, benchmark::State& state) {
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  ASSERT_OK_AND_ASSIGN(
      auto expr, ParseWithMacros(benchmark.expression, all_macros, "<input>"));

  InterpreterOptions options;
  auto builder =
      google::api::expr::runtime::CreateCelExpressionBuilder(options);

  ASSERT_OK(google::api::expr::runtime::RegisterBuiltinFunctions(
      builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(auto program, builder->CreateExpression(
                                         &expr.expr(), &expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  for (auto _ : state) {
    auto result = program->Evaluate(activation, &arena);
    benchmark::DoNotOptimize(result);
    ABSL_DCHECK_OK(result);
    ABSL_DCHECK(benchmark.matcher.Matches(*result));
  }
}

void BM_Simple(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[0], state);
}
void BM_MultipleReferences(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[1], state);
}
void BM_Nested(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[2], state);
}
void BM_NestedDefinition(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[3], state);
}
void BM_BindOusideLoop(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[4], state);
}
void BM_BindInsideLoop(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[5], state);
}
void BM_BindLoopBind(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[6], state);
}
void BM_TernaryDependsOnBind(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[7], state);
}
void BM_TernaryDoesNotDependOnBind(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[8], state);
}
void BM_TwiceNestedDefinition(benchmark::State& state) {
  RunBenchmark(BenchmarkCases()[9], state);
}

BENCHMARK(BM_Simple);
BENCHMARK(BM_MultipleReferences);
BENCHMARK(BM_Nested);
BENCHMARK(BM_NestedDefinition);
BENCHMARK(BM_BindOusideLoop);
BENCHMARK(BM_BindInsideLoop);
BENCHMARK(BM_BindLoopBind);
BENCHMARK(BM_TernaryDependsOnBind);
BENCHMARK(BM_TernaryDoesNotDependOnBind);
BENCHMARK(BM_TwiceNestedDefinition);

INSTANTIATE_TEST_SUITE_P(BindingsBenchmarkTest, BindingsBenchmarkTest,
                         ::testing::ValuesIn(BenchmarkCases()));

}  // namespace
}  // namespace cel::extensions
