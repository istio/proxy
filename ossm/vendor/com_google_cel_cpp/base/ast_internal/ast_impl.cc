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

#include "base/ast_internal/ast_impl.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"

namespace cel::ast_internal {
namespace {

const Type& DynSingleton() {
  static auto* singleton = new Type(TypeKind(DynamicType()));
  return *singleton;
}

}  // namespace

const Type& AstImpl::GetType(int64_t expr_id) const {
  auto iter = type_map_.find(expr_id);
  if (iter == type_map_.end()) {
    return DynSingleton();
  }
  return iter->second;
}

const Type& AstImpl::GetReturnType() const { return GetType(root_expr().id()); }

const Reference* AstImpl::GetReference(int64_t expr_id) const {
  auto iter = reference_map_.find(expr_id);
  if (iter == reference_map_.end()) {
    return nullptr;
  }
  return &iter->second;
}

}  // namespace cel::ast_internal
