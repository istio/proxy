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

#ifndef THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_FACTORY_H_

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "compiler/compiler.h"
#include "internal/noop_delete.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Creates a new unconfigured CompilerBuilder for creating a new CEL Compiler
// instance.
//
// The builder is thread-hostile and intended to be configured by a single
// thread, but the created Compiler instances are thread-compatible (and
// effectively immutable).
//
// The descriptor pool must include the standard definitions for the protobuf
// well-known types:
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
absl::StatusOr<std::unique_ptr<CompilerBuilder>> NewCompilerBuilder(
    std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool,
    CompilerOptions options = {});

// Convenience overload for non-owning pointers (such as the generated pool).
// The descriptor pool must outlive the compiler builder and any compiler
// instances it builds.
inline absl::StatusOr<std::unique_ptr<CompilerBuilder>> NewCompilerBuilder(
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    CompilerOptions options = {}) {
  return NewCompilerBuilder(
      std::shared_ptr<const google::protobuf::DescriptorPool>(
          descriptor_pool,
          internal::NoopDeleteFor<const google::protobuf::DescriptorPool>()),
      std::move(options));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMPILER_COMPILER_FACTORY_H_
