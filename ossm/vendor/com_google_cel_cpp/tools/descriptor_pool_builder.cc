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

#include "tools/descriptor_pool_builder.h"

#include <memory>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "common/minimal_descriptor_database.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"

namespace cel {

namespace {

absl::Status FindDeps(
    std::vector<const google::protobuf::FileDescriptor*>& to_resolve,
    absl::flat_hash_set<const google::protobuf::FileDescriptor*>& resolved,
    DescriptorPoolBuilder& builder) {
  while (!to_resolve.empty()) {
    const auto* file = to_resolve.back();
    to_resolve.pop_back();
    if (resolved.contains(file)) {
      continue;
    }
    google::protobuf::FileDescriptorProto file_proto;
    file->CopyTo(&file_proto);
    // Note: order doesn't matter here as long as all the cross references are
    // correct in the final database.
    CEL_RETURN_IF_ERROR(builder.AddFileDescriptor(file_proto));
    resolved.insert(file);
    for (int i = 0; i < file->dependency_count(); ++i) {
      to_resolve.push_back(file->dependency(i));
    }
  }
  return absl::OkStatus();
}

}  // namespace

DescriptorPoolBuilder::StateHolder::StateHolder(
    google::protobuf::DescriptorDatabase* base)
    : base(base), merged(base, &extensions), pool(&merged) {}

DescriptorPoolBuilder::DescriptorPoolBuilder()
    : state_(std::make_shared<DescriptorPoolBuilder::StateHolder>(
          cel::GetMinimalDescriptorDatabase())) {}

std::shared_ptr<const google::protobuf::DescriptorPool>
DescriptorPoolBuilder::Build() && {
  auto alias =
      std::shared_ptr<const google::protobuf::DescriptorPool>(state_, &state_->pool);
  state_.reset();
  return alias;
}

absl::Status DescriptorPoolBuilder::AddTransitiveDescriptorSet(
    const google::protobuf::Descriptor* absl_nonnull desc) {
  absl::flat_hash_set<const google::protobuf::FileDescriptor*> resolved;
  std::vector<const google::protobuf::FileDescriptor*> to_resolve{desc->file()};
  return FindDeps(to_resolve, resolved, *this);
}

absl::Status DescriptorPoolBuilder::AddTransitiveDescriptorSet(
    absl::Span<const google::protobuf::Descriptor* absl_nonnull> descs) {
  absl::flat_hash_set<const google::protobuf::FileDescriptor*> resolved;
  std::vector<const google::protobuf::FileDescriptor* absl_nonnull> to_resolve;
  to_resolve.reserve(descs.size());
  for (const google::protobuf::Descriptor* desc : descs) {
    to_resolve.push_back(desc->file());
  }

  return FindDeps(to_resolve, resolved, *this);
}

absl::Status DescriptorPoolBuilder::AddFileDescriptor(
    const google::protobuf::FileDescriptorProto& file) {
  if (!state_->extensions.Add(file)) {
    return absl::InvalidArgumentError(
        absl::StrCat("proto descriptor conflict: ", file.name()));
  }
  return absl::OkStatus();
}

absl::Status DescriptorPoolBuilder::AddFileDescriptorSet(
    const google::protobuf::FileDescriptorSet& file) {
  for (const auto& file : file.file()) {
    CEL_RETURN_IF_ERROR(AddFileDescriptor(file));
  }
  return absl::OkStatus();
}

}  // namespace cel
