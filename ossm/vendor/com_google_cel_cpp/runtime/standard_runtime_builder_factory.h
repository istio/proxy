// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_RUNTIME_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_RUNTIME_BUILDER_FACTORY_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Create a builder preconfigured with CEL standard definitions.
//
// See `CreateRuntimeBuilder` for a description of the requirements related to
// `descriptor_pool`.
absl::StatusOr<RuntimeBuilder> CreateStandardRuntimeBuilder(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const RuntimeOptions& options);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_RUNTIME_BUILDER_FACTORY_H_
