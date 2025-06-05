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

#include "internal/minimal_descriptor_pool.h"

#include "internal/testing.h"
#include "google/protobuf/descriptor.h"

namespace cel::internal {
namespace {

using ::testing::NotNull;

TEST(MinimalDescriptorPool, NullValue) {
  ASSERT_THAT(GetMinimalDescriptorPool()->FindEnumTypeByName(
                  "google.protobuf.NullValue"),
              NotNull());
}

TEST(MinimalDescriptorPool, BoolValue) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.BoolValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE);
}

TEST(MinimalDescriptorPool, Int32Value) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Int32Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE);
}

TEST(MinimalDescriptorPool, Int64Value) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Int64Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE);
}

TEST(MinimalDescriptorPool, UInt32Value) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.UInt32Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE);
}

TEST(MinimalDescriptorPool, UInt64Value) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.UInt64Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE);
}

TEST(MinimalDescriptorPool, FloatValue) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.FloatValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE);
}

TEST(MinimalDescriptorPool, DoubleValue) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.DoubleValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE);
}

TEST(MinimalDescriptorPool, BytesValue) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.BytesValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE);
}

TEST(MinimalDescriptorPool, StringValue) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.StringValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE);
}

TEST(MinimalDescriptorPool, Any) {
  const auto* desc =
      GetMinimalDescriptorPool()->FindMessageTypeByName("google.protobuf.Any");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(), google::protobuf::Descriptor::WELLKNOWNTYPE_ANY);
}

TEST(MinimalDescriptorPool, Duration) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Duration");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION);
}

TEST(MinimalDescriptorPool, Timestamp) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Timestamp");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP);
}

TEST(MinimalDescriptorPool, Value) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Value");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(), google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE);
}

TEST(MinimalDescriptorPool, ListValue) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.ListValue");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(),
            google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE);
}

TEST(MinimalDescriptorPool, Struct) {
  const auto* desc = GetMinimalDescriptorPool()->FindMessageTypeByName(
      "google.protobuf.Struct");
  ASSERT_THAT(desc, NotNull());
  EXPECT_EQ(desc->well_known_type(), google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT);
}

}  // namespace
}  // namespace cel::internal
