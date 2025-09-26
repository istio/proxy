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

#include "common/decl_proto.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_proto.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

absl::StatusOr<VariableDecl> VariableDeclFromProto(
    absl::string_view name, const cel::expr::Decl::IdentDecl& variable,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  CEL_ASSIGN_OR_RETURN(Type type,
                       TypeFromProto(variable.type(), descriptor_pool, arena));
  return cel::MakeVariableDecl(std::string(name), type);
}

absl::StatusOr<FunctionDecl> FunctionDeclFromProto(
    absl::string_view name,
    const cel::expr::Decl::FunctionDecl& function,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  cel::FunctionDecl decl;
  decl.set_name(name);
  for (const auto& overload_pb : function.overloads()) {
    cel::OverloadDecl ovl_decl;
    ovl_decl.set_id(overload_pb.overload_id());
    ovl_decl.set_member(overload_pb.is_instance_function());
    CEL_ASSIGN_OR_RETURN(
        cel::Type result,
        TypeFromProto(overload_pb.result_type(), descriptor_pool, arena));
    ovl_decl.set_result(result);
    std::vector<cel::Type> param_types;
    param_types.reserve(overload_pb.params_size());
    for (const auto& param_type_pb : overload_pb.params()) {
      CEL_ASSIGN_OR_RETURN(
          param_types.emplace_back(),
          TypeFromProto(param_type_pb, descriptor_pool, arena));
    }
    ovl_decl.mutable_args() = std::move(param_types);
    CEL_RETURN_IF_ERROR(decl.AddOverload(std::move(ovl_decl)));
  }
  return decl;
}

absl::StatusOr<absl::variant<VariableDecl, FunctionDecl>> DeclFromProto(
    const cel::expr::Decl& decl,
    const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool,
    google::protobuf::Arena* absl_nonnull arena) {
  if (decl.has_ident()) {
    return VariableDeclFromProto(decl.name(), decl.ident(), descriptor_pool,
                                 arena);
  } else if (decl.has_function()) {
    return FunctionDeclFromProto(decl.name(), decl.function(), descriptor_pool,
                                 arena);
  }
  return absl::InvalidArgumentError("empty google.api.expr.Decl proto");
}

}  // namespace cel
