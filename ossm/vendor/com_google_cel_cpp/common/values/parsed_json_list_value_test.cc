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
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using ParsedJsonListValueTest = common_internal::ValueTest<>;

TEST_F(ParsedJsonListValueTest, Kind) {
  EXPECT_EQ(ParsedJsonListValue::kind(), ParsedJsonListValue::kKind);
  EXPECT_EQ(ParsedJsonListValue::kind(), ValueKind::kList);
}

TEST_F(ParsedJsonListValueTest, GetTypeName) {
  EXPECT_EQ(ParsedJsonListValue::GetTypeName(), ParsedJsonListValue::kName);
  EXPECT_EQ(ParsedJsonListValue::GetTypeName(), "google.protobuf.ListValue");
}

TEST_F(ParsedJsonListValueTest, GetRuntimeType) {
  EXPECT_EQ(ParsedJsonListValue::GetRuntimeType(), JsonListType());
}

TEST_F(ParsedJsonListValueTest, DebugString_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  EXPECT_EQ(valid_value.DebugString(), "[]");
}

TEST_F(ParsedJsonListValueTest, IsZeroValue_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  EXPECT_TRUE(valid_value.IsZeroValue());
}

TEST_F(ParsedJsonListValueTest, SerializeTo_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(
      valid_value.SerializeTo(descriptor_pool(), message_factory(), &output),
      IsOk());
  EXPECT_THAT(std::move(output).Consume(), IsEmpty());
}

TEST_F(ParsedJsonListValueTest, ConvertToJson_Dynamic) {
  auto json = DynamicParseTextProto<google::protobuf::Value>(R"pb()pb");
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  EXPECT_THAT(valid_value.ConvertToJson(descriptor_pool(), message_factory(),
                                        cel::to_address(json)),
              IsOk());
  EXPECT_THAT(
      *json, EqualsTextProto<google::protobuf::Value>(R"pb(list_value: {})pb"));
}

TEST_F(ParsedJsonListValueTest, Equal_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  EXPECT_THAT(valid_value.Equal(BoolValue(), descriptor_pool(),
                                message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(
      valid_value.Equal(
          ParsedJsonListValue(
              DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"),
              arena()),
          descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Equal(ListValue(), descriptor_pool(),
                                message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedJsonListValueTest, Empty_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  EXPECT_TRUE(valid_value.IsEmpty());
}

TEST_F(ParsedJsonListValueTest, Size_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(R"pb()pb"), arena());
  EXPECT_EQ(valid_value.Size(), 0);
}

TEST_F(ParsedJsonListValueTest, Get_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"),
      arena());
  EXPECT_THAT(valid_value.Get(0, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(IsNullValue()));
  EXPECT_THAT(valid_value.Get(1, descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      valid_value.Get(2, descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument))));
}

TEST_F(ParsedJsonListValueTest, ForEach_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"),
      arena());
  {
    std::vector<Value> values;
    EXPECT_THAT(valid_value.ForEach(
                    [&](const Value& element) -> absl::StatusOr<bool> {
                      values.push_back(element);
                      return true;
                    },
                    descriptor_pool(), message_factory(), arena()),
                IsOk());
    EXPECT_THAT(values, ElementsAre(IsNullValue(), BoolValueIs(true)));
  }
  {
    std::vector<Value> values;
    EXPECT_THAT(valid_value.ForEach(
                    [&](size_t, const Value& element) -> absl::StatusOr<bool> {
                      values.push_back(element);
                      return true;
                    },
                    descriptor_pool(), message_factory(), arena()),
                IsOk());
    EXPECT_THAT(values, ElementsAre(IsNullValue(), BoolValueIs(true)));
  }
}

TEST_F(ParsedJsonListValueTest, NewIterator_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator());
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(IsNullValue()));
  ASSERT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  ASSERT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(ParsedJsonListValueTest, NewIterator1) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator());
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(IsNullValue())));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(BoolValueIs(true))));
  EXPECT_THAT(iterator->Next1(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedJsonListValueTest, NewIterator2) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true })pb"),
      arena());
  ASSERT_OK_AND_ASSIGN(auto iterator, valid_value.NewIterator());
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(0), IsNullValue()))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Optional(Pair(IntValueIs(1), BoolValueIs(true)))));
  EXPECT_THAT(iterator->Next2(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(ParsedJsonListValueTest, Contains_Dynamic) {
  ParsedJsonListValue valid_value(
      DynamicParseTextProto<google::protobuf::ListValue>(
          R"pb(values {}
               values { bool_value: true }
               values { number_value: 1.0 }
               values { string_value: "foo" }
               values { list_value: {} }
               values { struct_value: {} })pb"),
      arena());
  EXPECT_THAT(valid_value.Contains(BytesValue(), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(NullValue(), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(BoolValue(false), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(BoolValue(true), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(DoubleValue(0.0), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(DoubleValue(1.0), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(StringValue("bar"), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(StringValue("foo"), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(valid_value.Contains(
                  ParsedJsonListValue(
                      DynamicParseTextProto<google::protobuf::ListValue>(
                          R"pb(values {}
                               values { bool_value: true }
                               values { number_value: 1.0 }
                               values { string_value: "foo" }
                               values { list_value: {} }
                               values { struct_value: {} })pb"),
                      arena()),
                  descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(ListValue(), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
  EXPECT_THAT(
      valid_value.Contains(
          ParsedJsonMapValue(DynamicParseTextProto<google::protobuf::Struct>(
                                 R"pb(fields {
                                        key: "foo"
                                        value: { bool_value: true }
                                      })pb"),
                             arena()),
          descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(valid_value.Contains(MapValue(), descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
}

}  // namespace
}  // namespace cel
