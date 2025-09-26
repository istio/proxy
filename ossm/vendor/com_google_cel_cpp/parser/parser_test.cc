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

#include "parser/parser.h"

#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/ast/ast_impl.h"
#include "common/constant.h"
#include "common/expr.h"
#include "common/source.h"
#include "internal/testing.h"
#include "parser/macro.h"
#include "parser/options.h"
#include "parser/parser_interface.h"
#include "parser/source_factory.h"
#include "testutil/expr_printer.h"

namespace google::api::expr::parser {

namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::ConstantKindCase;
using ::cel::ExprKindCase;
using ::cel::test::ExprPrinter;
using ::cel::expr::Expr;
using ::testing::HasSubstr;
using ::testing::Not;

struct TestInfo {
  TestInfo(const std::string& I, const std::string& P,
           const std::string& E = "", const std::string& L = "",
           const std::string& R = "", const std::string& M = "")
      : I(I), P(P), E(E), L(L), R(R), M(M) {}

  // I contains the input expression to be parsed.
  std::string I;

  // P contains the type/id adorned debug output of the expression tree.
  std::string P;

  // E contains the expected error output for a failed parse, or "" if the parse
  // is expected to be successful.
  std::string E;

  // L contains the expected source adorned debug output of the expression tree.
  std::string L;

  // R contains the expected enriched source info output of the expression tree.
  std::string R;

  // M contains the expected macro call output of hte expression tree.
  std::string M;
};

std::vector<TestInfo> test_cases = {
    // Simple test cases we started with
    {"x * 2",
     "_*_(\n"
     "  x^#1:Expr.Ident#,\n"
     "  2^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"x * 2u",
     "_*_(\n"
     "  x^#1:Expr.Ident#,\n"
     "  2u^#3:uint64#\n"
     ")^#2:Expr.Call#"},
    {"x * 2.0",
     "_*_(\n"
     "  x^#1:Expr.Ident#,\n"
     "  2.0^#3:double#\n"
     ")^#2:Expr.Call#"},
    {"\"\\u2764\"", "\"\u2764\"^#1:string#"},
    {"\"\u2764\"", "\"\u2764\"^#1:string#"},
    {"! false",
     "!_(\n"
     "  false^#2:bool#\n"
     ")^#1:Expr.Call#"},
    {"-a",
     "-_(\n"
     "  a^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"a.b(5)",
     "a^#1:Expr.Ident#.b(\n"
     "  5^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"a[3]",
     "_[_](\n"
     "  a^#1:Expr.Ident#,\n"
     "  3^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"SomeMessage{foo: 5, bar: \"xyz\"}",
     "SomeMessage{\n"
     "  foo:5^#3:int64#^#2:Expr.CreateStruct.Entry#,\n"
     "  bar:\"xyz\"^#5:string#^#4:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"[3, 4, 5]",
     "[\n"
     "  3^#2:int64#,\n"
     "  4^#3:int64#,\n"
     "  5^#4:int64#\n"
     "]^#1:Expr.CreateList#"},
    {"{foo: 5, bar: \"xyz\"}",
     "{\n"
     "  foo^#3:Expr.Ident#:5^#4:int64#^#2:Expr.CreateStruct.Entry#,\n"
     "  bar^#6:Expr.Ident#:\"xyz\"^#7:string#^#5:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"a > 5 && a < 10",
     "_&&_(\n"
     "  _>_(\n"
     "    a^#1:Expr.Ident#,\n"
     "    5^#3:int64#\n"
     "  )^#2:Expr.Call#,\n"
     "  _<_(\n"
     "    a^#4:Expr.Ident#,\n"
     "    10^#6:int64#\n"
     "  )^#5:Expr.Call#\n"
     ")^#7:Expr.Call#"},
    {"a < 5 || a > 10",
     "_||_(\n"
     "  _<_(\n"
     "    a^#1:Expr.Ident#,\n"
     "    5^#3:int64#\n"
     "  )^#2:Expr.Call#,\n"
     "  _>_(\n"
     "    a^#4:Expr.Ident#,\n"
     "    10^#6:int64#\n"
     "  )^#5:Expr.Call#\n"
     ")^#7:Expr.Call#"},
    {"{", "",
     "ERROR: <input>:1:2: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '}', '(', '.', ',', '-', '!', '\\u003F', 'true', 'false', 'null', "
     "NUM_FLOAT, "
     "NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n | {\n"
     " | .^"},

    // test cases from Go
    {"\"A\"", "\"A\"^#1:string#"},
    {"true", "true^#1:bool#"},
    {"false", "false^#1:bool#"},
    {"0", "0^#1:int64#"},
    {"42", "42^#1:int64#"},
    {"0u", "0u^#1:uint64#"},
    {"23u", "23u^#1:uint64#"},
    {"24u", "24u^#1:uint64#"},
    {"0xAu", "10u^#1:uint64#"},
    {"-0xA", "-10^#1:int64#"},
    {"0xA", "10^#1:int64#"},
    {"-1", "-1^#1:int64#"},
    {"4--4",
     "_-_(\n"
     "  4^#1:int64#,\n"
     "  -4^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"4--4.1",
     "_-_(\n"
     "  4^#1:int64#,\n"
     "  -4.1^#3:double#\n"
     ")^#2:Expr.Call#"},
    {"b\"abc\"", "b\"abc\"^#1:bytes#"},
    {"23.39", "23.39^#1:double#"},
    {"!a",
     "!_(\n"
     "  a^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"null", "null^#1:NullValue#"},
    {"a", "a^#1:Expr.Ident#"},
    {"a?b:c",
     "_?_:_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#,\n"
     "  c^#4:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a || b",
     "_||_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#2:Expr.Ident#\n"
     ")^#3:Expr.Call#"},
    {"a || b || c || d || e || f ",
     "_||_(\n"
     "  _||_(\n"
     "    _||_(\n"
     "      a^#1:Expr.Ident#,\n"
     "      b^#2:Expr.Ident#\n"
     "    )^#3:Expr.Call#,\n"
     "    c^#4:Expr.Ident#\n"
     "  )^#5:Expr.Call#,\n"
     "  _||_(\n"
     "    _||_(\n"
     "      d^#6:Expr.Ident#,\n"
     "      e^#8:Expr.Ident#\n"
     "    )^#9:Expr.Call#,\n"
     "    f^#10:Expr.Ident#\n"
     "  )^#11:Expr.Call#\n"
     ")^#7:Expr.Call#"},
    {"a && b",
     "_&&_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#2:Expr.Ident#\n"
     ")^#3:Expr.Call#"},
    {"a && b && c && d && e && f && g",
     "_&&_(\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      a^#1:Expr.Ident#,\n"
     "      b^#2:Expr.Ident#\n"
     "    )^#3:Expr.Call#,\n"
     "    _&&_(\n"
     "      c^#4:Expr.Ident#,\n"
     "      d^#6:Expr.Ident#\n"
     "    )^#7:Expr.Call#\n"
     "  )^#5:Expr.Call#,\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      e^#8:Expr.Ident#,\n"
     "      f^#10:Expr.Ident#\n"
     "    )^#11:Expr.Call#,\n"
     "    g^#12:Expr.Ident#\n"
     "  )^#13:Expr.Call#\n"
     ")^#9:Expr.Call#"},
    {"a && b && c && d || e && f && g && h",
     "_||_(\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      a^#1:Expr.Ident#,\n"
     "      b^#2:Expr.Ident#\n"
     "    )^#3:Expr.Call#,\n"
     "    _&&_(\n"
     "      c^#4:Expr.Ident#,\n"
     "      d^#6:Expr.Ident#\n"
     "    )^#7:Expr.Call#\n"
     "  )^#5:Expr.Call#,\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      e^#8:Expr.Ident#,\n"
     "      f^#9:Expr.Ident#\n"
     "    )^#10:Expr.Call#,\n"
     "    _&&_(\n"
     "      g^#11:Expr.Ident#,\n"
     "      h^#13:Expr.Ident#\n"
     "    )^#14:Expr.Call#\n"
     "  )^#12:Expr.Call#\n"
     ")^#15:Expr.Call#"},
    {"a + b",
     "_+_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a - b",
     "_-_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a * b",
     "_*_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a / b",
     "_/_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {
        "a % b",
        "_%_(\n"
        "  a^#1:Expr.Ident#,\n"
        "  b^#3:Expr.Ident#\n"
        ")^#2:Expr.Call#",
    },
    {"a in b",
     "@in(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a == b",
     "_==_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a != b",
     "_!=_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a > b",
     "_>_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a >= b",
     "_>=_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a < b",
     "_<_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a <= b",
     "_<=_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a.b", "a^#1:Expr.Ident#.b^#2:Expr.Select#"},
    {"a.b.c", "a^#1:Expr.Ident#.b^#2:Expr.Select#.c^#3:Expr.Select#"},
    {"a[b]",
     "_[_](\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"foo{ }", "foo{}^#1:Expr.CreateStruct#"},
    {"foo{ a:b }",
     "foo{\n"
     "  a:b^#3:Expr.Ident#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"foo{ a:b, c:d }",
     "foo{\n"
     "  a:b^#3:Expr.Ident#^#2:Expr.CreateStruct.Entry#,\n"
     "  c:d^#5:Expr.Ident#^#4:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"{}", "{}^#1:Expr.CreateStruct#"},
    {"{a:b, c:d}",
     "{\n"
     "  a^#3:Expr.Ident#:b^#4:Expr.Ident#^#2:Expr.CreateStruct.Entry#,\n"
     "  c^#6:Expr.Ident#:d^#7:Expr.Ident#^#5:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"[]", "[]^#1:Expr.CreateList#"},
    {"[a]",
     "[\n"
     "  a^#2:Expr.Ident#\n"
     "]^#1:Expr.CreateList#"},
    {"[a, b, c]",
     "[\n"
     "  a^#2:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#,\n"
     "  c^#4:Expr.Ident#\n"
     "]^#1:Expr.CreateList#"},
    {"(a)", "a^#1:Expr.Ident#"},
    {"((a))", "a^#1:Expr.Ident#"},
    {"a()", "a()^#1:Expr.Call#"},
    {"a(b)",
     "a(\n"
     "  b^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"a(b, c)",
     "a(\n"
     "  b^#2:Expr.Ident#,\n"
     "  c^#3:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"a.b()", "a^#1:Expr.Ident#.b()^#2:Expr.Call#"},
    {
        "a.b(c)",
        "a^#1:Expr.Ident#.b(\n"
        "  c^#3:Expr.Ident#\n"
        ")^#2:Expr.Call#",
        /* E */ "",
        "a^#1[1,0]#.b(\n"
        "  c^#3[1,4]#\n"
        ")^#2[1,3]#",
        "[1,0,0]^#[2,3,3]^#[3,4,4]",
    },
    {
        "aaa.bbb(ccc)",
        "aaa^#1:Expr.Ident#.bbb(\n"
        "  ccc^#3:Expr.Ident#\n"
        ")^#2:Expr.Call#",
        /* E */ "",
        "aaa^#1[1,0]#.bbb(\n"
        "  ccc^#3[1,8]#\n"
        ")^#2[1,7]#",
        "[1,0,2]^#[2,7,7]^#[3,8,10]",
    },

    // Parse error tests
    {"*@a | b", "",
     "ERROR: <input>:1:1: Syntax error: extraneous input '*' expecting {'[', "
     "'{', "
     "'(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | *@a | b\n"
     " | ^\n"
     "ERROR: <input>:1:2: Syntax error: token recognition error at: '@'\n"
     " | *@a | b\n"
     " | .^\n"
     "ERROR: <input>:1:5: Syntax error: token recognition error at: '| '\n"
     " | *@a | b\n"
     " | ....^\n"
     "ERROR: <input>:1:7: Syntax error: extraneous input 'b' expecting <EOF>\n"
     " | *@a | b\n"
     " | ......^"},
    {"a | b", "",
     "ERROR: <input>:1:3: Syntax error: token recognition error at: '| '\n"
     " | a | b\n"
     " | ..^\n"
     "ERROR: <input>:1:5: Syntax error: extraneous input 'b' expecting <EOF>\n"
     " | a | b\n"
     " | ....^"},
    {"?", "",
     "ERROR: <input>:1:1: Syntax error: mismatched input '?' expecting "
     "{'[', '{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, "
     "NUM_INT, NUM_UINT, STRING, BYTES, IDENTIFIER}\n | ?\n | ^\n"
     "ERROR: <input>:1:2: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', '{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, "
     "NUM_INT, NUM_UINT, STRING, BYTES, IDENTIFIER}\n | ?\n | .^\n"
     "ERROR: <input>:4294967295:0: <<nil>> parsetree"},
    {"t{>C}", "",
     "ERROR: <input>:1:3: Syntax error: extraneous input '>' expecting {'}', "
     "',', '\\u003F', IDENTIFIER, ESC_IDENTIFIER}\n | t{>C}\n | ..^\nERROR: "
     "<input>:1:5: "
     "Syntax error: "
     "mismatched input '}' expecting ':'\n | t{>C}\n | ....^"},

    // Macro tests
    {"has(m.f)", "m^#2:Expr.Ident#.f~test-only~^#4:Expr.Select#", "",
     "m^#2[1,4]#.f~test-only~^#4[1,3]#", "[2,4,4]^#[3,5,5]^#[4,3,3]",
     "has(\n"
     "  m^#2:Expr.Ident#.f^#3:Expr.Select#\n"
     ")^#4:has"},
    {"m.exists_one(v, f)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  0^#5:int64#,\n"
     "  // LoopCondition\n"
     "  true^#6:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    f^#4:Expr.Ident#,\n"
     "    _+_(\n"
     "      @result^#7:Expr.Ident#,\n"
     "      1^#8:int64#\n"
     "    )^#9:Expr.Call#,\n"
     "    @result^#10:Expr.Ident#\n"
     "  )^#11:Expr.Call#,\n"
     "  // Result\n"
     "  _==_(\n"
     "    @result^#12:Expr.Ident#,\n"
     "    1^#13:int64#\n"
     "  )^#14:Expr.Call#)^#15:Expr.Comprehension#",
     "", "", "",
     "m^#1:Expr.Ident#.exists_one(\n"
     "  v^#3:Expr.Ident#,\n"
     "  f^#4:Expr.Ident#\n"
     ")^#15:exists_one"},
    {"m.map(v, f)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  []^#5:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#6:bool#,\n"
     "  // LoopStep\n"
     "  _+_(\n"
     "    @result^#7:Expr.Ident#,\n"
     "    [\n"
     "      f^#4:Expr.Ident#\n"
     "    ]^#8:Expr.CreateList#\n"
     "  )^#9:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#10:Expr.Ident#)^#11:Expr.Comprehension#",
     "", "", "",
     "m^#1:Expr.Ident#.map(\n"
     "  v^#3:Expr.Ident#,\n"
     "  f^#4:Expr.Ident#\n"
     ")^#11:map"},
    {"m.map(v, p, f)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  []^#6:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#7:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    p^#4:Expr.Ident#,\n"
     "    _+_(\n"
     "      @result^#8:Expr.Ident#,\n"
     "      [\n"
     "        f^#5:Expr.Ident#\n"
     "      ]^#9:Expr.CreateList#\n"
     "    )^#10:Expr.Call#,\n"
     "    @result^#11:Expr.Ident#\n"
     "  )^#12:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#13:Expr.Ident#)^#14:Expr.Comprehension#",
     "", "", "",
     "m^#1:Expr.Ident#.map(\n"
     "  v^#3:Expr.Ident#,\n"
     "  p^#4:Expr.Ident#,\n"
     "  f^#5:Expr.Ident#\n"
     ")^#14:map"},
    {"m.filter(v, p)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  []^#5:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#6:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    p^#4:Expr.Ident#,\n"
     "    _+_(\n"
     "      @result^#7:Expr.Ident#,\n"
     "      [\n"
     "        v^#3:Expr.Ident#\n"
     "      ]^#8:Expr.CreateList#\n"
     "    )^#9:Expr.Call#,\n"
     "    @result^#10:Expr.Ident#\n"
     "  )^#11:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#12:Expr.Ident#)^#13:Expr.Comprehension#",
     "", "", "",
     "m^#1:Expr.Ident#.filter(\n"
     "  v^#3:Expr.Ident#,\n"
     "  p^#4:Expr.Ident#\n"
     ")^#13:filter"},

    // Tests from Java parser
    {"[] + [1,2,3,] + [4]",
     "_+_(\n"
     "  _+_(\n"
     "    []^#1:Expr.CreateList#,\n"
     "    [\n"
     "      1^#4:int64#,\n"
     "      2^#5:int64#,\n"
     "      3^#6:int64#\n"
     "    ]^#3:Expr.CreateList#\n"
     "  )^#2:Expr.Call#,\n"
     "  [\n"
     "    4^#9:int64#\n"
     "  ]^#8:Expr.CreateList#\n"
     ")^#7:Expr.Call#"},
    {"{1:2u, 2:3u}",
     "{\n"
     "  1^#3:int64#:2u^#4:uint64#^#2:Expr.CreateStruct.Entry#,\n"
     "  2^#6:int64#:3u^#7:uint64#^#5:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"TestAllTypes{single_int32: 1, single_int64: 2}",
     "TestAllTypes{\n"
     "  single_int32:1^#3:int64#^#2:Expr.CreateStruct.Entry#,\n"
     "  single_int64:2^#5:int64#^#4:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"TestAllTypes(){single_int32: 1, single_int64: 2}", "",
     "ERROR: <input>:1:15: Syntax error: mismatched input '{' expecting <EOF>\n"
     " | TestAllTypes(){single_int32: 1, single_int64: 2}\n"
     " | ..............^"},
    {"size(x) == x.size()",
     "_==_(\n"
     "  size(\n"
     "    x^#2:Expr.Ident#\n"
     "  )^#1:Expr.Call#,\n"
     "  x^#4:Expr.Ident#.size()^#5:Expr.Call#\n"
     ")^#3:Expr.Call#"},
    {"1 + $", "",
     "ERROR: <input>:1:5: Syntax error: token recognition error at: '$'\n"
     " | 1 + $\n"
     " | ....^\n"
     "ERROR: <input>:1:6: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | 1 + $\n"
     " | .....^"},
    {"1 + 2\n"
     "3 +",
     "",
     "ERROR: <input>:2:1: Syntax error: mismatched input '3' expecting <EOF>\n"
     " | 3 +\n"
     " | ^"},
    {"\"\\\"\"", "\"\\\"\"^#1:string#"},
    {"[1,3,4][0]",
     "_[_](\n"
     "  [\n"
     "    1^#2:int64#,\n"
     "    3^#3:int64#,\n"
     "    4^#4:int64#\n"
     "  ]^#1:Expr.CreateList#,\n"
     "  0^#6:int64#\n"
     ")^#5:Expr.Call#"},
    {"1.all(2, 3)", "",
     "ERROR: <input>:1:7: all() variable name must be a simple identifier\n"
     " | 1.all(2, 3)\n"
     " | ......^"},
    {"x[\"a\"].single_int32 == 23",
     "_==_(\n"
     "  _[_](\n"
     "    x^#1:Expr.Ident#,\n"
     "    \"a\"^#3:string#\n"
     "  )^#2:Expr.Call#.single_int32^#4:Expr.Select#,\n"
     "  23^#6:int64#\n"
     ")^#5:Expr.Call#"},
    {"x.single_nested_message != null",
     "_!=_(\n"
     "  x^#1:Expr.Ident#.single_nested_message^#2:Expr.Select#,\n"
     "  null^#4:NullValue#\n"
     ")^#3:Expr.Call#"},
    {"false && !true || false ? 2 : 3",
     "_?_:_(\n"
     "  _||_(\n"
     "    _&&_(\n"
     "      false^#1:bool#,\n"
     "      !_(\n"
     "        true^#3:bool#\n"
     "      )^#2:Expr.Call#\n"
     "    )^#4:Expr.Call#,\n"
     "    false^#5:bool#\n"
     "  )^#6:Expr.Call#,\n"
     "  2^#8:int64#,\n"
     "  3^#9:int64#\n"
     ")^#7:Expr.Call#"},
    {"b\"abc\" + B\"def\"",
     "_+_(\n"
     "  b\"abc\"^#1:bytes#,\n"
     "  b\"def\"^#3:bytes#\n"
     ")^#2:Expr.Call#"},
    {"1 + 2 * 3 - 1 / 2 == 6 % 1",
     "_==_(\n"
     "  _-_(\n"
     "    _+_(\n"
     "      1^#1:int64#,\n"
     "      _*_(\n"
     "        2^#3:int64#,\n"
     "        3^#5:int64#\n"
     "      )^#4:Expr.Call#\n"
     "    )^#2:Expr.Call#,\n"
     "    _/_(\n"
     "      1^#7:int64#,\n"
     "      2^#9:int64#\n"
     "    )^#8:Expr.Call#\n"
     "  )^#6:Expr.Call#,\n"
     "  _%_(\n"
     "    6^#11:int64#,\n"
     "    1^#13:int64#\n"
     "  )^#12:Expr.Call#\n"
     ")^#10:Expr.Call#"},
    {"---a",
     "-_(\n"
     "  a^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"1 + +", "",
     "ERROR: <input>:1:5: Syntax error: mismatched input '+' expecting {'[', "
     "'{',"
     " '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT,"
     " STRING, BYTES, IDENTIFIER}\n"
     " | 1 + +\n"
     " | ....^\n"
     "ERROR: <input>:1:6: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | 1 + +\n"
     " | .....^"},
    {"\"abc\" + \"def\"",
     "_+_(\n"
     "  \"abc\"^#1:string#,\n"
     "  \"def\"^#3:string#\n"
     ")^#2:Expr.Call#"},
    {"{\"a\": 1}.\"a\"", "",
     "ERROR: <input>:1:10: Syntax error: no viable alternative at input "
     "'.\"a\"'\n"
     " | {\"a\": 1}.\"a\"\n"
     " | .........^"},
    {"\"\\xC3\\XBF\"", "\"Ã¿\"^#1:string#"},
    {"\"\\303\\277\"", "\"Ã¿\"^#1:string#"},
    {"\"hi\\u263A \\u263Athere\"", "\"hi☺ ☺there\"^#1:string#"},
    {"\"\\U000003A8\\?\"", "\"Ψ?\"^#1:string#"},
    {"\"\\a\\b\\f\\n\\r\\t\\v'\\\"\\\\\\? Legal escapes\"",
     "\"\\x07\\x08\\x0c\\n\\r\\t\\x0b'\\\"\\\\? Legal escapes\"^#1:string#"},
    {"\"\\xFh\"", "",
     "ERROR: <input>:1:1: Syntax error: token recognition error at: '\"\\xFh'\n"
     " | \"\\xFh\"\n"
     " | ^\n"
     "ERROR: <input>:1:6: Syntax error: token recognition error at: '\"'\n"
     " | \"\\xFh\"\n"
     " | .....^\n"
     "ERROR: <input>:1:7: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | \"\\xFh\"\n"
     " | ......^"},
    {"\"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"", "",
     "ERROR: <input>:1:1: Syntax error: token recognition error at: "
     "'\"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>'\n"
     " | \"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"\n"
     " | ^\n"
     "ERROR: <input>:1:42: Syntax error: token recognition error at: '\"'\n"
     " | \"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"\n"
     " | .........................................^\n"
     "ERROR: <input>:1:43: Syntax error: mismatched input '<EOF>' expecting "
     "{'[',"
     " '{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | \"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"\n"
     " | ..........................................^"},
    {"'😁' in ['😁', '😑', '😦']",
     "@in(\n"
     "  \"😁\"^#1:string#,\n"
     "  [\n"
     "    \"😁\"^#4:string#,\n"
     "    \"😑\"^#5:string#,\n"
     "    \"😦\"^#6:string#\n"
     "  ]^#3:Expr.CreateList#\n"
     ")^#2:Expr.Call#"},
    {"'\u00ff' in ['\u00ff', '\u00ff', '\u00ff']",
     "@in(\n"
     "  \"\u00ff\"^#1:string#,\n"
     "  [\n"
     "    \"\u00ff\"^#4:string#,\n"
     "    \"\u00ff\"^#5:string#,\n"
     "    \"\u00ff\"^#6:string#\n"
     "  ]^#3:Expr.CreateList#\n"
     ")^#2:Expr.Call#"},
    {"'\u00ff' in ['\uffff', '\U00100000', '\U0010ffff']",
     "@in(\n"
     "  \"\u00ff\"^#1:string#,\n"
     "  [\n"
     "    \"\uffff\"^#4:string#,\n"
     "    \"\U00100000\"^#5:string#,\n"
     "    \"\U0010ffff\"^#6:string#\n"
     "  ]^#3:Expr.CreateList#\n"
     ")^#2:Expr.Call#"},
    {"'\u00ff' in ['\U00100000', '\uffff', '\U0010ffff']",
     "@in(\n"
     "  \"\u00ff\"^#1:string#,\n"
     "  [\n"
     "    \"\U00100000\"^#4:string#,\n"
     "    \"\uffff\"^#5:string#,\n"
     "    \"\U0010ffff\"^#6:string#\n"
     "  ]^#3:Expr.CreateList#\n"
     ")^#2:Expr.Call#"},
    {"'😁' in ['😁', '😑', '😦']\n"
     "   && in.😁",
     "",
     "ERROR: <input>:2:7: Syntax error: extraneous input 'in' expecting {'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " |    && in.😁\n"
     " | ......^\n"
     "ERROR: <input>:2:10: Syntax error: token recognition error at: '😁'\n"
     " |    && in.😁\n"
     " | .........＾\n"
     "ERROR: <input>:2:11: Syntax error: no viable alternative at input '.'\n"
     " |    && in.😁\n"
     " | .........．^"},
    {"as", "",
     "ERROR: <input>:1:1: reserved identifier: as\n"
     " | as\n"
     " | ^"},
    {"break", "",
     "ERROR: <input>:1:1: reserved identifier: break\n"
     " | break\n"
     " | ^"},
    {"const", "",
     "ERROR: <input>:1:1: reserved identifier: const\n"
     " | const\n"
     " | ^"},
    {"continue", "",
     "ERROR: <input>:1:1: reserved identifier: continue\n"
     " | continue\n"
     " | ^"},
    {"else", "",
     "ERROR: <input>:1:1: reserved identifier: else\n"
     " | else\n"
     " | ^"},
    {"for", "",
     "ERROR: <input>:1:1: reserved identifier: for\n"
     " | for\n"
     " | ^"},
    {"function", "",
     "ERROR: <input>:1:1: reserved identifier: function\n"
     " | function\n"
     " | ^"},
    {"if", "",
     "ERROR: <input>:1:1: reserved identifier: if\n"
     " | if\n"
     " | ^"},
    {"import", "",
     "ERROR: <input>:1:1: reserved identifier: import\n"
     " | import\n"
     " | ^"},
    {"in", "",
     "ERROR: <input>:1:1: Syntax error: mismatched input 'in' expecting {'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | in\n"
     " | ^\n"
     "ERROR: <input>:1:3: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | in\n"
     " | ..^"},
    {"let", "",
     "ERROR: <input>:1:1: reserved identifier: let\n"
     " | let\n"
     " | ^"},
    {"loop", "",
     "ERROR: <input>:1:1: reserved identifier: loop\n"
     " | loop\n"
     " | ^"},
    {"package", "",
     "ERROR: <input>:1:1: reserved identifier: package\n"
     " | package\n"
     " | ^"},
    {"namespace", "",
     "ERROR: <input>:1:1: reserved identifier: namespace\n"
     " | namespace\n"
     " | ^"},
    {"return", "",
     "ERROR: <input>:1:1: reserved identifier: return\n"
     " | return\n"
     " | ^"},
    {"var", "",
     "ERROR: <input>:1:1: reserved identifier: var\n"
     " | var\n"
     " | ^"},
    {"void", "",
     "ERROR: <input>:1:1: reserved identifier: void\n"
     " | void\n"
     " | ^"},
    {"while", "",
     "ERROR: <input>:1:1: reserved identifier: while\n"
     " | while\n"
     " | ^"},
    {"[1, 2, 3].map(var, var * var)", "",
     "ERROR: <input>:1:15: reserved identifier: var\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | ..............^\n"
     "ERROR: <input>:1:15: map() variable name must be a simple identifier\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | ..............^\n"
     "ERROR: <input>:1:20: reserved identifier: var\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | ...................^\n"
     "ERROR: <input>:1:26: reserved identifier: var\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | .........................^"},
    {"[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
     "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
     "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
     "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[['too many']]]]]]]]]]]]]]]]]]]]]]]]]]]]"
     "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
     "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
     "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
     "]]]]]]",
     "", "Expression recursion limit exceeded. limit: 32", "", "", ""},
    {
        // Note, the ANTLR parse stack may recurse much more deeply and permit
        // more detailed expressions than the visitor can recurse over in
        // practice.
        "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[['just fine'],[1],[2],[3],[4],[5]]]]]]]"
        "]]]]]]]]]]]]]]]]]]]]]]]]",
        "",  // parse output not validated as it is too large.
        "",
        "",
        "",
        "",
    },
    {
        "[\n\t\r[\n\t\r[\n\t\r]\n\t\r]\n\t\r",
        "",  // parse output not validated as it is too large.
        "ERROR: <input>:6:3: Syntax error: mismatched input '<EOF>' expecting "
        "{']', ','}\n"
        " |  \r\n"
        " | ..^",
    },

    // Identifier quoting syntax tests.
    {"a.`b`", "a^#1:Expr.Ident#.b^#2:Expr.Select#"},
    {"a.`b-c`", "a^#1:Expr.Ident#.b-c^#2:Expr.Select#"},
    {"a.`b c`", "a^#1:Expr.Ident#.b c^#2:Expr.Select#"},
    {"a.`b/c`", "a^#1:Expr.Ident#.b/c^#2:Expr.Select#"},
    {"a.`b.c`", "a^#1:Expr.Ident#.b.c^#2:Expr.Select#"},
    {"a.`in`", "a^#1:Expr.Ident#.in^#2:Expr.Select#"},
    {"A{`b`: 1}",
     "A{\n"
     "  b:1^#3:int64#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"A{`b-c`: 1}",
     "A{\n"
     "  b-c:1^#3:int64#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"A{`b c`: 1}",
     "A{\n"
     "  b c:1^#3:int64#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"A{`b/c`: 1}",
     "A{\n"
     "  b/c:1^#3:int64#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"A{`b.c`: 1}",
     "A{\n"
     "  b.c:1^#3:int64#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"A{`in`: 1}",
     "A{\n"
     "  in:1^#3:int64#^#2:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"has(a.`b/c`)", "a^#2:Expr.Ident#.b/c~test-only~^#4:Expr.Select#"},
    // Unsupported quoted identifiers.
    {"a.`b\tc`", "",
     "ERROR: <input>:1:3: Syntax error: token recognition error at: '`b\\t'\n"
     " | a.`b c`\n"
     " | ..^\n"
     "ERROR: <input>:1:7: Syntax error: token recognition error at: '`'\n"
     " | a.`b c`\n"
     " | ......^"},
    {"a.`@foo`", "",
     "ERROR: <input>:1:3: Syntax error: token recognition error at: '`@'\n"
     " | a.`@foo`\n"
     " | ..^\n"
     "ERROR: <input>:1:8: Syntax error: token recognition error at: '`'\n"
     " | a.`@foo`\n"
     " | .......^"},
    {"a.`$foo`", "",
     "ERROR: <input>:1:3: Syntax error: token recognition error at: '`$'\n"
     " | a.`$foo`\n"
     " | ..^\n"
     "ERROR: <input>:1:8: Syntax error: token recognition error at: '`'\n"
     " | a.`$foo`\n"
     " | .......^"},
    {"`a.b`", "",
     "ERROR: <input>:1:1: Syntax error: mismatched input '`a.b`' expecting "
     "{'[', '{', "
     "'(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, "
     "BYTES, IDENTIFIER}\n"
     " | `a.b`\n"
     " | ^"},
    {"`a.b`()", "",
     "ERROR: <input>:1:1: Syntax error: extraneous input '`a.b`' expecting "
     "{'[', '{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, "
     "NUM_INT, NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | `a.b`()\n"
     " | ^\n"
     "ERROR: <input>:1:7: Syntax error: mismatched input ')' expecting {'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM"
     "_INT, NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | `a.b`()\n"
     " | ......^"},
    {"foo.`a.b`()", "",
     "ERROR: <input>:1:10: Syntax error: mismatched input '(' expecting <EOF>\n"
     " | foo.`a.b`()\n"
     " | .........^"},

    // Macro calls tests
    {"x.filter(y, y.filter(z, z > 0))",
     "__comprehension__(\n"
     "  // Variable\n"
     "  y,\n"
     "  // Target\n"
     "  x^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  []^#19:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#20:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    __comprehension__(\n"
     "      // Variable\n"
     "      z,\n"
     "      // Target\n"
     "      y^#4:Expr.Ident#,\n"
     "      // Accumulator\n"
     "      @result,\n"
     "      // Init\n"
     "      []^#10:Expr.CreateList#,\n"
     "      // LoopCondition\n"
     "      true^#11:bool#,\n"
     "      // LoopStep\n"
     "      _?_:_(\n"
     "        _>_(\n"
     "          z^#7:Expr.Ident#,\n"
     "          0^#9:int64#\n"
     "        )^#8:Expr.Call#,\n"
     "        _+_(\n"
     "          @result^#12:Expr.Ident#,\n"
     "          [\n"
     "            z^#6:Expr.Ident#\n"
     "          ]^#13:Expr.CreateList#\n"
     "        )^#14:Expr.Call#,\n"
     "        @result^#15:Expr.Ident#\n"
     "      )^#16:Expr.Call#,\n"
     "      // Result\n"
     "      @result^#17:Expr.Ident#)^#18:Expr.Comprehension#,\n"
     "    _+_(\n"
     "      @result^#21:Expr.Ident#,\n"
     "      [\n"
     "        y^#3:Expr.Ident#\n"
     "      ]^#22:Expr.CreateList#\n"
     "    )^#23:Expr.Call#,\n"
     "    @result^#24:Expr.Ident#\n"
     "  )^#25:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#26:Expr.Ident#)^#27:Expr.Comprehension#"
     "",
     "", "", "",
     "x^#1:Expr.Ident#.filter(\n"
     "  y^#3:Expr.Ident#,\n"
     "  ^#18:filter#\n"
     ")^#27:filter#,\n"
     "y^#4:Expr.Ident#.filter(\n"
     "  z^#6:Expr.Ident#,\n"
     "  _>_(\n"
     "    z^#7:Expr.Ident#,\n"
     "    0^#9:int64#\n"
     "  )^#8:Expr.Call#\n"
     ")^#18:filter"},
    {"has(a.b).filter(c, c)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  c,\n"
     "  // Target\n"
     "  a^#2:Expr.Ident#.b~test-only~^#4:Expr.Select#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  []^#8:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#9:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    c^#7:Expr.Ident#,\n"
     "    _+_(\n"
     "      @result^#10:Expr.Ident#,\n"
     "      [\n"
     "        c^#6:Expr.Ident#\n"
     "      ]^#11:Expr.CreateList#\n"
     "    )^#12:Expr.Call#,\n"
     "    @result^#13:Expr.Ident#\n"
     "  )^#14:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#15:Expr.Ident#)^#16:Expr.Comprehension#",
     "", "", "",
     "^#4:has#.filter(\n"
     "  c^#6:Expr.Ident#,\n"
     "  c^#7:Expr.Ident#\n"
     ")^#16:filter#,\n"
     "has(\n"
     "  a^#2:Expr.Ident#.b^#3:Expr.Select#\n"
     ")^#4:has"},
    {"x.filter(y, y.exists(z, has(z.a)) && y.exists(z, has(z.b)))",
     "__comprehension__(\n"
     "  // Variable\n"
     "  y,\n"
     "  // Target\n"
     "  x^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  []^#35:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#36:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    _&&_(\n"
     "      __comprehension__(\n"
     "        // Variable\n"
     "        z,\n"
     "        // Target\n"
     "        y^#4:Expr.Ident#,\n"
     "        // Accumulator\n"
     "        @result,\n"
     "        // Init\n"
     "        false^#11:bool#,\n"
     "        // LoopCondition\n"
     "        @not_strictly_false(\n"
     "          !_(\n"
     "            @result^#12:Expr.Ident#\n"
     "          )^#13:Expr.Call#\n"
     "        )^#14:Expr.Call#,\n"
     "        // LoopStep\n"
     "        _||_(\n"
     "          @result^#15:Expr.Ident#,\n"
     "          z^#8:Expr.Ident#.a~test-only~^#10:Expr.Select#\n"
     "        )^#16:Expr.Call#,\n"
     "        // Result\n"
     "        @result^#17:Expr.Ident#)^#18:Expr.Comprehension#,\n"
     "      __comprehension__(\n"
     "        // Variable\n"
     "        z,\n"
     "        // Target\n"
     "        y^#19:Expr.Ident#,\n"
     "        // Accumulator\n"
     "        @result,\n"
     "        // Init\n"
     "        false^#26:bool#,\n"
     "        // LoopCondition\n"
     "        @not_strictly_false(\n"
     "          !_(\n"
     "            @result^#27:Expr.Ident#\n"
     "          )^#28:Expr.Call#\n"
     "        )^#29:Expr.Call#,\n"
     "        // LoopStep\n"
     "        _||_(\n"
     "          @result^#30:Expr.Ident#,\n"
     "          z^#23:Expr.Ident#.b~test-only~^#25:Expr.Select#\n"
     "        )^#31:Expr.Call#,\n"
     "        // Result\n"
     "        @result^#32:Expr.Ident#)^#33:Expr.Comprehension#\n"
     "    )^#34:Expr.Call#,\n"
     "    _+_(\n"
     "      @result^#37:Expr.Ident#,\n"
     "      [\n"
     "        y^#3:Expr.Ident#\n"
     "      ]^#38:Expr.CreateList#\n"
     "    )^#39:Expr.Call#,\n"
     "    @result^#40:Expr.Ident#\n"
     "  )^#41:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#42:Expr.Ident#)^#43:Expr.Comprehension#",
     "", "", "",
     "x^#1:Expr.Ident#.filter(\n"
     "  y^#3:Expr.Ident#,\n"
     "  _&&_(\n"
     "    ^#18:exists#,\n"
     "    ^#33:exists#\n"
     "  )^#34:Expr.Call#\n"
     ")^#43:filter#,\n"
     "y^#19:Expr.Ident#.exists(\n"
     "  z^#21:Expr.Ident#,\n"
     "  ^#25:has#\n"
     ")^#33:exists#,\n"
     "has(\n"
     "  z^#23:Expr.Ident#.b^#24:Expr.Select#\n"
     ")^#25:has#,\n"
     "y^#4:Expr.Ident#."
     "exists(\n"
     "  z^#6:Expr.Ident#,\n"
     "  ^#10:has#\n"
     ")^#18:exists#,\n"
     "has(\n"
     "  z^#8:Expr.Ident#.a^#9:Expr.Select#\n"
     ")^#10:has"},
    {"has(a.b).asList().exists(c, c)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  c,\n"
     "  // Target\n"
     "  a^#2:Expr.Ident#.b~test-only~^#4:Expr.Select#.asList()^#5:Expr.Call#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  false^#9:bool#,\n"
     "  // LoopCondition\n"
     "  @not_strictly_false(\n"
     "    !_(\n"
     "      @result^#10:Expr.Ident#\n"
     "    )^#11:Expr.Call#\n"
     "  )^#12:Expr.Call#,\n"
     "  // LoopStep\n"
     "  _||_(\n"
     "    @result^#13:Expr.Ident#,\n"
     "    c^#8:Expr.Ident#\n"
     "  )^#14:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#15:Expr.Ident#)^#16:Expr.Comprehension#",
     "", "", "",
     "^#4:has#.asList()^#5:Expr.Call#.exists(\n"
     "  c^#7:Expr.Ident#,\n"
     "  c^#8:Expr.Ident#\n"
     ")^#16:exists#,\n"
     "has(\n"
     "  a^#2:Expr.Ident#.b^#3:Expr.Select#\n"
     ")^#4:has"},
    {"[has(a.b), has(c.d)].exists(e, e)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  e,\n"
     "  // Target\n"
     "  [\n"
     "    a^#3:Expr.Ident#.b~test-only~^#5:Expr.Select#,\n"
     "    c^#7:Expr.Ident#.d~test-only~^#9:Expr.Select#\n"
     "  ]^#1:Expr.CreateList#,\n"
     "  // Accumulator\n"
     "  @result,\n"
     "  // Init\n"
     "  false^#13:bool#,\n"
     "  // LoopCondition\n"
     "  @not_strictly_false(\n"
     "    !_(\n"
     "      @result^#14:Expr.Ident#\n"
     "    )^#15:Expr.Call#\n"
     "  )^#16:Expr.Call#,\n"
     "  // LoopStep\n"
     "  _||_(\n"
     "    @result^#17:Expr.Ident#,\n"
     "    e^#12:Expr.Ident#\n"
     "  )^#18:Expr.Call#,\n"
     "  // Result\n"
     "  @result^#19:Expr.Ident#)^#20:Expr.Comprehension#",
     "", "", "",
     "[\n"
     "  ^#5:has#,\n"
     "  ^#9:has#\n"
     "]^#1:Expr.CreateList#.exists(\n"
     "  e^#11:Expr.Ident#,\n"
     "  e^#12:Expr.Ident#\n"
     ")^#20:exists#,\n"
     "has(\n"
     "  c^#7:Expr.Ident#.d^#8:Expr.Select#\n"
     ")^#9:has#,\n"
     "has(\n"
     "  a^#3:Expr.Ident#.b^#4:Expr.Select#\n"
     ")^#5:has"},
    {"b'\\UFFFFFFFF'", "",
     "ERROR: <input>:1:1: Invalid bytes literal: Illegal escape sequence: "
     "Unicode escape sequence \\U cannot be used in bytes literals\n | "
     "b'\\UFFFFFFFF'\n | ^"},
    {"a.?b[?0] && a[?c]",
     "_&&_(\n  _[?_](\n    _?._(\n      a^#1:Expr.Ident#,\n      "
     "\"b\"^#3:string#\n    )^#2:Expr.Call#,\n    0^#5:int64#\n  "
     ")^#4:Expr.Call#,\n  _[?_](\n    a^#6:Expr.Ident#,\n    "
     "c^#8:Expr.Ident#\n  )^#7:Expr.Call#\n)^#9:Expr.Call#"},
    {"{?'key': value}",
     "{\n  "
     "?\"key\"^#3:string#:value^#4:Expr.Ident#^#2:Expr.CreateStruct.Entry#\n}^#"
     "1:Expr.CreateStruct#"},
    {"[?a, ?b]",
     "[\n  ?a^#2:Expr.Ident#,\n  ?b^#3:Expr.Ident#\n]^#1:Expr.CreateList#"},
    {"[?a[?b]]",
     "[\n  ?_[?_](\n    a^#2:Expr.Ident#,\n    b^#4:Expr.Ident#\n  "
     ")^#3:Expr.Call#\n]^#1:Expr.CreateList#"},
    {"Msg{?field: value}",
     "Msg{\n  "
     "?field:value^#3:Expr.Ident#^#2:Expr.CreateStruct.Entry#\n}^#1:Expr."
     "CreateStruct#"},
    {"m.optMap(v, f)",
     "_?_:_(\n  m^#1:Expr.Ident#.hasValue()^#6:Expr.Call#,\n  optional.of(\n   "
     " __comprehension__(\n      // Variable\n      #unused,\n      // "
     "Target\n      []^#7:Expr.CreateList#,\n      // Accumulator\n      v,\n  "
     "    // Init\n      m^#5:Expr.Ident#.value()^#8:Expr.Call#,\n      // "
     "LoopCondition\n      false^#9:bool#,\n      // LoopStep\n      "
     "v^#3:Expr.Ident#,\n      // Result\n      "
     "f^#4:Expr.Ident#)^#10:Expr.Comprehension#\n  )^#11:Expr.Call#,\n  "
     "optional.none()^#12:Expr.Call#\n)^#13:Expr.Call#"},
    {"m.optFlatMap(v, f)",
     "_?_:_(\n  m^#1:Expr.Ident#.hasValue()^#6:Expr.Call#,\n  "
     "__comprehension__(\n    // Variable\n    #unused,\n    // Target\n    "
     "[]^#7:Expr.CreateList#,\n    // Accumulator\n    v,\n    // Init\n    "
     "m^#5:Expr.Ident#.value()^#8:Expr.Call#,\n    // LoopCondition\n    "
     "false^#9:bool#,\n    // LoopStep\n    v^#3:Expr.Ident#,\n    // Result\n "
     "   f^#4:Expr.Ident#)^#10:Expr.Comprehension#,\n  "
     "optional.none()^#11:Expr.Call#\n)^#12:Expr.Call#"}};

absl::string_view ConstantKind(const cel::Constant& c) {
  switch (c.kind_case()) {
    case ConstantKindCase::kBool:
      return "bool";
    case ConstantKindCase::kInt:
      return "int64";
    case ConstantKindCase::kUint:
      return "uint64";
    case ConstantKindCase::kDouble:
      return "double";
    case ConstantKindCase::kString:
      return "string";
    case ConstantKindCase::kBytes:
      return "bytes";
    case ConstantKindCase::kNull:
      return "NullValue";
    default:
      return "unspecified_constant";
  }
}

absl::string_view ExprKind(const cel::Expr& e) {
  switch (e.kind_case()) {
    case ExprKindCase::kConstant:
      // special cased, this doesn't appear.
      return "Expr.Constant";
    case ExprKindCase::kIdentExpr:
      return "Expr.Ident";
    case ExprKindCase::kSelectExpr:
      return "Expr.Select";
    case ExprKindCase::kCallExpr:
      return "Expr.Call";
    case ExprKindCase::kListExpr:
      return "Expr.CreateList";
    case ExprKindCase::kMapExpr:
    case ExprKindCase::kStructExpr:
      return "Expr.CreateStruct";
    case ExprKindCase::kComprehensionExpr:
      return "Expr.Comprehension";
    default:
      return "unspecified_expr";
  }
}

class KindAndIdAdorner : public cel::test::ExpressionAdorner {
 public:
  // Use default source_info constructor to make source_info "optional". This
  // will prevent macro_calls lookups from interfering with adorning expressions
  // that don't need to use macro_calls, such as the parsed AST.
  explicit KindAndIdAdorner(
      const cel::expr::SourceInfo& source_info =
          cel::expr::SourceInfo::default_instance())
      : source_info_(source_info) {}

  std::string Adorn(const cel::Expr& e) const override {
    // source_info_ might be empty on non-macro_calls tests
    if (source_info_.macro_calls_size() != 0 &&
        source_info_.macro_calls().contains(e.id())) {
      return absl::StrFormat(
          "^#%d:%s#", e.id(),
          source_info_.macro_calls().at(e.id()).call_expr().function());
    }

    if (e.has_const_expr()) {
      auto& const_expr = e.const_expr();
      return absl::StrCat("^#", e.id(), ":", ConstantKind(const_expr), "#");
    } else {
      return absl::StrCat("^#", e.id(), ":", ExprKind(e), "#");
    }
  }

  std::string AdornStructField(const cel::StructExprField& e) const override {
    return absl::StrFormat("^#%d:Expr.CreateStruct.Entry#", e.id());
  }

  std::string AdornMapEntry(const cel::MapExprEntry& e) const override {
    return absl::StrFormat("^#%d:Expr.CreateStruct.Entry#", e.id());
  }

 private:
  const cel::expr::SourceInfo& source_info_;
};

class LocationAdorner : public cel::test::ExpressionAdorner {
 public:
  explicit LocationAdorner(const cel::expr::SourceInfo& source_info)
      : source_info_(source_info) {}

  std::string Adorn(const cel::Expr& e) const override {
    return LocationToString(e.id());
  }

  std::string AdornStructField(const cel::StructExprField& e) const override {
    return LocationToString(e.id());
  }

  std::string AdornMapEntry(const cel::MapExprEntry& e) const override {
    return LocationToString(e.id());
  }

 private:
  std::string LocationToString(int64_t id) const {
    auto loc = GetLocation(id);
    if (loc) {
      return absl::StrFormat("^#%d[%d,%d]#", id, loc->first, loc->second);
    } else {
      return absl::StrFormat("^#%d[NO_POS]#", id);
    }
  }

  absl::optional<std::pair<int32_t, int32_t>> GetLocation(int64_t id) const {
    absl::optional<std::pair<int32_t, int32_t>> location;
    const auto& positions = source_info_.positions();
    if (positions.find(id) == positions.end()) {
      return location;
    }
    int32_t pos = positions.at(id);

    int32_t line = 1;
    for (int i = 0; i < source_info_.line_offsets_size(); ++i) {
      if (source_info_.line_offsets(i) > pos) {
        break;
      } else {
        line += 1;
      }
    }
    int32_t col = pos;
    if (line > 1) {
      col = pos - source_info_.line_offsets(line - 2);
    }
    return std::make_pair(line, col);
  }

  const cel::expr::SourceInfo& source_info_;
};

std::string ConvertEnrichedSourceInfoToString(
    const EnrichedSourceInfo& enriched_source_info) {
  std::vector<std::string> offsets;
  for (const auto& offset : enriched_source_info.offsets()) {
    offsets.push_back(absl::StrFormat(
        "[%d,%d,%d]", offset.first, offset.second.first, offset.second.second));
  }
  return absl::StrJoin(offsets, "^#");
}

std::string ConvertMacroCallsToString(
    const cel::expr::SourceInfo& source_info) {
  KindAndIdAdorner macro_calls_adorner(source_info);
  ExprPrinter w(macro_calls_adorner);
  // Use a list so we can sort the macro calls ensuring order for appending
  std::vector<std::pair<int64_t, cel::expr::Expr>> macro_calls;
  for (auto pair : source_info.macro_calls()) {
    // Set ID to the map key for the adorner
    pair.second.set_id(pair.first);
    macro_calls.push_back(pair);
  }
  // Sort in reverse because the first macro will have the highest id
  absl::c_sort(macro_calls,
               [](const std::pair<int64_t, cel::expr::Expr>& p1,
                  const std::pair<int64_t, cel::expr::Expr>& p2) {
                 return p1.first > p2.first;
               });
  std::string result = "";
  for (const auto& pair : macro_calls) {
    result += w.PrintProto(pair.second) += ",\n";
  }
  // substring last ",\n"
  return result.substr(0, result.size() - 3);
}

class ExpressionTest : public testing::TestWithParam<TestInfo> {};

TEST_P(ExpressionTest, Parse) {
  const TestInfo& test_info = GetParam();
  ParserOptions options;
  options.enable_hidden_accumulator_var = true;
  if (!test_info.M.empty()) {
    options.add_macro_calls = true;
  }
  options.enable_optional_syntax = true;
  options.enable_quoted_identifiers = true;

  std::vector<Macro> macros = Macro::AllMacros();
  macros.push_back(cel::OptMapMacro());
  macros.push_back(cel::OptFlatMapMacro());
  auto result = EnrichedParse(test_info.I, macros, "<input>", options);
  if (test_info.E.empty()) {
    EXPECT_THAT(result, IsOk());
  } else {
    EXPECT_THAT(result, Not(IsOk()));
    EXPECT_EQ(test_info.E, result.status().message());
  }

  if (!test_info.P.empty()) {
    KindAndIdAdorner kind_and_id_adorner;
    ExprPrinter w(kind_and_id_adorner);
    std::string adorned_string = w.PrintProto(result->parsed_expr().expr());
    EXPECT_EQ(test_info.P, adorned_string) << result->parsed_expr();
  }

  if (!test_info.L.empty()) {
    LocationAdorner location_adorner(result->parsed_expr().source_info());
    ExprPrinter w(location_adorner);
    std::string adorned_string = w.PrintProto(result->parsed_expr().expr());
    EXPECT_EQ(test_info.L, adorned_string) << result->parsed_expr();
    ;
  }

  if (!test_info.R.empty()) {
    EXPECT_EQ(test_info.R, ConvertEnrichedSourceInfoToString(
                               result->enriched_source_info()));
  }

  if (!test_info.M.empty()) {
    EXPECT_EQ(test_info.M, ConvertMacroCallsToString(
                               result.value().parsed_expr().source_info()))
        << result->parsed_expr();
    ;
  }
}

TEST(ExpressionTest, TsanOom) {
  Parse(
      "[[a([[???[a[[??[a([[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
      "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[???["
      "a([[????")
      .IgnoreError();
}

TEST(ExpressionTest, ErrorRecoveryLimits) {
  ParserOptions options;
  options.error_recovery_limit = 1;
  auto result = Parse("......", "", options);
  EXPECT_THAT(result, Not(IsOk()));
  EXPECT_EQ(result.status().message(),
            "ERROR: :1:1: Syntax error: More than 1 parse errors.\n | ......\n "
            "| ^\nERROR: :1:2: Syntax error: no viable alternative at input "
            "'..'\n | ......\n | .^");
}

TEST(ExpressionTest, ExpressionSizeLimit) {
  ParserOptions options;
  options.expression_size_codepoint_limit = 10;
  auto result = Parse("...............", "", options);
  EXPECT_THAT(result, Not(IsOk()));
  EXPECT_EQ(
      result.status().message(),
      "expression size exceeds codepoint limit. input size: 15, limit: 10");
}

TEST(ExpressionTest, RecursionDepthLongArgList) {
  ParserOptions options;
  // The particular number here is an implementation detail: the underlying
  // visitor will recurse up to 8 times before branching to the create list or
  // const steps. The call graph looks something like:
  // visit->visitStart->visit->visitExpr->visit->visitOr->visit->visitAnd->visit
  // ->visitRelation->visit->visitCalc->visit->visitUnary->visit->visitPrimary
  // ->visitCreateList->visit[arg]->visitExpr...
  // The expected max depth for create list with an arbitrary number of elements
  // is 15.
  options.max_recursion_depth = 16;

  EXPECT_THAT(Parse("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]", "", options), IsOk());
}

TEST(ExpressionTest, RecursionDepthExceeded) {
  ParserOptions options;
  // AST visitor will recurse a variable amount depending on the terms used in
  // the expression. This check occurs in the business logic converting the raw
  // Antlr parse tree into an Expr. There is a separate check (via a custom
  // listener) for AST depth while running the antlr generated parser.
  options.max_recursion_depth = 6;
  auto result = Parse("1 + 2 + 3 + 4 + 5 + 6 + 7", "", options);

  EXPECT_THAT(result, Not(IsOk()));
  EXPECT_THAT(result.status().message(),
              HasSubstr("Exceeded max recursion depth of 6 when parsing."));
}

TEST(ExpressionTest, DisableQuotedIdentifiers) {
  ParserOptions options;
  options.enable_quoted_identifiers = false;
  auto result = Parse("foo.`bar`", "", options);

  EXPECT_THAT(result, Not(IsOk()));
  EXPECT_THAT(result.status().message(),
              HasSubstr("ERROR: :1:5: unsupported syntax '`'\n"
                        " | foo.`bar`\n"
                        " | ....^"));
}

TEST(ExpressionTest, DisableStandardMacros) {
  ParserOptions options;
  options.disable_standard_macros = true;

  auto result = Parse("has(foo.bar)", "", options);

  ASSERT_THAT(result, IsOk());
  KindAndIdAdorner kind_and_id_adorner;
  ExprPrinter w(kind_and_id_adorner);
  std::string adorned_string = w.PrintProto(result->expr());
  EXPECT_EQ(adorned_string,
            "has(\n"
            "  foo^#2:Expr.Ident#.bar^#3:Expr.Select#\n"
            ")^#1:Expr.Call#")
      << adorned_string;
}

TEST(ExpressionTest, RecursionDepthIgnoresParentheses) {
  ParserOptions options;
  options.max_recursion_depth = 6;
  auto result = Parse("(((1 + 2 + 3 + 4 + (5 + 6))))", "", options);

  EXPECT_THAT(result, IsOk());
}

const std::vector<TestInfo>& UpdatedAccuVarTestCases() {
  static const std::vector<TestInfo>* kInstance = new std::vector<TestInfo>{
      {"[].exists(x, x > 0)",
       "__comprehension__(\n"
       "  // Variable\n"
       "  x,\n"
       "  // Target\n"
       "  []^#1:Expr.CreateList#,\n"
       "  // Accumulator\n"
       "  __result__,\n"
       "  // Init\n"
       "  false^#7:bool#,\n"
       "  // LoopCondition\n"
       "  @not_strictly_false(\n"
       "    !_(\n"
       "      __result__^#8:Expr.Ident#\n"
       "    )^#9:Expr.Call#\n"
       "  )^#10:Expr.Call#,\n"
       "  // LoopStep\n"
       "  _||_(\n"
       "    __result__^#11:Expr.Ident#,\n"
       "    _>_(\n"
       "      x^#4:Expr.Ident#,\n"
       "      0^#6:int64#\n"
       "    )^#5:Expr.Call#\n"
       "  )^#12:Expr.Call#,\n"
       "  // Result\n"
       "  __result__^#13:Expr.Ident#)^#14:Expr.Comprehension#"},
      {"[].exists_one(x, x > 0)",
       "__comprehension__(\n"
       "  // Variable\n"
       "  x,\n"
       "  // Target\n"
       "  []^#1:Expr.CreateList#,\n"
       "  // Accumulator\n"
       "  __result__,\n"
       "  // Init\n"
       "  0^#7:int64#,\n"
       "  // LoopCondition\n"
       "  true^#8:bool#,\n"
       "  // LoopStep\n"
       "  _?_:_(\n"
       "    _>_(\n"
       "      x^#4:Expr.Ident#,\n"
       "      0^#6:int64#\n"
       "    )^#5:Expr.Call#,\n"
       "    _+_(\n"
       "      __result__^#9:Expr.Ident#,\n"
       "      1^#10:int64#\n"
       "    )^#11:Expr.Call#,\n"
       "    __result__^#12:Expr.Ident#\n"
       "  )^#13:Expr.Call#,\n"
       "  // Result\n"
       "  _==_(\n"
       "    __result__^#14:Expr.Ident#,\n"
       "    1^#15:int64#\n"
       "  )^#16:Expr.Call#)^#17:Expr.Comprehension#"},
      {"[].all(x, x > 0)",
       "__comprehension__(\n"
       "  // Variable\n"
       "  x,\n"
       "  // Target\n"
       "  []^#1:Expr.CreateList#,\n"
       "  // Accumulator\n"
       "  __result__,\n"
       "  // Init\n"
       "  true^#7:bool#,\n"
       "  // LoopCondition\n"
       "  @not_strictly_false(\n"
       "    __result__^#8:Expr.Ident#\n"
       "  )^#9:Expr.Call#,\n"
       "  // LoopStep\n"
       "  _&&_(\n"
       "    __result__^#10:Expr.Ident#,\n"
       "    _>_(\n"
       "      x^#4:Expr.Ident#,\n"
       "      0^#6:int64#\n"
       "    )^#5:Expr.Call#\n"
       "  )^#11:Expr.Call#,\n"
       "  // Result\n"
       "  __result__^#12:Expr.Ident#)^#13:Expr.Comprehension#"},
      {"[].map(x, x + 1)",
       "__comprehension__(\n"
       "  // Variable\n"
       "  x,\n"
       "  // Target\n"
       "  []^#1:Expr.CreateList#,\n"
       "  // Accumulator\n"
       "  __result__,\n"
       "  // Init\n"
       "  []^#7:Expr.CreateList#,\n"
       "  // LoopCondition\n"
       "  true^#8:bool#,\n"
       "  // LoopStep\n"
       "  _+_(\n"
       "    __result__^#9:Expr.Ident#,\n"
       "    [\n"
       "      _+_(\n"
       "        x^#4:Expr.Ident#,\n"
       "        1^#6:int64#\n"
       "      )^#5:Expr.Call#\n"
       "    ]^#10:Expr.CreateList#\n"
       "  )^#11:Expr.Call#,\n"
       "  // Result\n"
       "  __result__^#12:Expr.Ident#)^#13:Expr.Comprehension#"},
      {"[].map(x, x > 0, x + 1)",
       "__comprehension__(\n"
       "  // Variable\n"
       "  x,\n"
       "  // Target\n"
       "  []^#1:Expr.CreateList#,\n"
       "  // Accumulator\n"
       "  __result__,\n"
       "  // Init\n"
       "  []^#10:Expr.CreateList#,\n"
       "  // LoopCondition\n"
       "  true^#11:bool#,\n"
       "  // LoopStep\n"
       "  _?_:_(\n"
       "    _>_(\n"
       "      x^#4:Expr.Ident#,\n"
       "      0^#6:int64#\n"
       "    )^#5:Expr.Call#,\n"
       "    _+_(\n"
       "      __result__^#12:Expr.Ident#,\n"
       "      [\n"
       "        _+_(\n"
       "          x^#7:Expr.Ident#,\n"
       "          1^#9:int64#\n"
       "        )^#8:Expr.Call#\n"
       "      ]^#13:Expr.CreateList#\n"
       "    )^#14:Expr.Call#,\n"
       "    __result__^#15:Expr.Ident#\n"
       "  )^#16:Expr.Call#,\n"
       "  // Result\n"
       "  __result__^#17:Expr.Ident#)^#18:Expr.Comprehension#"},
      {"[].filter(x, x > 0)",
       "__comprehension__(\n"
       "  // Variable\n"
       "  x,\n"
       "  // Target\n"
       "  []^#1:Expr.CreateList#,\n"
       "  // Accumulator\n"
       "  __result__,\n"
       "  // Init\n"
       "  []^#7:Expr.CreateList#,\n"
       "  // LoopCondition\n"
       "  true^#8:bool#,\n"
       "  // LoopStep\n"
       "  _?_:_(\n"
       "    _>_(\n"
       "      x^#4:Expr.Ident#,\n"
       "      0^#6:int64#\n"
       "    )^#5:Expr.Call#,\n"
       "    _+_(\n"
       "      __result__^#9:Expr.Ident#,\n"
       "      [\n"
       "        x^#3:Expr.Ident#\n"
       "      ]^#10:Expr.CreateList#\n"
       "    )^#11:Expr.Call#,\n"
       "    __result__^#12:Expr.Ident#\n"
       "  )^#13:Expr.Call#,\n"
       "  // Result\n"
       "  __result__^#14:Expr.Ident#)^#15:Expr.Comprehension#"},
      // Maintain restriction on '__result__' variable name until the default is
      // changed everywhere.
      {
          "[].map(__result__, true)",
          /*.P=*/"",
          /*.E=*/
          "ERROR: <input>:1:20: map() variable name cannot be __result__\n"
          " | [].map(__result__, true)\n"
          " | ...................^",
      },
      {
          "[].map(__result__, true, false)",
          /*.P=*/"",
          /*.E=*/
          "ERROR: <input>:1:20: map() variable name cannot be __result__\n"
          " | [].map(__result__, true, false)\n"
          " | ...................^",
      },
      {
          "[].filter(__result__, true)",
          /*.P=*/"",
          /*.E=*/
          "ERROR: <input>:1:23: filter() variable name cannot be __result__\n"
          " | [].filter(__result__, true)\n"
          " | ......................^",
      },
      {
          "[].exists(__result__, true)",
          /*.P=*/"",
          /*.E=*/
          "ERROR: <input>:1:23: exists() variable name cannot be __result__\n"
          " | [].exists(__result__, true)\n"
          " | ......................^",
      },
      {
          "[].all(__result__, true)",
          /*.P=*/"",
          /*.E=*/
          "ERROR: <input>:1:20: all() variable name cannot be __result__\n"
          " | [].all(__result__, true)\n"
          " | ...................^",
      },
      {
          "[].exists_one(__result__, true)",
          /*.P=*/"",
          /*.E=*/
          "ERROR: <input>:1:27: exists_one() variable name cannot be "
          "__result__\n"
          " | [].exists_one(__result__, true)\n"
          " | ..........................^",
      }};
  return *kInstance;
}

class UpdatedAccuVarDisabledTest : public testing::TestWithParam<TestInfo> {};

TEST_P(UpdatedAccuVarDisabledTest, Parse) {
  const TestInfo& test_info = GetParam();
  ParserOptions options;
  options.enable_hidden_accumulator_var = false;
  if (!test_info.M.empty()) {
    options.add_macro_calls = true;
  }

  auto result =
      EnrichedParse(test_info.I, Macro::AllMacros(), "<input>", options);
  if (test_info.E.empty()) {
    EXPECT_THAT(result, IsOk());
  } else {
    EXPECT_THAT(result, Not(IsOk()));
    EXPECT_EQ(test_info.E, result.status().message());
  }

  if (!test_info.P.empty()) {
    KindAndIdAdorner kind_and_id_adorner;
    ExprPrinter w(kind_and_id_adorner);
    std::string adorned_string = w.PrintProto(result->parsed_expr().expr());
    EXPECT_EQ(test_info.P, adorned_string) << result->parsed_expr();
  }

  if (!test_info.L.empty()) {
    LocationAdorner location_adorner(result->parsed_expr().source_info());
    ExprPrinter w(location_adorner);
    std::string adorned_string = w.PrintProto(result->parsed_expr().expr());
    EXPECT_EQ(test_info.L, adorned_string) << result->parsed_expr();
  }

  if (!test_info.R.empty()) {
    EXPECT_EQ(test_info.R, ConvertEnrichedSourceInfoToString(
                               result->enriched_source_info()));
  }

  if (!test_info.M.empty()) {
    EXPECT_EQ(test_info.M, ConvertMacroCallsToString(
                               result.value().parsed_expr().source_info()))
        << result->parsed_expr();
  }
}

TEST(NewParserBuilderTest, Defaults) {
  auto builder = cel::NewParserBuilder();
  ASSERT_OK_AND_ASSIGN(auto parser, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(auto source,
                       cel::NewSource("has(a.b) && [].exists(x, x > 0)"));
  ASSERT_OK_AND_ASSIGN(auto ast, parser->Parse(*source));

  EXPECT_FALSE(ast->IsChecked());
}

TEST(NewParserBuilderTest, CustomMacros) {
  auto builder = cel::NewParserBuilder();
  builder->GetOptions().disable_standard_macros = true;
  ASSERT_THAT(builder->AddMacro(cel::HasMacro()), IsOk());
  ASSERT_OK_AND_ASSIGN(auto parser, std::move(*builder).Build());
  builder.reset();

  ASSERT_OK_AND_ASSIGN(auto source, cel::NewSource("has(a.b) && [].map(x, x)"));
  ASSERT_OK_AND_ASSIGN(auto ast, parser->Parse(*source));

  EXPECT_FALSE(ast->IsChecked());
  KindAndIdAdorner kind_and_id_adorner;
  ExprPrinter w(kind_and_id_adorner);
  const auto& ast_impl = cel::ast_internal::AstImpl::CastFromPublicAst(*ast);
  EXPECT_EQ(w.Print(ast_impl.root_expr()),
            "_&&_(\n"
            "  a^#2:Expr.Ident#.b~test-only~^#4:Expr.Select#,\n"
            "  []^#5:Expr.CreateList#.map(\n"
            "    x^#7:Expr.Ident#,\n"
            "    x^#8:Expr.Ident#\n"
            "  )^#6:Expr.Call#\n"
            ")^#9:Expr.Call#");
}

TEST(NewParserBuilderTest, StandardMacrosNotAddedWithStdlib) {
  auto builder = cel::NewParserBuilder();
  builder->GetOptions().disable_standard_macros = false;
  // Add a fake stdlib to check that we don't try to add the standard macros
  // again. Emulates what happens when we add support for subsetting stdlib by
  // ids.
  ASSERT_THAT(builder->AddLibrary({"stdlib",
                                   [](cel::ParserBuilder& b) {
                                     return b.AddMacro(cel::HasMacro());
                                   }}),
              IsOk());
  ASSERT_OK_AND_ASSIGN(auto parser, std::move(*builder).Build());
  builder.reset();

  ASSERT_OK_AND_ASSIGN(auto source, cel::NewSource("has(a.b) && [].map(x, x)"));
  ASSERT_OK_AND_ASSIGN(auto ast, parser->Parse(*source));

  EXPECT_FALSE(ast->IsChecked());
  KindAndIdAdorner kind_and_id_adorner;
  ExprPrinter w(kind_and_id_adorner);
  const auto& ast_impl = cel::ast_internal::AstImpl::CastFromPublicAst(*ast);
  EXPECT_EQ(w.Print(ast_impl.root_expr()),
            "_&&_(\n"
            "  a^#2:Expr.Ident#.b~test-only~^#4:Expr.Select#,\n"
            "  []^#5:Expr.CreateList#.map(\n"
            "    x^#7:Expr.Ident#,\n"
            "    x^#8:Expr.Ident#\n"
            "  )^#6:Expr.Call#\n"
            ")^#9:Expr.Call#");
}

TEST(NewParserBuilderTest, ForwardsOptions) {
  auto builder = cel::NewParserBuilder();
  builder->GetOptions().enable_optional_syntax = true;
  ASSERT_OK_AND_ASSIGN(auto parser, std::move(*builder).Build());
  ASSERT_OK_AND_ASSIGN(auto source, cel::NewSource("a.?b"));
  ASSERT_OK_AND_ASSIGN(auto ast, parser->Parse(*source));
  EXPECT_FALSE(ast->IsChecked());

  builder = cel::NewParserBuilder();
  builder->GetOptions().enable_optional_syntax = false;
  ASSERT_OK_AND_ASSIGN(parser, std::move(*builder).Build());
  ASSERT_OK_AND_ASSIGN(source, cel::NewSource("a.?b"));
  EXPECT_THAT(parser->Parse(*source),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

std::string TestName(const testing::TestParamInfo<TestInfo>& test_info) {
  std::string name = absl::StrCat(test_info.index, "-", test_info.param.I);
  absl::c_replace_if(name, [](char c) { return !absl::ascii_isalnum(c); }, '_');
  return name;
  return name;
}

INSTANTIATE_TEST_SUITE_P(CelParserTest, ExpressionTest,
                         testing::ValuesIn(test_cases), TestName);

INSTANTIATE_TEST_SUITE_P(UpdatedAccuVarTest, UpdatedAccuVarDisabledTest,
                         testing::ValuesIn(UpdatedAccuVarTestCases()),
                         TestName);

}  // namespace
}  // namespace google::api::expr::parser
