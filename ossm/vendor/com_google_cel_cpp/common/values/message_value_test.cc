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

#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/attribute.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::An;
using ::testing::Optional;

using TestAllTypesProto3 = ::cel::expr::conformance::proto3::TestAllTypes;

using MessageValueTest = common_internal::ValueTest<>;

TEST_F(MessageValueTest, Default) {
  MessageValue value;
  EXPECT_FALSE(value);
  google::protobuf::io::CordOutputStream output;
  EXPECT_THAT(value.SerializeTo(descriptor_pool(), message_factory(), &output),
              StatusIs(absl::StatusCode::kInternal));
  Value scratch;
  int count;
  EXPECT_THAT(
      value.Equal(NullValue(), descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.Equal(NullValue(), descriptor_pool(), message_factory(),
                          arena(), &scratch),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(
      value.GetFieldByName("", descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.GetFieldByName("", descriptor_pool(), message_factory(),
                                   arena(), &scratch),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(
      value.GetFieldByNumber(0, descriptor_pool(), message_factory(), arena()),
      StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.GetFieldByNumber(0, descriptor_pool(), message_factory(),
                                     arena(), &scratch),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.HasFieldByName(""), StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.HasFieldByNumber(0), StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.ForEachField([](absl::string_view, const Value&)
                                     -> absl::StatusOr<bool> { return true; },
                                 descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.Qualify({AttributeQualifier::OfString("foo")}, false,
                            descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kInternal));
  EXPECT_THAT(value.Qualify({AttributeQualifier::OfString("foo")}, false,
                            descriptor_pool(), message_factory(), arena(),
                            &scratch, &count),
              StatusIs(absl::StatusCode::kInternal));
}

template <typename T>
constexpr T& AsLValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return t;
}

template <typename T>
constexpr const T& AsConstLValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return t;
}

template <typename T>
constexpr T&& AsRValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return static_cast<T&&>(t);
}

template <typename T>
constexpr const T&& AsConstRValueRef(T& t ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return static_cast<const T&&>(t);
}

TEST_F(MessageValueTest, Parsed) {
  MessageValue value(ParsedMessageValue(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"), arena()));
  MessageValue other_value = value;
  EXPECT_TRUE(value);
  EXPECT_TRUE(value.Is<ParsedMessageValue>());
  EXPECT_THAT(value.As<ParsedMessageValue>(),
              Optional(An<ParsedMessageValue>()));
  EXPECT_THAT(AsLValueRef<MessageValue>(value).Get<ParsedMessageValue>(),
              An<ParsedMessageValue>());
  EXPECT_THAT(AsConstLValueRef<MessageValue>(value).Get<ParsedMessageValue>(),
              An<ParsedMessageValue>());
  EXPECT_THAT(AsRValueRef<MessageValue>(value).Get<ParsedMessageValue>(),
              An<ParsedMessageValue>());
  EXPECT_THAT(
      AsConstRValueRef<MessageValue>(other_value).Get<ParsedMessageValue>(),
      An<ParsedMessageValue>());
}

TEST_F(MessageValueTest, Kind) {
  MessageValue value;
  EXPECT_EQ(value.kind(), ParsedMessageValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kStruct);
}

TEST_F(MessageValueTest, GetTypeName) {
  MessageValue value(ParsedMessageValue(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"), arena()));
  EXPECT_EQ(value.GetTypeName(), "cel.expr.conformance.proto3.TestAllTypes");
}

TEST_F(MessageValueTest, GetRuntimeType) {
  MessageValue value(ParsedMessageValue(
      DynamicParseTextProto<TestAllTypesProto3>(R"pb()pb"), arena()));
  EXPECT_EQ(value.GetRuntimeType(), MessageType(value.GetDescriptor()));
}

}  // namespace
}  // namespace cel
