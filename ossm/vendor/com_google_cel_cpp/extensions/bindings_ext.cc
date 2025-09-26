// Copyright 2023 Google LLC
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

#include "extensions/bindings_ext.h"

#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/ast.h"
#include "parser/macro.h"
#include "parser/macro_expr_factory.h"

namespace cel::extensions {

namespace {

static constexpr char kCelNamespace[] = "cel";
static constexpr char kBind[] = "bind";
static constexpr char kUnusedIterVar[] = "#unused";

bool IsTargetNamespace(const Expr& target) {
  return target.has_ident_expr() && target.ident_expr().name() == kCelNamespace;
}

}  // namespace

std::vector<Macro> bindings_macros() {
  absl::StatusOr<Macro> cel_bind = Macro::Receiver(
      kBind, 3,
      [](MacroExprFactory& factory, Expr& target,
         absl::Span<Expr> args) -> absl::optional<Expr> {
        if (!IsTargetNamespace(target)) {
          return absl::nullopt;
        }
        if (!args[0].has_ident_expr()) {
          return factory.ReportErrorAt(
              args[0], "cel.bind() variable name must be a simple identifier");
        }
        auto var_name = args[0].ident_expr().name();
        return factory.NewComprehension(kUnusedIterVar, factory.NewList(),
                                        std::move(var_name), std::move(args[1]),
                                        factory.NewBoolConst(false),
                                        std::move(args[0]), std::move(args[2]));
      });
  return {*cel_bind};
}

}  // namespace cel::extensions
