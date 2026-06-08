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

#ifndef THIRD_PARTY_CEL_CPP_PARSER_MACRO_EXPR_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_PARSER_MACRO_EXPR_FACTORY_H_

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "common/expr.h"
#include "common/expr_factory.h"

namespace cel {

class ParserMacroExprFactory;
class TestMacroExprFactory;

// `MacroExprFactory` is a specialization of `ExprFactory` for `MacroExpander`
// which disallows explicitly specifying IDs.
class MacroExprFactory : protected ExprFactory {
 protected:
  using ExprFactory::IsArrayLike;
  using ExprFactory::IsExprLike;
  using ExprFactory::IsStringLike;

  template <typename T, typename U>
  struct IsRValue
      : std::bool_constant<
            std::disjunction_v<std::is_same<T, U>, std::is_same<T, U&&>>> {};

 public:
  ABSL_MUST_USE_RESULT Expr Copy(const Expr& expr);

  ABSL_MUST_USE_RESULT ListExprElement Copy(const ListExprElement& element);

  ABSL_MUST_USE_RESULT StructExprField Copy(const StructExprField& field);

  ABSL_MUST_USE_RESULT MapExprEntry Copy(const MapExprEntry& entry);

  ABSL_MUST_USE_RESULT Expr NewUnspecified() {
    return NewUnspecified(NextId());
  }

  ABSL_MUST_USE_RESULT Expr NewNullConst() { return NewNullConst(NextId()); }

  ABSL_MUST_USE_RESULT Expr NewBoolConst(bool value) {
    return NewBoolConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewIntConst(int64_t value) {
    return NewIntConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewUintConst(uint64_t value) {
    return NewUintConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewDoubleConst(double value) {
    return NewDoubleConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewBytesConst(std::string value) {
    return NewBytesConst(NextId(), std::move(value));
  }

  ABSL_MUST_USE_RESULT Expr NewBytesConst(absl::string_view value) {
    return NewBytesConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewBytesConst(const char* absl_nullable value) {
    return NewBytesConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewStringConst(std::string value) {
    return NewStringConst(NextId(), std::move(value));
  }

  ABSL_MUST_USE_RESULT Expr NewStringConst(absl::string_view value) {
    return NewStringConst(NextId(), value);
  }

  ABSL_MUST_USE_RESULT Expr NewStringConst(const char* absl_nullable value) {
    return NewStringConst(NextId(), value);
  }

  template <typename Name,
            typename = std::enable_if_t<IsStringLike<Name>::value>>
  ABSL_MUST_USE_RESULT Expr NewIdent(Name name) {
    return NewIdent(NextId(), std::move(name));
  }

  absl::string_view AccuVarName() { return ExprFactory::AccuVarName(); }

  ABSL_MUST_USE_RESULT Expr NewAccuIdent() { return NewAccuIdent(NextId()); }

  template <typename Operand, typename Field,
            typename = std::enable_if_t<IsExprLike<Operand>::value>,
            typename = std::enable_if_t<IsStringLike<Field>::value>>
  ABSL_MUST_USE_RESULT Expr NewSelect(Operand operand, Field field) {
    return NewSelect(NextId(), std::move(operand), std::move(field));
  }

  template <typename Operand, typename Field,
            typename = std::enable_if_t<IsExprLike<Operand>::value>,
            typename = std::enable_if_t<IsStringLike<Field>::value>>
  ABSL_MUST_USE_RESULT Expr NewPresenceTest(Operand operand, Field field) {
    return NewPresenceTest(NextId(), std::move(operand), std::move(field));
  }

  template <
      typename Function, typename... Args,
      typename = std::enable_if_t<IsStringLike<Function>::value>,
      typename = std::enable_if_t<std::conjunction_v<IsRValue<Expr, Args>...>>>
  ABSL_MUST_USE_RESULT Expr NewCall(Function function, Args&&... args) {
    std::vector<Expr> array;
    array.reserve(sizeof...(Args));
    (array.push_back(std::forward<Args>(args)), ...);
    return NewCall(NextId(), std::move(function), std::move(array));
  }

  template <typename Function, typename Args,
            typename = std::enable_if_t<IsStringLike<Function>::value>,
            typename = std::enable_if_t<IsArrayLike<Expr, Args>::value>>
  ABSL_MUST_USE_RESULT Expr NewCall(Function function, Args args) {
    return NewCall(NextId(), std::move(function), std::move(args));
  }

  template <
      typename Function, typename Target, typename... Args,
      typename = std::enable_if_t<IsStringLike<Function>::value>,
      typename = std::enable_if_t<IsExprLike<Target>::value>,
      typename = std::enable_if_t<std::conjunction_v<IsRValue<Expr, Args>...>>>
  ABSL_MUST_USE_RESULT Expr NewMemberCall(Function function, Target target,
                                          Args&&... args) {
    std::vector<Expr> array;
    array.reserve(sizeof...(Args));
    (array.push_back(std::forward<Args>(args)), ...);
    return NewMemberCall(NextId(), std::move(function), std::move(target),
                         std::move(array));
  }

  template <typename Function, typename Target, typename Args,
            typename = std::enable_if_t<IsStringLike<Function>::value>,
            typename = std::enable_if_t<IsExprLike<Target>::value>,
            typename = std::enable_if_t<IsArrayLike<Expr, Args>::value>>
  ABSL_MUST_USE_RESULT Expr NewMemberCall(Function function, Target target,
                                          Args args) {
    return NewMemberCall(NextId(), std::move(function), std::move(target),
                         std::move(args));
  }

  using ExprFactory::NewListElement;

  template <typename... Elements,
            typename = std::enable_if_t<
                std::conjunction_v<IsRValue<ListExprElement, Elements>...>>>
  ABSL_MUST_USE_RESULT Expr NewList(Elements&&... elements) {
    std::vector<ListExprElement> array;
    array.reserve(sizeof...(Elements));
    (array.push_back(std::forward<Elements>(elements)), ...);
    return NewList(NextId(), std::move(array));
  }

  template <typename Elements,
            typename =
                std::enable_if_t<IsArrayLike<ListExprElement, Elements>::value>>
  ABSL_MUST_USE_RESULT Expr NewList(Elements elements) {
    return NewList(NextId(), std::move(elements));
  }

  template <typename Name, typename Value,
            typename = std::enable_if_t<IsStringLike<Name>::value>,
            typename = std::enable_if_t<IsExprLike<Value>::value>>
  ABSL_MUST_USE_RESULT StructExprField NewStructField(Name name, Value value,
                                                      bool optional = false) {
    return NewStructField(NextId(), std::move(name), std::move(value),
                          optional);
  }

  template <typename Name, typename... Fields,
            typename = std::enable_if_t<IsStringLike<Name>::value>,
            typename = std::enable_if_t<
                std::conjunction_v<IsRValue<StructExprField, Fields>...>>>
  ABSL_MUST_USE_RESULT Expr NewStruct(Name name, Fields&&... fields) {
    std::vector<StructExprField> array;
    array.reserve(sizeof...(Fields));
    (array.push_back(std::forward<Fields>(fields)), ...);
    return NewStruct(NextId(), std::move(name), std::move(array));
  }

  template <
      typename Name, typename Fields,
      typename = std::enable_if_t<IsStringLike<Name>::value>,
      typename = std::enable_if_t<IsArrayLike<StructExprField, Fields>::value>>
  ABSL_MUST_USE_RESULT Expr NewStruct(Name name, Fields fields) {
    return NewStruct(NextId(), std::move(name), std::move(fields));
  }

  template <typename Key, typename Value,
            typename = std::enable_if_t<IsExprLike<Key>::value>,
            typename = std::enable_if_t<IsExprLike<Value>::value>>
  ABSL_MUST_USE_RESULT MapExprEntry NewMapEntry(Key key, Value value,
                                                bool optional = false) {
    return NewMapEntry(NextId(), std::move(key), std::move(value), optional);
  }

  template <typename... Entries, typename = std::enable_if_t<std::conjunction_v<
                                     IsRValue<MapExprEntry, Entries>...>>>
  ABSL_MUST_USE_RESULT Expr NewMap(Entries&&... entries) {
    std::vector<MapExprEntry> array;
    array.reserve(sizeof...(Entries));
    (array.push_back(std::forward<Entries>(entries)), ...);
    return NewMap(NextId(), std::move(array));
  }

  template <typename Entries, typename = std::enable_if_t<
                                  IsArrayLike<MapExprEntry, Entries>::value>>
  ABSL_MUST_USE_RESULT Expr NewMap(Entries entries) {
    return NewMap(NextId(), std::move(entries));
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
  ABSL_MUST_USE_RESULT Expr
  NewComprehension(IterVar iter_var, IterRange iter_range, AccuVar accu_var,
                   AccuInit accu_init, LoopCondition loop_condition,
                   LoopStep loop_step, Result result) {
    return NewComprehension(NextId(), std::move(iter_var),
                            std::move(iter_range), std::move(accu_var),
                            std::move(accu_init), std::move(loop_condition),
                            std::move(loop_step), std::move(result));
  }

  template <typename IterVar, typename IterVar2, typename IterRange,
            typename AccuVar, typename AccuInit, typename LoopCondition,
            typename LoopStep, typename Result,
            typename = std::enable_if_t<IsStringLike<IterVar>::value>,
            typename = std::enable_if_t<IsStringLike<IterVar2>::value>,
            typename = std::enable_if_t<IsExprLike<IterRange>::value>,
            typename = std::enable_if_t<IsStringLike<AccuVar>::value>,
            typename = std::enable_if_t<IsExprLike<AccuInit>::value>,
            typename = std::enable_if_t<IsExprLike<LoopStep>::value>,
            typename = std::enable_if_t<IsExprLike<LoopCondition>::value>,
            typename = std::enable_if_t<IsExprLike<Result>::value>>
  ABSL_MUST_USE_RESULT Expr NewComprehension(
      IterVar iter_var, IterVar2 iter_var2, IterRange iter_range,
      AccuVar accu_var, AccuInit accu_init, LoopCondition loop_condition,
      LoopStep loop_step, Result result) {
    return NewComprehension(NextId(), std::move(iter_var), std::move(iter_var2),
                            std::move(iter_range), std::move(accu_var),
                            std::move(accu_init), std::move(loop_condition),
                            std::move(loop_step), std::move(result));
  }

  ABSL_MUST_USE_RESULT virtual Expr ReportError(absl::string_view message) = 0;

  ABSL_MUST_USE_RESULT virtual Expr ReportErrorAt(
      const Expr& expr, absl::string_view message) = 0;

 protected:
  using ExprFactory::AccuVarName;
  using ExprFactory::NewAccuIdent;
  using ExprFactory::NewBoolConst;
  using ExprFactory::NewBytesConst;
  using ExprFactory::NewCall;
  using ExprFactory::NewComprehension;
  using ExprFactory::NewConst;
  using ExprFactory::NewDoubleConst;
  using ExprFactory::NewIdent;
  using ExprFactory::NewIntConst;
  using ExprFactory::NewList;
  using ExprFactory::NewMap;
  using ExprFactory::NewMapEntry;
  using ExprFactory::NewMemberCall;
  using ExprFactory::NewNullConst;
  using ExprFactory::NewPresenceTest;
  using ExprFactory::NewSelect;
  using ExprFactory::NewStringConst;
  using ExprFactory::NewStruct;
  using ExprFactory::NewStructField;
  using ExprFactory::NewUintConst;
  using ExprFactory::NewUnspecified;

  ABSL_MUST_USE_RESULT virtual ExprId NextId() = 0;

  ABSL_MUST_USE_RESULT virtual ExprId CopyId(ExprId id) = 0;

  ABSL_MUST_USE_RESULT ExprId CopyId(const Expr& expr) {
    return CopyId(expr.id());
  }

 private:
  friend class ParserMacroExprFactory;
  friend class TestMacroExprFactory;

  explicit MacroExprFactory(absl::string_view accu_var)
      : ExprFactory(accu_var) {}
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_PARSER_MACRO_EXPR_FACTORY_H_
