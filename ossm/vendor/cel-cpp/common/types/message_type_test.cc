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

using ::testing::An;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::StartsWith;

TEST(MessageType, Kind) { EXPECT_EQ(MessageType::kind(), TypeKind::kStruct); }

TEST(MessageType, Default) {
  MessageType type;
  EXPECT_FALSE(type);
  EXPECT_THAT(type.DebugString(), Eq(""));
  EXPECT_EQ(type, MessageType());
}

TEST(MessageType, Descriptor) {
  google::protobuf::DescriptorPool pool;
  {
    google::protobuf::FileDescriptorProto file_desc_proto;
    file_desc_proto.set_syntax("proto3");
    file_desc_proto.set_package("test");
    file_desc_proto.set_name("test/struct.proto");
    file_desc_proto.add_message_type()->set_name("Struct");
    ASSERT_THAT(pool.BuildFile(file_desc_proto), NotNull());
  }
  const google::protobuf::Descriptor* desc = pool.FindMessageTypeByName("test.Struct");
  ASSERT_THAT(desc, NotNull());
  MessageType type(desc);
  EXPECT_TRUE(type);
  EXPECT_THAT(type.name(), Eq("test.Struct"));
  EXPECT_THAT(type.DebugString(), StartsWith("test.Struct@0x"));
  EXPECT_THAT(type.GetParameters(), IsEmpty());
  EXPECT_NE(type, MessageType());
  EXPECT_NE(MessageType(), type);
  EXPECT_EQ(cel::to_address(type), desc);
}

TEST(MessageTypeField, Descriptor) {
  google::protobuf::DescriptorPool pool;
  {
    google::protobuf::FileDescriptorProto file_desc_proto;
    file_desc_proto.set_syntax("proto3");
    file_desc_proto.set_package("test");
    file_desc_proto.set_name("test/struct.proto");
    auto* message_type = file_desc_proto.add_message_type();
    message_type->set_name("Struct");
    auto* field = message_type->add_field();
    field->set_name("foo");
    field->set_json_name("foo");
    field->set_number(1);
    field->set_type(google::protobuf::FieldDescriptorProto::TYPE_INT64);
    field->set_label(google::protobuf::FieldDescriptorProto::LABEL_OPTIONAL);
    ASSERT_THAT(pool.BuildFile(file_desc_proto), NotNull());
  }
  const google::protobuf::Descriptor* desc = pool.FindMessageTypeByName("test.Struct");
  ASSERT_THAT(desc, NotNull());
  const google::protobuf::FieldDescriptor* field_desc = desc->FindFieldByName("foo");
  ASSERT_THAT(desc, NotNull());
  MessageTypeField message_type_field(field_desc);
  EXPECT_TRUE(message_type_field);
  EXPECT_THAT(message_type_field.name(), Eq("foo"));
  EXPECT_THAT(message_type_field.DebugString(), StartsWith("[1]foo@0x"));
  EXPECT_THAT(message_type_field.number(), Eq(1));
  EXPECT_THAT(message_type_field.GetType(), IntType());
  EXPECT_EQ(cel::to_address(message_type_field), field_desc);
  StructTypeField struct_type_field = message_type_field;
  EXPECT_TRUE(struct_type_field.IsMessage());
  EXPECT_THAT(struct_type_field.AsMessage(), Optional(An<MessageTypeField>()));
  EXPECT_THAT(static_cast<MessageTypeField>(struct_type_field),
              An<MessageTypeField>());
  EXPECT_EQ(struct_type_field.name(), message_type_field.name());
  EXPECT_EQ(struct_type_field.number(), message_type_field.number());
  EXPECT_EQ(struct_type_field.GetType(), message_type_field.GetType());
}

}  // namespace
}  // namespace cel
