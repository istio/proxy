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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_DESCRIPTOR_POOL_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_DESCRIPTOR_POOL_BUILDER_H_

#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/descriptor.h"
#include "absl/status/status.h"

namespace google::api::expr::runtime {

// Add standard message types required by CEL to given descriptor pool.
// This includes standard wrappers, timestamp, duration, any, etc.
// This does not work for descriptor pools that have a fallback database.
// Use GetStandardMessageTypesFileDescriptorSet() below instead to populate.
absl::Status AddStandardMessageTypesToDescriptorPool(
    google::protobuf::DescriptorPool& descriptor_pool);

// Get the standard message types required by CEL.
// This includes standard wrappers, timestamp, duration, any, etc. These can be
// used to, e.g., add them to a DescriptorDatabase backing a DescriptorPool.
google::protobuf::FileDescriptorSet GetStandardMessageTypesFileDescriptorSet();

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_CEL_PROTO_DESCRIPTOR_POOL_BUILDER_H_
