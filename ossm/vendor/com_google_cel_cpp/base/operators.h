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

#ifndef THIRD_PARTY_CEL_CPP_BASE_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_BASE_OPERATORS_H_

#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/internal/operators.h"

namespace cel {

enum class Arity {
  kUnary = 1,
  kBinary = 2,
  kTernary = 3,
};

enum class OperatorId {
  kConditional = 1,
  kLogicalAnd,
  kLogicalOr,
  kLogicalNot,
  kEquals,
  kNotEquals,
  kLess,
  kLessEquals,
  kGreater,
  kGreaterEquals,
  kAdd,
  kSubtract,
  kMultiply,
  kDivide,
  kModulo,
  kNegate,
  kIndex,
  kIn,
  kNotStrictlyFalse,
  kOldIn,
  kOldNotStrictlyFalse,
};

enum class UnaryOperatorId {
  kLogicalNot = static_cast<int>(OperatorId::kLogicalNot),
  kNegate = static_cast<int>(OperatorId::kNegate),
  kNotStrictlyFalse = static_cast<int>(OperatorId::kNotStrictlyFalse),
  kOldNotStrictlyFalse = static_cast<int>(OperatorId::kOldNotStrictlyFalse),
};

enum class BinaryOperatorId {
  kLogicalAnd = static_cast<int>(OperatorId::kLogicalAnd),
  kLogicalOr = static_cast<int>(OperatorId::kLogicalOr),
  kEquals = static_cast<int>(OperatorId::kEquals),
  kNotEquals = static_cast<int>(OperatorId::kNotEquals),
  kLess = static_cast<int>(OperatorId::kLess),
  kLessEquals = static_cast<int>(OperatorId::kLessEquals),
  kGreater = static_cast<int>(OperatorId::kGreater),
  kGreaterEquals = static_cast<int>(OperatorId::kGreaterEquals),
  kAdd = static_cast<int>(OperatorId::kAdd),
  kSubtract = static_cast<int>(OperatorId::kSubtract),
  kMultiply = static_cast<int>(OperatorId::kMultiply),
  kDivide = static_cast<int>(OperatorId::kDivide),
  kModulo = static_cast<int>(OperatorId::kModulo),
  kIndex = static_cast<int>(OperatorId::kIndex),
  kIn = static_cast<int>(OperatorId::kIn),
  kOldIn = static_cast<int>(OperatorId::kOldIn),
};

enum class TernaryOperatorId {
  kConditional = static_cast<int>(OperatorId::kConditional),
};

class UnaryOperator;
class BinaryOperator;
class TernaryOperator;

class Operator final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static TernaryOperator Conditional();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator LogicalAnd();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator LogicalOr();
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator LogicalNot();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Equals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator NotEquals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Less();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator LessEquals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Greater();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator GreaterEquals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Add();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Subtract();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Multiply();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Divide();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Modulo();
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator Negate();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Index();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator In();
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator NotStrictlyFalse();
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator OldIn();
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator OldNotStrictlyFalse();

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<Operator> FindByName(
      absl::string_view input);

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<Operator>
  FindByDisplayName(absl::string_view input);

  Operator() = delete;
  Operator(const Operator&) = default;
  Operator(Operator&&) = default;
  Operator& operator=(const Operator&) = default;
  Operator& operator=(Operator&&) = default;

  constexpr OperatorId id() const { return data_->id; }

  // Returns the name of the operator. This is the managed representation of the
  // operator, for example "_&&_".
  constexpr absl::string_view name() const { return data_->name; }

  // Returns the source text representation of the operator. This is the
  // unmanaged text representation of the operator, for example "&&".
  //
  // Note that this will be empty for operators like Conditional() and Index().
  constexpr absl::string_view display_name() const {
    return data_->display_name;
  }

  constexpr int precedence() const { return data_->precedence; }

  constexpr Arity arity() const { return static_cast<Arity>(data_->arity); }

 private:
  friend class UnaryOperator;
  friend class BinaryOperator;
  friend class TernaryOperator;

  constexpr explicit Operator(const base_internal::OperatorData* data)
      : data_(data) {}

  const base_internal::OperatorData* data_;
};

constexpr bool operator==(const Operator& lhs, const Operator& rhs) {
  return lhs.id() == rhs.id();
}

constexpr bool operator==(OperatorId lhs, const Operator& rhs) {
  return lhs == rhs.id();
}

constexpr bool operator==(const Operator& lhs, OperatorId rhs) {
  return operator==(rhs, lhs);
}

constexpr bool operator!=(const Operator& lhs, const Operator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(OperatorId lhs, const Operator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(const Operator& lhs, OperatorId rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const Operator& op) {
  return H::combine(std::move(state), static_cast<int>(op.id()));
}

class UnaryOperator final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator LogicalNot() {
    return Operator::LogicalNot();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator Negate() {
    return Operator::Negate();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator NotStrictlyFalse() {
    return Operator::NotStrictlyFalse();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static UnaryOperator OldNotStrictlyFalse() {
    return Operator::OldNotStrictlyFalse();
  }

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<UnaryOperator> FindByName(
      absl::string_view input);

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<UnaryOperator>
  FindByDisplayName(absl::string_view input);

  UnaryOperator() = delete;
  UnaryOperator(const UnaryOperator&) = default;
  UnaryOperator(UnaryOperator&&) = default;
  UnaryOperator& operator=(const UnaryOperator&) = default;
  UnaryOperator& operator=(UnaryOperator&&) = default;

  // Support for explicit casting of Operator to UnaryOperator.
  // `Operator::arity()` must return `Arity::kUnary`, or this will crash.
  explicit UnaryOperator(Operator op);

  constexpr UnaryOperatorId id() const {
    return static_cast<UnaryOperatorId>(data_->id);
  }

  // Returns the name of the operator. This is the managed representation of the
  // operator, for example "_&&_".
  constexpr absl::string_view name() const { return data_->name; }

  // Returns the source text representation of the operator. This is the
  // unmanaged text representation of the operator, for example "&&".
  //
  // Note that this will be empty for operators like Conditional() and Index().
  constexpr absl::string_view display_name() const {
    return data_->display_name;
  }

  constexpr int precedence() const { return data_->precedence; }

  constexpr Arity arity() const {
    ABSL_ASSERT(data_->arity == 1);
    return Arity::kUnary;
  }

  constexpr operator Operator() const {  // NOLINT(google-explicit-constructor)
    return Operator(data_);
  }

 private:
  friend class Operator;

  constexpr explicit UnaryOperator(const base_internal::OperatorData* data)
      : data_(data) {}

  const base_internal::OperatorData* data_;
};

constexpr bool operator==(const UnaryOperator& lhs, const UnaryOperator& rhs) {
  return lhs.id() == rhs.id();
}

constexpr bool operator==(UnaryOperatorId lhs, const UnaryOperator& rhs) {
  return lhs == rhs.id();
}

constexpr bool operator==(const UnaryOperator& lhs, UnaryOperatorId rhs) {
  return operator==(rhs, lhs);
}

constexpr bool operator!=(const UnaryOperator& lhs, const UnaryOperator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(UnaryOperatorId lhs, const UnaryOperator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(const UnaryOperator& lhs, UnaryOperatorId rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const UnaryOperator& op) {
  return H::combine(std::move(state), static_cast<int>(op.id()));
}

class BinaryOperator final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator LogicalAnd() {
    return Operator::LogicalAnd();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator LogicalOr() {
    return Operator::LogicalOr();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Equals() {
    return Operator::Equals();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator NotEquals() {
    return Operator::NotEquals();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Less() {
    return Operator::Less();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator LessEquals() {
    return Operator::LessEquals();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Greater() {
    return Operator::Greater();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator GreaterEquals() {
    return Operator::GreaterEquals();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Add() {
    return Operator::Add();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Subtract() {
    return Operator::Subtract();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Multiply() {
    return Operator::Multiply();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Divide() {
    return Operator::Divide();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Modulo() {
    return Operator::Modulo();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator Index() {
    return Operator::Index();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator In() {
    return Operator::In();
  }
  ABSL_ATTRIBUTE_PURE_FUNCTION static BinaryOperator OldIn() {
    return Operator::OldIn();
  }

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<BinaryOperator> FindByName(
      absl::string_view input);

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<BinaryOperator>
  FindByDisplayName(absl::string_view input);

  BinaryOperator() = delete;
  BinaryOperator(const BinaryOperator&) = default;
  BinaryOperator(BinaryOperator&&) = default;
  BinaryOperator& operator=(const BinaryOperator&) = default;
  BinaryOperator& operator=(BinaryOperator&&) = default;

  // Support for explicit casting of Operator to BinaryOperator.
  // `Operator::arity()` must return `Arity::kBinary`, or this will crash.
  explicit BinaryOperator(Operator op);

  constexpr BinaryOperatorId id() const {
    return static_cast<BinaryOperatorId>(data_->id);
  }

  // Returns the name of the operator. This is the managed representation of the
  // operator, for example "_&&_".
  constexpr absl::string_view name() const { return data_->name; }

  // Returns the source text representation of the operator. This is the
  // unmanaged text representation of the operator, for example "&&".
  //
  // Note that this will be empty for operators like Conditional() and Index().
  constexpr absl::string_view display_name() const {
    return data_->display_name;
  }

  constexpr int precedence() const { return data_->precedence; }

  constexpr Arity arity() const {
    ABSL_ASSERT(data_->arity == 2);
    return Arity::kBinary;
  }

  constexpr operator Operator() const {  // NOLINT(google-explicit-constructor)
    return Operator(data_);
  }

 private:
  friend class Operator;

  constexpr explicit BinaryOperator(const base_internal::OperatorData* data)
      : data_(data) {}

  const base_internal::OperatorData* data_;
};

constexpr bool operator==(const BinaryOperator& lhs,
                          const BinaryOperator& rhs) {
  return lhs.id() == rhs.id();
}

constexpr bool operator==(BinaryOperatorId lhs, const BinaryOperator& rhs) {
  return lhs == rhs.id();
}

constexpr bool operator==(const BinaryOperator& lhs, BinaryOperatorId rhs) {
  return operator==(rhs, lhs);
}

constexpr bool operator!=(const BinaryOperator& lhs,
                          const BinaryOperator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(BinaryOperatorId lhs, const BinaryOperator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(const BinaryOperator& lhs, BinaryOperatorId rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const BinaryOperator& op) {
  return H::combine(std::move(state), static_cast<int>(op.id()));
}

class TernaryOperator final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static TernaryOperator Conditional() {
    return Operator::Conditional();
  }

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<TernaryOperator>
  FindByName(absl::string_view input);

  ABSL_ATTRIBUTE_PURE_FUNCTION static absl::optional<TernaryOperator>
  FindByDisplayName(absl::string_view input);

  TernaryOperator() = delete;
  TernaryOperator(const TernaryOperator&) = default;
  TernaryOperator(TernaryOperator&&) = default;
  TernaryOperator& operator=(const TernaryOperator&) = default;
  TernaryOperator& operator=(TernaryOperator&&) = default;

  // Support for explicit casting of Operator to TernaryOperator.
  // `Operator::arity()` must return `Arity::kTernary`, or this will crash.
  explicit TernaryOperator(Operator op);

  constexpr TernaryOperatorId id() const {
    return static_cast<TernaryOperatorId>(data_->id);
  }

  // Returns the name of the operator. This is the managed representation of the
  // operator, for example "_&&_".
  constexpr absl::string_view name() const { return data_->name; }

  // Returns the source text representation of the operator. This is the
  // unmanaged text representation of the operator, for example "&&".
  //
  // Note that this will be empty for operators like Conditional() and Index().
  constexpr absl::string_view display_name() const {
    return data_->display_name;
  }

  constexpr int precedence() const { return data_->precedence; }

  constexpr Arity arity() const {
    ABSL_ASSERT(data_->arity == 3);
    return Arity::kTernary;
  }

  constexpr operator Operator() const {  // NOLINT(google-explicit-constructor)
    return Operator(data_);
  }

 private:
  friend class Operator;

  constexpr explicit TernaryOperator(const base_internal::OperatorData* data)
      : data_(data) {}

  const base_internal::OperatorData* data_;
};

constexpr bool operator==(const TernaryOperator& lhs,
                          const TernaryOperator& rhs) {
  return lhs.id() == rhs.id();
}

constexpr bool operator==(TernaryOperatorId lhs, const TernaryOperator& rhs) {
  return lhs == rhs.id();
}

constexpr bool operator==(const TernaryOperator& lhs, TernaryOperatorId rhs) {
  return operator==(rhs, lhs);
}

constexpr bool operator!=(const TernaryOperator& lhs,
                          const TernaryOperator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(TernaryOperatorId lhs, const TernaryOperator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(const TernaryOperator& lhs, TernaryOperatorId rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const TernaryOperator& op) {
  return H::combine(std::move(state), static_cast<int>(op.id()));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_OPERATORS_H_
