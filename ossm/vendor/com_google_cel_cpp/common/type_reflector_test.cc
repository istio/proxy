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

#include <cstdint>
#include <limits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/time/time.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "common/values/list_value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;

using TypeReflectorTest = common_internal::ThreadCompatibleValueTest<>;

#define TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(element_type)           \
  TEST_P(TypeReflectorTest, NewListValueBuilder_##element_type) {          \
    ASSERT_OK_AND_ASSIGN(auto list_value_builder,                          \
                         value_manager().NewListValueBuilder(ListType())); \
    EXPECT_TRUE(list_value_builder->IsEmpty());                            \
    EXPECT_EQ(list_value_builder->Size(), 0);                              \
    auto list_value = std::move(*list_value_builder).Build();              \
    EXPECT_THAT(list_value.IsEmpty(), IsOkAndHolds(true));                 \
    EXPECT_THAT(list_value.Size(), IsOkAndHolds(0));                       \
    EXPECT_EQ(list_value.DebugString(), "[]");                             \
  }

TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(BoolType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(BytesType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(DoubleType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(DurationType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(IntType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(ListType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(MapType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(NullType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(OptionalType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(StringType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(TimestampType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(TypeType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(UintType)
TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST(DynType)

#undef TYPE_REFLECTOR_NEW_LIST_VALUE_BUILDER_TEST

#define TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(key_type, value_type)     \
  TEST_P(TypeReflectorTest, NewMapValueBuilder_##key_type##_##value_type) { \
    ASSERT_OK_AND_ASSIGN(auto map_value_builder,                            \
                         value_manager().NewMapValueBuilder(MapType()));    \
    EXPECT_TRUE(map_value_builder->IsEmpty());                              \
    EXPECT_EQ(map_value_builder->Size(), 0);                                \
    auto map_value = std::move(*map_value_builder).Build();                 \
    EXPECT_THAT(map_value.IsEmpty(), IsOkAndHolds(true));                   \
    EXPECT_THAT(map_value.Size(), IsOkAndHolds(0));                         \
    EXPECT_EQ(map_value.DebugString(), "{}");                               \
  }

TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, BoolType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, BytesType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, DoubleType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, DurationType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, IntType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, ListType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, MapType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, NullType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, OptionalType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, StringType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, TimestampType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, TypeType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, UintType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(BoolType, DynType)

TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, BoolType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, BytesType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, DoubleType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, DurationType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, IntType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, ListType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, MapType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, NullType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, OptionalType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, StringType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, TimestampType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, TypeType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, UintType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(IntType, DynType)

TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, BoolType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, BytesType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, DoubleType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, DurationType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, IntType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, ListType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, MapType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, NullType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, OptionalType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, StringType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, TimestampType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, TypeType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, UintType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(UintType, DynType)

TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, BoolType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, BytesType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, DoubleType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, DurationType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, IntType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, ListType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, MapType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, NullType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, OptionalType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, StringType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, TimestampType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, TypeType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, UintType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(StringType, DynType)

TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, BoolType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, BytesType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, DoubleType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, DurationType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, IntType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, ListType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, MapType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, NullType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, OptionalType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, StringType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, TimestampType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, TypeType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, UintType)
TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST(DynType, DynType)

#undef TYPE_REFLECTOR_NEW_MAP_VALUE_BUILDER_TEST

TEST_P(TypeReflectorTest, NewListValueBuilderCoverage_Dynamic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewListValueBuilder(cel::ListType()));
  EXPECT_OK(builder->Add(IntValue(0)));
  EXPECT_OK(builder->Add(IntValue(1)));
  EXPECT_OK(builder->Add(IntValue(2)));
  EXPECT_EQ(builder->Size(), 3);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(value.DebugString(), "[0, 1, 2]");
}

TEST_P(TypeReflectorTest, NewMapValueBuilderCoverage_DynamicDynamic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewMapValueBuilder(MapType()));
  EXPECT_OK(builder->Put(BoolValue(false), IntValue(1)));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(2)));
  EXPECT_OK(builder->Put(IntValue(0), IntValue(3)));
  EXPECT_OK(builder->Put(IntValue(1), IntValue(4)));
  EXPECT_OK(builder->Put(UintValue(0), IntValue(5)));
  EXPECT_OK(builder->Put(UintValue(1), IntValue(6)));
  EXPECT_OK(builder->Put(StringValue("a"), IntValue(7)));
  EXPECT_OK(builder->Put(StringValue("b"), IntValue(8)));
  EXPECT_EQ(builder->Size(), 8);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_THAT(value.DebugString(), Not(IsEmpty()));
}

TEST_P(TypeReflectorTest, NewMapValueBuilderCoverage_StaticDynamic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewMapValueBuilder(MapType()));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(0)));
  EXPECT_EQ(builder->Size(), 1);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(value.DebugString(), "{true: 0}");
}

TEST_P(TypeReflectorTest, NewMapValueBuilderCoverage_DynamicStatic) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewMapValueBuilder(MapType()));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(0)));
  EXPECT_EQ(builder->Size(), 1);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_EQ(value.DebugString(), "{true: 0}");
}

TEST_P(TypeReflectorTest, JsonKeyCoverage) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewMapValueBuilder(
                                         MapType(cel::MapType())));
  EXPECT_OK(builder->Put(BoolValue(true), IntValue(1)));
  EXPECT_OK(builder->Put(IntValue(1), IntValue(2)));
  EXPECT_OK(builder->Put(UintValue(2), IntValue(3)));
  EXPECT_OK(builder->Put(StringValue("a"), IntValue(4)));
  auto value = std::move(*builder).Build();
  EXPECT_THAT(value.ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(TypeReflectorTest, NewValueBuilder_BoolValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.BoolValue"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", BoolValue(true)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", IntValue(1)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<BoolValue>(value));
  EXPECT_EQ(Cast<BoolValue>(value).NativeValue(), true);
}

TEST_P(TypeReflectorTest, NewValueBuilder_Int32Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Int32Value"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName(
                  "value", IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(
                  1, IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<IntValue>(value));
  EXPECT_EQ(Cast<IntValue>(value).NativeValue(), 1);
}

TEST_P(TypeReflectorTest, NewValueBuilder_Int64Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Int64Value"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<IntValue>(value));
  EXPECT_EQ(Cast<IntValue>(value).NativeValue(), 1);
}

TEST_P(TypeReflectorTest, NewValueBuilder_UInt32Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.UInt32Value"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName(
                  "value", UintValue(std::numeric_limits<uint64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(1, UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(
                  1, UintValue(std::numeric_limits<uint64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<UintValue>(value));
  EXPECT_EQ(Cast<UintValue>(value).NativeValue(), 1);
}

TEST_P(TypeReflectorTest, NewValueBuilder_UInt64Value) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.UInt64Value"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, UintValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, UintValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<UintValue>(value));
  EXPECT_EQ(Cast<UintValue>(value).NativeValue(), 1);
}

TEST_P(TypeReflectorTest, NewValueBuilder_FloatValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.FloatValue"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<DoubleValue>(value));
  EXPECT_EQ(Cast<DoubleValue>(value).NativeValue(), 1);
}

TEST_P(TypeReflectorTest, NewValueBuilder_DoubleValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.DoubleValue"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, DoubleValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, DoubleValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<DoubleValue>(value));
  EXPECT_EQ(Cast<DoubleValue>(value).NativeValue(), 1);
}

TEST_P(TypeReflectorTest, NewValueBuilder_StringValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.StringValue"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", StringValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", StringValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, StringValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, StringValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<StringValue>(value));
  EXPECT_EQ(Cast<StringValue>(value).NativeString(), "foo");
}

TEST_P(TypeReflectorTest, NewValueBuilder_BytesValue) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.BytesValue"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("value", BytesValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", BytesValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, BytesValue("foo")), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, BytesValue("foo")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<BytesValue>(value));
  EXPECT_EQ(Cast<BytesValue>(value).NativeString(), "foo");
}

TEST_P(TypeReflectorTest, NewValueBuilder_Duration) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Duration"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("seconds", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("seconds", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("nanos", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName(
                  "nanos", IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByName("nanos", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(3, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(
                  2, IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<DurationValue>(value));
  EXPECT_EQ(Cast<DurationValue>(value).NativeValue(),
            absl::Seconds(1) + absl::Nanoseconds(1));
}

TEST_P(TypeReflectorTest, NewValueBuilder_Timestamp) {
  ASSERT_OK_AND_ASSIGN(auto builder, value_manager().NewValueBuilder(
                                         "google.protobuf.Timestamp"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName("seconds", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("seconds", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("nanos", IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByName(
                  "nanos", IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByName("nanos", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(1, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(3, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(2, IntValue(1)), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(
                  2, IntValue(std::numeric_limits<int64_t>::max())),
              StatusIs(absl::StatusCode::kOutOfRange));
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<TimestampValue>(value));
  EXPECT_EQ(Cast<TimestampValue>(value).NativeValue(),
            absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1));
}

TEST_P(TypeReflectorTest, NewValueBuilder_Any) {
  ASSERT_OK_AND_ASSIGN(auto builder,
                       value_manager().NewValueBuilder("google.protobuf.Any"));
  ASSERT_THAT(builder, NotNull());
  EXPECT_THAT(builder->SetFieldByName(
                  "type_url",
                  StringValue("type.googleapis.com/google.protobuf.BoolValue")),
              IsOk());
  EXPECT_THAT(builder->SetFieldByName("does_not_exist", IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByName("type_url", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByName("value", BytesValue()), IsOk());
  EXPECT_THAT(builder->SetFieldByName("value", BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(
      builder->SetFieldByNumber(
          1, StringValue("type.googleapis.com/google.protobuf.BoolValue")),
      IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(3, IntValue(1)),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(builder->SetFieldByNumber(1, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(builder->SetFieldByNumber(2, BytesValue()), IsOk());
  EXPECT_THAT(builder->SetFieldByNumber(2, BoolValue(true)),
              StatusIs(absl::StatusCode::kInvalidArgument));
  auto value = std::move(*builder).Build();
  EXPECT_TRUE(InstanceOf<BoolValue>(value));
  EXPECT_EQ(Cast<BoolValue>(value).NativeValue(), false);
}

INSTANTIATE_TEST_SUITE_P(
    TypeReflectorTest, TypeReflectorTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    TypeReflectorTest::ToString);

}  // namespace
}  // namespace cel
