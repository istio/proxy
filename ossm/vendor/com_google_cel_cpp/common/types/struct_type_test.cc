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

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/nullability.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

using ::testing::Test;

class StructTypeTest : public Test {
 public:
  void SetUp() override {
    {
      google::protobuf::FileDescriptorProto file_desc_proto;
      file_desc_proto.set_syntax("proto3");
      file_desc_proto.set_package("test");
      file_desc_proto.set_name("test/struct.proto");
      file_desc_proto.add_message_type()->set_name("Struct");
      ABSL_CHECK(pool_.BuildFile(file_desc_proto) != nullptr);
    }
  }

  const google::protobuf::Descriptor* absl_nonnull GetDescriptor() const {
    return ABSL_DIE_IF_NULL(pool_.FindMessageTypeByName("test.Struct"));
  }

  MessageType GetMessageType() const { return MessageType(GetDescriptor()); }

  common_internal::BasicStructType GetBasicStructType() const {
    return common_internal::MakeBasicStructType("test.Struct");
  }

 private:
  google::protobuf::DescriptorPool pool_;
};

TEST(StructType, Kind) { EXPECT_EQ(StructType::kind(), TypeKind::kStruct); }

TEST_F(StructTypeTest, Name) {
  EXPECT_EQ(StructType(GetMessageType()).name(), GetMessageType().name());
  EXPECT_EQ(StructType(GetBasicStructType()).name(),
            GetBasicStructType().name());
}

TEST_F(StructTypeTest, DebugString) {
  EXPECT_EQ(StructType(GetMessageType()).DebugString(),
            GetMessageType().DebugString());
  EXPECT_EQ(StructType(GetBasicStructType()).DebugString(),
            GetBasicStructType().DebugString());
}

TEST_F(StructTypeTest, Hash) {
  EXPECT_EQ(absl::HashOf(StructType(GetMessageType())),
            absl::HashOf(StructType(GetBasicStructType())));
}

TEST_F(StructTypeTest, Equal) {
  EXPECT_EQ(StructType(GetMessageType()), StructType(GetBasicStructType()));
}

}  // namespace
}  // namespace cel
