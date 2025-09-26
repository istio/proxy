// Copyright 2021 Google LLC
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

// CEL does not support calling the parser during C++ static initialization.
// Callers must ensure the parser is only invoked after C++ static initializers
// are run. Failing to do so is undefined behavior. The current reason for this
// is the parser uses ANTLRv4, which also makes no guarantees about being safe
// with regard to C++ static initialization. As such, neither do we.

#ifndef THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
#define THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_

#include <memory>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/source.h"
#include "parser/macro.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser_interface.h"
#include "parser/source_factory.h"

namespace google::api::expr::parser {

class VerboseParsedExpr {
 public:
  VerboseParsedExpr(cel::expr::ParsedExpr parsed_expr,
                    EnrichedSourceInfo enriched_source_info)
      : parsed_expr_(std::move(parsed_expr)),
        enriched_source_info_(std::move(enriched_source_info)) {}

  const cel::expr::ParsedExpr& parsed_expr() const {
    return parsed_expr_;
  }
  const EnrichedSourceInfo& enriched_source_info() const {
    return enriched_source_info_;
  }

 private:
  cel::expr::ParsedExpr parsed_expr_;
  EnrichedSourceInfo enriched_source_info_;
};

// See comments at the top of the file for information about usage during C++
// static initialization.
absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    absl::string_view expression, const std::vector<Macro>& macros,
    absl::string_view description = "<input>",
    const ParserOptions& options = ParserOptions());

// See comments at the top of the file for information about usage during C++
// static initialization.
absl::StatusOr<cel::expr::ParsedExpr> Parse(
    absl::string_view expression, absl::string_view description = "<input>",
    const ParserOptions& options = ParserOptions());

// See comments at the top of the file for information about usage during C++
// static initialization.
absl::StatusOr<cel::expr::ParsedExpr> ParseWithMacros(
    absl::string_view expression, const std::vector<Macro>& macros,
    absl::string_view description = "<input>",
    const ParserOptions& options = ParserOptions());

// See comments at the top of the file for information about usage during C++
// static initialization.
absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    const cel::Source& source, const cel::MacroRegistry& registry,
    const ParserOptions& options = ParserOptions());

// See comments at the top of the file for information about usage during C++
// static initialization.
absl::StatusOr<cel::expr::ParsedExpr> Parse(
    const cel::Source& source, const cel::MacroRegistry& registry,
    const ParserOptions& options = ParserOptions());

}  // namespace google::api::expr::parser

namespace cel {
// Creates a new parser builder.
//
// Intended for use with the Compiler class, most users should prefer the free
// functions above for independent parsing of expressions.
std::unique_ptr<ParserBuilder> NewParserBuilder(
    const ParserOptions& options = {});
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_PARSER_PARSER_H_
