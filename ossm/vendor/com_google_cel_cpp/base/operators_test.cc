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

#include <type_traits>

#include "absl/hash/hash_testing.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/internal/operators.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::Eq;
using ::testing::Optional;

template <typename Op, typename OpId>
void TestOperator(Op op, OpId id, absl::string_view name,
                  absl::string_view display_name, int precedence, Arity arity) {
  EXPECT_EQ(op.id(), id);
  EXPECT_EQ(Operator(op).id(), static_cast<OperatorId>(id));
  EXPECT_EQ(op.name(), name);
  EXPECT_EQ(op.display_name(), display_name);
  EXPECT_EQ(op.precedence(), precedence);
  EXPECT_EQ(op.arity(), arity);
  EXPECT_EQ(Operator(op).arity(), arity);
  EXPECT_EQ(Op(Operator(op)), op);
}

void TestUnaryOperator(UnaryOperator op, UnaryOperatorId id,
                       absl::string_view name, absl::string_view display_name,
                       int precedence) {
  TestOperator(op, id, name, display_name, precedence, Arity::kUnary);
}

void TestBinaryOperator(BinaryOperator op, BinaryOperatorId id,
                        absl::string_view name, absl::string_view display_name,
                        int precedence) {
  TestOperator(op, id, name, display_name, precedence, Arity::kBinary);
}

void TestTernaryOperator(TernaryOperator op, TernaryOperatorId id,
                         absl::string_view name, absl::string_view display_name,
                         int precedence) {
  TestOperator(op, id, name, display_name, precedence, Arity::kTernary);
}

TEST(Operator, TypeTraits) {
  EXPECT_FALSE(std::is_default_constructible_v<Operator>);
  EXPECT_TRUE(std::is_copy_constructible_v<Operator>);
  EXPECT_TRUE(std::is_move_constructible_v<Operator>);
  EXPECT_TRUE(std::is_copy_assignable_v<Operator>);
  EXPECT_TRUE(std::is_move_assignable_v<Operator>);
  EXPECT_FALSE((std::is_convertible_v<Operator, UnaryOperator>));
  EXPECT_FALSE((std::is_convertible_v<Operator, BinaryOperator>));
  EXPECT_FALSE((std::is_convertible_v<Operator, TernaryOperator>));
}

TEST(UnaryOperator, TypeTraits) {
  EXPECT_FALSE(std::is_default_constructible_v<UnaryOperator>);
  EXPECT_TRUE(std::is_copy_constructible_v<UnaryOperator>);
  EXPECT_TRUE(std::is_move_constructible_v<UnaryOperator>);
  EXPECT_TRUE(std::is_copy_assignable_v<UnaryOperator>);
  EXPECT_TRUE(std::is_move_assignable_v<UnaryOperator>);
  EXPECT_TRUE((std::is_convertible_v<UnaryOperator, Operator>));
}

TEST(BinaryOperator, TypeTraits) {
  EXPECT_FALSE(std::is_default_constructible_v<BinaryOperator>);
  EXPECT_TRUE(std::is_copy_constructible_v<BinaryOperator>);
  EXPECT_TRUE(std::is_move_constructible_v<BinaryOperator>);
  EXPECT_TRUE(std::is_copy_assignable_v<BinaryOperator>);
  EXPECT_TRUE(std::is_move_assignable_v<BinaryOperator>);
  EXPECT_TRUE((std::is_convertible_v<BinaryOperator, Operator>));
}

TEST(TernaryOperator, TypeTraits) {
  EXPECT_FALSE(std::is_default_constructible_v<TernaryOperator>);
  EXPECT_TRUE(std::is_copy_constructible_v<TernaryOperator>);
  EXPECT_TRUE(std::is_move_constructible_v<TernaryOperator>);
  EXPECT_TRUE(std::is_copy_assignable_v<TernaryOperator>);
  EXPECT_TRUE(std::is_move_assignable_v<TernaryOperator>);
  EXPECT_TRUE((std::is_convertible_v<TernaryOperator, Operator>));
}

#define CEL_UNARY_OPERATOR(id, symbol, name, precedence, arity)          \
  TEST(UnaryOperator, id) {                                              \
    TestUnaryOperator(UnaryOperator::id(), UnaryOperatorId::k##id, name, \
                      symbol, precedence);                               \
  }

CEL_INTERNAL_UNARY_OPERATORS_ENUM(CEL_UNARY_OPERATOR)

#undef CEL_UNARY_OPERATOR

#define CEL_BINARY_OPERATOR(id, symbol, name, precedence, arity)            \
  TEST(BinaryOperator, id) {                                                \
    TestBinaryOperator(BinaryOperator::id(), BinaryOperatorId::k##id, name, \
                       symbol, precedence);                                 \
  }

CEL_INTERNAL_BINARY_OPERATORS_ENUM(CEL_BINARY_OPERATOR)

#undef CEL_BINARY_OPERATOR

#define CEL_TERNARY_OPERATOR(id, symbol, name, precedence, arity)              \
  TEST(TernaryOperator, id) {                                                  \
    TestTernaryOperator(TernaryOperator::id(), TernaryOperatorId::k##id, name, \
                        symbol, precedence);                                   \
  }

CEL_INTERNAL_TERNARY_OPERATORS_ENUM(CEL_TERNARY_OPERATOR)

#undef CEL_TERNARY_OPERATOR

TEST(Operator, FindByName) {
  EXPECT_THAT(Operator::FindByName("@in"), Optional(Eq(Operator::In())));
  EXPECT_THAT(Operator::FindByName("_in_"), Optional(Eq(Operator::OldIn())));
  EXPECT_THAT(Operator::FindByName("in"), Eq(absl::nullopt));
  EXPECT_THAT(Operator::FindByName(""), Eq(absl::nullopt));
}

TEST(Operator, FindByDisplayName) {
  EXPECT_THAT(Operator::FindByDisplayName("-"),
              Optional(Eq(Operator::Subtract())));
  EXPECT_THAT(Operator::FindByDisplayName("@in"), Eq(absl::nullopt));
  EXPECT_THAT(Operator::FindByDisplayName(""), Eq(absl::nullopt));
}

TEST(UnaryOperator, FindByName) {
  EXPECT_THAT(UnaryOperator::FindByName("-_"),
              Optional(Eq(Operator::Negate())));
  EXPECT_THAT(UnaryOperator::FindByName("_-_"), Eq(absl::nullopt));
  EXPECT_THAT(UnaryOperator::FindByName(""), Eq(absl::nullopt));
}

TEST(UnaryOperator, FindByDisplayName) {
  EXPECT_THAT(UnaryOperator::FindByDisplayName("-"),
              Optional(Eq(Operator::Negate())));
  EXPECT_THAT(UnaryOperator::FindByDisplayName("&&"), Eq(absl::nullopt));
  EXPECT_THAT(UnaryOperator::FindByDisplayName(""), Eq(absl::nullopt));
}

TEST(BinaryOperator, FindByName) {
  EXPECT_THAT(BinaryOperator::FindByName("_-_"),
              Optional(Eq(Operator::Subtract())));
  EXPECT_THAT(BinaryOperator::FindByName("-_"), Eq(absl::nullopt));
  EXPECT_THAT(BinaryOperator::FindByName(""), Eq(absl::nullopt));
}

TEST(BinaryOperator, FindByDisplayName) {
  EXPECT_THAT(BinaryOperator::FindByDisplayName("-"),
              Optional(Eq(Operator::Subtract())));
  EXPECT_THAT(BinaryOperator::FindByDisplayName("!"), Eq(absl::nullopt));
  EXPECT_THAT(BinaryOperator::FindByDisplayName(""), Eq(absl::nullopt));
}

TEST(TernaryOperator, FindByName) {
  EXPECT_THAT(TernaryOperator::FindByName("_?_:_"),
              Optional(Eq(TernaryOperator::Conditional())));
  EXPECT_THAT(TernaryOperator::FindByName("-_"), Eq(absl::nullopt));
  EXPECT_THAT(TernaryOperator::FindByName(""), Eq(absl::nullopt));
}

TEST(TernaryOperator, FindByDisplayName) {
  EXPECT_THAT(TernaryOperator::FindByDisplayName(""), Eq(absl::nullopt));
  EXPECT_THAT(TernaryOperator::FindByDisplayName("!"), Eq(absl::nullopt));
}

TEST(Operator, SupportsAbslHash) {
#define CEL_OPERATOR(id, symbol, name, precedence, arity) \
  Operator(Operator::id()),
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {CEL_INTERNAL_OPERATORS_ENUM(CEL_OPERATOR)}));
#undef CEL_OPERATOR
}

TEST(UnaryOperator, SupportsAbslHash) {
#define CEL_UNARY_OPERATOR(id, symbol, name, precedence, arity) \
  UnaryOperator::id(),
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {CEL_INTERNAL_UNARY_OPERATORS_ENUM(CEL_UNARY_OPERATOR)}));
#undef CEL_UNARY_OPERATOR
}

TEST(BinaryOperator, SupportsAbslHash) {
#define CEL_BINARY_OPERATOR(id, symbol, name, precedence, arity) \
  BinaryOperator::id(),
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {CEL_INTERNAL_BINARY_OPERATORS_ENUM(CEL_BINARY_OPERATOR)}));
#undef CEL_BINARY_OPERATOR
}

}  // namespace
}  // namespace cel
