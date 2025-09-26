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

#include "extensions/protobuf/bind_proto_to_activation.h"

#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "cel/expr/conformance/proto2/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::expr::conformance::proto2::TestAllTypes;
using ::cel::test::IntValueIs;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Optional;

using BindProtoToActivationTest = common_internal::ValueTest<>;

TEST_F(BindProtoToActivationTest, BindProtoToActivation) {
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("single_int64", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IntValueIs(123))));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationWktUnsupported) {
  google::protobuf::Int64Value int64_value;
  int64_value.set_value(123);
  Activation activation;

  EXPECT_THAT(BindProtoToActivation(int64_value, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("google.protobuf.Int64Value")));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationSkip) {
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("single_int32", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(activation.FindVariable("single_sint32", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Eq(absl::nullopt)));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationDefault) {
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_THAT(
      BindProtoToActivation(
          test_all_types, BindProtoUnsetFieldBehavior::kBindDefaultValue,
          descriptor_pool(), message_factory(), arena(), &activation),
      IsOk());

  // from test_all_types.proto
  // optional int32 single_int32 = 1 [default = -32];
  EXPECT_THAT(activation.FindVariable("single_int32", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IntValueIs(-32))));
  EXPECT_THAT(activation.FindVariable("single_sint32", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IntValueIs(0))));
}

// Special case any fields. Mirrors go evaluator behavior.
TEST_F(BindProtoToActivationTest, BindProtoToActivationDefaultAny) {
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_THAT(
      BindProtoToActivation(
          test_all_types, BindProtoUnsetFieldBehavior::kBindDefaultValue,
          descriptor_pool(), message_factory(), arena(), &activation),
      IsOk());

  EXPECT_THAT(activation.FindVariable("single_any", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(test::IsNullValue())));
}

MATCHER_P(IsListValueOfSize, size, "") {
  const Value& v = arg;

  auto value = As<ListValue>(v);
  if (!value) {
    return false;
  }
  auto s = value->Size();
  return s.ok() && *s == size;
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationRepeated) {
  TestAllTypes test_all_types;
  test_all_types.add_repeated_int64(123);
  test_all_types.add_repeated_int64(456);
  test_all_types.add_repeated_int64(789);

  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("repeated_int64", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsListValueOfSize(3))));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationRepeatedEmpty) {
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("repeated_int32", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsListValueOfSize(0))));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationRepeatedComplex) {
  TestAllTypes test_all_types;
  auto* nested = test_all_types.add_repeated_nested_message();
  nested->set_bb(123);
  nested = test_all_types.add_repeated_nested_message();
  nested->set_bb(456);
  nested = test_all_types.add_repeated_nested_message();
  nested->set_bb(789);
  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(
      activation.FindVariable("repeated_nested_message", descriptor_pool(),
                              message_factory(), arena()),
      IsOkAndHolds(Optional(IsListValueOfSize(3))));
}

MATCHER_P(IsMapValueOfSize, size, "") {
  const Value& v = arg;

  auto value = As<MapValue>(v);
  if (!value) {
    return false;
  }
  auto s = value->Size();
  return s.ok() && *s == size;
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationMap) {
  TestAllTypes test_all_types;
  (*test_all_types.mutable_map_int64_int64())[1] = 2;
  (*test_all_types.mutable_map_int64_int64())[2] = 4;

  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("map_int64_int64", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsMapValueOfSize(2))));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationMapEmpty) {
  TestAllTypes test_all_types;
  test_all_types.set_single_int64(123);
  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("map_int32_int32", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsMapValueOfSize(0))));
}

TEST_F(BindProtoToActivationTest, BindProtoToActivationMapComplex) {
  TestAllTypes test_all_types;
  TestAllTypes::NestedMessage value;
  value.set_bb(42);
  (*test_all_types.mutable_map_int64_message())[1] = value;
  (*test_all_types.mutable_map_int64_message())[2] = value;

  Activation activation;

  ASSERT_THAT(BindProtoToActivation(test_all_types, descriptor_pool(),
                                    message_factory(), arena(), &activation),
              IsOk());

  EXPECT_THAT(activation.FindVariable("map_int64_message", descriptor_pool(),
                                      message_factory(), arena()),
              IsOkAndHolds(Optional(IsMapValueOfSize(2))));
}

}  // namespace
}  // namespace cel::extensions
