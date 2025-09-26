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

#include "extensions/bindings_ext.h"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/attribute.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "internal/testing.h"
#include "parser/macro.h"
#include "parser/parser.h"
#include "cel/expr/conformance/proto2/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::expr::conformance::proto2::NestedTestAllTypes;
using ::cel::expr::CheckedExpr;
using ::cel::expr::Expr;
using ::cel::expr::ParsedExpr;
using ::cel::expr::SourceInfo;
using ::google::api::expr::parser::ParseWithMacros;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelExpressionBuilder;
using ::google::api::expr::runtime::CelFunction;
using ::google::api::expr::runtime::CelFunctionDescriptor;
using ::google::api::expr::runtime::CelProtoWrapper;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::FunctionAdapter;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using ::google::api::expr::runtime::UnknownProcessingOptions;
using ::google::api::expr::runtime::test::IsCelInt64;
using ::google::protobuf::Arena;
using ::google::protobuf::TextFormat;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Pair;

struct TestInfo {
  std::string expr;
  std::string err = "";
};

class TestFunction : public CelFunction {
 public:
  explicit TestFunction(absl::string_view name)
      : CelFunction(CelFunctionDescriptor(
            name, true,
            {CelValue::Type::kBool, CelValue::Type::kBool,
             CelValue::Type::kBool, CelValue::Type::kBool})) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        Arena* arena) const override {
    *result = CelValue::CreateBool(true);
    return absl::OkStatus();
  }
};

// Test function used to test macro collision and non-expansion.
constexpr absl::string_view kBind = "bind";
std::unique_ptr<CelFunction> CreateBindFunction() {
  return std::make_unique<TestFunction>(kBind);
}

class BindingsExtTest
    : public testing::TestWithParam<std::tuple<TestInfo, bool, bool>> {
 protected:
  const TestInfo& GetTestInfo() { return std::get<0>(GetParam()); }
  bool GetEnableConstantFolding() { return std::get<1>(GetParam()); }
  bool GetEnableRecursivePlan() { return std::get<2>(GetParam()); }
};

TEST_P(BindingsExtTest, Default) {
  const TestInfo& test_info = GetTestInfo();
  Arena arena;
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  auto result = ParseWithMacros(test_info.expr, all_macros, "<input>");
  if (!test_info.err.empty()) {
    EXPECT_THAT(result.status(), StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr(test_info.err)));
    return;
  }
  EXPECT_THAT(result, IsOk());

  ParsedExpr parsed_expr = *result;
  Expr expr = parsed_expr.expr();
  SourceInfo source_info = parsed_expr.source_info();

  // Obtain CEL Expression builder.
  InterpreterOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_empty_wrapper_null_unboxing = true;
  options.constant_folding = GetEnableConstantFolding();
  options.constant_arena = &arena;
  options.max_recursion_depth = GetEnableRecursivePlan() ? -1 : 0;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));
  Activation activation;
  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsBool()) << out.DebugString();
  EXPECT_EQ(out.BoolOrDie(), true);
}

TEST_P(BindingsExtTest, Tracing) {
  const TestInfo& test_info = GetTestInfo();
  Arena arena;
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  auto result = ParseWithMacros(test_info.expr, all_macros, "<input>");
  if (!test_info.err.empty()) {
    EXPECT_THAT(result.status(), StatusIs(absl::StatusCode::kInvalidArgument,
                                          HasSubstr(test_info.err)));
    return;
  }
  EXPECT_THAT(result, IsOk());

  ParsedExpr parsed_expr = *result;
  Expr expr = parsed_expr.expr();
  SourceInfo source_info = parsed_expr.source_info();

  // Obtain CEL Expression builder.
  InterpreterOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_empty_wrapper_null_unboxing = true;
  options.constant_folding = GetEnableConstantFolding();
  options.constant_arena = &arena;
  options.max_recursion_depth = GetEnableRecursivePlan() ? -1 : 0;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));
  Activation activation;
  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(
      CelValue out,
      cel_expr->Trace(activation, &arena,
                      [](int64_t, const CelValue&, google::protobuf::Arena*) {
                        return absl::OkStatus();
                      }));
  ASSERT_TRUE(out.IsBool()) << out.DebugString();
  EXPECT_EQ(out.BoolOrDie(), true);
}

INSTANTIATE_TEST_SUITE_P(
    CelBindingsExtTest, BindingsExtTest,
    testing::Combine(
        testing::ValuesIn<TestInfo>(
            {{"cel.bind(t, true, t)"},
             {"cel.bind(msg, \"hello\", msg + msg + msg) == "
              "\"hellohellohello\""},
             {"cel.bind(t1, true, cel.bind(t2, true, t1 && t2))"},
             {"cel.bind(valid_elems, [1, 2, 3], "
              "[3, 4, 5].exists(e, e in valid_elems))"},
             {"cel.bind(valid_elems, [1, 2, 3], "
              "![4, 5].exists(e, e in valid_elems))"},
             // Implementation detail: bind variables and comprehension
             // variables get mapped to an int index in the same space. Check
             // that mixing them works.
             {R"(
              cel.bind(
                  my_list,
                  ['a', 'b', 'c'].map(x, x + '_'),
                  [0, 1, 2].map(y, my_list[y] + string(y))) ==
              ['a_0', 'b_1', 'c_2'])"},
             // Check scoping rules.
             {"cel.bind(x, 1, "
              "  cel.bind(x, x + 1, x)) == 2"},
             // Testing a bound function with the same macro name, but non-cel
             // namespace. The function mirrors the macro signature, but just
             // returns true.
             {"false.bind(false, false, false)"},
             // Error case where the variable name is not a simple identifier.
             {"cel.bind(bad.name, true, bad.name)",
              "variable name must be a simple identifier"}}),
        /*constant_folding*/ testing::Bool(),
        /*recursive_plan*/ testing::Bool()));

constexpr absl::string_view kTraceExpr = R"pb(
  expr: {
    id: 11
    comprehension_expr: {
      iter_var: "#unused"
      iter_range: {
        id: 8
        list_expr: {}
      }
      accu_var: "x"
      accu_init: {
        id: 4
        const_expr: { int64_value: 20 }
      }
      loop_condition: {
        id: 9
        const_expr: { bool_value: false }
      }
      loop_step: {
        id: 10
        ident_expr: { name: "x" }
      }
      result: {
        id: 6
        call_expr: {
          function: "_*_"
          args: {
            id: 5
            ident_expr: { name: "x" }
          }
          args: {
            id: 7
            ident_expr: { name: "x" }
          }
        }
      }
    }
  })pb";

TEST(BindingsExtTest, TraceSupport) {
  ParsedExpr expr;
  ASSERT_TRUE(TextFormat::ParseFromString(kTraceExpr, &expr));
  InterpreterOptions options;
  options.enable_heterogeneous_equality = true;
  options.enable_empty_wrapper_null_unboxing = true;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));
  Activation activation;
  google::protobuf::Arena arena;
  absl::flat_hash_map<int64_t, CelValue> ids;
  ASSERT_OK_AND_ASSIGN(
      auto result,
      plan->Trace(activation, &arena,
                  [&](int64_t id, const CelValue& value, google::protobuf::Arena* arena) {
                    ids[id] = value;
                    return absl::OkStatus();
                  }));

  EXPECT_TRUE(result.IsInt64() && result.Int64OrDie() == 400)
      << result.DebugString();

  EXPECT_THAT(ids, Contains(Pair(4, IsCelInt64(20))));
  EXPECT_THAT(ids, Contains(Pair(7, IsCelInt64(20))));
}

// Test bind expression with nested field selection.
//
// cel.bind(submsg,
//          msg.child.child,
//          (false) ?
//            TestAllTypes{single_int64: -42}.single_int64 :
//            submsg.payload.single_int64)
constexpr absl::string_view kFieldSelectTestExpr = R"pb(
  reference_map: {
    key: 4
    value: { name: "msg" }
  }
  reference_map: {
    key: 8
    value: { overload_id: "conditional" }
  }
  reference_map: {
    key: 9
    value: { name: "cel.expr.conformance.proto2.TestAllTypes" }
  }
  reference_map: {
    key: 13
    value: { name: "submsg" }
  }
  reference_map: {
    key: 18
    value: { name: "submsg" }
  }
  type_map: {
    key: 4
    value: { message_type: "cel.expr.conformance.proto2.NestedTestAllTypes" }
  }
  type_map: {
    key: 5
    value: { message_type: "cel.expr.conformance.proto2.NestedTestAllTypes" }
  }
  type_map: {
    key: 6
    value: { message_type: "cel.expr.conformance.proto2.NestedTestAllTypes" }
  }
  type_map: {
    key: 7
    value: { primitive: BOOL }
  }
  type_map: {
    key: 8
    value: { primitive: INT64 }
  }
  type_map: {
    key: 9
    value: { message_type: "cel.expr.conformance.proto2.TestAllTypes" }
  }
  type_map: {
    key: 11
    value: { primitive: INT64 }
  }
  type_map: {
    key: 12
    value: { primitive: INT64 }
  }
  type_map: {
    key: 13
    value: { message_type: "cel.expr.conformance.proto2.NestedTestAllTypes" }
  }
  type_map: {
    key: 14
    value: { message_type: "cel.expr.conformance.proto2.TestAllTypes" }
  }
  type_map: {
    key: 15
    value: { primitive: INT64 }
  }
  type_map: {
    key: 16
    value: { list_type: { elem_type: { dyn: {} } } }
  }
  type_map: {
    key: 17
    value: { primitive: BOOL }
  }
  type_map: {
    key: 18
    value: { message_type: "cel.expr.conformance.proto2.NestedTestAllTypes" }
  }
  type_map: {
    key: 19
    value: { primitive: INT64 }
  }
  source_info: {
    location: "<input>"
    line_offsets: 120
    positions: { key: 1 value: 0 }
    positions: { key: 2 value: 8 }
    positions: { key: 3 value: 9 }
    positions: { key: 4 value: 17 }
    positions: { key: 5 value: 20 }
    positions: { key: 6 value: 26 }
    positions: { key: 7 value: 35 }
    positions: { key: 8 value: 42 }
    positions: { key: 9 value: 56 }
    positions: { key: 10 value: 69 }
    positions: { key: 11 value: 71 }
    positions: { key: 12 value: 75 }
    positions: { key: 13 value: 91 }
    positions: { key: 14 value: 97 }
    positions: { key: 15 value: 105 }
    positions: { key: 16 value: 8 }
    positions: { key: 17 value: 8 }
    positions: { key: 18 value: 8 }
    positions: { key: 19 value: 8 }
    macro_calls: {
      key: 19
      value: {
        call_expr: {
          target: {
            id: 1
            ident_expr: { name: "cel" }
          }
          function: "bind"
          args: {
            id: 3
            ident_expr: { name: "submsg" }
          }
          args: {
            id: 6
            select_expr: {
              operand: {
                id: 5
                select_expr: {
                  operand: {
                    id: 4
                    ident_expr: { name: "msg" }
                  }
                  field: "child"
                }
              }
              field: "child"
            }
          }
          args: {
            id: 8
            call_expr: {
              function: "_?_:_"
              args: {
                id: 7
                const_expr: { bool_value: false }
              }
              args: {
                id: 12
                select_expr: {
                  operand: {
                    id: 9
                    struct_expr: {
                      message_name: "cel.expr.conformance.proto2.TestAllTypes"
                      entries: {
                        id: 10
                        field_key: "single_int64"
                        value: {
                          id: 11
                          const_expr: { int64_value: -42 }
                        }
                      }
                    }
                  }
                  field: "single_int64"
                }
              }
              args: {
                id: 15
                select_expr: {
                  operand: {
                    id: 14
                    select_expr: {
                      operand: {
                        id: 13
                        ident_expr: { name: "submsg" }
                      }
                      field: "payload"
                    }
                  }
                  field: "single_int64"
                }
              }
            }
          }
        }
      }
    }
  }
  expr: {
    id: 19
    comprehension_expr: {
      iter_var: "#unused"
      iter_range: {
        id: 16
        list_expr: {}
      }
      accu_var: "submsg"
      accu_init: {
        id: 6
        select_expr: {
          operand: {
            id: 5
            select_expr: {
              operand: {
                id: 4
                ident_expr: { name: "msg" }
              }
              field: "child"
            }
          }
          field: "child"
        }
      }
      loop_condition: {
        id: 17
        const_expr: { bool_value: false }
      }
      loop_step: {
        id: 18
        ident_expr: { name: "submsg" }
      }
      result: {
        id: 8
        call_expr: {
          function: "_?_:_"
          args: {
            id: 7
            const_expr: { bool_value: false }
          }
          args: {
            id: 12
            select_expr: {
              operand: {
                id: 9
                struct_expr: {
                  message_name: "cel.expr.conformance.proto2.TestAllTypes"
                  entries: {
                    id: 10
                    field_key: "single_int64"
                    value: {
                      id: 11
                      const_expr: { int64_value: -42 }
                    }
                  }
                }
              }
              field: "single_int64"
            }
          }
          args: {
            id: 15
            select_expr: {
              operand: {
                id: 14
                select_expr: {
                  operand: {
                    id: 13
                    ident_expr: { name: "submsg" }
                  }
                  field: "payload"
                }
              }
              field: "single_int64"
            }
          }
        }
      }
    }
  })pb";

class BindingsExtInteractionsTest : public testing::TestWithParam<bool> {
 protected:
  bool GetEnableSelectOptimization() { return GetParam(); }
};

TEST_P(BindingsExtInteractionsTest, SelectOptimization) {
  CheckedExpr expr;
  ASSERT_TRUE(TextFormat::ParseFromString(kFieldSelectTestExpr, &expr));
  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(&expr));
  Arena arena;
  Activation activation;

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsInt64());
  EXPECT_EQ(out.Int64OrDie(), 42);
}

TEST_P(BindingsExtInteractionsTest, UnknownAttributesSelectOptimization) {
  CheckedExpr expr;
  ASSERT_TRUE(TextFormat::ParseFromString(kFieldSelectTestExpr, &expr));
  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.unknown_processing = UnknownProcessingOptions::kAttributeOnly;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(&expr));
  Arena arena;
  Activation activation;
  activation.set_unknown_attribute_patterns({AttributePattern(
      "msg", {AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("child")})});

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsUnknownSet());
  EXPECT_THAT(out.UnknownSetOrDie()->unknown_attributes(),
              testing::ElementsAre(
                  Attribute("msg", {AttributeQualifier::OfString("child"),
                                    AttributeQualifier::OfString("child")})));
}

TEST_P(BindingsExtInteractionsTest,
       UnknownAttributeSelectOptimizationReturnValue) {
  CheckedExpr expr;
  ASSERT_TRUE(TextFormat::ParseFromString(kFieldSelectTestExpr, &expr));
  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.unknown_processing = UnknownProcessingOptions::kAttributeOnly;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(&expr));
  Arena arena;
  Activation activation;
  activation.set_unknown_attribute_patterns({AttributePattern(
      "msg", {AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("payload"),
              AttributeQualifierPattern::OfString("single_int64")})});

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsUnknownSet()) << out.DebugString();
  EXPECT_THAT(out.UnknownSetOrDie()->unknown_attributes(),
              testing::ElementsAre(Attribute(
                  "msg", {AttributeQualifier::OfString("child"),
                          AttributeQualifier::OfString("child"),
                          AttributeQualifier::OfString("payload"),
                          AttributeQualifier::OfString("single_int64")})));
}

TEST_P(BindingsExtInteractionsTest, MissingAttributesSelectOptimization) {
  CheckedExpr expr;
  ASSERT_TRUE(TextFormat::ParseFromString(kFieldSelectTestExpr, &expr));
  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.enable_missing_attribute_errors = true;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(&expr));
  Arena arena;
  Activation activation;
  activation.set_missing_attribute_patterns({AttributePattern(
      "msg", {AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("payload"),
              AttributeQualifierPattern::OfString("single_int64")})});

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsError()) << out.DebugString();
  EXPECT_THAT(out.ErrorOrDie()->ToString(),
              HasSubstr("msg.child.child.payload.single_int64"));
}

TEST_P(BindingsExtInteractionsTest, UnknownAttribute) {
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, ParseWithMacros(
                                            R"(
                                              cel.bind(
                                              x,
                                              msg.child.payload.single_int64,
                                              x < 42 || 1 == 1))",
                                            all_macros));

  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.unknown_processing = UnknownProcessingOptions::kAttributeOnly;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  Arena arena;
  Activation activation;
  activation.set_unknown_attribute_patterns({AttributePattern(
      "msg", {AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("payload"),
              AttributeQualifierPattern::OfString("single_int64")})});

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsBool()) << out.DebugString();
  EXPECT_TRUE(out.BoolOrDie());
}

TEST_P(BindingsExtInteractionsTest, UnknownAttributeReturnValue) {
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, ParseWithMacros(
                                            R"(
                                            cel.bind(
                                                x,
                                                msg.child.payload.single_int64,
                                                x))",
                                            all_macros));

  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.unknown_processing = UnknownProcessingOptions::kAttributeOnly;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  Arena arena;
  Activation activation;
  activation.set_unknown_attribute_patterns({AttributePattern(
      "msg", {AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("payload"),
              AttributeQualifierPattern::OfString("single_int64")})});

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsUnknownSet()) << out.DebugString();
  EXPECT_THAT(out.UnknownSetOrDie()->unknown_attributes(),
              testing::ElementsAre(Attribute(
                  "msg", {AttributeQualifier::OfString("child"),
                          AttributeQualifier::OfString("payload"),
                          AttributeQualifier::OfString("single_int64")})));
}

TEST_P(BindingsExtInteractionsTest, MissingAttribute) {
  std::vector<Macro> all_macros = Macro::AllMacros();
  std::vector<Macro> bindings_macros = cel::extensions::bindings_macros();
  all_macros.insert(all_macros.end(), bindings_macros.begin(),
                    bindings_macros.end());
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, ParseWithMacros(
                                            R"(
                                            cel.bind(
                                                x,
                                                msg.child.payload.single_int64,
                                                x < 42 || 1 == 2))",
                                            all_macros));

  InterpreterOptions options;
  options.enable_empty_wrapper_null_unboxing = true;
  options.enable_missing_attribute_errors = true;
  options.enable_select_optimization = GetEnableSelectOptimization();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreateCelExpressionBuilder(options);

  ASSERT_OK(builder->GetRegistry()->Register(CreateBindFunction()));

  // Register builtins and configure the execution environment.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  Arena arena;
  Activation activation;
  activation.set_missing_attribute_patterns({AttributePattern(
      "msg", {AttributeQualifierPattern::OfString("child"),
              AttributeQualifierPattern::OfString("payload"),
              AttributeQualifierPattern::OfString("single_int64")})});

  NestedTestAllTypes msg;
  msg.mutable_child()->mutable_child()->mutable_payload()->set_single_int64(42);

  activation.InsertValue("msg", CelProtoWrapper::CreateMessage(&msg, &arena));

  // Run evaluation.
  ASSERT_OK_AND_ASSIGN(CelValue out, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(out.IsError()) << out.DebugString();
  EXPECT_THAT(out.ErrorOrDie()->ToString(),
              HasSubstr("msg.child.payload.single_int64"));
}

INSTANTIATE_TEST_SUITE_P(BindingsExtInteractionsTest,
                         BindingsExtInteractionsTest,
                         /*enable_select_optimization=*/testing::Bool());

}  // namespace
}  // namespace cel::extensions
