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

#include "checker/internal/type_inference_context.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_kind.h"

namespace cel::checker_internal {
namespace {

bool IsWildCardType(Type type) {
  switch (type.kind()) {
    case TypeKind::kAny:
    case TypeKind::kDyn:
    case TypeKind::kError:
      return true;
    default:
      return false;
  }
}

// Returns true if the given type is a legacy nullable type.
//
// Historically, structs and abstract types were considered nullable. This is
// inconsistent with CEL's usual interpretation of null as a literal JSON null.
//
// TODO(uncreated-issue/74): Need a concrete plan for updating existing CEL expressions
// that depend on the old behavior.
bool IsLegacyNullable(Type type) {
  switch (type.kind()) {
    case TypeKind::kStruct:
    case TypeKind::kDuration:
    case TypeKind::kTimestamp:
    case TypeKind::kAny:
    case TypeKind::kOpaque:
      return true;
    default:
      return false;
  }
}

bool IsTypeVar(absl::string_view name) { return absl::StartsWith(name, "T%"); }

bool IsUnionType(Type t) {
  switch (t.kind()) {
    case TypeKind::kAny:
    case TypeKind::kBoolWrapper:
    case TypeKind::kBytesWrapper:
    case TypeKind::kDyn:
    case TypeKind::kDoubleWrapper:
    case TypeKind::kIntWrapper:
    case TypeKind::kStringWrapper:
    case TypeKind::kUintWrapper:
      return true;
    default:
      return false;
  }
}

// Returns true if `a` is a subset of `b`.
// (b is more general than a and admits a).
bool IsSubsetOf(Type a, Type b) {
  switch (b.kind()) {
    case TypeKind::kAny:
      return true;
    case TypeKind::kBoolWrapper:
      return a.IsBool() || a.IsNull();
    case TypeKind::kBytesWrapper:
      return a.IsBytes() || a.IsNull();
    case TypeKind::kDoubleWrapper:
      return a.IsDouble() || a.IsNull();
    case TypeKind::kDyn:
      return true;
    case TypeKind::kIntWrapper:
      return a.IsInt() || a.IsNull();
    case TypeKind::kStringWrapper:
      return a.IsString() || a.IsNull();
    case TypeKind::kUintWrapper:
      return a.IsUint() || a.IsNull();
    default:
      return false;
  }
}

struct FunctionOverloadInstance {
  Type result_type;
  std::vector<Type> param_types;
};

FunctionOverloadInstance InstantiateFunctionOverload(
    TypeInferenceContext& inference_context, const OverloadDecl& ovl) {
  FunctionOverloadInstance result;
  result.param_types.reserve(ovl.args().size());

  TypeInferenceContext::InstanceMap substitutions;
  result.result_type =
      inference_context.InstantiateTypeParams(ovl.result(), substitutions);

  for (int i = 0; i < ovl.args().size(); ++i) {
    result.param_types.push_back(
        inference_context.InstantiateTypeParams(ovl.args()[i], substitutions));
  }
  return result;
}

// Converts a wrapper type to its corresponding primitive type.
// Returns nullopt if the type is not a wrapper type.
absl::optional<Type> WrapperToPrimitive(const Type& t) {
  switch (t.kind()) {
    case TypeKind::kBoolWrapper:
      return BoolType();
    case TypeKind::kBytesWrapper:
      return BytesType();
    case TypeKind::kDoubleWrapper:
      return DoubleType();
    case TypeKind::kStringWrapper:
      return StringType();
    case TypeKind::kIntWrapper:
      return IntType();
    case TypeKind::kUintWrapper:
      return UintType();
    default:
      return absl::nullopt;
  }
}

}  // namespace

Type TypeInferenceContext::InstantiateTypeParams(const Type& type) {
  InstanceMap substitutions;
  return InstantiateTypeParams(type, substitutions);
}

Type TypeInferenceContext::InstantiateTypeParams(
    const Type& type,
    absl::flat_hash_map<std::string, absl::string_view>& substitutions) {
  switch (type.kind()) {
    // Unparameterized types -- just forward.
    case TypeKind::kAny:
    case TypeKind::kBool:
    case TypeKind::kBoolWrapper:
    case TypeKind::kBytes:
    case TypeKind::kBytesWrapper:
    case TypeKind::kDouble:
    case TypeKind::kDoubleWrapper:
    case TypeKind::kDuration:
    case TypeKind::kDyn:
    case TypeKind::kError:
    case TypeKind::kInt:
    case TypeKind::kNull:
    case TypeKind::kString:
    case TypeKind::kStringWrapper:
    case TypeKind::kStruct:
    case TypeKind::kTimestamp:
    case TypeKind::kUint:
    case TypeKind::kIntWrapper:
    case TypeKind::kUintWrapper:
      return type;
    case TypeKind::kTypeParam: {
      absl::string_view name = type.AsTypeParam()->name();
      if (IsTypeVar(name)) {
        // Already instantiated (e.g. list comprehension variable).
        return type;
      }
      if (auto it = substitutions.find(name); it != substitutions.end()) {
        return TypeParamType(it->second);
      }
      absl::string_view substitution = NewTypeVar(name);
      substitutions[type.AsTypeParam()->name()] = substitution;
      return TypeParamType(substitution);
    }
    case TypeKind::kType: {
      auto type_type = type.AsType();
      auto parameters = type_type->GetParameters();
      if (parameters.size() == 1) {
        Type param = InstantiateTypeParams(parameters[0], substitutions);
        return TypeType(arena_, param);
      } else if (parameters.size() > 1) {
        return ErrorType();
      } else {  // generic type
        return type;
      }
    }
    case TypeKind::kList: {
      Type elem =
          InstantiateTypeParams(type.AsList()->element(), substitutions);
      return ListType(arena_, elem);
    }
    case TypeKind::kMap: {
      Type key = InstantiateTypeParams(type.AsMap()->key(), substitutions);
      Type value = InstantiateTypeParams(type.AsMap()->value(), substitutions);
      return MapType(arena_, key, value);
    }
    case TypeKind::kOpaque: {
      auto opaque_type = type.AsOpaque();
      auto parameters = opaque_type->GetParameters();
      std::vector<Type> param_instances;
      param_instances.reserve(parameters.size());

      for (int i = 0; i < parameters.size(); ++i) {
        param_instances.push_back(
            InstantiateTypeParams(parameters[i], substitutions));
      }
      return OpaqueType(arena_, type.AsOpaque()->name(), param_instances);
    }
    default:
      return ErrorType();
  }
}

bool TypeInferenceContext::IsAssignable(const Type& from, const Type& to) {
  SubstitutionMap prospective_substitutions;
  bool result = IsAssignableInternal(from, to, prospective_substitutions);
  if (result) {
    UpdateTypeParameterBindings(prospective_substitutions);
  }
  return result;
}

bool TypeInferenceContext::IsAssignableInternal(
    const Type& from, const Type& to,
    SubstitutionMap& prospective_substitutions) {
  Type to_subs = Substitute(to, prospective_substitutions);
  Type from_subs = Substitute(from, prospective_substitutions);

  // Types always assignable to themselves.
  // Remainder is checking for assignability across different types.
  if (to_subs == from_subs) {
    return true;
  }

  // Resolve free type parameters.
  if (to_subs.kind() == TypeKind::kTypeParam ||
      from_subs.kind() == TypeKind::kTypeParam) {
    return IsAssignableWithConstraints(from_subs, to_subs,
                                       prospective_substitutions);
  }

  // Maybe widen a prospective type binding if another potential binding is
  // more general and admits the previous binding.
  if (
      // Checking assignability to a specific type var
      // that has a prospective type assignment.
      to.kind() == TypeKind::kTypeParam &&
      prospective_substitutions.contains(to.AsTypeParam()->name())) {
    auto prospective_subs_cpy(prospective_substitutions);
    if (CompareGenerality(from_subs, to_subs, prospective_subs_cpy) ==
        RelativeGenerality::kMoreGeneral) {
      if (IsAssignableInternal(to_subs, from_subs, prospective_subs_cpy) &&
          !OccursWithin(to.name(), from_subs, prospective_subs_cpy)) {
        prospective_subs_cpy[to.AsTypeParam()->name()] = from_subs;
        prospective_substitutions = prospective_subs_cpy;
        return true;
        // otherwise, continue with normal assignability check.
      }
    }
  }

  // Type is as concrete as it can be under current substitutions.
  if (absl::optional<Type> wrapped_type = WrapperToPrimitive(to_subs);
      wrapped_type.has_value()) {
    return from_subs.IsNull() ||
           IsAssignableInternal(*wrapped_type, from_subs,
                                prospective_substitutions);
  }

  // Wrapper types are assignable to their corresponding primitive type (
  // somewhat similar to auto unboxing). This is a bit odd with CEL's null_type,
  // but there isn't a dedicated syntax for narrowing from the nullable.
  if (auto from_wrapper = WrapperToPrimitive(from_subs);
      from_wrapper.has_value()) {
    return IsAssignableInternal(*from_wrapper, to_subs,
                                prospective_substitutions);
  }

  if (enable_legacy_null_assignment_) {
    if (from_subs.IsNull() && IsLegacyNullable(to_subs)) {
      return true;
    }

    if (to_subs.IsNull() && IsLegacyNullable(from_subs)) {
      return true;
    }
  }

  if (from_subs.kind() == TypeKind::kType &&
      to_subs.kind() == TypeKind::kType) {
    // Types are always assignable to themselves (even if differently
    // parameterized).
    return true;
  }

  if (to_subs.kind() == TypeKind::kEnum && from_subs.kind() == TypeKind::kInt) {
    return true;
  }

  if (from_subs.kind() == TypeKind::kEnum && to_subs.kind() == TypeKind::kInt) {
    return true;
  }

  if (IsWildCardType(from_subs) || IsWildCardType(to_subs)) {
    return true;
  }

  if (to_subs.kind() != from_subs.kind() ||
      to_subs.name() != from_subs.name()) {
    return false;
  }

  // Recurse for the type parameters.
  auto to_params = to_subs.GetParameters();
  auto from_params = from_subs.GetParameters();
  const auto params_size = to_params.size();

  if (params_size != from_params.size()) {
    return false;
  }
  for (size_t i = 0; i < params_size; ++i) {
    if (!IsAssignableInternal(from_params[i], to_params[i],
                              prospective_substitutions)) {
      return false;
    }
  }
  return true;
}

Type TypeInferenceContext::Substitute(
    const Type& type, const SubstitutionMap& substitutions) const {
  Type subs = type;
  while (subs.kind() == TypeKind::kTypeParam) {
    TypeParamType t = subs.GetTypeParam();
    if (auto it = substitutions.find(t.name()); it != substitutions.end()) {
      subs = it->second;
      continue;
    }
    if (auto it = type_parameter_bindings_.find(t.name());
        it != type_parameter_bindings_.end()) {
      if (it->second.type.has_value()) {
        subs = *it->second.type;
        continue;
      }
    }
    break;
  }
  return subs;
}

TypeInferenceContext::RelativeGenerality
TypeInferenceContext::CompareGenerality(
    const Type& from, const Type& to,
    const SubstitutionMap& prospective_substitutions) const {
  Type from_subs = Substitute(from, prospective_substitutions);
  Type to_subs = Substitute(to, prospective_substitutions);

  if (from_subs == to_subs) {
    return RelativeGenerality::kEquivalent;
  }

  if (IsUnionType(from_subs) && IsSubsetOf(to_subs, from_subs)) {
    return RelativeGenerality::kMoreGeneral;
  }

  if (IsUnionType(to_subs)) {
    return RelativeGenerality::kLessGeneral;
  }

  if (enable_legacy_null_assignment_ && IsLegacyNullable(from_subs) &&
      to_subs.IsNull()) {
    return RelativeGenerality::kMoreGeneral;
  }

  // Not a polytype. Check if it is a parameterized type and all parameters are
  // equivalent and at least one is more general.
  if (from_subs.IsList() && to_subs.IsList()) {
    return CompareGenerality(from_subs.AsList()->GetElement(),
                             to_subs.AsList()->GetElement(),
                             prospective_substitutions);
  }

  if (from_subs.IsMap() && to_subs.IsMap()) {
    RelativeGenerality key_generality =
        CompareGenerality(from_subs.AsMap()->GetKey(),
                          to_subs.AsMap()->GetKey(), prospective_substitutions);
    RelativeGenerality value_generality = CompareGenerality(
        from_subs.AsMap()->GetValue(), to_subs.AsMap()->GetValue(),
        prospective_substitutions);
    if (key_generality == RelativeGenerality::kLessGeneral ||
        value_generality == RelativeGenerality::kLessGeneral) {
      return RelativeGenerality::kLessGeneral;
    }
    if (key_generality == RelativeGenerality::kMoreGeneral ||
        value_generality == RelativeGenerality::kMoreGeneral) {
      return RelativeGenerality::kMoreGeneral;
    }
    return RelativeGenerality::kEquivalent;
  }

  if (from_subs.IsOpaque() && to_subs.IsOpaque() &&
      from_subs.AsOpaque()->name() == to_subs.AsOpaque()->name() &&
      from_subs.AsOpaque()->GetParameters().size() ==
          to_subs.AsOpaque()->GetParameters().size()) {
    RelativeGenerality max_generality = RelativeGenerality::kEquivalent;
    for (int i = 0; i < from_subs.AsOpaque()->GetParameters().size(); ++i) {
      RelativeGenerality generality = CompareGenerality(
          from_subs.AsOpaque()->GetParameters()[i],
          to_subs.AsOpaque()->GetParameters()[i], prospective_substitutions);
      if (generality == RelativeGenerality::kLessGeneral) {
        return RelativeGenerality::kLessGeneral;
      }
      if (generality == RelativeGenerality::kMoreGeneral) {
        max_generality = RelativeGenerality::kMoreGeneral;
      }
    }
    return max_generality;
  }

  // Default not comparable. Since we ruled out polytypes, they should be
  // equivalent for the purposes of deciding the most general eligible
  // substitution.
  return RelativeGenerality::kEquivalent;
}

bool TypeInferenceContext::OccursWithin(
    absl::string_view var_name, const Type& type,
    const SubstitutionMap& substitutions) const {
  // This is difficult to trigger in normal CEL expressions, but may
  // happen with comprehensions where we can potentially reference a variable
  // with a free type var in different ways.
  //
  // This check guarantees that we don't introduce a recursive type definition
  // (a cycle in the substitution map).
  if (type.kind() == TypeKind::kTypeParam) {
    if (type.AsTypeParam()->name() == var_name) {
      return true;
    }
    auto typeSubs = Substitute(type, substitutions);
    if (typeSubs != type && OccursWithin(var_name, typeSubs, substitutions)) {
      return true;
    }
  }

  for (const auto& param : type.GetParameters()) {
    if (OccursWithin(var_name, param, substitutions)) {
      return true;
    }
  }
  return false;
}

bool TypeInferenceContext::IsAssignableWithConstraints(
    const Type& from, const Type& to,
    SubstitutionMap& prospective_substitutions) {
  if (to.kind() == TypeKind::kTypeParam &&
      from.kind() == TypeKind::kTypeParam) {
    if (to.AsTypeParam()->name() != from.AsTypeParam()->name()) {
      // Simple case, bind from to 'to' if both are free.
      prospective_substitutions[from.AsTypeParam()->name()] = to;
    }
    return true;
  }

  if (to.kind() == TypeKind::kTypeParam) {
    absl::string_view name = to.AsTypeParam()->name();
    if (!OccursWithin(name, from, prospective_substitutions)) {
      prospective_substitutions[name] = from;
      return true;
    }
  }

  if (from.kind() == TypeKind::kTypeParam) {
    absl::string_view name = from.AsTypeParam()->name();
    if (!OccursWithin(name, to, prospective_substitutions)) {
      prospective_substitutions[name] = to;
      return true;
    }
  }

  // If either types are wild cards but we weren't able to specialize,
  // assume assignable and continue.
  if (IsWildCardType(from) || IsWildCardType(to)) {
    return true;
  }

  return false;
}

absl::optional<TypeInferenceContext::OverloadResolution>
TypeInferenceContext::ResolveOverload(const FunctionDecl& decl,
                                      absl::Span<const Type> argument_types,
                                      bool is_receiver) {
  absl::optional<Type> result_type;

  std::vector<OverloadDecl> matching_overloads;
  for (const auto& ovl : decl.overloads()) {
    if (ovl.member() != is_receiver ||
        argument_types.size() != ovl.args().size()) {
      continue;
    }

    auto call_type_instance = InstantiateFunctionOverload(*this, ovl);
    ABSL_DCHECK_EQ(argument_types.size(),
                   call_type_instance.param_types.size());
    bool is_match = true;
    SubstitutionMap prospective_substitutions;
    for (int i = 0; i < argument_types.size(); ++i) {
      if (!IsAssignableInternal(argument_types[i],
                                call_type_instance.param_types[i],
                                prospective_substitutions)) {
        is_match = false;
        break;
      }
    }

    if (is_match) {
      matching_overloads.push_back(ovl);
      UpdateTypeParameterBindings(prospective_substitutions);
      if (!result_type.has_value()) {
        result_type = call_type_instance.result_type;
      } else {
        if (!TypeEquivalent(*result_type, call_type_instance.result_type)) {
          result_type = DynType();
        }
      }
    }
  }

  if (!result_type.has_value() || matching_overloads.empty()) {
    return absl::nullopt;
  }
  return OverloadResolution{
      .result_type = FullySubstitute(*result_type, /*free_to_dyn=*/false),
      .overloads = std::move(matching_overloads),
  };
}

void TypeInferenceContext::UpdateTypeParameterBindings(
    const SubstitutionMap& prospective_substitutions) {
  if (prospective_substitutions.empty()) {
    return;
  }
  for (auto iter = prospective_substitutions.begin();
       iter != prospective_substitutions.end(); ++iter) {
    if (auto binding_iter = type_parameter_bindings_.find(iter->first);
        binding_iter != type_parameter_bindings_.end()) {
      binding_iter->second.type = iter->second;
    } else {
      ABSL_LOG(WARNING) << "Uninstantiated type parameter: " << iter->first;
    }
  }
}

bool TypeInferenceContext::TypeEquivalent(const Type& a, const Type& b) {
  return a == b;
}

Type TypeInferenceContext::FullySubstitute(const Type& type,
                                           bool free_to_dyn) const {
  switch (type.kind()) {
    case TypeKind::kTypeParam: {
      Type subs = Substitute(type, {});
      if (subs.kind() == TypeKind::kTypeParam) {
        if (free_to_dyn) {
          return DynType();
        }
        return subs;
      }
      return FullySubstitute(subs, free_to_dyn);
    }
    case TypeKind::kType: {
      if (type.AsType()->GetParameters().empty()) {
        return type;
      }
      Type param = FullySubstitute(type.AsType()->GetType(), free_to_dyn);
      return TypeType(arena_, param);
    }
    case TypeKind::kList: {
      Type elem = FullySubstitute(type.AsList()->GetElement(), free_to_dyn);
      return ListType(arena_, elem);
    }
    case TypeKind::kMap: {
      Type key = FullySubstitute(type.AsMap()->GetKey(), free_to_dyn);
      Type value = FullySubstitute(type.AsMap()->GetValue(), free_to_dyn);
      return MapType(arena_, key, value);
    }
    case TypeKind::kOpaque: {
      std::vector<Type> types;
      for (const auto& param : type.AsOpaque()->GetParameters()) {
        types.push_back(FullySubstitute(param, free_to_dyn));
      }
      return OpaqueType(arena_, type.AsOpaque()->name(), types);
    }
    default:
      return type;
  }
}

bool TypeInferenceContext::AssignabilityContext::IsAssignable(const Type& from,
                                                              const Type& to) {
  return inference_context_.IsAssignableInternal(from, to,
                                                 prospective_substitutions_);
}

void TypeInferenceContext::AssignabilityContext::
    UpdateInferredTypeAssignments() {
  inference_context_.UpdateTypeParameterBindings(
      std::move(prospective_substitutions_));
}

void TypeInferenceContext::AssignabilityContext::Reset() {
  prospective_substitutions_.clear();
}

}  // namespace cel::checker_internal
