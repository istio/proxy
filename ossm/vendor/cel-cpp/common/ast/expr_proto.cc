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

#include "common/ast/expr_proto.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stack>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/variant.h"
#include "common/ast/constant_proto.h"
#include "common/constant.h"
#include "common/expr.h"
#include "internal/status_macros.h"

namespace cel::ast_internal {

namespace {

using ExprProto = cel::expr::Expr;
using ConstantProto = cel::expr::Constant;
using StructExprProto = cel::expr::Expr::CreateStruct;

class ExprToProtoState final {
 private:
  struct Frame final {
    const Expr* absl_nonnull expr;
    cel::expr::Expr* absl_nonnull proto;
  };

 public:
  absl::Status ExprToProto(const Expr& expr,
                           cel::expr::Expr* absl_nonnull proto) {
    Push(expr, proto);
    Frame frame;
    while (Pop(frame)) {
      CEL_RETURN_IF_ERROR(ExprToProtoImpl(*frame.expr, frame.proto));
    }
    return absl::OkStatus();
  }

 private:
  absl::Status ExprToProtoImpl(const Expr& expr,
                               cel::expr::Expr* absl_nonnull proto) {
    return absl::visit(
        absl::Overload(
            [&expr, proto](const UnspecifiedExpr&) -> absl::Status {
              proto->Clear();
              proto->set_id(expr.id());
              return absl::OkStatus();
            },
            [this, &expr, proto](const Constant& const_expr) -> absl::Status {
              return ConstExprToProto(expr, const_expr, proto);
            },
            [this, &expr, proto](const IdentExpr& ident_expr) -> absl::Status {
              return IdentExprToProto(expr, ident_expr, proto);
            },
            [this, &expr,
             proto](const SelectExpr& select_expr) -> absl::Status {
              return SelectExprToProto(expr, select_expr, proto);
            },
            [this, &expr, proto](const CallExpr& call_expr) -> absl::Status {
              return CallExprToProto(expr, call_expr, proto);
            },
            [this, &expr, proto](const ListExpr& list_expr) -> absl::Status {
              return ListExprToProto(expr, list_expr, proto);
            },
            [this, &expr,
             proto](const StructExpr& struct_expr) -> absl::Status {
              return StructExprToProto(expr, struct_expr, proto);
            },
            [this, &expr, proto](const MapExpr& map_expr) -> absl::Status {
              return MapExprToProto(expr, map_expr, proto);
            },
            [this, &expr, proto](
                const ComprehensionExpr& comprehension_expr) -> absl::Status {
              return ComprehensionExprToProto(expr, comprehension_expr, proto);
            }),
        expr.kind());
  }

  absl::Status ConstExprToProto(const Expr& expr, const Constant& const_expr,
                                ExprProto* absl_nonnull proto) {
    proto->Clear();
    proto->set_id(expr.id());
    return ConstantToProto(const_expr, proto->mutable_const_expr());
  }

  absl::Status IdentExprToProto(const Expr& expr, const IdentExpr& ident_expr,
                                ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* ident_proto = proto->mutable_ident_expr();
    proto->set_id(expr.id());
    ident_proto->set_name(ident_expr.name());
    return absl::OkStatus();
  }

  absl::Status SelectExprToProto(const Expr& expr,
                                 const SelectExpr& select_expr,
                                 ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* select_proto = proto->mutable_select_expr();
    proto->set_id(expr.id());
    if (select_expr.has_operand()) {
      Push(select_expr.operand(), select_proto->mutable_operand());
    }
    select_proto->set_field(select_expr.field());
    select_proto->set_test_only(select_expr.test_only());
    return absl::OkStatus();
  }

  absl::Status CallExprToProto(const Expr& expr, const CallExpr& call_expr,
                               ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* call_proto = proto->mutable_call_expr();
    proto->set_id(expr.id());
    if (call_expr.has_target()) {
      Push(call_expr.target(), call_proto->mutable_target());
    }
    call_proto->set_function(call_expr.function());
    if (!call_expr.args().empty()) {
      call_proto->mutable_args()->Reserve(
          static_cast<int>(call_expr.args().size()));
      for (const auto& argument : call_expr.args()) {
        Push(argument, call_proto->add_args());
      }
    }
    return absl::OkStatus();
  }

  absl::Status ListExprToProto(const Expr& expr, const ListExpr& list_expr,
                               ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* list_proto = proto->mutable_list_expr();
    proto->set_id(expr.id());
    if (!list_expr.elements().empty()) {
      list_proto->mutable_elements()->Reserve(
          static_cast<int>(list_expr.elements().size()));
      for (size_t i = 0; i < list_expr.elements().size(); ++i) {
        const auto& element_expr = list_expr.elements()[i];
        auto* element_proto = list_proto->add_elements();
        if (element_expr.has_expr()) {
          Push(element_expr.expr(), element_proto);
        }
        if (element_expr.optional()) {
          list_proto->add_optional_indices(static_cast<int32_t>(i));
        }
      }
    }
    return absl::OkStatus();
  }

  absl::Status StructExprToProto(const Expr& expr,
                                 const StructExpr& struct_expr,
                                 ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* struct_proto = proto->mutable_struct_expr();
    proto->set_id(expr.id());
    struct_proto->set_message_name(struct_expr.name());
    if (!struct_expr.fields().empty()) {
      struct_proto->mutable_entries()->Reserve(
          static_cast<int>(struct_expr.fields().size()));
      for (const auto& field_expr : struct_expr.fields()) {
        auto* field_proto = struct_proto->add_entries();
        field_proto->set_id(field_expr.id());
        field_proto->set_field_key(field_expr.name());
        if (field_expr.has_value()) {
          Push(field_expr.value(), field_proto->mutable_value());
        }
        if (field_expr.optional()) {
          field_proto->set_optional_entry(true);
        }
      }
    }
    return absl::OkStatus();
  }

  absl::Status MapExprToProto(const Expr& expr, const MapExpr& map_expr,
                              ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* map_proto = proto->mutable_struct_expr();
    proto->set_id(expr.id());
    if (!map_expr.entries().empty()) {
      map_proto->mutable_entries()->Reserve(
          static_cast<int>(map_expr.entries().size()));
      for (const auto& entry_expr : map_expr.entries()) {
        auto* entry_proto = map_proto->add_entries();
        entry_proto->set_id(entry_expr.id());
        if (entry_expr.has_key()) {
          Push(entry_expr.key(), entry_proto->mutable_map_key());
        }
        if (entry_expr.has_value()) {
          Push(entry_expr.value(), entry_proto->mutable_value());
        }
        if (entry_expr.optional()) {
          entry_proto->set_optional_entry(true);
        }
      }
    }
    return absl::OkStatus();
  }

  absl::Status ComprehensionExprToProto(
      const Expr& expr, const ComprehensionExpr& comprehension_expr,
      ExprProto* absl_nonnull proto) {
    proto->Clear();
    auto* comprehension_proto = proto->mutable_comprehension_expr();
    proto->set_id(expr.id());
    comprehension_proto->set_iter_var(comprehension_expr.iter_var());
    comprehension_proto->set_iter_var2(comprehension_expr.iter_var2());
    if (comprehension_expr.has_iter_range()) {
      Push(comprehension_expr.iter_range(),
           comprehension_proto->mutable_iter_range());
    }
    comprehension_proto->set_accu_var(comprehension_expr.accu_var());
    if (comprehension_expr.has_accu_init()) {
      Push(comprehension_expr.accu_init(),
           comprehension_proto->mutable_accu_init());
    }
    if (comprehension_expr.has_loop_condition()) {
      Push(comprehension_expr.loop_condition(),
           comprehension_proto->mutable_loop_condition());
    }
    if (comprehension_expr.has_loop_step()) {
      Push(comprehension_expr.loop_step(),
           comprehension_proto->mutable_loop_step());
    }
    if (comprehension_expr.has_result()) {
      Push(comprehension_expr.result(), comprehension_proto->mutable_result());
    }
    return absl::OkStatus();
  }

  void Push(const Expr& expr, ExprProto* absl_nonnull proto) {
    frames_.push(Frame{&expr, proto});
  }

  bool Pop(Frame& frame) {
    if (frames_.empty()) {
      return false;
    }
    frame = frames_.top();
    frames_.pop();
    return true;
  }

  std::stack<Frame, std::vector<Frame>> frames_;
};

class ExprFromProtoState final {
 private:
  struct Frame final {
    const ExprProto* absl_nonnull proto;
    Expr* absl_nonnull expr;
  };

 public:
  absl::Status ExprFromProto(const ExprProto& proto, Expr& expr) {
    Push(proto, expr);
    Frame frame;
    while (Pop(frame)) {
      CEL_RETURN_IF_ERROR(ExprFromProtoImpl(*frame.proto, *frame.expr));
    }
    return absl::OkStatus();
  }

 private:
  absl::Status ExprFromProtoImpl(const ExprProto& proto, Expr& expr) {
    switch (proto.expr_kind_case()) {
      case ExprProto::EXPR_KIND_NOT_SET:
        expr.Clear();
        expr.set_id(proto.id());
        return absl::OkStatus();
      case ExprProto::kConstExpr:
        return ConstExprFromProto(proto, proto.const_expr(), expr);
      case ExprProto::kIdentExpr:
        return IdentExprFromProto(proto, proto.ident_expr(), expr);
      case ExprProto::kSelectExpr:
        return SelectExprFromProto(proto, proto.select_expr(), expr);
      case ExprProto::kCallExpr:
        return CallExprFromProto(proto, proto.call_expr(), expr);
      case ExprProto::kListExpr:
        return ListExprFromProto(proto, proto.list_expr(), expr);
      case ExprProto::kStructExpr:
        if (proto.struct_expr().message_name().empty()) {
          return MapExprFromProto(proto, proto.struct_expr(), expr);
        }
        return StructExprFromProto(proto, proto.struct_expr(), expr);
      case ExprProto::kComprehensionExpr:
        return ComprehensionExprFromProto(proto, proto.comprehension_expr(),
                                          expr);
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("unexpected ExprKindCase: ",
                         static_cast<int>(proto.expr_kind_case())));
    }
  }

  absl::Status ConstExprFromProto(const ExprProto& proto,
                                  const ConstantProto& const_proto,
                                  Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    return ConstantFromProto(const_proto, expr.mutable_const_expr());
  }

  absl::Status IdentExprFromProto(const ExprProto& proto,
                                  const ExprProto::Ident& ident_proto,
                                  Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& ident_expr = expr.mutable_ident_expr();
    ident_expr.set_name(ident_proto.name());
    return absl::OkStatus();
  }

  absl::Status SelectExprFromProto(const ExprProto& proto,
                                   const ExprProto::Select& select_proto,
                                   Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& select_expr = expr.mutable_select_expr();
    if (select_proto.has_operand()) {
      Push(select_proto.operand(), select_expr.mutable_operand());
    }
    select_expr.set_field(select_proto.field());
    select_expr.set_test_only(select_proto.test_only());
    return absl::OkStatus();
  }

  absl::Status CallExprFromProto(const ExprProto& proto,
                                 const ExprProto::Call& call_proto,
                                 Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& call_expr = expr.mutable_call_expr();
    call_expr.set_function(call_proto.function());
    if (call_proto.has_target()) {
      Push(call_proto.target(), call_expr.mutable_target());
    }
    call_expr.mutable_args().reserve(
        static_cast<size_t>(call_proto.args().size()));
    for (const auto& argument_proto : call_proto.args()) {
      Push(argument_proto, call_expr.add_args());
    }
    return absl::OkStatus();
  }

  absl::Status ListExprFromProto(const ExprProto& proto,
                                 const ExprProto::CreateList& list_proto,
                                 Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& list_expr = expr.mutable_list_expr();
    list_expr.mutable_elements().reserve(
        static_cast<size_t>(list_proto.elements().size()));
    for (int i = 0; i < list_proto.elements().size(); ++i) {
      const auto& element_proto = list_proto.elements()[i];
      auto& element_expr = list_expr.add_elements();
      Push(element_proto, element_expr.mutable_expr());
      const auto& optional_indicies_proto = list_proto.optional_indices();
      element_expr.set_optional(std::find(optional_indicies_proto.begin(),
                                          optional_indicies_proto.end(),
                                          i) != optional_indicies_proto.end());
    }
    return absl::OkStatus();
  }

  absl::Status StructExprFromProto(const ExprProto& proto,
                                   const StructExprProto& struct_proto,
                                   Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& struct_expr = expr.mutable_struct_expr();
    struct_expr.set_name(struct_proto.message_name());
    struct_expr.mutable_fields().reserve(
        static_cast<size_t>(struct_proto.entries().size()));
    for (const auto& field_proto : struct_proto.entries()) {
      switch (field_proto.key_kind_case()) {
        case StructExprProto::Entry::KEY_KIND_NOT_SET:
          ABSL_FALLTHROUGH_INTENDED;
        case StructExprProto::Entry::kFieldKey:
          break;
        case StructExprProto::Entry::kMapKey:
          return absl::InvalidArgumentError("encountered map entry in struct");
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "unexpected struct field kind: ", field_proto.key_kind_case()));
      }
      auto& field_expr = struct_expr.add_fields();
      field_expr.set_id(field_proto.id());
      field_expr.set_name(field_proto.field_key());
      if (field_proto.has_value()) {
        Push(field_proto.value(), field_expr.mutable_value());
      }
      field_expr.set_optional(field_proto.optional_entry());
    }
    return absl::OkStatus();
  }

  absl::Status MapExprFromProto(const ExprProto& proto,
                                const ExprProto::CreateStruct& map_proto,
                                Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& map_expr = expr.mutable_map_expr();
    map_expr.mutable_entries().reserve(
        static_cast<size_t>(map_proto.entries().size()));
    for (const auto& entry_proto : map_proto.entries()) {
      switch (entry_proto.key_kind_case()) {
        case StructExprProto::Entry::KEY_KIND_NOT_SET:
          ABSL_FALLTHROUGH_INTENDED;
        case StructExprProto::Entry::kMapKey:
          break;
        case StructExprProto::Entry::kFieldKey:
          return absl::InvalidArgumentError("encountered struct field in map");
        default:
          return absl::InvalidArgumentError(absl::StrCat(
              "unexpected map entry kind: ", entry_proto.key_kind_case()));
      }
      auto& entry_expr = map_expr.add_entries();
      entry_expr.set_id(entry_proto.id());
      if (entry_proto.has_map_key()) {
        Push(entry_proto.map_key(), entry_expr.mutable_key());
      }
      if (entry_proto.has_value()) {
        Push(entry_proto.value(), entry_expr.mutable_value());
      }
      entry_expr.set_optional(entry_proto.optional_entry());
    }
    return absl::OkStatus();
  }

  absl::Status ComprehensionExprFromProto(
      const ExprProto& proto,
      const ExprProto::Comprehension& comprehension_proto, Expr& expr) {
    expr.Clear();
    expr.set_id(proto.id());
    auto& comprehension_expr = expr.mutable_comprehension_expr();
    comprehension_expr.set_iter_var(comprehension_proto.iter_var());
    comprehension_expr.set_iter_var2(comprehension_proto.iter_var2());
    comprehension_expr.set_accu_var(comprehension_proto.accu_var());
    if (comprehension_proto.has_iter_range()) {
      Push(comprehension_proto.iter_range(),
           comprehension_expr.mutable_iter_range());
    }
    if (comprehension_proto.has_accu_init()) {
      Push(comprehension_proto.accu_init(),
           comprehension_expr.mutable_accu_init());
    }
    if (comprehension_proto.has_loop_condition()) {
      Push(comprehension_proto.loop_condition(),
           comprehension_expr.mutable_loop_condition());
    }
    if (comprehension_proto.has_loop_step()) {
      Push(comprehension_proto.loop_step(),
           comprehension_expr.mutable_loop_step());
    }
    if (comprehension_proto.has_result()) {
      Push(comprehension_proto.result(), comprehension_expr.mutable_result());
    }
    return absl::OkStatus();
  }

  void Push(const ExprProto& proto, Expr& expr) {
    frames_.push(Frame{&proto, &expr});
  }

  bool Pop(Frame& frame) {
    if (frames_.empty()) {
      return false;
    }
    frame = frames_.top();
    frames_.pop();
    return true;
  }

  std::stack<Frame, std::vector<Frame>> frames_;
};

}  // namespace

absl::Status ExprToProto(const Expr& expr,
                         cel::expr::Expr* absl_nonnull proto) {
  ExprToProtoState state;
  return state.ExprToProto(expr, proto);
}

absl::Status ExprFromProto(const cel::expr::Expr& proto, Expr& expr) {
  ExprFromProtoState state;
  return state.ExprFromProto(proto, expr);
}

}  // namespace cel::ast_internal
