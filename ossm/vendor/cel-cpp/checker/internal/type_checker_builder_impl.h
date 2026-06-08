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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECKER_BUILDER_IMPL_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_CHECKER_BUILDER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "checker/checker_options.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_checker.h"
#include "checker/type_checker_builder.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel::checker_internal {

class TypeCheckerBuilderImpl;

// Builder for TypeChecker instances.
class TypeCheckerBuilderImpl : public TypeCheckerBuilder {
 public:
  TypeCheckerBuilderImpl(
      absl_nonnull std::shared_ptr<const google::protobuf::DescriptorPool>
          descriptor_pool,
      const CheckerOptions& options)
      : options_(options),
        target_config_(&default_config_),
        descriptor_pool_(std::move(descriptor_pool)) {}

  // Move only.
  TypeCheckerBuilderImpl(const TypeCheckerBuilderImpl&) = delete;
  TypeCheckerBuilderImpl(TypeCheckerBuilderImpl&&) = default;
  TypeCheckerBuilderImpl& operator=(const TypeCheckerBuilderImpl&) = delete;
  TypeCheckerBuilderImpl& operator=(TypeCheckerBuilderImpl&&) = default;

  absl::StatusOr<std::unique_ptr<TypeChecker>> Build() override;

  absl::Status AddLibrary(CheckerLibrary library) override;
  absl::Status AddLibrarySubset(TypeCheckerSubset subset) override;

  absl::Status AddVariable(const VariableDecl& decl) override;
  absl::Status AddOrReplaceVariable(const VariableDecl& decl) override;
  absl::Status AddContextDeclaration(absl::string_view type) override;

  absl::Status AddFunction(const FunctionDecl& decl) override;
  absl::Status MergeFunction(const FunctionDecl& decl) override;

  void SetExpectedType(const Type& type) override;

  void AddTypeProvider(std::unique_ptr<TypeIntrospector> provider) override;

  void set_container(absl::string_view container) override;

  const CheckerOptions& options() const override { return options_; }

  google::protobuf::Arena* absl_nonnull arena() override {
    if (arena_ == nullptr) {
      arena_ = std::make_shared<google::protobuf::Arena>();
    }
    return arena_.get();
  }

  const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool() const override {
    return descriptor_pool_.get();
  }

 private:
  // Sematic for adding a possibly duplicated declaration.
  enum class AddSemantic {
    kInsertIfAbsent,
    kInsertOrReplace,
    // Attempts to merge with any existing overloads for the same function.
    // Will fail if any of the IDs or signatures collide.
    kTryMerge,
  };

  struct VariableDeclRecord {
    VariableDecl decl;
    AddSemantic add_semantic;
  };

  struct FunctionDeclRecord {
    FunctionDecl decl;
    AddSemantic add_semantic;
  };

  // A record of configuration calls.
  // Used to replay the configuration in calls to Build().
  struct ConfigRecord {
    std::string id = "";
    std::vector<VariableDeclRecord> variables;
    std::vector<FunctionDeclRecord> functions;
    std::vector<std::shared_ptr<const TypeIntrospector>> type_providers;
    std::vector<const google::protobuf::Descriptor*> context_types;
  };

  absl::Status BuildLibraryConfig(const CheckerLibrary& library,
                                  ConfigRecord* absl_nonnull config);

  absl::Status ApplyConfig(ConfigRecord config, const TypeCheckerSubset* subset,
                           TypeCheckEnv& env);

  CheckerOptions options_;
  // Default target for configuration changes. Used for direct calls to
  // AddVariable, AddFunction, etc.
  ConfigRecord default_config_;
  // Active target for configuration changes.
  // This is used to track which library the change is made on behalf of.
  ConfigRecord* absl_nonnull target_config_;
  std::shared_ptr<const google::protobuf::DescriptorPool> descriptor_pool_;
  std::shared_ptr<google::protobuf::Arena> arena_;
  std::vector<CheckerLibrary> libraries_;
  absl::flat_hash_map<std::string, TypeCheckerSubset> subsets_;
  absl::flat_hash_set<std::string> library_ids_;
  std::string container_;
  absl::optional<Type> expected_type_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
