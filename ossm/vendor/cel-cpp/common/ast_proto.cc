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

#include "common/ast_proto.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "cel/expr/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/variant.h"
#include "common/ast.h"
#include "common/ast/constant_proto.h"
#include "common/ast/expr_proto.h"
#include "common/ast/source_info_proto.h"
#include "common/constant.h"
#include "common/expr.h"
#include "internal/status_macros.h"

namespace cel {
namespace {

using ::cel::ast_internal::ConstantFromProto;
using ::cel::ast_internal::ConstantToProto;
using ::cel::ast_internal::ExprFromProto;
using ::cel::ast_internal::ExprToProto;

using ExprPb = cel::expr::Expr;
using ParsedExprPb = cel::expr::ParsedExpr;
using CheckedExprPb = cel::expr::CheckedExpr;
using SourceInfoPb = cel::expr::SourceInfo;
using ExtensionPb = cel::expr::SourceInfo::Extension;
using ReferencePb = cel::expr::Reference;
using TypePb = cel::expr::Type;
using ExtensionPb = cel::expr::SourceInfo::Extension;

absl::StatusOr<Expr> ExprValueFromProto(const ExprPb& expr) {
  Expr result;
  CEL_RETURN_IF_ERROR(ExprFromProto(expr, result));
  return result;
}

absl::StatusOr<SourceInfo> ConvertProtoSourceInfoToNative(
    const cel::expr::SourceInfo& source_info) {
  absl::flat_hash_map<int64_t, Expr> macro_calls;
  for (const auto& pair : source_info.macro_calls()) {
    auto native_expr = ExprValueFromProto(pair.second);
    if (!native_expr.ok()) {
      return native_expr.status();
    }
    macro_calls.emplace(pair.first, *(std::move(native_expr)));
  }
  std::vector<ExtensionSpec> extensions;
  extensions.reserve(source_info.extensions_size());
  for (const auto& extension : source_info.extensions()) {
    std::vector<ExtensionSpec::Component> components;
    components.reserve(extension.affected_components().size());
    for (const auto& component : extension.affected_components()) {
      switch (component) {
        case ExtensionPb::COMPONENT_PARSER:
          components.push_back(ExtensionSpec::Component::kParser);
          break;
        case ExtensionPb::COMPONENT_TYPE_CHECKER:
          components.push_back(ExtensionSpec::Component::kTypeChecker);
          break;
        case ExtensionPb::COMPONENT_RUNTIME:
          components.push_back(ExtensionSpec::Component::kRuntime);
          break;
        default:
          components.push_back(ExtensionSpec::Component::kUnspecified);
          break;
      }
    }
    extensions.push_back(ExtensionSpec(
        extension.id(),
        std::make_unique<ExtensionSpec::Version>(extension.version().major(),
                                                 extension.version().minor()),
        std::move(components)));
  }
  return SourceInfo(
      source_info.syntax_version(), source_info.location(),
      std::vector<int32_t>(source_info.line_offsets().begin(),
                           source_info.line_offsets().end()),
      absl::flat_hash_map<int64_t, int32_t>(source_info.positions().begin(),
                                            source_info.positions().end()),
      std::move(macro_calls), std::move(extensions));
}

absl::StatusOr<TypeSpec> ConvertProtoTypeToNative(
    const cel::expr::Type& type);

absl::StatusOr<PrimitiveType> ToNative(
    cel::expr::Type::PrimitiveType primitive_type) {
  switch (primitive_type) {
    case cel::expr::Type::PRIMITIVE_TYPE_UNSPECIFIED:
      return PrimitiveType::kPrimitiveTypeUnspecified;
    case cel::expr::Type::BOOL:
      return PrimitiveType::kBool;
    case cel::expr::Type::INT64:
      return PrimitiveType::kInt64;
    case cel::expr::Type::UINT64:
      return PrimitiveType::kUint64;
    case cel::expr::Type::DOUBLE:
      return PrimitiveType::kDouble;
    case cel::expr::Type::STRING:
      return PrimitiveType::kString;
    case cel::expr::Type::BYTES:
      return PrimitiveType::kBytes;
    default:
      return absl::InvalidArgumentError(
          "Illegal type specified for "
          "cel::expr::Type::PrimitiveType.");
  }
}

absl::StatusOr<WellKnownTypeSpec> ToNative(
    cel::expr::Type::WellKnownType well_known_type) {
  switch (well_known_type) {
    case cel::expr::Type::WELL_KNOWN_TYPE_UNSPECIFIED:
      return WellKnownTypeSpec::kWellKnownTypeUnspecified;
    case cel::expr::Type::ANY:
      return WellKnownTypeSpec::kAny;
    case cel::expr::Type::TIMESTAMP:
      return WellKnownTypeSpec::kTimestamp;
    case cel::expr::Type::DURATION:
      return WellKnownTypeSpec::kDuration;
    default:
      return absl::InvalidArgumentError(
          "Illegal type specified for "
          "cel::expr::Type::WellKnownType.");
  }
}

absl::StatusOr<ListTypeSpec> ToNative(
    const cel::expr::Type::ListType& list_type) {
  auto native_elem_type = ConvertProtoTypeToNative(list_type.elem_type());
  if (!native_elem_type.ok()) {
    return native_elem_type.status();
  }
  return ListTypeSpec(
      std::make_unique<TypeSpec>(*(std::move(native_elem_type))));
}

absl::StatusOr<MapTypeSpec> ToNative(
    const cel::expr::Type::MapType& map_type) {
  auto native_key_type = ConvertProtoTypeToNative(map_type.key_type());
  if (!native_key_type.ok()) {
    return native_key_type.status();
  }
  auto native_value_type = ConvertProtoTypeToNative(map_type.value_type());
  if (!native_value_type.ok()) {
    return native_value_type.status();
  }
  return MapTypeSpec(
      std::make_unique<TypeSpec>(*(std::move(native_key_type))),
      std::make_unique<TypeSpec>(*(std::move(native_value_type))));
}

absl::StatusOr<FunctionTypeSpec> ToNative(
    const cel::expr::Type::FunctionType& function_type) {
  std::vector<TypeSpec> arg_types;
  arg_types.reserve(function_type.arg_types_size());
  for (const auto& arg_type : function_type.arg_types()) {
    auto native_arg = ConvertProtoTypeToNative(arg_type);
    if (!native_arg.ok()) {
      return native_arg.status();
    }
    arg_types.emplace_back(*(std::move(native_arg)));
  }
  auto native_result = ConvertProtoTypeToNative(function_type.result_type());
  if (!native_result.ok()) {
    return native_result.status();
  }
  return FunctionTypeSpec(
      std::make_unique<TypeSpec>(*(std::move(native_result))),
      std::move(arg_types));
}

absl::StatusOr<AbstractType> ToNative(
    const cel::expr::Type::AbstractType& abstract_type) {
  std::vector<TypeSpec> parameter_types;
  for (const auto& parameter_type : abstract_type.parameter_types()) {
    auto native_parameter_type = ConvertProtoTypeToNative(parameter_type);
    if (!native_parameter_type.ok()) {
      return native_parameter_type.status();
    }
    parameter_types.emplace_back(*(std::move(native_parameter_type)));
  }
  return AbstractType(abstract_type.name(), std::move(parameter_types));
}

absl::StatusOr<TypeSpec> ConvertProtoTypeToNative(
    const cel::expr::Type& type) {
  switch (type.type_kind_case()) {
    case cel::expr::Type::kDyn:
      return TypeSpec(DynTypeSpec());
    case cel::expr::Type::kNull:
      return TypeSpec(NullTypeSpec());
    case cel::expr::Type::kPrimitive: {
      auto native_primitive = ToNative(type.primitive());
      if (!native_primitive.ok()) {
        return native_primitive.status();
      }
      return TypeSpec(*(std::move(native_primitive)));
    }
    case cel::expr::Type::kWrapper: {
      auto native_wrapper = ToNative(type.wrapper());
      if (!native_wrapper.ok()) {
        return native_wrapper.status();
      }
      return TypeSpec(PrimitiveTypeWrapper(*(std::move(native_wrapper))));
    }
    case cel::expr::Type::kWellKnown: {
      auto native_well_known = ToNative(type.well_known());
      if (!native_well_known.ok()) {
        return native_well_known.status();
      }
      return TypeSpec(*std::move(native_well_known));
    }
    case cel::expr::Type::kListType: {
      auto native_list_type = ToNative(type.list_type());
      if (!native_list_type.ok()) {
        return native_list_type.status();
      }
      return TypeSpec(*(std::move(native_list_type)));
    }
    case cel::expr::Type::kMapType: {
      auto native_map_type = ToNative(type.map_type());
      if (!native_map_type.ok()) {
        return native_map_type.status();
      }
      return TypeSpec(*(std::move(native_map_type)));
    }
    case cel::expr::Type::kFunction: {
      auto native_function = ToNative(type.function());
      if (!native_function.ok()) {
        return native_function.status();
      }
      return TypeSpec(*(std::move(native_function)));
    }
    case cel::expr::Type::kMessageType:
      return TypeSpec(MessageTypeSpec(type.message_type()));
    case cel::expr::Type::kTypeParam:
      return TypeSpec(ParamTypeSpec(type.type_param()));
    case cel::expr::Type::kType: {
      if (type.type().type_kind_case() ==
          cel::expr::Type::TypeKindCase::TYPE_KIND_NOT_SET) {
        return TypeSpec(std::unique_ptr<TypeSpec>());
      }
      auto native_type = ConvertProtoTypeToNative(type.type());
      if (!native_type.ok()) {
        return native_type.status();
      }
      return TypeSpec(std::make_unique<TypeSpec>(*std::move(native_type)));
    }
    case cel::expr::Type::kError:
      return TypeSpec(ErrorTypeSpec::kValue);
    case cel::expr::Type::kAbstractType: {
      auto native_abstract = ToNative(type.abstract_type());
      if (!native_abstract.ok()) {
        return native_abstract.status();
      }
      return TypeSpec(*(std::move(native_abstract)));
    }
    case cel::expr::Type::TYPE_KIND_NOT_SET:
      return TypeSpec(UnsetTypeSpec());
    default:
      return absl::InvalidArgumentError(
          "Illegal type specified for cel::expr::Type.");
  }
}

absl::StatusOr<Reference> ConvertProtoReferenceToNative(
    const cel::expr::Reference& reference) {
  Reference ret_val;
  ret_val.set_name(reference.name());
  ret_val.mutable_overload_id().reserve(reference.overload_id_size());
  for (const auto& elem : reference.overload_id()) {
    ret_val.mutable_overload_id().emplace_back(elem);
  }
  if (reference.has_value()) {
    CEL_RETURN_IF_ERROR(
        ConstantFromProto(reference.value(), ret_val.mutable_value()));
  }
  return ret_val;
}

absl::StatusOr<ReferencePb> ReferenceToProto(const Reference& reference) {
  ReferencePb result;

  result.set_name(reference.name());

  for (const auto& overload_id : reference.overload_id()) {
    result.add_overload_id(overload_id);
  }

  if (reference.has_value()) {
    CEL_RETURN_IF_ERROR(
        ConstantToProto(reference.value(), result.mutable_value()));
  }

  return result;
}

absl::Status TypeToProto(const TypeSpec& type, TypePb* result);

struct TypeKindToProtoVisitor {
  absl::Status operator()(PrimitiveType primitive) {
    switch (primitive) {
      case PrimitiveType::kPrimitiveTypeUnspecified:
        result->set_primitive(TypePb::PRIMITIVE_TYPE_UNSPECIFIED);
        return absl::OkStatus();
      case PrimitiveType::kBool:
        result->set_primitive(TypePb::BOOL);
        return absl::OkStatus();
      case PrimitiveType::kInt64:
        result->set_primitive(TypePb::INT64);
        return absl::OkStatus();
      case PrimitiveType::kUint64:
        result->set_primitive(TypePb::UINT64);
        return absl::OkStatus();
      case PrimitiveType::kDouble:
        result->set_primitive(TypePb::DOUBLE);
        return absl::OkStatus();
      case PrimitiveType::kString:
        result->set_primitive(TypePb::STRING);
        return absl::OkStatus();
      case PrimitiveType::kBytes:
        result->set_primitive(TypePb::BYTES);
        return absl::OkStatus();
      default:
        break;
    }
    return absl::InvalidArgumentError("Unsupported primitive type");
  }

  absl::Status operator()(PrimitiveTypeWrapper wrapper) {
    CEL_RETURN_IF_ERROR(this->operator()(wrapper.type()));
    auto wrapped = result->primitive();
    result->set_wrapper(wrapped);
    return absl::OkStatus();
  }

  absl::Status operator()(UnsetTypeSpec) {
    result->clear_type_kind();
    return absl::OkStatus();
  }

  absl::Status operator()(DynTypeSpec) {
    result->mutable_dyn();
    return absl::OkStatus();
  }

  absl::Status operator()(ErrorTypeSpec) {
    result->mutable_error();
    return absl::OkStatus();
  }

  absl::Status operator()(NullTypeSpec) {
    result->set_null(google::protobuf::NULL_VALUE);
    return absl::OkStatus();
  }

  absl::Status operator()(const ListTypeSpec& list_type) {
    return TypeToProto(list_type.elem_type(),
                       result->mutable_list_type()->mutable_elem_type());
  }

  absl::Status operator()(const MapTypeSpec& map_type) {
    CEL_RETURN_IF_ERROR(TypeToProto(
        map_type.key_type(), result->mutable_map_type()->mutable_key_type()));
    return TypeToProto(map_type.value_type(),
                       result->mutable_map_type()->mutable_value_type());
  }

  absl::Status operator()(const MessageTypeSpec& message_type) {
    result->set_message_type(message_type.type());
    return absl::OkStatus();
  }

  absl::Status operator()(const WellKnownTypeSpec& well_known_type) {
    switch (well_known_type) {
      case WellKnownTypeSpec::kWellKnownTypeUnspecified:
        result->set_well_known(TypePb::WELL_KNOWN_TYPE_UNSPECIFIED);
        return absl::OkStatus();
      case WellKnownTypeSpec::kAny:
        result->set_well_known(TypePb::ANY);
        return absl::OkStatus();

      case WellKnownTypeSpec::kDuration:
        result->set_well_known(TypePb::DURATION);
        return absl::OkStatus();
      case WellKnownTypeSpec::kTimestamp:
        result->set_well_known(TypePb::TIMESTAMP);
        return absl::OkStatus();
      default:
        break;
    }
    return absl::InvalidArgumentError("Unsupported well-known type");
  }

  absl::Status operator()(const FunctionTypeSpec& function_type) {
    CEL_RETURN_IF_ERROR(
        TypeToProto(function_type.result_type(),
                    result->mutable_function()->mutable_result_type()));

    for (const TypeSpec& arg_type : function_type.arg_types()) {
      CEL_RETURN_IF_ERROR(
          TypeToProto(arg_type, result->mutable_function()->add_arg_types()));
    }
    return absl::OkStatus();
  }

  absl::Status operator()(const AbstractType& type) {
    auto* abstract_type_pb = result->mutable_abstract_type();
    abstract_type_pb->set_name(type.name());
    for (const TypeSpec& type_param : type.parameter_types()) {
      CEL_RETURN_IF_ERROR(
          TypeToProto(type_param, abstract_type_pb->add_parameter_types()));
    }
    return absl::OkStatus();
  }

  absl::Status operator()(const std::unique_ptr<TypeSpec>& type_type) {
    return TypeToProto((type_type != nullptr) ? *type_type : TypeSpec(),
                       result->mutable_type());
  }

  absl::Status operator()(const ParamTypeSpec& param_type) {
    result->set_type_param(param_type.type());
    return absl::OkStatus();
  }

  TypePb* result;
};

absl::Status TypeToProto(const TypeSpec& type, TypePb* result) {
  return absl::visit(TypeKindToProtoVisitor{result}, type.type_kind());
}

}  // namespace

absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const cel::expr::Expr& expr,
    const cel::expr::SourceInfo* source_info) {
  CEL_ASSIGN_OR_RETURN(auto runtime_expr, ExprValueFromProto(expr));
  SourceInfo runtime_source_info;
  if (source_info != nullptr) {
    CEL_ASSIGN_OR_RETURN(runtime_source_info,
                         ConvertProtoSourceInfoToNative(*source_info));
  }
  return std::make_unique<Ast>(std::move(runtime_expr),
                               std::move(runtime_source_info));
}

absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromParsedExpr(
    const ParsedExprPb& parsed_expr) {
  return CreateAstFromParsedExpr(parsed_expr.expr(),
                                 &parsed_expr.source_info());
}

absl::Status AstToParsedExpr(const Ast& ast,
                             cel::expr::ParsedExpr* absl_nonnull out) {
  ParsedExprPb& parsed_expr = *out;
  CEL_RETURN_IF_ERROR(ExprToProto(ast.root_expr(), parsed_expr.mutable_expr()));
  CEL_RETURN_IF_ERROR(ast_internal::SourceInfoToProto(
      ast.source_info(), parsed_expr.mutable_source_info()));

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<Ast>> CreateAstFromCheckedExpr(
    const CheckedExprPb& checked_expr) {
  CEL_ASSIGN_OR_RETURN(Expr expr, ExprValueFromProto(checked_expr.expr()));
  CEL_ASSIGN_OR_RETURN(SourceInfo source_info, ConvertProtoSourceInfoToNative(
                                                   checked_expr.source_info()));

  Ast::ReferenceMap reference_map;
  for (const auto& pair : checked_expr.reference_map()) {
    auto native_reference = ConvertProtoReferenceToNative(pair.second);
    if (!native_reference.ok()) {
      return native_reference.status();
    }
    reference_map.emplace(pair.first, *(std::move(native_reference)));
  }
  Ast::TypeMap type_map;
  for (const auto& pair : checked_expr.type_map()) {
    auto native_type = ConvertProtoTypeToNative(pair.second);
    if (!native_type.ok()) {
      return native_type.status();
    }
    type_map.emplace(pair.first, *(std::move(native_type)));
  }

  return std::make_unique<Ast>(std::move(expr), std::move(source_info),
                               std::move(reference_map), std::move(type_map),
                               checked_expr.expr_version());
}

absl::Status AstToCheckedExpr(
    const Ast& ast, cel::expr::CheckedExpr* absl_nonnull out) {
  if (!ast.is_checked()) {
    return absl::InvalidArgumentError("AST is not type-checked");
  }
  CheckedExprPb& checked_expr = *out;
  checked_expr.set_expr_version(ast.expr_version());
  CEL_RETURN_IF_ERROR(
      ExprToProto(ast.root_expr(), checked_expr.mutable_expr()));
  CEL_RETURN_IF_ERROR(ast_internal::SourceInfoToProto(
      ast.source_info(), checked_expr.mutable_source_info()));
  for (auto it = ast.reference_map().begin(); it != ast.reference_map().end();
       ++it) {
    ReferencePb& dest_reference =
        (*checked_expr.mutable_reference_map())[it->first];
    CEL_ASSIGN_OR_RETURN(dest_reference, ReferenceToProto(it->second));
  }

  for (auto it = ast.type_map().begin(); it != ast.type_map().end(); ++it) {
    TypePb& dest_type = (*checked_expr.mutable_type_map())[it->first];
    CEL_RETURN_IF_ERROR(TypeToProto(it->second, &dest_type));
  }

  return absl::OkStatus();
}

}  // namespace cel
