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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_EXPR_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_EXPR_FACTORY_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/constant.h"
#include "common/expr.h"

namespace cel {

class MacroExprFactory;
class ParserMacroExprFactory;

class ExprFactory {
 protected:
  // `IsExprLike` determines whether `T` is some `Expr`. Currently that means
  // either `Expr` or `std::unique_ptr<Expr>`. This allows us to make the
  // factory functions generic and avoid redefining them for every argument
  // combination.
  template <typename T>
  struct IsExprLike
      : std::bool_constant<std::disjunction_v<
            std::is_same<T, Expr>, std::is_same<T, std::unique_ptr<Expr>>>> {};

  // `IsStringLike` determines whether `T` is something that looks like a
  // string. Currently that means `const char*`, `std::string`, or
  // `absl::string_view`. This allows us to make the factory functions generic
  // and avoid redefining them for every argument combination. This is necessary
  // to avoid copies if possible.
  template <typename T>
  struct IsStringLike
      : std::bool_constant<std::disjunction_v<
            std::is_same<T, char*>, std::is_same<T, const char*>,
            std::is_same<T, std::string>, std::is_same<T, absl::string_view>>> {
  };

  template <size_t N>
  struct IsStringLike<const char[N]> : std::true_type {};

  // `IsArrayLike` determines whether `T` is something that looks like an array
  // or span of some element.
  template <typename Element, typename Array>
  struct IsArrayLike : std::false_type {};

  template <typename Element>
  struct IsArrayLike<Element, absl::Span<Element>> : std::true_type {};

  template <typename Element>
  struct IsArrayLike<Element, std::vector<Element>> : std::true_type {};

 public:
  ExprFactory(const ExprFactory&) = delete;
  ExprFactory(ExprFactory&&) = delete;
  ExprFactory& operator=(const ExprFactory&) = delete;
  ExprFactory& operator=(ExprFactory&&) = delete;

  virtual ~ExprFactory() = default;

  Expr NewUnspecified(ExprId id) {
    Expr expr;
    expr.set_id(id);
    return expr;
  }

  Expr NewConst(ExprId id, Constant value) {
    Expr expr;
    expr.set_id(id);
    expr.mutable_const_expr() = std::move(value);
    return expr;
  }

  Expr NewNullConst(ExprId id) {
    Constant constant;
    constant.set_null_value();
    return NewConst(id, std::move(constant));
  }

  Expr NewBoolConst(ExprId id, bool value) {
    Constant constant;
    constant.set_bool_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewIntConst(ExprId id, int64_t value) {
    Constant constant;
    constant.set_int_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewUintConst(ExprId id, uint64_t value) {
    Constant constant;
    constant.set_uint_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewDoubleConst(ExprId id, double value) {
    Constant constant;
    constant.set_double_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewBytesConst(ExprId id, BytesConstant value) {
    Constant constant;
    constant.set_bytes_value(std::move(value));
    return NewConst(id, std::move(constant));
  }

  Expr NewBytesConst(ExprId id, std::string value) {
    Constant constant;
    constant.set_bytes_value(std::move(value));
    return NewConst(id, std::move(constant));
  }

  Expr NewBytesConst(ExprId id, absl::string_view value) {
    Constant constant;
    constant.set_bytes_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewBytesConst(ExprId id, const char* value) {
    Constant constant;
    constant.set_bytes_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewStringConst(ExprId id, StringConstant value) {
    Constant constant;
    constant.set_string_value(std::move(value));
    return NewConst(id, std::move(constant));
  }

  Expr NewStringConst(ExprId id, std::string value) {
    Constant constant;
    constant.set_string_value(std::move(value));
    return NewConst(id, std::move(constant));
  }

  Expr NewStringConst(ExprId id, absl::string_view value) {
    Constant constant;
    constant.set_string_value(value);
    return NewConst(id, std::move(constant));
  }

  Expr NewStringConst(ExprId id, const char* value) {
    Constant constant;
    constant.set_string_value(value);
    return NewConst(id, std::move(constant));
  }

  template <typename Name,
            typename = std::enable_if_t<IsStringLike<Name>::value>>
  Expr NewIdent(ExprId id, Name name) {
    Expr expr;
    expr.set_id(id);
    auto& ident_expr = expr.mutable_ident_expr();
    ident_expr.set_name(std::move(name));
    return expr;
  }

  Expr NewAccuIdent(ExprId id) {
    return NewIdent(id, kAccumulatorVariableName);
  }

  template <typename Operand, typename Field,
            typename = std::enable_if_t<IsExprLike<Operand>::value>,
            typename = std::enable_if_t<IsStringLike<Field>::value>>
  Expr NewSelect(ExprId id, Operand operand, Field field) {
    Expr expr;
    expr.set_id(id);
    auto& select_expr = expr.mutable_select_expr();
    select_expr.set_operand(std::move(operand));
    select_expr.set_field(std::move(field));
    select_expr.set_test_only(false);
    return expr;
  }

  template <typename Operand, typename Field,
            typename = std::enable_if_t<IsExprLike<Operand>::value>,
            typename = std::enable_if_t<IsStringLike<Field>::value>>
  Expr NewPresenceTest(ExprId id, Operand operand, Field field) {
    Expr expr;
    expr.set_id(id);
    auto& select_expr = expr.mutable_select_expr();
    select_expr.set_operand(std::move(operand));
    select_expr.set_field(std::move(field));
    select_expr.set_test_only(true);
    return expr;
  }

  template <typename Function, typename Args,
            typename = std::enable_if_t<IsStringLike<Function>::value>,
            typename = std::enable_if_t<IsArrayLike<Expr, Args>::value>>
  Expr NewCall(ExprId id, Function function, Args args) {
    Expr expr;
    expr.set_id(id);
    auto& call_expr = expr.mutable_call_expr();
    call_expr.set_function(std::move(function));
    call_expr.set_args(std::move(args));
    return expr;
  }

  template <typename Function, typename Target, typename Args,
            typename = std::enable_if_t<IsStringLike<Function>::value>,
            typename = std::enable_if_t<IsExprLike<Target>::value>,
            typename = std::enable_if_t<IsArrayLike<Expr, Args>::value>>
  Expr NewMemberCall(ExprId id, Function function, Target target, Args args) {
    Expr expr;
    expr.set_id(id);
    auto& call_expr = expr.mutable_call_expr();
    call_expr.set_function(std::move(function));
    call_expr.set_target(std::move(target));
    call_expr.set_args(std::move(args));
    return expr;
  }

  template <typename Expr, typename = std::enable_if_t<IsExprLike<Expr>::value>>
  ListExprElement NewListElement(Expr expr, bool optional = false) {
    ListExprElement element;
    element.set_expr(std::move(expr));
    element.set_optional(optional);
    return element;
  }

  template <typename Elements,
            typename =
                std::enable_if_t<IsArrayLike<ListExprElement, Elements>::value>>
  Expr NewList(ExprId id, Elements elements) {
    Expr expr;
    expr.set_id(id);
    auto& list_expr = expr.mutable_list_expr();
    list_expr.set_elements(std::move(elements));
    return expr;
  }

  template <typename Name, typename Value,
            typename = std::enable_if_t<IsStringLike<Name>::value>,
            typename = std::enable_if_t<IsExprLike<Value>::value>>
  StructExprField NewStructField(ExprId id, Name name, Value value,
                                 bool optional = false) {
    StructExprField field;
    field.set_id(id);
    field.set_name(std::move(name));
    field.set_value(std::move(value));
    field.set_optional(optional);
    return field;
  }

  template <
      typename Name, typename Fields,
      typename = std::enable_if_t<IsStringLike<Name>::value>,
      typename = std::enable_if_t<IsArrayLike<StructExprField, Fields>::value>>
  Expr NewStruct(ExprId id, Name name, Fields fields) {
    Expr expr;
    expr.set_id(id);
    auto& struct_expr = expr.mutable_struct_expr();
    struct_expr.set_name(std::move(name));
    struct_expr.set_fields(std::move(fields));
    return expr;
  }

  template <typename Key, typename Value,
            typename = std::enable_if_t<IsExprLike<Key>::value>,
            typename = std::enable_if_t<IsExprLike<Value>::value>>
  MapExprEntry NewMapEntry(ExprId id, Key key, Value value,
                           bool optional = false) {
    MapExprEntry entry;
    entry.set_id(id);
    entry.set_key(std::move(key));
    entry.set_value(std::move(value));
    entry.set_optional(optional);
    return entry;
  }

  template <typename Entries, typename = std::enable_if_t<
                                  IsArrayLike<MapExprEntry, Entries>::value>>
  Expr NewMap(ExprId id, Entries entries) {
    Expr expr;
    expr.set_id(id);
    auto& map_expr = expr.mutable_map_expr();
    map_expr.set_entries(std::move(entries));
    return expr;
  }

  template <typename IterVar, typename IterRange, typename AccuVar,
            typename AccuInit, typename LoopCondition, typename LoopStep,
            typename Result,
            typename = std::enable_if_t<IsStringLike<IterVar>::value>,
            typename = std::enable_if_t<IsExprLike<IterRange>::value>,
            typename = std::enable_if_t<IsStringLike<AccuVar>::value>,
            typename = std::enable_if_t<IsExprLike<AccuInit>::value>,
            typename = std::enable_if_t<IsExprLike<LoopStep>::value>,
            typename = std::enable_if_t<IsExprLike<LoopCondition>::value>,
            typename = std::enable_if_t<IsExprLike<Result>::value>>
  Expr NewComprehension(ExprId id, IterVar iter_var, IterRange iter_range,
                        AccuVar accu_var, AccuInit accu_init,
                        LoopCondition loop_condition, LoopStep loop_step,
                        Result result) {
    Expr expr;
    expr.set_id(id);
    auto& comprehension_expr = expr.mutable_comprehension_expr();
    comprehension_expr.set_iter_var(std::move(iter_var));
    comprehension_expr.set_iter_range(std::move(iter_range));
    comprehension_expr.set_accu_var(std::move(accu_var));
    comprehension_expr.set_accu_init(std::move(accu_init));
    comprehension_expr.set_loop_condition(std::move(loop_condition));
    comprehension_expr.set_loop_step(std::move(loop_step));
    comprehension_expr.set_result(std::move(result));
    return expr;
  }

 private:
  friend class MacroExprFactory;
  friend class ParserMacroExprFactory;

  ExprFactory() = default;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_EXPR_FACTORY_H_
