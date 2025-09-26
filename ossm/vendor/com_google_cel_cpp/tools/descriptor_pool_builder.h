// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_DESCRIPTOR_POOL_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_DESCRIPTOR_POOL_BUILDER_H_

#include <memory>
#include <utility>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"

namespace cel {

// A helper class for building a descriptor pool from a set proto file
// descriptors. Manages lifetime for the descriptor databases backing
// the pool.
//
// Client must ensure that types are not added multiple times.
//
// Note: in the constructed pool, the definitions for the required types for
// CEL will shadow any added to the builder. Clients should not modify types
// from the google.protobuf package in general, but if they do the behavior of
// the constructed descriptor pool will be inconsistent.
class DescriptorPoolBuilder {
 public:
  DescriptorPoolBuilder();

  DescriptorPoolBuilder& operator=(const DescriptorPoolBuilder&) = delete;
  DescriptorPoolBuilder(const DescriptorPoolBuilder&) = delete;
  DescriptorPoolBuilder& operator=(const DescriptorPoolBuilder&&) = delete;
  DescriptorPoolBuilder(DescriptorPoolBuilder&&) = delete;

  ~DescriptorPoolBuilder() = default;

  // Returns a shared pointer to the new descriptor pool that manages the
  // underlying descriptor databases backing the pool.
  //
  // Consumes the builder instance. It is unsafe to make any further changes
  // to the descriptor databases after accessing the pool.
  std::shared_ptr<const google::protobuf::DescriptorPool> Build() &&;

  // Utility for adding the transitive dependencies of a message with a linked
  // descriptor.
  absl::Status AddTransitiveDescriptorSet(
      const google::protobuf::Descriptor* absl_nonnull desc);

  absl::Status AddTransitiveDescriptorSet(
      absl::Span<const google::protobuf::Descriptor* absl_nonnull>);

  // Adds a file descriptor set to the pool. Client must ensure that all
  // dependencies are satisfied and that files are not added multiple times.
  absl::Status AddFileDescriptorSet(const google::protobuf::FileDescriptorSet& files);

  // Adds a single proto file descriptor set to the pool. Client must ensure
  // that all dependencies are satisfied and that files are not added multiple
  // times.
  absl::Status AddFileDescriptor(const google::protobuf::FileDescriptorProto& file);

 private:
  struct StateHolder {
    explicit StateHolder(google::protobuf::DescriptorDatabase* base);

    google::protobuf::DescriptorDatabase* base;
    google::protobuf::SimpleDescriptorDatabase extensions;
    google::protobuf::MergedDescriptorDatabase merged;
    google::protobuf::DescriptorPool pool;
  };

  explicit DescriptorPoolBuilder(std::shared_ptr<StateHolder> state)
      : state_(std::move(state)) {}

  std::shared_ptr<StateHolder> state_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_DESCRIPTOR_POOL_BUILDER_H_
