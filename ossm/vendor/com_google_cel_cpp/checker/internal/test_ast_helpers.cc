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
#include "checker/internal/test_ast_helpers.h"

#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "extensions/protobuf/ast_converters.h"
#include "internal/status_macros.h"
#include "parser/options.h"
#include "parser/parser.h"

namespace cel::checker_internal {

using ::cel::extensions::CreateAstFromParsedExpr;
using ::google::api::expr::parser::Parse;

absl::StatusOr<std::unique_ptr<Ast>> MakeTestParsedAst(
    absl::string_view expression) {
  static ParserOptions options;
  options.enable_optional_syntax = true;
  CEL_ASSIGN_OR_RETURN(auto parsed,
                       Parse(expression, /*description=*/expression, options));

  return CreateAstFromParsedExpr(std::move(parsed));
}

}  // namespace cel::checker_internal
