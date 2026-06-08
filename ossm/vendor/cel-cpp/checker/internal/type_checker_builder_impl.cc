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

#include "checker/internal/type_checker_builder_impl.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "checker/internal/type_check_env.h"
#include "checker/internal/type_checker_impl.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/type_kind.h"
#include "internal/lexis.h"
#include "internal/status_macros.h"
#include "parser/macro.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {
namespace {

const absl::flat_hash_map<std::string, std::vector<Macro>>& GetStdMacros() {
  static const absl::NoDestructor<
      absl::flat_hash_map<std::string, std::vector<Macro>>>
      kStdMacros({
          {"has", {HasMacro()}},
          {"all", {AllMacro()}},
          {"exists", {ExistsMacro()}},
          {"exists_one", {ExistsOneMacro()}},
          {"filter", {FilterMacro()}},
          {"map", {Map2Macro(), Map3Macro()}},
          {"optMap", {OptMapMacro()}},
          {"optFlatMap", {OptFlatMapMacro()}},
      });
  return *kStdMacros;
}

absl::Status CheckStdMacroOverlap(const FunctionDecl& decl) {
  const auto& std_macros = GetStdMacros();
  auto it = std_macros.find(decl.name());
  if (it == std_macros.end()) {
    return absl::OkStatus();
  }
  const auto& macros = it->second;
  for (const auto& macro : macros) {
    bool macro_member = macro.is_receiver_style();
    size_t macro_arg_count = macro.argument_count() + (macro_member ? 1 : 0);
    for (const auto& ovl : decl.overloads()) {
      if (ovl.member() == macro_member &&
          ovl.args().size() == macro_arg_count) {
        return absl::InvalidArgumentError(absl::StrCat(
            "overload for name '", macro.function(), "' with ", macro_arg_count,
            " argument(s) overlaps with predefined macro"));
      }
    }
  }
  return absl::OkStatus();
}

absl::Status AddContextDeclarationVariables(
    const google::protobuf::Descriptor* absl_nonnull descriptor, TypeCheckEnv& env) {
  for (int i = 0; i < descriptor->field_count(); i++) {
    const google::protobuf::FieldDescriptor* proto_field = descriptor->field(i);
    MessageTypeField cel_field(proto_field);
    Type field_type = cel_field.GetType();
    if (field_type.IsEnum()) {
      field_type = IntType();
    }
    if (!env.InsertVariableIfAbsent(
            MakeVariableDecl(cel_field.name(), field_type))) {
      return absl::AlreadyExistsError(
          absl::StrCat("variable '", cel_field.name(),
                       "' declared multiple times (from context declaration: '",
                       descriptor->full_name(), "')"));
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<FunctionDecl> MergeFunctionDecls(
    const FunctionDecl& existing_decl, const FunctionDecl& new_decl) {
  if (existing_decl.name() != new_decl.name()) {
    return absl::InternalError(
        "Attempted to merge function decls with different names");
  }

  FunctionDecl merged_decl = existing_decl;
  for (const auto& ovl : new_decl.overloads()) {
    // We do not tolerate signature collisions, even if they are exact matches.
    CEL_RETURN_IF_ERROR(merged_decl.AddOverload(ovl));
  }

  return merged_decl;
}

absl::optional<FunctionDecl> FilterDecl(FunctionDecl decl,
                                        const TypeCheckerSubset& subset) {
  FunctionDecl filtered;
  std::string name = decl.release_name();
  std::vector<OverloadDecl> overloads = decl.release_overloads();
  for (const auto& ovl : overloads) {
    if (subset.should_include_overload(name, ovl.id())) {
      absl::Status s = filtered.AddOverload(std::move(ovl));
      if (!s.ok()) {
        // Should not be possible to construct the original decl in a way that
        // would cause this to fail.
        ABSL_LOG(DFATAL) << "failed to add overload to filtered decl: " << s;
      }
    }
  }
  if (filtered.overloads().empty()) {
    return absl::nullopt;
  }
  filtered.set_name(std::move(name));
  return filtered;
}

absl::Status ValidateType(const Type& t, bool check_type_param_name,
                          int depth_limit, int remaining_depth) {
  if (remaining_depth-- <= 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("type nesting limit of ", depth_limit, " exceeded"));
  }
  switch (t.kind()) {
    case TypeKind::kTypeParam: {
      if (!check_type_param_name) {
        return absl::OkStatus();
      }
      const TypeParamType& type_param = t.GetTypeParam();
      if (!internal::LexisIsIdentifier(type_param.name())) {
        return absl::InvalidArgumentError(
            absl::StrCat("type parameter name '", type_param.name(),
                         "' is not a valid identifier"));
      }
      return absl::OkStatus();
    }
    case TypeKind::kList: {
      Type element_type = t.AsList()->GetElement();
      return ValidateType(element_type, check_type_param_name, depth_limit,
                          remaining_depth);
    }
    case TypeKind::kMap: {
      Type key_type = t.AsMap()->GetKey();
      Type value_type = t.AsMap()->GetValue();
      CEL_RETURN_IF_ERROR(ValidateType(key_type, check_type_param_name,
                                       depth_limit, remaining_depth));
      return ValidateType(value_type, check_type_param_name, depth_limit,
                          remaining_depth);
    }
    case TypeKind::kStruct: {
      auto message_type = t.AsMessage();
      if (message_type.has_value() && !static_cast<bool>(*message_type)) {
        return absl::InvalidArgumentError(
            "an empty message type cannot be used in a type declaration");
      }
      return absl::OkStatus();
    }
    case TypeKind::kOpaque: {
      for (Type type_param : t.AsOpaque()->GetParameters()) {
        CEL_RETURN_IF_ERROR(ValidateType(type_param, check_type_param_name,
                                         depth_limit, remaining_depth));
      }
      return absl::OkStatus();
    }
    case TypeKind::kType: {
      for (Type type_param : t.AsType()->GetParameters()) {
        CEL_RETURN_IF_ERROR(ValidateType(type_param, check_type_param_name,
                                         depth_limit, remaining_depth));
      }
      return absl::OkStatus();
    }
    default:
      break;
  }
  return absl::OkStatus();
}

absl::Status ValidateFunctionDecl(const FunctionDecl& decl,
                                  bool check_type_param_name, int depth_limit) {
  CEL_RETURN_IF_ERROR(CheckStdMacroOverlap(decl));
  for (const auto& ovl : decl.overloads()) {
    CEL_RETURN_IF_ERROR(ValidateType(ovl.result(), check_type_param_name,
                                     depth_limit, depth_limit));
    for (const auto& arg : ovl.args()) {
      CEL_RETURN_IF_ERROR(
          ValidateType(arg, check_type_param_name, depth_limit, depth_limit));
    }
  }
  return absl::OkStatus();
}

absl::Status ValidateVariableDecl(const VariableDecl& decl,
                                  bool check_type_param_name, int depth_limit) {
  return ValidateType(decl.type(), check_type_param_name, depth_limit,
                      depth_limit);
}

}  // namespace

absl::Status TypeCheckerBuilderImpl::BuildLibraryConfig(
    const CheckerLibrary& library,
    TypeCheckerBuilderImpl::ConfigRecord* config) {
  target_config_ = config;
  absl::Cleanup reset([this] { target_config_ = &default_config_; });

  return library.configure(*this);
}

absl::Status TypeCheckerBuilderImpl::ApplyConfig(
    TypeCheckerBuilderImpl::ConfigRecord config,
    const TypeCheckerSubset* subset, TypeCheckEnv& env) {
  using FunctionDeclRecord = TypeCheckerBuilderImpl::FunctionDeclRecord;

  for (auto& type_provider : config.type_providers) {
    env.AddTypeProvider(std::move(type_provider));
  }

  for (FunctionDeclRecord& fn : config.functions) {
    FunctionDecl decl = std::move(fn.decl);
    if (subset != nullptr) {
      absl::optional<FunctionDecl> filtered =
          FilterDecl(std::move(decl), *subset);
      if (!filtered.has_value()) {
        continue;
      }
      decl = std::move(*filtered);
    }

    switch (fn.add_semantic) {
      case AddSemantic::kInsertIfAbsent: {
        std::string name = decl.name();
        if (!env.InsertFunctionIfAbsent(std::move(decl))) {
          return absl::AlreadyExistsError(
              absl::StrCat("function '", name, "' declared multiple times"));
        }
        break;
      }
      case AddSemantic::kTryMerge: {
        const FunctionDecl* existing_decl = env.LookupFunction(decl.name());
        FunctionDecl to_add = std::move(decl);
        if (existing_decl != nullptr) {
          CEL_ASSIGN_OR_RETURN(
              to_add, MergeFunctionDecls(*existing_decl, std::move(to_add)));
        }
        env.InsertOrReplaceFunction(std::move(to_add));
        break;
      }
      default:
        return absl::InternalError(absl::StrCat(
            "unsupported function add semantic: ", fn.add_semantic));
    }
  }

  for (const google::protobuf::Descriptor* context_type : config.context_types) {
    CEL_RETURN_IF_ERROR(AddContextDeclarationVariables(context_type, env));
  }

  for (VariableDeclRecord& var : config.variables) {
    switch (var.add_semantic) {
      case AddSemantic::kInsertIfAbsent: {
        if (!env.InsertVariableIfAbsent(var.decl)) {
          return absl::AlreadyExistsError(absl::StrCat(
              "variable '", var.decl.name(), "' declared multiple times"));
        }
        break;
      }
      case AddSemantic::kInsertOrReplace: {
        env.InsertOrReplaceVariable(var.decl);
        break;
      }
      default:
        return absl::InternalError(absl::StrCat(
            "unsupported variable add semantic: ", var.add_semantic));
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<TypeChecker>> TypeCheckerBuilderImpl::Build() {
  TypeCheckEnv env(descriptor_pool_, arena_);
  env.set_container(container_);
  if (expected_type_.has_value()) {
    env.set_expected_type(*expected_type_);
  }

  ConfigRecord anonymous_config;
  std::vector<ConfigRecord> configs;
  for (const auto& library : libraries_) {
    ConfigRecord* config = &anonymous_config;
    if (!library.id.empty()) {
      configs.emplace_back();
      config = &configs.back();
      config->id = library.id;
    }
    CEL_RETURN_IF_ERROR(BuildLibraryConfig(library, config));
  }

  for (const ConfigRecord& config : configs) {
    TypeCheckerSubset* subset = nullptr;
    if (!config.id.empty()) {
      auto it = subsets_.find(config.id);
      if (it != subsets_.end()) {
        subset = &it->second;
      }
    }
    CEL_RETURN_IF_ERROR(ApplyConfig(std::move(config), subset, env));
  }
  CEL_RETURN_IF_ERROR(ApplyConfig(std::move(anonymous_config),
                                  /*subset=*/nullptr, env));

  CEL_RETURN_IF_ERROR(ApplyConfig(default_config_, /*subset=*/nullptr, env));

  auto checker = std::make_unique<checker_internal::TypeCheckerImpl>(
      std::move(env), options_);
  return checker;
}

absl::Status TypeCheckerBuilderImpl::AddLibrary(CheckerLibrary library) {
  if (!library.id.empty() && !library_ids_.insert(library.id).second) {
    return absl::AlreadyExistsError(
        absl::StrCat("library '", library.id, "' already exists"));
  }
  if (!library.configure) {
    return absl::OkStatus();
  }

  libraries_.push_back(std::move(library));
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddLibrarySubset(
    TypeCheckerSubset subset) {
  if (subset.library_id.empty()) {
    return absl::InvalidArgumentError(
        "library_id must not be empty for subset");
  }
  std::string id = subset.library_id;
  if (!subsets_.insert({id, std::move(subset)}).second) {
    return absl::AlreadyExistsError(
        absl::StrCat("library subset for '", id, "' already exists"));
  }
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddVariable(const VariableDecl& decl) {
  CEL_RETURN_IF_ERROR(
      ValidateVariableDecl(decl, options_.enable_type_parameter_name_validation,
                           options_.max_type_decl_nesting));
  target_config_->variables.push_back({decl, AddSemantic::kInsertIfAbsent});
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddOrReplaceVariable(
    const VariableDecl& decl) {
  CEL_RETURN_IF_ERROR(
      ValidateVariableDecl(decl, options_.enable_type_parameter_name_validation,
                           options_.max_type_decl_nesting));
  target_config_->variables.push_back({decl, AddSemantic::kInsertOrReplace});
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddContextDeclaration(
    absl::string_view type) {
  const google::protobuf::Descriptor* desc =
      descriptor_pool_->FindMessageTypeByName(type);
  if (desc == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("context declaration '", type, "' not found"));
  }

  if (IsWellKnownMessageType(desc) &&
      !options_.allow_well_known_type_context_declarations) {
    return absl::InvalidArgumentError(
        absl::StrCat("context declaration '", type, "' is not a struct"));
  }

  for (const auto* context_type : target_config_->context_types) {
    if (context_type->full_name() == desc->full_name()) {
      return absl::AlreadyExistsError(
          absl::StrCat("context declaration '", type, "' already exists"));
    }
  }

  target_config_->context_types.push_back(desc);
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::AddFunction(const FunctionDecl& decl) {
  CEL_RETURN_IF_ERROR(
      ValidateFunctionDecl(decl, options_.enable_type_parameter_name_validation,
                           options_.max_type_decl_nesting));
  target_config_->functions.push_back(
      {std::move(decl), AddSemantic::kInsertIfAbsent});
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilderImpl::MergeFunction(const FunctionDecl& decl) {
  CEL_RETURN_IF_ERROR(
      ValidateFunctionDecl(decl, options_.enable_type_parameter_name_validation,
                           options_.max_type_decl_nesting));
  target_config_->functions.push_back(
      {std::move(decl), AddSemantic::kTryMerge});
  return absl::OkStatus();
}

void TypeCheckerBuilderImpl::AddTypeProvider(
    std::unique_ptr<TypeIntrospector> provider) {
  target_config_->type_providers.push_back(std::move(provider));
}

void TypeCheckerBuilderImpl::set_container(absl::string_view container) {
  container_ = container;
}

void TypeCheckerBuilderImpl::SetExpectedType(const Type& type) {
  expected_type_ = type;
}

}  // namespace cel::checker_internal
