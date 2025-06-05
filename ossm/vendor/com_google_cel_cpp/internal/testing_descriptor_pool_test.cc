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

#include "internal/testing_descriptor_pool.h"

#include "internal/testing.h"
#include "google/protobuf/descriptor.h"

namespace cel::internal {
namespace {

using ::testing::NotNull;

TEST(TestingDescriptorPool, NullValue) {
  ASSERT_THAT(GetTestingDescriptorPool()->FindEnumTypeByName(
                  "google.protobuf.NullValue"),
              NotNull());
}

TEST(TestingDescriptorPool, BoolValue) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.BoolValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE);
}

TEST(TestingDescriptorPool, Int32Value) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Int32Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE);
}

TEST(TestingDescriptorPool, Int64Value) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Int64Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE);
}

TEST(TestingDescriptorPool, UInt32Value) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.UInt32Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE);
}

TEST(TestingDescriptorPool, UInt64Value) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.UInt64Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE);
}

TEST(TestingDescriptorPool, FloatValue) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.FloatValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE);
}

TEST(TestingDescriptorPool, DoubleValue) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.DoubleValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE);
}

TEST(TestingDescriptorPool, BytesValue) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.BytesValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE);
}

TEST(TestingDescriptorPool, StringValue) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.StringValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE);
}

TEST(TestingDescriptorPool, Any) {
  const auto* desc =
      GetTestingDescriptorPool()->FindMessageTypeByName("google.protobuf.Any");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(), google::protobuf::Descriptor::WELLKNOWNTYPE_ANY);
}

TEST(TestingDescriptorPool, Duration) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Duration");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION);
}

TEST(TestingDescriptorPool, Timestamp) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Timestamp");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP);
}

TEST(TestingDescriptorPool, Value) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(), google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
}

TEST(TestingDescriptorPool, ListValue) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.ListValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);
}

TEST(TestingDescriptorPool, Struct) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Struct");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(), google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);
}

TEST(TestingDescriptorPool, FieldMask) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.FieldMask");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_FIELDMASK);
}

TEST(TestingDescriptorPool, Empty) {
  const auto* desc = GetTestingDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Empty");
  ASSERT_THAT(desc, NotNull());
}

TEST(TestingDescriptorPool, TestAllTypesProto2) {
  EXPECT_THAT(GetTestingDescriptorPool()->FindMessageTypeByName(
                  "google.api.expr.test.v1.proto2.TestAllTypes"),
              NotNull());
}

TEST(TestingDescriptorPool, TestAllTypesProto3) {
  EXPECT_THAT(GetTestingDescriptorPool()->FindMessageTypeByName(
                  "google.api.expr.test.v1.proto3.TestAllTypes"),
              NotNull());
}

}  // namespace
}  // namespace cel::internal
