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

#include "testutil/expr_printer.h"

#include <algorithm>
#include <memory>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_log.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "common/ast.h"
#include "common/ast_proto.h"
#include "common/constant.h"
#include "common/expr.h"
#include "internal/strings.h"

namespace cel::test {
namespace {

class EmptyAdornerImpl : public ExpressionAdorner {
 public:
  std::string Adorn(const Expr& e) const override { return ""; }

  std::string AdornStructField(const StructExprField& e) const override {
    return "";
  }

  std::string AdornMapEntry(const MapExprEntry& e) const override { return ""; }
};

class StringBuilder {
 public:
  explicit StringBuilder(const ExpressionAdorner& adorner)
      : adorner_(adorner), line_start_(true), indent_(0) {}

  std::string Print(const Expr& expr) {
    AppendExpr(expr);
    return s_;
  }

 private:
  void AppendExpr(const Expr& e) {
    switch (e.kind_case()) {
      case ExprKindCase::kConstant:
        Append(FormatLiteral(e.const_expr()));
        break;
      case ExprKindCase::kIdentExpr:
        Append(e.ident_expr().name());
        break;
      case ExprKindCase::kSelectExpr:
        AppendSelect(e.select_expr());
        break;
      case ExprKindCase::kCallExpr:
        AppendCall(e.call_expr());
        break;
      case ExprKindCase::kListExpr:
        AppendList(e.list_expr());
        break;
      case ExprKindCase::kMapExpr:
        AppendMap(e.map_expr());
        break;
      case ExprKindCase::kStructExpr:
        AppendStruct(e.struct_expr());
        break;
      case ExprKindCase::kComprehensionExpr:
        AppendComprehension(e.comprehension_expr());
        break;
      default:
        break;
    }
    Append(adorner_.Adorn(e));
  }

  void AppendSelect(const SelectExpr& sel) {
    AppendExpr(sel.operand());
    Append(".");
    Append(sel.field());
    if (sel.test_only()) {
      Append("~test-only~");
    }
  }

  void AppendCall(const CallExpr& call) {
    if (call.has_target()) {
      AppendExpr(call.target());
      s_ += ".";
    }

    Append(call.function());
    if (call.args().empty()) {
      Append("()");
      return;
    }

    Append("(");
    Indent();
    AppendLine();
    for (int i = 0; i < call.args().size(); ++i) {
      const auto& arg = call.args()[i];
      if (i > 0) {
        Append(",");
        AppendLine();
      }
      AppendExpr(arg);
    }
    AppendLine();
    Unindent();
    Append(")");
  }

  void AppendList(const ListExpr& list) {
    if (list.elements().empty()) {
      Append("[]");
      return;
    }
    Append("[");
    AppendLine();
    Indent();
    for (int i = 0; i < list.elements().size(); ++i) {
      const auto& elem = list.elements()[i];
      if (i > 0) {
        Append(",");
        AppendLine();
      }
      if (elem.optional()) {
        Append("?");
      }
      AppendExpr(elem.expr());
    }
    AppendLine();
    Unindent();
    Append("]");
  }

  void AppendStruct(const StructExpr& obj) {
    Append(obj.name());

    if (obj.fields().empty()) {
      Append("{}");
      return;
    }

    Append("{");
    AppendLine();
    Indent();
    for (int i = 0; i < obj.fields().size(); ++i) {
      const auto& entry = obj.fields()[i];
      if (i > 0) {
        Append(",");
        AppendLine();
      }
      if (entry.optional()) {
        Append("?");
      }
      Append(entry.name());
      Append(":");
      AppendExpr(entry.value());
      Append(adorner_.AdornStructField(entry));
    }
    AppendLine();
    Unindent();
    Append("}");
  }

  void AppendMap(const MapExpr& obj) {
    if (obj.entries().empty()) {
      Append("{}");
      return;
    }
    Append("{");
    AppendLine();
    Indent();
    for (int i = 0; i < obj.entries().size(); ++i) {
      const auto& entry = obj.entries()[i];
      if (i > 0) {
        Append(",");
        AppendLine();
      }
      if (entry.optional()) {
        Append("?");
      }
      AppendExpr(entry.key());
      Append(":");
      AppendExpr(entry.value());
      Append(adorner_.AdornMapEntry(entry));
    }
    AppendLine();
    Unindent();
    Append("}");
  }

  void AppendComprehension(const ComprehensionExpr& comprehension) {
    Append("__comprehension__(");
    Indent();
    AppendLine();
    Append("// Variable");
    AppendLine();
    Append(comprehension.iter_var());
    Append(",");
    AppendLine();
    Append("// Target");
    AppendLine();
    AppendExpr(comprehension.iter_range());
    Append(",");
    AppendLine();
    Append("// Accumulator");
    AppendLine();
    Append(comprehension.accu_var());
    Append(",");
    AppendLine();
    Append("// Init");
    AppendLine();
    AppendExpr(comprehension.accu_init());
    Append(",");
    AppendLine();
    Append("// LoopCondition");
    AppendLine();
    AppendExpr(comprehension.loop_condition());
    Append(",");
    AppendLine();
    Append("// LoopStep");
    AppendLine();
    AppendExpr(comprehension.loop_step());
    Append(",");
    AppendLine();
    Append("// Result");
    AppendLine();
    AppendExpr(comprehension.result());
    Append(")");
    Unindent();
  }

  void Append(const std::string& s) {
    if (line_start_) {
      line_start_ = false;
      for (int i = 0; i < indent_; ++i) {
        s_ += "  ";
      }
    }
    s_ += s;
  }

  void AppendLine() {
    s_ += "\n";
    line_start_ = true;
  }

  void Indent() { ++indent_; }
  void Unindent() {
    if (indent_ >= 0) {
      --indent_;
    } else {
      ABSL_LOG(ERROR) << "ExprPrinter indent underflow";
    }
  }

  std::string FormatLiteral(const Constant& c) {
    switch (c.kind_case()) {
      case ConstantKindCase::kBool:
        return absl::StrFormat("%s", c.bool_value() ? "true" : "false");
      case ConstantKindCase::kBytes:
        return cel::internal::FormatDoubleQuotedBytesLiteral(c.bytes_value());
      case ConstantKindCase::kDouble: {
        std::string s = absl::StrFormat("%f", c.double_value());
        // remove trailing zeros, i.e., convert 1.600000 to just 1.6 without
        // forcing a specific precision. There seems to be no flag to get this
        // directly from absl::StrFormat.
        auto idx = std::find_if_not(s.rbegin(), s.rend(),
                                    [](const char c) { return c == '0'; });
        s.erase(idx.base(), s.end());
        if (absl::EndsWith(s, ".")) {
          s += '0';
        }
        return s;
      }
      case ConstantKindCase::kInt:
        return absl::StrFormat("%d", c.int_value());
      case ConstantKindCase::kString:
        return cel::internal::FormatDoubleQuotedStringLiteral(c.string_value());
      case ConstantKindCase::kUint:
        return absl::StrFormat("%uu", c.uint_value());
      case ConstantKindCase::kNull:
        return "null";
      default:
        return "<<ERROR>>";
    }
  }

  std::string s_;
  const ExpressionAdorner& adorner_;
  bool line_start_;
  int indent_;
};

}  // namespace

const ExpressionAdorner& EmptyAdorner() {
  static absl::NoDestructor<EmptyAdornerImpl> kInstance;
  return *kInstance;
}

std::string ExprPrinter::PrintProto(const cel::expr::Expr& expr) const {
  StringBuilder w(adorner_);
  absl::StatusOr<std::unique_ptr<Ast>> ast = CreateAstFromParsedExpr(expr);
  if (!ast.ok()) {
    return std::string(ast.status().message());
  }
  return w.Print(ast.value()->root_expr());
}

std::string ExprPrinter::Print(const Expr& expr) const {
  StringBuilder w(adorner_);
  return w.Print(expr);
}

}  // namespace cel::test
