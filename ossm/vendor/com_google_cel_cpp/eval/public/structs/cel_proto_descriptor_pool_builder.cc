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

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/container/flat_hash_map.h"
#include "internal/proto_util.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
namespace {
template <class MessageType>
absl::Status AddOrValidateMessageType(google::protobuf::DescriptorPool& descriptor_pool) {
  const google::protobuf::Descriptor* descriptor = MessageType::descriptor();
  if (descriptor_pool.FindMessageTypeByName(descriptor->full_name()) !=
      nullptr) {
    return internal::ValidateStandardMessageType<MessageType>(descriptor_pool);
  }
  google::protobuf::FileDescriptorProto file_descriptor_proto;
  descriptor->file()->CopyTo(&file_descriptor_proto);
  if (descriptor_pool.BuildFile(file_descriptor_proto) == nullptr) {
    return absl::InternalError(
        absl::StrFormat("Failed to add descriptor '%s' to descriptor pool",
                        descriptor->full_name()));
  }
  return absl::OkStatus();
}

template <class MessageType>
void AddStandardMessageTypeToMap(
    absl::flat_hash_map<std::string, google::protobuf::FileDescriptorProto>& fdmap) {
  const google::protobuf::Descriptor* descriptor = MessageType::descriptor();

  if (fdmap.contains(descriptor->file()->name())) return;

  descriptor->file()->CopyTo(&fdmap[descriptor->file()->name()]);
}

}  // namespace

absl::Status AddStandardMessageTypesToDescriptorPool(
    google::protobuf::DescriptorPool& descriptor_pool) {
  // The types below do not depend on each other, hence we can add them in any
  // order. Should that change with new messages add them in the proper order,
  // i.e., dependencies first.
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Any>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::BoolValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::BytesValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::DoubleValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Duration>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::FloatValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Int32Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Int64Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::ListValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::StringValue>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Struct>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Timestamp>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::UInt32Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::UInt64Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Value>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::FieldMask>(descriptor_pool));
  CEL_RETURN_IF_ERROR(
      AddOrValidateMessageType<google::protobuf::Empty>(descriptor_pool));
  return absl::OkStatus();
}

google::protobuf::FileDescriptorSet GetStandardMessageTypesFileDescriptorSet() {
  // The types below do not depend on each other, hence we can add them to
  // an unordered map. Should that change with new messages being added here
  // adapt this to a sorted data structure and add in the proper order.
  absl::flat_hash_map<std::string, google::protobuf::FileDescriptorProto> files;
  AddStandardMessageTypeToMap<google::protobuf::Any>(files);
  AddStandardMessageTypeToMap<google::protobuf::BoolValue>(files);
  AddStandardMessageTypeToMap<google::protobuf::BytesValue>(files);
  AddStandardMessageTypeToMap<google::protobuf::DoubleValue>(files);
  AddStandardMessageTypeToMap<google::protobuf::Duration>(files);
  AddStandardMessageTypeToMap<google::protobuf::FloatValue>(files);
  AddStandardMessageTypeToMap<google::protobuf::Int32Value>(files);
  AddStandardMessageTypeToMap<google::protobuf::Int64Value>(files);
  AddStandardMessageTypeToMap<google::protobuf::ListValue>(files);
  AddStandardMessageTypeToMap<google::protobuf::StringValue>(files);
  AddStandardMessageTypeToMap<google::protobuf::Struct>(files);
  AddStandardMessageTypeToMap<google::protobuf::Timestamp>(files);
  AddStandardMessageTypeToMap<google::protobuf::UInt32Value>(files);
  AddStandardMessageTypeToMap<google::protobuf::UInt64Value>(files);
  AddStandardMessageTypeToMap<google::protobuf::Value>(files);
  AddStandardMessageTypeToMap<google::protobuf::FieldMask>(files);
  AddStandardMessageTypeToMap<google::protobuf::Empty>(files);
  google::protobuf::FileDescriptorSet fdset;
  for (const auto& [name, fdproto] : files) {
    *fdset.add_file() = fdproto;
  }
  return fdset;
}

}  // namespace google::api::expr::runtime
