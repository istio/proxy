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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_H_

#include "cel/expr/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Creates a Type from a google.api.expr.Type proto.
absl::StatusOr<Type> TypeFromProto(
    const cel::expr::Type& type_pb,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena);

absl::Status TypeToProto(const Type& type,
                         cel::expr::Type* absl_nonnull type_pb);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_PROTO_H_
