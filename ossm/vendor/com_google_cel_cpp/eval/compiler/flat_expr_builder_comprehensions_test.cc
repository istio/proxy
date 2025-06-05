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

#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/cel_expression_builder_flat_impl.h"
#include "eval/compiler/comprehension_vulnerability_check.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::testing::HasSubstr;

class CelExpressionBuilderFlatImplComprehensionsTest
    : public testing::TestWithParam<bool> {
 public:
  CelExpressionBuilderFlatImplComprehensionsTest() = default;

  bool enable_recursive_planning() { return GetParam(); }

  cel::RuntimeOptions GetRuntimeOptions() {
    cel::RuntimeOptions options;
    if (enable_recursive_planning()) {
      options.max_recursion_depth = -1;
    }
    options.enable_comprehension_list_append = true;
    return options;
  }
};

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest, NestedComp) {
  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       parser::Parse("[1, 2].filter(x, [3, 4].all(y, x < y))"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest, MapComp) {
  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr, parser::Parse("[1, 2].map(x, x * 2)"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
  EXPECT_THAT((*result.ListOrDie())[0],
              test::EqualsCelValue(CelValue::CreateInt64(2)));
  EXPECT_THAT((*result.ListOrDie())[1],
              test::EqualsCelValue(CelValue::CreateInt64(4)));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest, ExistsOneTrue) {
  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       parser::Parse("[7].exists_one(a, a == 7)"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest, ExistsOneFalse) {
  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       parser::Parse("[7, 7].exists_one(a, a == 7)"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelBool(false));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest, ListCompWithUnknowns) {
  cel::RuntimeOptions options = GetRuntimeOptions();
  options.unknown_processing = UnknownProcessingOptions::kAttributeAndFunction;
  CelExpressionBuilderFlatImpl builder(options);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       parser::Parse("items.exists(i, i < 0)"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  activation.set_unknown_attribute_patterns({CelAttributePattern{
      "items",
      {CreateCelAttributeQualifierPattern(CelValue::CreateInt64(1))}}});
  ContainerBackedListImpl list_impl = ContainerBackedListImpl({
      CelValue::CreateInt64(1),
      // element items[1] is marked unknown, so the computation should produce
      // and unknown set.
      CelValue::CreateInt64(-1),
      CelValue::CreateInt64(2),
  });
  activation.InsertValue("items", CelValue::CreateList(&list_impl));

  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsUnknownSet()) << result.DebugString();

  const auto& attrs = result.UnknownSetOrDie()->unknown_attributes();
  EXPECT_THAT(attrs, testing::SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), testing::Eq("items"));
  EXPECT_THAT(attrs.begin()->qualifier_path(), testing::SizeIs(1));
  EXPECT_THAT(attrs.begin()->qualifier_path().at(0).GetInt64Key().value(),
              testing::Eq(1));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       InvalidComprehensionWithRewrite) {
  CheckedExpr expr;
  // The rewrite step which occurs when an identifier gets a more qualified name
  // from the reference map has the potential to make invalid comprehensions
  // appear valid, by populating missing fields with default values.
  // var.<macro>(x, <missing>)
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        reference_map {
          key: 1
          value { name: "qualified.var" }
        }
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range {
              id: 1
              ident_expr { name: "var" }
            }
            accu_var: "y"
            accu_init {
              id: 1
              const_expr { bool_value: true }
            }
          }
        })pb",
      &expr);
  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       testing::AnyOf(HasSubstr("Invalid comprehension"),
                                      HasSubstr("Invalid empty expression"))));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithConcatVulernability) {
  CheckedExpr expr;
  // The comprehension loop step performs an unsafe concatenation of the
  // accumulation variable with itself or one of its children.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              call_expr {
                function: "_?_:_"
                args { const_expr { bool_value: true } }
                args { ident_expr { name: "y" } }
                args {
                  call_expr {
                    function: "_+_"
                    args {
                      call_expr {
                        function: "dyn"
                        args { ident_expr { name: "y" } }
                      }
                    }
                    args {
                      call_expr {
                        function: "_[_]"
                        args { ident_expr { name: "y" } }
                        args { const_expr { int64_value: 0 } }
                      }
                    }
                  }
                }
              }
            }
          }
        })pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithListVulernability) {
  CheckedExpr expr;
  // The comprehension
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              list_expr {
                elements { ident_expr { name: "y" } }
                elements {
                  list_expr {
                    elements {
                      select_expr {
                        operand { ident_expr { name: "y" } }
                        field: "z"
                      }
                    }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithStructVulernability) {
  CheckedExpr expr;
  // The comprehension loop step builds a deeply nested struct which expands
  // exponentially.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              struct_expr {
                entries {
                  map_key { const_expr { string_value: "key" } }
                  value { ident_expr { name: "y" } }
                }
                entries {
                  map_key { const_expr { string_value: "present" } }
                  value {
                    select_expr {
                      test_only: true
                      operand { ident_expr { name: "y" } }
                      field: "z"
                    }
                  }
                }
                entries {
                  map_key { const_expr { string_value: "key_subset" } }
                  value {
                    select_expr {
                      operand { ident_expr { name: "y" } }
                      field: "z"
                    }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithNestedComprehensionResultVulernability) {
  CheckedExpr expr;
  // The nested comprehension performs an unsafe concatenation on the parent
  // accumulator variable within its 'result' expression.
  //
  // The inner-most comprehension shadows its parent, but still refers to its
  // oldest ancestor. It, however, does not do anything unsafe.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr { comprehension_expr {
          iter_var: "x"
          iter_range { ident_expr { name: "var" } }
          accu_var: "y"
          accu_init { list_expr {} }
          result { ident_expr { name: "y" } }
          loop_condition { const_expr { bool_value: true } }
          loop_step {
            comprehension_expr {
              iter_var: "x"
              iter_range { ident_expr { name: "y" } }
              accu_var: "z"
              accu_init { list_expr {} }
              result {
                call_expr {
                  function: "_+_"
                  args { ident_expr { name: "y" } }
                  args { ident_expr { name: "y" } }
                }
              }
              loop_condition { const_expr { bool_value: true } }
              loop_step {
                comprehension_expr {
                  iter_var: "x"
                  iter_range { ident_expr { name: "y" } }
                  accu_var: "z"
                  accu_init { list_expr {} }
                  result {
                    call_expr {
                      function: "dyn"
                      args { ident_expr { name: "y" } }
                    }
                  }
                  loop_condition { const_expr { bool_value: true } }
                  loop_step {
                    call_expr {
                      function: "dyn"
                      args { ident_expr { name: "y" } }
                    }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithNestedComprehensionLoopStepVulernability) {
  CheckedExpr expr;
  // The nested comprehension performs an unsafe concatenation on the parent
  // accumulator variable within its 'loop_step'.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "var" } }
            accu_var: "y"
            accu_init { list_expr {} }
            result { ident_expr { name: "y" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              comprehension_expr {
                iter_var: "x"
                iter_range { ident_expr { name: "y" } }
                accu_var: "z"
                accu_init { list_expr {} }
                result { ident_expr { name: "z" } }
                loop_condition { const_expr { bool_value: true } }
                loop_step {
                  call_expr {
                    function: "_+_"
                    args { ident_expr { name: "y" } }
                    args { ident_expr { name: "y" } }
                  }
                }
              }
            }
          }
        }
      )pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithNestedComprehensionLoopStepVulernabilityResult) {
  CheckedExpr expr;
  // The nested comprehension performs an unsafe concatenation on the parent
  // accumulator.
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "outer_iter"
            iter_range { ident_expr { name: "input_list" } }
            accu_var: "outer_accu"
            accu_init { ident_expr { name: "input_list" } }
            loop_condition {
              id: 3
              const_expr { bool_value: true }
            }
            loop_step {
              comprehension_expr {
                # the iter_var shadows the outer accumulator on the loop step
                # but not the result step.
                iter_var: "outer_accu"
                iter_range { list_expr {} }
                accu_var: "inner_accu"
                accu_init { list_expr {} }
                loop_condition { const_expr { bool_value: true } }
                loop_step { list_expr {} }
                result {
                  call_expr {
                    function: "_+_"
                    args { ident_expr { name: "outer_accu" } }
                    args { ident_expr { name: "outer_accu" } }
                  }
                }
              }
            }
            result { list_expr {} }
          }
        }
      )pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder;
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       ComprehensionWithNestedComprehensionLoopStepIterRangeVulnerability) {
  CheckedExpr expr;
  // The nested comprehension unsafely modifies the parent accumulator
  // (outer_accu) being used as a iterable range
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "x"
            iter_range { ident_expr { name: "input_list" } }
            accu_var: "outer_accu"
            accu_init { ident_expr { name: "input_list" } }
            loop_condition { const_expr { bool_value: true } }
            loop_step {
              comprehension_expr {
                iter_var: "y"
                iter_range { ident_expr { name: "outer_accu" } }
                accu_var: "inner_accu"
                accu_init { ident_expr { name: "outer_accu" } }
                loop_condition { const_expr { bool_value: true } }
                loop_step {
                  call_expr {
                    function: "_+_"
                    args { ident_expr { name: "inner_accu" } }
                    args { const_expr { string_value: "12345" } }
                  }
                }
                result { ident_expr { name: "inner_accu" } }
              }
            }
            result { ident_expr { name: "outer_accu" } }
          }
        }
      )pb",
      &expr);

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder;
  builder.flat_expr_builder().AddProgramOptimizer(
      CreateComprehensionVulnerabilityCheck());
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("memory exhaustion vulnerability")));
}

TEST_P(CelExpressionBuilderFlatImplComprehensionsTest,
       InvalidBindComprehension) {
  ParsedExpr expr;
  // Trivial comprehensions (such as cel.bind), are optimized by skipping the
  // planning for the loop step, however the planner will still warn if the
  // loop step references the unused var.
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr {
          comprehension_expr {
            iter_var: "#unused"
            iter_range {
              id: 1
              list_expr {}
            }
            accu_var: "bind_var"
            accu_init {
              id: 1
              const_expr { bool_value: true }
            }
            loop_step {
              call_expr {
                function: "_&&_"
                args { ident_expr { name: "#unused" } }
                args { ident_expr { name: "bind_var" } }
              }
            }
            loop_condition { const_expr { bool_value: false } }
            result { ident_expr { name: "bind_var" } }
          }
        })pb",
      &expr));

  cel::RuntimeOptions options = GetRuntimeOptions();
  CelExpressionBuilderFlatImpl builder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  EXPECT_THAT(
      builder.CreateExpression(&(expr.expr()), nullptr).status(),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("Unexpected iter_var access in trivial comprehension")));
}

INSTANTIATE_TEST_SUITE_P(TestSuite,
                         CelExpressionBuilderFlatImplComprehensionsTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "recursive" : "default";
                         });

}  // namespace

}  // namespace google::api::expr::runtime
