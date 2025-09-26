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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_

#include <string>
#include <type_traits>

#include "google/protobuf/descriptor.pb.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

template <class MessageType>
absl::Status ValidateStandardMessageType(
    const google::protobuf::DescriptorPool& descriptor_pool) {
  if constexpr (std::is_base_of_v<google::protobuf::Message, MessageType>) {
    const google::protobuf::Descriptor* descriptor = MessageType::descriptor();
    const google::protobuf::Descriptor* descriptor_from_pool =
        descriptor_pool.FindMessageTypeByName(descriptor->full_name());
    if (descriptor_from_pool == nullptr) {
      return absl::NotFoundError(
          absl::StrFormat("Descriptor '%s' not found in descriptor pool",
                          descriptor->full_name()));
    }
    if (descriptor_from_pool == descriptor) {
      return absl::OkStatus();
    }
    google::protobuf::DescriptorProto descriptor_proto;
    google::protobuf::DescriptorProto descriptor_from_pool_proto;
    descriptor->CopyTo(&descriptor_proto);
    descriptor_from_pool->CopyTo(&descriptor_from_pool_proto);

    google::protobuf::util::MessageDifferencer descriptor_differencer;
    std::string differences;
    descriptor_differencer.ReportDifferencesToString(&differences);
    // The json_name is a compiler detail and does not change the message
    // content. It can differ, e.g., between C++ and Go compilers. Hence ignore.
    const google::protobuf::FieldDescriptor* json_name_field_desc =
        google::protobuf::FieldDescriptorProto::descriptor()->FindFieldByName(
            "json_name");
    if (json_name_field_desc != nullptr) {
      descriptor_differencer.IgnoreField(json_name_field_desc);
    }
    if (!descriptor_differencer.Compare(descriptor_proto,
                                        descriptor_from_pool_proto)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "The descriptor for '%s' in the descriptor pool differs from the "
          "compiled-in generated version as follows: %s",
          descriptor->full_name(), differences));
    }
  } else {
    // Lite runtime. Just verify the message exists.
    const auto& type_name = MessageType::default_instance().GetTypeName();
    const google::protobuf::Descriptor* descriptor_from_pool =
        descriptor_pool.FindMessageTypeByName(type_name);
    if (descriptor_from_pool == nullptr) {
      return absl::NotFoundError(absl::StrFormat(
          "Descriptor '%s' not found in descriptor pool", type_name));
    }
  }
  return absl::OkStatus();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_
