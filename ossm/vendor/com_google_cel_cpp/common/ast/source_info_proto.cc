// Copyright 2022 Google LLC
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

#include "common/ast/source_info_proto.h"

#include <cstdint>
#include <utility>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "common/ast/expr.h"
#include "common/ast/expr_proto.h"
#include "internal/status_macros.h"

namespace cel::ast_internal {

using ::cel::ast_internal::ExprToProto;
using ::cel::ast_internal::Extension;
using ::cel::ast_internal::SourceInfo;

using ExprPb = cel::expr::Expr;
using ParsedExprPb = cel::expr::ParsedExpr;
using CheckedExprPb = cel::expr::CheckedExpr;
using ExtensionPb = cel::expr::SourceInfo::Extension;

absl::Status SourceInfoToProto(const SourceInfo& source_info,
                               cel::expr::SourceInfo* out) {
  cel::expr::SourceInfo& result = *out;
  result.set_syntax_version(source_info.syntax_version());
  result.set_location(source_info.location());

  for (int32_t line_offset : source_info.line_offsets()) {
    result.add_line_offsets(line_offset);
  }

  for (auto pos_iter = source_info.positions().begin();
       pos_iter != source_info.positions().end(); ++pos_iter) {
    (*result.mutable_positions())[pos_iter->first] = pos_iter->second;
  }

  for (auto macro_iter = source_info.macro_calls().begin();
       macro_iter != source_info.macro_calls().end(); ++macro_iter) {
    ExprPb& dest_macro = (*result.mutable_macro_calls())[macro_iter->first];
    CEL_RETURN_IF_ERROR(ExprToProto(macro_iter->second, &dest_macro));
  }

  for (const auto& extension : source_info.extensions()) {
    auto* extension_pb = result.add_extensions();
    extension_pb->set_id(extension.id());
    auto* version_pb = extension_pb->mutable_version();
    version_pb->set_major(extension.version().major());
    version_pb->set_minor(extension.version().minor());

    for (auto component : extension.affected_components()) {
      switch (component) {
        case Extension::Component::kParser:
          extension_pb->add_affected_components(ExtensionPb::COMPONENT_PARSER);
          break;
        case Extension::Component::kTypeChecker:
          extension_pb->add_affected_components(
              ExtensionPb::COMPONENT_TYPE_CHECKER);
          break;
        case Extension::Component::kRuntime:
          extension_pb->add_affected_components(ExtensionPb::COMPONENT_RUNTIME);
          break;
        default:
          extension_pb->add_affected_components(
              ExtensionPb::COMPONENT_UNSPECIFIED);
          break;
      }
    }
  }

  return absl::OkStatus();
}

}  // namespace cel::ast_internal
