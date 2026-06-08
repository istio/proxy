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
//
// Converters to/from versioned Decl protos to the equivalent CEL C++ types.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_DECL_PROTO_V1ALPHA1_H_
#define THIRD_PARTY_CEL_CPP_COMMON_DECL_PROTO_V1ALPHA1_H_

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/decl.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Creates a VariableDecl from a google.api.expr.v1alpha1.Decl.IdentDecl proto.
absl::StatusOr<VariableDecl> VariableDeclFromV1Alpha1Proto(
    absl::string_view name,
    const google::api::expr::v1alpha1::Decl::IdentDecl& variable,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena);

// Creates a FunctionDecl from a google.api.expr.v1alpha1.Decl.FunctionDecl
// proto.
absl::StatusOr<FunctionDecl> FunctionDeclFromV1Alpha1Proto(
    absl::string_view name,
    const google::api::expr::v1alpha1::Decl::FunctionDecl& function,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena);

// Creates a VariableDecl or FunctionDecl from a google.api.expr.v1alpha1.Decl
// proto.
absl::StatusOr<absl::variant<VariableDecl, FunctionDecl>> DeclFromV1Alpha1Proto(
    const google::api::expr::v1alpha1::Decl& decl,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_DECL_PROTO_V1ALPHA1_H_
