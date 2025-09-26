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

#include "common/decl.h"

#include "absl/status/status.h"
#include "common/constant.h"
#include "common/type.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

TEST(VariableDecl, Name) {
  VariableDecl variable_decl;
  EXPECT_THAT(variable_decl.name(), IsEmpty());
  variable_decl.set_name("foo");
  EXPECT_EQ(variable_decl.name(), "foo");
  EXPECT_EQ(variable_decl.release_name(), "foo");
  EXPECT_THAT(variable_decl.name(), IsEmpty());
}

TEST(VariableDecl, Type) {
  VariableDecl variable_decl;
  EXPECT_EQ(variable_decl.type(), DynType{});
  variable_decl.set_type(StringType{});
  EXPECT_EQ(variable_decl.type(), StringType{});
}

TEST(VariableDecl, Value) {
  VariableDecl variable_decl;
  EXPECT_FALSE(variable_decl.has_value());
  EXPECT_EQ(variable_decl.value(), Constant{});
  Constant value;
  value.set_bool_value(true);
  variable_decl.set_value(value);
  EXPECT_TRUE(variable_decl.has_value());
  EXPECT_EQ(variable_decl.value(), value);
  EXPECT_EQ(variable_decl.release_value(), value);
  EXPECT_EQ(variable_decl.value(), Constant{});
}

Constant MakeBoolConstant(bool value) {
  Constant constant;
  constant.set_bool_value(value);
  return constant;
}

TEST(VariableDecl, Equality) {
  VariableDecl variable_decl;
  EXPECT_EQ(variable_decl, VariableDecl{});
  variable_decl.mutable_value().set_bool_value(true);
  EXPECT_NE(variable_decl, VariableDecl{});

  EXPECT_EQ(MakeVariableDecl("foo", StringType{}),
            MakeVariableDecl("foo", StringType{}));
  EXPECT_EQ(MakeVariableDecl("foo", StringType{}),
            MakeVariableDecl("foo", StringType{}));
  EXPECT_EQ(
      MakeConstantVariableDecl("foo", StringType{}, MakeBoolConstant(true)),
      MakeConstantVariableDecl("foo", StringType{}, MakeBoolConstant(true)));
  EXPECT_EQ(
      MakeConstantVariableDecl("foo", StringType{}, MakeBoolConstant(true)),
      MakeConstantVariableDecl("foo", StringType{}, MakeBoolConstant(true)));
}

TEST(OverloadDecl, Id) {
  OverloadDecl overload_decl;
  EXPECT_THAT(overload_decl.id(), IsEmpty());
  overload_decl.set_id("foo");
  EXPECT_EQ(overload_decl.id(), "foo");
  EXPECT_EQ(overload_decl.release_id(), "foo");
  EXPECT_THAT(overload_decl.id(), IsEmpty());
}

TEST(OverloadDecl, Result) {
  OverloadDecl overload_decl;
  EXPECT_EQ(overload_decl.result(), DynType{});
  overload_decl.set_result(StringType{});
  EXPECT_EQ(overload_decl.result(), StringType{});
}

TEST(OverloadDecl, Args) {
  OverloadDecl overload_decl;
  EXPECT_THAT(overload_decl.args(), IsEmpty());
  overload_decl.mutable_args().push_back(StringType{});
  EXPECT_THAT(overload_decl.args(), ElementsAre(StringType{}));
  EXPECT_THAT(overload_decl.release_args(), ElementsAre(StringType{}));
  EXPECT_THAT(overload_decl.args(), IsEmpty());
}

TEST(OverloadDecl, Member) {
  OverloadDecl overload_decl;
  EXPECT_FALSE(overload_decl.member());
  overload_decl.set_member(true);
  EXPECT_TRUE(overload_decl.member());
}

TEST(OverloadDecl, Equality) {
  OverloadDecl overload_decl;
  EXPECT_EQ(overload_decl, OverloadDecl{});
  overload_decl.set_member(true);
  EXPECT_NE(overload_decl, OverloadDecl{});
}

TEST(OverloadDecl, GetTypeParams) {
  google::protobuf::Arena arena;
  auto overload_decl = MakeOverloadDecl(
      "foo", ListType(&arena, TypeParamType("A")),
      MapType(&arena, TypeParamType("B"), TypeParamType("C")),
      OpaqueType(&arena, "bar",
                 {FunctionType(&arena, TypeParamType("D"), {})}));
  EXPECT_THAT(overload_decl.GetTypeParams(),
              UnorderedElementsAre("A", "B", "C", "D"));
}

TEST(FunctionDecl, Name) {
  FunctionDecl function_decl;
  EXPECT_THAT(function_decl.name(), IsEmpty());
  function_decl.set_name("foo");
  EXPECT_EQ(function_decl.name(), "foo");
  EXPECT_EQ(function_decl.release_name(), "foo");
  EXPECT_THAT(function_decl.name(), IsEmpty());
}

TEST(FunctionDecl, Overloads) {
  ASSERT_OK_AND_ASSIGN(
      auto function_decl,
      MakeFunctionDecl(
          "hello", MakeOverloadDecl("foo", StringType{}, StringType{}),
          MakeMemberOverloadDecl("bar", StringType{}, StringType{}),
          MakeOverloadDecl("baz", IntType{}, IntType{})));

  EXPECT_THAT(function_decl.overloads(),
              ElementsAre(Property(&OverloadDecl::id, "foo"),
                          Property(&OverloadDecl::id, "bar"),
                          Property(&OverloadDecl::id, "baz")));

  EXPECT_THAT(function_decl.AddOverload(
                  MakeOverloadDecl("qux", DynType{}, StringType{})),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

using common_internal::TypeIsAssignable;

TEST(TypeIsAssignable, BoolWrapper) {
  EXPECT_TRUE(TypeIsAssignable(BoolWrapperType{}, BoolWrapperType{}));
  EXPECT_TRUE(TypeIsAssignable(BoolWrapperType{}, NullType{}));
  EXPECT_TRUE(TypeIsAssignable(BoolWrapperType{}, BoolType{}));
  EXPECT_FALSE(TypeIsAssignable(BoolWrapperType{}, DurationType{}));
}

TEST(TypeIsAssignable, IntWrapper) {
  EXPECT_TRUE(TypeIsAssignable(IntWrapperType{}, IntWrapperType{}));
  EXPECT_TRUE(TypeIsAssignable(IntWrapperType{}, NullType{}));
  EXPECT_TRUE(TypeIsAssignable(IntWrapperType{}, IntType{}));
  EXPECT_FALSE(TypeIsAssignable(IntWrapperType{}, DurationType{}));
}

TEST(TypeIsAssignable, UintWrapper) {
  EXPECT_TRUE(TypeIsAssignable(UintWrapperType{}, UintWrapperType{}));
  EXPECT_TRUE(TypeIsAssignable(UintWrapperType{}, NullType{}));
  EXPECT_TRUE(TypeIsAssignable(UintWrapperType{}, UintType{}));
  EXPECT_FALSE(TypeIsAssignable(UintWrapperType{}, DurationType{}));
}

TEST(TypeIsAssignable, DoubleWrapper) {
  EXPECT_TRUE(TypeIsAssignable(DoubleWrapperType{}, DoubleWrapperType{}));
  EXPECT_TRUE(TypeIsAssignable(DoubleWrapperType{}, NullType{}));
  EXPECT_TRUE(TypeIsAssignable(DoubleWrapperType{}, DoubleType{}));
  EXPECT_FALSE(TypeIsAssignable(DoubleWrapperType{}, DurationType{}));
}

TEST(TypeIsAssignable, BytesWrapper) {
  EXPECT_TRUE(TypeIsAssignable(BytesWrapperType{}, BytesWrapperType{}));
  EXPECT_TRUE(TypeIsAssignable(BytesWrapperType{}, NullType{}));
  EXPECT_TRUE(TypeIsAssignable(BytesWrapperType{}, BytesType{}));
  EXPECT_FALSE(TypeIsAssignable(BytesWrapperType{}, DurationType{}));
}

TEST(TypeIsAssignable, StringWrapper) {
  EXPECT_TRUE(TypeIsAssignable(StringWrapperType{}, StringWrapperType{}));
  EXPECT_TRUE(TypeIsAssignable(StringWrapperType{}, NullType{}));
  EXPECT_TRUE(TypeIsAssignable(StringWrapperType{}, StringType{}));
  EXPECT_FALSE(TypeIsAssignable(StringWrapperType{}, DurationType{}));
}

TEST(TypeIsAssignable, Complex) {
  google::protobuf::Arena arena;
  EXPECT_TRUE(TypeIsAssignable(OptionalType(&arena, DynType{}),
                               OptionalType(&arena, StringType{})));
  EXPECT_FALSE(TypeIsAssignable(OptionalType(&arena, BoolType{}),
                                OptionalType(&arena, StringType{})));
}

}  // namespace
}  // namespace cel
