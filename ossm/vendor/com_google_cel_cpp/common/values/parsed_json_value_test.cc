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

#include "common/values/parsed_json_value.h"

#include "google/protobuf/struct.pb.h"
#include "absl/strings/string_view.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"

namespace cel::common_internal {
namespace {

using ::cel::test::BoolValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::ListValueElements;
using ::cel::test::ListValueIs;
using ::cel::test::MapValueElements;
using ::cel::test::MapValueIs;
using ::cel::test::StringValueIs;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using ParsedJsonValueTest = common_internal::ValueTest<>;

TEST_F(ParsedJsonValueTest, Null_Dynamic) {
  EXPECT_THAT(ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                                  R"pb(null_value: NULL_VALUE)pb"),
                              arena()),
              IsNullValue());
  EXPECT_THAT(ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                                  R"pb(null_value: NULL_VALUE)pb"),
                              arena()),
              IsNullValue());
}

TEST_F(ParsedJsonValueTest, Bool_Dynamic) {
  EXPECT_THAT(ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                                  R"pb(bool_value: true)pb"),
                              arena()),
              BoolValueIs(true));
}

TEST_F(ParsedJsonValueTest, Double_Dynamic) {
  EXPECT_THAT(ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                                  R"pb(number_value: 1.0)pb"),
                              arena()),
              DoubleValueIs(1.0));
}

TEST_F(ParsedJsonValueTest, String_Dynamic) {
  EXPECT_THAT(ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                                  R"pb(string_value: "foo")pb"),
                              arena()),
              StringValueIs("foo"));
}

TEST_F(ParsedJsonValueTest, List_Dynamic) {
  EXPECT_THAT(ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                                  R"pb(list_value: {
                                         values {}
                                         values { bool_value: true }
                                       })pb"),
                              arena()),
              ListValueIs(ListValueElements(
                  ElementsAre(IsNullValue(), BoolValueIs(true)),
                  descriptor_pool(), message_factory(), arena())));
}

TEST_F(ParsedJsonValueTest, Map_Dynamic) {
  EXPECT_THAT(
      ParsedJsonValue(DynamicParseTextProto<google::protobuf::Value>(
                          R"pb(struct_value: {
                                 fields {
                                   key: "foo"
                                   value: {}
                                 }
                                 fields {
                                   key: "bar"
                                   value: { bool_value: true }
                                 }
                               })pb"),
                      arena()),
      MapValueIs(MapValueElements(
          UnorderedElementsAre(Pair(StringValueIs("foo"), IsNullValue()),
                               Pair(StringValueIs("bar"), BoolValueIs(true))),
          descriptor_pool(), message_factory(), arena())));
}

}  // namespace
}  // namespace cel::common_internal
