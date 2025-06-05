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

#include "common/expr.h"

#include "absl/base/no_destructor.h"

namespace cel {

const UnspecifiedExpr& UnspecifiedExpr::default_instance() {
  static const absl::NoDestructor<UnspecifiedExpr> instance;
  return *instance;
}

const IdentExpr& IdentExpr::default_instance() {
  static const absl::NoDestructor<IdentExpr> instance;
  return *instance;
}

const SelectExpr& SelectExpr::default_instance() {
  static const absl::NoDestructor<SelectExpr> instance;
  return *instance;
}

const CallExpr& CallExpr::default_instance() {
  static const absl::NoDestructor<CallExpr> instance;
  return *instance;
}

const ListExpr& ListExpr::default_instance() {
  static const absl::NoDestructor<ListExpr> instance;
  return *instance;
}

const StructExpr& StructExpr::default_instance() {
  static const absl::NoDestructor<StructExpr> instance;
  return *instance;
}

const MapExpr& MapExpr::default_instance() {
  static const absl::NoDestructor<MapExpr> instance;
  return *instance;
}

const ComprehensionExpr& ComprehensionExpr::default_instance() {
  static const absl::NoDestructor<ComprehensionExpr> instance;
  return *instance;
}

const Expr& Expr::default_instance() {
  static const absl::NoDestructor<Expr> instance;
  return *instance;
}

}  // namespace cel
