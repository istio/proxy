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

#include "google/protobuf/descriptor.pb.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::StartsWith;

TEST(EnumType, Kind) { EXPECT_EQ(EnumType::kind(), TypeKind::kEnum); }

TEST(EnumType, Default) {
  EnumType type;
  EXPECT_FALSE(type);
  EXPECT_THAT(type.DebugString(), Eq(""));
  EXPECT_EQ(type, EnumType());
}

TEST(EnumType, Descriptor) {
  google::protobuf::DescriptorPool pool;
  {
    google::protobuf::FileDescriptorProto file_desc_proto;
    file_desc_proto.set_syntax("proto3");
    file_desc_proto.set_package("test");
    file_desc_proto.set_name("test/enum.proto");
    auto* enum_desc = file_desc_proto.add_enum_type();
    enum_desc->set_name("Enum");
    auto* enum_value_desc = enum_desc->add_value();
    enum_value_desc->set_number(0);
    enum_value_desc->set_name("VALUE");
    ASSERT_THAT(pool.BuildFile(file_desc_proto), NotNull());
  }
  const google::protobuf::EnumDescriptor* desc = pool.FindEnumTypeByName("test.Enum");
  ASSERT_THAT(desc, NotNull());
  EnumType type(desc);
  EXPECT_TRUE(type);
  EXPECT_THAT(type.name(), Eq("test.Enum"));
  EXPECT_THAT(type.DebugString(), StartsWith("test.Enum@0x"));
  EXPECT_THAT(type.GetParameters(), IsEmpty());
  EXPECT_NE(type, EnumType());
  EXPECT_NE(EnumType(), type);
  EXPECT_EQ(cel::to_address(type), desc);
}

}  // namespace
}  // namespace cel
