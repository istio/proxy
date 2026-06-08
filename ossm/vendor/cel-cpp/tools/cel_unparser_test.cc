// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/cel_unparser.h"

#include <string>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "google/protobuf/text_format.h"

namespace google::api::expr {
namespace {

using ::absl_testing::StatusIs;
using ::cel::internal::test::EqualsProto;
using ::cel::expr::Expr;
using ::cel::expr::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::testing::HasSubstr;
using ::testing::ValuesIn;

struct UnparserTestCaseTextProto {
  std::string proto_text;
  absl::StatusOr<std::string> expr;
};

class UnparserTestTextProto
    : public testing::TestWithParam<UnparserTestCaseTextProto> {};

TEST_P(UnparserTestTextProto, Test) {
  auto test_case = GetParam();
  Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(test_case.proto_text, &expr));
  absl::StatusOr<std::string> result = Unparse(expr);
  if (result.ok()) {
    ASSERT_OK(test_case.expr);
    ASSERT_EQ(*(test_case.expr), *result);
  } else {
    ASSERT_THAT(result.status(),
                StatusIs(test_case.expr.status().code(),
                         HasSubstr(test_case.expr.status().message())));
  }
}

// these tests make explicit assumptions about specific proto structures
// that are to be observed
INSTANTIATE_TEST_SUITE_P(
    UnparseCompProto, UnparserTestTextProto,
    ValuesIn<UnparserTestCaseTextProto>(
        {// Empty Expr error
         {"", absl::InvalidArgumentError("Unsupported Expr")},

         // Constants
         {"const_expr{}", absl::InvalidArgumentError("Unsupported Constant")},
         {"const_expr{bool_value: true}", "true"},
         {"const_expr{int64_value: 4}", "4"},
         {"const_expr{uint64_value: 4}", "4u"},

         // Sequences
         {
             R"pb(
               struct_expr {
                 entries { value { const_expr { uint64_value: 2 } } }
               })pb",
             absl::InvalidArgumentError("Unexpected struct")},
         {R"pb(
            list_expr {
              elements { const_expr { int64_value: 1 } }
              elements { const_expr { uint64_value: 2 } }
            }
          )pb",
          "[1, 2u]"},
         {R"pb(
            struct_expr {
              entries {
                map_key { const_expr { int64_value: 1 } }
                value { const_expr { uint64_value: 2 } }
              }
              entries {
                map_key { const_expr { int64_value: 2 } }
                value { const_expr { uint64_value: 3 } }
              }
            })pb",
          "{1: 2u, 2: 3u}"},

         // Messages
         {R"pb(
            struct_expr {
              message_name: 'TestAllTypes'
              entries {
                field_key: 'single_int32'
                value { const_expr { int64_value: 1 } }
              }
              entries {
                field_key: 'single_int64'
                value { const_expr { int64_value: 2 } }
              }
            }
          )pb",
          "TestAllTypes{single_int32: 1, single_int64: 2}"},

         // Conditionals
         {R"pb(
            call_expr { function: '!_' }
          )pb",
          absl::InvalidArgumentError("Unexpected unary")},
         {R"pb(
            call_expr { function: '_||_' }
          )pb",
          absl::InvalidArgumentError("Unexpected binary")},
         {R"pb(
            call_expr { function: '_[_]' }
          )pb",
          absl::InvalidArgumentError("Unexpected index")},
         {R"pb(
            call_expr { function: '_?_:_' }
          )pb",
          absl::InvalidArgumentError("Unexpected ternary")},
         {R"pb(
            call_expr {
              function: '_||_'
              args {
                call_expr {
                  function: '_&&_'
                  args { const_expr { bool_value: false } }
                  args {
                    call_expr {
                      function: '!_'
                      args { const_expr { bool_value: true } }
                    }
                  }
                }
              }
              args { const_expr { bool_value: false } }
            })pb",
          "false && !true || false"},
         {R"pb(
            call_expr {
              function: '_&&_'
              args { const_expr { bool_value: false } }
              args {
                call_expr {
                  function: '_||_'
                  args {
                    call_expr {
                      function: '!_'
                      args { const_expr { bool_value: true } }
                    }
                  }
                  args { const_expr { bool_value: false } }
                }
              }
            })pb",
          "false && (!true || false)"},
         {R"pb(
            call_expr {
              function: '_?_:_'
              args {
                call_expr {
                  function: '_||_'
                  args {
                    call_expr {
                      function: '_&&_'
                      args { const_expr { bool_value: false } }
                      args {
                        call_expr {
                          function: "!_"
                          args { const_expr { bool_value: true } }
                        }
                      }
                    }
                  }
                  args { const_expr { bool_value: false } }
                }
              }
              args { const_expr { int64_value: 2 } }
              args { const_expr { int64_value: 3 } }
            })pb",
          "(false && !true || false) ? 2 : 3"},
         {R"pb(
            call_expr {
              function: '!_'
              args {
                call_expr {
                  function: '!_'
                  args { const_expr { bool_value: true } }
                }
              }
            })pb",
          "!!true"},
         {R"pb(
            call_expr {
              function: '_?_:_'
              args {
                call_expr {
                  function: '_<_'
                  args { ident_expr { name: 'x' } }
                  args { const_expr { int64_value: 5 } }
                }
              }
              args { ident_expr { name: 'x' } }
              args { const_expr { int64_value: 5 } }
            })pb",
          "(x < 5) ? x : 5"},
         {R"pb(
            call_expr {
              function: '_?_:_'
              args {
                call_expr {
                  function: '_>_'
                  args { ident_expr { name: 'x' } }
                  args { const_expr { int64_value: 5 } }
                }
              }
              args {
                call_expr {
                  function: '_-_'
                  args { ident_expr { name: 'x' } }
                  args { const_expr { int64_value: 5 } }
                }
              }
              args { const_expr { int64_value: 0 } }
            })pb",
          "(x > 5) ? (x - 5) : 0"},
         {R"pb(
            call_expr {
              function: '_?_:_'
              args {
                call_expr {
                  function: '_>_'
                  args { ident_expr { name: 'x' } }
                  args { const_expr { int64_value: 5 } }
                }
              }
              args {
                call_expr {
                  function: '_?_:_'
                  args {
                    call_expr {
                      function: '_>_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 10 } }
                    }
                  }
                  args {
                    call_expr {
                      function: '_-_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 10 } }
                    }
                  }
                  args { const_expr { int64_value: 5 } }
                }
              }
              args { const_expr { int64_value: 0 } }
            })pb",
          "(x > 5) ? ((x > 10) ? (x - 10) : 5) : 0"},
         {R"pb(
            call_expr {
              function: '_in_'
              args { ident_expr { name: 'a' } }
              args { ident_expr { name: 'b' } }
            })pb",
          "a in b"},

         // Calculations
         {R"pb(
            call_expr {
              function: '_*_'
              args {
                call_expr {
                  function: '_+_'
                  args { const_expr { int64_value: 1 } }
                  args { const_expr { int64_value: 2 } }
                }
              }
              args { const_expr { int64_value: 3 } }
            })pb",
          "(1 + 2) * 3"},
         {R"pb(
            call_expr {
              function: '_+_'
              args { const_expr { int64_value: 1 } }
              args {
                call_expr {
                  function: '_*_'
                  args { const_expr { int64_value: 2 } }
                  args { const_expr { int64_value: 3 } }
                }
              }
            })pb",
          "1 + 2 * 3"},
         {R"pb(
            call_expr {
              function: '-_'
              args {
                call_expr {
                  function: '_*_'
                  args { const_expr { int64_value: 1 } }
                  args { const_expr { int64_value: 2 } }
                }
              }
            })pb",
          "-(1 * 2)"},

         // Comprehensions
         {R"pb(
            comprehension_expr {
              iter_var: 'x'
              iter_range {
                list_expr {
                  elements { const_expr { int64_value: 1 } }
                  elements { const_expr { int64_value: 2 } }
                  elements { const_expr { int64_value: 3 } }
                }
              }
              accu_var: 'accu'
              accu_init { const_expr { bool_value: true } }
              loop_condition { ident_expr { name: 'accu' } }
              loop_step {
                call_expr {
                  function: '_&&_'
                  args { ident_expr { name: 'x' } }
                  args {
                    call_expr {
                      function: '_>_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 0 } }
                    }
                  }
                }
              }
              result { ident_expr { name: 'accu' } }
            })pb",
          "[1, 2, 3].all(x, x > 0)"},
         {R"pb(
            comprehension_expr {
              iter_var: 'x'
              iter_range {
                list_expr {
                  elements { const_expr { int64_value: 1 } }
                  elements { const_expr { int64_value: 2 } }
                  elements { const_expr { int64_value: 3 } }
                }
              }
              accu_var: 'accu'
              accu_init { const_expr { bool_value: false } }
              loop_condition {
                call_expr {
                  function: '!_'
                  args { ident_expr { name: 'accu' } }
                }
              }
              loop_step {
                call_expr {
                  function: '_||_'
                  args { ident_expr { name: 'x' } }
                  args {
                    call_expr {
                      function: '_>_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 0 } }
                    }
                  }
                }
              }
              result { ident_expr { name: 'accu' } }
            })pb",
          "[1, 2, 3].exists(x, x > 0)"},
         {R"pb(
            comprehension_expr {
              iter_var: 'x'
              iter_range {
                list_expr {
                  elements { const_expr { int64_value: 1 } }
                  elements { const_expr { int64_value: 2 } }
                  elements { const_expr { int64_value: 3 } }
                }
              }
              accu_var: 'accu'
              accu_init { list_expr {} }
              loop_condition { const_expr { bool_value: false } }
              loop_step {
                call_expr {
                  function: '_?_:_'
                  args {
                    call_expr {
                      function: '_>=_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 2 } }
                    }
                  }
                  args {
                    call_expr {
                      function: '_+_'
                      args { ident_expr { name: 'accu' } }
                      args {
                        list_expr {
                          elements {
                            call_expr {
                              function: '_*_'
                              args { ident_expr { name: 'x' } }
                              args { const_expr { int64_value: 4 } }
                            }
                          }
                        }
                      }
                    }
                  }
                  args { ident_expr { name: 'accu' } }
                }
              }
              result { ident_expr { name: 'accu' } }
            })pb",
          "[1, 2, 3].map(x, x >= 2, x * 4)"},
         {R"pb(
            comprehension_expr {
              iter_var: 'x'
              iter_range {
                list_expr {
                  elements { const_expr { int64_value: 1 } }
                  elements { const_expr { int64_value: 2 } }
                  elements { const_expr { int64_value: 3 } }
                }
              }
              accu_var: 'accu'
              accu_init { const_expr { int64_value: 0 } }
              loop_condition {
                call_expr {
                  function: '_<=_'
                  args { ident_expr { name: 'accu' } }
                  args { const_expr { int64_value: 1 } }
                }
              }
              loop_step {
                call_expr {
                  function: '_?_:_'
                  args {
                    call_expr {
                      function: '_>=_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 2 } }
                    }
                  }
                  args {
                    call_expr {
                      function: '_+_'
                      args { ident_expr { name: 'accu' } }
                      args { const_expr { int64_value: 1 } }
                    }
                  }
                  args { ident_expr { name: 'accu' } }
                }
              }
              result {
                call_expr {
                  function: '_==_'
                  args { ident_expr { name: 'accu' } }
                  args { const_expr { int64_value: 1 } }
                }
              }
            })pb",
          "[1, 2, 3].exists_one(x, x >= 2)"},
         {R"pb(
            select_expr {
              operand {
                call_expr {
                  function: '_[_]'
                  args { ident_expr { name: 'x' } }
                  args { const_expr { string_value: 'a' } }
                }
              }
              field: 'single_int32'
              test_only: true
            })pb",
          "has(x[\"a\"].single_int32)"},

         // This is a filter expression but is decompiled back to
         // map(x, filter_function, x) for which the evaluation is
         // equal to filter(x, filter_function).
         {R"pb(
            comprehension_expr {
              iter_var: 'x'
              iter_range {
                list_expr {
                  elements { const_expr { int64_value: 1 } }
                  elements { const_expr { int64_value: 2 } }
                  elements { const_expr { int64_value: 3 } }
                }
              }
              accu_var: 'accu'
              accu_init { list_expr {} }
              loop_condition { const_expr { bool_value: false } }
              loop_step {
                call_expr {
                  function: '_?_:_'
                  args {
                    call_expr {
                      function: '_>=_'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { int64_value: 2 } }
                    }
                  }
                  args {
                    call_expr {
                      function: '_+_'
                      args { ident_expr { name: 'accu' } }
                      args {
                        list_expr { elements { ident_expr { name: 'x' } } }
                      }
                    }
                  }
                  args { ident_expr { name: 'accu' } }
                }
              }
              result { ident_expr { name: 'accu' } }
            })pb",
          "[1, 2, 3].map(x, x >= 2, x)"},

         // Index
         {R"pb(
            call_expr {
              function: '_==_'
              args {
                select_expr {
                  operand {
                    call_expr {
                      function: '_[_]'
                      args { ident_expr { name: 'x' } }
                      args { const_expr { string_value: 'a' } }
                    }
                  }
                  field: 'single_int32'
                }
              }
              args { const_expr { int64_value: 23 } }
            })pb",
          "x[\"a\"].single_int32 == 23"},
         {R"pb(
            call_expr {
              function: '_[_]'
              args {
                call_expr {
                  function: '_[_]'
                  args { ident_expr { name: 'a' } }
                  args { const_expr { int64_value: 1 } }
                }
              }
              args { const_expr { string_value: 'b' } }
            })pb",
          "a[1][\"b\"]"},

         // Functions
         {R"pb(
            call_expr {
              function: '_!=_'
              args { ident_expr { name: 'x' } }
              args { const_expr { string_value: 'a' } }
            })pb",
          "x != \"a\""},
         {R"pb(
            call_expr {
              function: '_==_'
              args {
                call_expr {
                  function: 'size'
                  args { ident_expr { name: 'x' } }
                }
              }
              args {
                call_expr {
                  target { ident_expr { name: 'x' } }
                  function: 'size'
                }
              }
            })pb",
          "size(x) == x.size()"},

         // Long string
         {R"pb(
            list_expr {
              elements {
                const_expr {
                  string_value: 'Loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong'
                }
              }
            })pb",
          R"(["Loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong"])"}}));

struct UnparserTestCaseTextExpr {
  std::string expr;
  std::string equiv_expected;
};

class UnparserTestTextExpr
    : public testing::TestWithParam<UnparserTestCaseTextExpr> {};

TEST_P(UnparserTestTextExpr, Test) {
  Expr expr;

  parser::ParserOptions options;
  options.add_macro_calls = true;
  options.enable_optional_syntax = true;
  options.enable_quoted_identifiers = true;

  ASSERT_OK_AND_ASSIGN(ParsedExpr result,
                       Parse(GetParam().expr, "unparser", options));

  ASSERT_OK_AND_ASSIGN(std::string result_expr, Unparse(result));

  if (!GetParam().equiv_expected.empty()) {
    ASSERT_EQ(GetParam().equiv_expected, result_expr);
  } else {
    ASSERT_EQ(GetParam().expr, result_expr);
  }

  if (GetParam().equiv_expected.empty()) {
    // parse again, confirm it's the same result
    ASSERT_OK_AND_ASSIGN(ParsedExpr result2,
                         Parse(result_expr, "unparser", options));
    EXPECT_THAT(result, EqualsProto(result2));
  } else {
    // We cannot compare the original parsed proto and the equivalent expected
    // proto, since the IDs will most likely be different, e.g., due to
    // rebalancing logical expressions.
  }
}

// These test cases check that Unparse(Parse(expr)) is idempotent
// (if there is one string in an entry), or equivalent to some other
// form (if there are two strings in an entry). The latter can occur
// especially due to spacing in the expression, or if the logical
// expression balancer modifies an expression.
INSTANTIATE_TEST_SUITE_P(
    UnparseCompExpr, UnparserTestTextExpr,
    ValuesIn<UnparserTestCaseTextExpr>({
        {"a + b - c", ""},
        {"a && b && c && d && e", ""},
        {"a || b && (c || d) && e", ""},
        {"a ? b : c", ""},
        {"a[1][\"b\"]", ""},
        {"x[\"a\"].single_int32 == 23", ""},
        {"a * (b / c) % 0", ""},
        {"a + b * c", ""},
        {"(a + b) * c / (d - e)", ""},
        {"a * b / c % 0", ""},
        {"!true", ""},
        {"-num", ""},
        {"a || b || c || d || e", ""},
        {"-(1 * 2)", ""},
        {"-(1 + 2)", ""},
        {"(x > 5) ? (x - 5) : 0", ""},
        {"size(a ? (b ? c : d) : e)", ""},
        {"a.hello(\"world\")", ""},
        {"zero()", ""},
        {"one(\"a\")", ""},
        {"and(d, 32u)", ""},
        {"max(a, b, 100)", ""},
        {"x != \"a\"", ""},
        {"[]", ""},
        {"[1]", ""},
        {"[\"hello, world\", \"goodbye, world\", \"sure, why not?\"]", ""},
        {"b\"Ã¿\"", "b\"\\xc3\\x83\\xc2\\xbf\""},
        {"b'aaa\"bbb'", "b\"aaa\\\"bbb\""},
        {"-42.101", ""},
        {"false", ""},
        {"-405069", ""},
        {"null", ""},
        {"\"hello:\\t'world'\"", ""},
        {"true", ""},
        {"42u", ""},
        {"my_ident", ""},
        {"has(hello.world)", ""},
        {"{}", ""},
        {"{\"a\": a.b.c, b\"b\": bytes(a.b.c)}", ""},
        {"{a: a, b: a.b, c: a.b.c, a ? b : c: false, a || b: true}", ""},
        {"v1alpha1.Expr{}", ""},
        {"v1alpha1.Expr{id: 1, call_expr: v1alpha1.Call_Expr{function: "
         "\"name\"}}",
         ""},
        {"a.b.c", ""},
        {"a[b][c].name", ""},
        {"(a + b).name", ""},
        {"(a ? b : c).name", ""},
        {"(a ? b : c)[0]", ""},
        {"(a1 && a2) ? b : c", ""},
        {"a ? (b1 || b2) : (c1 && c2)", ""},
        {"(a ? b : c).method(d)", ""},

        // the following give the expected equivalent representation that
        // is to be observed when parsing and decompiling again, note the
        // differences in spacing and simplification of logical expressions
        {"a+b-c", "a + b - c"},
        {"a ? b          : c", "a ? b : c"},
        {"a[  1  ][\"b\"]", "a[1][\"b\"]"},
        {"(false && !true) || false", "false && !true || false"},
        {"a . b . c", "a.b.c"},
        // here we expect the expression balancer to remove the double negation
        {"!!true", "true"},

        // From protos above
        // Constants
        {"true", ""},
        {"4", ""},
        {"4u", ""},

        // Sequences
        {"[1, 2u]", ""},
        {"{1: 2u, 2: 3u}", ""},

        // Messages
        {"TestAllTypes{single_int32: 1, single_int64: 2}", ""},

        // Conditionals
        {"false && !true || false", ""},
        {"false && (!true || false)", ""},
        {"(false && !true || false) ? 2 : 3", ""},
        {"(x < 5) ? x : 5", ""},
        {"(x > 5) ? (x - 5) : 0", ""},
        {"(x > 5) ? ((x > 10) ? (x - 10) : 5) : 0", ""},
        {"a in b", ""},

        // Calculations
        {"(1 + 2) * 3", ""},
        {"1 + 2 * 3", ""},
        {"-(1 * 2)", ""},

        // Comprehensions
        {"[1, 2, 3].all(x, x > 0)", ""},
        {"[1, 2, 3].exists(x, x > 0)", ""},
        {"[1, 2, 3].map(x, x >= 2, x * 4)", ""},
        {"[1, 2, 3].exists_one(x, x >= 2)", ""},
        {"[[1], [2], [3]].all(x, x.all(y, y >= 2))", ""},
        {"(has(x.y) ? x.y : []).filter(z, z == \"zed\")", ""},

        // Macros
        {"has(x[\"a\"].single_int32)", ""},

        // This is a filter expression but is decompiled back to
        // map(x, filter_function, x) for which the evaluation is
        // equal to filter(x, filter_function).
        {"[1, 2, 3].map(x, x >= 2, x)", ""},

        // Index
        {"x[\"a\"].single_int32 == 23", ""},
        {"a[1][\"b\"]", ""},

        // Functions
        {"x != \"a\"", ""},
        {"size(x) == x.size()", ""},

        // Long string
        {R"(["Loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong"])",
         ""},
        {"a.?b[?0] && a[?c]", ""},
        {"{?\"key\": value}", ""},
        {"[?a, ?b]", ""},
        {"[?a[?b]]", ""},
        {"Msg{?field: value}", ""},
        {"Msg{`in`: value}", ""},
        {"Msg{?`b.c`: value}", ""},
        {"has(a.`b.c`)", ""},
        {"a.`b/c`", ""},
        {"a.?`b/c`", ""},
    }));

}  // namespace
}  // namespace google::api::expr
