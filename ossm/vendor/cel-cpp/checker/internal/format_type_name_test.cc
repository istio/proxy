// Copyright 2025 Google LLC
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

#include "checker/internal/format_type_name.h"

#include "common/type.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto2/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {
namespace {

using ::cel::expr::conformance::proto2::GlobalEnum_descriptor;
using ::cel::expr::conformance::proto2::TestAllTypes;
using ::testing::MatchesRegex;

TEST(FormatTypeNameTest, PrimitiveTypes) {
  EXPECT_EQ(FormatTypeName(IntType()), "int");
  EXPECT_EQ(FormatTypeName(UintType()), "uint");
  EXPECT_EQ(FormatTypeName(DoubleType()), "double");
  EXPECT_EQ(FormatTypeName(StringType()), "string");
  EXPECT_EQ(FormatTypeName(BytesType()), "bytes");
  EXPECT_EQ(FormatTypeName(BoolType()), "bool");
  EXPECT_EQ(FormatTypeName(NullType()), "null_type");
  EXPECT_EQ(FormatTypeName(DynType()), "dyn");
}

TEST(FormatTypeNameTest, SpecialTypes) {
  EXPECT_EQ(FormatTypeName(ErrorType()), "*error*");
  EXPECT_EQ(FormatTypeName(UnknownType()), "*unknown*");
  EXPECT_EQ(FormatTypeName(FunctionType()), "*error*");
}

TEST(FormatTypeNameTest, WellKnownTypes) {
  EXPECT_EQ(FormatTypeName(AnyType()), "any");
  EXPECT_EQ(FormatTypeName(DurationType()), "google.protobuf.Duration");
  EXPECT_EQ(FormatTypeName(TimestampType()), "google.protobuf.Timestamp");
}

TEST(FormatTypeNameTest, Wrappers) {
  EXPECT_EQ(FormatTypeName(IntWrapperType()), "wrapper(int)");
  EXPECT_EQ(FormatTypeName(UintWrapperType()), "wrapper(uint)");
  EXPECT_EQ(FormatTypeName(DoubleWrapperType()), "wrapper(double)");
  EXPECT_EQ(FormatTypeName(StringWrapperType()), "wrapper(string)");
  EXPECT_EQ(FormatTypeName(BytesWrapperType()), "wrapper(bytes)");
  EXPECT_EQ(FormatTypeName(BoolWrapperType()), "wrapper(bool)");
}

TEST(FormatTypeNameTest, ProtobufTypes) {
  EXPECT_EQ(FormatTypeName(MessageType(TestAllTypes::descriptor())),
            "cel.expr.conformance.proto2.TestAllTypes");
  EXPECT_EQ(FormatTypeName(EnumType(GlobalEnum_descriptor())), "int");
}

TEST(FormatTypeNameTest, Type) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FormatTypeName(TypeType()), "type");
  EXPECT_EQ(FormatTypeName(TypeType(&arena, IntType())), "type(int)");
  EXPECT_EQ(FormatTypeName(TypeType(&arena, TypeType(&arena, IntType()))),
            "type(type(int))");
  EXPECT_EQ(FormatTypeName(TypeType(&arena, TypeParamType("T"))), "type(T)");
}

TEST(FormatTypeNameTest, List) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FormatTypeName(ListType()), "list(dyn)");
  EXPECT_EQ(FormatTypeName(ListType(&arena, IntType())), "list(int)");
  EXPECT_EQ(FormatTypeName(ListType(&arena, ListType(&arena, IntType()))),
            "list(list(int))");
}

TEST(FormatTypeNameTest, Map) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FormatTypeName(MapType()), "map(dyn, dyn)");
  EXPECT_EQ(FormatTypeName(MapType(&arena, IntType(), IntType())),
            "map(int, int)");
  EXPECT_EQ(FormatTypeName(MapType(&arena, IntType(),
                                   MapType(&arena, IntType(), IntType()))),
            "map(int, map(int, int))");
}

TEST(FormatTypeNameTest, Opaque) {
  google::protobuf::Arena arena;
  EXPECT_EQ(FormatTypeName(OpaqueType(&arena, "opaque", {})), "opaque");
  Type two_tuple_type = OpaqueType(&arena, "tuple", {IntType(), IntType()});
  Type three_tuple_type = OpaqueType(
      &arena, "tuple", {two_tuple_type, two_tuple_type, two_tuple_type});
  EXPECT_EQ(FormatTypeName(three_tuple_type),
            "tuple(tuple(int, int), tuple(int, int), tuple(int, int))");
}

TEST(FormatTypeNameTest, ArbitraryNesting) {
  google::protobuf::Arena arena;
  Type type = IntType();
  for (int i = 0; i < 1000; ++i) {
    type = OpaqueType(&arena, "ptype", {type});
  }

  EXPECT_THAT(FormatTypeName(type),
              MatchesRegex(R"(^(ptype\(){1000}int(\)){1000})"));
}

}  // namespace
}  // namespace cel::checker_internal
