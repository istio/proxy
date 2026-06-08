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

#include "testutil/baseline_tests.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "common/ast.h"
#include "common/expr.h"
#include "extensions/protobuf/ast_converters.h"
#include "testutil/expr_printer.h"

namespace cel::test {
namespace {

std::string FormatPrimitive(PrimitiveType t) {
  switch (t) {
    case PrimitiveType::kBool:
      return "bool";
    case PrimitiveType::kInt64:
      return "int";
    case PrimitiveType::kUint64:
      return "uint";
    case PrimitiveType::kDouble:
      return "double";
    case PrimitiveType::kString:
      return "string";
    case PrimitiveType::kBytes:
      return "bytes";
    default:
      return "<unspecified primitive>";
  }
}

std::string FormatType(const TypeSpec& t) {
  if (t.has_dyn()) {
    return "dyn";
  } else if (t.has_null()) {
    return "null";
  } else if (t.has_primitive()) {
    return FormatPrimitive(t.primitive());
  } else if (t.has_wrapper()) {
    return absl::StrCat("wrapper(", FormatPrimitive(t.wrapper()), ")");
  } else if (t.has_well_known()) {
    switch (t.well_known()) {
      case WellKnownTypeSpec::kAny:
        return "google.protobuf.Any";
      case WellKnownTypeSpec::kDuration:
        return "google.protobuf.Duration";
      case WellKnownTypeSpec::kTimestamp:
        return "google.protobuf.Timestamp";
      default:
        return "<unspecified wellknown>";
    }
  } else if (t.has_abstract_type()) {
    const auto& abs_type = t.abstract_type();
    std::string s = abs_type.name();
    if (!abs_type.parameter_types().empty()) {
      absl::StrAppend(&s, "(",
                      absl::StrJoin(abs_type.parameter_types(), ",",
                                    [](std::string* out, const auto& t) {
                                      absl::StrAppend(out, FormatType(t));
                                    }),
                      ")");
    }
    return s;
  } else if (t.has_type()) {
    if (t.type() == TypeSpec()) {
      return "type";
    }
    return absl::StrCat("type(", FormatType(t.type()), ")");
  } else if (t.has_message_type()) {
    return t.message_type().type();
  } else if (t.has_type_param()) {
    return t.type_param().type();
  } else if (t.has_list_type()) {
    return absl::StrCat("list(", FormatType(t.list_type().elem_type()), ")");
  } else if (t.has_map_type()) {
    return absl::StrCat("map(", FormatType(t.map_type().key_type()), ", ",
                        FormatType(t.map_type().value_type()), ")");
  }
  return "<error>";
}

std::string FormatReference(const cel::Reference& r) {
  if (r.overload_id().empty()) {
    return r.name();
  }
  return absl::StrJoin(r.overload_id(), "|");
}

class TypeAdorner : public ExpressionAdorner {
 public:
  explicit TypeAdorner(const Ast& ast) : ast_(ast) {}

  std::string Adorn(const Expr& e) const override {
    std::string s;

    auto t = ast_.type_map().find(e.id());
    if (t != ast_.type_map().end()) {
      absl::StrAppend(&s, "~", FormatType(t->second));
    }
    if (const auto r = ast_.reference_map().find(e.id());
        r != ast_.reference_map().end()) {
      absl::StrAppend(&s, "^", FormatReference(r->second));
    }
    return s;
  }

  std::string AdornStructField(const StructExprField& e) const override {
    return "";
  }

  std::string AdornMapEntry(const MapExprEntry& e) const override { return ""; }

 private:
  const Ast& ast_;
};

}  // namespace

std::string FormatBaselineAst(const Ast& ast) {
  TypeAdorner adorner(ast);
  ExprPrinter printer(adorner);
  return printer.Print(ast.root_expr());
}

std::string FormatBaselineCheckedExpr(
    const cel::expr::CheckedExpr& checked) {
  auto ast = cel::extensions::CreateAstFromCheckedExpr(checked);
  if (!ast.ok()) {
    return ast.status().ToString();
  }
  return FormatBaselineAst(**ast);
}

}  // namespace cel::test
