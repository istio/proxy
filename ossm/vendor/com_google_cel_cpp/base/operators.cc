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

#include "base/operators.h"

#include <algorithm>
#include <array>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/internal/operators.h"

namespace cel {

namespace {

using base_internal::OperatorData;

struct OperatorDataNameComparer {
  using is_transparent = void;

  bool operator()(const OperatorData* lhs, const OperatorData* rhs) const {
    return lhs->name < rhs->name;
  }

  bool operator()(const OperatorData* lhs, absl::string_view rhs) const {
    return lhs->name < rhs;
  }

  bool operator()(absl::string_view lhs, const OperatorData* rhs) const {
    return lhs < rhs->name;
  }
};

struct OperatorDataDisplayNameComparer {
  using is_transparent = void;

  bool operator()(const OperatorData* lhs, const OperatorData* rhs) const {
    return lhs->display_name < rhs->display_name;
  }

  bool operator()(const OperatorData* lhs, absl::string_view rhs) const {
    return lhs->display_name < rhs;
  }

  bool operator()(absl::string_view lhs, const OperatorData* rhs) const {
    return lhs < rhs->display_name;
  }
};

#define CEL_OPERATORS_DATA(id, symbol, name, precedence, arity) \
  ABSL_CONST_INIT const OperatorData id##_storage = {           \
      OperatorId::k##id, name, symbol, precedence, arity};
CEL_INTERNAL_OPERATORS_ENUM(CEL_OPERATORS_DATA)
#undef CEL_OPERATORS_DATA

#define CEL_OPERATORS_COUNT(id, symbol, name, precedence, arity) +1

using OperatorsArray =
    std::array<const OperatorData*,
               0 + CEL_INTERNAL_OPERATORS_ENUM(CEL_OPERATORS_COUNT)>;

using UnaryOperatorsArray =
    std::array<const OperatorData*,
               0 + CEL_INTERNAL_UNARY_OPERATORS_ENUM(CEL_OPERATORS_COUNT)>;

using BinaryOperatorsArray =
    std::array<const OperatorData*,
               0 + CEL_INTERNAL_BINARY_OPERATORS_ENUM(CEL_OPERATORS_COUNT)>;

using TernaryOperatorsArray =
    std::array<const OperatorData*,
               0 + CEL_INTERNAL_TERNARY_OPERATORS_ENUM(CEL_OPERATORS_COUNT)>;

#undef CEL_OPERATORS_COUNT

ABSL_CONST_INIT absl::once_flag operators_once_flag;

#define CEL_OPERATORS_DO(id, symbol, name, precedence, arity) &id##_storage,

OperatorsArray operators_by_name = {
    CEL_INTERNAL_OPERATORS_ENUM(CEL_OPERATORS_DO)};

OperatorsArray operators_by_display_name = {
    CEL_INTERNAL_OPERATORS_ENUM(CEL_OPERATORS_DO)};

UnaryOperatorsArray unary_operators_by_name = {
    CEL_INTERNAL_UNARY_OPERATORS_ENUM(CEL_OPERATORS_DO)};

UnaryOperatorsArray unary_operators_by_display_name = {
    CEL_INTERNAL_UNARY_OPERATORS_ENUM(CEL_OPERATORS_DO)};

BinaryOperatorsArray binary_operators_by_name = {
    CEL_INTERNAL_BINARY_OPERATORS_ENUM(CEL_OPERATORS_DO)};

BinaryOperatorsArray binary_operators_by_display_name = {
    CEL_INTERNAL_BINARY_OPERATORS_ENUM(CEL_OPERATORS_DO)};

TernaryOperatorsArray ternary_operators_by_name = {
    CEL_INTERNAL_TERNARY_OPERATORS_ENUM(CEL_OPERATORS_DO)};

TernaryOperatorsArray ternary_operators_by_display_name = {
    CEL_INTERNAL_TERNARY_OPERATORS_ENUM(CEL_OPERATORS_DO)};

#undef CEL_OPERATORS_DO

void InitializeOperators() {
  std::stable_sort(operators_by_name.begin(), operators_by_name.end(),
                   OperatorDataNameComparer{});
  std::stable_sort(operators_by_display_name.begin(),
                   operators_by_display_name.end(),
                   OperatorDataDisplayNameComparer{});
  std::stable_sort(unary_operators_by_name.begin(),
                   unary_operators_by_name.end(), OperatorDataNameComparer{});
  std::stable_sort(unary_operators_by_display_name.begin(),
                   unary_operators_by_display_name.end(),
                   OperatorDataDisplayNameComparer{});
  std::stable_sort(binary_operators_by_name.begin(),
                   binary_operators_by_name.end(), OperatorDataNameComparer{});
  std::stable_sort(binary_operators_by_display_name.begin(),
                   binary_operators_by_display_name.end(),
                   OperatorDataDisplayNameComparer{});
  std::stable_sort(ternary_operators_by_name.begin(),
                   ternary_operators_by_name.end(), OperatorDataNameComparer{});
  std::stable_sort(ternary_operators_by_display_name.begin(),
                   ternary_operators_by_display_name.end(),
                   OperatorDataDisplayNameComparer{});
}

}  // namespace

UnaryOperator::UnaryOperator(Operator op) : data_(op.data_) {
  ABSL_CHECK(op.arity() == Arity::kUnary);  // Crask OK
}

BinaryOperator::BinaryOperator(Operator op) : data_(op.data_) {
  ABSL_CHECK(op.arity() == Arity::kBinary);  // Crask OK
}

TernaryOperator::TernaryOperator(Operator op) : data_(op.data_) {
  ABSL_CHECK(op.arity() == Arity::kTernary);  // Crask OK
}

#define CEL_UNARY_OPERATOR(id, symbol, name, precedence, arity) \
  UnaryOperator Operator::id() { return UnaryOperator(&id##_storage); }

CEL_INTERNAL_UNARY_OPERATORS_ENUM(CEL_UNARY_OPERATOR)

#undef CEL_UNARY_OPERATOR

#define CEL_BINARY_OPERATOR(id, symbol, name, precedence, arity) \
  BinaryOperator Operator::id() { return BinaryOperator(&id##_storage); }

CEL_INTERNAL_BINARY_OPERATORS_ENUM(CEL_BINARY_OPERATOR)

#undef CEL_BINARY_OPERATOR

#define CEL_TERNARY_OPERATOR(id, symbol, name, precedence, arity) \
  TernaryOperator Operator::id() { return TernaryOperator(&id##_storage); }

CEL_INTERNAL_TERNARY_OPERATORS_ENUM(CEL_TERNARY_OPERATOR)

#undef CEL_TERNARY_OPERATOR

absl::optional<Operator> Operator::FindByName(absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it =
      std::lower_bound(operators_by_name.cbegin(), operators_by_name.cend(),
                       input, OperatorDataNameComparer{});
  if (it == operators_by_name.cend() || (*it)->name != input) {
    return absl::nullopt;
  }
  return Operator(*it);
}

absl::optional<Operator> Operator::FindByDisplayName(absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(operators_by_display_name.cbegin(),
                             operators_by_display_name.cend(), input,
                             OperatorDataDisplayNameComparer{});
  if (it == operators_by_name.cend() || (*it)->display_name != input) {
    return absl::nullopt;
  }
  return Operator(*it);
}

absl::optional<UnaryOperator> UnaryOperator::FindByName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(unary_operators_by_name.cbegin(),
                             unary_operators_by_name.cend(), input,
                             OperatorDataNameComparer{});
  if (it == unary_operators_by_name.cend() || (*it)->name != input) {
    return absl::nullopt;
  }
  return UnaryOperator(*it);
}

absl::optional<UnaryOperator> UnaryOperator::FindByDisplayName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(unary_operators_by_display_name.cbegin(),
                             unary_operators_by_display_name.cend(), input,
                             OperatorDataDisplayNameComparer{});
  if (it == unary_operators_by_display_name.cend() ||
      (*it)->display_name != input) {
    return absl::nullopt;
  }
  return UnaryOperator(*it);
}

absl::optional<BinaryOperator> BinaryOperator::FindByName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(binary_operators_by_name.cbegin(),
                             binary_operators_by_name.cend(), input,
                             OperatorDataNameComparer{});
  if (it == binary_operators_by_name.cend() || (*it)->name != input) {
    return absl::nullopt;
  }
  return BinaryOperator(*it);
}

absl::optional<BinaryOperator> BinaryOperator::FindByDisplayName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(binary_operators_by_display_name.cbegin(),
                             binary_operators_by_display_name.cend(), input,
                             OperatorDataDisplayNameComparer{});
  if (it == binary_operators_by_display_name.cend() ||
      (*it)->display_name != input) {
    return absl::nullopt;
  }
  return BinaryOperator(*it);
}

absl::optional<TernaryOperator> TernaryOperator::FindByName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(ternary_operators_by_name.cbegin(),
                             ternary_operators_by_name.cend(), input,
                             OperatorDataNameComparer{});
  if (it == ternary_operators_by_name.cend() || (*it)->name != input) {
    return absl::nullopt;
  }
  return TernaryOperator(*it);
}

absl::optional<TernaryOperator> TernaryOperator::FindByDisplayName(
    absl::string_view input) {
  absl::call_once(operators_once_flag, InitializeOperators);
  if (input.empty()) {
    return absl::nullopt;
  }
  auto it = std::lower_bound(ternary_operators_by_display_name.cbegin(),
                             ternary_operators_by_display_name.cend(), input,
                             OperatorDataDisplayNameComparer{});
  if (it == ternary_operators_by_display_name.cend() ||
      (*it)->display_name != input) {
    return absl::nullopt;
  }
  return TernaryOperator(*it);
}

}  // namespace cel
