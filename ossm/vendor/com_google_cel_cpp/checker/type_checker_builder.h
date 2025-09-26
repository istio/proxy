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

#include "absl/base/nullability.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/checker_options.h"
#include "checker/type_checker.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

class TypeCheckerBuilder;
class TypeCheckerBuilderImpl;

// Functional implementation to apply the library features to a
// TypeCheckerBuilder.
using TypeCheckerBuilderConfigurer =
    absl::AnyInvocable<absl::Status(TypeCheckerBuilder&) const>;

struct CheckerLibrary {
  // Optional identifier to avoid collisions re-adding the same declarations.
  // If id is empty, it is not considered.
  std::string id;
  TypeCheckerBuilderConfigurer configure;
};

// Represents a declaration to only use a subset of a library.
struct TypeCheckerSubset {
  using FunctionPredicate = absl::AnyInvocable<bool(
      absl::string_view function, absl::string_view overload_id) const>;

  // The id of the library to subset. Only one subset can be applied per
  // library id.
  //
  // Must be non-empty.
  std::string library_id;
  // Predicate to apply to function overloads. If true, the overload will be
  // included in the subset. If no overload for a function is included, the
  // entire function is excluded.
  FunctionPredicate should_include_overload;
};

// Interface for TypeCheckerBuilders.
class TypeCheckerBuilder {
 public:
  virtual ~TypeCheckerBuilder() = default;

  // Adds a library to the TypeChecker being built.
  //
  // Libraries are applied in the order they are added. They effectively
  // apply before any direct calls to AddVariable, AddFunction, etc.
  virtual absl::Status AddLibrary(CheckerLibrary library) = 0;

  // Adds a subset declaration for a library to the TypeChecker being built.
  //
  // At most one subset can be applied per library id.
  virtual absl::Status AddLibrarySubset(TypeCheckerSubset subset) = 0;

  // Adds a variable declaration that may be referenced in expressions checked
  // with the resulting type checker.
  virtual absl::Status AddVariable(const VariableDecl& decl) = 0;

  // Adds a variable declaration that may be referenced in expressions checked
  // with the resulting type checker.
  //
  // This version replaces any existing variable declaration with the same name.
  virtual absl::Status AddOrReplaceVariable(const VariableDecl& decl) = 0;

  // Declares struct type by fully qualified name as a context declaration.
  //
  // Context declarations are a way to declare a group of variables based on the
  // definition of a struct type. Each top level field of the struct is declared
  // as an individual variable of the field type.
  //
  // It is an error if the type contains a field that overlaps with another
  // declared variable.
  //
  // Note: only protobuf backed struct types are supported at this time.
  virtual absl::Status AddContextDeclaration(absl::string_view type) = 0;

  // Adds a function declaration that may be referenced in expressions checked
  // with the resulting TypeChecker.
  virtual absl::Status AddFunction(const FunctionDecl& decl) = 0;

  // Adds function declaration overloads to the TypeChecker being built.
  //
  // Attempts to merge with any existing overloads for a function decl with the
  // same name. If the overloads are not compatible, an error is returned and
  // no change is made.
  virtual absl::Status MergeFunction(const FunctionDecl& decl) = 0;

  // Sets the expected type for checked expressions.
  //
  // Validation will fail with an ERROR level issue if the deduced type of the
  // expression is not assignable to this type.
  //
  // Note: if set multiple times, the last value is used.
  virtual void SetExpectedType(const Type& type) = 0;

  // Adds a type provider to the TypeChecker being built.
  //
  // Type providers are used to describe custom types with typed field
  // traversal. This is not needed for built-in types or protobuf messages
  // described by the associated descriptor pool.
  virtual void AddTypeProvider(std::unique_ptr<TypeIntrospector> provider) = 0;

  // Set the container for the TypeChecker being built.
  //
  // This is used for resolving references in the expressions being built.
  //
  // Note: if set multiple times, the last value is used. This can lead to
  // surprising behavior if used in a custom library.
  virtual void set_container(absl::string_view container) = 0;

  // The current options for the TypeChecker being built.
  virtual const CheckerOptions& options() const = 0;

  // Builds a new TypeChecker instance.
  virtual absl::StatusOr<std::unique_ptr<TypeChecker>> Build() = 0;

  // Returns a pointer to an arena that can be used to allocate memory for types
  // that will be used by the TypeChecker being built.
  //
  // On Build(), the arena is transferred to the TypeChecker being built.
  virtual google::protobuf::Arena* absl_nonnull arena() = 0;

  // The configured descriptor pool.
  virtual const google::protobuf::DescriptorPool* absl_nonnull descriptor_pool()
      const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_TYPE_CHECKER_BUILDER_H_
