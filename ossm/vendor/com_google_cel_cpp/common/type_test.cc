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

#include "common/type.h"

#include "absl/hash/hash.h"
#include "absl/hash/hash_testing.h"
#include "absl/log/die_if_null.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::cel::internal::GetTestingDescriptorPool;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;

TEST(Type, Default) {
  EXPECT_EQ(Type(), DynType());
  EXPECT_TRUE(Type().IsDyn());
}

TEST(Type, Enum) {
  EXPECT_EQ(
      Type::Enum(
          ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
              "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))),
      EnumType(ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
          "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))));
  EXPECT_EQ(Type::Enum(
                ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                    "google.protobuf.NullValue"))),
            NullType());
}

TEST(Type, Field) {
  google::protobuf::Arena arena;
  const auto* descriptor =
      ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindMessageTypeByName(
          "google.api.expr.test.v1.proto3.TestAllTypes"));
  EXPECT_EQ(
      Type::Field(ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_bool"))),
      BoolType());
  EXPECT_EQ(
      Type::Field(ABSL_DIE_IF_NULL(descriptor->FindFieldByName("null_value"))),
      NullType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_int32"))),
            IntType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_sint32"))),
            IntType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_sfixed32"))),
            IntType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_int64"))),
            IntType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_sint64"))),
            IntType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_sfixed64"))),
            IntType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_fixed32"))),
            UintType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_uint32"))),
            UintType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_fixed64"))),
            UintType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_uint64"))),
            UintType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_float"))),
            DoubleType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_double"))),
            DoubleType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_bytes"))),
            BytesType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_string"))),
            StringType());
  EXPECT_EQ(
      Type::Field(ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_any"))),
      AnyType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_duration"))),
            DurationType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_timestamp"))),
            TimestampType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_struct"))),
            JsonMapType());
  EXPECT_EQ(
      Type::Field(ABSL_DIE_IF_NULL(descriptor->FindFieldByName("list_value"))),
      JsonListType());
  EXPECT_EQ(Type::Field(
                ABSL_DIE_IF_NULL(descriptor->FindFieldByName("single_value"))),
            JsonType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_bool_wrapper"))),
            BoolWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_int32_wrapper"))),
            IntWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_int64_wrapper"))),
            IntWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_uint32_wrapper"))),
            UintWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_uint64_wrapper"))),
            UintWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_float_wrapper"))),
            DoubleWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_double_wrapper"))),
            DoubleWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_bytes_wrapper"))),
            BytesWrapperType());
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("single_string_wrapper"))),
            StringWrapperType());
  EXPECT_EQ(
      Type::Field(
          ABSL_DIE_IF_NULL(descriptor->FindFieldByName("standalone_enum"))),
      EnumType(ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
          "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))));
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("repeated_int32"))),
            ListType(&arena, IntType()));
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                descriptor->FindFieldByName("map_int32_int32"))),
            MapType(&arena, IntType(), IntType()));
}

TEST(Type, Kind) {
  google::protobuf::Arena arena;

  EXPECT_EQ(Type(AnyType()).kind(), AnyType::kKind);

  EXPECT_EQ(Type(BoolType()).kind(), BoolType::kKind);

  EXPECT_EQ(Type(BoolWrapperType()).kind(), BoolWrapperType::kKind);

  EXPECT_EQ(Type(BytesType()).kind(), BytesType::kKind);

  EXPECT_EQ(Type(BytesWrapperType()).kind(), BytesWrapperType::kKind);

  EXPECT_EQ(Type(DoubleType()).kind(), DoubleType::kKind);

  EXPECT_EQ(Type(DoubleWrapperType()).kind(), DoubleWrapperType::kKind);

  EXPECT_EQ(Type(DurationType()).kind(), DurationType::kKind);

  EXPECT_EQ(Type(DynType()).kind(), DynType::kKind);

  EXPECT_EQ(
      Type(EnumType(
               ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                   "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))
          .kind(),
      EnumType::kKind);

  EXPECT_EQ(Type(ErrorType()).kind(), ErrorType::kKind);

  EXPECT_EQ(Type(FunctionType(&arena, DynType(), {})).kind(),
            FunctionType::kKind);

  EXPECT_EQ(Type(IntType()).kind(), IntType::kKind);

  EXPECT_EQ(Type(IntWrapperType()).kind(), IntWrapperType::kKind);

  EXPECT_EQ(Type(ListType()).kind(), ListType::kKind);

  EXPECT_EQ(Type(MapType()).kind(), MapType::kKind);

  EXPECT_EQ(Type(MessageType(ABSL_DIE_IF_NULL(
                     GetTestingDescriptorPool()->FindMessageTypeByName(
                         "google.api.expr.test.v1.proto3.TestAllTypes"))))
                .kind(),
            MessageType::kKind);
  EXPECT_EQ(Type(MessageType(ABSL_DIE_IF_NULL(
                     GetTestingDescriptorPool()->FindMessageTypeByName(
                         "google.api.expr.test.v1.proto3.TestAllTypes"))))
                .kind(),
            MessageType::kKind);

  EXPECT_EQ(Type(NullType()).kind(), NullType::kKind);

  EXPECT_EQ(Type(OptionalType()).kind(), OpaqueType::kKind);

  EXPECT_EQ(Type(StringType()).kind(), StringType::kKind);

  EXPECT_EQ(Type(StringWrapperType()).kind(), StringWrapperType::kKind);

  EXPECT_EQ(Type(TimestampType()).kind(), TimestampType::kKind);

  EXPECT_EQ(Type(UintType()).kind(), UintType::kKind);

  EXPECT_EQ(Type(UintWrapperType()).kind(), UintWrapperType::kKind);

  EXPECT_EQ(Type(UnknownType()).kind(), UnknownType::kKind);
}

TEST(Type, GetParameters) {
  google::protobuf::Arena arena;

  EXPECT_THAT(Type(AnyType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(BoolType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(BoolWrapperType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(BytesType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(BytesWrapperType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(DoubleType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(DoubleWrapperType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(DurationType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(DynType()).GetParameters(), IsEmpty());

  EXPECT_THAT(
      Type(EnumType(
               ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                   "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))
          .GetParameters(),
      IsEmpty());

  EXPECT_THAT(Type(ErrorType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(FunctionType(&arena, DynType(),
                                {IntType(), StringType(), DynType()}))
                  .GetParameters(),
              ElementsAre(DynType(), IntType(), StringType(), DynType()));

  EXPECT_THAT(Type(IntType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(IntWrapperType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(ListType()).GetParameters(), ElementsAre(DynType()));

  EXPECT_THAT(Type(MapType()).GetParameters(),
              ElementsAre(DynType(), DynType()));

  EXPECT_THAT(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .GetParameters(),
              IsEmpty());

  EXPECT_THAT(Type(NullType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(OptionalType()).GetParameters(), ElementsAre(DynType()));

  EXPECT_THAT(Type(StringType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(StringWrapperType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(TimestampType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(UintType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(UintWrapperType()).GetParameters(), IsEmpty());

  EXPECT_THAT(Type(UnknownType()).GetParameters(), IsEmpty());
}

TEST(Type, Is) {
  google::protobuf::Arena arena;

  EXPECT_TRUE(Type(AnyType()).Is<AnyType>());

  EXPECT_TRUE(Type(BoolType()).Is<BoolType>());

  EXPECT_TRUE(Type(BoolWrapperType()).Is<BoolWrapperType>());
  EXPECT_TRUE(Type(BoolWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(BytesType()).Is<BytesType>());

  EXPECT_TRUE(Type(BytesWrapperType()).Is<BytesWrapperType>());
  EXPECT_TRUE(Type(BytesWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(DoubleType()).Is<DoubleType>());

  EXPECT_TRUE(Type(DoubleWrapperType()).Is<DoubleWrapperType>());
  EXPECT_TRUE(Type(DoubleWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(DurationType()).Is<DurationType>());

  EXPECT_TRUE(Type(DynType()).Is<DynType>());

  EXPECT_TRUE(
      Type(EnumType(
               ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                   "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))
          .Is<EnumType>());

  EXPECT_TRUE(Type(ErrorType()).Is<ErrorType>());

  EXPECT_TRUE(Type(FunctionType(&arena, DynType(), {})).Is<FunctionType>());

  EXPECT_TRUE(Type(IntType()).Is<IntType>());

  EXPECT_TRUE(Type(IntWrapperType()).Is<IntWrapperType>());
  EXPECT_TRUE(Type(IntWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(ListType()).Is<ListType>());

  EXPECT_TRUE(Type(MapType()).Is<MapType>());

  EXPECT_TRUE(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .IsStruct());
  EXPECT_TRUE(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .IsMessage());

  EXPECT_TRUE(Type(NullType()).Is<NullType>());

  EXPECT_TRUE(Type(OptionalType()).Is<OpaqueType>());
  EXPECT_TRUE(Type(OptionalType()).Is<OptionalType>());

  EXPECT_TRUE(Type(StringType()).Is<StringType>());

  EXPECT_TRUE(Type(StringWrapperType()).Is<StringWrapperType>());
  EXPECT_TRUE(Type(StringWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(TimestampType()).Is<TimestampType>());

  EXPECT_TRUE(Type(TypeType()).Is<TypeType>());

  EXPECT_TRUE(Type(TypeParamType("T")).Is<TypeParamType>());

  EXPECT_TRUE(Type(UintType()).Is<UintType>());

  EXPECT_TRUE(Type(UintWrapperType()).Is<UintWrapperType>());
  EXPECT_TRUE(Type(UintWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(UnknownType()).Is<UnknownType>());
}

TEST(Type, As) {
  google::protobuf::Arena arena;

  EXPECT_THAT(Type(AnyType()).As<AnyType>(), Optional(An<AnyType>()));

  EXPECT_THAT(Type(BoolType()).As<BoolType>(), Optional(An<BoolType>()));

  EXPECT_THAT(Type(BoolWrapperType()).As<BoolWrapperType>(),
              Optional(An<BoolWrapperType>()));

  EXPECT_THAT(Type(BytesType()).As<BytesType>(), Optional(An<BytesType>()));

  EXPECT_THAT(Type(BytesWrapperType()).As<BytesWrapperType>(),
              Optional(An<BytesWrapperType>()));

  EXPECT_THAT(Type(DoubleType()).As<DoubleType>(), Optional(An<DoubleType>()));

  EXPECT_THAT(Type(DoubleWrapperType()).As<DoubleWrapperType>(),
              Optional(An<DoubleWrapperType>()));

  EXPECT_THAT(Type(DurationType()).As<DurationType>(),
              Optional(An<DurationType>()));

  EXPECT_THAT(Type(DynType()).As<DynType>(), Optional(An<DynType>()));

  EXPECT_THAT(
      Type(EnumType(
               ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                   "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))
          .As<EnumType>(),
      Optional(An<EnumType>()));

  EXPECT_THAT(Type(ErrorType()).As<ErrorType>(), Optional(An<ErrorType>()));

  EXPECT_TRUE(Type(FunctionType(&arena, DynType(), {})).Is<FunctionType>());

  EXPECT_THAT(Type(IntType()).As<IntType>(), Optional(An<IntType>()));

  EXPECT_THAT(Type(IntWrapperType()).As<IntWrapperType>(),
              Optional(An<IntWrapperType>()));

  EXPECT_THAT(Type(ListType()).As<ListType>(), Optional(An<ListType>()));

  EXPECT_THAT(Type(MapType()).As<MapType>(), Optional(An<MapType>()));

  EXPECT_THAT(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .As<StructType>(),
              Optional(An<StructType>()));
  EXPECT_THAT(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .As<MessageType>(),
              Optional(An<MessageType>()));

  EXPECT_THAT(Type(NullType()).As<NullType>(), Optional(An<NullType>()));

  EXPECT_THAT(Type(OptionalType()).As<OptionalType>(),
              Optional(An<OptionalType>()));
  EXPECT_THAT(Type(OptionalType()).As<OptionalType>(),
              Optional(An<OptionalType>()));

  EXPECT_THAT(Type(StringType()).As<StringType>(), Optional(An<StringType>()));

  EXPECT_THAT(Type(StringWrapperType()).As<StringWrapperType>(),
              Optional(An<StringWrapperType>()));

  EXPECT_THAT(Type(TimestampType()).As<TimestampType>(),
              Optional(An<TimestampType>()));

  EXPECT_THAT(Type(TypeType()).As<TypeType>(), Optional(An<TypeType>()));

  EXPECT_THAT(Type(TypeParamType("T")).As<TypeParamType>(),
              Optional(An<TypeParamType>()));

  EXPECT_THAT(Type(UintType()).As<UintType>(), Optional(An<UintType>()));

  EXPECT_THAT(Type(UintWrapperType()).As<UintWrapperType>(),
              Optional(An<UintWrapperType>()));

  EXPECT_THAT(Type(UnknownType()).As<UnknownType>(),
              Optional(An<UnknownType>()));
}

template <typename T>
T DoGet(const Type& type) {
  return type.template Get<T>();
}

TEST(Type, Get) {
  google::protobuf::Arena arena;

  EXPECT_THAT(DoGet<AnyType>(Type(AnyType())), An<AnyType>());

  EXPECT_THAT(DoGet<BoolType>(Type(BoolType())), An<BoolType>());

  EXPECT_THAT(DoGet<BoolWrapperType>(Type(BoolWrapperType())),
              An<BoolWrapperType>());
  EXPECT_THAT(DoGet<BoolWrapperType>(Type(BoolWrapperType())),
              An<BoolWrapperType>());

  EXPECT_THAT(DoGet<BytesType>(Type(BytesType())), An<BytesType>());

  EXPECT_THAT(DoGet<BytesWrapperType>(Type(BytesWrapperType())),
              An<BytesWrapperType>());
  EXPECT_THAT(DoGet<BytesWrapperType>(Type(BytesWrapperType())),
              An<BytesWrapperType>());

  EXPECT_THAT(DoGet<DoubleType>(Type(DoubleType())), An<DoubleType>());

  EXPECT_THAT(DoGet<DoubleWrapperType>(Type(DoubleWrapperType())),
              An<DoubleWrapperType>());
  EXPECT_THAT(DoGet<DoubleWrapperType>(Type(DoubleWrapperType())),
              An<DoubleWrapperType>());

  EXPECT_THAT(DoGet<DurationType>(Type(DurationType())), An<DurationType>());

  EXPECT_THAT(DoGet<DynType>(Type(DynType())), An<DynType>());

  EXPECT_THAT(
      DoGet<EnumType>(Type(EnumType(
          ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
              "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))),
      An<EnumType>());

  EXPECT_THAT(DoGet<ErrorType>(Type(ErrorType())), An<ErrorType>());

  EXPECT_THAT(DoGet<FunctionType>(Type(FunctionType(&arena, DynType(), {}))),
              An<FunctionType>());

  EXPECT_THAT(DoGet<IntType>(Type(IntType())), An<IntType>());

  EXPECT_THAT(DoGet<IntWrapperType>(Type(IntWrapperType())),
              An<IntWrapperType>());
  EXPECT_THAT(DoGet<IntWrapperType>(Type(IntWrapperType())),
              An<IntWrapperType>());

  EXPECT_THAT(DoGet<ListType>(Type(ListType())), An<ListType>());

  EXPECT_THAT(DoGet<MapType>(Type(MapType())), An<MapType>());

  EXPECT_THAT(DoGet<StructType>(Type(MessageType(ABSL_DIE_IF_NULL(
                  GetTestingDescriptorPool()->FindMessageTypeByName(
                      "google.api.expr.test.v1.proto3.TestAllTypes"))))),
              An<StructType>());
  EXPECT_THAT(DoGet<MessageType>(Type(MessageType(ABSL_DIE_IF_NULL(
                  GetTestingDescriptorPool()->FindMessageTypeByName(
                      "google.api.expr.test.v1.proto3.TestAllTypes"))))),
              An<MessageType>());

  EXPECT_THAT(DoGet<NullType>(Type(NullType())), An<NullType>());

  EXPECT_THAT(DoGet<OptionalType>(Type(OptionalType())), An<OptionalType>());
  EXPECT_THAT(DoGet<OptionalType>(Type(OptionalType())), An<OptionalType>());

  EXPECT_THAT(DoGet<StringType>(Type(StringType())), An<StringType>());

  EXPECT_THAT(DoGet<StringWrapperType>(Type(StringWrapperType())),
              An<StringWrapperType>());
  EXPECT_THAT(DoGet<StringWrapperType>(Type(StringWrapperType())),
              An<StringWrapperType>());

  EXPECT_THAT(DoGet<TimestampType>(Type(TimestampType())), An<TimestampType>());

  EXPECT_THAT(DoGet<TypeType>(Type(TypeType())), An<TypeType>());

  EXPECT_THAT(DoGet<TypeParamType>(Type(TypeParamType("T"))),
              An<TypeParamType>());

  EXPECT_THAT(DoGet<UintType>(Type(UintType())), An<UintType>());

  EXPECT_THAT(DoGet<UintWrapperType>(Type(UintWrapperType())),
              An<UintWrapperType>());
  EXPECT_THAT(DoGet<UintWrapperType>(Type(UintWrapperType())),
              An<UintWrapperType>());

  EXPECT_THAT(DoGet<UnknownType>(Type(UnknownType())), An<UnknownType>());
}

TEST(Type, VerifyTypeImplementsAbslHashCorrectly) {
  google::protobuf::Arena arena;

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Type(AnyType()),
       Type(BoolType()),
       Type(BoolWrapperType()),
       Type(BytesType()),
       Type(BytesWrapperType()),
       Type(DoubleType()),
       Type(DoubleWrapperType()),
       Type(DurationType()),
       Type(DynType()),
       Type(ErrorType()),
       Type(FunctionType(&arena, DynType(), {DynType()})),
       Type(IntType()),
       Type(IntWrapperType()),
       Type(ListType(&arena, DynType())),
       Type(MapType(&arena, DynType(), DynType())),
       Type(NullType()),
       Type(OptionalType(&arena, DynType())),
       Type(StringType()),
       Type(StringWrapperType()),
       Type(StructType(common_internal::MakeBasicStructType("test.Struct"))),
       Type(TimestampType()),
       Type(TypeParamType("T")),
       Type(TypeType()),
       Type(UintType()),
       Type(UintWrapperType()),
       Type(UnknownType())}));

  EXPECT_EQ(
      absl::HashOf(Type::Field(
          ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindMessageTypeByName(
                               "google.api.expr.test.v1.proto3.TestAllTypes"))
              ->FindFieldByName("repeated_int64"))),
      absl::HashOf(Type(ListType(&arena, IntType()))));
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                            GetTestingDescriptorPool()->FindMessageTypeByName(
                                "google.api.expr.test.v1.proto3.TestAllTypes"))
                            ->FindFieldByName("repeated_int64")),
            Type(ListType(&arena, IntType())));

  EXPECT_EQ(
      absl::HashOf(Type::Field(
          ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindMessageTypeByName(
                               "google.api.expr.test.v1.proto3.TestAllTypes"))
              ->FindFieldByName("map_int64_int64"))),
      absl::HashOf(Type(MapType(&arena, IntType(), IntType()))));
  EXPECT_EQ(Type::Field(ABSL_DIE_IF_NULL(
                            GetTestingDescriptorPool()->FindMessageTypeByName(
                                "google.api.expr.test.v1.proto3.TestAllTypes"))
                            ->FindFieldByName("map_int64_int64")),
            Type(MapType(&arena, IntType(), IntType())));

  EXPECT_EQ(absl::HashOf(Type(MessageType(ABSL_DIE_IF_NULL(
                GetTestingDescriptorPool()->FindMessageTypeByName(
                    "google.api.expr.test.v1.proto3.TestAllTypes"))))),
            absl::HashOf(Type(StructType(common_internal::MakeBasicStructType(
                "google.api.expr.test.v1.proto3.TestAllTypes")))));
  EXPECT_EQ(Type(MessageType(ABSL_DIE_IF_NULL(
                GetTestingDescriptorPool()->FindMessageTypeByName(
                    "google.api.expr.test.v1.proto3.TestAllTypes")))),
            Type(StructType(common_internal::MakeBasicStructType(
                "google.api.expr.test.v1.proto3.TestAllTypes"))));
}

TEST(Type, Unwrap) {
  EXPECT_EQ(Type(BoolWrapperType()).Unwrap(), BoolType());
  EXPECT_EQ(Type(IntWrapperType()).Unwrap(), IntType());
  EXPECT_EQ(Type(UintWrapperType()).Unwrap(), UintType());
  EXPECT_EQ(Type(DoubleWrapperType()).Unwrap(), DoubleType());
  EXPECT_EQ(Type(BytesWrapperType()).Unwrap(), BytesType());
  EXPECT_EQ(Type(StringWrapperType()).Unwrap(), StringType());
  EXPECT_EQ(Type(AnyType()).Unwrap(), AnyType());
}

TEST(Type, Wrap) {
  EXPECT_EQ(Type(BoolType()).Wrap(), BoolWrapperType());
  EXPECT_EQ(Type(IntType()).Wrap(), IntWrapperType());
  EXPECT_EQ(Type(UintType()).Wrap(), UintWrapperType());
  EXPECT_EQ(Type(DoubleType()).Wrap(), DoubleWrapperType());
  EXPECT_EQ(Type(BytesType()).Wrap(), BytesWrapperType());
  EXPECT_EQ(Type(StringType()).Wrap(), StringWrapperType());
  EXPECT_EQ(Type(AnyType()).Wrap(), AnyType());
}

}  // namespace
}  // namespace cel
