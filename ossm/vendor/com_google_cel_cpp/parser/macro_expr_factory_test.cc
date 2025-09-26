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

#include "parser/macro_expr_factory.h"

#include <cstdint>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/expr.h"
#include "common/expr_factory.h"
#include "internal/testing.h"

namespace cel {

class TestMacroExprFactory final : public MacroExprFactory {
 public:
  TestMacroExprFactory() : MacroExprFactory(kAccumulatorVariableName) {}

  ExprId id() const { return id_; }

  Expr ReportError(absl::string_view) override {
    return NewUnspecified(NextId());
  }

  Expr ReportErrorAt(const Expr&, absl::string_view) override {
    return NewUnspecified(NextId());
  }

  using MacroExprFactory::NewBoolConst;
  using MacroExprFactory::NewCall;
  using MacroExprFactory::NewComprehension;
  using MacroExprFactory::NewIdent;
  using MacroExprFactory::NewList;
  using MacroExprFactory::NewListElement;
  using MacroExprFactory::NewMap;
  using MacroExprFactory::NewMapEntry;
  using MacroExprFactory::NewMemberCall;
  using MacroExprFactory::NewSelect;
  using MacroExprFactory::NewStruct;
  using MacroExprFactory::NewStructField;
  using MacroExprFactory::NewUnspecified;

 protected:
  ExprId NextId() override { return id_++; }

  ExprId CopyId(ExprId id) override {
    if (id == 0) {
      return 0;
    }
    return NextId();
  }

 private:
  int64_t id_ = 1;
};

namespace {

TEST(MacroExprFactory, CopyUnspecified) {
  TestMacroExprFactory factory;
  EXPECT_EQ(factory.Copy(factory.NewUnspecified()), factory.NewUnspecified(2));
}

TEST(MacroExprFactory, CopyIdent) {
  TestMacroExprFactory factory;
  EXPECT_EQ(factory.Copy(factory.NewIdent("foo")), factory.NewIdent(2, "foo"));
}

TEST(MacroExprFactory, CopyConst) {
  TestMacroExprFactory factory;
  EXPECT_EQ(factory.Copy(factory.NewBoolConst(true)),
            factory.NewBoolConst(2, true));
}

TEST(MacroExprFactory, CopySelect) {
  TestMacroExprFactory factory;
  EXPECT_EQ(factory.Copy(factory.NewSelect(factory.NewIdent("foo"), "bar")),
            factory.NewSelect(3, factory.NewIdent(4, "foo"), "bar"));
}

TEST(MacroExprFactory, CopyCall) {
  TestMacroExprFactory factory;
  std::vector<Expr> copied_args;
  copied_args.reserve(1);
  copied_args.push_back(factory.NewIdent(6, "baz"));
  EXPECT_EQ(factory.Copy(factory.NewMemberCall("bar", factory.NewIdent("foo"),
                                               factory.NewIdent("baz"))),
            factory.NewMemberCall(4, "bar", factory.NewIdent(5, "foo"),
                                  absl::MakeSpan(copied_args)));
}

TEST(MacroExprFactory, CopyList) {
  TestMacroExprFactory factory;
  std::vector<ListExprElement> copied_elements;
  copied_elements.reserve(1);
  copied_elements.push_back(factory.NewListElement(factory.NewIdent(4, "foo")));
  EXPECT_EQ(factory.Copy(factory.NewList(
                factory.NewListElement(factory.NewIdent("foo")))),
            factory.NewList(3, absl::MakeSpan(copied_elements)));
}

TEST(MacroExprFactory, CopyStruct) {
  TestMacroExprFactory factory;
  std::vector<StructExprField> copied_fields;
  copied_fields.reserve(1);
  copied_fields.push_back(
      factory.NewStructField(5, "bar", factory.NewIdent(6, "baz")));
  EXPECT_EQ(factory.Copy(factory.NewStruct(
                "foo", factory.NewStructField("bar", factory.NewIdent("baz")))),
            factory.NewStruct(4, "foo", absl::MakeSpan(copied_fields)));
}

TEST(MacroExprFactory, CopyMap) {
  TestMacroExprFactory factory;
  std::vector<MapExprEntry> copied_entries;
  copied_entries.reserve(1);
  copied_entries.push_back(factory.NewMapEntry(6, factory.NewIdent(7, "bar"),
                                               factory.NewIdent(8, "baz")));
  EXPECT_EQ(factory.Copy(factory.NewMap(factory.NewMapEntry(
                factory.NewIdent("bar"), factory.NewIdent("baz")))),
            factory.NewMap(5, absl::MakeSpan(copied_entries)));
}

TEST(MacroExprFactory, CopyComprehension) {
  TestMacroExprFactory factory;
  EXPECT_EQ(
      factory.Copy(factory.NewComprehension(
          "foo", factory.NewList(), "bar", factory.NewBoolConst(true),
          factory.NewIdent("baz"), factory.NewIdent("foo"),
          factory.NewIdent("bar"))),
      factory.NewComprehension(
          7, "foo", factory.NewList(8, std::vector<ListExprElement>()), "bar",
          factory.NewBoolConst(9, true), factory.NewIdent(10, "baz"),
          factory.NewIdent(11, "foo"), factory.NewIdent(12, "bar")));
}

}  // namespace
}  // namespace cel
