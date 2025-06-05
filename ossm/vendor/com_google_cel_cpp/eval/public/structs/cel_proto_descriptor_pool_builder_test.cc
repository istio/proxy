/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/public/structs/cel_proto_descriptor_pool_builder.h"

#include <string>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "absl/container/flat_hash_map.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

TEST(DescriptorPoolUtilsTest, PopulatesEmptyDescriptorPool) {
  google::protobuf::DescriptorPool descriptor_pool;

  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Any"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.BoolValue"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.BytesValue"),
            nullptr);
  ASSERT_EQ(
      descriptor_pool.FindMessageTypeByName("google.protobuf.DoubleValue"),
      nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Duration"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.FloatValue"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Int32Value"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Int64Value"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.ListValue"),
            nullptr);
  ASSERT_EQ(
      descriptor_pool.FindMessageTypeByName("google.protobuf.StringValue"),
      nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Struct"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Timestamp"),
            nullptr);
  ASSERT_EQ(
      descriptor_pool.FindMessageTypeByName("google.protobuf.UInt32Value"),
      nullptr);
  ASSERT_EQ(
      descriptor_pool.FindMessageTypeByName("google.protobuf.UInt64Value"),
      nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.Value"),
            nullptr);
  ASSERT_EQ(descriptor_pool.FindMessageTypeByName("google.protobuf.FieldMask"),
            nullptr);

  ASSERT_OK(AddStandardMessageTypesToDescriptorPool(descriptor_pool));

  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Any"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.BoolValue"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.BytesValue"),
            nullptr);
  EXPECT_NE(
      descriptor_pool.FindMessageTypeByName("google.protobuf.DoubleValue"),
      nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Duration"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.FloatValue"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Int32Value"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Int64Value"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.ListValue"),
            nullptr);
  EXPECT_NE(
      descriptor_pool.FindMessageTypeByName("google.protobuf.StringValue"),
      nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Struct"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Timestamp"),
            nullptr);
  EXPECT_NE(
      descriptor_pool.FindMessageTypeByName("google.protobuf.UInt32Value"),
      nullptr);
  EXPECT_NE(
      descriptor_pool.FindMessageTypeByName("google.protobuf.UInt64Value"),
      nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Value"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.FieldMask"),
            nullptr);
  EXPECT_NE(descriptor_pool.FindMessageTypeByName("google.protobuf.Empty"),
            nullptr);
}

TEST(DescriptorPoolUtilsTest, AcceptsPreAddedStandardTypes) {
  google::protobuf::DescriptorPool descriptor_pool;

  for (auto proto_name : std::vector<std::string>{
           "google.protobuf.Any", "google.protobuf.BoolValue",
           "google.protobuf.BytesValue", "google.protobuf.DoubleValue",
           "google.protobuf.Duration", "google.protobuf.FloatValue",
           "google.protobuf.Int32Value", "google.protobuf.Int64Value",
           "google.protobuf.ListValue", "google.protobuf.StringValue",
           "google.protobuf.Struct", "google.protobuf.Timestamp",
           "google.protobuf.UInt32Value", "google.protobuf.UInt64Value",
           "google.protobuf.Value", "google.protobuf.FieldMask",
           "google.protobuf.Empty"}) {
    const google::protobuf::Descriptor* descriptor =
        google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
            proto_name);
    ASSERT_NE(descriptor, nullptr);
    google::protobuf::FileDescriptorProto file_descriptor_proto;
    descriptor->file()->CopyTo(&file_descriptor_proto);
    ASSERT_NE(descriptor_pool.BuildFile(file_descriptor_proto), nullptr);
  }

  EXPECT_OK(AddStandardMessageTypesToDescriptorPool(descriptor_pool));
}

TEST(DescriptorPoolUtilsTest, RejectsModifiedStandardType) {
  google::protobuf::DescriptorPool descriptor_pool;

  const google::protobuf::Descriptor* descriptor =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.protobuf.Duration");
  ASSERT_NE(descriptor, nullptr);
  google::protobuf::FileDescriptorProto file_descriptor_proto;
  descriptor->file()->CopyTo(&file_descriptor_proto);
  // We emulate a modification by external code that replaced the nanos by a
  // millis field.
  google::protobuf::FieldDescriptorProto seconds_desc_proto;
  google::protobuf::FieldDescriptorProto nanos_desc_proto;
  descriptor->FindFieldByName("seconds")->CopyTo(&seconds_desc_proto);
  descriptor->FindFieldByName("nanos")->CopyTo(&nanos_desc_proto);
  nanos_desc_proto.set_name("millis");
  file_descriptor_proto.mutable_message_type(0)->clear_field();
  *file_descriptor_proto.mutable_message_type(0)->add_field() =
      seconds_desc_proto;
  *file_descriptor_proto.mutable_message_type(0)->add_field() =
      nanos_desc_proto;

  descriptor_pool.BuildFile(file_descriptor_proto);

  EXPECT_THAT(
      AddStandardMessageTypesToDescriptorPool(descriptor_pool),
      StatusIs(absl::StatusCode::kFailedPrecondition, HasSubstr("differs")));
}

TEST(DescriptorPoolUtilsTest, GetStandardMessageTypesFileDescriptorSet) {
  google::protobuf::FileDescriptorSet fdset = GetStandardMessageTypesFileDescriptorSet();
  std::vector<std::string> file_names;
  for (int i = 0; i < fdset.file_size(); ++i) {
    file_names.push_back(fdset.file(i).name());
  }
  EXPECT_THAT(
      file_names,
      UnorderedElementsAre(
          "google/protobuf/any.proto", "google/protobuf/struct.proto",
          "google/protobuf/wrappers.proto", "google/protobuf/timestamp.proto",
          "google/protobuf/duration.proto", "google/protobuf/field_mask.proto",
          "google/protobuf/empty.proto"));
}

}  // namespace

}  // namespace google::api::expr::runtime
