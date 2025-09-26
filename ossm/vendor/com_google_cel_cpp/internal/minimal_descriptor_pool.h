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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_MINIMAL_DESCRIPTOR_POOL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_MINIMAL_DESCRIPTOR_POOL_H_

#include "absl/base/nullability.h"
#include "google/protobuf/descriptor.h"

namespace cel::internal {

// GetMinimalDescriptorPool returns a pointer to a `google::protobuf::DescriptorPool`
// which includes has the minimally necessary descriptors required by the Common
// Expression Language. The returning `google::protobuf::DescriptorPool` is valid for the
// lifetime of the process.
//
// This descriptor pool can be used as an underlay for another descriptor pool:
//
// google::protobuf::DescriptorPool my_descriptor_pool(GetMinimalDescriptorPool());
const google::protobuf::DescriptorPool* absl_nonnull GetMinimalDescriptorPool();

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_MINIMAL_DESCRIPTOR_POOL_H_
