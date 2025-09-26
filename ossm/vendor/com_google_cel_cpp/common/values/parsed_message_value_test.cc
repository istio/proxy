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

#include "google/protobuf/struct.pb.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
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
using ::cel::test::BoolValueIs;
using ::testing::_;
using ::testing::IsEmpty;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using ParsedMessageValueTest = common_internal::ValueTest<>;

TEST_F(ParsedMessageValueTest, Kind) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_EQ(value.kind(), ParsedMessageValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kStruct);
}

TEST_F(ParsedMessageValueTest, GetTypeName) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_EQ(value.GetTypeName(), "cel.expr.conformance.proto3.TestAllTypes");
}

TEST_F(ParsedMessageValueTest, GetRuntimeType) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_EQ(value.GetRuntimeType(), MessageType(value.GetDescriptor()));
}

TEST_F(ParsedMessageValueTest, DebugString) {
  ParsedMessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_THAT(value.DebugString(), _);
}

TEST_F(ParsedMessageValueTest, IsZeroValue) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_TRUE(value.IsZeroValue());
}

TEST_F(ParsedMessageValueTest, SerializeTo) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(value.SerializeTo(descriptor_pool(), message_factory(), &output),
              IsOk());
  EXPECT_THAT(std::move(output).Consume(), IsEmpty());
}

TEST_F(ParsedMessageValueTest, ConvertToJson) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  auto json = DynamicParseTextProto<google::protobuf::Value>(R"pb()pb");
  EXPECT_THAT(value.ConvertToJson(descriptor_pool(), message_factory(),
                                  cel::to_address(json)),
              IsOk());
  EXPECT_THAT(*json, EqualsTextProto<google::protobuf::Value>(
                         R"pb(struct_value: {})pb"));
}

TEST_F(ParsedMessageValueTest, Equal) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_THAT(
      value.Equal(BoolValue(), descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
  EXPECT_THAT(value.Equal(MakeParsedMessage<TestAllTypesProto3>(),
                          descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(true)));
}

TEST_F(ParsedMessageValueTest, GetFieldByName) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_THAT(value.GetFieldByName("single_bool", descriptor_pool(),
                                   message_factory(), arena()),
              IsOkAndHolds(BoolValueIs(false)));
}

TEST_F(ParsedMessageValueTest, GetFieldByNumber) {
  MessageValue value = MakeParsedMessage<TestAllTypesProto3>();
  EXPECT_THAT(
      value.GetFieldByNumber(13, descriptor_pool(), message_factory(), arena()),
      IsOkAndHolds(BoolValueIs(false)));
}

}  // namespace
}  // namespace cel
