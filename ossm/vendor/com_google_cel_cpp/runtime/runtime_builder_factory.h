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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_FACTORY_H_

#include <memory>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Create an unconfigured builder using the default Runtime implementation.
//
// The provided descriptor pool is used when dealing with `google.protobuf.Any`
// messages, as well as for implementing struct creation syntax
// `foo.Bar{my_field: 1}`. The descriptor pool must outlive the resulting
// RuntimeBuilder, the `Runtime` it creates, and any `Program` that the
// `Runtime` creates. The descriptor pool must include the minimally necessary
// descriptors required by CEL. Those are the following:
// - google.protobuf.NullValue
// - google.protobuf.BoolValue
// - google.protobuf.Int32Value
// - google.protobuf.Int64Value
// - google.protobuf.UInt32Value
// - google.protobuf.UInt64Value
// - google.protobuf.FloatValue
// - google.protobuf.DoubleValue
// - google.protobuf.BytesValue
// - google.protobuf.StringValue
// - google.protobuf.Any
// - google.protobuf.Duration
// - google.protobuf.Timestamp
//
// This is provided for environments that only use a subset of the CEL standard
// builtins. Most users should prefer CreateStandardRuntimeBuilder.
//
// Callers must register appropriate builtins.
absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool
        ABSL_ATTRIBUTE_LIFETIME_BOUND,
    const RuntimeOptions& options);
absl::StatusOr<RuntimeBuilder> CreateRuntimeBuilder(
    absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool,
    const RuntimeOptions& options);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_BUILDER_FACTORY_H_
