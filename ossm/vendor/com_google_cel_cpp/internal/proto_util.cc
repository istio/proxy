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

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "internal/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

absl::Status ValidateStandardMessageTypes(
    const google::protobuf::DescriptorPool& descriptor_pool) {
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Any>(descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::BoolValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::BytesValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::DoubleValue>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Duration>(descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::FloatValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::Int32Value>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::Int64Value>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::ListValue>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::StringValue>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Struct>(descriptor_pool));
  CEL_RETURN_IF_ERROR(ValidateStandardMessageType<google::protobuf::Timestamp>(
      descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::UInt32Value>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::UInt64Value>(
          descriptor_pool));
  CEL_RETURN_IF_ERROR(
      ValidateStandardMessageType<google::protobuf::Value>(descriptor_pool));
  return absl::OkStatus();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
