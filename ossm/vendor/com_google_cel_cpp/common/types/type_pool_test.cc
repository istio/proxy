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

#include "common/types/type_pool.h"

#include "common/type.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel::common_internal {
namespace {

using ::cel::internal::GetTestingDescriptorPool;
using ::testing::_;

TEST(TypePool, MakeStructType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeStructType("foo.Bar"),
            MakeBasicStructType("foo.Bar"));
  EXPECT_TRUE(
      type_pool.MakeStructType("cel.expr.conformance.proto3.TestAllTypes")
          .IsMessage());
  EXPECT_DEBUG_DEATH(
      static_cast<void>(type_pool.MakeStructType("google.protobuf.BoolValue")),
      _);
}

TEST(TypePool, MakeFunctionType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeFunctionType(BoolType(), {IntType(), IntType()}),
            FunctionType(&arena, BoolType(), {IntType(), IntType()}));
}

TEST(TypePool, MakeListType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeListType(DynType()), ListType());
  EXPECT_EQ(type_pool.MakeListType(DynType()), JsonListType());
  EXPECT_EQ(type_pool.MakeListType(StringType()),
            ListType(&arena, StringType()));
}

TEST(TypePool, MakeMapType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeMapType(DynType(), DynType()), MapType());
  EXPECT_EQ(type_pool.MakeMapType(StringType(), DynType()), JsonMapType());
  EXPECT_EQ(type_pool.MakeMapType(StringType(), StringType()),
            MapType(&arena, StringType(), StringType()));
}

TEST(TypePool, MakeOpaqueType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeOpaqueType("custom_type", {DynType(), DynType()}),
            OpaqueType(&arena, "custom_type", {DynType(), DynType()}));
}

TEST(TypePool, MakeOptionalType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeOptionalType(DynType()), OptionalType());
  EXPECT_EQ(type_pool.MakeOptionalType(StringType()),
            OptionalType(&arena, StringType()));
}

TEST(TypePool, MakeTypeParamType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeTypeParamType("T"), TypeParamType("T"));
}

TEST(TypePool, MakeTypeType) {
  google::protobuf::Arena arena;
  TypePool type_pool(GetTestingDescriptorPool(), &arena);
  EXPECT_EQ(type_pool.MakeTypeType(BoolType()), TypeType(&arena, BoolType()));
}

}  // namespace
}  // namespace cel::common_internal
