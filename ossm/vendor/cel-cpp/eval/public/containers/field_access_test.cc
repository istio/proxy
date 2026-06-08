// Copyright 2021 Google LLC
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

#include "eval/public/containers/field_access.h"

#include <array>
#include <limits>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"
#include "internal/time.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::internal::MaxDuration;
using ::cel::internal::MaxTimestamp;
using ::google::protobuf::Arena;
using ::google::protobuf::FieldDescriptor;
using ::testing::HasSubstr;

TEST(FieldAccessTest, SetDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(CelValue::CreateDuration(MaxDuration()),
                                      field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetDurationBadDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MaxDuration() + absl::Seconds(1)), field, &msg,
      &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetDurationBadInputType) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status =
      SetValueToSingleField(CelValue::CreateInt64(1), field, &msg, &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestamp) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(CelValue::CreateTimestamp(MaxTimestamp()),
                                      field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetTimestampBadTime) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MaxTimestamp() + absl::Seconds(1)), field, &msg,
      &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestampBadInputType) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status =
      SetValueToSingleField(CelValue::CreateInt64(1), field, &msg, &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetInt32Overflow) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_int32");
  EXPECT_THAT(
      SetValueToSingleField(
          CelValue::CreateInt64(std::numeric_limits<int32_t>::max() + 1L),
          field, &msg, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Could not assign")));
}

TEST(FieldAccessTest, SetUint32Overflow) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_uint32");
  EXPECT_THAT(
      SetValueToSingleField(
          CelValue::CreateUint64(std::numeric_limits<uint32_t>::max() + 1L),
          field, &msg, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Could not assign")));
}

TEST(FieldAccessTest, SetMessage) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("standalone_message");
  TestAllTypes::NestedMessage* nested_msg =
      google::protobuf::Arena::Create<TestAllTypes::NestedMessage>(&arena);
  nested_msg->set_bb(1);
  auto status = SetValueToSingleField(
      CelProtoWrapper::CreateMessage(nested_msg, &arena), field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetMessageWithNul) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("standalone_message");
  auto status =
      SetValueToSingleField(CelValue::CreateNull(), field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

constexpr std::array<const char*, 9> kWrapperFieldNames = {
    "single_bool_wrapper",   "single_int64_wrapper",  "single_int32_wrapper",
    "single_uint64_wrapper", "single_uint32_wrapper", "single_double_wrapper",
    "single_float_wrapper",  "single_string_wrapper", "single_bytes_wrapper"};

// Unset wrapper type fields are treated as null if accessed after option
// enabled.
TEST(CreateValueFromFieldTest, UnsetWrapperTypesNullIfEnabled) {
  CelValue result;
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  for (const auto& field : kWrapperFieldNames) {
    ASSERT_OK(CreateValueFromSingleField(
        &test_message, TestAllTypes::GetDescriptor()->FindFieldByName(field),
        ProtoWrapperTypeOptions::kUnsetNull, &arena, &result))
        << field;
    ASSERT_TRUE(result.IsNull()) << field << ": " << result.DebugString();
  }
}

// Unset wrapper type fields are treated as proto default under old
// behavior.
TEST(CreateValueFromFieldTest, UnsetWrapperTypesDefaultValueIfDisabled) {
  CelValue result;
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  for (const auto& field : kWrapperFieldNames) {
    ASSERT_OK(CreateValueFromSingleField(
        &test_message, TestAllTypes::GetDescriptor()->FindFieldByName(field),
        ProtoWrapperTypeOptions::kUnsetProtoDefault, &arena, &result))
        << field;
    ASSERT_FALSE(result.IsNull()) << field << ": " << result.DebugString();
  }
}

// If a wrapper type is set to default value, the corresponding CelValue is the
// proto default value.
TEST(CreateValueFromFieldTest, SetWrapperTypesDefaultValue) {
  CelValue result;
  TestAllTypes test_message;
  google::protobuf::Arena arena;

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        single_bool_wrapper {}
        single_int64_wrapper {}
        single_int32_wrapper {}
        single_uint64_wrapper {}
        single_uint32_wrapper {}
        single_double_wrapper {}
        single_float_wrapper {}
        single_string_wrapper {}
        single_bytes_wrapper {}
      )pb",
      &test_message));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_bool_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull, &arena, &result));
  EXPECT_THAT(result, test::IsCelBool(false));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_int64_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull, &arena, &result));
  EXPECT_THAT(result, test::IsCelInt64(0));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_int32_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull, &arena, &result));
  EXPECT_THAT(result, test::IsCelInt64(0));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_uint64_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull, &arena, &result));
  EXPECT_THAT(result, test::IsCelUint64(0));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_uint32_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull, &arena, &result));
  EXPECT_THAT(result, test::IsCelUint64(0));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_double_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull,

      &arena, &result));
  EXPECT_THAT(result, test::IsCelDouble(0.0f));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_float_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull,

      &arena, &result));
  EXPECT_THAT(result, test::IsCelDouble(0.0f));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_string_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull,

      &arena, &result));
  EXPECT_THAT(result, test::IsCelString(""));

  ASSERT_OK(CreateValueFromSingleField(
      &test_message,
      TestAllTypes::GetDescriptor()->FindFieldByName("single_bytes_wrapper"),
      ProtoWrapperTypeOptions::kUnsetNull,

      &arena, &result));
  EXPECT_THAT(result, test::IsCelBytes(""));
}

}  // namespace

}  // namespace google::api::expr::runtime
