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
#include "common/decl_proto_v1alpha1.h"

#include "cel/expr/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/decl.h"
#include "common/decl_proto.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

absl::StatusOr<VariableDecl> VariableDeclFromV1Alpha1Proto(
    absl::string_view name,
    const google::api::expr::v1alpha1::Decl::IdentDecl& variable,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  cel::expr::Decl::IdentDecl unversioned;
  if (!unversioned.MergeFromString(variable.SerializeAsString())) {
    return absl::InternalError(
        "failed to convert versioned to unversioned Decl proto");
  }
  return VariableDeclFromProto(name, unversioned, descriptor_pool, arena);
}

absl::StatusOr<FunctionDecl> FunctionDeclFromV1Alpha1Proto(
    absl::string_view name,
    const google::api::expr::v1alpha1::Decl::FunctionDecl& function,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  cel::expr::Decl::FunctionDecl unversioned;
  if (!unversioned.MergeFromString(function.SerializeAsString())) {
    return absl::InternalError(
        "failed to convert versioned to unversioned Decl proto");
  }
  return FunctionDeclFromProto(name, unversioned, descriptor_pool, arena);
}

absl::StatusOr<absl::variant<VariableDecl, FunctionDecl>> DeclFromV1Alpha1Proto(
    const google::api::expr::v1alpha1::Decl& decl,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  cel::expr::Decl unversioned;
  if (!unversioned.MergeFromString(decl.SerializeAsString())) {
    return absl::InternalError(
        "failed to convert versioned to unversioned Decl proto");
  }
  return DeclFromProto(unversioned, descriptor_pool, arena);
}

}  // namespace cel
