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
using ::cel::test::ErrorValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::StringValueIs;
using ::testing::AnyOf;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using ParsedJsonMapValueTest = common_internal::ValueTest<>;

TEST_F(ParsedJsonMapValueTest, Kind) {
  EXPECT_EQ(ParsedJsonMapValue::kind(), ParsedJsonMapValue::kKind);
  EXPECT_EQ(ParsedJsonMapValue::kind(), ValueKind::kMap);
}

TEST_F(ParsedJsonMapValueTest, GetTypeName) {
  EXPECT_EQ(ParsedJsonMapValue::GetTypeName(), ParsedJsonMapValue::kName);
  EXPECT_EQ(ParsedJsonMapValue::GetTypeName(), "google.protobuf.Struct");
}

TEST_F(ParsedJsonMapValueTest, GetRuntimeType) {
  EXPECT_EQ(ParsedJsonMapValue::GetRuntimeType(), JsonMapType());
}

TEST_F(ParsedJsonMapValueTest, DebugString_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  EXPECT_EQ(valid_value.DebugString(), "{}");
}

TEST_F(ParsedJsonMapValueTest, IsZeroValue_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  EXPECT_TRUE(valid_value.IsZeroValue());
}

TEST_F(ParsedJsonMapValueTest, SerializeTo_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(
      valid_value.SerializeTo(descriptor_pool(), message_factory(), &output),
      IsOk());
  EXPECT_THAT(std::move(output).Consume(), IsEmpty());
}

TEST_F(ParsedJsonMapValueTest, ConvertToJson_Dynamic) {
  auto json = DynamicParseTextProto<google::protobuf::Value>(R"pb()pb");
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  EXPECT_THAT(valid_value.ConvertToJson(descriptor_pool(), message_factory(),
                                        cel::to_address(json)),
              IsOk());
  EXPECT_THAT(*json, EqualsTextProto<google::protobuf::Value>(
                         R"pb(struct_value: {})pb"));
}

TEST_F(ParsedJsonMapValueTest, Equal_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  EXPECT_THAT(valid_value.Equal(BoolValue(), descriptor_pool(),
                                message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      valid_value.Equal(
          ParsedJsonMapValue(
              DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"),
              arena()),
          descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Equal(MapValue(), descriptor_pool(),
                                message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedJsonMapValueTest, Empty_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  EXPECT_TRUE(valid_value.IsEmpty());
}

TEST_F(ParsedJsonMapValueTest, Size_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(R"pb()pb"), arena());
  EXPECT_EQ(valid_value.Size(), 0);
}

TEST_F(ParsedJsonMapValueTest, Get_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  EXPECT_THAT(
      valid_value.Get(BoolValue(), descriptor_pool(), message_factory(),
                      arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
  EXPECT_THAT(valid_value.Get(StringValue("foo"), descriptor_pool(),
                              message_factory(), arena()),
              IsOkAndHolds(IsNullValue()));
  EXPECT_THAT(valid_value.Get(StringValue("bar"), descriptor_pool(),
                              message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      valid_value.Get(StringValue("baz"), descriptor_pool(), message_factory(),
                      arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kNotFound))));
}

TEST_F(ParsedJsonMapValueTest, Find_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  EXPECT_THAT(valid_value.Find(BoolValue(), descriptor_pool(),
                               message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(valid_value.Find(StringValue("foo"), descriptor_pool(),
                               message_factory(), arena()),
              IsOkAndHolds(Optional(IsNullValue())));
  EXPECT_THAT(valid_value.Find(StringValue("bar"), descriptor_pool(),
                               message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  EXPECT_THAT(valid_value.Find(StringValue("baz"), descriptor_pool(),
                               message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedJsonMapValueTest, Has_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  EXPECT_THAT(valid_value.Has(BoolValue(), descriptor_pool(), message_factory(),
                              arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Has(StringValue("foo"), descriptor_pool(),
                              message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Has(StringValue("bar"), descriptor_pool(),
                              message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Has(StringValue("baz"), descriptor_pool(),
                              message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(ParsedJsonMapValueTest, ListKeys_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(
      auto keys,
      valid_value.ListKeys(descriptor_pool(), message_factory(), arena()));
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

TEST_F(ParsedJsonMapValueTest, ForEach_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  std::vector<std::pair<Value, Value>> entries;
  EXPECT_THAT(
      valid_value.ForEach(
          [&](const Value& key, const Value& value) -> absl::StatusOr<bool> {
            entries.push_back(std::pair{std::move(key), std::move(value)});
            return true;
          },
          descriptor_pool(), message_factory(), arena()),
      IsOk());
  EXPECT_THAT(entries, UnorderedElementsAre(
                           Pair(StringValueIs("foo"), IsNullValue()),
                           Pair(StringValueIs("bar"), BoolValueIs(true))));
}

TEST_F(ParsedJsonMapValueTest, NewIterator_Dynamic) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator());
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

TEST_F(ParsedJsonMapValueTest, NewIterator1) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(
                  Optional(AnyOf(StringValueIs("foo"), StringValueIs("bar")))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(
                  Optional(AnyOf(StringValueIs("foo"), StringValueIs("bar")))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedJsonMapValueTest, NewIterator2) {
  ParsedJsonMapValue valid_value(
      DynamicParseTextProto<google::protobuf::Struct>(
          R"pb(fields {
                 key: "foo"
                 value: {}
               }
               fields {
                 key: "bar"
                 value: { bool_value: true }
               })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator());
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(
                  AnyOf(Pair(StringValueIs("foo"), IsNullValue()),
                        Pair(StringValueIs("bar"), BoolValueIs(true))))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(
                  AnyOf(Pair(StringValueIs("foo"), IsNullValue()),
                        Pair(StringValueIs("bar"), BoolValueIs(true))))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

}  // namespace
}  // namespace cel
