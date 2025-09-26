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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_EQUALITY_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_EQUALITY_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::internal {

// Tests whether one message is equal to another following CEL equality
// semantics.
absl::StatusOr<bool> MessageEquals(
    const google::protobuf::Message& lhs, const google::protobuf::Message& rhs,
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory);

// Tests whether one message field is equal to another following CEL equality
// semantics.
absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs,
    const google::protobuf::FieldDescriptor* absl_nonnull lhs_field,
    const google::protobuf::Message& rhs,
    const google::protobuf::FieldDescriptor* absl_nonnull rhs_field,
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory);
absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs, const google::protobuf::Message& rhs,
    const google::protobuf::FieldDescriptor* absl_nonnull rhs_field,
    const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory);
absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs,
    const google::protobuf::FieldDescriptor* absl_nonnull lhs_field,
    const google::protobuf::Message& rhs, const google::protobuf::DescriptorPool* absl_nonnull pool,
    google::protobuf::MessageFactory* absl_nonnull factory);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_EQUALITY_H_
