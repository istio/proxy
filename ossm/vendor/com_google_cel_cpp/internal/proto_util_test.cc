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

#include "internal/proto_util.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/descriptor.h"
#include "eval/public/structs/cel_proto_descriptor_pool_builder.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

using google::api::expr::internal::ValidateStandardMessageType;
using google::api::expr::internal::ValidateStandardMessageTypes;
using google::api::expr::runtime::AddStandardMessageTypesToDescriptorPool;
using google::api::expr::runtime::GetStandardMessageTypesFileDescriptorSet;

using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

TEST(ProtoUtil, ValidateStandardMessageTypesOk) {
  google::protobuf::DescriptorPool descriptor_pool;
  ASSERT_OK(AddStandardMessageTypesToDescriptorPool(descriptor_pool));
  EXPECT_OK(ValidateStandardMessageTypes(descriptor_pool));
}

TEST(ProtoUtil, ValidateStandardMessageTypesRejectsMissing) {
  google::protobuf::DescriptorPool descriptor_pool;
  EXPECT_THAT(ValidateStandardMessageTypes(descriptor_pool),
              StatusIs(absl::StatusCode::kNotFound,
                       HasSubstr("not found in descriptor pool")));
}

TEST(ProtoUtil, ValidateStandardMessageTypesRejectsIncompatible) {
  google::protobuf::DescriptorPool descriptor_pool;
  google::protobuf::FileDescriptorSet standard_fds =
      GetStandardMessageTypesFileDescriptorSet();

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
      ValidateStandardMessageType<google::protobuf::Duration>(descriptor_pool),
      StatusIs(absl::StatusCode::kFailedPrecondition, HasSubstr("differs")));
}

TEST(ProtoUtil, ValidateStandardMessageTypesIgnoredJsonName) {
  google::protobuf::DescriptorPool descriptor_pool;
  google::protobuf::FileDescriptorSet standard_fds =
      GetStandardMessageTypesFileDescriptorSet();
  bool modified = false;
  // This nested loops are used to find the field descriptor proto to modify the
  // json_name field of.
  for (int i = 0; i < standard_fds.file_size(); ++i) {
    if (standard_fds.file(i).name() == "google/protobuf/duration.proto") {
      google::protobuf::FileDescriptorProto* fdp = standard_fds.mutable_file(i);
      for (int j = 0; j < fdp->message_type_size(); ++j) {
        if (fdp->message_type(j).name() == "Duration") {
          google::protobuf::DescriptorProto* dp = fdp->mutable_message_type(j);
          for (int k = 0; k < dp->field_size(); ++k) {
            if (dp->field(k).name() == "seconds") {
              // we need to set this to something we are reasonable sure of that
              // it won't be set for real to make sure it is ignored
              dp->mutable_field(k)->set_json_name("FOOBAR");
              modified = true;
            }
          }
        }
      }
    }
  }
  ASSERT_TRUE(modified);

  for (int i = 0; i < standard_fds.file_size(); ++i) {
    descriptor_pool.BuildFile(standard_fds.file(i));
  }

  EXPECT_OK(ValidateStandardMessageTypes(descriptor_pool));
}

}  // namespace
}  // namespace cel::internal
