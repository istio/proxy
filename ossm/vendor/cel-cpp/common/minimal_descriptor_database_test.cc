// Copyright 2025 Google LLC
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

#include "common/minimal_descriptor_database.h"

#include "google/protobuf/descriptor.pb.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

using ::testing::IsTrue;

TEST(GetMinimalDescriptorDatabase, NullValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.NullValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, BoolValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.BoolValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Int32Value) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Int32Value", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Int64Value) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Int64Value", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, UInt32Value) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.UInt32Value", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, UInt64Value) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.UInt64Value", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, FloatValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.FloatValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, DoubleValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.DoubleValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, BytesValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.BytesValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, StringValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.StringValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Any) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Any", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Duration) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Duration", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Timestamp) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Timestamp", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Value) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Value", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, ListValue) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.ListValue", &fd),
              IsTrue());
}

TEST(GetMinimalDescriptorDatabase, Struct) {
  google::protobuf::FileDescriptorProto fd;
  EXPECT_THAT(GetMinimalDescriptorDatabase()->FindFileContainingSymbol(
                  "google.protobuf.Struct", &fd),
              IsTrue());
}

}  // namespace
}  // namespace cel
