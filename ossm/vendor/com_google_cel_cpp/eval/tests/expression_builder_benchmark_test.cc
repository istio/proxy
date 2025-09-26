/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/minimal_descriptor_pool.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_type_registry.h"
#include "eval/tests/request_context.pb.h"
#include "internal/benchmark.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using cel::expr::CheckedExpr;
using cel::expr::ParsedExpr;
using google::api::expr::parser::Parse;

enum BenchmarkParam : int {
  kDefault = 0,
  kFoldConstants = 1,
};

void BM_RegisterBuiltins(benchmark::State& state) {
  for (auto _ : state) {
    auto builder = CreateCelExpressionBuilder();
    auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
    ASSERT_OK(reg_status);
  }
}

BENCHMARK(BM_RegisterBuiltins);

InterpreterOptions OptionsForParam(BenchmarkParam param, google::protobuf::Arena& arena) {
  InterpreterOptions options;

  switch (param) {
    case BenchmarkParam::kFoldConstants:
      options.constant_arena = &arena;
      options.constant_folding = true;
      break;
    case BenchmarkParam::kDefault:
      options.constant_folding = false;
      break;
  }
  return options;
}

void BM_SymbolicPolicy(benchmark::State& state) {
  auto param = static_cast<BenchmarkParam>(state.range(0));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"cel(
   !(request.ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((request.path.startsWith("v1") && request.token in ["v1", "v2", "admin"]) ||
    (request.path.startsWith("v2") && request.token in ["v2", "admin"]) ||
    (request.path.startsWith("/admin") && request.token == "admin" &&
     request.ip in ["10.0.1.1",  "10.0.1.2", "10.0.1.3"])
   ))cel"));

  google::protobuf::Arena arena;
  InterpreterOptions options = OptionsForParam(param, arena);

  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    arena.Reset();
  }
}

BENCHMARK(BM_SymbolicPolicy)
    ->Arg(BenchmarkParam::kDefault)
    ->Arg(BenchmarkParam::kFoldConstants);

absl::StatusOr<std::unique_ptr<CelExpressionBuilder>> MakeBuilderForEnums(
    absl::string_view container, absl::string_view enum_type,
    int num_enum_values) {
  auto builder =
      CreateCelExpressionBuilder(cel::GetMinimalDescriptorPool(), nullptr, {});
  builder->set_container(std::string(container));
  CelTypeRegistry* type_registry = builder->GetTypeRegistry();
  std::vector<CelTypeRegistry::Enumerator> enumerators;
  enumerators.reserve(num_enum_values);
  for (int i = 0; i < num_enum_values; ++i) {
    enumerators.push_back(
        CelTypeRegistry::Enumerator{absl::StrCat("ENUM_VALUE_", i), i});
  }
  type_registry->RegisterEnum(enum_type, std::move(enumerators));

  CEL_RETURN_IF_ERROR(RegisterBuiltinFunctions(builder->GetRegistry()));
  return builder;
}

void BM_EnumResolutionSimple(benchmark::State& state) {
  static const CelExpressionBuilder* builder = []() {
    auto builder = MakeBuilderForEnums("", "com.example.TestEnum", 4);
    ABSL_CHECK_OK(builder.status());
    return builder->release();
  }();

  ASSERT_OK_AND_ASSIGN(auto expr, Parse("com.example.TestEnum.ENUM_VALUE_0"));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    benchmark::DoNotOptimize(expression);
  }
}

BENCHMARK(BM_EnumResolutionSimple)->ThreadRange(1, 32);

void BM_EnumResolutionContainer(benchmark::State& state) {
  static const CelExpressionBuilder* builder = []() {
    auto builder =
        MakeBuilderForEnums("com.example", "com.example.TestEnum", 4);
    ABSL_CHECK_OK(builder.status());
    return builder->release();
  }();

  ASSERT_OK_AND_ASSIGN(auto expr, Parse("TestEnum.ENUM_VALUE_0"));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    benchmark::DoNotOptimize(expression);
  }
}

BENCHMARK(BM_EnumResolutionContainer)->ThreadRange(1, 32);

void BM_EnumResolution32Candidate(benchmark::State& state) {
  static const CelExpressionBuilder* builder = []() {
    auto builder =
        MakeBuilderForEnums("com.example.foo", "com.example.foo.TestEnum", 8);
    ABSL_CHECK_OK(builder.status());
    return builder->release();
  }();

  ASSERT_OK_AND_ASSIGN(auto expr,
                       Parse("com.example.foo.TestEnum.ENUM_VALUE_0"));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    benchmark::DoNotOptimize(expression);
  }
}

BENCHMARK(BM_EnumResolution32Candidate)->ThreadRange(1, 32);

void BM_EnumResolution256Candidate(benchmark::State& state) {
  static const CelExpressionBuilder* builder = []() {
    auto builder =
        MakeBuilderForEnums("com.example.foo", "com.example.foo.TestEnum", 64);
    ABSL_CHECK_OK(builder.status());
    return builder->release();
  }();

  ASSERT_OK_AND_ASSIGN(auto expr,
                       Parse("com.example.foo.TestEnum.ENUM_VALUE_0"));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    benchmark::DoNotOptimize(expression);
  }
}

BENCHMARK(BM_EnumResolution256Candidate)->ThreadRange(1, 32);

void BM_NestedComprehension(benchmark::State& state) {
  auto param = static_cast<BenchmarkParam>(state.range(0));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"(
    [4, 5, 6].all(x, [1, 2, 3].all(y, x > y) && [7, 8, 9].all(z, x < z))
  )"));

  google::protobuf::Arena arena;
  InterpreterOptions options = OptionsForParam(param, arena);

  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    arena.Reset();
  }
}

BENCHMARK(BM_NestedComprehension)
    ->Arg(BenchmarkParam::kDefault)
    ->Arg(BenchmarkParam::kFoldConstants);

void BM_Comparisons(benchmark::State& state) {
  auto param = static_cast<BenchmarkParam>(state.range(0));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"(
    v11 < v12 && v12 < v13
      && v21 > v22 && v22 > v23
      && v31 == v32 && v32 == v33
      && v11 != v12 && v12 != v13
  )"));

  google::protobuf::Arena arena;
  InterpreterOptions options = OptionsForParam(param, arena);

  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    arena.Reset();
  }
}

BENCHMARK(BM_Comparisons)
    ->Arg(BenchmarkParam::kDefault)
    ->Arg(BenchmarkParam::kFoldConstants);

void BM_ComparisonsConcurrent(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"(
    v11 < v12 && v12 < v13
      && v21 > v22 && v22 > v23
      && v31 == v32 && v32 == v33
      && v11 != v12 && v12 != v13
  )"));

  static const CelExpressionBuilder* builder = [] {
    InterpreterOptions options;
    auto builder = CreateCelExpressionBuilder(options);
    auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
    ABSL_CHECK_OK(reg_status);
    return builder.release();
  }();

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    benchmark::DoNotOptimize(expression);
  }
}

BENCHMARK(BM_ComparisonsConcurrent)->ThreadRange(1, 32);

void RegexPrecompilationBench(bool enabled, benchmark::State& state) {
  auto param = static_cast<BenchmarkParam>(state.range(0));

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"cel(
    input_str.matches(r'192\.168\.' + '[0-9]{1,3}' + r'\.' + '[0-9]{1,3}') ||
    input_str.matches(r'10(\.[0-9]{1,3}){3}')
  )cel"));

  // Fake a checked expression with enough reference information for the expr
  // builder to identify the regex as optimize-able.
  CheckedExpr checked_expr;
  checked_expr.mutable_expr()->Swap(expr.mutable_expr());
  checked_expr.mutable_source_info()->Swap(expr.mutable_source_info());
  (*checked_expr.mutable_reference_map())[2].add_overload_id("matches_string");
  (*checked_expr.mutable_reference_map())[11].add_overload_id("matches_string");

  google::protobuf::Arena arena;
  InterpreterOptions options = OptionsForParam(param, arena);
  options.enable_regex_precompilation = enabled;

  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(auto expression,
                         builder->CreateExpression(&checked_expr));
    arena.Reset();
  }
}

void BM_RegexPrecompilationDisabled(benchmark::State& state) {
  RegexPrecompilationBench(false, state);
}

BENCHMARK(BM_RegexPrecompilationDisabled)
    ->Arg(BenchmarkParam::kDefault)
    ->Arg(BenchmarkParam::kFoldConstants);

void BM_RegexPrecompilationEnabled(benchmark::State& state) {
  RegexPrecompilationBench(true, state);
}

BENCHMARK(BM_RegexPrecompilationEnabled)
    ->Arg(BenchmarkParam::kDefault)
    ->Arg(BenchmarkParam::kFoldConstants);

void BM_StringConcat(benchmark::State& state) {
  auto param = static_cast<BenchmarkParam>(state.range(0));
  auto size = state.range(1);

  std::string source = "'1234567890' + '1234567890'";
  auto height = static_cast<int>(std::log2(size));
  for (int i = 1; i < height; i++) {
    // Force the parse to be a binary tree, otherwise we can hit
    // recursion limits.
    source = absl::StrCat("(", source, " + ", source, ")");
  }

  // add a non const branch to the expression.
  absl::StrAppend(&source, " + identifier");

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(source));

  google::protobuf::Arena arena;
  InterpreterOptions options = OptionsForParam(param, arena);

  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    arena.Reset();
  }
}

BENCHMARK(BM_StringConcat)
    ->Args({BenchmarkParam::kDefault, 2})
    ->Args({BenchmarkParam::kDefault, 4})
    ->Args({BenchmarkParam::kDefault, 8})
    ->Args({BenchmarkParam::kDefault, 16})
    ->Args({BenchmarkParam::kDefault, 32})
    ->Args({BenchmarkParam::kFoldConstants, 2})
    ->Args({BenchmarkParam::kFoldConstants, 4})
    ->Args({BenchmarkParam::kFoldConstants, 8})
    ->Args({BenchmarkParam::kFoldConstants, 16})
    ->Args({BenchmarkParam::kFoldConstants, 32});

void BM_StringConcat32Concurrent(benchmark::State& state) {
  std::string source = "'1234567890' + '1234567890'";
  auto height = static_cast<int>(std::log2(32));
  for (int i = 1; i < height; i++) {
    // Force the parse to be a binary tree, otherwise we can hit
    // recursion limits.
    source = absl::StrCat("(", source, " + ", source, ")");
  }

  // add a non const branch to the expression.
  absl::StrAppend(&source, " + identifier");

  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(source));

  static const CelExpressionBuilder* builder = [] {
    InterpreterOptions options;
    auto builder = CreateCelExpressionBuilder(options);
    auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
    ABSL_CHECK_OK(reg_status);
    return builder.release();
  }();

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
    benchmark::DoNotOptimize(expression);
  }
}

BENCHMARK(BM_StringConcat32Concurrent)->ThreadRange(1, 32);

}  // namespace
}  // namespace google::api::expr::runtime
