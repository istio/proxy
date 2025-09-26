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
#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_FACTORY_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "checker/checker_options.h"
#include "checker/type_checker_builder.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Creates a new `TypeCheckerBuilder`.
//
// The builder implementation is thread-hostile and should only be used from a
// single thread, but the resulting `TypeChecker` instance is thread-safe.
//
// When passing a raw pointer to a descriptor pool, the descriptor pool must
// outlive the type checker builder and the type checker builder it creates.
//
// The descriptor pool must include the minimally necessary
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
absl::StatusOr<std::unique_ptr<TypeCheckerBuilder>> CreateTypeCheckerBuilder(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    const CheckerOptions& options = {});
absl::StatusOr<std::unique_ptr<TypeCheckerBuilder>> CreateTypeCheckerBuilder(
    absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool,
    const CheckerOptions& options = {});

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_FACTORY_H_
