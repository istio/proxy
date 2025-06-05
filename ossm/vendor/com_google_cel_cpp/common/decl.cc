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

#include "common/decl.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "common/type.h"
#include "common/type_kind.h"

namespace cel {

namespace common_internal {

bool TypeIsAssignable(const Type& to, const Type& from) {
  if (to == from) {
    return true;
  }
  const auto to_kind = to.kind();
  if (to_kind == TypeKind::kDyn) {
    return true;
  }
  switch (to_kind) {
    case TypeKind::kBoolWrapper:
      return TypeIsAssignable(NullType{}, from) ||
             TypeIsAssignable(BoolType{}, from);
    case TypeKind::kIntWrapper:
      return TypeIsAssignable(NullType{}, from) ||
             TypeIsAssignable(IntType{}, from);
    case TypeKind::kUintWrapper:
      return TypeIsAssignable(NullType{}, from) ||
             TypeIsAssignable(UintType{}, from);
    case TypeKind::kDoubleWrapper:
      return TypeIsAssignable(NullType{}, from) ||
             TypeIsAssignable(DoubleType{}, from);
    case TypeKind::kBytesWrapper:
      return TypeIsAssignable(NullType{}, from) ||
             TypeIsAssignable(BytesType{}, from);
    case TypeKind::kStringWrapper:
      return TypeIsAssignable(NullType{}, from) ||
             TypeIsAssignable(StringType{}, from);
    default:
      break;
  }
  const auto from_kind = from.kind();
  if (to_kind != from_kind || to.name() != from.name()) {
    return false;
  }
  auto to_params = to.GetParameters();
  auto from_params = from.GetParameters();
  const auto params_size = to_params.size();
  if (params_size != from_params.size()) {
    return false;
  }
  for (size_t i = 0; i < params_size; ++i) {
    if (!TypeIsAssignable(to_params[i], from_params[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace common_internal

namespace {

bool SignaturesOverlap(const OverloadDecl& lhs, const OverloadDecl& rhs) {
  if (lhs.member() != rhs.member()) {
    return false;
  }
  const auto& lhs_args = lhs.args();
  const auto& rhs_args = rhs.args();
  const auto args_size = lhs_args.size();
  if (args_size != rhs_args.size()) {
    return false;
  }
  bool args_overlap = true;
  for (size_t i = 0; i < args_size; ++i) {
    args_overlap =
        args_overlap &&
        (common_internal::TypeIsAssignable(lhs_args[i], rhs_args[i]) ||
         common_internal::TypeIsAssignable(rhs_args[i], lhs_args[i]));
  }
  return args_overlap;
}

template <typename Overload>
void AddOverloadInternal(std::vector<OverloadDecl>& insertion_order,
                         OverloadDeclHashSet& overloads, Overload&& overload,
                         absl::Status& status) {
  if (!status.ok()) {
    return;
  }
  if (auto it = overloads.find(overload.id()); it != overloads.end()) {
    status = absl::AlreadyExistsError(
        absl::StrCat("overload already exists: ", overload.id()));
    return;
  }
  for (const auto& existing : overloads) {
    if (SignaturesOverlap(overload, existing)) {
      status = absl::InvalidArgumentError(
          absl::StrCat("overload signature collision: ", existing.id(),
                       " collides with ", overload.id()));
      return;
    }
  }
  const auto inserted = overloads.insert(std::forward<Overload>(overload));
  ABSL_DCHECK(inserted.second);
  insertion_order.push_back(*inserted.first);
}

void CollectTypeParams(absl::flat_hash_set<std::string>& type_params,
                       const Type& type) {
  const auto kind = type.kind();
  switch (kind) {
    case TypeKind::kList: {
      const auto& list_type = type.GetList();
      CollectTypeParams(type_params, list_type.element());
    } break;
    case TypeKind::kMap: {
      const auto& map_type = type.GetMap();
      CollectTypeParams(type_params, map_type.key());
      CollectTypeParams(type_params, map_type.value());
    } break;
    case TypeKind::kOpaque: {
      const auto& opaque_type = type.GetOpaque();
      for (const auto& param : opaque_type.GetParameters()) {
        CollectTypeParams(type_params, param);
      }
    } break;
    case TypeKind::kFunction: {
      const auto& function_type = type.GetFunction();
      CollectTypeParams(type_params, function_type.result());
      for (const auto& arg : function_type.args()) {
        CollectTypeParams(type_params, arg);
      }
    } break;
    case TypeKind::kTypeParam:
      type_params.emplace(type.GetTypeParam().name());
      break;
    default:
      break;
  }
}

}  // namespace

absl::flat_hash_set<std::string> OverloadDecl::GetTypeParams() const {
  absl::flat_hash_set<std::string> type_params;
  CollectTypeParams(type_params, result());
  for (const auto& arg : args()) {
    CollectTypeParams(type_params, arg);
  }
  return type_params;
}

void FunctionDecl::AddOverloadImpl(const OverloadDecl& overload,
                                   absl::Status& status) {
  AddOverloadInternal(overloads_.insertion_order, overloads_.set, overload,
                      status);
}

void FunctionDecl::AddOverloadImpl(OverloadDecl&& overload,
                                   absl::Status& status) {
  AddOverloadInternal(overloads_.insertion_order, overloads_.set,
                      std::move(overload), status);
}

}  // namespace cel
