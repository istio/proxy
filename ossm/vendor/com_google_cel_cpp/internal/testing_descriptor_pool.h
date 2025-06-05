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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_DESCRIPTOR_POOL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_DESCRIPTOR_POOL_H_

#include <memory>

#include "absl/base/nullability.h"
#include "google/protobuf/descriptor.h"

namespace cel::internal {

// GetTestingDescriptorPool returns a pointer to a `google::protobuf::DescriptorPool`
// which includes has the necessary descriptors required for the purposes of
// testing. The returning `google::protobuf::DescriptorPool` is valid for the lifetime of
// the process.
absl::Nonnull<const google::protobuf::DescriptorPool*> GetTestingDescriptorPool();
absl::Nonnull<std::shared_ptr<const google::protobuf::DescriptorPool>>
GetSharedTestingDescriptorPool();

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_TESTING_DESCRIPTOR_POOL_H_
