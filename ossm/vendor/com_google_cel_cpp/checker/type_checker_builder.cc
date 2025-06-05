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
#include "checker/type_checker_builder.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/checker_options.h"
#include "checker/internal/type_check_env.h"
#include "checker/internal/type_checker_impl.h"
#include "checker/type_checker.h"
#include "common/decl.h"
#include "common/type_introspector.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "parser/macro.h"
#include "google/protobuf/descriptor.h"

namespace cel {
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

}  // namespace

absl::StatusOr<TypeCheckerBuilder> CreateTypeCheckerBuilder(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    const CheckerOptions& options) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  return CreateTypeCheckerBuilder(std::shared_ptr<const google::protobuf::DescriptorPool>(
      descriptor_pool, [](absl::Nullable<const google::protobuf::DescriptorPool*>) {}));
}

absl::StatusOr<TypeCheckerBuilder> CreateTypeCheckerBuilder(
    absl::Nonnull<std::shared_ptr<const google::protobuf::DescriptorPool>>
        descriptor_pool,
    const CheckerOptions& options) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  // Verify the standard descriptors, we do not need to keep
  // `well_known_types::Reflection` at the moment here.
  CEL_RETURN_IF_ERROR(
      well_known_types::Reflection().Initialize(descriptor_pool.get()));
  return TypeCheckerBuilder(std::move(descriptor_pool), options);
}

absl::StatusOr<std::unique_ptr<TypeChecker>> TypeCheckerBuilder::Build() && {
  auto checker = std::make_unique<checker_internal::TypeCheckerImpl>(
      std::move(env_), options_);
  return checker;
}

absl::Status TypeCheckerBuilder::AddLibrary(CheckerLibrary library) {
  if (!library.id.empty() && !library_ids_.insert(library.id).second) {
    return absl::AlreadyExistsError(
        absl::StrCat("library '", library.id, "' already exists"));
  }
  absl::Status status = library.options(*this);

  libraries_.push_back(std::move(library));
  return status;
}

absl::Status TypeCheckerBuilder::AddVariable(const VariableDecl& decl) {
  bool inserted = env_.InsertVariableIfAbsent(decl);
  if (!inserted) {
    return absl::AlreadyExistsError(
        absl::StrCat("variable '", decl.name(), "' already exists"));
  }
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilder::AddFunction(const FunctionDecl& decl) {
  CEL_RETURN_IF_ERROR(CheckStdMacroOverlap(decl));
  bool inserted = env_.InsertFunctionIfAbsent(decl);
  if (!inserted) {
    return absl::AlreadyExistsError(
        absl::StrCat("function '", decl.name(), "' already exists"));
  }
  return absl::OkStatus();
}

absl::Status TypeCheckerBuilder::MergeFunction(const FunctionDecl& decl) {
  const FunctionDecl* existing = env_.LookupFunction(decl.name());
  if (existing == nullptr) {
    return AddFunction(decl);
  }

  CEL_RETURN_IF_ERROR(CheckStdMacroOverlap(decl));

  FunctionDecl merged = *existing;

  for (const auto& overload : decl.overloads()) {
    if (!merged.AddOverload(overload).ok()) {
      return absl::AlreadyExistsError(
          absl::StrCat("function '", decl.name(),
                       "' already has overload that conflicts with overload ''",
                       overload.id(), "'"));
    }
  }

  env_.InsertOrReplaceFunction(std::move(merged));

  return absl::OkStatus();
}

void TypeCheckerBuilder::AddTypeProvider(
    std::unique_ptr<TypeIntrospector> provider) {
  env_.AddTypeProvider(std::move(provider));
}

void TypeCheckerBuilder::set_container(absl::string_view container) {
  env_.set_container(std::string(container));
}

}  // namespace cel
