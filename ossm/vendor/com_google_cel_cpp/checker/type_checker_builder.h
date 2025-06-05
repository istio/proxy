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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/checker_options.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_checker.h"
#include "common/decl.h"
#include "common/type_introspector.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class TypeCheckerBuilder;

// Creates a new `TypeCheckerBuilder`.
//
// When passing a raw pointer to a descriptor pool, the descriptor pool must
// outlive the type checker builder and the type checker builder it creates.
//
// The descriptor pool must include the minimally necessary
// descriptors required by CEL. Those are the following:
// - google.protobuf.NullValue
// - google.protobuf.BoolValue
// - google.protobuf.Int32Value
// - google.protobuf.Int64Value
// - google.protobuf.UInt32Value
// - google.protobuf.UInt64Value
// - google.protobuf.FloatValue
// - google.protobuf.DoubleValue
// - google.protobuf.BytesValue
// - google.protobuf.StringValue
// - google.protobuf.Any
// - google.protobuf.Duration
// - google.protobuf.Timestamp
absl::StatusOr<TypeCheckerBuilder> CreateTypeCheckerBuilder(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    const CheckerOptions& options = {});
absl::StatusOr<TypeCheckerBuilder> CreateTypeCheckerBuilder(
    absl::Nonnull<std::shared_ptr<const google::protobuf::DescriptorPool>>
        descriptor_pool,
    const CheckerOptions& options = {});

using ConfigureBuilderCallback =
    absl::AnyInvocable<absl::Status(TypeCheckerBuilder&)>;

struct CheckerLibrary {
  // Optional identifier to avoid collisions re-adding the same declarations.
  // If id is empty, it is not considered.
  std::string id;
  // Functional implementation applying the library features to the builder.
  ConfigureBuilderCallback options;
};

// Builder for TypeChecker instances.
class TypeCheckerBuilder {
 public:
  TypeCheckerBuilder(const TypeCheckerBuilder&) = delete;
  TypeCheckerBuilder(TypeCheckerBuilder&&) = default;
  TypeCheckerBuilder& operator=(const TypeCheckerBuilder&) = delete;
  TypeCheckerBuilder& operator=(TypeCheckerBuilder&&) = default;

  absl::StatusOr<std::unique_ptr<TypeChecker>> Build() &&;

  absl::Status AddLibrary(CheckerLibrary library);

  absl::Status AddVariable(const VariableDecl& decl);
  absl::Status AddFunction(const FunctionDecl& decl);

  // Adds function declaration overloads to the TypeChecker being built.
  //
  // Attempts to merge with any existing overloads for a function decl with the
  // same name. If the overloads are not compatible, an error is returned and
  // no change is made.
  absl::Status MergeFunction(const FunctionDecl& decl);

  void AddTypeProvider(std::unique_ptr<TypeIntrospector> provider);

  void set_container(absl::string_view container);

  const CheckerOptions& options() const { return options_; }

 private:
  friend absl::StatusOr<TypeCheckerBuilder> CreateTypeCheckerBuilder(
      absl::Nonnull<std::shared_ptr<const google::protobuf::DescriptorPool>>
          descriptor_pool,
      const CheckerOptions& options);

  TypeCheckerBuilder(
      absl::Nonnull<std::shared_ptr<const google::protobuf::DescriptorPool>>
          descriptor_pool,
      const CheckerOptions& options)
      : options_(options), env_(std::move(descriptor_pool)) {}

  CheckerOptions options_;
  std::vector<CheckerLibrary> libraries_;
  absl::flat_hash_set<std::string> library_ids_;

  checker_internal::TypeCheckEnv env_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
