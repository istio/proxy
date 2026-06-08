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

#include "common/ast.h"

#include <cstdint>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "common/ast/metadata.h"

namespace cel {
namespace {

const TypeSpec& DynSingleton() {
  static absl::NoDestructor<TypeSpec> singleton{TypeSpecKind(DynTypeSpec())};
  return *singleton;
}

}  // namespace

const TypeSpec* absl_nullable Ast::GetType(int64_t expr_id) const {
  auto iter = type_map_.find(expr_id);
  if (iter == type_map_.end()) {
    return nullptr;
  }
  return &iter->second;
}

const TypeSpec& Ast::GetTypeOrDyn(int64_t expr_id) const {
  if (const TypeSpec* type = GetType(expr_id); type != nullptr) {
    return *type;
  }
  return DynSingleton();
}

const TypeSpec& Ast::GetReturnType() const {
  return GetTypeOrDyn(root_expr().id());
}

const Reference* absl_nullable Ast::GetReference(int64_t expr_id) const {
  auto iter = reference_map_.find(expr_id);
  if (iter == reference_map_.end()) {
    return nullptr;
  }
  return &iter->second;
}

}  // namespace cel
