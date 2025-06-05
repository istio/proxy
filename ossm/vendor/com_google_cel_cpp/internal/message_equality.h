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
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory);

// Tests whether one message field is equal to another following CEL equality
// semantics.
absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> lhs_field,
    const google::protobuf::Message& rhs,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> rhs_field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory);
absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs, const google::protobuf::Message& rhs,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> rhs_field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory);
absl::StatusOr<bool> MessageFieldEquals(
    const google::protobuf::Message& lhs,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> lhs_field,
    const google::protobuf::Message& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MESSAGE_EQUALITY_H_
