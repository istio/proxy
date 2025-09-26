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

#include <cstddef>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::BytesValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::DurationValueIs;
using ::cel::test::ErrorValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::UintValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using ParsedRepeatedFieldValueTest = common_internal::ValueTest<>;

TEST_F(ParsedRepeatedFieldValueTest, Field) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_TRUE(value);
}

TEST_F(ParsedRepeatedFieldValueTest, Kind) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_EQ(value.kind(), ParsedRepeatedFieldValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kList);
}

TEST_F(ParsedRepeatedFieldValueTest, GetTypeName) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_EQ(value.GetTypeName(), ParsedRepeatedFieldValue::kName);
  EXPECT_EQ(value.GetTypeName(), "list");
}

TEST_F(ParsedRepeatedFieldValueTest, GetRuntimeType) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_EQ(value.GetRuntimeType(), ListType());
}

TEST_F(ParsedRepeatedFieldValueTest, DebugString) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_THAT(value.DebugString(), _);
}

TEST_F(ParsedRepeatedFieldValueTest, IsZeroValue) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_TRUE(value.IsZeroValue());
}

TEST_F(ParsedRepeatedFieldValueTest, SerializeTo) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(value.SerializeTo(descriptor_pool(), message_factory(), &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), IsEmpty());
}

TEST_F(ParsedRepeatedFieldValueTest, ConvertToJson) {
  auto json = DynamicParseTextProto<google::protobuf::Value>(R"pb()pb");
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_THAT(value.ConvertToJson(descriptor_pool(), message_factory(),
                                  cel::to_address(json)),
              IsOk());
  EXPECT_THAT(
      *json, EqualsTextProto<google::protobuf::Value>(R"pb(list_value: {})pb"));
}

TEST_F(ParsedRepeatedFieldValueTest, Equal_RepeatedField) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_THAT(
      value.Equal(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      value.Equal(
          ParsedRepeatedFieldValue(
              DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
              DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena()),
          descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      value.Equal(ListValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedRepeatedFieldValueTest, Equal_JsonList) {
  ParsedRepeatedFieldValue repeated_value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_int64: 1
                                                     repeated_int64: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  ParsedJsonListValue json_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(
            values { number_value: 1 }
            values { number_value: 0 }
          )pb"),
      arena());
  EXPECT_THAT(repeated_value.Equal(json_value, descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(json_value.Equal(repeated_value, descriptor_pool(),
                               message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedRepeatedFieldValueTest, Empty) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_TRUE(value.IsEmpty());
}

TEST_F(ParsedRepeatedFieldValueTest, Size) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int64"), arena());
  EXPECT_EQ(value.Size(), 0);
}

TEST_F(ParsedRepeatedFieldValueTest, Get) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"), arena());
  EXPECT_THAT(value.Get(0, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Get(1, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      value.Get(2, descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Bool) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"), arena());
  {
    std::vector<Value> values;
    EXPECT_THAT(value.ForEach(
                    [&](const Value& element) -> absl::StatusOr<bool> {
                      values.push_back(element);
                      return true;
                    },
                    descriptor_pool(), message_factory(), arena()),
                IsOk());
    EXPECT_THAT(values, ElementsAre(BoolValueIs(false), BoolValueIs(true)));
  }
  {
    std::vector<Value> values;
    EXPECT_THAT(value.ForEach(
                    [&](size_t, const Value& element) -> absl::StatusOr<bool> {
                      values.push_back(element);
                      return true;
                    },
                    descriptor_pool(), message_factory(), arena()),
                IsOk());
    EXPECT_THAT(values, ElementsAre(BoolValueIs(false), BoolValueIs(true)));
  }
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Double) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_double: 1
                                                     repeated_double: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_double"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(DoubleValueIs(1), DoubleValueIs(0)));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Float) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_float: 1
                                                     repeated_float: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_float"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(DoubleValueIs(1), DoubleValueIs(0)));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_UInt64) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_uint64: 1
                                                     repeated_uint64: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_uint64"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(UintValueIs(1), UintValueIs(0)));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Int32) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_int32: 1
                                                     repeated_int32: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_int32"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(IntValueIs(1), IntValueIs(0)));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_UInt32) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_uint32: 1
                                                     repeated_uint32: 0)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_uint32"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(UintValueIs(1), UintValueIs(0)));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Duration) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(repeated_duration: { seconds: 1 nanos: 1 }
               repeated_duration: {})pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_duration"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(DurationValueIs(absl::Seconds(1) +
                                                  absl::Nanoseconds(1)),
                                  DurationValueIs(absl::ZeroDuration())));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Bytes) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(repeated_bytes: "bar" repeated_bytes: "foo")pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bytes"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(BytesValueIs("bar"), BytesValueIs("foo")));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Enum) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(repeated_nested_enum: BAR repeated_nested_enum: FOO)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_nested_enum"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(IntValueIs(1), IntValueIs(0)));
}

TEST_F(ParsedRepeatedFieldValueTest, ForEach_Null) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_null_value:
                                                         NULL_VALUE
                                                     repeated_null_value:
                                                         NULL_VALUE)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_null_value"), arena());
  std::vector<Value> values;
  EXPECT_THAT(value.ForEach(
                  [&](const Value& element) -> absl::StatusOr<bool> {
                    values.push_back(element);
                    return true;
                  },
                  descriptor_pool(), message_factory(), arena()),
              IsOk());
  EXPECT_THAT(values, ElementsAre(IsNullValue(), IsNullValue()));
}

TEST_F(ParsedRepeatedFieldValueTest, NewIterator) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"), arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ParsedRepeatedFieldValueTest, NewIterator1) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"), arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(false))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedRepeatedFieldValueTest, NewIterator2) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: false
                                                     repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"), arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(0), BoolValueIs(false)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(1), BoolValueIs(true)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedRepeatedFieldValueTest, Contains) {
  ParsedRepeatedFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(repeated_bool: true)pb"),
      DynamicGetField<TestAllTypesProto3>("repeated_bool"), arena());
  EXPECT_THAT(value.Contains(BytesValue(), descriptor_pool(), message_factory(),
                             arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(NullValue(), descriptor_pool(), message_factory(),
                             arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(BoolValue(false), descriptor_pool(),
                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(BoolValue(true), descriptor_pool(),
                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Contains(DoubleValue(0.0), descriptor_pool(),
                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(DoubleValue(1.0), descriptor_pool(),
                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(StringValue("bar"), descriptor_pool(),
                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Contains(StringValue("foo"), descriptor_pool(),
                             message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      value.Contains(MapValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
}

}  // namespace
}  // namespace cel
