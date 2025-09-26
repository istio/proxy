// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_INFERENCE_CONTEXT_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_INFERENCE_CONTEXT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/decl.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {

// Class manages context for type inferences in the type checker.
// TODO(uncreated-issue/72): for now, just checks assignability for concrete types.
// Support for finding substitutions of type parameters will be added in a
// follow-up CL.
class TypeInferenceContext {
 public:
  // Convenience alias for an instance map for type parameters mapped to type
  // vars in a given context.
  //
  // This should be treated as opaque, the client should not manually modify.
  using InstanceMap = absl::flat_hash_map<std::string, absl::string_view>;

  struct OverloadResolution {
    Type result_type;
    std::vector<OverloadDecl> overloads;
  };

 private:
  // Alias for a map from type var name to the type it is bound to.
  //
  // Used for prospective substitutions during type inference to make progress
  // without affecting final assigned types.
  using SubstitutionMap = absl::flat_hash_map<absl::string_view, Type>;

 public:
  // Helper class for managing several dependent type assignability checks.
  //
  // Note: while allowed, updating multiple AssignabilityContexts concurrently
  // can lead to inconsistencies in the final type bindings.
  class AssignabilityContext {
   public:
    // Checks if `from` is assignable to `to` with the current type
    // substitutions and any additional prospective substitutions in the parent
    // inference context.
    bool IsAssignable(const Type& from, const Type& to);

    // Applies any prospective type assignments to the parent inference context.
    //
    // This should only be called after all assignability checks have completed.
    //
    // Leaves the AssignabilityContext in the starting state (i.e. no
    // prospective substitutions).
    void UpdateInferredTypeAssignments();

    // Return the AssignabilityContext to the starting state (i.e. no
    // prospective substitutions).
    void Reset();

   private:
    explicit AssignabilityContext(TypeInferenceContext& inference_context)
        : inference_context_(inference_context) {}

    AssignabilityContext(const AssignabilityContext&) = delete;
    AssignabilityContext& operator=(const AssignabilityContext&) = delete;
    AssignabilityContext(AssignabilityContext&&) = delete;
    AssignabilityContext& operator=(AssignabilityContext&&) = delete;

    friend class TypeInferenceContext;

    TypeInferenceContext& inference_context_;
    SubstitutionMap prospective_substitutions_;
  };

  explicit TypeInferenceContext(google::protobuf::Arena* arena,
                                bool enable_legacy_null_assignment = true)
      : arena_(arena),
        enable_legacy_null_assignment_(enable_legacy_null_assignment) {}

  // Creates a new AssignabilityContext for the current inference context.
  //
  // This is intended for managing several dependent type assignability checks
  // that should only be added to the final type bindings if all checks succeed.
  //
  // Note: while allowed, updating multiple AssignabilityContexts concurrently
  // can lead to inconsistencies in the final type bindings.
  AssignabilityContext CreateAssignabilityContext()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return AssignabilityContext(*this);
  }
  // Resolves any remaining type parameters in the given type to a concrete
  // type or dyn.
  Type FinalizeType(const Type& type) const {
    return FullySubstitute(type, /*free_to_dyn=*/true);
  }

  // Recursively apply any substitutions to the given type.
  Type FullySubstitute(const Type& type, bool free_to_dyn = false) const;

  // Replace any generic type parameters in the given type with specific type
  // variables. Internally, type variables are just a unique string parameter
  // name.
  Type InstantiateTypeParams(const Type& type);

  // Overload for function overload types that need coordination across
  // multiple function parameters.
  Type InstantiateTypeParams(const Type& type, InstanceMap& substitutions);

  // Resolves the applicable overloads for the given function call given the
  // inferred argument types.
  //
  // If found, returns the result type and the list of applicable overloads.
  absl::optional<OverloadResolution> ResolveOverload(
      const FunctionDecl& decl, absl::Span<const Type> argument_types,
      bool is_receiver);

  // Checks if `from` is assignable to `to`.
  bool IsAssignable(const Type& from, const Type& to);

  std::string DebugString() const {
    return absl::StrCat(
        "type_parameter_bindings: ",
        absl::StrJoin(
            type_parameter_bindings_, "\n ",
            [](std::string* out, const auto& binding) {
              absl::StrAppend(
                  out, binding.first, " (", binding.second.name, ") -> ",
                  binding.second.type.value_or(Type(TypeParamType("none")))
                      .DebugString());
            }));
  }

 private:
  struct TypeVar {
    absl::optional<Type> type;
    absl::string_view name;
  };

  // Relative generality between two types.
  enum class RelativeGenerality {
    kMoreGeneral,
    // Note: kLessGeneral does not imply it is definitely more specific, only
    // that we cannot determine if equivalent or more general.
    kLessGeneral,
    kEquivalent,
  };

  absl::string_view NewTypeVar(absl::string_view name = "") {
    next_type_parameter_id_++;
    auto inserted = type_parameter_bindings_.insert(
        {absl::StrCat("T%", next_type_parameter_id_), {absl::nullopt, name}});
    ABSL_DCHECK(inserted.second);
    return inserted.first->first;
  }

  // Returns true if the two types are equivalent with the current type
  // substitutions.
  bool TypeEquivalent(const Type& a, const Type& b);

  // Returns true if `from` is assignable to `to` with the current type
  // substitutions and any additional prospective substitutions.
  //
  // `prospective_substitutions` is a map from type var name to the type it
  // should be bound to in the current context, augmenting any existing
  // substitutions.
  //
  // If the types are not assignable, returns false and leaves
  // `prospective_substitutions` unmodified.
  //
  // If the types are assignable, returns true and updates
  // `prospective_substitutions` with any new type parameter bindings.
  bool IsAssignableInternal(const Type& from, const Type& to,
                            SubstitutionMap& prospective_substitutions);

  bool IsAssignableWithConstraints(const Type& from, const Type& to,
                                   SubstitutionMap& prospective_substitutions);

  // Relative generality of `from` as compared to `to` with the current type
  // substitutions and any additional prospective substitutions.
  //
  // Generality is only defined as a partial ordering. Some types are
  // incomparable. However we only need to know if a type is definitely more
  // general or not.
  RelativeGenerality CompareGenerality(
      const Type& from, const Type& to,
      const SubstitutionMap& prospective_substitutions) const;

  Type Substitute(const Type& type, const SubstitutionMap& substitutions) const;

  bool OccursWithin(absl::string_view var_name, const Type& type,
                    const SubstitutionMap& substitutions) const;

  void UpdateTypeParameterBindings(
      const SubstitutionMap& prospective_substitutions);

  // Map from type var parameter name to the type it is bound to.
  //
  // Type var parameters are formatted as "T%<id>" to avoid collisions with
  // provided type parameter names.
  //
  // node_hash_map is used to preserve pointer stability for use with
  // TypeParamType.
  //
  // Type parameter instances should be resolved to a concrete type during type
  // checking to remove the lifecycle dependency on the inference context
  // instance.
  //
  // nullopt signifies a free type variable.
  absl::node_hash_map<std::string, TypeVar> type_parameter_bindings_;
  int64_t next_type_parameter_id_ = 0;
  google::protobuf::Arena* arena_;
  bool enable_legacy_null_assignment_;
};

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_INTERNAL_TYPE_INFERENCE_CONTEXT_H_
