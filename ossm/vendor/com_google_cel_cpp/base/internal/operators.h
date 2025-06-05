// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_OPERATORS_H_

#include "absl/strings/string_view.h"

namespace cel {

enum class OperatorId;

namespace base_internal {

struct OperatorData final {
  OperatorData() = delete;
  OperatorData(const OperatorData&) = delete;
  OperatorData(OperatorData&&) = delete;
  OperatorData& operator=(const OperatorData&) = delete;
  OperatorData& operator=(OperatorData&&) = delete;

  constexpr OperatorData(cel::OperatorId id, absl::string_view name,
                         absl::string_view display_name, int precedence,
                         int arity)
      : id(id),
        name(name),
        display_name(display_name),
        precedence(precedence),
        arity(arity) {}

  const cel::OperatorId id;
  const absl::string_view name;
  const absl::string_view display_name;
  const int precedence;
  const int arity;
};

#define CEL_INTERNAL_UNARY_OPERATORS_ENUM(XX)           \
  XX(LogicalNot, "!", "!_", 2, 1)                       \
  XX(Negate, "-", "-_", 2, 1)                           \
  XX(NotStrictlyFalse, "", "@not_strictly_false", 0, 1) \
  XX(OldNotStrictlyFalse, "", "__not_strictly_false__", 0, 1)

#define CEL_INTERNAL_BINARY_OPERATORS_ENUM(XX) \
  XX(Equals, "==", "_==_", 5, 2)               \
  XX(NotEquals, "!=", "_!=_", 5, 2)            \
  XX(Less, "<", "_<_", 5, 2)                   \
  XX(LessEquals, "<=", "_<=_", 5, 2)           \
  XX(Greater, ">", "_>_", 5, 2)                \
  XX(GreaterEquals, ">=", "_>=_", 5, 2)        \
  XX(In, "in", "@in", 5, 2)                    \
  XX(OldIn, "in", "_in_", 5, 2)                \
  XX(Index, "", "_[_]", 1, 2)                  \
  XX(LogicalOr, "||", "_||_", 7, 2)            \
  XX(LogicalAnd, "&&", "_&&_", 6, 2)           \
  XX(Add, "+", "_+_", 4, 2)                    \
  XX(Subtract, "-", "_-_", 4, 2)               \
  XX(Multiply, "*", "_*_", 3, 2)               \
  XX(Divide, "/", "_/_", 3, 2)                 \
  XX(Modulo, "%", "_%_", 3, 2)

#define CEL_INTERNAL_TERNARY_OPERATORS_ENUM(XX) \
  XX(Conditional, "", "_?_:_", 8, 3)

// Macro definining all the operators and their properties.
// (1) - The identifier.
// (2) - The display name if applicable, otherwise an empty string.
// (3) - The name.
// (4) - The precedence if applicable, otherwise 0.
// (5) - The arity.
#define CEL_INTERNAL_OPERATORS_ENUM(XX)   \
  CEL_INTERNAL_TERNARY_OPERATORS_ENUM(XX) \
  CEL_INTERNAL_BINARY_OPERATORS_ENUM(XX)  \
  CEL_INTERNAL_UNARY_OPERATORS_ENUM(XX)

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_OPERATORS_H_
