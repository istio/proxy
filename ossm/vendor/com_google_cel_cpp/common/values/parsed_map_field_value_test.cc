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
using ::cel::test::StringValueIs;
using ::cel::test::UintValueIs;
using ::testing::_;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using ParsedMapFieldValueTest = common_internal::ValueTest<>;

TEST_F(ParsedMapFieldValueTest, Field) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_TRUE(value);
}

TEST_F(ParsedMapFieldValueTest, Kind) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_EQ(value.kind(), ParsedMapFieldValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kMap);
}

TEST_F(ParsedMapFieldValueTest, GetTypeName) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_EQ(value.GetTypeName(), ParsedMapFieldValue::kName);
  EXPECT_EQ(value.GetTypeName(), "map");
}

TEST_F(ParsedMapFieldValueTest, GetRuntimeType) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_EQ(value.GetRuntimeType(), MapType());
}

TEST_F(ParsedMapFieldValueTest, DebugString) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_THAT(value.DebugString(), _);
}

TEST_F(ParsedMapFieldValueTest, IsZeroValue) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_TRUE(value.IsZeroValue());
}

TEST_F(ParsedMapFieldValueTest, SerializeTo) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(value.SerializeTo(descriptor_pool(), message_factory(), &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), IsEmpty());
}

TEST_F(ParsedMapFieldValueTest, ConvertToJson) {
  auto json = DynamicParseTextProto<google::protobuf::Value>(R"pb()pb");
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_THAT(value.ConvertToJson(descriptor_pool(), message_factory(),
                                  cel::to_address(json)),
              IsOk());
  EXPECT_THAT(*json, EqualsTextProto<google::protobuf::Value>(
                         R"pb(struct_value: {})pb"));
}

TEST_F(ParsedMapFieldValueTest, Equal_MapField) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_THAT(
      value.Equal(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      value.Equal(
          ParsedMapFieldValue(
              DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
              DynamicGetField<TestAllTypesProto3>("map_int32_int32"), arena()),
          descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      value.Equal(MapValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedMapFieldValueTest, Equal_JsonMap) {
  ParsedMapFieldValue map_value(
      DynamicParseTextProto<TestAllTypesProto3>(
          R"pb(map_string_string { key: "foo" value: "bar" }
               map_string_string { key: "bar" value: "foo" })pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_string"), arena());
  ParsedJsonMapValue json_value(DynamicParseTextProto<google::protobuf::Struct>(
                                    R"pb(
                                      fields {
                                        key: "foo"
                                        value { string_value: "bar" }
                                      }
                                      fields {
                                        key: "bar"
                                        value { string_value: "foo" }
                                      }
                                    )pb"),
                                arena());
  EXPECT_THAT(map_value.Equal(json_value, descriptor_pool(), message_factory(),
                              arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(json_value.Equal(map_value, descriptor_pool(), message_factory(),
                               arena()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedMapFieldValueTest, Empty) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_TRUE(value.IsEmpty());
}

TEST_F(ParsedMapFieldValueTest, Size) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_int64"), arena());
  EXPECT_EQ(value.Size(), 0);
}

TEST_F(ParsedMapFieldValueTest, Get) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  EXPECT_THAT(
      value.Get(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
  EXPECT_THAT(value.Get(StringValue("foo"), descriptor_pool(),
                        message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Get(StringValue("bar"), descriptor_pool(),
                        message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      value.Get(StringValue("baz"), descriptor_pool(), message_factory(),
                arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_F(ParsedMapFieldValueTest, Find) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  EXPECT_THAT(
      value.Find(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(value.Find(StringValue("foo"), descriptor_pool(),
                         message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(false))));
  EXPECT_THAT(value.Find(StringValue("bar"), descriptor_pool(),
                         message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  EXPECT_THAT(value.Find(StringValue("baz"), descriptor_pool(),
                         message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedMapFieldValueTest, Has) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  EXPECT_THAT(
      value.Has(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Has(StringValue("foo"), descriptor_pool(),
                        message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Has(StringValue("bar"), descriptor_pool(),
                        message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(value.Has(StringValue("baz"), descriptor_pool(),
                        message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(ParsedMapFieldValueTest, ListKeys) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  ASSERT_OK_AND_ASSIGN(
      auto keys, value.ListKeys(descriptor_pool(), message_factory(), arena()));
  EXPECT_THAT(keys.Size(), IsOkAndHolds(2));
  EXPECT_THAT(keys.DebugString(),
              AnyOf("[\"foo\", \"bar\"]", "[\"bar\", \"foo\"]"));
  EXPECT_THAT(
      keys.Contains(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(keys.Contains(StringValue("bar"), descriptor_pool(),
                            message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(keys.Get(0, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  EXPECT_THAT(keys.Get(1, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_StringBool) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), BoolValueIs(false)),
                           Pair(StringValueIs("bar"), BoolValueIs(true))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_Int32Double) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_int32_double { key: 1 value: 2 }
        map_int32_double { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_int32_double"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(IntValueIs(1), DoubleValueIs(2)),
                                   Pair(IntValueIs(2), DoubleValueIs(1))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_Int64Float) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_int64_float { key: 1 value: 2 }
        map_int64_float { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_int64_float"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(IntValueIs(1), DoubleValueIs(2)),
                                   Pair(IntValueIs(2), DoubleValueIs(1))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_UInt32UInt64) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_uint32_uint64 { key: 1 value: 2 }
        map_uint32_uint64 { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_uint32_uint64"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(UintValueIs(1), UintValueIs(2)),
                                   Pair(UintValueIs(2), UintValueIs(1))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_UInt64Int32) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_uint64_int32 { key: 1 value: 2 }
        map_uint64_int32 { key: 2 value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_uint64_int32"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(UintValueIs(1), IntValueIs(2)),
                                   Pair(UintValueIs(2), IntValueIs(1))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_BoolUInt32) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_bool_uint32 { key: true value: 2 }
        map_bool_uint32 { key: false value: 1 }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_bool_uint32"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(BoolValueIs(true), UintValueIs(2)),
                                   Pair(BoolValueIs(false), UintValueIs(1))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_StringString) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_string { key: "foo" value: "bar" }
        map_string_string { key: "bar" value: "foo" }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_string"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), StringValueIs("bar")),
                           Pair(StringValueIs("bar"), StringValueIs("foo"))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_StringDuration) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_duration {
          key: "foo"
          value: { seconds: 1 nanos: 1 }
        }
        map_string_duration {
          key: "bar"
          value: {}
        }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_duration"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(
      entries,
      UnorderedElementsAre(
          Pair(StringValueIs("foo"),
               DurationValueIs(absl::Seconds(1) + absl::Nanoseconds(1))),
          Pair(StringValueIs("bar"), DurationValueIs(absl::ZeroDuration()))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_StringBytes) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bytes { key: "foo" value: "bar" }
        map_string_bytes { key: "bar" value: "foo" }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bytes"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), BytesValueIs("bar")),
                           Pair(StringValueIs("bar"), BytesValueIs("foo"))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_StringEnum) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_enum { key: "foo" value: BAR }
        map_string_enum { key: "bar" value: FOO }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_enum"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(StringValueIs("foo"), IntValueIs(1)),
                                   Pair(StringValueIs("bar"), IntValueIs(0))));
}

TEST_F(ParsedMapFieldValueTest, ForEach_StringNull) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_null_value { key: "foo" value: NULL_VALUE }
        map_string_null_value { key: "bar" value: NULL_VALUE }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_null_value"), arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(StringValueIs("foo"), IsNullValue()),
                                   Pair(StringValueIs("bar"), IsNullValue())));
}

TEST_F(ParsedMapFieldValueTest, NewIterator) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(AnyOf(StringValueIs("foo"), StringValueIs("bar"))));
  ASSERT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ParsedMapFieldValueTest, NewIterator1) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(
                  Optional(AnyOf(StringValueIs("foo"), StringValueIs("bar")))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(
                  Optional(AnyOf(StringValueIs("foo"), StringValueIs("bar")))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedMapFieldValueTest, NewIterator2) {
  ParsedMapFieldValue value(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb(
        map_string_bool { key: "foo" value: false }
        map_string_bool { key: "bar" value: true }
      )pb"),
      DynamicGetField<TestAllTypesProto3>("map_string_bool"), arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(
                  AnyOf(Pair(StringValueIs("foo"), BoolValueIs(false)),
                        Pair(StringValueIs("bar"), BoolValueIs(true))))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(
                  AnyOf(Pair(StringValueIs("foo"), BoolValueIs(false)),
                        Pair(StringValueIs("bar"), BoolValueIs(true))))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

}  // namespace
}  // namespace cel
