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

#include "common/reference.h"

#include <cstdint>
#include <string>
#include <vector>

#include "common/constant.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::VariantWith;

TEST(VariableReference, Value) {
  VariableReference variable_reference;
  EXPECT_FALSE(variable_reference.has_value());
  EXPECT_EQ(variable_reference.value(), Constant{});
  Constant value;
  value.set_bool_value(true);
  variable_reference.set_value(value);
  EXPECT_TRUE(variable_reference.has_value());
  EXPECT_EQ(variable_reference.value(), value);
  EXPECT_EQ(variable_reference.release_value(), value);
  EXPECT_EQ(variable_reference.value(), Constant{});
}

TEST(VariableReference, Equality) {
  VariableReference variable_reference;
  EXPECT_EQ(variable_reference, VariableReference{});
  variable_reference.mutable_value().set_bool_value(true);
  EXPECT_NE(variable_reference, VariableReference{});
}

TEST(FunctionReference, Overloads) {
  FunctionReference function_reference;
  EXPECT_THAT(function_reference.overloads(), IsEmpty());
  function_reference.mutable_overloads().reserve(2);
  function_reference.mutable_overloads().push_back("foo");
  function_reference.mutable_overloads().push_back("bar");
  EXPECT_THAT(function_reference.release_overloads(),
              ElementsAre("foo", "bar"));
  EXPECT_THAT(function_reference.overloads(), IsEmpty());
}

TEST(FunctionReference, Equality) {
  FunctionReference function_reference;
  EXPECT_EQ(function_reference, FunctionReference{});
  function_reference.mutable_overloads().push_back("foo");
  EXPECT_NE(function_reference, FunctionReference{});
}

TEST(Reference, Name) {
  Reference reference;
  EXPECT_THAT(reference.name(), IsEmpty());
  reference.set_name("foo");
  EXPECT_EQ(reference.name(), "foo");
  EXPECT_EQ(reference.release_name(), "foo");
  EXPECT_THAT(reference.name(), IsEmpty());
}

TEST(Reference, Variable) {
  Reference reference;
  EXPECT_THAT(reference.kind(), VariantWith<VariableReference>(_));
  EXPECT_TRUE(reference.has_variable());
  EXPECT_THAT(reference.release_variable(), Eq(VariableReference{}));
  EXPECT_TRUE(reference.has_variable());
}

TEST(Reference, Function) {
  Reference reference;
  EXPECT_FALSE(reference.has_function());
  EXPECT_THAT(reference.function(), Eq(FunctionReference{}));
  reference.mutable_function();
  EXPECT_TRUE(reference.has_function());
  EXPECT_THAT(reference.variable(), Eq(VariableReference{}));
  EXPECT_THAT(reference.kind(), VariantWith<FunctionReference>(_));
  EXPECT_THAT(reference.release_function(), Eq(FunctionReference{}));
  EXPECT_FALSE(reference.has_function());
}

TEST(Reference, Equality) {
  EXPECT_EQ(MakeVariableReference("foo"), MakeVariableReference("foo"));
  EXPECT_NE(MakeVariableReference("foo"),
            MakeConstantVariableReference("foo", Constant(int64_t{1})));
  EXPECT_EQ(
      MakeFunctionReference("foo", std::vector<std::string>{"bar", "baz"}),
      MakeFunctionReference("foo", std::vector<std::string>{"bar", "baz"}));
  EXPECT_NE(
      MakeFunctionReference("foo", std::vector<std::string>{"bar", "baz"}),
      MakeFunctionReference("foo", std::vector<std::string>{"bar"}));
}

}  // namespace
}  // namespace cel
