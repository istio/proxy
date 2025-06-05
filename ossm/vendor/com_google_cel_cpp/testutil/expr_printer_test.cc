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

#include "testutil/expr_printer.h"

#include <string>

#include "absl/base/no_destructor.h"
#include "absl/strings/str_cat.h"
#include "common/expr.h"
#include "internal/testing.h"
#include "parser/options.h"
#include "parser/parser.h"

namespace cel::test {
namespace {

using ::google::api::expr::parser::Parse;

class TestAdorner : public ExpressionAdorner {
 public:
  static const TestAdorner& Get() {
    static absl::NoDestructor<TestAdorner> kInstance;
    return *kInstance;
  }

  std::string Adorn(const Expr& e) const override {
    return absl::StrCat("#", e.id());
  }

  std::string AdornStructField(const StructExprField& e) const override {
    return absl::StrCat("#", e.id());
  }

  std::string AdornMapEntry(const MapExprEntry& e) const override {
    return absl::StrCat("#", e.id());
  }
};

TEST(ExprPrinterTest, Identifier) {
  Expr expr;
  expr.mutable_ident_expr().set_name("foo");
  expr.set_id(1);
  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), ("foo#1"));
}

TEST(ExprPrinterTest, ConstantString) {
  Expr expr;
  expr.mutable_const_expr().set_string_value("foo");
  expr.set_id(1);

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"("foo"#1)"));
}

TEST(ExprPrinterTest, ConstantBytes) {
  Expr expr;
  expr.mutable_const_expr().set_bytes_value("foo");
  expr.set_id(1);

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(b"foo"#1)"));
}

TEST(ExprPrinterTest, ConstantInt) {
  Expr expr;
  expr.mutable_const_expr().set_int_value(1);
  expr.set_id(1);

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(1#1)"));
}

TEST(ExprPrinterTest, ConstantUint) {
  Expr expr;
  expr.mutable_const_expr().set_uint_value(1);
  expr.set_id(1);

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(1u#1)"));
}

TEST(ExprPrinterTest, ConstantDouble) {
  Expr expr;
  expr.mutable_const_expr().set_double_value(1.1);
  expr.set_id(1);

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(1.1#1)"));
}

TEST(ExprPrinterTest, ConstantBool) {
  Expr expr;
  expr.mutable_const_expr().set_bool_value(true);
  expr.set_id(1);

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(true#1)"));
}

TEST(ExprPrinterTest, Call) {
  Expr expr;
  expr.mutable_call_expr().set_function("foo");
  expr.set_id(1);
  {
    Expr& arg1 = expr.mutable_call_expr().add_args();
    arg1.mutable_const_expr().set_int_value(1);
    arg1.set_id(2);
  }
  {
    Expr& arg2 = expr.mutable_call_expr().add_args();
    arg2.mutable_const_expr().set_int_value(2);
    arg2.set_id(3);
  }

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(foo(
  1#2,
  2#3
)#1)"));
}

TEST(ExprPrinterTest, ReceiverCall) {
  Expr expr;
  expr.mutable_call_expr().set_function("foo");
  expr.set_id(1);
  {
    Expr& target = expr.mutable_call_expr().mutable_target();
    target.mutable_const_expr().set_string_value("bar");
    target.set_id(2);
  }
  {
    Expr& arg2 = expr.mutable_call_expr().add_args();
    arg2.mutable_const_expr().set_int_value(2);
    arg2.set_id(3);
  }

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"("bar"#2.foo(
  2#3
)#1)"));
}

TEST(ExprPrinterTest, List) {
  Expr expr;
  expr.set_id(1);
  {
    ListExprElement& arg1 = expr.mutable_list_expr().add_elements();
    arg1.set_optional(true);
    arg1.mutable_expr().set_id(2);
    arg1.mutable_expr().mutable_const_expr().set_int_value(1);
  }
  {
    ListExprElement& arg2 = expr.mutable_list_expr().add_elements();
    arg2.set_optional(false);
    arg2.mutable_expr().set_id(3);
    arg2.mutable_expr().mutable_const_expr().set_int_value(2);
  }

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"([
  ?1#2,
  2#3
]#1)"));
}

TEST(ExprPrinterTest, Map) {
  Expr expr;
  expr.set_id(1);
  {
    MapExprEntry& entry = expr.mutable_map_expr().add_entries();
    entry.set_id(2);
    entry.set_optional(true);
    entry.mutable_key().set_id(3);
    entry.mutable_key().mutable_const_expr().set_string_value("k1");
    entry.mutable_value().set_id(4);
    entry.mutable_value().mutable_const_expr().set_string_value("v1");
  }
  {
    MapExprEntry& entry = expr.mutable_map_expr().add_entries();
    entry.set_id(5);
    entry.set_optional(false);
    entry.mutable_key().set_id(6);
    entry.mutable_key().mutable_const_expr().set_string_value("k2");
    entry.mutable_value().set_id(7);
    entry.mutable_value().mutable_const_expr().set_string_value("v2");
  }

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"({
  ?"k1"#3:"v1"#4#2,
  "k2"#6:"v2"#7#5
}#1)"));
}

TEST(ExprPrinterTest, Struct) {
  Expr expr;
  expr.set_id(1);
  auto& struct_expr = expr.mutable_struct_expr();
  struct_expr.set_name("Foo");
  {
    StructExprField& field1 = struct_expr.add_fields();
    field1.set_optional(true);
    field1.set_id(2);
    field1.set_name("field1");
    field1.mutable_value().set_id(3);
    field1.mutable_value().mutable_const_expr().set_int_value(1);
  }
  {
    StructExprField& field2 = struct_expr.add_fields();
    field2.set_optional(false);
    field2.set_id(4);
    field2.set_name("field2");
    field2.mutable_value().set_id(5);
    field2.mutable_value().mutable_const_expr().set_int_value(1);
  }

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), (R"(Foo{
  ?field1:1#3#2,
  field2:1#5#4
}#1)"));
}

TEST(ExprPrinterTest, Comprehension) {
  Expr expr;
  expr.set_id(1);
  expr.mutable_comprehension_expr().set_iter_var("x");
  expr.mutable_comprehension_expr().set_accu_var("__result__");
  auto& range = expr.mutable_comprehension_expr().mutable_iter_range();
  range.set_id(2);
  range.mutable_ident_expr().set_name("range");
  auto& accu_init = expr.mutable_comprehension_expr().mutable_accu_init();
  accu_init.set_id(3);
  accu_init.mutable_ident_expr().set_name("accu_init");
  auto& loop_condition =
      expr.mutable_comprehension_expr().mutable_loop_condition();
  loop_condition.set_id(4);
  loop_condition.mutable_ident_expr().set_name("loop_condition");
  auto& loop_step = expr.mutable_comprehension_expr().mutable_loop_step();
  loop_step.set_id(5);
  loop_step.mutable_ident_expr().set_name("loop_step");
  auto& result = expr.mutable_comprehension_expr().mutable_result();
  result.set_id(6);
  result.mutable_ident_expr().set_name("result");

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.Print(expr), R"(__comprehension__(
  // Variable
  x,
  // Target
  range#2,
  // Accumulator
  __result__,
  // Init
  accu_init#3,
  // LoopCondition
  loop_condition#4,
  // LoopStep
  loop_step#5,
  // Result
  result#6)#1)");
}

TEST(ExprPrinterTest, Proto) {
  ParserOptions options;
  options.enable_optional_syntax = true;
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse(R"cel(
    "foo".startsWith("bar") ||
    [1, ?2, 3].exists(x, x in {?"b": "foo"}) ||
    Foo{
      byte_value: b'bytes',
      bool_value: false,
      uint_value: 1u,
      double_value: 1.1,
    }.bar
  )cel",
                                               "", options));

  ExprPrinter printer(TestAdorner::Get());
  EXPECT_EQ(printer.PrintProto(parsed_expr.expr()),
            R"ast(_||_(
  _||_(
    "foo"#1.startsWith(
      "bar"#3
    )#2,
    __comprehension__(
      // Variable
      x,
      // Target
      [
        1#5,
        ?2#6,
        3#7
      ]#4,
      // Accumulator
      __result__,
      // Init
      false#16,
      // LoopCondition
      @not_strictly_false(
        !_(
          __result__#17
        )#18
      )#19,
      // LoopStep
      _||_(
        __result__#20,
        @in(
          x#10,
          {
            ?"b"#14:"foo"#15#13
          }#12
        )#11
      )#21,
      // Result
      __result__#22)#23
  )#24,
  Foo{
    byte_value:b"bytes"#27#26,
    bool_value:false#29#28,
    uint_value:1u#31#30,
    double_value:1.1#33#32
  }#25.bar#34
)#35)ast");
}

}  // namespace
}  // namespace cel::test
